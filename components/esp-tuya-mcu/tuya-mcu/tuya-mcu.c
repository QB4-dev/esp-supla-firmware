#include "tuya-mcu.h"
#include "platform.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>

#define PID_LEN 16 // Product ID length
#define VER_LEN 5  // Version length

#define RX_BUF_SIZE 256
#define TX_BUF_SIZE 256

struct tuya_mcu {
    char                product_id[PID_LEN + 1]; // Product ID
    char                version[VER_LEN + 1];    // Version
    enum tuya_mcu_state state;                   // Device state
    uint32_t            last_heartbeat;          // Last heartbeat timestamp
    uint32_t            last_query;              // Last query timestamp
    bool                heartbeat_received;      // Heartbeat received flag

    tuya_mcu_state_handler_t state_handler;     // State handler
    void                    *state_handler_arg; // Argument for state handler

    tuya_mcu_config_handler_t config_handler;     // Configuration handler
    void                     *config_handler_arg; // Argument for config handler

    tuya_mcu_dp_handler_t dp_handler;     // Data point handler
    void                 *dp_handler_arg; // Argument for data point handler

    void   *uart_context;
    uint8_t rx_buf[RX_BUF_SIZE];
    uint8_t tx_buf[TX_BUF_SIZE];
    size_t  rx_pos;
    size_t  tx_pos;
};

int tuya_mcu_init(tuya_mcu_t *mcu, void *uart_ctx)
{
    if (!mcu || !uart_ctx)
        return -1;

    *mcu = calloc(1, sizeof(struct tuya_mcu));
    if (!*mcu)
        return -1;

    // Initialize MCU structure
    (*mcu)->state = TUYA_MCU_INIT_HEARTBEAT;
    memset((*mcu)->product_id, 0, sizeof((*mcu)->product_id));
    memset((*mcu)->version, 0, sizeof((*mcu)->version));
    (*mcu)->rx_pos = 0;
    (*mcu)->tx_pos = 0;
    (*mcu)->uart_context = uart_ctx;
    return 0;
}

int tuya_mcu_deinit(tuya_mcu_t mcu)
{
    if (!mcu)
        return -1;

    free(mcu);
    return 0;
}

char *tuya_mcu_get_product_id(tuya_mcu_t mcu)
{
    return mcu->product_id;
}
char *tuya_mcu_get_version(tuya_mcu_t mcu)
{
    return mcu->version;
}

int tuya_mcu_set_state_handler(tuya_mcu_t mcu, tuya_mcu_state_handler_t handler, void *arg)
{
    if (!mcu || !handler)
        return -1;

    mcu->state_handler = handler;
    mcu->state_handler_arg = arg;
    return 0;
}

int tuya_mcu_set_config_handler(tuya_mcu_t mcu, tuya_mcu_config_handler_t handler, void *arg)
{
    if (!mcu || !handler)
        return -1;

    mcu->config_handler = handler;
    mcu->config_handler_arg = arg;
    return 0;
}

int tuya_mcu_set_dp_handler(tuya_mcu_t mcu, tuya_mcu_dp_handler_t handler, void *arg)
{
    if (!mcu || !handler)
        return -1;

    mcu->dp_handler = handler;
    mcu->dp_handler_arg = arg;
    return 0;
}

static uint8_t get_check_sum(uint8_t *pack, size_t pack_len)
{
    int     i;
    uint8_t check_sum = 0;

    for (i = 0; i < pack_len; i++)
        check_sum += *pack++;

    return check_sum;
}

void print_hex(const unsigned char *buf, int len)
{
    for (int i = 0; i < len; ++i)
        printf("%02X ", buf[i]);
    printf("\n");
}

