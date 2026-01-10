/******************************************************************************
 * Copyright (C) 2021, 
 *
 *  
 *  
 *  
 *  
 *
 ******************************************************************************/
 
/******************************************************************************
 ** @file image_transfer.c
 **
 ** @brief Source file for image transfer functions
 **
 ** @author MADS Team 
 **
 ******************************************************************************/


/******************************************************************************
 * Include files
 ******************************************************************************/
#include "flash_manager.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "base_types.h"
#include "image_transfer.h"
#include "uart_interface.h"
#include "crc_utils.h"
#include "uart.h"


/******************************************************************************
 * Local pre-processor symbols/macros ('#define')                            
 ******************************************************************************/
 /* command */
/* 带 frameIsFull 的命令格式（14字节）：*/
/* byte 0: header magic number 0xCA */
/* byte 1: command ID */
/* byte 2: image magic BW_IMAGE_DATA 0xA3 RED_IMAGE_DATA 0xA4 */
/* byte 3: slot ID 0x00~0x07 */
/* byte 4-11: frameIsFull (64 bits, little-endian) */
/* byte 12: tail magic number 0xAC*/
/* byte 13: tail new line 0x0A*/

/* 不带 frameIsFull 的命令格式（8字节）：*/
/* byte 0: header magic number 0xCA */
/* byte 1: command ID */
/* byte 2: sub ID or parameter */
/* byte 3: parameter */
/* byte 4: parameter */
/* byte 5: parameter */
/* byte 6: tail magic number 0xAC*/
/* byte 7: tail new line 0x0A*/

/* Command ID 0xB0 0x01 0xff 0xff 0xff 超时重传（8字节）*/
/* Command ID 0xB0 0x02 xx 0xff 0xff busy，byte 3 发送数值*100ms，后上位机可以尝试重发（8字节）*/
/* Command ID 0xB0 0x03 xx 0xff 0xFF 内部错误，停止传输，后面跟一个字节错误码（8字节）*/
/* Command ID 0xB1 上一个page发送成功，可继续发送下一个page，后续跟上一个page发的magic，slot，和当前frameIsFull（14字节）*/
/* Command ID 0xB3 上一个page发送失败，后续发上一个page发的magic，slot，和当前frameIsFull，请求立即重传（14字节）*/
/* Command ID 0xB5 当前帧数据全部接收并存储完成，后续发magic，slot，和当前frameIsFull，可开始下帧传输（14字节）*/
/* Command ID 0xB7 当前接收序列错误，后续发当前正在传输的magic，slot，和当前frameIsFull，请求立即重传（14字节）*/
/* Command ID 0xB7 0xFF 0xFF 0xFF 0xFF 传输错误，在IDLE状态收到非帧头数据，或接收数据magic，slot，frameNum不匹配（8字节）*/

#define IMAGE_PAGE_SIZE PAYLOAD_SIZE
#define IMAGE_TOTAL_SIZE 15000
#define IMAGE_PAGE_COUNT 61

#define IMAGE_FRAME_HEAD_MAGIC  0xA5A5A5A5
#define IMAGE_FRAME_TAIL_MAGIC  0x5A5A5A5A

#define IMAGE_FRAME_HEAD_SIZE   4
#define IMAGE_FRAME_TAIL_SIZE   4
#define IMAGE_META_SIZE         8  // slot_id(1) + type(1) + page_idx(1) + reserved(1) + crc32(2,建议4)
#define IMAGE_FRAME_SIZE        (IMAGE_FRAME_HEAD_SIZE + IMAGE_META_SIZE + IMAGE_PAGE_SIZE + IMAGE_FRAME_TAIL_SIZE)

#define IMAGE_TRANSFER_TIMEOUT_CYCLE  1000  // 可根据实际调整
#define IMAGE_HEADER_MAGIC          0xCA
#define IMAGE_TAIL_MAGIC            0xAC
#define IMAGE_TAIL_NEWLINE          0x0A
#define IMAGE_COMMAND_ID_GENERAL    0xB0
#define IMAGE_COMMAND_ID_ACK        0xB1
#define IMAGE_COMMAND_ID_NACK       0xB3
#define IMAGE_COMMAND_ID_COMPLETE   0xB5
#define IMAGE_COMMAND_ID_SEQ_ERR    0xB7
#define IMAGE_COMMAND_SUBID_TIMEOUT 0x01
#define IMAGE_COMMAND_SUBID_BUSY    0x02
#define IMAGE_COMMAND_SUBID_INTERNAL 0x03

