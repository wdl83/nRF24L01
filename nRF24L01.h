#pragma once

#include <stddef.h>
#include <stdint.h>

/* nRF24L01 SPI instruction set ----------------------------------------------*/
#define nRF24L01_R_REGISTER(addr) (0x1F & (addr)) // b000? ????
#define nRF24L01_W_REGISTER(addr) (0x20 | (0x1F & addr)) // b001? ????
#define nRF24L01_R_RX_PAYLOAD     UINT8_C(0x61) // b0110 0001
#define nRF24L01_W_TX_PAYLOAD     UINT8_C(0xA0) // b1010 0000
#define nRF24L01_FLUSH_TX         UINT8_C(0xE1) // b1110 0001
#define nRF24L01_FLUSH_RX         UINT8_C(0xE2) // b1110 0010
#define nRF24L01_REUSE_TX_PL      UINT8_C(0xE3) // b1110 0011
#define nRF24L01_NOP              UINT8_C(0xFF) // b1111 1111

typedef uint8_t nRF24L01_spi_cmd_t;
/*----------------------------------------------------------------------------*/

/* nRF24L01 register addresses -----------------------------------------------*/
#define nRF24L01_ADDR_config      0x00
#define nRF24L01_ADDR_en_aa       0x01
#define nRF24L01_ADDR_en_rxaddr   0x02
#define nRF24L01_ADDR_setup_aw    0x03
#define nRF24L01_ADDR_setup_retr  0x04
#define nRF24L01_ADDR_rf_ch       0x05
#define nRF24L01_ADDR_rf_setup    0x06
#define nRF24L01_ADDR_status      0x07
#define nRF24L01_ADDR_observe_tx  0x08
#define nRF24L01_ADDR_cd          0x09
#define nRF24L01_ADDR_rx_addr_p0  0x0A
#define nRF24L01_ADDR_rx_addr_p1  0x0B
#define nRF24L01_ADDR_rx_addr_p2  0x0C
#define nRF24L01_ADDR_rx_addr_p3  0x0D
#define nRF24L01_ADDR_rx_addr_p4  0x0E
#define nRF24L01_ADDR_rx_addr_p5  0x0F
#define nRF24L01_ADDR_tx_addr     0x10
#define nRF24L01_ADDR_rx_pw_p0    0x11
#define nRF24L01_ADDR_rx_pw_p1    0x12
#define nRF24L01_ADDR_rx_pw_p2    0x13
#define nRF24L01_ADDR_rx_pw_p3    0x14
#define nRF24L01_ADDR_rx_pw_p4    0x15
#define nRF24L01_ADDR_rx_pw_p5    0x16
#define nRF24L01_ADDR_fifo_status 0x17

#define nRF24L01_ADDR_rx_addr_p(i)  (nRF24L01_ADDR_rx_addr_p0 + (i))
#define nRF24L01_ADDR_rx_pw_p(i)  (nRF24L01_ADDR_rx_pw_p0 + (i))
/*----------------------------------------------------------------------------*/

/* nRF24L01 registers --------------------------------------------------------*/
typedef union
{
    struct
    {
        uint8_t PRIM_RX : 1; // Primary, 1: PRX, 0: PTX                   0 @PoR
        uint8_t PWR_UP : 1; // 1: power up, 0: power down                 1 @PoR
        uint8_t CRCO : 1; // 1: 2 byte CRC, 0: 1 byte CRC                 0 @PoR
        uint8_t EN_CRC : 1; // 1: CRC enable 0: CRC disable               1 @PoR
        uint8_t MASK_MAX_RT : 1;                                       // 0 @PoR
        uint8_t MASK_TX_DS : 1;                                        // 0 @PoR
        uint8_t MASK_RX_DR : 1;                                        // 0 @PoR
        uint8_t : 1; // MSB                                            // 0 @PoR
    };
    uint8_t value;
    uint8_t byte[0];
} nRF24L01_config_t;

