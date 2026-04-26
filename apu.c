// poor NES APU implementation
// by hchunhui
// MIT License
#include "apu.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

// 1.789773 MHz for NTSC, 1.662607 MHz for PAL, and 1.773448 MHz for Dendy
#define FCPU 1773448.0f

static void update_pulse(Pulse *p, int16_t *buffer, int count)
{
	float dt = 1.0f / SAMPLING_RATE;
	if (!p->active)
		return;

	for (int i = 0; i < count; i++) {
		if (p->volume > 0 && p->freq >= 20 &&
		    (!p->length_en || p->length > 0)) {
			int v = (p->phase < p->duty) ? 1 : -1;
			buffer[i] += v * p->volume * 0.2f * 32767;
		}

		p->freq *= p->sweep;
		if (!isfinite(p->freq) || p->freq > 20000) p->freq = 0;
		p->volume -= p->decay * dt;
		p->phase += p->freq * dt;
		if (p->length_en) p->length--;
		if (p->phase > 1.0f) p->phase -= 1.0f;
	}
}

static void update_triangle(Triangle *t, int16_t *buffer, int count)
{
	float dt = 1.0f / SAMPLING_RATE;
	if (!t->active || t->volume <= 0 || t->freq < 20 || t->freq > 20000)
		return;

	for (int i = 0; i < count; i++) {
		if (!t->length_en || t->length > 0) {
			int step = t->phase * 32;
			int value;
			if (step < 16) {
				value = 15 - step;
			} else {
				value = step - 16;
			}
			buffer[i] += ((float) value / 7.5f - 1.0f) * t->volume * 0.3f * 32767;
		}

		if (t->length_en) t->length--;
		t->phase += t->freq * dt;
		if (t->phase > 1.0f) t->phase -= 1.0f;
	}
}

static void update_noise(Noise *n, int16_t *buffer, int count)
{
	float dt = 1.0f / SAMPLING_RATE;
	const int cycles_per_sample = FCPU / SAMPLING_RATE;
	if (!n->active || n->volume <= 0)
		return;

	for (int i = 0; i < count; i++) {
		if (!n->length_en || n->length > 0) {

			for (int c = 0; c < cycles_per_sample; c++) {
				if (--n->timer <= 0) {
					n->timer = n->period;
					uint8_t feedback = (n->shift_reg & 1) ^ ((n->shift_reg >> (n->mode ? 6 : 1)) & 1);
					n->shift_reg = (n->shift_reg >> 1) | (feedback << 14);
				}
			}

			float vol = n->volume;
			if ((n->shift_reg & 1)) vol = 0.0f;
			buffer[i] += vol * 0.3f * 32767;
		}

		if (n->length_en) n->length--;
		n->volume -= n->decay * dt;
	}
}

void apu_callback(void* ud, uint8_t* stream, int len)
{
	APU *apu = ud;
	int16_t *buffer = (int16_t *)stream;
	int count = len / 2;

	for (int i = 0; i < count; i++) {
		buffer[i] = 0;
	}
	update_pulse(&(apu->pulse1), buffer, count);
	update_pulse(&(apu->pulse2), buffer, count);
	update_triangle(&(apu->triangle), buffer, count);
	update_noise(&(apu->noise), buffer, count);
}

void apu_reset(APU *apu)
{
	memset(apu, 0, sizeof(APU));
	apu->noise.shift_reg = 1;
}

const static int llut[32] = {
	10, 254, 20, 2,  40, 4,  80, 6,  160, 8,  60, 10, 14, 12, 26, 14,
	12, 16,  24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30
};

const static int plut[] = {
	4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068
};

