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
#include <stdbool.h>
#include <stdlib.h>

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
void UARTIF_uartPrintf(uint8_t uartNumber, const char *format, ...);

/******************************************************************************
 * Local variable definitions ('static')                                      *
 ******************************************************************************/
static Queue uartRecdata, lpUartRecdata;
static uint8_t cmd = 0xff;
static uint32_t uartRxCount = 0;  // 统计UART接收字节数
static uint32_t queueOverflowCount = 0;  // 统计队列溢出次数

char buffer[256]; // 假设最大字符串长度为 256
size_t bufferIndex = 0;

/* 支持接收多页（每页 PAGE_SIZE 字节），最多 60 页。接收到每页后写入 flash，但不立即刷新显示。
    接收方通过发送文本命令 "DISPLAY" (不含引号，结尾以 CR/LF) 来触发一次性显示已接收的所有页。
    也可发送 "RESET_PAGES" 来重置接收页计数。 */
#define MAX_PAGES_SUPPORTED 60
static uint16_t receivedPageCount = 0;

/* 当前目标图像槽位（0..7），由主机通过 "SET_SLOT:<1-8>" 指定。默认0（槽位1） */
static uint8_t currentImageSlot = 0;

/* 最近写入的图像是否为红色通道（true 表示 RED 数据页已被写入） */
static bool lastImageIsRed = false;

/* 红黑合成图像追踪 */
static uint8_t redLayerReceived = 0;    // 0=未收, 1=已收
static uint8_t blackLayerReceived = 0;  // 0=未收, 1=已收

/* Frame magic for new protocol */
#define FRAME_MAGIC_0 0xAB
#define FRAME_MAGIC_1 0xCD
/* 最大允许的单帧有效负载长度（安全上限） */
#define FRAME_MAX_PAYLOAD 1024
/* 静态解压缓冲区，避免栈溢出 */
static uint8_t decompressBuffer[PAGE_SIZE];
/* 注意：不再为 clear 页分配独立静态缓冲（原 clearPageBuffer 被移除），
 * 在需要写入全白/全黑页时复用 decompressBuffer 以节省静态内存。
 */

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

/**
 * @brief RLE 解压缩（就地解压到固定248字节缓冲区）
 * @param compressed 压缩数据
 * @param compLen 压缩数据长度
 * @param output 输出缓冲区（必须至少248字节）
 * @param maxOutLen 输出缓冲区最大长度
 * @return 解压后的实际长度，失败返回0
 */
