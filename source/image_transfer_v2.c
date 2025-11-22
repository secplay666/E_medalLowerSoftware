/******************************************************************************
 * @file image_transfer_v2.c
 * @brief Image transfer protocol V2 - Reliable frame-based protocol
 * @author Rewritten for stability
 ******************************************************************************/

#include "image_transfer_v2.h"
#include "uart_interface.h"
#include "flash_manager.h"
#include "crc_utils.h"
#include <string.h>
#include <stdio.h>
#include "uart.h"

/******************************************************************************
 * Defines
 ******************************************************************************/

// Debug output control
// CRITICAL: Disable debug output during data transfer to avoid UART conflicts
#define ENABLE_DEBUG_OUTPUT       0  // Set to 0 to disable debug logs during transfer, 1 to enable

// Protocol Marks
#define PROTO_START_MARK          0x55
#define PROTO_STOP_MARK           0xAA

// Command Types
#define CMD_START                 0x01
#define CMD_END                   0x02
#define FRAME_TYPE_IMAGE_DATA     0x10  // Only data frames, no header frame

// Response Types (Control Frames)
#define RESP_READY                0x03
#define RESP_ACK                  0x20
#define RESP_NAK                  0x21

// Detailed NAK Error Codes (for error diagnosis)
// 这些错误代码用于区分不同类型的 NAK 原因
#define RESP_NAK_CHECKSUM         0x22  // Checksum 错误
#define RESP_NAK_CRC              0x23  // CRC32 错误
#define RESP_NAK_STATE_MISMATCH   0x24  // 状态不匹配
#define RESP_NAK_BUFFER_OVERFLOW  0x25  // 缓冲区溢出
#define RESP_NAK_INVALID_FRAME    0x26  // 帧号超范围或长度错误
#define RESP_NAK_FLASH_WRITE_FAIL 0x27  // Flash 写入失败

#define RESP_COMPLETE             0x04
#define RESP_FAIL                 0x05

// Timeouts (in ms, checked via 1ms timer)
#define TIMEOUT_FRAME             3000
#define TIMEOUT_IDLE              5000

// Limits
#define MAX_RETRIES               5
#define IMAGE_PAGES               61
#define FRAME_PAYLOAD_SIZE        248

// Conditional debug macro
#if ENABLE_DEBUG_OUTPUT
    #define DEBUG_PRINTF(fmt, ...) UARTIF_uartPrintf(0, fmt, ##__VA_ARGS__)
#else
    #define DEBUG_PRINTF(fmt, ...) do {} while(0)
#endif

/******************************************************************************
 * Types
 ******************************************************************************/

typedef enum {
    RX_STATE_IDLE,
    RX_STATE_WAITING_DATA,      // Waiting for 61 data frames
    RX_STATE_VERIFY_COMPLETE,   // All frames received, verifying
    RX_STATE_COMPLETE           // Transfer complete
} rx_state_t;

typedef struct {
    rx_state_t state;
    uint8_t frame_buf[259];        // Single frame buffer
    uint16_t frame_idx;            // Current position in frame
    uint16_t current_frame_num;    // Last frame number received
    uint8_t current_slot_id;       // Current slot ID
    uint32_t timeout_counter;
    uint32_t total_frames_received;
    uint64_t frame_bitmap;         // Track which frames received
} rx_context_t;

/******************************************************************************
 * Static Variables
 ******************************************************************************/

static rx_context_t rx_ctx;

/******************************************************************************
 * Helper Functions
 ******************************************************************************/

/**
 * @brief Calculate simple checksum (sum of all bytes)
 */
static uint8_t calc_checksum(const uint8_t *data, uint16_t len)
{
    uint16_t i;
    uint8_t sum = 0;

    for (i = 0; i < len; i++) {
        sum += data[i];
    }
    return sum;
}

/**
 * @brief Send a control frame (START/END/READY/COMPLETE/FAIL)
 */
