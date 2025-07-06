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
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

#include "hc32l110.h"
#include "ddl.h"
#include "uart.h"
#include "bt.h"
#include "gpio.h"
#include "clk.h"
#include "waveinit.h"
#include "epd.h"
#include "draw.h"
#include "lpuart.h"
#include "queue.h"
#include "uart_interface.h"
#include "e104.h"
#include "image.h"
#include "lpt.h"
#include "lpm.h"
#include "w25q32.h"
#include "flash_manager.h"
#include "image_transfer_manager.h"
#include "image_display_new.h"
// #include "flash_test.h"

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
static volatile boolean_t wakeup = FALSE;
static volatile boolean_t tg8s = FALSE;
// static volatile boolean_t tg2s = FALSE;
static float temperature = 0.0, humidity = 0.0;
static boolean_t linkFlag = FALSE;
static flash_manager_t g_flash_manager;
static image_transfer_manager_t g_image_transfer_manager;
static image_display_new_t g_image_display;
static uint8_t read_buffer[64];
static uint8_t test_data[] = "Hello Flash Manager!";
extern Queue lpUartRecdata;
/******************************************************************************
 * Local pre-processor symbols/macros ('#define')                             
 ******************************************************************************/

/*****************************************************************************
 * Function implementation - global ('extern') and local ('static')
 ******************************************************************************/
uint8_t cmd[3] = {0xAC,0x33,0x00};
uint8_t u8Recdata[8]={0x00};
static uint8_t buffer[256];