static size_t decompressPageRLE(const uint8_t *compressed, size_t compLen,
                                uint8_t *output, size_t maxOutLen)
{
    size_t inPos = 0;
    size_t outPos = 0;
		uint8_t count;
		uint8_t value;
		size_t runLength;
		size_t literalLen;

	

    while (inPos < compLen) {
        if (inPos >= compLen) break;

        count = compressed[inPos++];

        if (count >= 128) {
            /* 重复模式：257 - count = 实际重复次数 */
            if (inPos >= compLen) {
                UARTIF_uartPrintf(0, "RLE ERR: missing repeat value at pos %u\r\n", (unsigned)inPos);
                return 0;
            }

            value = compressed[inPos++];
            runLength = 257 - count;

            if (outPos + runLength > maxOutLen) {
                UARTIF_uartPrintf(0, "RLE ERR: buffer overflow, need %u have %u\r\n",
                                  (unsigned)(outPos + runLength), (unsigned)maxOutLen);
                return 0;
            }

            memset(&output[outPos], value, runLength);
            outPos += runLength;
        } else {
            /* 字面量模式 */
            literalLen = count;

            if (inPos + literalLen > compLen) {
                UARTIF_uartPrintf(0, "RLE ERR: insufficient literal data at pos %u\r\n", (unsigned)inPos);
                return 0;
            }
            if (outPos + literalLen > maxOutLen) {
                UARTIF_uartPrintf(0, "RLE ERR: buffer overflow in literal mode\r\n");
                return 0;
            }

            memcpy(&output[outPos], &compressed[inPos], literalLen);
            inPos += literalLen;
            outPos += literalLen;
        }
    }

    return outPos;
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
            //Uart_SendData(UARTCH1, data);

                /* 将字节追加到缓冲区（不再依赖回车） */
                if (bufferIndex < sizeof(buffer) - 1)
                {
                    buffer[bufferIndex++] = (char)data;
                }

                /* 尝试从缓冲区头部解析若干完整帧：
                 * 新帧格式：MAGIC(2B)=0xABCD | FLAGS(1B) | LEN(2B big-endian) | PAYLOAD(len) | CRC(2B)
                 */
                while (bufferIndex >= 7) /* minimal frame header with FLAGS */
                {
                    /* pre-declare variables to satisfy older C compilers */
                    size_t k;
                    uint8_t flags = 0;
                    uint16_t payloadLen = 0;
                    size_t frameTotal = 0;
                    uint16_t sw_calc = 0;
                    uint8_t high = 0;
                    uint8_t low = 0;
                    uint16_t recv_crc = 0;
                    uint8_t isCompressed = 0;
                    size_t copyLen = 0;
                    char tmp[64];  /* 减小到64字节，足够DISPLAY命令 */
                    uint16_t i = 0;
                    uint16_t id = 0;
                    flash_result_t fres = FLASH_OK;
                    size_t finalLen = 0;
                    uint8_t *pData = NULL; /* 指向最终数据的指针 */
                    uint8_t isRed;
                    uint8_t dataMagic;
                    uint8_t clearDataMagic;
                    uint8_t headerMagic;
					uint8_t isRedBlackComposite;
                    uint8_t showMode;
                    /* 查找并对齐到 MAGIC 开头 */
                    if ((uint8_t)buffer[0] != FRAME_MAGIC_0 || (uint8_t)buffer[1] != FRAME_MAGIC_1)
                    {
                        k = 1;
                        for (; k + 1 < bufferIndex; ++k)
                        {
                            if ((uint8_t)buffer[k] == FRAME_MAGIC_0 && (uint8_t)buffer[k+1] == FRAME_MAGIC_1) break;
                        }
                        if (k + 1 >= bufferIndex)
                        {
                            /* 未找到 MAGIC，清空缓冲区以避免无限增长 */
                            bufferIndex = 0;
                            break;
                        }
                        /* 丢弃前 k 字节并继续 */
                        memmove(buffer, &buffer[k], bufferIndex - k);
                        bufferIndex -= k;
                        continue;
                    }

                    /* 至少需要 5 字节以读取 FLAGS + LEN */
                    if (bufferIndex < 5) break;

                    flags = (uint8_t)buffer[2];
                    payloadLen = (((uint8_t)buffer[3]) << 8) | (uint8_t)buffer[4];
                    isCompressed = flags & 0x01;
                    /* flags bit1 (0x02) 用于指示颜色：0=黑色，1=红色 */
                    isRed = (flags & 0x02) ? 1u : 0u;

                    if (payloadLen > FRAME_MAX_PAYLOAD)
                    {
                        /* 非法长度，丢弃首字节重同步 */
                        memmove(buffer, &buffer[1], bufferIndex - 1);
                        bufferIndex -= 1;
                        continue;
                    }

                    frameTotal = 2 + 1 + 2 + (size_t)payloadLen + 2; /* MAGIC+FLAGS+LEN+PAYLOAD+CRC */
                    if (bufferIndex < frameTotal) break; /* 等待更多字节 */

                    /* 计算并比较 CRC（对压缩后的 payload 计算） */
                    sw_calc = crc16_ccitt((uint8_t *)&buffer[5], (uint32_t)payloadLen);
                    high = (uint8_t)buffer[5 + payloadLen];
                    low = (uint8_t)buffer[5 + payloadLen + 1];
                    recv_crc = ((uint16_t)high << 8) | (uint16_t)low;

                    if (sw_calc == recv_crc)
                    {
                        /* CRC 校验通过，处理payload */
                        if (isCompressed)
                        {
                            /* 解压到静态缓冲区 */
                            finalLen = decompressPageRLE((uint8_t *)&buffer[5], payloadLen,
                                                         decompressBuffer, PAGE_SIZE);

                            if (finalLen == 0) {
                                UARTIF_uartPrintf(0, "RLE decompress FAILED\r\n");
                                /* 丢弃此帧 */
                                if (bufferIndex > frameTotal) {
                                    memmove(buffer, &buffer[frameTotal], bufferIndex - frameTotal);
                                }
                                bufferIndex -= frameTotal;
                                continue;
                            }

                            /* RLE OK */
                            pData = decompressBuffer;
                        }
                        else
                        {
                            /* 未压缩，直接使用buffer中的数据 */
                            if (payloadLen > PAGE_SIZE) {
                                UARTIF_uartPrintf(0, "Payload too large: %u > %u\r\n", payloadLen, PAGE_SIZE);
                                if (bufferIndex > frameTotal) {
                                    memmove(buffer, &buffer[frameTotal], bufferIndex - frameTotal);
                                }
                                bufferIndex -= frameTotal;
                                continue;
                            }
                            pData = (uint8_t *)&buffer[5];
                            finalLen = payloadLen;
                        }

                        /* 根据finalLen判断是页数据还是控制命令 */
                        if (finalLen == PAGE_SIZE)
                        {
                            /* 写入Flash（直接写入，不经过testWritePage，因为CRC已在帧层验证） */
                            id = (uint16_t)(receivedPageCount | ((uint16_t)currentImageSlot << 8));
                            /* 若是本张图片的第一包，使用 flags 指定颜色（整张图片同色） */
                            if (receivedPageCount == 0) {
                                /* 恢复为原始逻辑：flags 中 1 表示红色 */
                                lastImageIsRed = (isRed != 0);
                            }
                            dataMagic = lastImageIsRed ? MAGIC_RED_IMAGE_DATA : MAGIC_BW_IMAGE_DATA;
                            /* 数据的颜色（RED/BW）已由发送端通过 flags 指定。
                             * 发送端应负责对 RED 通道做按位取反以匹配设备约定，
                             * 因此此处直接把接收到的 pData 写入 flash，避免在 MCU 栈上分配大数组。
                             */
                            fres = FM_writeData(dataMagic, id, pData, PAGE_SIZE);
                            if (fres == FLASH_OK) {
                                /* Page written OK */
                                /* 颜色已在写入前根据第一包的 flags 处理 */
                                /* 如果这是最后一页（frame == MAX_FRAME_NUM），则视为本张图片接收完成，写入 image header 并清空对侧通道（不触发显示） */
                                if (receivedPageCount == MAX_FRAME_NUM)
                                {
                                    uint8_t isRedBlackComposite = 0;
                                    
                                    /* Image receive complete */
                                    // 追踪接收状态
                                    if (lastImageIsRed) {
                                        redLayerReceived = 1;
                                    } else {
                                        blackLayerReceived = 1;
                                    }
                                    
                                    // 判断是否已收到两层（红黑合成）
                                    isRedBlackComposite = redLayerReceived && blackLayerReceived;
                                    
                                    if (lastImageIsRed) {
                                        /* RED layer complete */
                                        if (!isRedBlackComposite) {
                                            /* RED only mode: 清空 BW 页（写为白色 0xFF），再写入 headers */
                                            /* 写入 RED / BW header 保持向后兼容顺序 */
                                            fres = FM_writeImageHeader(MAGIC_RED_IMAGE_HEADER, currentImageSlot);
                                            if (fres != FLASH_OK) {}
                                            {
                                                uint16_t j;
                                                uint16_t wid;
                                                for (j = 0; j <= MAX_FRAME_NUM; ++j) {
                                                    memset(buffer, 0xFF, PAYLOAD_SIZE);
                                                    wid = (uint16_t)(j | ((uint16_t)currentImageSlot << 8));
                                                    fres = FM_writeData(MAGIC_BW_IMAGE_DATA, wid, buffer, PAYLOAD_SIZE);
                                                    UARTIF_uartPrintf(0, "CLEAR BW page %u sus id=0x%04X\n", j, wid);
                                                    if (fres != FLASH_OK) {
                                                        UARTIF_uartPrintf(0, "CLEAR BW page %u fail id=0x%04X err=%d\n", j, wid, fres);
                                                        break;
                                                    }
                                                }
                                            }
                                            fres = FM_writeImageHeader(MAGIC_BW_IMAGE_HEADER, currentImageSlot);
                                            if (fres != FLASH_OK) {}
                                        } else {
                                            /* Composite: RED waiting for BW */
                                            fres = FM_writeImageHeader(MAGIC_RED_IMAGE_HEADER, currentImageSlot);
                                            if (fres != FLASH_OK) {}
                                        }
                                    } else {
                                        /* BW layer complete */
                                        if (!isRedBlackComposite) {
                                            /* BW only mode: 清空 RED 页（写为白色 0x00），再写入 headers */
                                            
                                            /* 写入 BW / RED header 保持向后兼容顺序 */
                                            fres = FM_writeImageHeader(MAGIC_BW_IMAGE_HEADER, currentImageSlot);
                                            if (fres != FLASH_OK) {}
                                            {
                                                uint16_t j;
                                                uint16_t wid;
                                                for (j = 0; j <= MAX_FRAME_NUM; ++j) {
                                                    memset(buffer, 0x00, PAYLOAD_SIZE);
                                                    wid = (uint16_t)(j | ((uint16_t)currentImageSlot << 8));
                                                    fres = FM_writeData(MAGIC_RED_IMAGE_DATA, wid, buffer, PAYLOAD_SIZE);
                                                    UARTIF_uartPrintf(0, "CLEAR RED page %u sus id=0x%04X\n", j, wid);
                                                    if (fres != FLASH_OK) {
                                                        UARTIF_uartPrintf(0, "CLEAR RED page %u fail id=0x%04X err=%d\n", j, wid, fres);
                                                        break;
                                                    }
                                                }
                                            }
                                            fres = FM_writeImageHeader(MAGIC_RED_IMAGE_HEADER, currentImageSlot);
                                            if (fres != FLASH_OK) {}
                                        } else {
                                            /* Composite: BW complete, write BW header only */
                                            fres = FM_writeImageHeader(MAGIC_BW_IMAGE_HEADER, currentImageSlot);
                                            if (fres != FLASH_OK) {}
                                        }
                                    }
                                    
                                    /* If both layers received, composite image done */
                                    if (isRedBlackComposite) {
                                        /* Composite image complete */
                                    }
                                }
                                else
                                {
                                    /* 继续接收下一页 */
                                    if (receivedPageCount < MAX_FRAME_NUM)
                                    {
                                        receivedPageCount++;
                                    }
                                    else
                                    {
                                        UARTIF_uartPrintf(0, "Reached max pages: %d\r\n", MAX_PAGES_SUPPORTED);
                                    }
                                }
                            } else {
                                UARTIF_uartPrintf(0, "Flash write fail: page %u id=0x%04X err=%d\r\n",
                                                  receivedPageCount, id, fres);
                            }
                        }
                        else
                        {
                            /* 控制命令 */
                            copyLen = (finalLen < sizeof(tmp)-1) ? finalLen : (sizeof(tmp)-1);
                            memcpy(tmp, pData, copyLen);
                            tmp[copyLen] = '\0';
                            UARTIF_uartPrintf(0, "CTRL: '%s'\r\n", tmp);

                            /* 控制帧不再改变颜色，颜色由首包决定以保持简单一致 */

                            if (strcmp(tmp, "DISPLAY") == 0)
                            {
                                UARTIF_uartPrintf(0, "DISPLAY: rendering %d pages\r\n", receivedPageCount);
                                UARTIF_uartPrintf(0, "DEBUG: redLayerReceived=%u, blackLayerReceived=%u, lastImageIsRed=%u\r\n", 
                                                  redLayerReceived, blackLayerReceived, lastImageIsRed);
                                
                                // /* 根据接收的层数来决定显示模式 */
                                // showMode = IMAGE_BW;
                                // if (redLayerReceived && blackLayerReceived) {
                                //     /* 合成图像：同时有红黑两层 */
                                //     showMode = IMAGE_BW_AND_RED;
                                //     UARTIF_uartPrintf(0, "Display mode: IMAGE_BW_AND_RED (Composite)\r\n");
                                // } else if (lastImageIsRed) {
                                //     /* 只有红色层 */
                                //     showMode = IMAGE_BW_AND_RED;
                                //     UARTIF_uartPrintf(0, "Display mode: IMAGE_BW_AND_RED (Red only)\r\n");
                                // } else {
                                //     /* 只有黑白层 */
                                //     UARTIF_uartPrintf(0, "Display mode: IMAGE_BW (Black&White only)\r\n");
                                // }

                                EPD_WhiteScreenGDEY042Z98UsingFlashDate(currentImageSlot);
                                receivedPageCount = 0;
                                
                                /* 显示完成后重置标志，准备下一个图像 */
                                redLayerReceived = 0;
                                blackLayerReceived = 0;
                            }
                            else if (strncmp(tmp, "SET_SLOT:", 9) == 0)
                            {
                                int v = atoi(&tmp[9]);
                                if (v >= 1 && v <= 8)
                                {
                                    currentImageSlot = (uint8_t)(v - 1);
                                    UARTIF_uartPrintf(0, "SET_SLOT -> %d (slotIndex=%u)\r\n", v, currentImageSlot);
                                    /* 重置已接收页计数，准备写入新槽 */
                                    receivedPageCount = 0;
                                }
                                else
                                {
                                    UARTIF_uartPrintf(0, "SET_SLOT invalid: %s\r\n", tmp);
                                }
                            }
                            else if (strcmp(tmp, "RESET_PAGES") == 0)
                            {
                                UARTIF_uartPrintf(0, "RESET_PAGES\r\n");
                                receivedPageCount = 0;
                            }
                        }

                        /* 移除已处理的完整帧并继续解析后续帧 */
                        if (bufferIndex > frameTotal)
                        {
                            memmove(buffer, &buffer[frameTotal], bufferIndex - frameTotal);
                        }
                        bufferIndex -= frameTotal;
                        continue;
                    }
                    else
                    {
                        /* CRC 错误 */
                        UARTIF_uartPrintf(0, "CRC ERR: recv=0x%04X calc=0x%04X\r\n", recv_crc, sw_calc);
                        memmove(buffer, &buffer[1], bufferIndex - 1);
                        bufferIndex -= 1;
                        continue;
                    }
                }
            // }
        }
        data = 0;
    }
}

