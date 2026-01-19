/******************************************************************************
 * Copyright (C) 2021, Xiaohua Semiconductor Co., Ltd. All rights reserved.
 *
 * This software component is licensed by XHSC under BSD 3-Clause license
 * (the "License"); You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *                    opensource.org/licenses/BSD-3-Clause
 *
 ******************************************************************************/
 
/******************************************************************************
 ** @file main.c
 **
 ** @brief Source file for MAIN functions
 **
 ** @author MADS Team 
 **
 ******************************************************************************/

/******************************************************************************
 * Include files
 ******************************************************************************/
#include <stdio.h>
// #include <stdarg.h>
// #include <stdint.h>
// #include <string.h>

#include "hc32l110.h"
#include "ddl.h"
#include "uart.h"
#include "bt.h"
#include "gpio.h"
#include "clk.h"
#include "waveinit.h"
#include "epd.h"
#include "drawWithFlash.h"
#include "lpuart.h"
#include "queue.h"
#include "uart_interface.h"
#include "e104.h"
#include "image.h"
#include "lpt.h"
#include "lpm.h"
#include "w25q32.h"
#include "flash_manager.h"
#include "image_transfer_v2.h"
// #include "testCase.h"
#include <stdlib.h>

#include "interrupts_hc32l110.h"
/******************************************************************************
 * Local pre-processor symbols/macros ('#define')                            
 ******************************************************************************/

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
// static uint8_t uart1RxFlg = 0;
static volatile uint16_t timer0 = 0;
static volatile boolean_t tg1 = FALSE;
static volatile boolean_t tg5ms = FALSE;  // 5ms task flag for image transfer
static volatile boolean_t wakeup = FALSE;
static volatile boolean_t tg8s = FALSE;
// static volatile boolean_t tg2s = FALSE;
//static float temperature = 0.0, humidity = 0.0;
//static boolean_t linkFlag = FALSE;

/******************************************************************************
 * Local pre-processor symbols/macros ('#define')                             
 ******************************************************************************/

/*****************************************************************************
 * Function implementation - global ('extern') and local ('static')
 ******************************************************************************/
uint8_t cmd[3] = {0xAC,0x33,0x00};
uint8_t u8Recdata[8]={0x00};


//编码器部分
typedef struct {
    uint8_t u8Port;
    uint8_t u8Pin;
}stc_gpio_list_t;

const stc_gpio_list_t gGpiolist[] =
{
    /*{ 0, 0 }, */{ 0, 1 }, { 0, 2 }, /*{ 0, 3 },*/
    { 1, 4 }, { 1, 5 },
    { 2, 3 }, { 2, 4 }, { 2, 5 }, { 2, 6 },
    { 2, 7 },
    { 3, 1 },
    { 3, 2 }, { 3, 3 }, { 3, 4 }, { 3, 5 }, { 3, 6 },
};

// 临时测试：中断频率测试计数
static volatile uint32_t g_u32TestInterruptCount = 0;

// 编码器计数结构体
typedef struct {
    int32_t i32Count;              // 计数值（支持正负）
    uint8_t u8LastStateA;          // P26(A相)上次状态
    uint8_t u8LastStateB;          // P25(B相)上次状态
    int8_t i8Direction;            // 旋转方向：1=正向，-1=反向，0=未动
    // uint32_t u32LastInterruptTime; // 上次中断时间（用于防抖）
}stc_encoder_t;

// 全局编码器变量
// volatile stc_encoder_t g_stcEncoder = {0, 0, 0, 0, 0};
volatile stc_encoder_t g_stcEncoder = {0, 0, 0, 0};
volatile uint32_t g_u32InterruptCount = 0;  // 中断计数器
volatile uint32_t g_u32SystemTick = 0;     // 系统节拍（毫秒）

// 图像显示相关
volatile int8_t rotation = 0;               //旋转方向
// volatile int8_t currentImageSlot = 0;  // 当前图像槽位，范围1-8
volatile int8_t g_i8LastDirection = 0;      // 上次方向
volatile uint8_t g_u8DirectionConfirmCount = 0;  // 连续相同方向计数（需要2次）

