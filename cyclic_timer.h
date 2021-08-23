#pragma once

#include <drv/tmrx.h>

void cyclic_tmr_start(uint16_t period, timer_cb_t cb, uintptr_t);
void cyclic_tmr_stop(void);
