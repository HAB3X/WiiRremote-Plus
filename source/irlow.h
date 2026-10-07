#ifndef IRLOW_H
#define IRLOW_H
#include "common.h"
/* Low-level carrier generation, implemented per platform. */
void irlow_begin(u32 carrier_hz);
void irlow_mark(u32 us);
void irlow_space(u32 us);
void irlow_end(void);
void irlow_idle(void);       /* restore sensor bar idle state (pointer on/off) */
u32  irlow_lock(void);
void irlow_unlock(u32 lvl);
#endif