void gpio_init(void)
{
    boolean_t initStateA, initStateB;
    UARTIF_uartPrintf(0, "=== GPIO初始化开始 ===\n");
    UARTIF_uartPrintf(0, "配置编码器: P26(A相) + P25(B相)\n");

    // 配置P26(A相)为中断输入端口，双沿触发
    Gpio_InitIOExt(2, 6, GpioDirIn, TRUE, FALSE, FALSE, 0);
    Gpio_ClearIrq(2, 6);
    Gpio_EnableIrq(2, 6, GpioIrqRising);   // 上升沿
    Gpio_EnableIrq(2, 6, GpioIrqFalling);  // 下降沿
    UARTIF_uartPrintf(0, "P26(A相) 上升/下降沿中断配置完成\n");

    // 配置P25(B相)为中断输入端口，双沿触发
    Gpio_InitIOExt(2, 5, GpioDirIn, TRUE, FALSE, FALSE, 0);
    Gpio_ClearIrq(2, 5);
    Gpio_EnableIrq(2, 5, GpioIrqRising);   // 上升沿
    Gpio_EnableIrq(2, 5, GpioIrqFalling);  // 下降沿
    UARTIF_uartPrintf(0, "P25(B相) 上升/下降沿中断配置完成\n");

    // 使能NVIC中断
    EnableNvic(PORT2_IRQn, DDL_IRQ_LEVEL_DEFAULT, TRUE);
    UARTIF_uartPrintf(0, "PORT2 NVIC中断使能完成\n");

    // 读取初始状态
    initStateA = Gpio_GetIO(2, 6);
    initStateB = Gpio_GetIO(2, 5);
    g_stcEncoder.u8LastStateA = initStateA;
    g_stcEncoder.u8LastStateB = initStateB;

    UARTIF_uartPrintf(0, "P26(A相)初始状态: %d\n", g_stcEncoder.u8LastStateA);
    UARTIF_uartPrintf(0, "P25(B相)初始状态: %d\n", g_stcEncoder.u8LastStateB);
    UARTIF_uartPrintf(0, "=== GPIO初始化完成 ===\n\n");
}

void DisplayEncoderCount(void)
{
    // 显示编码器计数值和旋转方向
    const char *direction_str;
    
    switch (g_stcEncoder.i8Direction)
    {
        case 1:
            direction_str = "正向";
            break;
        case -1:
            direction_str = "反向";
            break;
        default:
            direction_str = "未动";
            break;
    }
    
    UARTIF_uartPrintf(0, "编码器计数: %d | 方向: %s | A=%d B=%d | 中断次数: %d | 时间: %dms\n",
                      g_stcEncoder.i32Count, 
                      direction_str,
                      g_stcEncoder.u8LastStateA,
                      g_stcEncoder.u8LastStateB,
                      g_u32InterruptCount,
                      g_u32SystemTick);
}

void Gpio_IRQHandler(uint8_t u8Param)
{
    // ====== 四相查表+相邻状态保护编码器处理 ======
    uint8_t u8CurrentStateA;
    uint8_t u8CurrentStateB;
    uint8_t prevAB;
    uint8_t currAB;
    int8_t step;
    static const int8_t stateTable[4][4] = {
        // 00, 01, 10, 11
        { 0,  1, -1,  0}, // 00->
        {-1,  0,  0,  1}, // 01->
        { 1,  0,  0, -1}, // 10->
        { 0, -1,  1,  0}  // 11->
    };
		

    if (u8Param != 2) {
        *((uint32_t *)((uint32_t)&M0P_GPIO->P0ICLR + u8Param * 0x40)) = 0;
        return;
    }
    
    u8CurrentStateA = Gpio_GetIO(2, 6);
    u8CurrentStateB = Gpio_GetIO(2, 5);
    prevAB = ((g_stcEncoder.u8LastStateA & 0x1) << 1) | (g_stcEncoder.u8LastStateB & 0x1);
    currAB = ((u8CurrentStateA & 0x1) << 1) | (u8CurrentStateB & 0x1);

    // 只允许相邻状态跳变，否则丢弃本次采样
    step = stateTable[prevAB][currAB];
    if ((prevAB != currAB) && (step != 0)) {
        g_stcEncoder.i32Count += step;
        g_stcEncoder.i8Direction = (step > 0) ? 1 : -1;

        // 调试：打印每次有效跳变
        //UARTIF_uartPrintf(0, "ENC: prevAB=%d currAB=%d step=%d cnt=%ld dir=%d\n",
        //    prevAB, currAB, step, g_stcEncoder.i32Count, g_stcEncoder.i8Direction);

        // 方向确认逻辑（可选，防止误判）
        if (g_stcEncoder.i8Direction == g_i8LastDirection) {
            g_u8DirectionConfirmCount++;
            if (g_u8DirectionConfirmCount >= 2) {
                if(rotation == 0) {
                    if (g_stcEncoder.i8Direction > 0) {
                        rotation = 1;
                    } else {
                        rotation = -1;
                    }
                }
                g_u8DirectionConfirmCount = 0;
            }
        } else {
            g_i8LastDirection = g_stcEncoder.i8Direction;
            g_u8DirectionConfirmCount = 1;
        }
        g_u32InterruptCount++;
    }
    // 更新上次状态（必须在最后更新）
    g_stcEncoder.u8LastStateA = u8CurrentStateA;
    g_stcEncoder.u8LastStateB = u8CurrentStateB;


    // 清除中断标志位
    *((uint32_t *)((uint32_t)&M0P_GPIO->P0ICLR + u8Param * 0x40)) = 0;
}


