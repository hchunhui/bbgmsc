// poor NES APU implementation
// by hchunhui
// MIT License
#include "apu.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

// 1.789773 MHz for NTSC, 1.662607 MHz for PAL, and 1.773448 MHz for Dendy
#define FCPU 1773448

static int32_t mulq30(int32_t a, int32_t b)
{
	return (((int64_t) a * b)) >> 30;
}

static void update_pulse(Pulse *p, int16_t *buffer, int count)
{
	int32_t dt = (1 << 30) / SAMPLING_RATE;
	if (!p->active)
		return;

	for (int i = 0; i < count; i++) {
		if (p->volume > 0 && (!p->length_en || p->length > 0)) {
			int v = (p->volume / 5) >> 15;
			if (p->phase >= p->duty) v = -v;
			buffer[i] += v;
		}

		p->freq = mulq30(p->freq, p->sweep);
		if (p->freq > (SAMPLING_RATE << 14) / 2) p->freq = 0;
		p->volume -= mulq30(p->decay, dt) * 240;
		p->phase += mulq30(p->freq, dt);
		if (p->length_en) p->length--;
		if (p->phase > (1 << 14)) p->phase -= (1 << 14);
	}
}

static void update_triangle(Triangle *t, int16_t *buffer, int count)
{
	int32_t dt = (1 << 30) / SAMPLING_RATE;
	if (!t->active)
		return;

	for (int i = 0; i < count; i++) {
		if (!t->length_en || t->length > 0) {
			int step = (t->phase * 32) >> 14;
			int value;
			if (step < 16) {
				value = 15 - step;
			} else {
				value = step - 16;
			}
			buffer[i] += (value * 2 - 15) * 32767 / 50;
		}

		if (t->length_en) t->length--;
		t->phase += mulq30(t->freq, dt);
		if (t->phase > (1 << 14)) t->phase -= (1 << 14);
	}
}

