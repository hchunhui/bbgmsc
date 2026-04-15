/*******************************************************************************
 *
 *  LPC-10 D6 Synthesizer
 *
 *  Author:  <87430545@qq.com>
 *
 *  Create:  Oct/31/2021 by fanoble
 *
 *******************************************************************************
 */

#ifndef __LPC_D6_SYNTH_H__
#define __LPC_D6_SYNTH_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

typedef int (*lpc_feed_t)(void* host, unsigned char* food);

#define LPC_CMD_STOP		   -1
#define LPC_CMD_NONE			0
#define LPC_CMD_RESET			1
#define LPC_CMD_PAYLOAD			2

#define LPC_STD_VARIANT_BBK		0
#define LPC_STD_VARIANT_SB2K	1

void* lpc_d6_synth_new(lpc_feed_t feed, void* host, int variant);
bool  lpc_d6_synth_do(void* lpc, short* pcm, int* pcm_size, int* restart, int *res);
int   lpc_d6_synth_reset(void* lpc);
void  lpc_d6_synth_delete(void* lpc);

#ifdef __cplusplus
}
#endif

#endif
