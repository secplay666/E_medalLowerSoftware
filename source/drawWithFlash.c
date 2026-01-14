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
 ** @file draw.c
 **
 ** @brief Source file for draw functions
 **
 ** @author MADS Team 
 **
 ******************************************************************************/

/******************************************************************************
 * Include files
 ******************************************************************************/
#include <stdio.h>
#include <string.h>
#include "epd.h"
#include "base_types.h"
#include <stdint.h>   // for int16_t, uint8_t, etc.
#include <math.h>     // 如果需要fabs之类，但也可不用
#include "image.h"
#include "w25q32.h"
#include "flash_manager.h"
#include "uart_interface.h"

/******************************************************************************
 * Local pre-processor symbols/macros ('#define')                            
 ******************************************************************************/
#define SCREEN_WIDTH 400
#define SCREEN_HEIGHT 300
#define BYTES_PER_ROW (SCREEN_WIDTH / 8)
#define PAGE_SIZE 248
#define PAGE_COUNT 61

/******************************************************************************
 * Global variable definitions (declared in header file with 'extern')
 ******************************************************************************/

/******************************************************************************
 * Local type definitions ('typedef')                                         
 ******************************************************************************/

/******************************************************************************
 * Local function prototypes ('static')
 ******************************************************************************/

/******************************************************************************
 * Local variable definitions ('static')                                      *
 ******************************************************************************/