static void send_ctrl_frame(uint8_t command)
{
    uint8_t frame[4];
    int i;
//    char *cmd_str;

    frame[0] = PROTO_START_MARK;
    frame[1] = command;
    frame[2] = calc_checksum(&frame[0], 2);
    frame[3] = PROTO_STOP_MARK;

    for (i = 0; i < 4; i++) {
        Uart_SendData(UARTCH1, frame[i]);
    }

    // CRITICAL: Do NOT output debug log after sending control frame
    // It will interfere with UART protocol communication
    // Use DEBUG_PRINTF instead which can be disabled
    // switch (command) {
    //     case RESP_READY:    cmd_str = "READY";    break;
    //     case RESP_COMPLETE: cmd_str = "COMPLETE"; break;
    //     case RESP_FAIL:     cmd_str = "FAIL";     break;
    //     default:            cmd_str = "UNKNOWN";  break;
    // }
    // DEBUG_PRINTF("[IMG_V2] TX CTRL: %s (0x%02X)\r\n", cmd_str, command);
}

/**
 * @brief Send ACK/NAK response
 * @param resp_type Response type (RESP_ACK, RESP_NAK, or detailed RESP_NAK_xxx codes)
 * @param frame_num Frame number being acknowledged
 */
static void send_response(uint8_t resp_type, uint16_t frame_num)
{
    uint8_t frame[6];
    int i;

    frame[0] = PROTO_START_MARK;
    frame[1] = resp_type;
    frame[2] = (uint8_t)(frame_num & 0xFF);
    frame[3] = (uint8_t)((frame_num >> 8) & 0xFF);
    frame[4] = calc_checksum(&frame[0], 4);
    frame[5] = PROTO_STOP_MARK;

    for (i = 0; i < 6; i++) {
        Uart_SendData(UARTCH1, frame[i]);
    }

    // CRITICAL: Do NOT output debug log after sending response frame
    // It will interfere with UART protocol communication
    // Optional debug output (only when ENABLE_DEBUG_OUTPUT=1):
    // DEBUG_PRINTF("[IMG_V2] TX RESP: type=0x%02X, frame=%d\r\n", resp_type, frame_num);
}

/**
 * @brief Process control frame (START/END)
 */
static uint8_t process_ctrl_frame(void)
{
    uint8_t command;
    uint8_t checksum;
    uint8_t expected_checksum;

    // Expected: [0x55, CMD, CHECKSUM, 0xAA]
    if (rx_ctx.frame_idx < 4) {
        DEBUG_PRINTF("[IMG_V2_DEBUG] CTRL frame incomplete: idx=%d/4\r\n", rx_ctx.frame_idx);
        return 0; // Not complete
    }

    command = rx_ctx.frame_buf[1];
    checksum = rx_ctx.frame_buf[2];
    expected_checksum = calc_checksum(&rx_ctx.frame_buf[0], 2);

    DEBUG_PRINTF("[IMG_V2_DEBUG] CTRL frame check: cmd=0x%02X, checksum=%02X (expected=%02X)\r\n",
                    command, checksum, expected_checksum);

    if (checksum != expected_checksum) {
        DEBUG_PRINTF("[IMG_V2] ERROR CTRL checksum error: got %02X, expected %02X\r\n", checksum, expected_checksum);
        return 0;
    }

    DEBUG_PRINTF("[IMG_V2] OK RX CTRL: cmd=0x%02X\r\n", command);
    return command;
}

/**
 * @brief Process data frame (image data only, no header)
 */