//-----------------------------------------------------------
void Bt0Int(void)//20ms一次
{
    Bt_ClearIntFlag(TIM0);

    g_u32SystemTick++;

    // Call passThrough and image transfer processing every 1ms
    // UARTIF_passThrough();
    (void)E104_getLinkState();

    // High frequency image transfer processing (every 1ms)
    tg5ms = TRUE;  // Note: now 1ms, not 5ms, but keep variable name for compatibility

    // Maintain original timer counter logic for generating other time flags
    if (timer0 < 7999)  // 1s (originally 200->5ms, now 8000->1ms)
    {
        timer0++;
    }
    else
    {
        timer0 = 0;
    }

    if ((timer0 % 10) == 0) // 10ms (originally 2->10ms, now 10->10ms)
    {
        tg1 = TRUE;
    }
}

void LptInt(void)
{
    if (TRUE == Lpt_GetIntFlag())
    {
        Lpt_ClearIntFlag();
        wakeup = TRUE;
        if (timer0 < 4)  // 10s
        {
            timer0++;
        }
        else
        {
            timer0 = 0;
            tg8s = TRUE;
        }
    }
}


/**********************************************************
*  CRC Check Type: CRC8
*  Polynomial: X8+X5+X4+1
*  Poly:0011 0001 0x31
**********************************************************/

//static uint8_t calcCrc8(unsigned char *message,unsigned char num)
//{
//   uint8_t i;
//   uint8_t byte;
//   uint8_t crc =0xFF;
//   for (byte = 0;byte<num;byte++)
//   {
//       crc^=(message[byte]);
//       for(i=8;i>0;--i)
//       {
//           if(crc&0x80)
//               crc=(crc<<1)^0x31;
//           else
//               crc=(crc<<1);
//       }
//   }
//   return crc;
//}


//static float temperatureConvert(uint32_t d0, uint32_t d1, uint32_t d2)
//{
//   float result = 0.0;
//   uint32_t data = (d2 | (d1 << 8) | (((d0 & 0x0000000f)) << 16));
//   result = (data / 1048576.0)*200.0 - 50.0;
//   return result;
//}

//static float humidityConvert(uint32_t d0, uint32_t d1, uint32_t d2)
//{
//   float result = 0.0;
//   uint32_t data = (((d2 & 0x000000f0) >> 4) | (d1 << 4) | (d0 << 12));
//   result = (data / 1048576.0)*100.0;
//   return result;
//}

static void timInit(void)
{
    stc_bt_config_t   stcConfig;

    stcConfig.pfnTim1Cb = NULL;

    stcConfig.enGateP = BtPositive;
    stcConfig.enGate  = BtGateDisable;
    stcConfig.enPRS   = BtPCLKDiv8;  // PCLK/8
    stcConfig.enTog   = BtTogDisable;
    stcConfig.enCT    = BtTimer;
    stcConfig.enMD    = BtMode2;
    //Bt初始化
    Bt_Stop(TIM0);

    Bt_Init(TIM0, &stcConfig);

    //TIM中断使能
    Bt_ClearIntFlag(TIM0);
    Bt_EnableIrq(TIM0);
    EnableNvic(TIM0_IRQn, 0, TRUE);

    // Set reload value for interrupt period of 1ms (changed to high frequency data reception)
    // Assume PCLK=8MHz, prescale=/8, actual clock=1MHz
    // 1ms needs 1000 counts: 1000-1 = 999 = 0x3E7
    // Original config 0xC537(50487) produces ~50ms interrupt, insufficient receive buffer
    // To handle streaming data faster, changed to 1ms interrupt
    Bt_ARRSet(TIM0, 0x03E7);   // 1ms interrupt for faster data reception
    Bt_Cnt16Set(TIM0, 0x03E7);
    Bt_Run(TIM0);

}

