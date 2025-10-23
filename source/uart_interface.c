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
 ** @file uart_interface.c
 **
 ** @brief Source file for uart_interface functions
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
#include "lpuart.h"
#include "queue.h"

/******************************************************************************
 * Local pre-processor symbols/macros ('#define')                            
 ******************************************************************************/
#define DEBUGLEVEL        1

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
static Queue uartRecdata, lpUartRecdata;
static uint8_t cmd = 0xff;
static uint32_t uartRxCount = 0;  // 统计UART接收字节数
static uint32_t queueOverflowCount = 0;  // 统计队列溢出次数

/******************************************************************************
 * Local pre-processor symbols/macros ('#define')                             
 ******************************************************************************/

/*****************************************************************************
 * Function implementation - global ('extern') and local ('static')
 ******************************************************************************/

void UART_rxIntCallback(void)
{
    volatile char data = 0;
    data = Uart_ReceiveData(UARTCH1);
    uartRxCount++;

    if (Queue_Enqueue(&uartRecdata, data))
    {
        Uart_ClrStatus(UARTCH1,UartRxFull);
    }
    else
    {
        // 队列满，数据丢失
        queueOverflowCount++;
        // 注：不在中断中输出，避免影响时序
    }
}

void UART_errIntCallback(void)
{
      Uart_ClrStatus(UARTCH1,UartRFRAMEError);

}

void LPUART_rxIntCallback(void)
{
    volatile char data = 0;
    data = LPUart_ReceiveData();

    if (Queue_Enqueue(&lpUartRecdata, data))
    {
        LPUart_ClrStatus(LPUartRxFull);
    }
}

void UARTIF_uartPrintf(uint8_t uartNumber, const char *format, ...)
{
    char buffer[256]; // 缓冲区，用于存储格式化后的字符串
    va_list args;     // 可变参数列表
    uint8_t len = 0;
    uint8_t i = 0;

    // 初始化可变参数
    va_start(args, format);

    // 格式化字符串并存入 buffer 中
    len = vsnprintf(buffer, sizeof(buffer), format, args);

    // 清理可变参数列表
    va_end(args);

    // 如果格式化成功，逐字节发送字符串
    if (len > 0) {
        for (i = 0; i < len; i++) 
        {
            // 调用 Uart_SendData 发送字符
            if (uartNumber == 0)
            {
                if (Uart_SendData(UARTCH1, (uint8_t)buffer[i]) != Ok) 
                {
                    // 发送失败处理（根据实际需求添加）
                    break;
                }
            }
           else if (uartNumber == 2)
           {
               if (LPUart_SendData((uint8_t)buffer[i]) != Ok)
               {
                   // 发送失败处理（根据实际需求添加）
                   break;
               }
           }
           else
           {
               // do nothing
           }
        }
    }
}

void UARTIF_uartPrintfFloat(uint8_t uartNumber, const char *head, const float data)
{
   uint8_t integerPart = (uint8_t)data;
   float decimalPart = data * 10000 - integerPart * 10000;
   UARTIF_uartPrintf(uartNumber, "%s%d.%d !!\n",head,integerPart,(uint16_t)decimalPart);
}


