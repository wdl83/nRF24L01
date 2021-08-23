#include <string.h>

#include <drv/usart0.h>

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

typedef union
{
    struct
    {
        header_t header;
        uint8_t data[MAX_DATA_SIZE];
    };
    uint8_t byte[0];
} payload_t;

#define PADDING_BYTE UINT8_C(0xCD)
#define MIN(a, b) ((b) < (a) ? (b) : (a))

typedef struct
{
    nRF24L01_status_t status;
    nRF24L01_fifo_status_t fifo_status;
} state_t;

static
uint8_t read_register(nRF24L01_t *dev, uint8_t addr)
{
    union {
        struct {
            nRF24L01_status_t status;
            uint8_t data;
        };
        nRF24L01_spi_cmd_t cmd[2];
        uint8_t byte[0];
    } xdata =
    {
        .cmd[0] = nRF24L01_R_REGISTER(addr),
        .cmd[1] = nRF24L01_NOP
    };

    dev->spi_xchg(xdata.byte, xdata.byte + sizeof(xdata));
    return xdata.data;
}

static
void write_register(nRF24L01_t *dev, uint8_t addr, uint8_t data)
{
    uint8_t wdata[] =
    {
        nRF24L01_W_REGISTER(addr),
        data
    };
    dev->spi_xchg(wdata, wdata + sizeof(wdata));
}

static
state_t read_state(nRF24L01_t *dev)
{
    union {
        state_t state;
        nRF24L01_spi_cmd_t cmd[2];
        uint8_t byte[0];
    } xdata =
    {
        .cmd[0] = nRF24L01_R_REGISTER(nRF24L01_ADDR_fifo_status),
        .cmd[1] = nRF24L01_NOP
    };

    dev->spi_xchg(xdata.byte, xdata.byte + sizeof(xdata));
    return xdata.state;
}

static
void tx_reset(nRF24L01_t *dev)
{
    {
        uint8_t wdata[] =
        {
            nRF24L01_FLUSH_TX
        };
        dev->spi_xchg(wdata, wdata + sizeof(wdata));
    }

    write_register(
        dev,
        nRF24L01_ADDR_status,
        (nRF24L01_status_t){.MAX_RT = 1, .TX_DS = 1}.value);
}

static
void on_transmission_error(
    nRF24L01_t *dev,
    nRF24L01_status_t status,
    nRF24L01_fifo_status_t fifo_status)
{
    tx_reset(dev);

    const nRF24L01_err_cb_t err_cb = dev->tx.err_cb;
    const uintptr_t user_data = dev->tx.user_data;

    dev->tx.begin = NULL;
    dev->tx.end = NULL;
    dev->tx.cb = NULL;
    dev->tx.err_cb = NULL;
    dev->tx.user_data = 0;

    if(err_cb) (*err_cb)(status, fifo_status, user_data);
}