typedef union
{
    struct
    {
        // enable auto acknowledgement
        uint8_t ENAA_P0 : 1;
        uint8_t ENAA_P1 : 1;
        uint8_t ENAA_P2 : 1;
        uint8_t ENAA_P3 : 1;
        uint8_t ENAA_P4 : 1;
        uint8_t ENAA_P5 : 1;
        uint8_t : 2; // MSB
    };
    uint8_t value;
    uint8_t byte[0];
} nRF24L01_en_aa_t;

typedef union
{
    struct
    {
        uint8_t ERX_P0 : 1;
        uint8_t ERX_P1 : 1;
        uint8_t ERX_P2 : 1;
        uint8_t ERX_P3 : 1;
        uint8_t ERX_P4 : 1;
        uint8_t ERX_P5 : 1;
        uint8_t : 2; // MSB
    };
    uint8_t value;
    uint8_t byte[0];
} nRF24L01_en_rxaddr_t;

typedef union
{
    struct
    {
        uint8_t AW : 2; // addr width 0: illegal, 1: 3B, 2: 4B, 3: 5B (default@PoR)
        uint8_t : 6; // MSB
    };
    uint8_t value;
    uint8_t byte[0];
} nRF24L01_setup_aw_t;

typedef union
{
    struct
    {
        uint8_t ARC : 4; // Auto Retransmit Count [0-15]
        uint8_t ARD : 4;  // MSB, Auto Re-transmit Delay
    };
    uint8_t value;
    uint8_t byte[0];
} nRF24L01_setup_retr_t;

typedef union
{
    struct
    {
        uint8_t RF_CH : 7; // RF channel [0:127]
        uint8_t : 1; // MSB
    };
    uint8_t value;
    uint8_t byte[0];
} nRF24L01_rf_ch_t;

typedef union
{
    struct
    {
        uint8_t LNA_HCURR : 1; // Setup LNA gain
        uint8_t RF_PWR : 2; // RF TX output power [0: 18, 1: 12, 2: 6, 3: 0]dBm
        uint8_t RF_DR : 1; // Data Rate, 1: 2Mbps, 0: 1Mbps
        uint8_t PLL_LOCK : 1; // Force PLL lock signal (test only)
        uint8_t : 3; // MSB
    };
    uint8_t value;
    uint8_t byte[0];
} nRF24L01_rf_setup_t;

typedef union
{
    struct
    {
        uint8_t TX_FULL : 1;
        uint8_t RX_P_NO : 3;
        uint8_t MAX_RT : 1; // max TX retries interrupt,  set to clear
        uint8_t TX_DS : 1; // data sent (TX FIFO interrupt), set to clear
        uint8_t RX_DR : 1; // data ready (RX FIFO interrupt), set to clear
        uint8_t : 1; // MSB
    };
    uint8_t value;
    uint8_t byte[0];
} nRF24L01_status_t;

typedef union
{
    struct
    {
        uint8_t ARC_CNT : 4; // resent packets count
        uint8_t PLOS_CNT : 4; // MSB, lost packets count
    };
    uint8_t value;
    uint8_t byte[0];
} nRF24L01_observe_tx_t;

typedef union
{
    struct
    {
        uint8_t CD : 1; // carrier detect
        uint8_t : 7; // MSB
    };
    uint8_t value;
    uint8_t byte[0];
} nRF24L01_cd_t;

typedef union
{
    uint8_t addr[5];  // 40 bits
    uint8_t byte[0];
} nRF24L01_addr40_t;

typedef union
{
    // 40 bits, LSb p5 MSb p1[1:4] address is shared with p1
    uint8_t addr;
    uint8_t byte[0];
} nRF24L01_addr8_t;

typedef nRF24L01_addr40_t nRF24L01_tx_addr_t;               // 0xE7E7E7E7E7 @PoR
typedef nRF24L01_addr40_t nRF24L01_rx_addr_p0_t;            // 0xE7E7E7E7E7 @PoR
typedef nRF24L01_addr40_t nRF24L01_rx_addr_p1_t;            // 0xC2C2C2C2C2 @PoR
typedef nRF24L01_addr8_t nRF24L01_rx_addr_p2_t;                     // 0xC3 @PoR
typedef nRF24L01_addr8_t nRF24L01_rx_addr_p3_t;                     // 0xC4 @PoR
typedef nRF24L01_addr8_t nRF24L01_rx_addr_p4_t;                     // 0xC5 @PoR
typedef nRF24L01_addr8_t nRF24L01_rx_addr_p5_t;                     // 0xC6 @PoR