void UARTIF_uartInit(void)
{
    uint16_t timer=0;
    uint32_t pclk=0;

    stc_uart_config_t  stcConfig;
    stc_uart_irq_cb_t stcUartIrqCb;
    stc_uart_multimode_t stcMulti;
    stc_uart_baud_config_t stcBaud;
    stc_bt_config_t stcBtConfig;
    

    DDL_ZERO_STRUCT(stcUartIrqCb);
    DDL_ZERO_STRUCT(stcMulti);
    DDL_ZERO_STRUCT(stcBaud);
    DDL_ZERO_STRUCT(stcBtConfig);

    Gpio_InitIOExt(3,5,GpioDirOut,TRUE,FALSE,FALSE,FALSE);   
    Gpio_InitIOExt(3,6,GpioDirOut,TRUE,FALSE,FALSE,FALSE); 
    
    //通道端口配置
    Gpio_SetFunc_UART1TX_P35();
    Gpio_SetFunc_UART1RX_P36();
    //外设时钟使能
    Clk_SetPeripheralGate(ClkPeripheralBt,TRUE);//模式0/2可以不使能
    Clk_SetPeripheralGate(ClkPeripheralUart1,TRUE);

    stcUartIrqCb.pfnRxIrqCb = UART_rxIntCallback;
    stcUartIrqCb.pfnTxIrqCb = NULL;
    stcUartIrqCb.pfnRxErrIrqCb = UART_errIntCallback;
    stcConfig.pstcIrqCb = &stcUartIrqCb;
    stcConfig.bTouchNvic = TRUE;

    stcConfig.enRunMode = UartMode1;//测试项，更改此处来转换4种模式测试
    stcMulti.enMulti_mode = UartNormal;//测试项，更改此处来转换多主机模式，mode2/3才有多主机模式
    stcConfig.pstcMultiMode = &stcMulti;

    stcBaud.bDbaud = 1u;//双倍波特率功能
    stcBaud.u32Baud = 115200;//更新波特率位置
    stcBaud.u8Mode = UartMode1; //计算波特率需要模式参数
    pclk = Clk_GetPClkFreq();
    timer=Uart_SetBaudRate(UARTCH1,pclk,&stcBaud);

    stcBtConfig.enMD = BtMode2;
    stcBtConfig.enCT = BtTimer;
    Bt_Init(TIM1, &stcBtConfig);//调用basetimer1设置函数产生波特率
    Bt_ARRSet(TIM1,timer);
    Bt_Cnt16Set(TIM1,timer);
    Bt_Run(TIM1);

    Uart_Init(UARTCH1, &stcConfig);
    Uart_EnableIrq(UARTCH1,UartRxIrq);
    Uart_ClrStatus(UARTCH1,UartRxFull);
    Uart_EnableFunc(UARTCH1,UartRx);

    Queue_Init(&uartRecdata);
    Queue_Init(&lpUartRecdata);

}

void UARTIF_lpuartInit(void)
{
   uint32_t u32sclk;
   uint16_t u16timer;
   //    stc_clk_config_t stcClkCfg;
   stc_lpuart_config_t  stcConfig;
   stc_lpuart_irq_cb_t stcLPUartIrqCb;
   stc_lpuart_multimode_t stcMulti;
   stc_lpuart_sclk_sel_t  stcLpuart_clk;
   stc_lpuart_mode_t       stcRunMode;
   stc_lpuart_baud_config_t  stcBaud;
   stc_bt_config_t stcBtConfig;
   
   DDL_ZERO_STRUCT(stcConfig);
   DDL_ZERO_STRUCT(stcLPUartIrqCb);
   DDL_ZERO_STRUCT(stcMulti);
   DDL_ZERO_STRUCT(stcBtConfig);
   
   Clk_SetPeripheralGate(ClkPeripheralLpUart,TRUE);//使能LPUART时钟
   
   //通道端口配置
   Gpio_InitIOExt(3,3,GpioDirOut,TRUE,FALSE,FALSE,FALSE);
   Gpio_InitIOExt(3,4,GpioDirOut,TRUE,FALSE,FALSE,FALSE);

   Gpio_SetFunc_UART2RX_P33();
   Gpio_SetFunc_UART2TX_P34();
   
  
   stcLpuart_clk.enSclk_sel = LPUart_Pclk;//LPUart_Rcl;
   
   stcLpuart_clk.enSclk_Prs = LPUartDiv1;
   stcConfig.pstcLpuart_clk = &stcLpuart_clk;

   stcRunMode.enLpMode = LPUartNoLPMode;//正常工作模式或低功耗工作模式配置
   stcRunMode.enMode   = LPUartMode3;
   stcConfig.pstcRunMode = &stcRunMode;

   stcLPUartIrqCb.pfnRxIrqCb = LPUART_rxIntCallback;
   stcLPUartIrqCb.pfnTxIrqCb = NULL;
   stcLPUartIrqCb.pfnRxErrIrqCb = NULL;
   stcConfig.pstcIrqCb = &stcLPUartIrqCb;
   stcConfig.bTouchNvic = TRUE;

   stcMulti.enMulti_mode = LPUartNormal;//只有模式2/3才有多主机模式

   stcConfig.pstcMultiMode = &stcMulti;
  
   LPUart_Init(&stcConfig);

   u32sclk = Clk_GetPClkFreq();

   stcBaud.u32Baud = 19200;
   stcBaud.bDbaud = 1;
   stcBaud.u8LpMode = LPUartNoLPMode;
   stcBaud.u8Mode = LPUartMode3;
   u16timer = LPUart_SetBaudRate(u32sclk,stcLpuart_clk.enSclk_Prs,&stcBaud);
   stcBtConfig.enMD = BtMode2;
   stcBtConfig.enCT = BtTimer;
   stcBtConfig.enTog = BtTogEnable;
   Bt_Init(TIM2, &stcBtConfig);//调用basetimer2设置函数产生波特率
   Bt_ARRSet(TIM2,u16timer);
   Bt_Cnt16Set(TIM2,u16timer);
   Bt_Run(TIM2);

   LPUart_EnableFunc(LPUartRx);
   LPUart_EnableIrq(LPUartRxIrq);
   LPUart_ClrStatus(LPUartRxFull);
}