void Bt0Int(void)
{

    UARTIF_passThrough();
    (void)E104_getLinkState();

    /* 5ms */
    // if (TRUE == Bt_GetIntFlag(TIM0))
    // {
        Bt_ClearIntFlag(TIM0);
    // }
    // timer0++;
    if (timer0 < 199)  // 1s
    {
        timer0++;
    }
    else
    {
        timer0 = 0;
    }

    if ((timer0 % 2) == 0) // 10ms
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
*  CRC校验类型：CRC8
*  多项式：X8+X5+X4+1
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

static void timInit(void)
{
    stc_bt_config_t   stcConfig;
    
    stcConfig.pfnTim1Cb = NULL;
        
    stcConfig.enGateP = BtPositive;
    stcConfig.enGate  = BtGateDisable;
    stcConfig.enPRS   = BtPCLKDiv8;
    stcConfig.enTog   = BtTogDisable;
    stcConfig.enCT    = BtTimer;
    stcConfig.enMD    = BtMode2;
    //Bt初始化
    Bt_Stop(TIM0);

    Bt_Init(TIM0, &stcConfig);
    
    //TIM1中断使能
    Bt_ClearIntFlag(TIM0);
    Bt_EnableIrq(TIM0);
    EnableNvic(TIM0_IRQn, 0, TRUE);

    //设置重载值和计数值，启动计数
    Bt_ARRSet(TIM0, 0xC537);
    Bt_Cnt16Set(TIM0, 0xC537);
    Bt_Run(TIM0);

}

static void lpmInit(void)
{
    stc_lpt_config_t stcConfig;
    stc_lpm_config_t stcLpmCfg;
    uint16_t         u16ArrData = 0;

    Clk_Enable(ClkRCL, TRUE);
    //使能Lpt、GPIO外设时钟
    Clk_SetPeripheralGate(ClkPeripheralLpTim, TRUE);

    stcConfig.enGateP  = LptPositive;
    stcConfig.enGate   = LptGateDisable;
    stcConfig.enTckSel = LptIRC32K;
    stcConfig.enTog    = LptTogDisable;
    stcConfig.enCT     = LptTimer;
    stcConfig.enMD     = LptMode2;
    
    stcConfig.pfnLpTimCb = LptInt;
    Lpt_Init(&stcConfig);
    //Lpm Cfg
    stcLpmCfg.enSEVONPEND   = SevPndDisable;
    stcLpmCfg.enSLEEPDEEP   = SlpDpEnable;
    stcLpmCfg.enSLEEPONEXIT = SlpExtDisable;
    Lpm_Config(&stcLpmCfg);
    
    //Lpt 中断使能
    Lpt_ClearIntFlag();
    Lpt_EnableIrq();
    EnableNvic(LPTIM_IRQn, 0, TRUE);
    
    
    //设置重载值，计数初值，启动计数
    Lpt_ARRSet(u16ArrData);
    Lpt_Run();

    // 进入低功耗模式……
    Lpm_GotoLpmMode(); 

}

static void task3(void)
{
    if (tg1) // 10ms
    {
        tg1 = FALSE;
        UARTIF_passThrough();
        (void)E104_getLinkState();
        // (void)E104_getDataState();
        E104_executeCommand();
    }
}

// static void task1(void)
// {
//     DRAW_initScreen();
//     DRAW_DisplayTempHumiRot(temperature,humidity,linkFlag);

//     DRAW_outputScreen();
// }

static void task0(void)
{
    temperature = 12.34;
    humidity = 56.78;
    UARTIF_uartPrintfFloat(0, "Temperature is ",temperature);
    UARTIF_uartPrintfFloat(0, "Humidity is ",humidity);

    if (E104_getLinkState())
    {
        UARTIF_uartPrintfFloat(2, "Temperature is ",temperature);
        UARTIF_uartPrintfFloat(2, "Humidity is ",humidity);
        if (!linkFlag)
        {
            linkFlag = TRUE;
        }
    }
    else
    {
        if (linkFlag)
        {
            linkFlag = FALSE;
            E104_setWakeUpMode();

            delay1ms(30);
            E104_setSleepMode();
        }

    }
}

static void handleClearEpdEvent(void)
{
    typedef enum 
    {
        STATE_IDLE,     // 空闲状态
        STATE_WAITING_3_4,  // 等待第三次唤醒
        STATE_WAITING_4,  // 等待第四次唤醒
    } State;

    static State currentState = STATE_IDLE;
    static uint8_t counter = 0;
    // static boolean_t firstflag = TRUE;

    switch (currentState)
    {
        case STATE_IDLE:
            if (counter > 40 && tg8s)
            {
                currentState = STATE_WAITING_3_4;
                counter = 0;
            }
            else
            {
                counter++; 
            }
            break;
        case STATE_WAITING_3_4:
            if (counter < 2)
            {
                counter++;
            }
            else
            {
                EPD_poweroff();
                delay1ms(10);
                EPD_initWft0154cz17(TRUE);
                currentState = STATE_WAITING_4;
                counter = 0;
            }
            break;
        case STATE_WAITING_4:
            if (counter < 3)
            {
                counter++;
            }
            else
            {
                EPD_initWft0154cz17(FALSE);
                currentState = STATE_IDLE;
                counter = 0;
            }
            break;
        default:
            break;
    }
    // UARTIF_uartPrintf(0, "State is %d, counter is %d.\n",currentState,counter);
}

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
	flash_result_t result = FLASH_OK;
    uint32_t chipId = 0;
   uint16_t i = 0;
    uint8_t read_size = sizeof(read_buffer);
	 uint32_t used_pages, free_pages, data_count;
    UARTIF_uartInit();
    // i2cInit();
    UARTIF_lpuartInit();
    E104_ioInit();
    W25Q32_Init();
    delay1ms(30);
    E104_setSleepMode();

    // while (data != 0x36)
    // {
    //     UARTIF_uartPrintf(0, "Input 6 to start.\n");
    //     data = Uart_ReceiveData(UARTCH1);
    //     delay1ms(500);
    // }
    timInit();
    // EPD_initWft0154cz17(TRUE);
    EPD_initGDEY042Z98();
    // Gpio_InitIO(0, 1, GpioDirOut);
    // Gpio_SetIO(0, 1, 1);               //DC输出高

    // Gpio_InitIO(0, 2, GpioDirOut);
    // Gpio_SetIO(0, 2, 1);               //RST输出高

    // Gpio_InitIO(0, 3, GpioDirOut);
    // Gpio_SetIO(0, 3, 1);               //RST输出高


    // Gpio_InitIO(1, 4, GpioDirOut);
    // Gpio_SetIO(1, 4, 1);               //DC输出高

    // Gpio_InitIO(1, 5, GpioDirOut);
    // Gpio_SetIO(1, 5, 1);               //DC输出高

    // Gpio_InitIO(2, 3, GpioDirOut);
    // Gpio_SetIO(2, 3, 0);               //
    // Gpio_InitIO(2, 4, GpioDirOut);
    // Gpio_SetIO(2, 4, 0);               //
    
    UARTIF_uartPrintf(0, "Done! \n");
    delay1ms(100);
    chipId = W25Q32_ReadID();
    delay1ms(100);
    UARTIF_uartPrintf(0, "Chip id is 0x%x ! \n", chipId);
    delay1ms(100);
    
    // 初始化Flash管理器
    UARTIF_uartPrintf(0, "Initializing Flash Manager...\n");
    result = flash_manager_init(&g_flash_manager);
    
    // 初始化图像传输管理器
    if (result == FLASH_OK) {
        result = image_transfer_init(&g_image_transfer_manager, &g_flash_manager, &lpUartRecdata);
        if (result == FLASH_OK) {
            UARTIF_uartPrintf(0, "Image Transfer Manager initialized!\n");
        } else {
            UARTIF_uartPrintf(0, "Failed to initialize Image Transfer Manager: %d\n", result);
        }
        
        // 初始化图像显示器
        result = image_display_new_init(&g_image_display, &g_flash_manager);
        if (result == FLASH_OK) {
            UARTIF_uartPrintf(0, "Image Display initialized!\n");
        } else {
            UARTIF_uartPrintf(0, "Failed to initialize Image Display: %d\n", result);
        }
    }
    if (result == FLASH_OK) {
        UARTIF_uartPrintf(0, "Flash Manager initialized successfully!\n");
        
        // 获取Flash状态

        flash_get_status(&g_flash_manager, &used_pages, &free_pages, &data_count);
        UARTIF_uartPrintf(0, "Flash Status: Used=%d, Free=%d, Data=%d\n", used_pages, free_pages, data_count);
        
        // 测试写入数据

        result = flash_write_data(&g_flash_manager, 0x1001, test_data, (uint8_t)sizeof(test_data));
        if (result == FLASH_OK) {
            UARTIF_uartPrintf(0, "Test data written successfully!\n");
            
            // 测试读取数据
            
            result = flash_read_data(&g_flash_manager, 0x1001, read_buffer, &read_size);
            if (result == FLASH_OK) {
                UARTIF_uartPrintf(0, "Test data read successfully: %s\n", read_buffer);
            } else {
                UARTIF_uartPrintf(0, "Failed to read test data: %d\n", result);
            }
        } else {
             UARTIF_uartPrintf(0, "Failed to write test data: %d\n", result);
         }
         
         // 运行详细测试
        //  flash_manager_test();
         
        //  // 运行垃圾回收测试
        //  flash_gc_test();
         
     } else {
         UARTIF_uartPrintf(0, "Failed to initialize Flash Manager: %d\n", result);
     }

    // sts = W25Q32_ReadStatusReg();
    // UARTIF_uartPrintf(0, "Status Reg is 0x%x ! \n", sts);
    // UARTIF_uartPrintf(0, "Write Enable ! \n");
    // W25Q32_WriteEnable();
    // delay1ms(100);
    // UARTIF_uartPrintf(0, "Erase page 0!\n");
    // W25Q32_EraseSector(0x000000);
    // do 
    // {
    //     sts = W25Q32_ReadStatusReg();
    //     UARTIF_uartPrintf(0, "Status Reg is 0x%x ! \n", sts);
    //     delay1ms(300);
    // }
    // while(sts & 0x01);

    // 擦除第0扇区 (地址0x000000)
//    W25Q32_EraseSector(0x000000);  

   // 写入一页数据
//    UARTIF_uartPrintf(0, "Write page 0 as 0xAA! \n");

//    memset(buffer, 0xAA, 256);  // 填充测试数据

//    W25Q32_WritePage(0x000000, buffer, 256);
//     do 
//     {
//         sts = W25Q32_ReadStatusReg();
//         UARTIF_uartPrintf(0, "Status Reg is 0x%x ! \n", sts);
//         delay1ms(300);
//     }
//     while(sts & 0x01);

//    // 读取验证
   UARTIF_uartPrintf(0, "Read page 0! \n");
   memset(buffer, 0, 256);
   W25Q32_ReadData(0x000000, buffer, 256);
   delay1ms(100);
   for (;i<256;i++)
   {
       UARTIF_uartPrintf(0, "Byte %d is 0x%x! \n", i,buffer[i]);
       delay1ms(1);
   }

	UARTIF_uartPrintf(0, "Goto while ! \n");

    while(1)
    {
        // 处理图像传输
        image_transfer_process(&g_image_transfer_manager);
        
        // 检查传输状态
        if (g_image_transfer_manager.context.state == IMG_TRANSFER_COMPLETE) {
            UARTIF_uartPrintf(0, "Image transfer completed! Slot=%d\n", 
                             g_image_transfer_manager.context.current_slot);
            
            // 自动显示接收到的图像
            if (g_image_display.state == DISPLAY_STATE_IDLE) {
                image_display_new_show(&g_image_display, g_image_transfer_manager.context.current_slot);
            }
            
            image_transfer_reset(&g_image_transfer_manager);
        }
        
        // 处理图像显示
        image_display_new_process(&g_image_display);
        
        // 检查显示状态
        if (g_image_display.state == DISPLAY_STATE_COMPLETE) {
            UARTIF_uartPrintf(0, "Image display completed!\n");
            image_display_new_reset(&g_image_display);
        } else if (g_image_display.state == DISPLAY_STATE_ERROR) {
            UARTIF_uartPrintf(0, "Image display error! Resetting...\n");
            image_display_new_reset(&g_image_display);
        }
        
        // Gpio_SetIO(0, 1, 1);               //DC输出高
        // Gpio_SetIO(0, 3, 1);               //RST输出高
        // Gpio_SetIO(0, 2, 1);               //RST输出高
        // Gpio_SetIO(1, 4, 1);               //DC输出高
        // Gpio_SetIO(1, 5, 1);               //DC输出高
        // Gpio_SetIO(2, 3, 1);               //DC输出高
        // Gpio_SetIO(2, 4, 1);               //RST输出
        // delay1ms(3000);
        //     UARTIF_uartPrintf(0, "Low! \n");

        // Gpio_SetIO(0, 1, 0);               //DC输出高
        // Gpio_SetIO(0, 3, 0);               //RST输出高
        // Gpio_SetIO(0, 2, 0);               //RST输出高
        // Gpio_SetIO(1, 4, 0);               //DC输出高
        // Gpio_SetIO(1, 5, 0);               //DC输出高
        // Gpio_SetIO(2, 3, 0);               //DC输出高
        // Gpio_SetIO(2, 4, 0);               //RST输出
        // delay1ms(3000);
        //     UARTIF_uartPrintf(0, "High! \n");

        // task3();
    //    EPD_WhiteScreenGDEY042Z98ALLBlack();
    //    UARTIF_uartPrintf(0, "P1! \n");
    //    delay1ms(10000);
    //    EPD_WhiteScreenGDEY042Z98ALLWrite();
    //    UARTIF_uartPrintf(0, "P2! \n");
    //    delay1ms(10000);
    //    EPD_WhiteScreenGDEY042Z98ALLRed();
    //    UARTIF_uartPrintf(0, "P3! \n");
    //    delay1ms(10000);

        // if (wakeup)
        // {
        //     wakeup = FALSE;
        //     task0();
        //     // UARTIF_uartPrintf(0, "Wake up! \n");
        //     // handleClearEpdEvent();
        //     if (tg8s)
        //     {
        //         tg8s = FALSE;
        //         task1();
        //         // UARTIF_uartPrintf(0, "tg8s! \n");
        //     }
        //     Lpm_GotoLpmMode(); 
        // }
    }
}

/******************************************************************************
 * EOF (not truncated)
 ******************************************************************************/