static uint8_t process_data_frame(void)
{
    uint8_t frame_type;
    uint16_t frame_num;
    uint8_t slot_id;
    uint32_t crc_rx;
    uint8_t checksum_rx;
    uint8_t checksum_calc;
    uint8_t *payload;
    uint32_t crc_calc;
    flash_result_t result;
    uint16_t data_key;

    // Expected: [0x55, FRAME_TYPE, FRAME_NUM_L, FRAME_NUM_H, SLOT_ID, CRC(4), PAYLOAD(248), CHECKSUM, 0xAA]
    // frame_idx should be exactly 259 (including STOP_MARK)
    if (rx_ctx.frame_idx != 259) {
        DEBUG_PRINTF("[IMG_V2] ERROR: Frame size mismatch: idx=%d (expected=259)\r\n", rx_ctx.frame_idx);
        // Extract frame number for NAK
        if (rx_ctx.frame_idx >= 4) {
            uint16_t frame_num = rx_ctx.frame_buf[2] | (rx_ctx.frame_buf[3] << 8);
            send_response(RESP_NAK_INVALID_FRAME, frame_num);  // ✅ 详细错误代码：长度错误
        }
        rx_ctx.frame_idx = 0;
        return 0; // Not valid
    }

    frame_type = rx_ctx.frame_buf[1];
    frame_num = rx_ctx.frame_buf[2] | (rx_ctx.frame_buf[3] << 8);
    slot_id = rx_ctx.frame_buf[4];
    crc_rx = rx_ctx.frame_buf[5] | (rx_ctx.frame_buf[6] << 8) |
                      (rx_ctx.frame_buf[7] << 16) | (rx_ctx.frame_buf[8] << 24);
    checksum_rx = rx_ctx.frame_buf[257];
    checksum_calc = calc_checksum(&rx_ctx.frame_buf[0], 257);

    // Verify checksum
    if (checksum_rx != checksum_calc) {
        DEBUG_PRINTF("[IMG_V2] DATA checksum error: rx=0x%02X, calc=0x%02X\r\n", checksum_rx, checksum_calc);
        send_response(RESP_NAK_CHECKSUM, frame_num);  // ✅ 详细错误代码：Checksum 错误
        rx_ctx.frame_idx = 0;
        return 0;
    }

    // Verify payload CRC
    payload = &rx_ctx.frame_buf[9];
    crc_calc = calculate_crc32_default(payload, FRAME_PAYLOAD_SIZE);

    if (crc_rx != crc_calc) {
        DEBUG_PRINTF("[IMG_V2] DATA CRC error: rx=0x%08lX, calc=0x%08lX\r\n", crc_rx, crc_calc);
        send_response(RESP_NAK_CRC, frame_num);  // ✅ 详细错误代码：CRC 错误
        rx_ctx.frame_idx = 0;
        return 0;
    }

    // Verify frame number is valid (0-60)
    if (frame_num > MAX_FRAME_NUM) {
        DEBUG_PRINTF("[IMG_V2] Invalid frame_num: %d (max=%d)\r\n", frame_num, MAX_FRAME_NUM);
        send_response(RESP_NAK_INVALID_FRAME, frame_num);  // ✅ 详细错误代码：帧号超范围
        rx_ctx.frame_idx = 0;
        return 0;
    }

    // Save frame info
    rx_ctx.current_frame_num = frame_num;
    rx_ctx.current_slot_id = slot_id;

    // Write data frame to flash (magic = BW_IMAGE_DATA)
    data_key = (uint16_t)((slot_id << 8) | frame_num);
    result = FM_writeData(MAGIC_BW_IMAGE_DATA, data_key, payload, FRAME_PAYLOAD_SIZE);

    if (result == FLASH_OK) {
        rx_ctx.frame_bitmap |= ((uint64_t)1 << frame_num);
        rx_ctx.total_frames_received++;
        DEBUG_PRINTF("[IMG_V2] Frame %d saved (total=%u): bitmap=0x%016llX\r\n",
                         frame_num, rx_ctx.total_frames_received, rx_ctx.frame_bitmap);
        send_response(RESP_ACK, frame_num);
    } else {
        DEBUG_PRINTF("[IMG_V2] Frame write failed: %d\r\n", result);
        send_response(RESP_NAK_FLASH_WRITE_FAIL, frame_num);  // ✅ 详细错误代码：Flash 写入失败
    }

    rx_ctx.frame_idx = 0;
    return 1; // Frame processed
}

/******************************************************************************
 * Public Functions
 ******************************************************************************/

void ImageTransferV2_Init(void)
{
    memset(&rx_ctx, 0, sizeof(rx_context_t));
    rx_ctx.state = RX_STATE_IDLE;
    UARTIF_uartPrintf(0, "[IMG_V2] Protocol V2 initialized\r\n");
    UARTIF_uartPrintf(0, "[IMG_V2] MAX_RETRIES=%d, IMAGE_PAGES=%d\r\n", MAX_RETRIES, IMAGE_PAGES);
    UARTIF_uartPrintf(0, "[IMG_V2] FRAME_PAYLOAD_SIZE=%d, TIMEOUT_FRAME=%dms\r\n", FRAME_PAYLOAD_SIZE, TIMEOUT_FRAME);
}