// 调试开关
#define IMAGE_TRANSFER_DEBUG 1

// 错误子码定义（用于调试）
#define ERROR_SUBCODE_IDLE_NOT_HEADER       0x10  // IDLE状态收到非帧头
#define ERROR_SUBCODE_TAIL_MISMATCH         0x11  // 帧尾不匹配
#define ERROR_SUBCODE_CRC_FAIL              0x12  // CRC校验失败
#define ERROR_SUBCODE_INVALID_MAGIC         0x13  // 无效的magic
#define ERROR_SUBCODE_INVALID_SLOT          0x14  // 无效的slot
#define ERROR_SUBCODE_INVALID_FRAME         0x15  // 无效的frame编号
/******************************************************************************
 * Global variable definitions (declared in header file with 'extern')
 ******************************************************************************/

/******************************************************************************
 * Local type definitions ('typedef')                                         
 ******************************************************************************/
typedef enum 
{
    IMGTRSF_IDLE = 0,
    IMGTRSF_RECEIVING_DATA,
    IMGTRSF_SAVING_DATA,
    IMGTRSF_WAITING_NEXT_PAGE
} imageTransferStates;

/******************************************************************************
 * Local function prototypes ('static')
 ******************************************************************************/

/******************************************************************************
 * Local variable definitions ('static')                                      *
 ******************************************************************************/
// static ImageTransferStatus g_transferStatus;

static uint8_t rx_buf[IMAGE_FRAME_SIZE];
static uint16_t rx_idx = 0;
static uint16_t timeout_cycle = 0;
static uint64_t frameIsFull = 0x00;
static imageTransferStates state = IMGTRSF_IDLE;
static uint8_t lastImageMagic = 0xff;
static uint8_t lastSlotId = 0xff;
static uint8_t lastFrameNum = 0xff;
static uint32_t totalBytesReceived = 0;  // 统计接收总字节数
static uint32_t frameCount = 0;          // 统计帧数


/*****************************************************************************
 * Function implementation - local ('static')
 ******************************************************************************/

#if IMAGE_TRANSFER_DEBUG
/**
 * @brief 调试输出：打印十六进制数据
 */
static void debugPrintHex(const char *prefix, const uint8_t *data, uint16_t len)
{
    uint16_t i;
    UARTIF_uartPrintf(0, "[IMG_DBG] %s: ", prefix);
    for (i = 0; i < len && i < 32; i++)  // 最多打印32字节
    {
        UARTIF_uartPrintf(0, "%02X ", data[i]);
    }
    if (len > 32)
    {
        UARTIF_uartPrintf(0, "... (total %d bytes)", len);
    }
    UARTIF_uartPrintf(0, "\r\n");
}

/**
 * @brief 调试输出：打印状态信息
 */
static void debugPrint(const char *msg)
{
    UARTIF_uartPrintf(0, "[IMG_DBG] %s\r\n", msg);
}
#else
#define debugPrintHex(prefix, data, len)
#define debugPrint(msg)
#endif

/**
 * @brief 在缓冲区中查找帧头位置
 * @return 帧头位置索引，如果没找到返回-1
 */
static int16_t findFrameHeader(const uint8_t *buf, uint16_t len)
{
    uint16_t i;
    uint32_t magic;

    if (len < IMAGE_FRAME_HEAD_SIZE)
    {
        return -1;
    }

    for (i = 0; i <= len - IMAGE_FRAME_HEAD_SIZE; i++)
    {
        magic = (buf[i]<<24)|(buf[i+1]<<16)|(buf[i+2]<<8)|buf[i+3];
        if (magic == IMAGE_FRAME_HEAD_MAGIC)
        {
            return (int16_t)i;
        }
    }

    return -1;
}

static void sendCmd(uint8_t *cmd, uint8_t length)
{
    uint8_t i = 0;
    for (i = 0; i < length; i++)
    {
        Uart_SendData(UARTCH1, cmd[i]);
    }
}

/**
 * @brief 发送8字节通用命令（不带frameIsFull）
 * @param cmdId 命令ID
 * @param param1 参数1
 * @param param2 参数2
 * @param param3 参数3
 * @param param4 参数4
 */
