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
#include "drawWithFlash.h"
#include "crc.h"

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

char buffer[256]; // 假设最大字符串长度为 256
size_t bufferIndex = 0;

// 接收处理函数原型
static void processReceivedBuffer(void);

/* 软件 CRC16-CCITT (poly 0x1021, init 0xFFFF)，用于与硬件驱动结果比对 */
static uint16_t crc16_ccitt(const uint8_t *data, uint32_t len)
{
    uint16_t crc = 0xFFFF;
    uint32_t i;
    for (i = 0; i < len; ++i)
    {
        crc ^= ((uint16_t)data[i]) << 8;
        {
            uint8_t j;
            for (j = 0; j < 8; ++j)
            {
                if (crc & 0x8000u)
                {
                    crc = (uint16_t)((crc << 1) ^ 0x1021u);
                }
                else
                {
                    crc = (uint16_t)(crc << 1);
                }
            }
        }
    }
    return crc;
}

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
    /* 确保 CRC 外设时钟已使能，驱动在调用时需要外设时钟 */
    Clk_SetPeripheralGate(ClkPeripheralCrc, TRUE);

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
            /* 先回显到 UART1 */
            Uart_SendData(UARTCH1, data);

            /* 检测回车或换行作为消息结束：判定为结束时不要把终止符写入缓冲区 */
            if (data == '\r' || data == '\n')
            {
                /* 处理接收到的完整消息（不包含终止符，含 CRC 校验） */
                processReceivedBuffer();
            }
            else
            {
                /* 将数据拼接到缓冲区中，保留一个位置给终止符 */
                if (bufferIndex < sizeof(buffer) - 1)
                {
                    buffer[bufferIndex++] = (char)data;
                }

                /* 如果刚好收到 PAGE_SIZE + 2（二进制页面帧 + 2 CRC）则直接处理为页面写入测试 */
                if (bufferIndex == (PAGE_SIZE + 2))
                {
                    processReceivedBuffer();
                }

                /* 若缓冲区已满但未收到终止符，强制结束并处理，避免丢失数据 */
                if (bufferIndex >= sizeof(buffer) - 1)
                {
                    processReceivedBuffer();
                }
            }
        }
        data = 0;
    }
}

/**
 * @brief 处理接收到的缓冲区：校验 CRC16-CCITT（高字节在前），并在校验通过时输出调试信息与显示
 */
static void processReceivedBuffer(void)
{
    if (bufferIndex == 0) return;

    if (bufferIndex >= 2)
    {
        /* 最后两个字节为 CRC，高字节在前 */
        uint8_t high = (uint8_t)buffer[bufferIndex - 2];
        uint8_t low = (uint8_t)buffer[bufferIndex - 1];
        uint16_t recv_crc = ((uint16_t)high << 8) | (uint16_t)low;

        /* 使用软件 CRC16-CCITT 作为主校验（高字节在前）并与硬件结果并列打印以便诊断 */
        {
            uint16_t sw_calc = crc16_ccitt((uint8_t *)buffer, (uint32_t)(bufferIndex - 2));

            /* 专门处理 PAGE_SIZE + 2 的二进制页面帧：若 CRC 匹配则交由 DRAW_testWriteFirstPage 处理并返回 */
            if (bufferIndex == (PAGE_SIZE + 2))
            {
                if (sw_calc == recv_crc)
                {
                    UARTIF_uartPrintf(0, "PAGE FRAME CRC OK: len=%d\r\n", (int)(bufferIndex - 2));
                    DRAW_testWriteFirstPage(IMAGE_BW, 0, (const uint8_t *)buffer, (uint32_t)bufferIndex);
                }
                else
                {
                    UARTIF_uartPrintf(0, "PAGE FRAME CRC ERR: recv=0x%04X calc_sw=0x%04X len=%d\r\n", recv_crc, sw_calc, (int)(bufferIndex - 2));
                    UARTIF_uartPrintf(0, "Recv CRC bytes: %02X %02X\r\n", (uint8_t)buffer[bufferIndex - 2], (uint8_t)buffer[bufferIndex - 1]);
                }

                /* 不走字符串显示路径，清空缓冲区并返回 */
                bufferIndex = 0;
                return;
            }

            if (sw_calc == recv_crc)
            {
                /* 校验成功（软件计算通过） */
                UARTIF_uartPrintf(0, "CRC OK: len=%d\r\n", (int)(bufferIndex - 2));
                /* 在显示前确保字符串以 NUL 终止（覆盖 CRC 起始位置） */
                buffer[bufferIndex - 2] = '\0';
                DRAW_initScreen(IMAGE_BW, 0);
                DRAW_string(IMAGE_BW, 0, 10, 10, buffer, 3, BLACK);
                (void)FM_writeImageHeader(MAGIC_BW_IMAGE_HEADER, 0);
                EPD_WhiteScreenGDEY042Z98UsingFlashDate(IMAGE_BW,0);
            }
            else
            {
                /* 校验失败 - 使用软件计算为准，并打印软件计算值供排查（已移除硬件驱动调用） */
                size_t i;
                UARTIF_uartPrintf(0, "CRC ERR: recv=0x%04X calc_sw=0x%04X len=%d\r\n", recv_crc, sw_calc, (int)(bufferIndex - 2));
                /* 打印负载十六进制 */
                UARTIF_uartPrintf(0, "Payload HEX:");
                for (i = 0; i < bufferIndex - 2; ++i)
                {
                    UARTIF_uartPrintf(0, " %02X", (uint8_t)buffer[i]);
                    /* 每16字节换行，便于阅读 */
                    if (((i + 1) % 16) == 0)
                    {
                        UARTIF_uartPrintf(0, "\r\n");
                    }
                }
                UARTIF_uartPrintf(0, "\r\n");
                /* 打印接收到的 CRC 字节 */
                UARTIF_uartPrintf(0, "Recv CRC bytes: %02X %02X\r\n", (uint8_t)buffer[bufferIndex - 2], (uint8_t)buffer[bufferIndex - 1]);
            }
        }
    }
    else
    {
        /* 数据过短，无法校验，直接显示并记录 */
        buffer[bufferIndex] = '\0';
        UARTIF_uartPrintf(0, "No CRC: len=%d\r\n", (int)bufferIndex);
        DRAW_initScreen(IMAGE_BW, 0);
        DRAW_string(IMAGE_BW, 0, 10, 10, buffer, 3, BLACK);
        (void)FM_writeImageHeader(MAGIC_BW_IMAGE_HEADER, 0);
        EPD_WhiteScreenGDEY042Z98UsingFlashDate(IMAGE_BW,0);
    }

    /* 重置缓冲区索引，准备接收下一条消息 */
    bufferIndex = 0;
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

    // 快速提取所有队列数据，不做阻塞操作（不做echo）
    // 这确保数据帧能在单个处理周期内完整接收
    while (!Queue_IsEmpty(&uartRecdata))
    {
        Queue_Dequeue(&uartRecdata, &byte);
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