int parse_product_info(tuya_mcu_t mcu, const char *data, size_t len)
{
    char buf[256];
    if (len >= sizeof(buf))
        len = sizeof(buf) - 1;

    memcpy(buf, data, len);
    buf[len] = 0;

    // Extract fields
    char p[64] = { 0 }, v[32] = { 0 }, ir[32] = { 0 };
    int  m = -1, mt = -1, n = -1, low = -1;
    int  found_pid = 0, found_ver = 0;

    // Product ID
    char *found = strstr(buf, "\"p\":");
    if (found) {
        char *start = strchr(found + 3, '"');
        if (start) {
            start++;
            char *end = strchr(start, '"');
            if (end && end - start < (int)sizeof(p)) {
                strncpy(p, start, end - start);
                p[end - start] = 0;
                found_pid = 1;
                strncpy(mcu->product_id, p, PID_LEN);
                mcu->product_id[PID_LEN] = 0;
            }
        }
    }

    // Version
    found = strstr(buf, "\"v\":");
    if (found) {
        char *start = strchr(found + 3, '"');
        if (start) {
            start++;
            char *end = strchr(start, '"');
            if (end && end - start < (int)sizeof(v)) {
                strncpy(v, start, end - start);
                v[end - start] = 0;
                found_ver = 1;
                strncpy(mcu->version, v, VER_LEN);
                mcu->version[VER_LEN] = 0;
            }
        }
    }

    // m
    found = strstr(buf, "\"m\":");
    if (found)
        sscanf(found, "\"m\":%d", &m);

    // mt
    found = strstr(buf, "\"mt\":");
    if (found)
        sscanf(found, "\"mt\":%d", &mt);

    // n
    found = strstr(buf, "\"n\":");
    if (found)
        sscanf(found, "\"n\":%d", &n);

    // ir
    found = strstr(buf, "\"ir\":");
    if (found) {
        char *start = strchr(found + 4, '"');
        if (start) {
            start++;
            char *end = strchr(start, '"');
            if (end && end - start < (int)sizeof(ir)) {
                strncpy(ir, start, end - start);
                ir[end - start] = 0;
            }
        }
    }

    // low
    found = strstr(buf, "\"low\":");
    if (found)
        sscanf(found, "\"low\":%d", &low);

    // printf("Product Info:\n");
    // if (p[0])
    //     printf("  Product ID: %s\n", p);
    // if (v[0])
    //     printf("  Version: %s\n", v);
    // if (m != -1)
    //     printf("  m: %d\n", m);
    // if (mt != -1)
    //     printf("  mt: %d\n", mt);
    // if (n != -1)
    //     printf("  n: %d\n", n);
    // if (ir[0])
    //     printf("  ir: %s\n", ir);
    // if (low != -1)
    //     printf("  low: %d\n", low);

    return (found_pid && found_ver) ? 0 : -1;
}

static int tuya_frame_send(tuya_mcu_t mcu, uint8_t version, uint8_t cmd, const uint8_t *data,
                           size_t len)
{
    if (len > TX_BUF_SIZE - PROTOCOL_HEAD) // 6 bytes header + 1 byte checksum
        return -1;                         // Data too long

    mcu->tx_buf[0] = FRAME_FIRST;
    mcu->tx_buf[1] = FRAME_SECOND;
    mcu->tx_buf[2] = version;
    mcu->tx_buf[3] = cmd;
    mcu->tx_buf[4] = (len >> 8) & 0xFF; // Length high byte
    mcu->tx_buf[5] = len & 0xFF;        // Length low byte

    memcpy(mcu->tx_buf + 6, data, len);
    mcu->tx_buf[6 + len] = get_check_sum(mcu->tx_buf, 6 + len); // Checksum

    //    printf("TUYA frame tx: ");
    //    print_hex(mcu->tx_buf, PROTOCOL_HEAD + len);
    // Send the frame
    for (size_t i = 0; i < PROTOCOL_HEAD + len; ++i) {
        if (tuya_mcu_uart_tx(mcu->uart_context, mcu->tx_buf[i]) < 0) {
            return -1; // Error sending data
        }
    }
    return 0; // Success
}

