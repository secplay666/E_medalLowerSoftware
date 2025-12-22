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

void Bt0Int(void)
{
    Bt_ClearIntFlag(TIM0);

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
//    uint8_t data = 0;
//   uint8_t crc = 0;
//    boolean_t trig1s = FALSE;
    uint32_t chipId = 0;
//   uint8_t sts = 0;
//flash_result_t result = FLASH_OK;
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
    DRAW_initScreen(IMAGE_BW, 0);

    // DRAW_string(IMAGE_BW, 0, 10, 10, "Hello World", 3, BLACK);
    // (void)FM_writeImageHeader(MAGIC_BW_IMAGE_HEADER, 0);

    EPD_WhiteScreenGDEY042Z98UsingFlashDate(IMAGE_BW,0);
    // DRAW_initScreen(IMAGE_RED, 0);

    // DRAW_string(IMAGE_RED, 0, 10, 100, "Completely", 7, RED);
    // (void)FM_writeImageHeader(MAGIC_RED_IMAGE_HEADER, 0);

    // EPD_WhiteScreenGDEY042Z98UsingFlashDate(IMAGE_BW_AND_RED,0);

    UARTIF_uartPrintf(0, "Goto while ! \n");


    while(1)
    {
        UARTIF_passThrough();
        // 5ms task: image transfer processing
        // if (tg5ms)
        // {
        //     tg5ms = FALSE;
        //     ImageTransferV2_Process();
        // }

        // EPD_WhiteScreenGDEY042Z98UsingFlashDate(0x000000);
        // UARTIF_uartPrintf(0, "P1! \n");
        // delay1ms(10000);

        // EPD_WhiteScreenGDEY042Z98ALLWrite();
        // delay1ms(10000);

        // EPD_WhiteScreenGDEY042Z98UsingFlashDate(0x000100);
        // UARTIF_uartPrintf(0, "P2! \n");
        // delay1ms(10000);

        // EPD_WhiteScreenGDEY042Z98ALLWrite();
        // delay1ms(10000);

    }


}

/******************************************************************************
 * EOF (not truncated)
 ******************************************************************************/


