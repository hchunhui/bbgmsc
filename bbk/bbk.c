#include "bbk.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdatomic.h>

void Joy_Reset();
void Joy_Write4016(unsigned char data);
unsigned char Joy_Read4016(void);
unsigned char Joy_Read4017(void);
#include "LPC_D6_SYNTH.H"

extern uint8_t mirror;

/*
  ram or rom ff01
  01
  23 DRAM 0x7a000
  45 DRAM 0x78000
  67 DRAM
  89 DRAM ff14... / ROM ff14
  ab DRAM ff1c... / ROM ff1c
  cd DRAM ff24... / ROM 0x1c000
  ef < ff00 DRAM ff2c... / ROM 0x1e000
     >= ff00 IO
 */

uint8_t *map_addr(BBK *bbk, uint16_t addr, int is_write)
{
	switch (addr >> 13) {
		case 1: // 23
			if (!(addr & 2) && bbk->b_mapram && !bbk->b_ff01_d4 && is_write) {
				return &(bbk->dram[0x7A000 + (addr & 0x1FFF)]);
			}
			break;
		case 2: // 45
			if (addr < 0x4400)
				break;
			// fall through
		case 3: // 67
			return &(bbk->dram[0x78000 + (addr & 0x3FFF)]);
			break;
		case 4: // 89
			if (bbk->reg_ff14 & 0x40) {
				return &(bbk->dram[(bbk->reg_ff14 & 0x3F) * 0x2000 + (addr & 0x1FFF)]);
			} else if (!is_write) {
				return &(bbk->bios[(bbk->reg_ff14 & 0xF) * 0x2000 + (addr & 0x1FFF)]);
			}
			break;
		case 5: // AB
			if (bbk->reg_ff14 & 0x40) {
				return &(bbk->dram[(bbk->reg_ff1c & 0x3F) * 0x2000 + (addr & 0x1FFF)]);
			} else if (!is_write) {
				return &(bbk->bios[(bbk->reg_ff1c & 0xF) * 0x2000 + (addr & 0x1FFF)]);
			}
			break;
		case 6: // CD
			if (bbk->b_mapram) {
				return &(bbk->dram[(bbk->reg_ff24 & 0x3F) * 0x2000 + (addr & 0x1FFF)]);
			} else if (!is_write) {
				return &(bbk->bios[0x1C000 + (addr & 0x1FFF)]);
			}
			break;
		case 7: // EF
			if (addr < 0xFF00) {
				if (bbk->b_mapram) {
					return &(bbk->dram[(bbk->reg_ff2c & 0x3F) * 0x2000 + (addr & 0x1FFF)]);
				} else if (!is_write) {
					return &(bbk->bios[0x1E000 + (addr & 0x1FFF)]);
				}
			} else if (is_write) { // FF00-FFFF
				// IO
			} else if (bbk->b_mapram) {
				// Read
				return &(bbk->dram[(bbk->reg_ff2c & 0x3F) * 0x2000 + (addr & 0x1FFF)]);
			} else if (0 == (addr & 7)) { // Map to ROM
				// Read IO
			} else {
				return &(bbk->bios[0x1E000 + (addr & 0x1FFF)]);
			}
			break;
	}
	return NULL;
}

static void CheckIRQ(BBK *bbk)
{
	if (254 == bbk->line_count && bbk->b_splitmode) {
		if (bbk->qindex == bbk->nr_sr) {
			if (bbk->b_enable_irq) {
				bbk->irq = 1;
				return;
			}
		}
	}

	if (254 == bbk->line_count && !bbk->b_splitmode) {
		if (bbk->b_enable_irq) {
			bbk->irq = 1;
			return;
		}
	}

	bbk->irq = 0;
}

uint8_t *map_chraddr(BBK *bbk, uint16_t addr)
{
	return bbk->vbank[addr >> 10] + (addr & 1023);
}