static int tuya_frame_send_heartbeat(tuya_mcu_t mcu)
{
    // Send heartbeat frame
    mcu->last_heartbeat = tuya_mcu_get_tick();
    return tuya_frame_send(mcu, MCU_TX_VER, HEARTBEAT_CMD, NULL, 0);
}
static int tuya_frame_query_product_info(tuya_mcu_t mcu)
{
    // Send product info frame
    mcu->last_query = tuya_mcu_get_tick();
    return tuya_frame_send(mcu, MCU_TX_VER, PRODUCT_INFO_CMD, NULL, 0);
}
static int tuya_frame_send_wifi_mode_ack(tuya_mcu_t mcu)
{
    // Send product info frame
    mcu->last_query = tuya_mcu_get_tick();
    return tuya_frame_send(mcu, MCU_TX_VER, WIFI_MODE_CMD, NULL, 0);
}

int tuya_mcu_send_wifi_status(tuya_mcu_t mcu, uint8_t state)
{
    // Send wifi state info frame
    return tuya_frame_send(mcu, MCU_TX_VER, WIFI_STATE_CMD, &state, 1);
}

static int tuya_mcu_send_state_request(tuya_mcu_t mcu)
{
    // Send state query frame
    return tuya_frame_send(mcu, MCU_TX_VER, STATE_QUERY_CMD, NULL, 0);
}

int tuya_mcu_send_dp(tuya_mcu_t mcu, tuya_dp_t *dp)
{
    uint8_t buf[128];
    // Send data query frame
    tuya_dp_serialize(dp, buf, sizeof(buf));
    return tuya_frame_send(mcu, MCU_TX_VER, DATA_QUERT_CMD, buf, tuya_dp_get_len(dp));
}

static int tuya_frame_handle(tuya_mcu_t mcu, uint8_t ver, uint8_t cmd, uint8_t *data, size_t len)
{
    // Handle the received frame based on cmd
    switch (cmd) {
    case HEARTBEAT_CMD:
        //printf("Received Heartbeat Frame: ver=0x%02X cmd=0x%02X data=0x%02X\n", ver, cmd, data[0]);
        if (data[0] == 0x01)
            mcu->heartbeat_received = true; // Heartbeat received
        break;
    case PRODUCT_INFO_CMD:
        //printf("Received Product Info Frame: ver=0x%02X cmd=0x%02X\n", ver, cmd);
        parse_product_info(mcu, (const char *)data, len);
        break;
    case WORK_MODE_CMD:
        //printf("Received Work Mode Query Frame: ver=0x%02X cmd=0x%02X\n", ver, cmd);
        break;
    case WIFI_STATE_CMD:
        //printf("Received WiFi State Frame: ver=0x%02X cmd=0x%02X\n", ver, cmd);
        break;
    case WIFI_RESET_CMD:
        //printf("Received WiFi Reset Frame: ver=0x%02X cmd=0 x%02X data= x%02X\n", ver, cmd,data[0]);
        break;
    case WIFI_MODE_CMD:
        // printf("Received WiFi Mode Frame: ver=0x%02X cmd=0x%02X data= x%02X\n", ver, cmd, data[0]);
        tuya_frame_send_wifi_mode_ack(mcu);
        if (mcu->config_handler) {
            mcu->config_handler(mcu, mcu->config_handler_arg);
        }
        break;
    case DATA_QUERT_CMD: {
        //printf("Received Data Query Frame: ver=0x%02X cmd=0x%02X\n", ver, cmd);
    } break;
    case STATE_UPLOAD_CMD: {
        tuya_dp_t dp;
        //printf("Received State Upload Frame: ver=0x%02X cmd=0x%02X\n", ver, cmd);
        if (mcu->dp_handler) {
            parse_tuya_dp(data, len, &dp);
            mcu->dp_handler(mcu, &dp, mcu->dp_handler_arg);
        }
    } break;
    case STATE_QUERY_CMD:
        //printf("Received State Query Frame: ver=0x%02X cmd=0x%02X\n", ver, cmd);
        break;
    // Add more cases for other commands as needed
    default:
        //printf("Unknown command 0x%02X received\n", cmd);
        return -1;
    }
    return 0; // Success
}