#if defined(__GNUC__) && !defined(__clang__)
#define X(P, S) 1.0f/__builtin_powf(1.0f - 1.0f/(1 << S), 120.0f / (P+1) / SAMPLING_RATE)
const static float slut1[8][8] = {
	{ X(0, 0), X(0, 1), X(0, 2), X(0, 3), X(0, 4), X(0, 5), X(0, 6), X(0, 7) },
	{ X(1, 0), X(1, 1), X(1, 2), X(1, 3), X(1, 4), X(1, 5), X(1, 6), X(1, 7) },
	{ X(2, 0), X(2, 1), X(2, 2), X(2, 3), X(2, 4), X(2, 5), X(2, 6), X(2, 7) },
	{ X(3, 0), X(3, 1), X(3, 2), X(3, 3), X(3, 4), X(3, 5), X(3, 6), X(3, 7) },
	{ X(4, 0), X(4, 1), X(4, 2), X(4, 3), X(4, 4), X(4, 5), X(4, 6), X(4, 7) },
	{ X(5, 0), X(5, 1), X(5, 2), X(5, 3), X(5, 4), X(5, 5), X(5, 6), X(5, 7) },
	{ X(6, 0), X(6, 1), X(6, 2), X(6, 3), X(6, 4), X(6, 5), X(6, 6), X(6, 7) },
	{ X(7, 0), X(7, 1), X(7, 2), X(7, 3), X(7, 4), X(7, 5), X(7, 6), X(7, 7) },
};
#undef X
#define X(P, S) 1.0f/__builtin_powf(1.0f + 1.0f/(1 << S), 120.0f / (P+1) / SAMPLING_RATE)
const static float slut2[8][8] = {
	{ X(0, 0), X(0, 1), X(0, 2), X(0, 3), X(0, 4), X(0, 5), X(0, 6), X(0, 7) },
	{ X(1, 0), X(1, 1), X(1, 2), X(1, 3), X(1, 4), X(1, 5), X(1, 6), X(1, 7) },
	{ X(2, 0), X(2, 1), X(2, 2), X(2, 3), X(2, 4), X(2, 5), X(2, 6), X(2, 7) },
	{ X(3, 0), X(3, 1), X(3, 2), X(3, 3), X(3, 4), X(3, 5), X(3, 6), X(3, 7) },
	{ X(4, 0), X(4, 1), X(4, 2), X(4, 3), X(4, 4), X(4, 5), X(4, 6), X(4, 7) },
	{ X(5, 0), X(5, 1), X(5, 2), X(5, 3), X(5, 4), X(5, 5), X(5, 6), X(5, 7) },
	{ X(6, 0), X(6, 1), X(6, 2), X(6, 3), X(6, 4), X(6, 5), X(6, 6), X(6, 7) },
	{ X(7, 0), X(7, 1), X(7, 2), X(7, 3), X(7, 4), X(7, 5), X(7, 6), X(7, 7) },
};
#undef X
#elif SAMPLING_RATE == 44100
const static float slut1[8][8] = {
	{ 1.0, 1.0018879, 1.0007831, 1.0003635, 1.0001756, 1.0000864, 1.0000429, 1.0000213 },
	{ 1.0, 1.0009434, 1.0003915, 1.0001817, 1.0000879, 1.0000433, 1.0000215, 1.0000107 },
	{ 1.0, 1.0006289, 1.0002609, 1.0001211, 1.0000585, 1.0000288, 1.0000143, 1.0000072 },
	{ 1.0, 1.0004716, 1.0001957, 1.0000908, 1.0000440, 1.0000216, 1.0000107, 1.0000054 },
	{ 1.0, 1.0003773, 1.0001565, 1.0000727, 1.0000352, 1.0000173, 1.0000086, 1.0000043 },
	{ 1.0, 1.0003144, 1.0001305, 1.0000606, 1.0000293, 1.0000144, 1.0000072, 1.0000036 },
	{ 1.0, 1.0002695, 1.0001118, 1.0000520, 1.0000252, 1.0000124, 1.0000062, 1.0000031 },
	{ 1.0, 1.0002358, 1.0000979, 1.0000454, 1.0000219, 1.0000108, 1.0000054, 1.0000027 },
};

const static float slut2[8][8] = {
	{ 0.9981157, 0.9988973, 0.9993930, 0.9996796, 0.9998350, 0.9999163, 0.9999578, 0.9999788 },
	{ 0.9990574, 0.9994485, 0.9996965, 0.9998398, 0.9999175, 0.9999582, 0.9999789, 0.9999894 },
	{ 0.9993715, 0.9996322, 0.9997976, 0.9998932, 0.9999450, 0.9999721, 0.9999859, 0.9999930 },
	{ 0.9995286, 0.9997242, 0.9998482, 0.9999199, 0.9999588, 0.9999790, 0.9999895, 0.9999948 },
	{ 0.9996228, 0.9997794, 0.9998785, 0.9999359, 0.9999670, 0.9999833, 0.9999915, 0.9999957 },
	{ 0.9996858, 0.9998161, 0.9998988, 0.9999466, 0.9999725, 0.9999861, 0.9999930, 0.9999964 },
	{ 0.9997305, 0.9998424, 0.9999132, 0.9999542, 0.9999764, 0.9999881, 0.9999939, 0.9999970 },
	{ 0.9997643, 0.9998621, 0.9999241, 0.9999599, 0.9999794, 0.9999895, 0.9999948, 0.9999974 },
};
#endif

