#ifndef APU_H
#define APU_H

#include <stdint.h>
#define SAMPLING_RATE 44100

typedef struct {
	float freq;
	float phase;
	float duty;
	float volume;
	float sweep;
	float decay;
	int timer;
	int length_en;
	int length;
	int active;
} Pulse;

typedef struct {
	float freq;
	float phase;
	float volume;
	int timer;
	int length_en;
	int length;
	int active;
} Triangle;

typedef struct {
	uint16_t shift_reg;
	int mode;
	int timer;
	int period;
	float volume;
	float decay;
	int length_en;
	int length;
	int active;
} Noise;

typedef struct {
	Pulse pulse1;
	Pulse pulse2;
	Triangle triangle;
	Noise noise;
} APU;

uint8_t apu_mem(APU *apu, uint16_t addr, uint8_t val, uint8_t write);
void apu_callback(void *ud, uint8_t *stream, int free);
void apu_reset(APU *apu);

#endif /* APU_H */
