#ifndef FLASH_CONFIG_H
#define FLASH_CONFIG_H

#include "base_types.h"

// Flash芯片基本参数
#define FLASH_BASE_ADDRESS      0x000000    // Flash起始地址
#define FLASH_TOTAL_SIZE        0x400000    // 4MB总容量
#define FLASH_PAGE_SIZE         256         // 页大小
#define FLASH_SECTOR_SIZE       4096        // 扇区大小
#define FLASH_BLOCK_SIZE        65536       // 块大小

// Segment配置
#define FLASH_SEGMENT_COUNT     2           // 两个segment
#define FLASH_SEGMENT_SIZE      (FLASH_TOTAL_SIZE / 2)  // 每个segment 2MB
#define FLASH_SEGMENT0_BASE     FLASH_BASE_ADDRESS      // Segment 0基地址
#define FLASH_SEGMENT1_BASE     (FLASH_BASE_ADDRESS + FLASH_SEGMENT_SIZE)  // Segment 1基地址

// 每个segment的page配置
#define FLASH_PAGES_PER_SEGMENT (FLASH_SEGMENT_SIZE / FLASH_PAGE_SIZE)  // 每个segment的page数量：8192
#define FLASH_DATA_PAGES_PER_SEGMENT (FLASH_PAGES_PER_SEGMENT - 1)     // 数据page数量：8191（除去header page）

// 数据管理配置
#define MAX_DATA_ENTRIES        8         // 最大数据条目数
#define INVALID_DATA_ID         0xFFFFFFFF  // 无效数据ID
#define INVALID_ADDRESS         0xFFFFFFFF  // 无效地址

// 状态魔法数字定义
#define SEGMENT_MAGIC_ACTIVE    0x12345678  // 激活状态
#define SEGMENT_MAGIC_BACKUP    0x87654321  // 备用状态
#define SEGMENT_MAGIC_GC        0xAAAABBBB  // 垃圾回收进行中
#define SEGMENT_MAGIC_ERASED    0xFFFFFFFF  // 未初始化/已擦除

// CRC32多项式
#define CRC32_POLYNOMIAL        0xEDB88320

#endif // FLASH_CONFIG_H