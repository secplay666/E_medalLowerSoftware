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

/******************************************************************************
 * Local pre-processor symbols/macros ('#define')                            
 ******************************************************************************/
// #define WIDTH 152
// #define HEIGHT 152
// #define BYTES_PER_ROW (WIDTH / 8)
#define WIDTH WIDTH_420
#define HEIGHT HEIGHT_420
//#define BYTES_PER_ROW BYTES_PER_ROW_420

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
// static const unsigned char FONT_5X7[62][5] = {
//     {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 0
//     {0x00, 0x42, 0x7F, 0x40, 0x00}, // 1
//     {0x42, 0x61, 0x51, 0x49, 0x46}, // 2
//     {0x22, 0x41, 0x49, 0x49, 0x36}, // 3
//     {0x18, 0x14, 0x12, 0x7F, 0x10}, // 4
//     {0x27, 0x45, 0x45, 0x45, 0x39}, // 5
//     {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 6
//     {0x01, 0x71, 0x09, 0x05, 0x03}, // 7
//     {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
//     {0x06, 0x49, 0x49, 0x29, 0x1E}, // 9
//     {0x7E, 0x11, 0x11, 0x11, 0x7E}, // A
//     {0x7F, 0x49, 0x49, 0x49, 0x36}, // B
//     {0x3E, 0x41, 0x41, 0x41, 0x22}, // C
//     {0x7F, 0x41, 0x41, 0x22, 0x1C}, // D
//     {0x7F, 0x49, 0x49, 0x49, 0x41}, // E
//     {0x7F, 0x09, 0x09, 0x09, 0x01}, // F
//     {0x3E, 0x41, 0x49, 0x49, 0x7A}, // G
//     {0x7F, 0x08, 0x08, 0x08, 0x7F}, // H
//     {0x00, 0x41, 0x7F, 0x41, 0x00}, // I
//     {0x20, 0x40, 0x41, 0x3F, 0x01}, // J
//     {0x7F, 0x08, 0x14, 0x22, 0x41}, // K
//     {0x7F, 0x40, 0x40, 0x40, 0x40}, // L
//     {0x7F, 0x02, 0x04, 0x02, 0x7F}, // M
//     {0x7F, 0x04, 0x08, 0x10, 0x7F}, // N
//     {0x3E, 0x41, 0x41, 0x41, 0x3E}, // O
//     {0x7F, 0x09, 0x09, 0x09, 0x06}, // P
//     {0x3E, 0x41, 0x51, 0x21, 0x5E}, // Q
//     {0x7F, 0x09, 0x19, 0x29, 0x46}, // R
//     {0x26, 0x49, 0x49, 0x49, 0x32}, // S
//     {0x01, 0x01, 0x7F, 0x01, 0x01}, // T
//     {0x3F, 0x40, 0x40, 0x40, 0x3F}, // U
//     {0x1F, 0x20, 0x40, 0x20, 0x1F}, // V
//     {0x7F, 0x20, 0x18, 0x20, 0x7F}, // W
//     {0x63, 0x14, 0x08, 0x14, 0x63}, // X
//     {0x03, 0x04, 0x78, 0x04, 0x03}, // Y
//     {0x61, 0x51, 0x49, 0x45, 0x43}, // Z
//     {0x20, 0x54, 0x54, 0x54, 0x78}, // a
//     {0x7F, 0x48, 0x44, 0x44, 0x38}, // b
//     {0x38, 0x44, 0x44, 0x44, 0x20}, // c
//     {0x38, 0x44, 0x44, 0x48, 0x7F}, // d
//     {0x38, 0x54, 0x54, 0x54, 0x18}, // e
//     {0x08, 0x7E, 0x09, 0x01, 0x02}, // f
//     {0x18, 0xA4, 0xA4, 0xA4, 0x7C}, // g
//     {0x7F, 0x08, 0x04, 0x04, 0x78}, // h
//     {0x00, 0x44, 0x7D, 0x40, 0x00}, // i
//     {0x40, 0x80, 0x84, 0x7D, 0x00}, // j
//     {0x7F, 0x10, 0x28, 0x44, 0x00}, // k
//     {0x00, 0x41, 0x7F, 0x40, 0x00}, // l
//     {0x7C, 0x04, 0x18, 0x04, 0x78}, // m
//     {0x7C, 0x08, 0x04, 0x04, 0x78}, // n
//     {0x38, 0x44, 0x44, 0x44, 0x38}, // o
//     {0xFC, 0x18, 0x24, 0x24, 0x18}, // p
//     {0x18, 0x24, 0x24, 0x18, 0xFC}, // q
//     {0x7C, 0x08, 0x04, 0x04, 0x08}, // r
//     {0x48, 0x54, 0x54, 0x54, 0x20}, // s
//     {0x04, 0x3F, 0x44, 0x40, 0x20}, // t
//     {0x3C, 0x40, 0x40, 0x20, 0x7C}, // u
//     {0x1C, 0x20, 0x40, 0x20, 0x1C}, // v
//     {0x3C, 0x40, 0x30, 0x40, 0x3C}, // w
//     {0x44, 0x28, 0x10, 0x28, 0x44}, // x
//     {0x9C, 0xA0, 0xA0, 0xA0, 0x7C}, // y
//     {0x44, 0x64, 0x54, 0x4C, 0x44}  // z
// };