uint8_t apu_mem(APU *apu, uint16_t addr, uint8_t val, uint8_t write)
{
	Pulse *pulse;
	Triangle *triangle = &(apu->triangle);
	Noise *noise = &(apu->noise);
	Pulse *p[2] = { &(apu->pulse1), &(apu->pulse2) };
	switch (addr) {
	case 0x4000:
	case 0x4004:
		pulse = p[(addr >> 2) & 1];
		if (write) {
			int D = (val >> 6) & 3;
			int L = (val >> 5) & 1;
			int C = (val >> 4) & 1;
			int V = (val) & 0x0f;
			switch (D) {
			case 0: pulse->duty = 0.125; break;
			case 1: pulse->duty = 0.25; break;
			case 2: pulse->duty = 0.5; break;
			case 3: pulse->duty = 0.25; break;
			}
			if (C) {
				pulse->volume = V / 15.0f;
				pulse->decay = 0.0f;
			} else {
				pulse->volume = 1.0f;
				pulse->decay = 1.0f / 15 * 240.0f / (V + 1);
			}
			pulse->length_en = !L;
		}
		break;
	case 0x4001:
	case 0x4005:
		pulse = p[(addr >> 2) & 1];
		if (write) {
			int E = (val >> 7) & 1;
			int P = (val >> 4) & 7;
			int N = (val >> 3) & 1;
			int S = (val) & 7;
			if (E) {
				if (N) {
					pulse->sweep = slut1[P][S];
				} else {
					pulse->sweep = slut2[P][S];
				}
			} else {
				pulse->sweep = 1.0f;
			}
		}
		break;
	case 0x4002:
	case 0x4006:
		pulse = p[(addr >> 2) & 1];
		if (write) {
			pulse->timer = (pulse->timer & ~0xff) | val;
			pulse->freq = FCPU / (16 * (pulse->timer + 1));
		}
		break;
	case 0x4003:
	case 0x4007:
		pulse = p[(addr >> 2) & 1];
		if (write) {
			int L = (val >> 3) & 0x1f;
			int T = val & 7;
			pulse->timer = (pulse->timer & 0xff) | (T << 8);
			pulse->freq = FCPU / (16 * (pulse->timer + 1));
			pulse->length = llut[L] * SAMPLING_RATE / 120;
		}
		break;
	case 0x4008:
		if (write) {
			int C = (val >> 7) & 1;
			int R = val & 0x7f;
			triangle->volume = 1.0f;
			triangle->length_en = !C;
			triangle->length = R * SAMPLING_RATE / 240;
		}
		break;
	case 0x4009:
		break;
	case 0x400a:
		if (write) {
			triangle->timer = (triangle->timer & ~0xff) | val;
			triangle->freq = FCPU / (32 * (triangle->timer + 1));
		}
		break;
	case 0x400b:
		if (write) {
			int L = (val >> 3) & 0x1f;
			int T = val & 7;
			triangle->timer = (triangle->timer & 0xff) | (T << 8);
			triangle->freq = FCPU / (32 * (triangle->timer + 1));
			triangle->length = llut[L] * SAMPLING_RATE / 120;
		}
		break;
	case 0x400c:
		if (write) {
			int L = (val >> 5) & 1;
			int C = (val >> 4) & 1;
			int V = (val) & 0x0f;
			if (C) {
				noise->volume = V / 15.0f;
				noise->decay = 0.0f;
			} else {
				noise->volume = 1.0f;
				noise->decay = 1.0f / 15 * 240.0f / (V + 1);
			}
			noise->volume = V / 15.0f;
			noise->length_en = !L;
		}
		break;
	case 0x400d:
		break;
	case 0x400e:
		if (write) {
			noise->mode = (val >> 7) & 1;
			noise->period = plut[val & 0xf];
		}
		break;
	case 0x400f:
		if (write) {
			int L = (val >> 3) & 0x1f;
			noise->length = llut[L] * SAMPLING_RATE / 120;
		}
		break;
	case 0x4015:
		if (write) {
			apu->pulse1.active = !!(val & 1);
			apu->pulse2.active = !!(val & 2);
			apu->triangle.active = !!(val & 4);
			apu->noise.active = !!(val & 8);
			if (!apu->pulse1.active) {
				apu->pulse1.length_en = 1;
				apu->pulse1.length = 0;
			}
			if (!apu->pulse2.active) {
				apu->pulse2.length_en = 1;
				apu->pulse2.length = 0;
			}
			if (!apu->triangle.active) {
				apu->triangle.length_en = 1;
				apu->triangle.length = 0;
			}
			if (!apu->noise.active) {
				apu->noise.length_en = 1;
				apu->noise.length = 0;
			}
		} else {
			uint8_t out = 0;
			if (apu->pulse1.active) out |= 1;
			if (apu->pulse2.active) out |= 2;
			if (apu->triangle.active) out |= 4;
			if (apu->noise.active) out |= 8;
			return out;
		}
		break;
	}
	return ~0;
}
