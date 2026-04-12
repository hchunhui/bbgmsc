#ifndef BBK_H
#define BBK_H

#include <stdint.h>
#include "nes_fdc.h"

typedef struct {
	uint8_t vram[32 * 1024];
	uint8_t dram[512 * 1024];
	uint8_t *vbank[8];

	// inno
	uint8_t b_mapram;
	uint8_t b_ff01_d4;
	uint8_t reg_ff14;
	uint8_t reg_ff1c;
	uint8_t reg_ff24;
	uint8_t reg_ff2c;

	// holtek
	uint8_t b_splitmode;
	uint8_t b_enable_irq;
	int line_count;
	int nr_sr;
	int nr_vr;
	uint8_t queue_sr[32];
	uint8_t queue_vr[32];
	int qindex;

	uint8_t *bios;
	FDC fdc;
	int irq;
} BBK;

void bbk_reset(BBK *bbk, uint8_t *bios, char *fdd);
uint8_t bbk_memlow(BBK *bbk, uint16_t addr, uint8_t val, uint8_t write);
uint8_t bbk_mem(BBK *bbk, uint16_t addr, uint8_t val, uint8_t write);
uint8_t *map_chraddr(BBK *bbk, uint16_t addr);
void bbk_hsync(BBK *bbk, int line);

#endif /* BBK_H */