/******************************************************************************
 * Local pre-processor symbols/macros ('#define')                             
 ******************************************************************************/

/*****************************************************************************
 * Function implementation - global ('extern') and local ('static')
 ******************************************************************************/
// 屏幕数据，每行 19 个字节
static unsigned char screen[HEIGHT][BYTES_PER_ROW];

// 初始化屏幕为白色
void DRAW_initScreen() 
{
    memset(screen, 0x00, sizeof(screen)); // 设置所有位为 1，即全白
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

void DRAW_outputScreen(void)
{
    // EPD_display(screen);
    EPD_WhiteScreenGDEY042Z98ALL(screen);
}

// void DRAW_char(int x, int y, char c, int fontSize, int color)
// {
//     int row, col, dx, dy;
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
//             screenRow = y + row; // 屏幕上的行
//             if (screenRow < 0 || screenRow >= HEIGHT)
//             {
//                 continue; // 超出屏幕范围，跳过
//             }

//             for (col = 0; col < 5; col++) // 遍历字体的每一列
//             {
//                 screenCol = x + col; // 屏幕上的列
//                 if (screenCol < 0 || screenCol >= WIDTH)
//                 {
//                     continue; // 超出屏幕范围，跳过
//                 }

//                 byteIndex = screenCol / 8; // 字节索引
//                 bitIndex = 7 - (screenCol % 8); // 位索引（高位在左）

//                 // 按行扫描字体数据，决定是否设置像素
//                 if (glyph[col] & (1 << row)) // 每列的第 row 位
//                 {
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
//                         screenRow = y + row * fontSize + dy;
//                         if (screenRow < 0 || screenRow >= HEIGHT)
//                         {
//                             continue; // 超出屏幕范围，跳过
//                         }

//                         for (dx = 0; dx < fontSize; dx++)
//                         {
//                             screenCol = x + col * fontSize + dx;
//                             if (screenCol < 0 || screenCol >= WIDTH)
//                             {
//                                 continue; // 超出屏幕范围，跳过
//                             }

//                             byteIndex = screenCol / 8; // 字节索引
//                             bitIndex = 7 - (screenCol % 8); // 位索引（高位在左）

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

// void DRAW_string(int x, int y, const char *str, int fontSize, int color)
// {
//     int cursorX;
//     cursorX = x;

//     while (*str)
//     {
//         DRAW_char(cursorX, y, *str, fontSize, color);
//         cursorX += (5 * fontSize + 1); // 每个字符宽度为 5，加一个像素间隔
//         str++;
//     }
// }

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

void DRAW_Image(int x, int y, const unsigned char *imageData, int width, int height, int scale, int invertColor, int rotate90)
{
    // 定义所有的局部变量
    int screenRow, screenCol;
    int byteIndex, bitIndex;
    int imgIndex;
    unsigned char lineByte;
    int fy, fx, bit,row,col;
    int originalRow, originalCol;

    // 如果需要旋转图像
    if (rotate90 == 1) {
        for (row = 0; row < height; row++) {
            imgIndex = row * (width / 8);

            // 遍历每个字节（每字节包含 8 个像素）
            for (col = 0; col < (width / 8); col++) {
                lineByte = imageData[imgIndex + col];  // 获取图像中的字节数据

                // 按位逐个检查字节中的每一位（每8位代表1个像素）
                for (bit = 0; bit < 8; bit++) {
                    if ((lineByte >> (7 - bit)) & 0x01) {  // 如果该位为 1，则为白色像素
                        // 根据反色参数决定颜色
                        if (invertColor == 1) {
                            for (fy = 0; fy < scale; fy++) {
                                for (fx = 0; fx < scale; fx++) {
                                    originalRow = screenRow = y + row * scale + fy;  // 沿纵向放大
                                    if (screenRow < 0 || screenRow >= HEIGHT) {
                                        continue;
                                    }

                                    for (fx = 0; fx < scale; fx++) {
                                        originalCol = screenCol = x + col * 8 * scale + bit * scale + fx;  // 沿横向放大
                                        screenRow = WIDTH - 1 - originalCol; // 新行
                                        screenCol = originalRow;            // 新列

                                        // 检查横向是否超出屏幕范围
                                        if (screenCol < 0 || screenCol >= WIDTH || screenRow < 0 || screenRow >= HEIGHT) {
                                            continue;
                                        }

                                        byteIndex = screenCol / 8;          // 字节索引
                                        bitIndex = 7 - (screenCol % 8);     // 位索引（高位在左）

                                        // 设置屏幕像素为黑色
                                        screen[screenRow][byteIndex] &= ~(1 << bitIndex);
                                    }
                                }
                            }
                        } else {
                            for (fy = 0; fy < scale; fy++) {
                                for (fx = 0; fx < scale; fx++) {
                                    originalRow = screenRow = y + row * scale + fy;  // 沿纵向放大

                                    // 检查纵向是否超出屏幕范围
                                    if (screenRow < 0 || screenRow >= HEIGHT) {
                                        continue;
                                    }

                                    for (fx = 0; fx < scale; fx++) {
                                        originalCol = screenCol = x + col * 8 * scale + bit * scale + fx;  // 沿横向放大

                                        // 检查横向是否超出屏幕范围
                                        if (screenCol < 0 || screenCol >= WIDTH || screenRow < 0 || screenRow >= HEIGHT) {
                                            continue;
                                        }

                                        byteIndex = screenCol / 8;          // 字节索引
                                        bitIndex = 7 - (screenCol % 8);     // 位索引（高位在左）

                                        // 设置屏幕像素为白色
                                        screen[screenRow][byteIndex] |= (1 << bitIndex);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    } 
    else 
    {
        // 如果不需要旋转，则按原始方向绘制
        for (row = 0; row < height; row++) {
            imgIndex = row * (width / 8);

            // 遍历每个字节（每字节包含 8 个像素）
            for (col = 0; col < (width / 8); col++) {
                lineByte = imageData[imgIndex + col];  // 获取图像中的字节数据

                // 按位逐个检查字节中的每一位（每8位代表1个像素）
                for (bit = 0; bit < 8; bit++) {
                    if ((lineByte >> (7 - bit)) & 0x01) {  // 如果该位为 1，则为白色像素
                        // 根据反色参数决定颜色
                        if (invertColor == 1) {
                            for (fy = 0; fy < scale; fy++) {
                                for (fx = 0; fx < scale; fx++) {
                                    screenRow = y + row * scale + fy;  // 沿纵向放大

                                    // 检查纵向是否超出屏幕范围
                                    if (screenRow < 0 || screenRow >= HEIGHT) {
                                        continue;
                                    }

                                    for (fx = 0; fx < scale; fx++) {
                                        screenCol = x + col * 8 * scale + bit * scale + fx;  // 沿横向放大

                                        // 检查横向是否超出屏幕范围
                                        if (screenCol < 0 || screenCol >= WIDTH) {
                                            continue;
                                        }

                                        byteIndex = screenCol / 8;          // 字节索引
                                        bitIndex = 7 - (screenCol % 8);     // 位索引（高位在左）

                                        // 设置屏幕像素为黑色
                                        screen[screenRow][byteIndex] &= ~(1 << bitIndex);
                                    }
                                }
                            }
                        } else {
                            for (fy = 0; fy < scale; fy++) {
                                for (fx = 0; fx < scale; fx++) {
                                    screenRow = y + row * scale + fy;  // 沿纵向放大

                                    // 检查纵向是否超出屏幕范围
                                    if (screenRow < 0 || screenRow >= HEIGHT) {
                                        continue;
                                    }

                                    for (fx = 0; fx < scale; fx++) {
                                        screenCol = x + col * 8 * scale + bit * scale + fx;  // 沿横向放大

                                        // 检查横向是否超出屏幕范围
                                        if (screenCol < 0 || screenCol >= WIDTH) {
                                            continue;
                                        }

                                        byteIndex = screenCol / 8;          // 字节索引
                                        bitIndex = 7 - (screenCol % 8);     // 位索引（高位在左）

                                        // 设置屏幕像素为白色
                                        screen[screenRow][byteIndex] |= (1 << bitIndex);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

void DRAW_DisplayTest(void)
{
    uint8_t baseX = 20;
    uint8_t baseY = 20;
    uint8_t dx = 30;
    uint8_t dy = 20;
    const unsigned char* fontArray[10] = 
    {
        FONT_18PX_0,
        FONT_18PX_1,
        FONT_18PX_2,
        FONT_18PX_3,
        FONT_18PX_4,
        FONT_18PX_5,
        FONT_18PX_6,
        FONT_18PX_7,
        FONT_18PX_8,
        FONT_18PX_9 
    };

    DRAW_Image(baseX, baseY, fontArray[1], 16, 18,2,0,1);
    baseX += dx;
    DRAW_Image(baseX, baseY, fontArray[2], 16, 18,2,0,1);
    baseX += dx;
    baseY += dy;

    DRAW_Image(baseX, baseY, fontArray[5], 16, 18,2,0,1);
    baseX += dx;
    DRAW_Image(baseX, baseY, fontArray[6], 16, 18,2,0,1);
    baseX += dx;
}


void DRAW_DisplayTempHumiRot(float temperature, float humidity, boolean_t linkFlag)
{

    uint8_t baseX = 00;
    uint8_t baseY = 29;
    uint8_t dx = 24;
    uint8_t dy = 76;
    uint8_t d0,d1,i0,i1;
    uint8_t integerPart;
    float decimalPart;
    boolean_t ts = TRUE;
    const unsigned char* fontArray[10] = 
    {
        FONT_18PX_0,
        FONT_18PX_1,
        FONT_18PX_2,
        FONT_18PX_3,
        FONT_18PX_4,
        FONT_18PX_5,
        FONT_18PX_6,
        FONT_18PX_7,
        FONT_18PX_8,
        FONT_18PX_9 
    };

    if(temperature > 99.99)
    {
        temperature = 99.99;
    }
    else if(temperature < -99.99)
    {
        temperature = -99.99;
    }
    else
    {

    }

    if (temperature < 0)
    {
        ts = FALSE;
        temperature *= (-1.0);
    }

    if(humidity > 99.99)
    {
        humidity = 99.99;
    }
    else if(humidity < 0)
    {
        humidity = 0;
    }
    else
    {
        
    }

    integerPart = (uint8_t)temperature;
    decimalPart = temperature * 100 - integerPart * 100;

    i0 = integerPart % 10;
    i1 = integerPart / 10;

    d0 = (uint8_t)decimalPart % 10;
    d1 = (uint8_t)decimalPart / 10;

    if (!ts)
    {
        DRAW_Image(baseX, baseY, FONT_24PX_MINUS, 8, 18,2,1,1);
        baseX += 10;
    }
    else
    {
        baseX += 5;
    }
    
    DRAW_Image(baseX, baseY, fontArray[i1], 16, 18,2,1,1);
    baseX += dx;
    DRAW_Image(baseX, baseY, fontArray[i0], 16, 18,2,1,1);
    baseX += dx;
    baseX += 1;
    DRAW_Image(baseX, baseY, FONT_24PX_POINT, 8, 18,2,1,1);
    baseX += 8;

    DRAW_Image(baseX, baseY, fontArray[d1], 16, 18,2,1,1);
    baseX += dx;
    DRAW_Image(baseX, baseY, fontArray[d0], 16, 18,2,1,1);
    baseX += dx;
    DRAW_Image(baseX, baseY, FONT_24PX_DEGREE, 24, 18,2,1,1);

    baseX = 5;
    baseY += dy;

    integerPart = (uint8_t)humidity;
    decimalPart = humidity * 100 - integerPart * 100;

    i0 = integerPart % 10;
    i1 = integerPart / 10;

    d0 = (uint8_t)decimalPart % 10;
    d1 = (uint8_t)decimalPart / 10;

    DRAW_Image(baseX, baseY, fontArray[i1], 16, 18,2,1,1);
    baseX += dx;
    DRAW_Image(baseX, baseY, fontArray[i0], 16, 18,2,1,1);
    baseX += dx;

    baseX += 1;
    DRAW_Image(baseX, baseY, FONT_24PX_POINT, 8, 18,2,1,1);
    baseX += 8;

    DRAW_Image(baseX, baseY, fontArray[d1], 16, 18,2,1,1);
    baseX += dx;
    DRAW_Image(baseX, baseY, fontArray[d0], 16, 18,2,1,1);
    baseX += dx;
    DRAW_Image(baseX, baseY, FONT_24PX_PER, 24, 18,2,1,1);

    DRAW_Image(2, 2, FONT_24PX_WEN, 24, 23,1,1,1);
    DRAW_Image(32,2 , FONT_24PX_DU, 24, 23,1,1,1);

    DRAW_Image(2, 78, FONT_24PX_SHI, 24, 23,1,1,1);
    DRAW_Image(32, 78, FONT_24PX_DU, 24, 23,1,1,1);

    if (linkFlag)
    {
        DRAW_Image(120, 2, FONT_24PX_BTLOGO, 24, 22,1,1,1);
    }

}

/******************************************************************************
 * EOF (not truncated)
 ******************************************************************************/