uint8_t bbk_memlow(BBK *bbk, uint16_t addr, uint8_t val, uint8_t write)
{
	uint8_t *ptr = map_addr(bbk, addr, write);
	if (ptr) {
		if (write) {
			*ptr = val;
		}
		return *ptr;
	}
	switch (addr) {
	case 0x4016:
		if (write) {
			Joy_Write4016(val);
		} else {
			return Joy_Read4016();
		}
		break;
	case 0x4017:
		if (!write) {
			return Joy_Read4017();
		}
		break;
	default:
//		printf("lowaddr %04x val %02x write %d\n",
//		       addr, val, write);
	}
	return ~0;
}

uint8_t bbk_mem(BBK *bbk, uint16_t addr, uint8_t val, uint8_t write)
{
	uint8_t *ptr;

	ptr = map_addr(bbk, addr, write);
	if (ptr) {
		if (write) {
			*ptr = val;
		}
		return *ptr;
	}

	if (addr >= 0xff80 && addr <= 0xffb8) {
		unsigned char nPort = (addr >> 3) & 7;
		if (!write) {
			return FdcRead(&(bbk->fdc), nPort);
		} else {
			FdcWrite(&(bbk->fdc), nPort, val);
			return val;
		}
	}

	switch (addr) {
	case 0xff01: // VideoCtrlPort
		if (write) {
			switch (val & 3) {
			case 0: mirror = 2; break; // VRAM_VMIRROR
			case 1: mirror = 3; break; // VRAM_HMIRROR
			case 2: mirror = 0; break; // VRAM_MIRROR4L
			case 3: mirror = 1; break; // VRAM_MIRROR4H
			}
			bbk->b_splitmode = (val & 0x40) ? 1 : 0;
			bbk->b_enable_irq = (val & 4) ? 1 : 0;
			CheckIRQ(bbk);

			if (val & 8) bbk->b_mapram = 1;
			else bbk->b_mapram = 0;
			bbk->b_ff01_d4 = !!(val & 0x10);
		}
		break;
	case 0xff02: // IntCountPortL
		if (write) {
			bbk->line_count = val;
		}
		break;
	case 0xff06:
		if (write) {
			bbk->line_count &= 0x0f;
			bbk->line_count |= (val & 0x0f) << 4;
		}
		break;
	case 0xff12: // Init
		if (write) {
			if (!bbk->b_enable_irq) {
				bbk->nr_sr = 0;
				bbk->nr_vr = 0;
				if (bbk->b_splitmode)
					bbk->qindex = 0;
			}
			CheckIRQ(bbk);
		}
		break;
	case 0xff0a: // Scanline
		if (write) {
			bbk->queue_sr[bbk->nr_sr] = val;
			if (bbk->b_enable_irq)
				bbk->nr_sr = 0;
			else
				bbk->nr_sr++;
			CheckIRQ(bbk);
		}
		break;
	case 0xFF1A: // VideoBank
		if (write) {
			bbk->queue_vr[bbk->nr_vr] = val;
			if (bbk->b_enable_irq)
				bbk->nr_vr = 0;
			else
				bbk->nr_vr++;
			CheckIRQ(bbk);
		}
		break;
	case 0xFF22: // Start
		if (write) {
			if (bbk->b_splitmode) {
				unsigned char page;

				if (!bbk->b_enable_irq) {
					bbk->qindex = 0;
					bbk->line_count = 0;
				}

				bbk->line_count = bbk->queue_sr[bbk->qindex];

				page = bbk->queue_vr[bbk->qindex] & 15;
				bbk->vbank[0] = bbk->vram + page * 0x0800 + 0x0000;
				bbk->vbank[1] = bbk->vram + page * 0x0800 + 0x0400;
				page = (bbk->queue_vr[bbk->qindex] >> 4) & 15;
				bbk->vbank[2] = bbk->vram + page * 0x0800 + 0x0000;
				bbk->vbank[3] = bbk->vram + page * 0x0800 + 0x0400;
				bbk->qindex++;
			}
			CheckIRQ(bbk);
		}
		break;
	case 0xff03: // VideoDataPort0
	case 0xff0b: // VideoDataPort1
	case 0xff13: // VideoDataPort2
	case 0xff1b: // VideoDataPort3
		if (write) {
			int i = (addr - 0xff03) >> 2;
			val &= 0x0f;
			bbk->vbank[i] = bbk->vram + val * 0x0800;
			bbk->vbank[i + 1] = bbk->vram + val * 0x0800 + 0x0400;
		}
		break;
	case 0xff23:
	case 0xff2b:
	case 0xff33:
	case 0xff3b:
	case 0xff43:
	case 0xff4b:
	case 0xff53:
	case 0xff5b:
		if (write) {
			int i = (addr - 0xff23) >> 3;
			val &= 0x1f;
			bbk->vbank[i] = bbk->vram + val * 0x0400;
		}
		break;
	case 0xff04: // DRAMPagePort
		if (write) {
			bbk->reg_ff14 = (val << 1) & 0x7f;
			bbk->reg_ff1c = ((val << 1) & 0x3f) | 1;
		}
		break;
	case 0xff14:
		if (write) {
			val &= 0x3F;
			bbk->reg_ff14 = val | 0x40;
		}
		break;
	case 0xff1c:
		if (write) {
			bbk->reg_ff1c = val & 0x3f;
		}
		break;
	case 0xff24: // DRAMPagePortCD
		if (write) {
			bbk->reg_ff24 = val & 0x3f;
		}
		break;
	case 0xff2c: // DRAMPagePortEF
		if (write) {
			bbk->reg_ff2c = val & 0x3f;
		}
		break;
	case 0xff10: // SoundPort0/SpeakInitPort
		if (write) {
			if (bbk->reg_spint == 0 && (val & 1)) {
				lpc_d6_synth_reset(bbk->lpc.lpc);
			}
			bbk->reg_spint = val & 1;
		}
		break;
	case 0xff18: // SoundPort1/SpeakDataPort
		if (write) {
			if (((bbk->lpc.wpos + 1) & (SP_BUF_SIZE - 1)) !=
			    bbk->lpc.rpos) {
				bbk->lpc.data[bbk->lpc.wpos++] = val;
				bbk->lpc.wpos &= SP_BUF_SIZE - 1;
			}
		} else {
			if (((bbk->lpc.wpos + 1) & (SP_BUF_SIZE - 1)) ==
			    bbk->lpc.rpos)
				return 0; // buffer full, busy
			else
				return 0x8f; // idle
		}
		break;
	case 0xff40: // PCC port
	case 0xff48:
	case 0xff50:
		return 0;
		break;
	default:
//		printf("addr %04x val %02x write %d\n",
//		       addr, val, write);
	}
	return ~0;
}


