#include "cyclic_timer.h"

#include <drv/tmr1.h>

void cyclic_tmr_start(uint16_t period, timer_cb_t cb, uintptr_t user_data)
{
    timer1_cb(cb, user_data);
    /* Clear Timer on Compare */
    TMR1_MODE_CTC();
    TMR1_WR16_A(period);
    TMR1_WR16_CNTR(0);
    TMR1_A_INT_ENABLE();
    /* 16MHz / 64 = 250kHz == 4us
     * 4us * 2^16 ~ 262.1ms
     *
     * 16MHz / 256 = 62.5kHz = 16us
     * 16us * 2^16 ~ 1.049s
     *
     * 16MHz / 1024 = 15625Hz = 64us
     * 64us * 2^16 ~ 4.194s */
    TMR1_CLK_DIV_1024();
}

void cyclic_tmr_stop(void)
{
    TMR1_CLK_DISABLE();
    TMR1_A_INT_DISABLE();
    TMR1_A_INT_CLEAR();
    timer1_cb(NULL, 0);
}
