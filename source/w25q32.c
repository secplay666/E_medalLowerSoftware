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
 ** @file w25q32.c
 **
 ** @brief Source file for w25q32 driver
 **
 ** @author MADS Team 
 **
 ******************************************************************************/

/******************************************************************************
 * Include files
 ******************************************************************************/
#include "gpio.h"
#include "spi.h"
#include "ddl.h"
#include "w25q32.h"

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

/******************************************************************************
 * Local pre-processor symbols/macros ('#define')                             
 ******************************************************************************/

/*****************************************************************************
 * Function implementation - global ('extern') and local ('static')
 ******************************************************************************/
/* 片选控制函数 */
void W25Q32_CS(uint8_t state) 
{
    Gpio_SetIO(1, 4, state); //DC输出高
}

/* 初始化SPI接口 */
uint8_t W25Q32_Init(void) 
{
	uint32_t id = 0;
    Gpio_InitIO(1, 4, GpioDirOut);
    Gpio_SetIO(1, 4, 1);               //RST输出高
    
    // 检查Flash是否正常响应
     id = W25Q32_ReadID();
    if (id == 0x00000000 || id == 0xFFFFFFFF) {
        return W25Q32_ERROR;  // Flash未正常响应
    }
    
    return W25Q32_OK;
}

/* 读取状态寄存器1 (BUSY位在bit0) */
uint8_t W25Q32_ReadStatusReg(void)
{
    uint8_t status;
    
    W25Q32_CS(0);
    Spi_SendData(W25Q32_CMD_READ_STATUS_REG1);
    status = Spi_ReceiveData();
    W25Q32_CS(1);
    
    return status;
}

/* 等待Flash就绪 (检查BUSY位) */
uint8_t W25Q32_WaitForReady(void) 
{
    uint32_t timeout = 10000;  // 超时计数器
    
    while ((W25Q32_ReadStatusReg() & 0x01) && timeout > 0) {  // BIT0=1表示忙
        delay100us(1);
        timeout--;
    }
    
    if (timeout == 0) {
        return W25Q32_ERROR;  // 超时错误
    }
    
    return W25Q32_OK;
}

/* 写使能命令 (必须在前置擦除/编程操作前调用) */
uint8_t W25Q32_WriteEnable(void) 
{
	uint8_t status = 0;
    W25Q32_CS(0);
    Spi_SendData(W25Q32_CMD_WRITE_ENABLE);
    W25Q32_CS(1);
    
    // 检查写使能位是否设置成功
     status = W25Q32_ReadStatusReg();
    if ((status & 0x02) == 0) {  // BIT1=0表示写使能失败
        return W25Q32_ERROR;
    }
    
    return W25Q32_OK;
}

void W25Q32_WriteDisable(void) 
{
    W25Q32_CS(0);
    Spi_SendData(W25Q32_CMD_WRITE_DISABLE);
    W25Q32_CS(1);
}

/* 读取JEDEC ID (制造商+设备ID) */
uint32_t W25Q32_ReadID(void) 
{
    uint8_t idBuf[3];
    
    W25Q32_CS(0);
    Spi_SendData(W25Q32_CMD_JEDEC_ID);

    idBuf[2] = Spi_ReceiveData();
    idBuf[1] = Spi_ReceiveData();
    idBuf[0] = Spi_ReceiveData();

    W25Q32_CS(1);
    
    return (idBuf[0] << 16) | (idBuf[1] << 8) | idBuf[2];
}

/* 扇区擦除 (4KB) */
uint8_t W25Q32_EraseSector(uint32_t sectorAddr) 
{    
    // 检查地址有效性
    if (sectorAddr >= W25Q32_TOTAL_SIZE) {
        return W25Q32_ERROR;
    }

    // 使能写操作
    if (W25Q32_WriteEnable() != W25Q32_OK) {
        return W25Q32_ERROR;
    }
    
    W25Q32_CS(0);
    Spi_SendData(W25Q32_CMD_SECTOR_ERASE);
    Spi_SendData((uint8_t)((sectorAddr >> 16) & 0xFF));
    Spi_SendData((uint8_t)((sectorAddr >> 8) & 0xFF));
    Spi_SendData((uint8_t)(sectorAddr & 0xFF));
    W25Q32_CS(1);
    
    // 等待擦除完成
    if (W25Q32_WaitForReady() != W25Q32_OK) {
        return W25Q32_ERROR;
    }
    
    return W25Q32_OK;
}

/* 整片擦除 */
uint8_t W25Q32_EraseChip(void) 
{
    // 使能写操作
    if (W25Q32_WriteEnable() != W25Q32_OK) {
        return W25Q32_ERROR;
    }
    
    W25Q32_CS(0);
    Spi_SendData(W25Q32_CMD_CHIP_ERASE);
    W25Q32_CS(1);
    
    // 等待擦除完成（时间较长）
    if (W25Q32_WaitForReady() != W25Q32_OK) {
        return W25Q32_ERROR;
    }
    
    return W25Q32_OK;
}

/* 读取数据 (支持跨页连续读) */
uint8_t W25Q32_ReadData(uint32_t addr, uint8_t *buf, uint32_t len) 
{
    uint32_t i = 0;    
    
    // 参数检查
    if (buf == NULL || len == 0) {
        return W25Q32_ERROR;
    }
    
    // 地址范围检查
    if (addr >= W25Q32_TOTAL_SIZE || (addr + len) > W25Q32_TOTAL_SIZE) {
        return W25Q32_ERROR;
    }

    W25Q32_CS(0);
    Spi_SendData(W25Q32_CMD_READ_DATA);
    Spi_SendData((uint8_t)((addr >> 16) & 0xFF));
    Spi_SendData((uint8_t)((addr >> 8) & 0xFF));
    Spi_SendData((uint8_t)(addr & 0xFF));

    for (i = 0; i < len; i++) {
        *(buf + i) = Spi_ReceiveData();
    }
    W25Q32_CS(1);
    
    return W25Q32_OK;
}

/* 写入数据 (页编程，单次最大256字节) */
uint8_t W25Q32_WritePage(uint32_t addr, uint8_t *buf, uint16_t len) 
{
    uint32_t i = 0;
    
    // 参数检查
    if (buf == NULL || len == 0) {
        return W25Q32_ERROR;
    }
    
    // 地址范围检查
    if (addr >= W25Q32_TOTAL_SIZE) {
        return W25Q32_ERROR;
    }
    
    // 长度不能超过页边界
    if (len > W25Q32_PAGE_SIZE) {
        return W25Q32_ERROR;
    }
    
    // 检查是否跨页
    if ((addr & 0xFF) + len > W25Q32_PAGE_SIZE) {
        return W25Q32_ERROR;
    }

    // 使能写操作
    if (W25Q32_WriteEnable() != W25Q32_OK) {
        return W25Q32_ERROR;
    }
    
    W25Q32_CS(0);
    Spi_SendData(W25Q32_CMD_PAGE_PROGRAM);
    Spi_SendData((uint8_t)((addr >> 16) & 0xFF));
    Spi_SendData((uint8_t)((addr >> 8) & 0xFF));
    Spi_SendData((uint8_t)(addr & 0xFF));
    
    for (i = 0; i < len; i++) {
        Spi_SendData(*(buf + i));
    }
    
    W25Q32_CS(1);
    
    // 等待写入完成
    if (W25Q32_WaitForReady() != W25Q32_OK) {
        return W25Q32_ERROR;
    }

    W25Q32_WriteDisable();
    return W25Q32_OK;
}

/******************************************************************************
 * EOF (not truncated)
 ******************************************************************************/