/**
 * @brief 处理接收到的缓冲区：校验 CRC16-CCITT（高字节在前），并在校验通过时输出调试信息与显示
 */
// static void processReceivedBuffer(void)
// {
//     if (bufferIndex == 0) return;

//     if (bufferIndex >= 2)
//     {
//         /* 最后两个字节为 CRC，高字节在前 */
//         uint8_t high = (uint8_t)buffer[bufferIndex - 2];
//         uint8_t low = (uint8_t)buffer[bufferIndex - 1];
//         uint16_t recv_crc = ((uint16_t)high << 8) | (uint16_t)low;

//         /* 使用软件 CRC16-CCITT 作为主校验（高字节在前）并与硬件结果并列打印以便诊断 */
//         {
//             uint16_t sw_calc = crc16_ccitt((uint8_t *)buffer, (uint32_t)(bufferIndex - 2));

//             /* 专门处理 PAGE_SIZE + 2 的二进制页面帧：若 CRC 匹配则写入 flash（按序），但不立即显示 */
//                     if (bufferIndex == (PAGE_SIZE + 2))
//             {
//                 if (sw_calc == recv_crc)
//                 {
//                     UARTIF_uartPrintf(0, "PAGE FRAME CRC OK: len=%d page=%d\r\n", (int)(bufferIndex - 2), (int)receivedPageCount);
//                     /* 将该页写入 flash，使用 receivedPageCount 作为页索引（从 0 开始） */
//                     /* 传入 IMAGE_RED 表示写入 RED 通道，IMAGE_BW 表示写入 BW 通道。
//                      * 之前使用 IMAGE_BW_AND_RED 导致在写入端与显示端语义混淆。
//                      */
//                     DRAW_testWritePage(lastImageIsRed ? IMAGE_RED : IMAGE_BW, currentImageSlot, receivedPageCount, (const uint8_t *)buffer, (uint32_t)bufferIndex);