typedef union
{
    struct
    {
        uint8_t RX_PW : 6; // number of bytes in RX payload
        uint8_t : 2; // MSB
    };
    uint8_t value;
    uint8_t byte[0];
} nRF24L01_payload_len_t;

typedef nRF24L01_payload_len_t nRF24L01_rx_pw_p0_t;
typedef nRF24L01_payload_len_t nRF24L01_rx_pw_p1_t;
typedef nRF24L01_payload_len_t nRF24L01_rx_pw_p2_t;
typedef nRF24L01_payload_len_t nRF24L01_rx_pw_p3_t;
typedef nRF24L01_payload_len_t nRF24L01_rx_pw_p4_t;
typedef nRF24L01_payload_len_t nRF24L01_rx_pw_p5_t;

typedef union
{
    struct
    {
        uint8_t RX_EMPTY : 1;
        uint8_t RX_FULL : 1;
        uint8_t : 2;
        uint8_t TX_EMPTY : 1;
        uint8_t TX_FULL : 1;
        uint8_t TX_REUSE : 1; // reuse last sent data packet
        uint8_t : 1; // MSB
    };
    uint8_t value;
    uint8_t byte[0];
} nRF24L01_fifo_status_t;
/*----------------------------------------------------------------------------*/

typedef union
{
    struct
    {
        uint8_t data[32];
    };
    uint8_t byte[0];
} nRF24L01_payload_t;

#define nRF24L01_PAYLOAD_SIZE (sizeof(nRF24L01_payload_t))
#define nRF24L01_RX_PIPE_NUM 6

typedef union
{
    struct
    {
        uint8_t CE : 1; // Chip Enable pin state
        uint8_t : 7;
    };

    uint8_t byte[0];
} nRF24L01_ce_t;

typedef
void (*nRF24L01_recv_cb_t)(uint8_t *curr, uint8_t pipe_no, uintptr_t);
typedef
void (*nRF24L01_send_cb_t)(uintptr_t);
typedef
void (*nRF24L01_spi_xchg_t)(uint8_t *begin, const uint8_t *const end);
typedef
void (*nRF24L01_ce_set_t)(nRF24L01_ce_t);

typedef struct
{
    nRF24L01_ce_set_t ce_set;
    nRF24L01_spi_xchg_t spi_xchg;
    struct
    {
        const uint8_t *begin;
        const uint8_t *end;
        nRF24L01_send_cb_t cb;
        nRF24L01_send_cb_t err_cb;
        uintptr_t user_data;
    } tx;
    struct
    {
        uint8_t *begin;
        const uint8_t *end;
        nRF24L01_recv_cb_t cb;
        uintptr_t user_data;
    } rx;
    struct
    {
        uint8_t updated : 1;
        uint8_t : 7;
    };
} nRF24L01_t;

/* SPI must be active before calling init() */
void nRF24L01_init(
    nRF24L01_t *,
    nRF24L01_ce_set_t,
    nRF24L01_spi_xchg_t);

#define nRF24L01_CFG(dev, tag, ...) \
    { \
        union { \
            struct { \
                nRF24L01_spi_cmd_t cmd; \
                nRF24L01_##tag##_t data; \
            }; \
            uint8_t byte[0]; \
        } xdata = \
        { \
            .cmd = nRF24L01_W_REGISTER(nRF24L01_ADDR_##tag), \
            .data = {__VA_ARGS__} \
        }; \
        (dev)->spi_xchg(xdata.byte, xdata.byte + sizeof(xdata)); \
    }

void nRF24L01_event(nRF24L01_t *);

void nRF24L01_send(
    nRF24L01_t *,
    const uint8_t *begin, const uint8_t *const end,
    nRF24L01_send_cb_t,
    uintptr_t user_data);

void nRF24L01_recv(
    nRF24L01_t *,
    uint8_t *begin, const uint8_t *const end,
    nRF24L01_recv_cb_t,
    uintptr_t user_data);