static void update_noise(Noise *n, int16_t *buffer, int count)
{
	int32_t dt = (1 << 30) / SAMPLING_RATE;
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

			int32_t vol = n->volume;
			if ((n->shift_reg & 1)) vol = -vol;
			buffer[i] += mulq30(vol, (1 << 30) / 10 * 3) >> 15;
		}

		if (n->length_en) n->length--;
		n->volume -= mulq30(n->decay, dt) * 240;
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
#define X(P, S) (1 << 30) / __builtin_powf(1.0f - 1.0f/(1 << S), 120.0f / (P+1) / SAMPLING_RATE)
const static int32_t slut1[8][8] = {
	{ (1<<30), X(0, 1), X(0, 2), X(0, 3), X(0, 4), X(0, 5), X(0, 6), X(0, 7) },
	{ (1<<30), X(1, 1), X(1, 2), X(1, 3), X(1, 4), X(1, 5), X(1, 6), X(1, 7) },
	{ (1<<30), X(2, 1), X(2, 2), X(2, 3), X(2, 4), X(2, 5), X(2, 6), X(2, 7) },
	{ (1<<30), X(3, 1), X(3, 2), X(3, 3), X(3, 4), X(3, 5), X(3, 6), X(3, 7) },
	{ (1<<30), X(4, 1), X(4, 2), X(4, 3), X(4, 4), X(4, 5), X(4, 6), X(4, 7) },
	{ (1<<30), X(5, 1), X(5, 2), X(5, 3), X(5, 4), X(5, 5), X(5, 6), X(5, 7) },
	{ (1<<30), X(6, 1), X(6, 2), X(6, 3), X(6, 4), X(6, 5), X(6, 6), X(6, 7) },
	{ (1<<30), X(7, 1), X(7, 2), X(7, 3), X(7, 4), X(7, 5), X(7, 6), X(7, 7) },
};
#undef X
#define X(P, S) (1 << 30) / __builtin_powf(1.0f + 1.0f/(1 << S), 120.0f / (P+1) / SAMPLING_RATE)
const static int32_t slut2[8][8] = {
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
const static int32_t slut1[8][8] = {
	{ 0x40000000, 0x401eee80, 0x400cd480, 0x4005f480, 0x4002e080, 0x40016a80, 0x4000b400, 0x40005980 },
	{ 0x40000000, 0x400f7500, 0x40066a00, 0x4002fa00, 0x40017080, 0x4000b580, 0x40005a00, 0x40002d00 },
	{ 0x40000000, 0x400a4e00, 0x40044680, 0x4001fc00, 0x4000f580, 0x40007900, 0x40003c00, 0x40001e00 },
	{ 0x40000000, 0x4007ba00, 0x40033500, 0x40017d00, 0x4000b880, 0x40005a80, 0x40002d00, 0x40001680 },
	{ 0x40000000, 0x40062e80, 0x40029080, 0x40013100, 0x40009380, 0x40004880, 0x40002400, 0x40001200 },
	{ 0x40000000, 0x40052680, 0x40022380, 0x4000fe00, 0x40007b00, 0x40003c80, 0x40001e00, 0x40000f00 },
	{ 0x40000000, 0x40046a80, 0x4001d500, 0x4000da00, 0x40006980, 0x40003400, 0x40001a00, 0x40000d00 },
	{ 0x40000000, 0x4003dd00, 0x40019a80, 0x4000be80, 0x40005c00, 0x40002d80, 0x40001680, 0x40000b80 },
};

const static int32_t slut2[8][8] = {
	{ 0x3fe12080, 0x3fedef00, 0x3ff60e00, 0x3ffac000, 0x3ffd4c00, 0x3ffea100, 0x3fff4f00, 0x3fffa700 },
	{ 0x3ff08e40, 0x3ff6f6c0, 0x3ffb0700, 0x3ffd6000, 0x3ffea600, 0x3fff5080, 0x3fffa780, 0x3fffd380 },
	{ 0x3ff5b3c0, 0x3ff9f980, 0x3ffcaf40, 0x3ffe4000, 0x3fff1980, 0x3fff8b00, 0x3fffc500, 0x3fffe280 },
	{ 0x3ff84700, 0x3ffb7b40, 0x3ffd8380, 0x3ffeb000, 0x3fff5300, 0x3fffa800, 0x3fffd400, 0x3fffea00 },
	{ 0x3ff9d200, 0x3ffc62c0, 0x3ffe0280, 0x3ffef300, 0x3fff7580, 0x3fffba00, 0x3fffdc80, 0x3fffee00 },
	{ 0x3ffada00, 0x3ffcfcc0, 0x3ffe5780, 0x3fff2000, 0x3fff8c80, 0x3fffc580, 0x3fffe280, 0x3ffff100 },
	{ 0x3ffb95c0, 0x3ffd6b00, 0x3ffe9400, 0x3fff4000, 0x3fff9d00, 0x3fffce00, 0x3fffe680, 0x3ffff380 },
	{ 0x3ffc2340, 0x3ffdbd80, 0x3ffec180, 0x3fff5800, 0x3fffa980, 0x3fffd400, 0x3fffea00, 0x3ffff500 },
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
			case 0: pulse->duty = (1<<14) / 8; break;
			case 1: pulse->duty = (1<<14) / 4; break;
			case 2: pulse->duty = (1<<14) / 2; break;
			case 3: pulse->duty = (1<<14) / 4; break;
			}
			if (C) {
				pulse->volume = ((V << 27) / 15) << 3;
				pulse->decay = 0;
			} else {
				pulse->volume = 1 << 30;
				pulse->decay = (1 << 30) / (15 * (V + 1));
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
				pulse->sweep = 1 << 30;
			}
		}
		break;
	case 0x4002:
	case 0x4006:
		pulse = p[(addr >> 2) & 1];
		if (write) {
			pulse->timer = (pulse->timer & ~0xff) | val;
			int freq = (FCPU<<10) / (pulse->timer + 1);
			if (freq > (SAMPLING_RATE<<14) / 2)
				freq = 0;
			pulse->freq = freq;
		}
		break;
	case 0x4003:
	case 0x4007:
		pulse = p[(addr >> 2) & 1];
		if (write) {
			int L = (val >> 3) & 0x1f;
			int T = val & 7;
			pulse->timer = (pulse->timer & 0xff) | (T << 8);
			int freq = (FCPU<<10) / (pulse->timer + 1);
			if (freq > (SAMPLING_RATE<<14) / 2)
				freq = 0;
			pulse->freq = freq;
			pulse->length = llut[L] * SAMPLING_RATE / 120;
		}
		break;
	case 0x4008:
		if (write) {
			int C = (val >> 7) & 1;
			int R = val & 0x7f;
			triangle->length_en = !C;
			triangle->length = R * SAMPLING_RATE / 240;
		}
		break;
	case 0x4009:
		break;
	case 0x400a:
		if (write) {
			triangle->timer = (triangle->timer & ~0xff) | val;
			int freq = (FCPU<<10) / (2 * (triangle->timer + 1));
			if (freq > (SAMPLING_RATE<<14) / 2)
				freq = 0;
			triangle->freq = freq;
		}
		break;
	case 0x400b:
		if (write) {
			int L = (val >> 3) & 0x1f;
			int T = val & 7;
			triangle->timer = (triangle->timer & 0xff) | (T << 8);
			int freq = (FCPU<<10) / (2 * (triangle->timer + 1));
			if (freq > (SAMPLING_RATE<<14) / 2)
				freq = 0;
			triangle->freq = freq;
			triangle->length = llut[L] * SAMPLING_RATE / 120;
		}
		break;
	case 0x400c:
		if (write) {
			int L = (val >> 5) & 1;
			int C = (val >> 4) & 1;
			int V = (val) & 0x0f;
			if (C) {
				noise->decay = 0;
			} else {
				noise->decay = (1 << 30) / (15 * (V + 1));
			}
			noise->volume = ((V << 27) / 15) << 3;
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
