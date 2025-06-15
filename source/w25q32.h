#ifndef __W25Q32_H__
#define __W25Q32_H__

#include <stdint.h>

/* 返回值宏定义 */
#define W25Q32_OK       0    // 操作成功
#define W25Q32_ERROR    1    // 操作失败

/* 硬件配置 */
#define W25Q32_CS_PIN       GPIO_PIN_4
#define W25Q32_CS_PORT      GPIOA
#define W25Q32_SPI_HANDLE   hspi1  // 修改为实际SPI句柄

/* 指令集 (参考数据手册) */
#define W25Q32_CMD_READ_DATA        0x03
#define W25Q32_CMD_PAGE_PROGRAM     0x02
#define W25Q32_CMD_SECTOR_ERASE     0x20
#define W25Q32_CMD_CHIP_ERASE       0xC7
#define W25Q32_CMD_READ_STATUS_REG1 0x05
#define W25Q32_CMD_WRITE_ENABLE     0x06
#define W25Q32_CMD_WRITE_DISABLE    0x04

#define W25Q32_CMD_JEDEC_ID         0x9F

/* 存储参数 */
#define W25Q32_PAGE_SIZE         256     // 页大小 (字节)
#define W25Q32_SECTOR_SIZE       4096    // 扇区大小 (字节)
#define W25Q32_BLOCK_SIZE        65536   // 块大小 (字节)
#define W25Q32_TOTAL_SIZE        4194304 // 总容量 (4MB)

/* 函数声明 */
uint8_t W25Q32_Init(void);
void W25Q32_CS(uint8_t state);  // 片选控制
uint8_t W25Q32_ReadStatusReg(void);
uint8_t W25Q32_WriteEnable(void);
uint8_t W25Q32_WaitForReady(void);
uint32_t W25Q32_ReadID(void);
uint8_t W25Q32_EraseSector(uint32_t sectorAddr);
uint8_t W25Q32_EraseChip(void);
uint8_t W25Q32_ReadData(uint32_t addr, uint8_t *buf, uint32_t len);
uint8_t W25Q32_WritePage(uint32_t addr, uint8_t *buf, uint16_t len);

#endif