//                     /* 计数并限制接收页数上限，避免越界 */
//                     if (receivedPageCount < MAX_FRAME_NUM)
//                     {
//                         receivedPageCount++;
//                     }
//                     else
//                     {
//                         UARTIF_uartPrintf(0, "Reached max supported pages: %d\r\n", MAX_PAGES_SUPPORTED);
//                     }
//                 }
//                 else
//                 {
//                     UARTIF_uartPrintf(0, "PAGE FRAME CRC ERR: recv=0x%04X calc_sw=0x%04X len=%d\r\n", recv_crc, sw_calc, (int)(bufferIndex - 2));
//                     UARTIF_uartPrintf(0, "Recv CRC bytes: %02X %02X\r\n", (uint8_t)buffer[bufferIndex - 2], (uint8_t)buffer[bufferIndex - 1]);
//                 }

//                 /* 不走字符串显示路径，清空缓冲区并返回 */
//                 bufferIndex = 0;
//                 return;
//             }

//             /* 非页面消息（小于 PAGE_SIZE+2）：如果 CRC 校验通过，解析为控制命令或忽略文本显示 */
//                 if (sw_calc == recv_crc)
//             {
//                 /* 确保以 NUL 结束，方便作为字符串处理 */
//                 buffer[bufferIndex - 2] = '\0';
//                 UARTIF_uartPrintf(0, "CRC OK (control/text): len=%d payload='%s'\r\n", (int)(bufferIndex - 2), buffer);