static void sendCmd8(uint8_t cmdId, uint8_t param1, uint8_t param2, uint8_t param3, uint8_t param4)
{
    uint8_t cmd[8];
    cmd[0] = IMAGE_HEADER_MAGIC;
    cmd[1] = cmdId;
    cmd[2] = param1;
    cmd[3] = param2;
    cmd[4] = param3;
    cmd[5] = param4;
    cmd[6] = IMAGE_TAIL_MAGIC;
    cmd[7] = IMAGE_TAIL_NEWLINE;
    sendCmd(cmd, 8);
}

/**
 * @brief 发送14字节命令（带完整64位frameIsFull）
 * @param cmdId 命令ID
 * @param magic 图像magic
 * @param slotId 槽位ID
 */
static void sendCmd14(uint8_t cmdId, uint8_t magic, uint8_t slotId)
{
    uint8_t cmd[14];
    cmd[0] = IMAGE_HEADER_MAGIC;
    cmd[1] = cmdId;
    cmd[2] = magic;
    cmd[3] = slotId;
    // 发送完整的 64 位 frameIsFull (little-endian)
    cmd[4] = (uint8_t)(frameIsFull & 0xFF);
    cmd[5] = (uint8_t)((frameIsFull >> 8) & 0xFF);
    cmd[6] = (uint8_t)((frameIsFull >> 16) & 0xFF);
    cmd[7] = (uint8_t)((frameIsFull >> 24) & 0xFF);
    cmd[8] = (uint8_t)((frameIsFull >> 32) & 0xFF);
    cmd[9] = (uint8_t)((frameIsFull >> 40) & 0xFF);
    cmd[10] = (uint8_t)((frameIsFull >> 48) & 0xFF);
    cmd[11] = (uint8_t)((frameIsFull >> 56) & 0xFF);
    cmd[12] = IMAGE_TAIL_MAGIC;
    cmd[13] = IMAGE_TAIL_NEWLINE;
    sendCmd(cmd, 14);
}

static void checkTimeout(void)
{
    if (timeout_cycle > IMAGE_TRANSFER_TIMEOUT_CYCLE)
    {
        sendCmd8(IMAGE_COMMAND_ID_GENERAL, IMAGE_COMMAND_SUBID_TIMEOUT, 0xff, 0xff, 0xff);
        ImageTransfer_Init();
    }
    else
    {
        timeout_cycle++;
    }
}

static void sendCmdAck(uint8_t ack)
{
    sendCmd14(ack, lastImageMagic, lastSlotId);
}

static void sendCmdNack(void)
{
    sendCmd14(IMAGE_COMMAND_ID_NACK, lastImageMagic, lastSlotId);
}

/*****************************************************************************
 * Function implementation - global
 ******************************************************************************/

void ImageTransfer_Init(void)
{
    frameIsFull = 0x00;
    rx_idx = 0;
    timeout_cycle = 0;
    state = IMGTRSF_IDLE;
    lastImageMagic = 0xff;
    lastSlotId = 0xff;
    lastFrameNum = 0xff;
    memset(rx_buf, 0, IMAGE_FRAME_SIZE);

    // 报告当前的传输统计信息
    UARTIF_uartPrintf(0, "[IMG_DBG] Transfer init. Total frames received=%u, Total bytes=%u\r\n",
                     frameCount, totalBytesReceived);

    // 重置统计信息用于下一次传输
    frameCount = 0;
    totalBytesReceived = 0;
}

