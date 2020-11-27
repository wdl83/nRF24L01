#include <string.h>

#include "nRF24L01.h"

/* payload is always 32B in size
 * to handle unaligned data transmission header is injected as byte[0]
 * and padding bytes are appended at the end of data to aligned to 32B boundary */
typedef struct
{
    // payload size without padding
    uint8_t data_size : 5;
    uint8_t : 3;
} header_t;

#define PAYLOAD_SIZE nRF24L01_PAYLOAD_SIZE
#define MAX_DATA_SIZE (PAYLOAD_SIZE - sizeof(header_t))
#define PADDING_BYTE UINT8_C(0xCD)

static
void tx_reset(nRF24L01_t *dev)
{
    // TODO
}

static
void write_payload(nRF24L01_t *dev)
{
    if(dev->tx.end == dev->tx.begin) goto exit;

    { /* check last transmission status */
        union {
            struct {
                nRF24L01_status_t status;
                nRF24L01_fifo_status_t fifo_status;
            };
            nRF24L01_spi_cmd_t cmd[2];
            uint8_t byte[0];
        } xdata =
        {
            .cmd[0] = nRF24L01_R_REGISTER(nRF24L01_ADDR_fifo_status),
            .cmd[1] = nRF24L01_NOP
        };

        dev->spi_xchg(xdata.byte, xdata.byte + sizeof(xdata));

        if(xdata.status.MAX_RT) goto transmission_error;
        if(xdata.status.TX_FULL) goto exit;
        if(!xdata.status.TX_DS && !xdata.fifo_status.TX_EMPTY) goto exit;

        /* clear TX_DS */
        if(xdata.status.TX_DS)
        {
            uint8_t wdata[] =
            {
                nRF24L01_W_REGISTER(nRF24L01_ADDR_status),
                (nRF24L01_status_t){.TX_DS = 1}.value
            };

            dev->spi_xchg(wdata, wdata + sizeof(wdata));
        }

        if(dev->tx.begin == dev->tx.end) goto transmission_complete;
    }

    { /* write payload, must be written in continuously ~ one spi_xchg call */
        const size_t size = dev->tx.end - dev->tx.begin;
        const uint8_t data_size = size >= MAX_DATA_SIZE ? MAX_DATA_SIZE : size;
        union {
            struct {
                union {
                    nRF24L01_status_t status;
                    nRF24L01_spi_cmd_t cmd;
                };
                union {
                    header_t header;
                    uint8_t data[MAX_DATA_SIZE];
                };
            };
            uint8_t byte[0];
        } xdata =
        {
            .cmd = nRF24L01_W_TX_PAYLOAD,
            .header.data_size = data_size
        };

        memcpy(xdata.data, dev->tx.begin, data_size);
        memset(xdata.data + data_size, PADDING_BYTE, MAX_DATA_SIZE - data_size);
        dev->tx.begin += data_size;
        dev->spi_xchg(xdata.byte, xdata.byte + sizeof(xdata));
        goto exit;
    }
transmission_error:
    {
        tx_reset(dev);
        if(dev->tx.err_cb) (*dev->tx.err_cb)(dev->tx.user_data);
        dev->tx.begin = NULL;
        dev->tx.end = NULL;
        goto exit;
    }
transmission_complete:
    {
        if(dev->tx.cb) (*dev->tx.cb)(dev->tx.user_data);
    }
exit:
    ; // this is required by syntax
}

static
void read_payload(nRF24L01_t *dev)
{
    if((size_t)(dev->rx.end - dev->rx.begin) < MAX_DATA_SIZE) goto exit;

    { /* check last reception status */
        union {
            nRF24L01_status_t status;
            nRF24L01_spi_cmd_t cmd;
            uint8_t byte[0];
        } xdata = {.cmd = nRF24L01_NOP};

        dev->spi_xchg(xdata.byte, xdata.byte + sizeof(xdata));

        if(!xdata.status.RX_DR) goto exit;
        if(!(nRF24L01_RX_PIPE_NUM > xdata.status.RX_P_NO)) goto exit;
    }
    { /* clear RX_DR */
        uint8_t wdata[] =
        {
            nRF24L01_W_REGISTER(nRF24L01_ADDR_status),
            (nRF24L01_status_t){.RX_DR = 1}.value
        };

        dev->spi_xchg(wdata, wdata + sizeof(wdata));
    }
    { /* read payload */
        union {
            struct {
                union {
                    nRF24L01_status_t status;
                    nRF24L01_spi_cmd_t cmd;
                };
                union {
                    header_t header;
                    uint8_t data[MAX_DATA_SIZE];
                };
            };
            uint8_t byte[0];
        } xdata = {.cmd = nRF24L01_R_RX_PAYLOAD};

        dev->spi_xchg(xdata.byte, xdata.byte + sizeof(xdata));
        memcpy(dev->rx.begin, xdata.data, xdata.header.data_size);
        dev->rx.begin += xdata.header.data_size;
        if(dev->rx.cb) (*dev->rx.cb)(dev->rx.begin, xdata.status.RX_P_NO, dev->rx.user_data);
    }
exit:
    ; // this is required by syntax
}