void ImageTransferV2_Process(void)
{
    uint8_t temp_buf[259];
    uint16_t temp_idx = 0u;
    uint16_t counter;
    counter = UARTIF_fetchDataFromUart(temp_buf, &temp_idx);
    if (counter > 0u)
    {
        UARTIF_uartPrintf(0, "counter is %d\n", counter);
        UARTIF_uartPrintf(0, "temp_idx is %d\n", temp_idx);
        UARTIF_uartPrintf(0, "temp_buf[0] is 0x%02x\n", temp_buf[0]);
        UARTIF_uartPrintf(0, "temp_buf[1] is 0x%02x\n", temp_buf[1]);
        // UARTIF_uartPrintf(0, "temp_buf[2] is 0x02%x\n", temp_buf[2]);
        // UARTIF_uartPrintf(0, "temp_buf[temp_idx-1] is 0x02%x\n", temp_buf[temp_idx-1u]);
        UARTIF_uartPrintf(0, "temp_buf[temp_idx-2] is 0x%02x\n", temp_buf[temp_idx-2u]);
        UARTIF_uartPrintf(0, "temp_buf[temp_idx-1] is 0x%02x\n", temp_buf[temp_idx-1u]);

    }

}

// void ImageTransferV2_Process(void)
// {
//     uint8_t byte;
//     uint16_t bytes_fetched;
//     uint8_t temp_buf[259];
//     uint16_t temp_idx;
//     uint16_t i;
//     uint8_t frame_type;
//     uint8_t result;
//     uint8_t cmd;
//     uint16_t expected_frames;
//     uint64_t expected_bitmap;
//     flash_result_t header_result;
//     // Try to fetch data from UART queue
//     temp_idx = 0;
//     // UARTIF_uartPrintf(0, "IT\r\n");

//     bytes_fetched = UARTIF_fetchDataFromUart(temp_buf, &temp_idx);

//     if (bytes_fetched == 0) {
//         rx_ctx.timeout_counter++;
//         if (rx_ctx.timeout_counter > TIMEOUT_FRAME && rx_ctx.state != RX_STATE_IDLE) {
//             DEBUG_PRINTF("[IMG_V2] TIMEOUT in state %d (counter=%u)\r\n",
//                             rx_ctx.state, rx_ctx.timeout_counter);
//             rx_ctx.timeout_counter = 0;
//         }
//         return;
//     }

//     // ==================== Debug: Output received data ====================
//     // DISABLED: Debug output during data transfer can interfere with UART communication
//     // Uncomment only for troubleshooting if needed
//     // UARTIF_uartPrintf(0, "[IMG_V2] RX %d bytes: ", bytes_fetched);
//     // for (i = 0; i < temp_idx && i < 20; i++) {
//     //     UARTIF_uartPrintf(0, "%02X ", temp_buf[i]);
//     // }
//     // if (temp_idx > 20) {
//     //     UARTIF_uartPrintf(0, "...");
//     // }
//     // UARTIF_uartPrintf(0, " [state=%d]\r\n", rx_ctx.state);

//     // Process fetched bytes
//     for (i = 0; i < temp_idx; i++) {
//         byte = temp_buf[i];

//         // Look for START_MARK
//         if (byte == PROTO_START_MARK) {
//             rx_ctx.frame_idx = 0;
//             rx_ctx.frame_buf[rx_ctx.frame_idx++] = byte;
//             rx_ctx.timeout_counter = 0;
//             continue;
//         }

//         // If not in frame, skip
//         if (rx_ctx.frame_idx == 0) {
//             continue;
//         }

//         // Add to frame
//         if (rx_ctx.frame_idx < 259) {
//             rx_ctx.frame_buf[rx_ctx.frame_idx++] = byte;
//         } else {
//             // CRITICAL: Frame buffer overflow detected
//             // This means frame was too long without STOP_MARK
//             // Send detailed NAK to notify PC of the error
//             DEBUG_PRINTF("[IMG_V2] ERROR: Frame buffer overflow at idx=%d, resetting\r\n", rx_ctx.frame_idx);

//             // Extract frame_num if possible (need at least 4 bytes)
//             if (rx_ctx.frame_idx >= 4) {
//                 uint16_t frame_num = rx_ctx.frame_buf[2] | (rx_ctx.frame_buf[3] << 8);
//                 send_response(RESP_NAK_BUFFER_OVERFLOW, frame_num);  // ✅ 详细错误代码：缓冲区溢出
//             }

//             rx_ctx.frame_idx = 0;
//             continue;
//         }

//         // Check for STOP_MARK
//         if (byte == PROTO_STOP_MARK && rx_ctx.frame_idx >= 4) {
//             // Try to process frame
//             frame_type = rx_ctx.frame_buf[1];
//             result = 0;