void ImageTransfer_Process(void)
{
    uint32_t magic = 0;
    uint32_t crcReceived = 0;
    uint32_t crcCalc = 0;
    const uint8_t *data = NULL;
    flash_result_t re = FLASH_OK;
    int16_t headerPos = -1;
    uint16_t i;
    uint16_t newBytes = 0;

    switch (state)
    {
        case IMGTRSF_IDLE:
            // Handle idle state
            newBytes = UARTIF_fetchDataFromUart(rx_buf, &rx_idx);
            if (newBytes > 0)
            {
                totalBytesReceived += newBytes;
                UARTIF_uartPrintf(0, "[IMG_DBG] IDLE: Received %d new bytes, total=%u, rx_idx=%d\r\n",
                                 newBytes, totalBytesReceived, rx_idx);
            }

            if (rx_idx >= IMAGE_FRAME_HEAD_SIZE)
            {
                debugPrintHex("RX_BUF in IDLE", rx_buf, rx_idx < 32 ? rx_idx : 32);

                // 查找帧头位置
                headerPos = findFrameHeader(rx_buf, rx_idx);

                if (headerPos >= 0)
                {
                    debugPrint("Frame header found");
                    frameCount++;
                    UARTIF_uartPrintf(0, "[IMG_DBG] Frame %u: Header pos=%d, rx_idx=%d\r\n", frameCount, headerPos, rx_idx);

                    // 如果帧头不在起始位置，需要移动数据
                    if (headerPos > 0)
                    {
                        debugPrint("Shifting buffer to align header");
                        // 将帧头移到缓冲区开始位置
                        for (i = 0; i < (rx_idx - headerPos); i++)
                        {
                            rx_buf[i] = rx_buf[headerPos + i];
                        }
                        rx_idx = rx_idx - headerPos;
                        debugPrintHex("After shift", rx_buf, rx_idx < 32 ? rx_idx : 32);
                    }

                    // 切换到接收数据状态
                    state = IMGTRSF_RECEIVING_DATA;
                    timeout_cycle = 0;
                }
                else
                {
                    // 没有找到帧头
                    if (rx_idx >= IMAGE_FRAME_SIZE)
                    {
                        // 缓冲区满了还没找到帧头，发送错误并重置
                        debugPrint("Buffer full, no header found");
                        UARTIF_uartPrintf(0, "[IMG_DBG] ERROR: Buffer full without header. First 16 bytes: ");
                        for (i = 0; i < 16 && i < rx_idx; i++)
                        {
                            UARTIF_uartPrintf(0, "%02X ", rx_buf[i]);
                        }
                        UARTIF_uartPrintf(0, "\r\n");
                        sendCmd8(IMAGE_COMMAND_ID_SEQ_ERR, ERROR_SUBCODE_IDLE_NOT_HEADER, rx_buf[0], rx_buf[1], rx_buf[2]);
                        ImageTransfer_Init();
                    }
                    else
                    {
                        // 保留最后几个字节，防止帧头跨buffer
                        if (rx_idx > (IMAGE_FRAME_SIZE - IMAGE_FRAME_HEAD_SIZE))
                        {
                            UARTIF_uartPrintf(0, "[IMG_DBG] Keep last %d bytes for frame alignment\r\n", IMAGE_FRAME_HEAD_SIZE);
                            for (i = 0; i < IMAGE_FRAME_HEAD_SIZE; i++)
                            {
                                rx_buf[i] = rx_buf[rx_idx - IMAGE_FRAME_HEAD_SIZE + i];
                            }
                            rx_idx = IMAGE_FRAME_HEAD_SIZE;
                        }
                        checkTimeout();
                    }
                }
            }
            else if(rx_idx > 0)
            {
                checkTimeout();
            }
            else
            {
                // nothing to do
            }
            break;
        case IMGTRSF_RECEIVING_DATA:
            // Handle receiving data state
            newBytes = UARTIF_fetchDataFromUart(rx_buf, &rx_idx);
            if (newBytes > 0u)
            {
                timeout_cycle = 0; // 有数据则重置超时
                UARTIF_uartPrintf(0, "[IMG_DBG] RECEIVING: Added %d bytes, total rx_idx=%d/%d\r\n",
                                 newBytes, rx_idx, IMAGE_FRAME_SIZE);
            }
            else
            {
                checkTimeout();
            }

            if (rx_idx >= IMAGE_FRAME_SIZE)
            {
                debugPrint("Full frame received");
                UARTIF_uartPrintf(0, "[IMG_DBG] Frame complete: rx_idx=%d bytes\r\n", rx_idx);
                debugPrintHex("Frame header+meta", rx_buf, 16);

                // 检查帧尾
                magic = (rx_buf[IMAGE_FRAME_SIZE-4]<<24)|
                        (rx_buf[IMAGE_FRAME_SIZE-3]<<16)|
                        (rx_buf[IMAGE_FRAME_SIZE-2]<<8)|
                        rx_buf[IMAGE_FRAME_SIZE-1];

                debugPrintHex("Frame tail", &rx_buf[IMAGE_FRAME_SIZE-4], 4);

                if (magic != IMAGE_FRAME_TAIL_MAGIC)
                {
                    // 帧尾错误，发送NACK
                    UARTIF_uartPrintf(0, "[IMG_DBG] Tail error! Got 0x%08lX, expected 0x%08lX\r\n",
                                     magic, IMAGE_FRAME_TAIL_MAGIC);
                    sendCmd8(IMAGE_COMMAND_ID_SEQ_ERR, ERROR_SUBCODE_TAIL_MISMATCH,
                            rx_buf[IMAGE_FRAME_SIZE-4], rx_buf[IMAGE_FRAME_SIZE-3], rx_buf[IMAGE_FRAME_SIZE-2]);
                    rx_idx = 0;
                    timeout_cycle = 0;
                    state = IMGTRSF_IDLE;
                    memset(rx_buf, 0, IMAGE_FRAME_SIZE);
                }
                else
                {
                    // 解析接收到的帧数据
                    // 帧格式: [帧头4] [slot_id1] [magic1] [frame_id1] [reserved1] [crc32_4] [payload248] [帧尾4]
                    //         0-3      4          5         6           7           8-11      12-259      260-263
                    lastSlotId = rx_buf[4];        // 字节4 = slot_id
                    lastImageMagic = rx_buf[5];    // 字节5 = magic/type
                    lastFrameNum = rx_buf[6];      // 字节6 = frame_id/pageIdx
                    // rx_buf[7];                  // 字节7 = reserved
                    crcReceived = (rx_buf[8]<<24)|(rx_buf[9]<<16)|(rx_buf[10]<<8)|rx_buf[11];
                    data = &rx_buf[12];

                    UARTIF_uartPrintf(0, "[IMG_DBG] Parse: magic=0x%02X, slot=%d, frame=%d\r\n",
                                     lastImageMagic, lastSlotId, lastFrameNum);

                    // CRC校验和参数有效性检查
                    crcCalc = calculate_crc32_default(data, IMAGE_PAGE_SIZE);

                    UARTIF_uartPrintf(0, "[IMG_DBG] CRC: rx=0x%08lX, calc=0x%08lX\r\n",
                                     crcReceived, crcCalc);

                    if (crcCalc != crcReceived)
                    {
                        debugPrint("CRC mismatch!");
                        sendCmd8(IMAGE_COMMAND_ID_SEQ_ERR, ERROR_SUBCODE_CRC_FAIL, lastFrameNum, 0xff, 0xff);
                        rx_idx = 0;
                        timeout_cycle = 0;
                        state = IMGTRSF_IDLE;
                        memset(rx_buf, 0, IMAGE_FRAME_SIZE);
                    }
                    else if (lastImageMagic != MAGIC_BW_IMAGE_DATA && lastImageMagic != MAGIC_RED_IMAGE_DATA)
                    {
                        debugPrint("Invalid magic!");
                        sendCmd8(IMAGE_COMMAND_ID_SEQ_ERR, ERROR_SUBCODE_INVALID_MAGIC, lastImageMagic, 0xff, 0xff);
                        rx_idx = 0;
                        timeout_cycle = 0;
                        state = IMGTRSF_IDLE;
                        memset(rx_buf, 0, IMAGE_FRAME_SIZE);
                    }
                    else if (lastSlotId >= MAX_IMAGE_ENTRIES)
                    {
                        debugPrint("Invalid slot!");
                        sendCmd8(IMAGE_COMMAND_ID_SEQ_ERR, ERROR_SUBCODE_INVALID_SLOT, lastSlotId, 0xff, 0xff);
                        rx_idx = 0;
                        timeout_cycle = 0;
                        state = IMGTRSF_IDLE;
                        memset(rx_buf, 0, IMAGE_FRAME_SIZE);
                    }
                    else if (lastFrameNum >= IMAGE_PAGE_COUNT)
                    {
                        debugPrint("Invalid frame num!");
                        sendCmd8(IMAGE_COMMAND_ID_SEQ_ERR, ERROR_SUBCODE_INVALID_FRAME, lastFrameNum, 0xff, 0xff);
                        rx_idx = 0;
                        timeout_cycle = 0;
                        state = IMGTRSF_IDLE;
                        memset(rx_buf, 0, IMAGE_FRAME_SIZE);
                    }
                    else
                    {
                        debugPrint("Frame valid, saving...");
                        state = IMGTRSF_SAVING_DATA;
                    }
                }
            }
            break;
        case IMGTRSF_SAVING_DATA:
            // Handle saving data state
            data = &rx_buf[12];
            debugPrint("Writing to flash...");
            re = FM_writeData(lastImageMagic, (uint16_t)(lastSlotId << 8 | lastFrameNum), data, IMAGE_PAGE_SIZE);
            if (re != FLASH_OK)
            {
                UARTIF_uartPrintf(0, "[IMG_DBG] Flash write error: %d\r\n", re);
                sendCmd8(IMAGE_COMMAND_ID_GENERAL, IMAGE_COMMAND_SUBID_INTERNAL, re, 0xff, 0xff);
                ImageTransfer_Init();
            }
            else
            {
                // 更新接收状态
                frameIsFull |= ((uint64_t)1 << lastFrameNum);

                UARTIF_uartPrintf(0, "[IMG_DBG] Frame %d saved, frameIsFull=0x%08lX%08lX\r\n",
                                 lastFrameNum,
                                 (uint32_t)(frameIsFull >> 32),
                                 (uint32_t)(frameIsFull & 0xFFFFFFFF));

                // 清理缓冲区，准备接收下一帧
                rx_idx = 0;
                timeout_cycle = 0;
                memset(rx_buf, 0, IMAGE_FRAME_SIZE);
                state = IMGTRSF_IDLE;

                if (frameIsFull == 0x1FFFFFFFFFFFFF) // 61页全满 (bits 0-60)
                {
                    debugPrint("All frames received! Writing header...");
                    /* pass color flag inferred from lastImageMagic (data magic) */
                    re = FM_writeImageHeader(lastImageMagic - 2u, lastSlotId,
                                             (lastImageMagic == MAGIC_RED_IMAGE_DATA) ? 1u : 0u);
                    if (re != FLASH_OK)
                    {
                        UARTIF_uartPrintf(0, "[IMG_DBG] Header write error: %d\r\n", re);
                        sendCmd8(IMAGE_COMMAND_ID_GENERAL, IMAGE_COMMAND_SUBID_INTERNAL, re, 0xff, 0xff);
                    }
                    else
                    {
                        // 全部接收完成，发送 COMPLETE
                        debugPrint("Transfer COMPLETE!");
                        sendCmdAck(IMAGE_COMMAND_ID_COMPLETE);
                    }
                    ImageTransfer_Init();
                }
                else
                {
                    debugPrint("Sending ACK");
                    sendCmdAck(IMAGE_COMMAND_ID_ACK);
                }
            }
            break;

        default:
            break;
    }
}