// 简单的字体数据（5x7 字体，每个字符占 5 列）
static const unsigned char FONT_5X7[62][5] = {
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 0
    {0x00, 0x42, 0x7F, 0x40, 0x00}, // 1
    {0x42, 0x61, 0x51, 0x49, 0x46}, // 2
    {0x22, 0x41, 0x49, 0x49, 0x36}, // 3
    {0x18, 0x14, 0x12, 0x7F, 0x10}, // 4
    {0x27, 0x45, 0x45, 0x45, 0x39}, // 5
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 6
    {0x01, 0x71, 0x09, 0x05, 0x03}, // 7
    {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
    {0x06, 0x49, 0x49, 0x29, 0x1E}, // 9
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, // A
    {0x7F, 0x49, 0x49, 0x49, 0x36}, // B
    {0x3E, 0x41, 0x41, 0x41, 0x22}, // C
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, // D
    {0x7F, 0x49, 0x49, 0x49, 0x41}, // E
    {0x7F, 0x09, 0x09, 0x09, 0x01}, // F
    {0x3E, 0x41, 0x49, 0x49, 0x7A}, // G
    {0x7F, 0x08, 0x08, 0x08, 0x7F}, // H
    {0x00, 0x41, 0x7F, 0x41, 0x00}, // I
    {0x20, 0x40, 0x41, 0x3F, 0x01}, // J
    {0x7F, 0x08, 0x14, 0x22, 0x41}, // K
    {0x7F, 0x40, 0x40, 0x40, 0x40}, // L
    {0x7F, 0x02, 0x04, 0x02, 0x7F}, // M
    {0x7F, 0x04, 0x08, 0x10, 0x7F}, // N
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, // O
    {0x7F, 0x09, 0x09, 0x09, 0x06}, // P
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, // Q
    {0x7F, 0x09, 0x19, 0x29, 0x46}, // R
    {0x26, 0x49, 0x49, 0x49, 0x32}, // S
    {0x01, 0x01, 0x7F, 0x01, 0x01}, // T
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, // U
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, // V
    {0x7F, 0x20, 0x18, 0x20, 0x7F}, // W
    {0x63, 0x14, 0x08, 0x14, 0x63}, // X
    {0x03, 0x04, 0x78, 0x04, 0x03}, // Y
    {0x61, 0x51, 0x49, 0x45, 0x43}, // Z
    {0x20, 0x54, 0x54, 0x54, 0x78}, // a
    {0x7F, 0x48, 0x44, 0x44, 0x38}, // b
    {0x38, 0x44, 0x44, 0x44, 0x20}, // c
    {0x38, 0x44, 0x44, 0x48, 0x7F}, // d
    {0x38, 0x54, 0x54, 0x54, 0x18}, // e
    {0x08, 0x7E, 0x09, 0x01, 0x02}, // f
    {0x18, 0xA4, 0xA4, 0xA4, 0x7C}, // g
    {0x7F, 0x08, 0x04, 0x04, 0x78}, // h
    {0x00, 0x44, 0x7D, 0x40, 0x00}, // i
    {0x40, 0x80, 0x84, 0x7D, 0x00}, // j
    {0x7F, 0x10, 0x28, 0x44, 0x00}, // k
    {0x00, 0x41, 0x7F, 0x40, 0x00}, // l
    {0x7C, 0x04, 0x18, 0x04, 0x78}, // m
    {0x7C, 0x08, 0x04, 0x04, 0x78}, // n
    {0x38, 0x44, 0x44, 0x44, 0x38}, // o
    {0xFC, 0x18, 0x24, 0x24, 0x18}, // p
    {0x18, 0x24, 0x24, 0x18, 0xFC}, // q
    {0x7C, 0x08, 0x04, 0x04, 0x08}, // r
    {0x48, 0x54, 0x54, 0x54, 0x20}, // s
    {0x04, 0x3F, 0x44, 0x40, 0x20}, // t
    {0x3C, 0x40, 0x40, 0x20, 0x7C}, // u
    {0x1C, 0x20, 0x40, 0x20, 0x1C}, // v
    {0x3C, 0x40, 0x30, 0x40, 0x3C}, // w
    {0x44, 0x28, 0x10, 0x28, 0x44}, // x
    {0x9C, 0xA0, 0xA0, 0xA0, 0x7C}, // y
    {0x44, 0x64, 0x54, 0x4C, 0x44}  // z
};
static uint8_t pageBuffer[PAGE_SIZE];

/* 软件 CRC16-CCITT (poly 0x1021, init 0xFFFF) */
static uint16_t crc16_ccitt_sw(const uint8_t *data, uint32_t len)
{
    uint16_t crc = 0xFFFF;
    uint32_t i;
	  uint8_t j;
    for (i = 0; i < len; ++i) {
        crc ^= ((uint16_t)data[i]) << 8;

        for (j = 0; j < 8; ++j) {
            if (crc & 0x8000u) crc = (uint16_t)((crc << 1) ^ 0x1021u);
            else crc = (uint16_t)(crc << 1);
        }
    }
    return crc;
}

/**
 * 测试接口：接受一帧数据（应包含 PAGE_SIZE 字节的数据，随后2字节 CRC 高字节/低字节），
 * 校验通过则将该 page 写入 slot 的第 0 页，其余页写白色，并刷新电子纸显示。
 * 输入要求：buf 长度应 >= PAGE_SIZE + 2。
 */
/**
 * 写入一页数据到指定的 flash page（不刷新显示）
 * 注意：len 应 >= PAGE_SIZE + 2（PAGE_SIZE 字节数据 + 2 字节 CRC）
 */
// void DRAW_testWritePage(imageType_t type, uint8_t slot, uint16_t pageIndex, const uint8_t *buf, uint32_t len)
// {
//     uint16_t id;
//     flash_result_t res;
//     uint32_t i;
//     uint16_t recv_crc;
//     uint16_t calc;

//     if (buf == NULL || len < (PAGE_SIZE + 2)) {
//         UARTIF_uartPrintf(0, "TEST: input too short len=%lu\r\n", (unsigned long)len);
//         return;
//     }

//     /* 复制数据页 */
//     memcpy(pageBuffer, buf, PAGE_SIZE);

//     /* 取接收的 CRC（高字节先） */
//     recv_crc = ((uint16_t)buf[PAGE_SIZE] << 8) | (uint16_t)buf[PAGE_SIZE + 1];

//     /* 计算软件 CRC */
//     calc = crc16_ccitt_sw(pageBuffer, PAGE_SIZE);

//     if (calc != recv_crc) {
//         UARTIF_uartPrintf(0, "TEST: CRC ERR recv=0x%04X calc=0x%04X\r\n", recv_crc, calc);
//         return;
//     }

//     /* 写入指定 pageIndex（只写本页，不触发刷新，也不清空其它页） */
//     id = (uint16_t)(pageIndex | (slot << 8));
//     res = FM_writeData((type == IMAGE_BW) ? MAGIC_BW_IMAGE_DATA : MAGIC_RED_IMAGE_DATA, id, pageBuffer, PAGE_SIZE);
//     if (res != FLASH_OK) {
//         UARTIF_uartPrintf(0, "TEST: write page fail id=0x%04X err=%d\r\n", id, res);
//         return;
//     }

//     UARTIF_uartPrintf(0, "TEST: write page %u ok id=0x%04X\r\n", pageIndex, id);
// }

/**
 * @brief 快速测试：不需要通过UART发送数据包，直接写入RED+BW合成图像到flash并显示
 */
void DRAW_testCompositeQuick(uint8_t slot)
{
    uint16_t i, j, id;
    
    /* 测试：调换顺序，先写BW，后写RED */
    
    /* Write BW layer (0x55 pattern) - 先写 */
    for (j = 0; j < PAGE_SIZE; j++) pageBuffer[j] = 0x55;
    for (i = 0; i <= MAX_FRAME_NUM; i++)
    {
        id = (uint16_t)(i | (slot << 8));
        if (FM_writeData(MAGIC_BW_IMAGE_DATA, id, pageBuffer, PAGE_SIZE) != FLASH_OK) return;
    }
    if (FM_writeImageHeader(MAGIC_BW_IMAGE_HEADER, slot) != FLASH_OK) return;
    
    /* Write RED layer (0xAA pattern) - 后写 */
    for (j = 0; j < PAGE_SIZE; j++) pageBuffer[j] = 0x55;
    for (i = 0; i <= MAX_FRAME_NUM; i++)
    {
        id = (uint16_t)(i | (slot << 8));
        if (FM_writeData(MAGIC_RED_IMAGE_DATA, id, pageBuffer, PAGE_SIZE) != FLASH_OK) return;
    }
    if (FM_writeImageHeader(MAGIC_RED_IMAGE_HEADER, slot) != FLASH_OK) return;
    
    EPD_WhiteScreenGDEY042Z98UsingFlashDate(slot);
}

/******************************************************************************
 * Local pre-processor symbols/macros ('#define')                             
 ******************************************************************************/

/*****************************************************************************
 * Function implementation - global ('extern') and local ('static')
 ******************************************************************************/
// 屏幕数据，每行 19 个字节
// static unsigned char screen[HEIGHT][BYTES_PER_ROW];

// 初始化屏幕为白色
void DRAW_initScreen(imageType_t type, uint8_t slot)
{
    uint16_t i = 0;
    uint16_t id = 0;
    flash_result_t result = FLASH_OK;
    uint8_t dataMagic = 0;
    uint8_t headerMagic = 0;
    uint8_t value = 0;

    if (type == IMAGE_BW) {
        dataMagic = MAGIC_BW_IMAGE_DATA;
        headerMagic = MAGIC_BW_IMAGE_HEADER;
        value = 0xFF; // 白色
    } else if (type == IMAGE_RED) {
        dataMagic = MAGIC_RED_IMAGE_DATA;
        headerMagic = MAGIC_RED_IMAGE_HEADER;
        value = 0x00; // 白色
    } else {
        // 不支持的类型，直接返回
        return;
    }

    for (i = 0;i< MAX_FRAME_NUM + 1;i++)
    {
        memset(pageBuffer, value, PAYLOAD_SIZE);
        id = i | (slot << 8);
        result = FM_writeData(dataMagic,id, pageBuffer, PAYLOAD_SIZE);

        if (result != FLASH_OK)
        {
            UARTIF_uartPrintf(0, "Flash write image data id 0x%04x fail! error code is %d \n", id, result);
            break;
        }
    }

    if (result == FLASH_OK)
    {
        result = FM_writeImageHeader(headerMagic, slot);
    }

    if (result == FLASH_OK)
    {
        UARTIF_uartPrintf(0, "Write image header success! \n");
    }
    else
    {
        UARTIF_uartPrintf(0, "Write image header fail! error code is %d \n", result);
    }
}

// 设置某个像素点 (x, y)，color 为 0 或 1
// void DRAW_pixel(uint16_t x, uint16_t y, boolean_t color) 
// {
//     uint16_t byteIndex = 0;
//     uint16_t bitIndex = 0;
//     if ((x < WIDTH)  && (y < HEIGHT)) 
// 	  {
//         byteIndex = x / 8;
//         bitIndex = 7 - (x % 8);
//         if (!color) {
//             screen[y][byteIndex] &= ~(1 << bitIndex); // 设置为黑色
//         } else {
//             screen[y][byteIndex] |= (1 << bitIndex);  // 设置为白色
//         }
//     }
// }

// 绘制水平直线，直接操作数组
// void DRAW_hLine(uint16_t x, uint16_t y, uint16_t length, boolean_t color) 
// {
//     uint16_t startByte = x / 8;
//     uint16_t endByte = (x + length - 1) / 8;
//     uint16_t startBit = 7 - (x % 8);
//     uint16_t endBit = 7 - ((x + length - 1) % 8);
//     int i;
//     uint8_t mask = 0;
//     uint8_t startMask = 0;
//     uint8_t endMask = 0;
//     if ((y >= HEIGHT) || (x >= WIDTH) || (length <= 0))
// 		{
//         return;
//     }
//     if (x + length > WIDTH) { // 处理超出右边界
//         length = WIDTH - x;
//     }

//     if (startByte == endByte) {
//         // 在同一个字节内
//         mask = ((1 << (startBit + 1)) - 1) & ~((1 << endBit) - 1);
//         if (!color) 
//         {
//             screen[y][startByte] &= ~mask; // 设置为黑色
//         } 
//         else 
//         {
//             screen[y][startByte] |= mask;  // 设置为白色
//         }
//     }
//      else 
//     {
//         // 跨多个字节
//         startMask = (1 << (startBit + 1)) - 1;
//         endMask = ~((1 << endBit) - 1);

//         if (!color) {
//             screen[y][startByte] &= ~startMask;
//             for (i = startByte + 1; i < endByte; i++) {
//                 screen[y][i] = 0x00;
//             }
//             screen[y][endByte] &= ~endMask;
//         } else {
//             screen[y][startByte] |= startMask;
//             for (i = startByte + 1; i < endByte; i++) {
//                 screen[y][i] = 0xFF;
//             }
//             screen[y][endByte] |= endMask;
//         }
//     }
// }

// 绘制垂直直线，直接操作数组
// void DRAW_vLine(uint16_t x, uint16_t y, uint16_t length, boolean_t color) 
// {
//     uint16_t byteIndex = x / 8;
//     uint16_t bitIndex = 7 - (x % 8);
//     unsigned char mask = (1 << bitIndex);
//     int i;
//     if (x >= WIDTH || y >= HEIGHT || length <= 0) 
// 		{
//         return;
//     }
//     if (y + length > HEIGHT) { // 处理超出下边界
//         length = HEIGHT - y;
//     }

//     if (!color) {
//         for (i = 0; i < length; i++) {
//             screen[y + i][byteIndex] &= ~mask; // 设置为黑色
//         }
//     } else {
//         for (i = 0; i < length; i++) {
//             screen[y + i][byteIndex] |= mask;  // 设置为白色
//         }
//     }
// }

// 绘制矩形，filled 为 1 表示实心，0 表示空心
// void DRAW_rectangle(uint16_t x, uint16_t y, uint16_t width, uint16_t height, boolean_t color, boolean_t filled) 
// {
//     uint16_t i;
//     if (filled) 
//     {
//         for (i = 0; i < height; i++) 
//         {
//             DRAW_hLine(x, y + i, width, color);
//         }
//     } 
//     else 
//     {
//         DRAW_hLine(x, y, width, color);                 // 上边
//         DRAW_hLine(x, y + height - 1, width, color);    // 下边
//         DRAW_vLine(x, y, height, color);               // 左边
//         DRAW_vLine(x + width - 1, y, height, color);   // 右边
//     }
// }

// void DRAW_outputScreen(void)
// {
//     // EPD_display(screen);
//     EPD_WhiteScreenGDEY042Z98ALL(screen);
// }



// extern void Flash_ReadPage(uint16_t pageIdx, uint8_t *buffer);   // 读一页
// extern void Flash_WritePage(uint16_t pageIdx, const uint8_t *buffer); // 写一页
// extern uint8_t Flash_PageBuffer[PAGE_SIZE]; // 单页缓冲区

// 计算像素 (x, y) 在 flash 中的 page 和 offset
// static void get_flash_addr(uint16_t x, uint16_t y, uint16_t *pageIdx, uint16_t *offset) 
// {
//     uint32_t pixelIdx = y * BYTES_PER_ROW + (x / 8);
//     *pageIdx = pixelIdx / PAGE_SIZE;
//     *offset = pixelIdx % PAGE_SIZE;
// }

// 批量设置一个字符的所有像素到 flash
// void DRAW_char(uint16_t x, uint16_t y, char c, int fontSize, int color) {
//     int row, col, dx, dy;
//     const unsigned char *glyph;
//     // 字符映射
//     if (c >= '0' && c <= '9') glyph = FONT_5X7[c - '0'];
//     else if (c >= 'A' && c <= 'Z') glyph = FONT_5X7[c - 'A' + 10];
//     else if (c >= 'a' && c <= 'z') glyph = FONT_5X7[c - 'a' + 36];
//     else return;

//     // 统计所有像素点，按 page 分组
//     // 由于字符很小，最多跨2-3个page，直接用一个 page buffer处理
//     // 记录本次操作涉及的所有 page
//     uint8_t pageBuffers[3][PAGE_SIZE];
//     uint8_t pageUsed[3] = {0};
//     uint16_t pageIdxs[3] = {0};

//     // 预处理所有像素点，分组到 page
//     for (row = 0; row < 7 * fontSize; row++) {
//         for (col = 0; col < 5 * fontSize; col++) {
//             // 判断该点是否为字体点
//             int fontRow = row / fontSize;
//             int fontCol = col / fontSize;
//             if (glyph[fontCol] & (1 << fontRow)) {
//                 uint16_t px = x + col;
//                 uint16_t py = y + row;
//                 if (px >= SCREEN_WIDTH || py >= SCREEN_HEIGHT) continue;
//                 uint16_t pageIdx, offset;
//                 get_flash_addr(px, py, &pageIdx, &offset);
//                 // 查找 pageIdx 是否已在 pageIdxs
//                 int p;
//                 for (p = 0; p < 3; p++) {
//                     if (pageUsed[p] && pageIdxs[p] == pageIdx) break;
//                 }
//                 if (p == 3) continue; // 超过3页不处理
//                 if (!pageUsed[p]) {
//                     Flash_ReadPage(pageIdx, pageBuffers[p]);
//                     pageIdxs[p] = pageIdx;
//                     pageUsed[p] = 1;
//                 }
//                 uint8_t bitIdx = 7 - (px % 8);
//                 if (color)
//                     pageBuffers[p][offset] |= (1 << bitIdx);
//                 else
//                     pageBuffers[p][offset] &= ~(1 << bitIdx);
//             }
//         }
//     }
//     // 写回所有用到的 page
//     for (int p = 0; p < 3; p++) {
//         if (pageUsed[p]) {
//             Flash_WritePage(pageIdxs[p], pageBuffers[p]);
//         }
//     }
// }

void DRAW_string(imageType_t type, uint8_t slot, uint16_t x, uint16_t y, const char *str, uint8_t fontSize, boolean_t color)
{
    int strLen;
    const char *s;
    int charWidth;
    int charHeight;
    int totalWidth;
    uint32_t startPixel, endPixel, pixelIdx;
    uint16_t startPage, endPage, pageIdx, offset, py, px_byte, px;
    uint8_t bit;
    int rel_x, charIdx, char_x, char_y, fontRow, fontCol;
    char c;
    const unsigned char *glyph;
    uint8_t dataMagic = 0;
    // uint8_t headerMagic = 0;
    uint16_t id = 0;

    if (type == IMAGE_BW) {
        dataMagic = MAGIC_BW_IMAGE_DATA;
    } else if (type == IMAGE_RED) {
        dataMagic = MAGIC_RED_IMAGE_DATA;
    } else {
        // 不支持的类型，直接返回
        return;
    }

    // 参数安全性校验
    if (str == NULL || fontSize <= 0 || x >= SCREEN_WIDTH || y >= SCREEN_HEIGHT) return;

    // 计算字符串长度
    strLen = 0;
    s = str;
    while (*s) { strLen++; s++; }
    if (strLen == 0) return;

    charWidth = 5 * fontSize;
    charHeight = 7 * fontSize;
    totalWidth = strLen * (charWidth + 1) - 1;
    if (x + totalWidth > SCREEN_WIDTH) totalWidth = SCREEN_WIDTH - x;
    if (y + charHeight > SCREEN_HEIGHT) charHeight = SCREEN_HEIGHT - y;

    // 计算字符串区域涉及的 page 范围
    startPixel = y * BYTES_PER_ROW + (x / 8);
    endPixel = (y + charHeight - 1) * BYTES_PER_ROW + ((x + totalWidth - 1) / 8);
    startPage = startPixel / PAGE_SIZE;
    endPage = endPixel / PAGE_SIZE;

    for (pageIdx = startPage; pageIdx <= endPage; pageIdx++) {
        // Flash_ReadPage(pageIdx, pageBuffer);
        (void)FM_readImage(dataMagic, slot, pageIdx, pageBuffer);


        // 遍历该 page 的所有像素点，判断是否属于字符串像素
        for (offset = 0; offset < PAGE_SIZE; offset++) {
            pixelIdx = pageIdx * PAGE_SIZE + offset;
            py = pixelIdx / BYTES_PER_ROW;
            px_byte = pixelIdx % BYTES_PER_ROW;
            px = px_byte * 8;
            // 该字节的8个像素都在 (px, py)~(px+7, py)
            if (py < y || py >= y + charHeight) continue;
            for (bit = 0; bit < 8; bit++) {
                uint16_t pixel_x = px + (7 - bit); // 高位在左
                if (pixel_x < x || pixel_x >= x + totalWidth) continue;
                // 计算该像素属于哪个字符
                rel_x = pixel_x - x;
                charIdx = rel_x / (charWidth + 1);
                char_x = rel_x % (charWidth + 1);
                char_y = py - y;
                if (charIdx < 0 || charIdx >= strLen || char_x >= charWidth) continue;
                c = str[charIdx];
                glyph = NULL;
                if (c >= '0' && c <= '9') glyph = FONT_5X7[c - '0'];
                else if (c >= 'A' && c <= 'Z') glyph = FONT_5X7[c - 'A' + 10];
                else if (c >= 'a' && c <= 'z') glyph = FONT_5X7[c - 'a' + 36];
                if (glyph == NULL) continue;
                fontRow = char_y / fontSize;
                fontCol = char_x / fontSize;
                if (fontRow < 0 || fontRow >= 7 || fontCol < 0 || fontCol >= 5) continue;
                if (glyph[fontCol] & (1 << fontRow)) {
                    if (color)
                        pageBuffer[offset] |= (1 << bit);
                    else
                        pageBuffer[offset] &= ~(1 << bit);
                }
            }
        }
        // Flash_WritePage(pageIdx, pageBuffer);
        id = pageIdx | (slot << 8);
        (void)FM_writeData(dataMagic,id, pageBuffer, PAYLOAD_SIZE);

    }
}

// void DRAW_rotatedChar(int x, int y, char c, int fontSize, int color)
// {
//     int row, col, dx, dy;
//     int originalRow, originalCol;
//     int byteIndex, bitIndex, screenRow, screenCol;
//     const unsigned char *glyph;

//     // 判断字符范围并选择字体数据
//     if (c >= '0' && c <= '9')
//     {
//         c -= '0';
//     }
//     else if (c >= 'A' && c <= 'Z')
//     {
//         c -= 'A' - 10;
//     }
//     else if (c >= 'a' && c <= 'z')
//     {
//         c -= 'a' - 36;
//     }
//     else
//     {
//         return; // 不支持的字符
//     }

//     glyph = FONT_5X7[(int)c];

//     if (fontSize == 1)
//     {
//         for (row = 0; row < 7; row++) // 遍历字体的每一行
//         {
//             for (col = 0; col < 5; col++) // 遍历字体的每一列
//             {
//                 if (glyph[col] & (1 << row)) // 每列的第 row 位
//                 {
//                     // 逆时针旋转坐标
//                     originalRow = y + row;
//                     originalCol = x + col;
//                     screenRow = WIDTH - 1 - originalCol; // 新行
//                     screenCol = originalRow;            // 新列

//                     if (screenRow < 0 || screenRow >= HEIGHT || screenCol < 0 || screenCol >= WIDTH)
//                     {
//                         continue; // 超出屏幕范围，跳过
//                     }

//                     byteIndex = screenCol / 8;          // 字节索引
//                     bitIndex = 7 - (screenCol % 8);     // 位索引

//                     if (color == 1)
//                     {
//                         screen[screenRow][byteIndex] |= (1 << bitIndex); // 设为白色
//                     }
//                     else
//                     {
//                         screen[screenRow][byteIndex] &= ~(1 << bitIndex); // 设为黑色
//                     }
//                 }
//             }
//         }
//     }
//     else // 字体放大处理
//     {
//         for (row = 0; row < 7; row++)
//         {
//             for (col = 0; col < 5; col++)
//             {
//                 if (glyph[col] & (1 << row)) // 每列的第 row 位
//                 {
//                     for (dy = 0; dy < fontSize; dy++)
//                     {
//                         for (dx = 0; dx < fontSize; dx++)
//                         {
//                             // 逆时针旋转坐标
//                             originalRow = y + row * fontSize + dy;
//                             originalCol = x + col * fontSize + dx;
//                             screenRow = WIDTH - 1 - originalCol; // 新行
//                             screenCol = originalRow;            // 新列

//                             if (screenRow < 0 || screenRow >= HEIGHT || screenCol < 0 || screenCol >= WIDTH)
//                             {
//                                 continue; // 超出屏幕范围，跳过
//                             }

//                             byteIndex = screenCol / 8;          // 字节索引
//                             bitIndex = 7 - (screenCol % 8);     // 位索引

//                             if (color == 1)
//                             {
//                                 screen[screenRow][byteIndex] |= (1 << bitIndex); // 设为白色
//                             }
//                             else
//                             {
//                                 screen[screenRow][byteIndex] &= ~(1 << bitIndex); // 设为黑色
//                             }
//                         }
//                     }
//                 }
//             }
//         }
//     }
// }

// void DRAW_rotatedString(int x, int y, const char *str, int fontSize, int color)
// {
//     int offset = 0; // 字符间的水平偏移

//     while (*str)
//     {
//         DRAW_rotatedChar(x + offset, y, *str, fontSize, color);
//         offset += 6 * fontSize; // 每个字符宽度（5 像素 + 1 像素间距）
//         str++;
//     }
// }

// void DRAW_Image(int x, int y, const unsigned char *imageData, int width, int height, int scale, int invertColor, int rotate90)
// {
//     // 定义所有的局部变量
//     int screenRow, screenCol;
//     int byteIndex, bitIndex;
//     int imgIndex;
//     unsigned char lineByte;
//     int fy, fx, bit,row,col;
//     int originalRow, originalCol;

//     // 如果需要旋转图像
//     if (rotate90 == 1) {
//         for (row = 0; row < height; row++) {
//             imgIndex = row * (width / 8);

//             // 遍历每个字节（每字节包含 8 个像素）
//             for (col = 0; col < (width / 8); col++) {
//                 lineByte = imageData[imgIndex + col];  // 获取图像中的字节数据

//                 // 按位逐个检查字节中的每一位（每8位代表1个像素）
//                 for (bit = 0; bit < 8; bit++) {
//                     if ((lineByte >> (7 - bit)) & 0x01) {  // 如果该位为 1，则为白色像素
//                         // 根据反色参数决定颜色
//                         if (invertColor == 1) {
//                             for (fy = 0; fy < scale; fy++) {
//                                 for (fx = 0; fx < scale; fx++) {
//                                     originalRow = screenRow = y + row * scale + fy;  // 沿纵向放大
//                                     if (screenRow < 0 || screenRow >= HEIGHT) {
//                                         continue;
//                                     }

//                                     for (fx = 0; fx < scale; fx++) {
//                                         originalCol = screenCol = x + col * 8 * scale + bit * scale + fx;  // 沿横向放大
//                                         screenRow = WIDTH - 1 - originalCol; // 新行
//                                         screenCol = originalRow;            // 新列

//                                         // 检查横向是否超出屏幕范围
//                                         if (screenCol < 0 || screenCol >= WIDTH || screenRow < 0 || screenRow >= HEIGHT) {
//                                             continue;
//                                         }

//                                         byteIndex = screenCol / 8;          // 字节索引
//                                         bitIndex = 7 - (screenCol % 8);     // 位索引（高位在左）

//                                         // 设置屏幕像素为黑色
//                                         screen[screenRow][byteIndex] &= ~(1 << bitIndex);
//                                     }
//                                 }
//                             }
//                         } else {
//                             for (fy = 0; fy < scale; fy++) {
//                                 for (fx = 0; fx < scale; fx++) {
//                                     originalRow = screenRow = y + row * scale + fy;  // 沿纵向放大

//                                     // 检查纵向是否超出屏幕范围
//                                     if (screenRow < 0 || screenRow >= HEIGHT) {
//                                         continue;
//                                     }

//                                     for (fx = 0; fx < scale; fx++) {
//                                         originalCol = screenCol = x + col * 8 * scale + bit * scale + fx;  // 沿横向放大

//                                         // 检查横向是否超出屏幕范围
//                                         if (screenCol < 0 || screenCol >= WIDTH || screenRow < 0 || screenRow >= HEIGHT) {
//                                             continue;
//                                         }

//                                         byteIndex = screenCol / 8;          // 字节索引
//                                         bitIndex = 7 - (screenCol % 8);     // 位索引（高位在左）

//                                         // 设置屏幕像素为白色
//                                         screen[screenRow][byteIndex] |= (1 << bitIndex);
//                                     }
//                                 }
//                             }
//                         }
//                     }
//                 }
//             }
//         }
//     } 
//     else 
//     {
//         // 如果不需要旋转，则按原始方向绘制
//         for (row = 0; row < height; row++) {
//             imgIndex = row * (width / 8);

//             // 遍历每个字节（每字节包含 8 个像素）
//             for (col = 0; col < (width / 8); col++) {
//                 lineByte = imageData[imgIndex + col];  // 获取图像中的字节数据

//                 // 按位逐个检查字节中的每一位（每8位代表1个像素）
//                 for (bit = 0; bit < 8; bit++) {
//                     if ((lineByte >> (7 - bit)) & 0x01) {  // 如果该位为 1，则为白色像素
//                         // 根据反色参数决定颜色
//                         if (invertColor == 1) {
//                             for (fy = 0; fy < scale; fy++) {
//                                 for (fx = 0; fx < scale; fx++) {
//                                     screenRow = y + row * scale + fy;  // 沿纵向放大

//                                     // 检查纵向是否超出屏幕范围
//                                     if (screenRow < 0 || screenRow >= HEIGHT) {
//                                         continue;
//                                     }

//                                     for (fx = 0; fx < scale; fx++) {
//                                         screenCol = x + col * 8 * scale + bit * scale + fx;  // 沿横向放大

//                                         // 检查横向是否超出屏幕范围
//                                         if (screenCol < 0 || screenCol >= WIDTH) {
//                                             continue;
//                                         }

//                                         byteIndex = screenCol / 8;          // 字节索引
//                                         bitIndex = 7 - (screenCol % 8);     // 位索引（高位在左）

//                                         // 设置屏幕像素为黑色
//                                         screen[screenRow][byteIndex] &= ~(1 << bitIndex);
//                                     }
//                                 }
//                             }
//                         } else {
//                             for (fy = 0; fy < scale; fy++) {
//                                 for (fx = 0; fx < scale; fx++) {
//                                     screenRow = y + row * scale + fy;  // 沿纵向放大

//                                     // 检查纵向是否超出屏幕范围
//                                     if (screenRow < 0 || screenRow >= HEIGHT) {
//                                         continue;
//                                     }

//                                     for (fx = 0; fx < scale; fx++) {
//                                         screenCol = x + col * 8 * scale + bit * scale + fx;  // 沿横向放大

//                                         // 检查横向是否超出屏幕范围
//                                         if (screenCol < 0 || screenCol >= WIDTH) {
//                                             continue;
//                                         }

//                                         byteIndex = screenCol / 8;          // 字节索引
//                                         bitIndex = 7 - (screenCol % 8);     // 位索引（高位在左）

//                                         // 设置屏幕像素为白色
//                                         screen[screenRow][byteIndex] |= (1 << bitIndex);
//                                     }
//                                 }
//                             }
//                         }
//                     }
//                 }
//             }
//         }
//     }
// }

// void DRAW_DisplayTest(void)
// {
//     uint8_t baseX = 20;
//     uint8_t baseY = 20;
//     uint8_t dx = 30;
//     uint8_t dy = 20;
//     const unsigned char* fontArray[10] = 
//     {
//         FONT_18PX_0,
//         FONT_18PX_1,
//         FONT_18PX_2,
//         FONT_18PX_3,
//         FONT_18PX_4,
//         FONT_18PX_5,
//         FONT_18PX_6,
//         FONT_18PX_7,
//         FONT_18PX_8,
//         FONT_18PX_9 
//     };

//     DRAW_Image(baseX, baseY, fontArray[1], 16, 18,2,0,1);
//     baseX += dx;
//     DRAW_Image(baseX, baseY, fontArray[2], 16, 18,2,0,1);
//     baseX += dx;
//     baseY += dy;

//     DRAW_Image(baseX, baseY, fontArray[5], 16, 18,2,0,1);
//     baseX += dx;
//     DRAW_Image(baseX, baseY, fontArray[6], 16, 18,2,0,1);
//     baseX += dx;
// }


// void DRAW_DisplayTempHumiRot(float temperature, float humidity, boolean_t linkFlag)
// {

//     uint8_t baseX = 00;
//     uint8_t baseY = 29;
//     uint8_t dx = 24;
//     uint8_t dy = 76;
//     uint8_t d0,d1,i0,i1;
//     uint8_t integerPart;
//     float decimalPart;
//     boolean_t ts = TRUE;
//     const unsigned char* fontArray[10] = 
//     {
//         FONT_18PX_0,
//         FONT_18PX_1,
//         FONT_18PX_2,
//         FONT_18PX_3,
//         FONT_18PX_4,
//         FONT_18PX_5,
//         FONT_18PX_6,
//         FONT_18PX_7,
//         FONT_18PX_8,
//         FONT_18PX_9 
//     };

//     if(temperature > 99.99)
//     {
//         temperature = 99.99;
//     }
//     else if(temperature < -99.99)
//     {
//         temperature = -99.99;
//     }
//     else
//     {

//     }

//     if (temperature < 0)
//     {
//         ts = FALSE;
//         temperature *= (-1.0);
//     }

//     if(humidity > 99.99)
//     {
//         humidity = 99.99;
//     }
//     else if(humidity < 0)
//     {
//         humidity = 0;
//     }
//     else
//     {
        
//     }

//     integerPart = (uint8_t)temperature;
//     decimalPart = temperature * 100 - integerPart * 100;

//     i0 = integerPart % 10;
//     i1 = integerPart / 10;

//     d0 = (uint8_t)decimalPart % 10;
//     d1 = (uint8_t)decimalPart / 10;

//     if (!ts)
//     {
//         DRAW_Image(baseX, baseY, FONT_24PX_MINUS, 8, 18,2,1,1);
//         baseX += 10;
//     }
//     else
//     {
//         baseX += 5;
//     }
    
//     DRAW_Image(baseX, baseY, fontArray[i1], 16, 18,2,1,1);
//     baseX += dx;
//     DRAW_Image(baseX, baseY, fontArray[i0], 16, 18,2,1,1);
//     baseX += dx;
//     baseX += 1;
//     DRAW_Image(baseX, baseY, FONT_24PX_POINT, 8, 18,2,1,1);
//     baseX += 8;

//     DRAW_Image(baseX, baseY, fontArray[d1], 16, 18,2,1,1);
//     baseX += dx;
//     DRAW_Image(baseX, baseY, fontArray[d0], 16, 18,2,1,1);
//     baseX += dx;
//     DRAW_Image(baseX, baseY, FONT_24PX_DEGREE, 24, 18,2,1,1);

//     baseX = 5;
//     baseY += dy;

//     integerPart = (uint8_t)humidity;
//     decimalPart = humidity * 100 - integerPart * 100;

//     i0 = integerPart % 10;
//     i1 = integerPart / 10;

//     d0 = (uint8_t)decimalPart % 10;
//     d1 = (uint8_t)decimalPart / 10;

//     DRAW_Image(baseX, baseY, fontArray[i1], 16, 18,2,1,1);
//     baseX += dx;
//     DRAW_Image(baseX, baseY, fontArray[i0], 16, 18,2,1,1);
//     baseX += dx;

//     baseX += 1;
//     DRAW_Image(baseX, baseY, FONT_24PX_POINT, 8, 18,2,1,1);
//     baseX += 8;

//     DRAW_Image(baseX, baseY, fontArray[d1], 16, 18,2,1,1);
//     baseX += dx;
//     DRAW_Image(baseX, baseY, fontArray[d0], 16, 18,2,1,1);
//     baseX += dx;
//     DRAW_Image(baseX, baseY, FONT_24PX_PER, 24, 18,2,1,1);

//     DRAW_Image(2, 2, FONT_24PX_WEN, 24, 23,1,1,1);
//     DRAW_Image(32,2 , FONT_24PX_DU, 24, 23,1,1,1);

//     DRAW_Image(2, 78, FONT_24PX_SHI, 24, 23,1,1,1);
//     DRAW_Image(32, 78, FONT_24PX_DU, 24, 23,1,1,1);

//     if (linkFlag)
//     {
//         DRAW_Image(120, 2, FONT_24PX_BTLOGO, 24, 22,1,1,1);
//     }

// }

/******************************************************************************
 * EOF (not truncated)
 ******************************************************************************/