static
void write_payload(nRF24L01_t *dev)
{
    if(dev->tx.end == dev->tx.begin) return;
    /* write payload, must be written in continuously ~ one spi_xchg call */
    const size_t size = dev->tx.end - dev->tx.begin;
    const uint8_t data_size = size >= MAX_DATA_SIZE ? MAX_DATA_SIZE : size;
    union {
        struct {
            nRF24L01_spi_cmd_t cmd;
            header_t header;
            uint8_t data[MAX_DATA_SIZE];
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
}

static
void send(nRF24L01_t *dev)
{
    state_t state = read_state(dev);

    if(state.status.MAX_RT)
    {
        on_transmission_error(dev, state.status, state.fifo_status);
        goto exit;
    }
    if(state.status.TX_FULL) goto exit;
    if(!state.status.TX_DS && !state.fifo_status.TX_EMPTY) goto exit;

    if(state.status.TX_DS)
    {
        /* clear data sent, TX FIFO interrupt */
        write_register(dev, nRF24L01_ADDR_status, (nRF24L01_status_t){.TX_DS = 1}.value);
    }

    if(dev->tx.begin == dev->tx.end) goto transmission_complete;

    write_payload(dev);
    goto exit;

transmission_complete:
    {
        const nRF24L01_send_cb_t cb = dev->tx.cb;
        const uintptr_t user_data = dev->tx.user_data;

        dev->tx.begin = NULL;
        dev->tx.end = NULL;
        dev->tx.cb = NULL;
        dev->tx.err_cb = NULL;
        dev->tx.user_data = 0;

        if(cb) (*cb)(user_data);
    }
exit:
    ; // this is required by syntax
}

static
void on_reception_error(
    nRF24L01_t *dev,
    nRF24L01_status_t status,
    nRF24L01_fifo_status_t fifo_status)
{
    const nRF24L01_err_cb_t err_cb = dev->rx.err_cb;
    const uintptr_t user_data = dev->rx.user_data;

    dev->rx.begin = NULL;
    dev->rx.end = NULL;
    dev->rx.cb = NULL;
    dev->rx.err_cb = NULL;
    dev->rx.user_data = 0;
    usart0_send_str("on_reception_error\n");

    // TODO FLUSH RX FIFO

    if(err_cb) (*err_cb)(status, fifo_status, user_data);
}

static
void read_payload(nRF24L01_t *dev, uint8_t max_size)
{
    union {
        struct {
            nRF24L01_spi_cmd_t cmd;
            payload_t payload;
        };
        uint8_t byte[0];
    } xdata = {.cmd = nRF24L01_R_RX_PAYLOAD};

    memset(xdata.payload.data, nRF24L01_NOP, sizeof(xdata.payload.data));

    const size_t capacity = dev->rx.end - dev->rx.begin;
    const uint8_t size = MIN(capacity, max_size);

    dev->spi_xchg(xdata.byte, xdata.byte + sizeof(nRF24L01_spi_cmd_t) + size);

    if(size >= xdata.payload.header.data_size)
    {
        memcpy(dev->rx.begin, xdata.payload.data, xdata.payload.header.data_size);
        dev->rx.begin += xdata.payload.header.data_size;
    }
    else
    {
        /* TODO: report error */
    }

}

static
void clear_rx_data_ready(nRF24L01_t *dev)
{
    /* clear RX_DR */
    write_register(dev, nRF24L01_ADDR_status, (nRF24L01_status_t){.RX_DR = 1}.value);
}

static
void recv(nRF24L01_t *dev)
{
    state_t state = read_state(dev);

    if(state.fifo_status.RX_EMPTY)
    {
        clear_rx_data_ready(dev);
        // TODO FLUSH RX FIFO, report error
        goto exit;
    }

    if(!(nRF24L01_RX_PIPE_NUM > state.status.RX_P_NO))
    {
        on_reception_error(dev, state.status, state.fifo_status);
        goto exit;
    }

    nRF24L01_payload_len_t payload_len =
    {
        .value = read_register(dev, nRF24L01_ADDR_rx_pw_p(state.status.RX_P_NO))
    };

    read_payload(dev, payload_len.RX_PW);

    if(state.status.RX_DR) clear_rx_data_ready(dev);

    goto reception_complete;
reception_complete:
    {
        uint8_t *const begin = dev->rx.begin;
        const nRF24L01_recv_cb_t cb = dev->rx.cb;
        const uintptr_t user_data = dev->rx.user_data;

        dev->rx.begin = NULL;
        dev->rx.end = NULL;
        dev->rx.cb = NULL;
        dev->rx.user_data = 0;

        if(cb) (*cb)(begin, state.status.RX_P_NO, user_data);
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

    /* payload size is fixed by impl. */
    uint8_t pipe_num = nRF24L01_RX_PIPE_NUM;
    do
    {
        write_register(dev, nRF24L01_ADDR_rx_pw_p(--pipe_num), PAYLOAD_SIZE);
    } while(pipe_num);
}

void nRF24L01_event(nRF24L01_t *dev)
{
    if(!dev->updated) return;
    else dev->updated = 0;

    nRF24L01_config_t config = {.value = read_register(dev, nRF24L01_ADDR_config)};

    if(config.PRIM_RX) recv(dev);
    else send(dev);
}

void nRF24L01_send(
    nRF24L01_t *dev,
    const uint8_t *begin, const uint8_t *const end,
    nRF24L01_send_cb_t cb,
    nRF24L01_err_cb_t err_cb,
    uintptr_t user_data)
{
    dev->tx.begin = begin;
    dev->tx.end = end;
    dev->tx.cb = cb;
    dev->tx.err_cb = err_cb;
    dev->tx.user_data = user_data;

    /* set to PRIM_TX if needed */
    nRF24L01_config_t config = {.value = read_register(dev, nRF24L01_ADDR_config)};

    if(config.PRIM_RX)
    {
        config.PRIM_RX = 0;
        write_register(dev, nRF24L01_ADDR_config, config.value);
    }
    write_payload(dev);
    dev->ce_set((nRF24L01_ce_t){.CE = 1});
}

void nRF24L01_recv(
    nRF24L01_t *dev,
    uint8_t *begin, const uint8_t *const end,
    nRF24L01_recv_cb_t cb,
    nRF24L01_err_cb_t err_cb,
    uintptr_t user_data)
{
    dev->rx.begin = begin;
    dev->rx.end = end;
    dev->rx.cb = cb;
    dev->rx.err_cb = err_cb;
    dev->rx.user_data = user_data;

    /* set to PRIM_RX if needed */
    nRF24L01_config_t config = {.value = read_register(dev, nRF24L01_ADDR_config)};

    if(!config.PRIM_RX)
    {
        config.PRIM_RX = 1;
        write_register(dev, nRF24L01_ADDR_config, config.value);
    }
    dev->ce_set((nRF24L01_ce_t){.CE = 1});
}

nRF24L01_rpd_t nRF24L01_rpd(nRF24L01_t *dev)
{
    return (nRF24L01_rpd_t){.value = read_register(dev, nRF24L01_ADDR_rpd)};
}