void nRF24L01_init(
    nRF24L01_t *dev,
    nRF24L01_ce_set_t ce_set,
    nRF24L01_spi_xchg_t spi_xchg)
{
    memset(dev, 0, sizeof(nRF24L01_t));
    dev->ce_set = ce_set;
    dev->spi_xchg = spi_xchg;

    { /* payload size is fixed by impl. */
        uint8_t pipe_num = nRF24L01_RX_PIPE_NUM;
        do
        {
            uint8_t wdata[] =
            {
                nRF24L01_ADDR_rx_pw_p(--pipe_num),
                PAYLOAD_SIZE
            };
            spi_xchg(wdata, wdata + sizeof(wdata));
        } while(pipe_num);
    }

}

void nRF24L01_event(nRF24L01_t *dev)
{
    if(!dev->updated) return;
    else dev->updated = 0;

    union {
        struct {
            nRF24L01_status_t status;
            nRF24L01_config_t config;
        };
        nRF24L01_spi_cmd_t cmd;
        uint8_t byte[0];
    } xdata = {.cmd = nRF24L01_R_REGISTER(nRF24L01_ADDR_config)};

    if(xdata.config.PRIM_RX) read_payload(dev);
    else write_payload(dev);
}

void nRF24L01_send(
    nRF24L01_t *dev,
    const uint8_t *begin, const uint8_t *const end,
    nRF24L01_send_cb_t cb,
    uintptr_t user_data)
{
    dev->tx.begin = begin;
    dev->tx.end = end;
    dev->tx.cb = cb;
    dev->rx.user_data = user_data;

    { /* set to PRIM_TX if needed */
        union {
            struct {
                nRF24L01_status_t status;
                nRF24L01_config_t config;
            };
            nRF24L01_spi_cmd_t cmd[2];
            uint8_t byte[0];
        } xdata =
        {
            .cmd[0] = nRF24L01_R_REGISTER(nRF24L01_ADDR_config),
            .cmd[1] = nRF24L01_NOP
        };

        dev->spi_xchg(xdata.byte, xdata.byte + sizeof(xdata));

        if(xdata.config.PRIM_RX)
        {
            xdata.cmd[0] = nRF24L01_W_REGISTER(nRF24L01_ADDR_config);
            xdata.config.PRIM_RX = 0;
            dev->spi_xchg(xdata.byte, xdata.byte + sizeof(xdata));
        }
    }

    write_payload(dev);
    dev->ce_set((nRF24L01_ce_t){.CE = 1});
}

void nRF24L01_recv(
    nRF24L01_t *dev,
    uint8_t *begin, const uint8_t *const end,
    nRF24L01_recv_cb_t cb,
    uintptr_t user_data)
{
    dev->rx.begin = begin;
    dev->rx.end = end;
    dev->rx.cb = cb;
    dev->rx.user_data = user_data;

    { /* set to PRIM_RX if needed */
        union {
            struct {
                nRF24L01_status_t status;
                nRF24L01_config_t config;
            };
            nRF24L01_spi_cmd_t  cmd[2];
            uint8_t byte[0];
        } xdata =
        {
            .cmd[0] = nRF24L01_R_REGISTER(nRF24L01_ADDR_config),
            .cmd[1] = nRF24L01_NOP
        };

        dev->spi_xchg(xdata.byte, xdata.byte + sizeof(xdata));

        if(!xdata.config.PRIM_RX)
        {
            xdata.cmd[0] = nRF24L01_W_REGISTER(nRF24L01_ADDR_config);
            xdata.config.PRIM_RX = 1;
            dev->spi_xchg(xdata.byte, xdata.byte + sizeof(xdata));
        }
    }
    dev->ce_set((nRF24L01_ce_t){.CE = 1});
}