void bbk_hsync(BBK *bbk, int line)
{
	// auto start while IRQ pending
	if (0 == line && bbk->b_splitmode) {
		unsigned char page;

		// continue use settings of last frame
		bbk->qindex = 0;
		bbk->line_count = bbk->queue_sr[bbk->qindex];

		page = bbk->queue_vr[bbk->qindex] & 15;
		bbk->vbank[0] = bbk->vram + page * 0x0800 + 0x0000;
		bbk->vbank[1] = bbk->vram + page * 0x0800 + 0x0400;
		page = (bbk->queue_vr[bbk->qindex] >> 4) & 15;
		bbk->vbank[2] = bbk->vram + page * 0x0800 + 0x0000;
		bbk->vbank[3] = bbk->vram + page * 0x0800 + 0x0400;

		bbk->qindex++;
	}

	if (line >= 240)
		return;

	CheckIRQ(bbk);

	if (255 == bbk->line_count) {
		if (bbk->qindex != bbk->nr_sr) {
			unsigned char page;

			bbk->line_count = bbk->queue_sr[bbk->qindex];

			page = bbk->queue_vr[bbk->qindex] & 15;
			bbk->vbank[0] = bbk->vram + page * 0x0800 + 0x0000;
			bbk->vbank[1] = bbk->vram + page * 0x0800 + 0x0400;
			page = (bbk->queue_vr[bbk->qindex] >> 4) & 15;
			bbk->vbank[2] = bbk->vram + page * 0x0800 + 0x0000;
			bbk->vbank[3] = bbk->vram + page * 0x0800 + 0x0400;

			bbk->qindex++;
		}
	} else if (bbk->b_splitmode || bbk->b_enable_irq) {
		bbk->line_count++;
	}
}