//             if (frame_type == CMD_START || frame_type == CMD_END) {
//                 cmd = process_ctrl_frame();
//                 if (cmd == CMD_START) {
//                     // Reset state and bitmap for new transfer
//                     rx_ctx.state = RX_STATE_WAITING_DATA;
//                     rx_ctx.frame_bitmap = 0;
//                     rx_ctx.total_frames_received = 0;
//                     send_ctrl_frame(RESP_READY);
//                     // CRITICAL: Do NOT output log after sending READY
//                     // It will interfere with PC understanding UART protocol
//                     // DEBUG_PRINTF("[IMG_V2] Transfer started, waiting for 61 data frames...\r\n");
//                 } else if (cmd == CMD_END) {
//                     rx_ctx.state = RX_STATE_VERIFY_COMPLETE;

//                     // Verify integrity: Check if all 61 frames received
//                     expected_frames = IMAGE_PAGES; // 61 frames (0-60)
//                     expected_bitmap = ((uint64_t)1 << IMAGE_PAGES) - 1; // All 61 bits set

//                     if ((rx_ctx.frame_bitmap & expected_bitmap) == expected_bitmap) {
//                         // All frames verified! Now write image header automatically
//                         header_result = FM_writeImageHeader(MAGIC_BW_IMAGE_HEADER, rx_ctx.current_slot_id);

//                         if (header_result == FLASH_OK) {
//                             rx_ctx.state = RX_STATE_COMPLETE;
//                             send_ctrl_frame(RESP_COMPLETE);
//                             // CRITICAL: Do NOT output log after sending COMPLETE
//                             // DEBUG_PRINTF("[IMG_V2] OK Transfer COMPLETE! All %u frames verified, header written (slot=%d)\r\n",
//                             //                          rx_ctx.total_frames_received, rx_ctx.current_slot_id);
//                         } else {
//                             send_ctrl_frame(RESP_FAIL);
//                             // DEBUG_PRINTF("[IMG_V2] ERROR Header write failed: %d\r\n", header_result);
//                         }
//                     } else {
//                         send_ctrl_frame(RESP_FAIL);
//                         // CRITICAL: Do NOT output log after sending FAIL
//                         // DEBUG_PRINTF("[IMG_V2] ERROR Transfer FAILED! Missing frames. Bitmap=0x%016llX, Expected=0x%016llX\r\n",
//                         //                         rx_ctx.frame_bitmap, expected_bitmap);
//                     }
//                 }
//                 rx_ctx.frame_idx = 0;
//             } else if (frame_type == FRAME_TYPE_IMAGE_DATA) {
//                 if (rx_ctx.state == RX_STATE_WAITING_DATA) {
//                     result = process_data_frame();
//                     // Continue waiting for more frames
//                 } else {
//                     // CRITICAL FIX: Must send NAK if state is wrong
//                     // Otherwise frame_idx is never reset and protocol hangs
//                     uint16_t frame_num = rx_ctx.frame_buf[2] | (rx_ctx.frame_buf[3] << 8);
//                     DEBUG_PRINTF("[IMG_V2] ERROR: Received data frame but state=%d (expected=%d), sending NAK\r\n",
//                                 rx_ctx.state, RX_STATE_WAITING_DATA);
//                     send_response(RESP_NAK_STATE_MISMATCH, frame_num);  // ✅ 详细错误代码：状态不匹配
//                     rx_ctx.frame_idx = 0;  // CRITICAL: Reset frame buffer
//                 }
//             }
//         }
//     }
// }

/**
 * @brief Get transfer statistics
 */
void ImageTransferV2_GetStats(image_transfer_stats_t *stats)
{
    if (stats) {
        stats->state = rx_ctx.state;
        stats->frames_received = rx_ctx.total_frames_received;
        stats->frame_bitmap = rx_ctx.frame_bitmap;
        stats->current_slot_id = rx_ctx.current_slot_id;
    }
}

/**
 * @brief Reset transfer
 */
void ImageTransferV2_Reset(void)
{
    memset(&rx_ctx, 0, sizeof(rx_context_t));
    rx_ctx.state = RX_STATE_IDLE;
    UARTIF_uartPrintf(0, "[IMG_V2] Transfer reset\r\n");
}