//static void lpmInit(void)
//{
//    stc_lpt_config_t stcConfig;
//    stc_lpm_config_t stcLpmCfg;
//    uint16_t         u16ArrData = 0;

//    Clk_Enable(ClkRCL, TRUE);
//    // Enable Lpt and GPIO peripheral clock
//    Clk_SetPeripheralGate(ClkPeripheralLpTim, TRUE);

//    stcConfig.enGateP  = LptPositive;
//    stcConfig.enGate   = LptGateDisable;
//    stcConfig.enTckSel = LptIRC32K;
//    stcConfig.enTog    = LptTogDisable;
//    stcConfig.enCT     = LptTimer;
//    stcConfig.enMD     = LptMode2;
//    
//    stcConfig.pfnLpTimCb = LptInt;
//    Lpt_Init(&stcConfig);
//    //Lpm Cfg
//    stcLpmCfg.enSEVONPEND   = SevPndDisable;
//    stcLpmCfg.enSLEEPDEEP   = SlpDpEnable;
//    stcLpmCfg.enSLEEPONEXIT = SlpExtDisable;
//    Lpm_Config(&stcLpmCfg);
//    
//    // Enable Lpt interrupt
//    Lpt_ClearIntFlag();
//    Lpt_EnableIrq();
//    EnableNvic(LPTIM_IRQn, 0, TRUE);
//
//
//    // Set reload value, count initial value, start counting
//    Lpt_ARRSet(u16ArrData);
//    Lpt_Run();

//    // Enter low power mode...
//    Lpm_GotoLpmMode(); 

//}

//static void task3(void)
//{
//    if (tg1) // 10ms
//    {
//        tg1 = FALSE;
//        UARTIF_passThrough();
//        (void)E104_getLinkState();
//        // (void)E104_getDataState();
//        E104_executeCommand();
//    }
//}

//static void task1(void)
//{
//    DRAW_initScreen();
//    DRAW_DisplayTempHumiRot(temperature,humidity,linkFlag);

//    DRAW_outputScreen();
//}

//static void task0(void)
//{
//    temperature = 12.34;
//    humidity = 56.78;
//    UARTIF_uartPrintfFloat(0, "Temperature is ",temperature);
//    UARTIF_uartPrintfFloat(0, "Humidity is ",humidity);

//    if (E104_getLinkState())
//    {
//        UARTIF_uartPrintfFloat(2, "Temperature is ",temperature);
//        UARTIF_uartPrintfFloat(2, "Humidity is ",humidity);
//        if (!linkFlag)
//        {
//            linkFlag = TRUE;
//        }
//    }
//    else
//    {
//        if (linkFlag)
//        {
//            linkFlag = FALSE;
//            E104_setWakeUpMode();

//            delay1ms(30);
//            E104_setSleepMode();
//        }

//    }
//}

//static void handleClearEpdEvent(void)
//{
//    typedef enum 
//    {
//        STATE_IDLE,     // Idle state
//        STATE_WAITING_3_4,  // Wait for third wake
//        STATE_WAITING_4,  // Wait for fourth wake
//    } State;

//    static State currentState = STATE_IDLE;
//    static uint8_t counter = 0;
//    // static boolean_t firstflag = TRUE;

//    switch (currentState)
//    {
//        case STATE_IDLE:
//            if (counter > 40 && tg8s)
//            {
//                currentState = STATE_WAITING_3_4;
//                counter = 0;
//            }
//            else
//            {
//                counter++; 
//            }
//            break;
//        case STATE_WAITING_3_4:
//            if (counter < 2)
//            {
//                counter++;
//            }
//            else
//            {
//                EPD_poweroff();
//                delay1ms(10);
//                EPD_initWft0154cz17(TRUE);
//                currentState = STATE_WAITING_4;
//                counter = 0;
//            }
//            break;
//        case STATE_WAITING_4:
//            if (counter < 3)
//            {
//                counter++;
//            }
//            else
//            {
//                EPD_initWft0154cz17(FALSE);
//                currentState = STATE_IDLE;
//                counter = 0;
//            }
//            break;
//        default:
//            break;
//    }
//    // UARTIF_uartPrintf(0, "State is %d, counter is %d.\n",currentState,counter);
//}


