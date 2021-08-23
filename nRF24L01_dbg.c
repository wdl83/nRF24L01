#include <stdio.h>

#include <drv/usart0.h>

#include "nRF24L01.h"

void nRF24L01_dump(nRF24L01_t *dev)
{
    const char *lookup[] =
    {
        "0000", "0001", "0010", "0011",
        "0100", "0101", "0110", "0111",
        "1000", "1001", "1010", "1011",
        "1100", "1101", "1110", "1111"
    };

    uint8_t addr[] =
    {
        nRF24L01_ADDR_config,
        nRF24L01_ADDR_en_aa,
        nRF24L01_ADDR_en_rxaddr,
        nRF24L01_ADDR_setup_aw,
        nRF24L01_ADDR_setup_retr,
        nRF24L01_ADDR_rf_ch,
        nRF24L01_ADDR_rf_setup,
        nRF24L01_ADDR_status,
        nRF24L01_ADDR_observe_tx,
        nRF24L01_ADDR_rpd,
        nRF24L01_ADDR_rx_pw_p0,
        nRF24L01_ADDR_rx_pw_p1,
        nRF24L01_ADDR_rx_pw_p2,
        nRF24L01_ADDR_rx_pw_p3,
        nRF24L01_ADDR_rx_pw_p4,
        nRF24L01_ADDR_rx_pw_p5,
        nRF24L01_ADDR_fifo_status
    };

    uint8_t reg[sizeof(addr)];

    for(uint8_t i = 0; i < sizeof(addr); ++i)
    {
        uint8_t xdata[] =
        {
            nRF24L01_R_REGISTER(addr[i]),
            nRF24L01_NOP
        };

        dev->spi_xchg(xdata, xdata + sizeof(xdata));
        reg[i] = xdata[1];
    }

    for(uint8_t i = 0; i < sizeof(addr); ++i)
    {
        char str[16];
        sprintf(str, "[%" PRIx8 "] 0x%" PRIx8 " ", addr[i], reg[i]);
        usart0_send_str(str);
        usart0_send_str(lookup[(reg[i] >> 4) & 0xF]);
        usart0_send_str(lookup[reg[i] & 0xF]);
        usart0_send_str("\r\n");
    }
}

