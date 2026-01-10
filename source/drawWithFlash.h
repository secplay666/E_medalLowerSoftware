#ifndef SCREEN_H
#define SCREEN_H

#include <stddef.h>
#include "base_types.h"
#include "epd.h"
#include "flash_manager.h"
// 屏幕尺寸定义
// #define WIDTH 152
// #define HEIGHT 152
// #define BYTES_PER_ROW (WIDTH / 8)
#define WIDTH WIDTH_420
#define HEIGHT HEIGHT_420
#define BYTES_PER_ROW (WIDTH / 8)

#define PAGE_SIZE 248

#define WHITE 1
#define BLACK 0
#define RED 1

// 初始化屏幕为全白
// void DRAW_initScreen(void);

// 设置某个像素点 (x, y)，color 为 0 表示白色，1 表示黑色
// void DRAW_pixel(int x, int y, int color);

// 绘制水平直线
// x: 起始点的横坐标
// y: 起始点的纵坐标
// length: 直线长度
// color: 0 表示白色，1 表示黑色
// void DRAW_hLine(int x, int y, int length, int color);

// 绘制垂直直线
// x: 起始点的横坐标
// y: 起始点的纵坐标
// length: 直线长度
// color: 0 表示白色，1 表示黑色
// void DRAW_vLine(int x, int y, int length, int color);

// 绘制矩形，filled 为 1 表示实心，0 表示空心
// void DRAW_rectangle(uint16_t x, uint16_t y, uint16_t width, uint16_t height, boolean_t color, boolean_t filled);

// 返回屏幕数据指针
// 返回值类型为 unsigned char (*)[BYTES_PER_ROW]，用于访问屏幕字节数组
// void DRAW_outputScreen(void);

// 绘制一个字符
// void DRAW_char(int x, int y, char c, int fontSize, int color);

// 绘制字符串
// void DRAW_string(int x, int y, const char *str, int fontSize, int color);

// void DRAW_rotateScreen90(void);

// void DRAW_rotatedString(int x, int y, const char *str, int fontSize, int color);
// void DRAW_rotatedChar(int x, int y, char c, int fontSize, int color);
// void DRAW_Char40_Rot(int x, int y, char c, int color);
// void DRAW_DisplayTempHumiRot(float temperature, float humidity, boolean_t linkFlag);
// void DRAW_Image(int x, int y, const unsigned char *imageData, int width, int height, int scale, int invertColor, int rotate90);
// void DRAW_DisplayTest(void);
void DRAW_initScreen(imageType_t type, uint8_t slot);

void DRAW_string(imageType_t type, uint8_t slot, uint16_t x, uint16_t y, const char *str, uint8_t fontSize, boolean_t color);

/* Test helper: write one flash page from received buffer (PAGE_SIZE bytes + 2 CRC bytes)
	pageIndex: flash page index within the image slot */
void DRAW_testWritePage(imageType_t type, uint8_t slot, uint16_t pageIndex, const uint8_t *buf, uint32_t len);

/* Quick composite image test: writes RED+BW test patterns directly to flash without UART data transfer
   Useful for rapid debugging without sending 122 data pages
   Usage: send "TEST_COMPOSITE" command to trigger this test */
void DRAW_testCompositeQuick(uint8_t slot);

#endif // SCREEN_H