/**
 * @brief 诊断函数：输出当前接收状态
 */
void ImageTransfer_PrintDiagnostics(void)
{
    uint32_t uartRxCnt = 0, queueOverflow = 0;
    uint16_t i;

    UARTIF_getUartStats(&uartRxCnt, &queueOverflow);

    UARTIF_uartPrintf(0, "\r\n=== Image Transfer Diagnostics ===\r\n");
    UARTIF_uartPrintf(0, "State: %d (0=IDLE, 1=RCV_DATA, 2=SAVING, 3=WAIT_NEXT)\r\n", state);
    UARTIF_uartPrintf(0, "RX Buffer Index: %d/%d\r\n", rx_idx, IMAGE_FRAME_SIZE);
    UARTIF_uartPrintf(0, "Frames received: %u\r\n", frameCount);
    UARTIF_uartPrintf(0, "Total bytes received: %u\r\n", totalBytesReceived);
    UARTIF_uartPrintf(0, "UART RX count: %u\r\n", uartRxCnt);
    UARTIF_uartPrintf(0, "Queue overflows: %u\r\n", queueOverflow);
    UARTIF_uartPrintf(0, "frameIsFull: 0x%08lX%08lX\r\n",
                     (uint32_t)(frameIsFull >> 32), (uint32_t)(frameIsFull & 0xFFFFFFFF));
    UARTIF_uartPrintf(0, "Last magic=0x%02X, slot=%d, frame=%d\r\n",
                     lastImageMagic, lastSlotId, lastFrameNum);

    if (rx_idx > 0 && rx_idx < 32)
    {
        UARTIF_uartPrintf(0, "Buffer content: ");
        for (i = 0; i < rx_idx; i++)
        {
            UARTIF_uartPrintf(0, "%02X ", rx_buf[i]);
        }
        UARTIF_uartPrintf(0, "\r\n");
    }
    else if (rx_idx >= 32)
    {
        UARTIF_uartPrintf(0, "Buffer first 32 bytes: ");
        for (i = 0; i < 32; i++)
        {
            UARTIF_uartPrintf(0, "%02X ", rx_buf[i]);
        }
        UARTIF_uartPrintf(0, "\r\n");
    }
    UARTIF_uartPrintf(0, "=== End Diagnostics ===\r\n\r\n");
}

/******************************************************************************
 * EOF (not truncated)
 ******************************************************************************/
