#include <string.h>
#include <stdio.h>

#include <avr/interrupt.h>
#include <avr/sleep.h>

#include <drv/spi0.h>
#include <drv/usart0.h>
#include <drv/watchdog.h>

#include <bootloader/fixed.h>

#include "nRF24L01.h"

// nRF IRQ      PC.2/PCINT10       pin: A2 pro-mini
// nRF CE       PC.1/PCINT9        pin: A1 pro-mini
// RLY CTL      PC.0/PCINT8        pin: A0 pro-mini
// SPI0 SCK     PB.5/PCINT5        pin: 13 pro-mini
// SPI0 MISO    PB.4/PCINT4        pin: 12 pro-mini
// SPI0 MOSI    PB.3/PCINT3        pin: 11 pro-mini
// SPI0 !SS     PB.2/PCINT2        pin: 10 pro-mini

static
void spi_chip_select_on(void)
{
    /* SPI setup for nRF24L01 interfaceing
     * 1) MSB order (default on POR)
     * 2) SCK is LOW when idle (default on POR)
     * 3) CSN is HIGH when idle (!SS default on POR)
     * */
    PORTB &= ~M1(DDB2); // SPI0/!SS PB.2 low
}

static
void spi_chip_select_off(void)
{
    PORTB |= M1(DDB2); /* SPI0/!SS PB.2 high */
}

static
void spi_xchg(uint8_t *begin, const uint8_t *const end)
{
    spi_chip_select_on();
    spi0_xchg(begin, end);
    spi_chip_select_off();
}

static
void ce_set(nRF24L01_ce_t state)
{
    if(state.CE) PORTC |= M1(DDC1);
    else PORTC &= ~M1(DDC1);
}

static
void init(nRF24L01_t *dev)
{
    /* PC.1 / nRF CE */
    PORTC &= ~M1(DDC1); // to low
    DDRC |= M1(DDC1); // to output

    // PC.2 / nRF IRQ
    DDRC &= ~M1(DDC2); // to input
    PORTC |= M1(PORTC2); // pull-up
    // PC.2 / nRF IRQ pin-change interrupt enable (PCINT10)
    PCICR |= M1(PCIE1);
    PCMSK1 |= M1(PCINT10);

    // PB.2 / SPI0 !SS
    DDRB |= M1(DDB2); // output
    PORTB &= ~M1(DDB2); // low

    // PB.3 / SPI0 MOSI
    PORTB |= M1(DDB3); // output
    DDRB |= M1(DDB3); // high

    // PB.5 / SPI0/CLK
    PORTB |= M1(DDB5); // output
    DDRB |= M1(DDB5); // high

    SPI0_MASTER();
    SPI0_CLK_DIV_16(); // 1MHz
    SPI0_ENABLE();

    nRF24L01_init(dev, ce_set, spi_xchg);

    /* TX, 1B CRC, CRC enabled, interrupts not masked */
    nRF24L01_CFG(dev, config, .PRIM_RX = 0, .PWR_UP = 1, .CRCO = 0, .EN_CRC = 1, .MASK_MAX_RT = 0, .MASK_TX_DS = 0, .MASK_RX_DR = 0); // @PoR
    /* enable auto ACK for data pipe 0/1/2/3/4/5 */
    nRF24L01_CFG(dev, en_aa, .ENAA_P0 = 1, .ENAA_P1 = 1, .ENAA_P2 = 1, .ENAA_P3 = 0, .ENAA_P4 = 1, .ENAA_P5 = 1); // @PoR
    /* enable data pipe 0/1 */
    nRF24L01_CFG(dev, en_rxaddr, .ERX_P0 = 1, .ERX_P1 = 1); // @PoR
    /* adress width 5B */
    nRF24L01_CFG(dev, setup_aw, .AW = 3); // @PoR
    /* auto re-transmit count 3, auto re-transmit delay 250+86us */
    nRF24L01_CFG(dev, setup_retr, .ARC = 3, .ARD = 0); // @PoR
    /* channel 2 */
    nRF24L01_CFG(dev, rf_ch, .RF_CH = 2); // @PoR
    /* 0dBm, 2Mbps */
    nRF24L01_CFG(dev, rf_setup, .RF_PWR = 3, .RF_DR_LOW = 1); // @PoR
    nRF24L01_CFG(dev, rx_addr_p0, .addr = {0xE7, 0xE7, 0xE7, 0xE7, 0xE7}); // @PoR
    nRF24L01_CFG(dev, rx_addr_p1, .addr = {0xC2, 0xC2, 0xC2, 0xC2, 0xC2}); // @PoR
    nRF24L01_CFG(dev, rx_addr_p2, .addr = 0xC3); // @PoR
    nRF24L01_CFG(dev, rx_addr_p3, .addr = 0xC4); // @PoR
    nRF24L01_CFG(dev, rx_addr_p4, .addr = 0xC5); // @PoR
    nRF24L01_CFG(dev, rx_addr_p5, .addr = 0xC6); // @PoR
    nRF24L01_CFG(dev, tx_addr, .addr = {0xE7, 0xE7, 0xE7, 0xE7, 0xE7}); // @PoR
}

static
void broadcast(uintptr_t);

static
void on_send_error(
    nRF24L01_status_t status,
    nRF24L01_fifo_status_t fifo_status,
    uintptr_t user_data)
{
    if(!user_data) return;
    //nRF24L01_t *dev = (nRF24L01_t *)user_data;

    if(status.MAX_RT)
    {
        broadcast(user_data);
    }
}

static
void broadcast(uintptr_t user_data)
{
    if(!user_data) return;

    nRF24L01_t *dev = (nRF24L01_t *)user_data;
    const char *message = "hello from nRF24L01\r\n";

    //usart0_send_str("broadcast\r\n");
    nRF24L01_send(
        dev,
        (const uint8_t *)message, (const uint8_t *)(message + 21),
        broadcast,
        on_send_error,
        user_data);
}


__attribute__((noreturn))
void main(void)
{
    /* watchdog is enabled by bootloader whenever it "jumps" to app code */
    fixed__.app_reset_code.curr = RESET_CODE_APP_IDLE;
    watchdog_disable();
    //watchdog_enable(WATCHDOG_TIMEOUT_1000ms);

    nRF24L01_t dev;

    USART0_BR(CALC_BR(CPU_CLK, 19200));
    USART0_PARITY_EVEN();
    USART0_TX_ENABLE();

    init(&dev);
    /* set SMCR SE (Sleep Enable bit) */
    sleep_enable();

    broadcast((uintptr_t)&dev);

    for(;;)
    {
        cli();
        {
            usart0_send_str("loop\r\n");

            /* TODO: do event dispatch once per main event loop
             * provide periodic timer, for now dispatch all events to avoid
             * missing some */
            while(0 == (PINC & M1(PINC2)))
            {
                dev.updated = 1;
                nRF24L01_event(&dev);
            }
        }
        sei();
        sleep_cpu();
    }
}

ISR(PCINT1_vect) {}