//                 /* 解析简单控制命令：DISPLAY / RESET_PAGES */
//                 if (strcmp(buffer, "DISPLAY") == 0)
//                 {
//                     UARTIF_uartPrintf(0, "DISPLAY command received: rendering %d pages\r\n", (int)receivedPageCount);
//                     /* 在刷新显示前，将未写入的剩余页全部写为 0xFF（白色） */
//                     {
//                         uint16_t i;
//                         uint16_t id;
//                             flash_result_t fres;
//                             uint8_t clearBuf[PAYLOAD_SIZE];
//                             uint8_t clearDataMagic;
//                             uint8_t headerMagic;
//                         memset(clearBuf, 0xFF, PAYLOAD_SIZE);
//                         /* 根据 lastImageIsRed 选择清除类型与 header */
//                         clearDataMagic = lastImageIsRed ? MAGIC_RED_IMAGE_DATA : MAGIC_BW_IMAGE_DATA;
//                         headerMagic = lastImageIsRed ? MAGIC_RED_IMAGE_HEADER : MAGIC_BW_IMAGE_HEADER;
//                         for (i = receivedPageCount; i <= MAX_FRAME_NUM; ++i) {
//                             id = (uint16_t)(i | ((uint16_t)currentImageSlot << 8));
//                             fres = FM_writeData(clearDataMagic, id, clearBuf, PAYLOAD_SIZE);
//                             if (fres != FLASH_OK) {
//                                 UARTIF_uartPrintf(0, "DISPLAY: clear page %u fail id=0x%04X err=%d\r\n", i, id, fres);
//                             }
//                         }