/**
 ******************************************************************************
 ** \brief  Main function of project
 **
 ** \return uint32_t return value, if needed
 **
 ******************************************************************************/
int32_t main(void)
{
    /*   编码器   */
    uint32_t u32LastDisplayCount = 0;
	boolean_t currentState = 0;
    /*   编码器   */

//    uint8_t data = 0;
//   uint8_t crc = 0;
//    boolean_t trig1s = FALSE;
    uint32_t chipId = 0;
//   uint8_t sts = 0;
    flash_result_t result = FLASH_OK;
    UARTIF_uartInit();
    // i2cInit();
    UARTIF_lpuartInit();
    E104_ioInit();
    W25Q32_Init();
    delay1ms(30);
    E104_setSleepMode();

    timInit();

    EPD_initGDEY042Z98();
    
    UARTIF_uartPrintf(0, "Done! \n");
    delay1ms(100);
    chipId = W25Q32_ReadID();
    delay1ms(100);
    UARTIF_uartPrintf(0, "Chip id is 0x%x ! \n", chipId);
    delay1ms(100);

    /*   编码器   */
	// 初始化计数显示
    gpio_init();
	/*   编码器   */

    // Erase sector 0 (address 0x000000)
    // When using 0x20 to erase sector, erase address like 0x003000, last three bits unused
//    W25Q32_EraseSector(FLASH_SEGMENT0_BASE);
//    W25Q32_EraseSector(FLASH_SEGMENT1_BASE);

        // W25Q32_EraseChip();
// UARTIF_uartPrintf(0, "Start erase block 0!\n");
    //  W25Q32_Erase64k(0x000000);
// UARTIF_uartPrintf(0, "Start erase block 4!\n");
    // W25Q32_Erase64k(0x040000);

   // Write one page of data
//    UARTIF_uartPrintf(0, "Start write block test!\n");
//    writeBlockTest();
//     delay1ms(500);
//    UARTIF_uartPrintf(0, "Start read block test!\n");
//    readBlockTest();

//    for (i = 0;i<0x3f;i++)
//    {
//       UARTIF_uartPrintf(0, "Start write block test at blockAddress 0x%x!!\n",i);
//       writeBlockTest(i);

//    }
    delay1ms(500);
//     for (i = 0;i<0x1f;i++)
//    {
//       UARTIF_uartPrintf(0, "Start read block test at blockAddress 0x%x!!\n",i);
//       readBlockTest(i);

//    }

//    UARTIF_uartPrintf(0, "Write page 0 as 0x77! \n");
//    memset(buffer, 0x77, 256);  // Fill test data
//    W25Q32_WritePage(0x000000, buffer, 256);


//    UARTIF_uartPrintf(0, "Write page 1 as 0x55! \n");
//    memset(buffer, 0x55, 256);  // Fill test data
//    W25Q32_WritePage(0x000100, buffer, 256);



    //    EPD_WhiteScreenGDEY042Z98UsingFlashDate();

    // testReadRawData();
    // testReadRawDataByAddress(0x00003e00);
    if (FM_init() == FLASH_OK)
    {
        UARTIF_uartPrintf(0, "flash_manager init completely!\n");
    }

    // Initialize image transfer module
    // ImageTransferV2_Init();
    // UARTIF_uartPrintf(0, "image_transfer init completely!\n");

    // // ==================== Debug: Test UART and Protocol ====================
    // UARTIF_uartPrintf(0, "\n");
    // UARTIF_uartPrintf(0, "===================================================\n");
    // UARTIF_uartPrintf(0, "   Image Transfer Protocol V2 - Debug Mode Started\n");
    // UARTIF_uartPrintf(0, "===================================================\n");
    // UARTIF_uartPrintf(0, "[DEBUG] MCU Ready, waiting for PC START command...\n");
    // UARTIF_uartPrintf(0, "[DEBUG] UART Baud Rate: 9600 baud\n");
    // UARTIF_uartPrintf(0, "[DEBUG] Flash Manager: %s\n", (FM_init() == FLASH_OK) ? "OK" : "FAIL");
    // UARTIF_uartPrintf(0, "[DEBUG] If START command not seen, check:\n");
    // UARTIF_uartPrintf(0, "  1. PC to MCU UART connection\n");
    // UARTIF_uartPrintf(0, "  2. Baud rate settings match\n");
    // UARTIF_uartPrintf(0, "  3. PC sent START correctly\n");
    // UARTIF_uartPrintf(0, "===================================================\n\n");
    // testFlashManagerReadAndWrite();
    // testFlashManagerReadAndWrite();
    // FM_forceGarbageCollect();
    // testFlashManagerRead();
    // testFlashManagerGarbageCollection();
    // testFlashManagerReadAndWrite2();
    // testFlashManagerRead2();
    // testWriteImage();
    // testWriteImageOnePage(0x09, 0xEA);
    // testWriteImageOnePage(0x16, 0xEB);
    // testWriteImageOnePage(0x23, 0xEC);
    // testWriteImageOnePage(0x30, 0xED);

    // testFlashManagerRead2();
    // result = FM_writeImageHeader(MAGIC_BW_IMAGE_HEADER, 0x01);

    // if (result == FLASH_OK)
    // {
    //     UARTIF_uartPrintf(0, "Write image header success! \n");
    // }
    // else
    // {
    //     UARTIF_uartPrintf(0, "Write image header fail! error code is %d \n", result);
    // }
    // testReadImage();

    // testWriteImage4();
    // testReadImage4();

    // TEST_WriteImage();
    // DRAW_initScreen(IMAGE_BW, 1);

    // DRAW_string(IMAGE_BW, 0, 10, 10, "Hello World", 3, BLACK);
    // (void)FM_writeImageHeader(MAGIC_BW_IMAGE_HEADER, 0);

    // EPD_WhiteScreenGDEY042Z98UsingFlashDate(IMAGE_BW,1);
    // DRAW_initScreen(IMAGE_RED, 0);

    // DRAW_string(IMAGE_RED, 0, 10, 100, "Completely", 7, RED);
    // (void)FM_writeImageHeader(MAGIC_RED_IMAGE_HEADER, 0);

    // EPD_WhiteScreenGDEY042Z98UsingFlashDate(IMAGE_BW_AND_RED,0);
    result = FM_readData(DATA_PAGE_MAGIC, 0, (uint8_t *)&currentImageSlot, 1); // Read saved slot
    UARTIF_uartPrintf(0, "Goto while ! \n");
    
    /* ===== QUICK COMPOSITE TEST ===== */
    /* Uncomment this line to automatically test RED+BW composite display on startup */
    //DRAW_testCompositeQuick(0);
    /* ====================================== */

    while(1)
    {
        UARTIF_passThrough();
        
        if (rotation == 1) {
            UARTIF_uartPrintf(0, "Rotation detected: %d\n", rotation);
            currentImageSlot++;
            UARTIF_uartPrintf(0, "Current Image Slot: %d\n", currentImageSlot);
            if (currentImageSlot > 7)
                currentImageSlot = 0;
            EPD_WhiteScreenGDEY042Z98UsingFlashDate(currentImageSlot);
            result = FM_writeData(DATA_PAGE_MAGIC, 0, (uint8_t *)&currentImageSlot, 1); // Save current slot
            delay1ms(6000);
            rotation = 0;  // Reset rotation after handling
        } else if (rotation == -1) {
            UARTIF_uartPrintf(0, "Rotation detected: %d\n", rotation);
            currentImageSlot--;
            UARTIF_uartPrintf(0, "Current Image Slot after decrement: %d\n", currentImageSlot);
            if (currentImageSlot < 0)
                currentImageSlot = 7;
            EPD_WhiteScreenGDEY042Z98UsingFlashDate(currentImageSlot);
            result = FM_writeData(DATA_PAGE_MAGIC, 0, (uint8_t *)&currentImageSlot, 1); // Save current slot
            delay1ms(6000);
            rotation = 0;  // Reset rotation after handling
        }

        // 5ms task: image transfer processing
        // if (tg5ms)
        // {
        //     tg5ms = FALSE;
        //     ImageTransferV2_Process();
        // }

    }


}

/******************************************************************************
 * EOF (not truncated)
 ******************************************************************************/