void bbk_run(BBK *bbk)
{
	int pcm_ok = atomic_load_explicit(&(bbk->lpc.pcm_ok),
					  memory_order_acquire);
	if (!pcm_ok) {
		int eos = 0;
		bool ok = true;
		while (ok && bbk->lpc.pcm_size < 3000) {
			int new_size;
			ok = lpc_d6_synth_do(bbk->lpc.lpc,
					     bbk->lpc.pcm + bbk->lpc.pcm_size,
					     &new_size, NULL, &eos);
			if (ok) bbk->lpc.pcm_size += new_size;
		}
		if (eos || bbk->lpc.pcm_size >= 2048) {
			bbk->lpc.eos = eos;
			atomic_store_explicit(&(bbk->lpc.pcm_ok), 1,
					      memory_order_release);
		}
	}
}

void bbk_lpc_callback(void* ud, uint8_t* stream, int len)
{
	BBK *bbk = ud;
	int16_t *buffer = (int16_t *)stream;
	int count = len / 2;

	for (int i = 0; i < count; i++) {
		buffer[i] = 0;
	}

	int pcm_ok = atomic_load_explicit(&(bbk->lpc.pcm_ok),
					  memory_order_acquire);
	if (pcm_ok) {
		if (bbk->lpc.pcm_size < count) {
			memcpy(buffer, bbk->lpc.pcm, bbk->lpc.pcm_size * 2);
			bbk->lpc.pcm_size = 0;
			bbk->lpc.eos = 0;
			atomic_store_explicit(&(bbk->lpc.pcm_ok), 0,
					      memory_order_release);
		} else {
			memcpy(buffer, bbk->lpc.pcm, count);
			bbk->lpc.pcm_size -= count;
			bbk->lpc.eos = 0;
			memmove(bbk->lpc.pcm, bbk->lpc.pcm + count, bbk->lpc.pcm_size * 2);
			atomic_store_explicit(&(bbk->lpc.pcm_ok), 0,
					      memory_order_release);
		}
	}
}

static int feed(void *host, uint8_t *food)
{
	BBK *bbk = host;
	if (bbk->lpc.rpos == bbk->lpc.wpos)
		return LPC_CMD_NONE;
	*food = bbk->lpc.data[bbk->lpc.rpos++];
	bbk->lpc.rpos &= SP_BUF_SIZE - 1;
	return LPC_CMD_PAYLOAD;
}

void bbk_reset(BBK *bbk, uint8_t *bios, char *fdd)
{
	memset(bbk, 0, sizeof(BBK));
	bbk->bios = bios;

	bbk->b_mapram = 0;
	bbk->b_ff01_d4 = 0;
	bbk->reg_ff14 = 0;
	bbk->reg_ff1c = 1;
	bbk->reg_ff24 = 0;
	bbk->reg_ff2c = 0;
	bbk->reg_spint = 0;

	bbk->b_splitmode = 0;
	bbk->b_enable_irq = 0;
	bbk->line_count = 0;
	bbk->nr_sr = 0;
	bbk->nr_vr = 0;
	bbk->qindex = 0;
	memset(bbk->queue_sr, 0, 32);
	memset(bbk->queue_vr, 0, 32);

	for (int i = 0; i < 8; i++)
		bbk->vbank[i] = bbk->vram + 1024 * i;

	FdcInit(&(bbk->fdc));
	FdcLoadDiskImage(&(bbk->fdc), fdd);
	Joy_Reset();
	mirror = 2;

	bbk->lpc.rpos = 0;
	bbk->lpc.wpos = 0;
	bbk->lpc.pcm_ok = 0;
	bbk->lpc.lpc = lpc_d6_synth_new(feed, bbk, LPC_STD_VARIANT_BBK);
}