//                         /* 写入 image header 并展示 flash 中的图像 */
//                         {
//                             flash_result_t fres = FM_writeImageHeader(headerMagic, currentImageSlot);
//                             if (fres != FLASH_OK) {
//                                 UARTIF_uartPrintf(0, "DISPLAY: write header fail err=%d\r\n", fres);
//                             }
//                         }
//                         EPD_WhiteScreenGDEY042Z98UsingFlashDate(lastImageIsRed ? IMAGE_BW_AND_RED : IMAGE_BW, currentImageSlot);
//                         /* 显示后重置计数，准备下一次接收 */
//                         receivedPageCount = 0;
//                     }
//                 }
//                 else if (strncmp(buffer, "SET_SLOT:", 9) == 0)
//                 {
//                     int v = atoi(&buffer[9]);
//                     if (v >= 1 && v <= 8)
//                     {
//                         currentImageSlot = (uint8_t)(v - 1);
//                         UARTIF_uartPrintf(0, "SET_SLOT -> %d (slotIndex=%u)\r\n", v, currentImageSlot);
//                         receivedPageCount = 0;
//                     }
//                     else
//                     {
//                         UARTIF_uartPrintf(0, "SET_SLOT invalid: %s\r\n", buffer);
//                     }
//                 }
//                 else if (strcmp(buffer, "RESET_PAGES") == 0)
//                 {
//                     UARTIF_uartPrintf(0, "RESET_PAGES command received: resetting page count\r\n");
//                     receivedPageCount = 0;
//                 }                
//                 else
//                 {
//                     /* 其他文本不再显示到屏幕，仅记录日志 */
//                     UARTIF_uartPrintf(0, "Ignored text payload\r\n");
//                 }
//             }
//             else
//             {
//                 /* 校验失败 - 使用软件计算为准，并打印软件计算值供排查（已移除硬件驱动调用） */
//                 size_t i;
//                 UARTIF_uartPrintf(0, "CRC ERR: recv=0x%04X calc_sw=0x%04X len=%d\r\n", recv_crc, sw_calc, (int)(bufferIndex - 2));
//                 /* 打印负载十六进制 */
//                 UARTIF_uartPrintf(0, "Payload HEX:");
//                 for (i = 0; i < bufferIndex - 2; ++i)
//                 {
//                     UARTIF_uartPrintf(0, " %02X", (uint8_t)buffer[i]);
//                     /* 每16字节换行，便于阅读 */
//                     if (((i + 1) % 16) == 0)
//                     {
//                         UARTIF_uartPrintf(0, "\r\n");
//                     }
//                 }
//                 UARTIF_uartPrintf(0, "\r\n");
//                 /* 打印接收到的 CRC 字节 */
//                 UARTIF_uartPrintf(0, "Recv CRC bytes: %02X %02X\r\n", (uint8_t)buffer[bufferIndex - 2], (uint8_t)buffer[bufferIndex - 1]);
//             }
//         }
//     }
//     else
//     {
//         /* 数据过短，无法校验：记录日志但不在屏幕上直接显示文本 */
//         buffer[bufferIndex] = '\0';
//         UARTIF_uartPrintf(0, "No CRC (short message): len=%d payload='%s'\r\n", (int)bufferIndex, buffer);
//         /* 不进行 DRAW_string、FM_writeImageHeader 或 EPD 刷新，以防止任意文本显示到屏幕 */
//     }

//     /* 重置缓冲区索引，准备接收下一条消息 */
//     bufferIndex = 0;
// }

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


