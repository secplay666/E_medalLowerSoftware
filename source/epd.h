#ifndef EPD_H
#define EPD_H
#include "base_types.h"
#include "stdint.h"

#define WIDTH_420  160
// #define WIDTH_VISIBLE = WIDTH;
#define HEIGHT_420  150
#define EPD_ARRAY   WIDTH_420*HEIGHT_420/8 
#define WIDTH WIDTH_420
#define HEIGHT HEIGHT_420
// #define BYTES_PER_ROW (WIDTH / 8)
typedef enum {
    IMAGE_BW_AND_RED = 0,
    IMAGE_RED,
    IMAGE_BW,
} imageType_t;

void EPD_initWft0154cz17(boolean_t isfull);
void EPD_poweroff(void);
// void EPD_display(unsigned char data[][BYTES_PER_ROW]);
void EPD_initwft0420cz15(void);
void EPD_displaywft0420cz15(void);
void EPD_initGDEY042Z98(void);
// void EPD_WhiteScreenGDEY042Z98ALL(unsigned char data[][BYTES_PER_ROW]);
// void EPD_display420(unsigned char data[][BYTES_PER_ROW]);
void EPD_WhiteScreenGDEY042Z98ALLBlack(void);
void EPD_WhiteScreenGDEY042Z98ALLWrite(void);
void EPD_WhiteScreenGDEY042Z98ALLRed(void);
void EPD_WhiteScreenGDEY042Z98UsingFlashDate(uint8_t slotId);


#endif // EPD_H