static int tuya_frame_receive(tuya_mcu_t mcu)
{
    uint8_t byte;
    int     n;

    while ((n = tuya_mcu_uart_rx(mcu->uart_context, &byte)) > 0) {
        // Store received byte in buffer
        if (mcu->rx_pos < RX_BUF_SIZE) {
            mcu->rx_buf[mcu->rx_pos++] = byte;
        } else {
            // Buffer overflow, reset position
            mcu->rx_pos = 0;
        }

        // TUYA frame: 0x55 0xAA [version] [cmd] [lenH] [lenL] [data...] [checksum]
        while (mcu->rx_pos >= 6) {
            if (mcu->rx_buf[0] != FRAME_FIRST || mcu->rx_buf[1] != FRAME_SECOND) {
                // Shift buffer left until header found
                memmove(mcu->rx_buf, mcu->rx_buf + 1, --mcu->rx_pos);
                continue;
            }
            size_t len = (mcu->rx_buf[4] << 8) | mcu->rx_buf[5];
            size_t frame_len = 6 + len + 1; // header+ver+cmd+lenH+lenL+data+checksum
            if (mcu->rx_pos < frame_len)
                break; // Wait for more data

            // Validate checksum
            uint8_t check_sum = get_check_sum(mcu->rx_buf, 6 + len);
            if (check_sum == mcu->rx_buf[frame_len - 1]) {
                unsigned char  version = mcu->rx_buf[2];
                unsigned char  cmd = mcu->rx_buf[3];
                unsigned char *data = mcu->rx_buf + 6;

                // printf("TUYA frame rx: ");
                // print_hex(mcu->rx_buf, PROTOCOL_HEAD + len);
                tuya_frame_handle(mcu, version, cmd, data, len);
            } else {
                return -1; // Checksum error
            }
            // Remove processed frame from buffer
            if (mcu->rx_pos > frame_len) {
                memmove(mcu->rx_buf, mcu->rx_buf + frame_len, mcu->rx_pos - frame_len);
                mcu->rx_pos -= frame_len;
            } else {
                mcu->rx_pos = 0;
            }
        }
    }
    return 0;
}

static void tuya_mcu_state_change(tuya_mcu_t mcu, enum tuya_mcu_state new_state)
{
    if (mcu->state_handler) {
        mcu->state_handler(mcu, new_state, mcu->state_handler_arg);
    }
    mcu->state = new_state;
}

int tuya_mcu_tick(tuya_mcu_t mcu)
{
    uint32_t tick = tuya_mcu_get_tick();
    if (!mcu)
        return -1;

    switch (mcu->state) {
    case TUYA_MCU_INIT_HEARTBEAT:
        /* should send heartbeat frames every second */
        if (tick - mcu->last_heartbeat > 1000)
            tuya_frame_send_heartbeat(mcu);

        if (mcu->heartbeat_received)
            tuya_mcu_state_change(mcu, TUYA_MCU_QUERY_INFO);

        break;
    case TUYA_MCU_QUERY_INFO:
        /* should send product info query every 5 seconds */
        if (tick - mcu->last_query > 5000)
            tuya_frame_query_product_info(mcu);

        // Check if we have received product info
        if (strlen(mcu->product_id) > 0 && strlen(mcu->version) > 0) {
            tuya_mcu_send_state_request(mcu);
            tuya_mcu_state_change(mcu, TUYA_MCU_INITIALIZED);
        }
        break;

    case TUYA_MCU_INITIALIZED:
        /* should send heartbeat every 15 seconds  */
        if (tick - mcu->last_heartbeat > 15000)
            tuya_frame_send_heartbeat(mcu);

        break;
    default:
        break;
    }

    // Receive data from UART
    if (tuya_frame_receive(mcu) < 0) {
        return -1;
    }

    return 0;
}
