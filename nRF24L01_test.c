#include <avr/interrupt.h>
#include <avr/sleep.h>

#include "nRF24L01.h"

#include "drv/spi0.h"

// nRFIRQ   PC.2/PCINT10 (A2 pro-mini)
// nRFCE    PC.1/PCINT9 (A1 pro-mini)
// RLYCTL   PC.0/PCINT8 (A0 pro-mini)

static
void spi_chip_select_on(void)
{
    /* SPI setup for nRF24L01 interfaceing
     * 1) MSB order (default on POR)
     * 2) SCK is LOW when idle (default on POR)
     * 3) CSN is HIGH when idle (!SS default on POR)
     * */
    // switch SPI0 PB.2/!SS (10 pro-mini) to output
    DDRB |= M1(DDB2);
    // 4MHz CLK
    SPI0_CLK_DIV_4();
    //SPI0_CLK_x2(); // 4MHz x2 = 8MHz
}

static
void spi_chip_select_off(void)
{
    /* switch SPI0 PB.2/!SS (10 pro-mini) to input */
    DDRB &= ~M1(DDB2);
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
    { /* PC.1/nRFCE (A1 pro-mini) */
        DDRC |= M1(DDC1); // to output
        PORTC &= ~M1(DDC1); // to low
    }

    { // PC.2/nRFIRQ (A2 pro-mini)
        DDRC &= ~M1(DDC2); // to input
    }

    { // PC.2/nRFIRQ pin-change interrupt (PCINT10)
        PCICR |= M1(PCIE1);
        PCMSK1 |= ~M1(PCINT10);
    }

    SPI0_MASTER();
    SPI0_ENABLE();

    nRF24L01_init(dev,  ce_set, spi_xchg);

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
    nRF24L01_CFG(dev, rf_setup, .RF_PWR = 3, .RF_DR = 1); // @PoR
    nRF24L01_CFG(dev, rx_addr_p0, .addr = {0xE7, 0xE7, 0xE7, 0xE7, 0xE7}); // @PoR
    nRF24L01_CFG(dev, rx_addr_p1, .addr = {0xC2, 0xC2, 0xC2, 0xC2, 0xC2}); // @PoR
    nRF24L01_CFG(dev, rx_addr_p2, .addr = 0xC3); // @PoR
    nRF24L01_CFG(dev, rx_addr_p3, .addr = 0xC4); // @PoR
    nRF24L01_CFG(dev, rx_addr_p4, .addr = 0xC5); // @PoR
    nRF24L01_CFG(dev, rx_addr_p5, .addr = 0xC6); // @PoR
    nRF24L01_CFG(dev, tx_addr, .addr = {0xE7, 0xE7, 0xE7, 0xE7, 0xE7}); // @PoR
}

static
void mask_irq(void)
{
    // mask nRFIRQ, until nRF request is serviced by nRF24L01_irq routine
    if(PCIFR & M1(PCIF1)) PCMSK1 |= M1(PCINT10);
}

static
void unmask_irq(void)
{
    // by now IRQ should be cleared by nRF, unmask pin-change ISR
    if(PCMSK1 & M1(PCINT10)) PCMSK1 &= ~M1(PCINT10);
}


__attribute__((noreturn))
void main(void)
{
    nRF24L01_t dev;

    init(&dev);
    /* set SMCR SE (Sleep Enable bit) */
    sleep_enable();

    for(;;)
    {
        cli();
        {
            dev.updated = (PCMSK1 & M1(PCINT10)) && (PINC & M1(PINC2));
            nRF24L01_event(&dev);
            unmask_irq();
        }
        sei();
        sleep_cpu();
    }
}

ISR(PCINT2_vect)
{
    mask_irq();
}