void UARTIF_passThrough(void)
{
	   uint8_t data = 0;
    if (!Queue_IsEmpty(&uartRecdata))
    {
        Queue_Dequeue(&uartRecdata, &data);
        if (data == '#')
        {
            Queue_Dequeue(&uartRecdata, &cmd);
            
            while (Queue_Dequeue(&uartRecdata, &data));
        }
        else
        {
            LPUart_SendData(data);
            while (Queue_Dequeue(&uartRecdata, &data)) 
            {
                LPUart_SendData(data);
            }

        }

        // while (Queue_Dequeue(&uartRecdata, &data)) 
        // {
        //     LPUart_SendData(data);
        // }
        data = 0;
    }

    if (!Queue_IsEmpty(&lpUartRecdata))
    {
        while (Queue_Dequeue(&lpUartRecdata, &data)) 
        {
            Uart_SendData(UARTCH1, data);
        }
        data = 0;
    }
}

uint8_t UARTIF_passThroughCmd(void)
{
    uint8_t tcmd = cmd;
    cmd = 0xff;
    return tcmd;
}

uint16_t UARTIF_fetchDataFromUart(uint8_t *buf, uint16_t *idx)
{
    uint8_t byte;
    uint16_t cnt = 0;
    if (idx == NULL) return 0;
    if (buf == NULL) return 0;

    while (!Queue_IsEmpty(&uartRecdata))
    {
        Queue_Dequeue(&uartRecdata, &byte);
        Uart_SendData(UARTCH1, byte);
        buf[(*idx)++] = byte;
        cnt++;
    }
    return cnt;
}

/**
 * @brief 获取UART接收统计信息
 * @param rxCount 指针，返回接收总字节数
 * @param overflowCount 指针，返回队列溢出次数
 */
void UARTIF_getUartStats(uint32_t *rxCount, uint32_t *overflowCount)
{
    if (rxCount != NULL)
    {
        *rxCount = uartRxCount;
    }
    if (overflowCount != NULL)
    {
        *overflowCount = queueOverflowCount;
    }
}

/**
 * @brief 重置UART统计信息
 */
void UARTIF_resetUartStats(void)
{
    uartRxCount = 0;
    queueOverflowCount = 0;
}

/******************************************************************************
 * EOF (not truncated)
 ******************************************************************************/


