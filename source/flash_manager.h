#ifndef FLASH_MANAGER_H
#define FLASH_MANAGER_H

#include "base_types.h"
#include "flash_config.h"

// 返回值枚举
typedef enum {
    FLASH_OK = 0,               // 操作成功
    FLASH_ERROR_INVALID_PARAM,  // 无效参数
    FLASH_ERROR_NOT_FOUND,      // 数据未找到
    FLASH_ERROR_NO_SPACE,       // 空间不足
    FLASH_ERROR_CRC_FAIL,       // CRC校验失败
    FLASH_ERROR_WRITE_FAIL,     // 写入失败
    FLASH_ERROR_READ_FAIL,      // 读取失败
    FLASH_ERROR_ERASE_FAIL,     // 擦除失败
    FLASH_ERROR_GC_FAIL,        // 垃圾回收失败
    FLASH_ERROR_INIT_FAIL,      // 初始化失败
    FLASH_ERROR_IMAGE_FRAME_LOST // 图像帧丢失
} flash_result_t;

// typedef enum {
//     FLASH_SCAN_JOB_NONE = 0,
//     FLASH_SCAN_JOB_INIT,
//     FLASH_SCAN_JOB_SCAN_IMAGE_DATA_BW,
//     FLASH_SCAN_JOB_SCAN_IMAGE_DATA_RED
// } flashScanJob_t;
// Segment头结构（14字节）
typedef struct {
    uint8_t headerMagic;       // Segment头魔法数字 区分数据page
    uint8_t segmentId;         // Segment ID (0或1)
    uint32_t statusMagic;      // 状态魔法数字
    uint32_t gcCounter;          // 垃圾回收次数
    uint32_t crc32;            // 头部CRC32校验
} segment_header_t;

// typedef struct {
//     // uint8_t frame_num;       // 帧编号
//     uint8_t blockAddress;     // 对应的block地址
//     uint8_t pageAddress;      // 对应的page地址
// } image_address_t;
// 数据Page结构（256字节）
// typedef struct {
//     uint8_t  magic;            // 魔法数字（区分数据页类型）
//     uint16_t dataId;          // 数据块ID
//     uint8_t  data_size;        // 数据大小（1-247字节）
//     uint32_t crc32;           // 数据CRC32校验
//     uint8_t  data[247];       // 实际数据
// } data_page_t;

// 数据条目映射
// typedef struct {
//     // uint8_t dataId;          // 数据ID
//     uint8_t blockAddress;     // 对应的block地址
//     uint8_t pageAddress;      // 对应的page地址
// } address_t;

// Flash管理器上下文
typedef struct {
    uint8_t activeSegmentBaseStatus;   // 0x00为初始化状态或作为状态；0xAC 表示active_segment_base 为为0x000000，backup_segment_base 为0x200000；0xBD表示相反
    uint8_t  gcInProgress;           // 垃圾回收进行标志
    uint16_t nextWriteAddress;       // 下次写入地址
    // uint16_t data_count;               // 当前数据条目数
    uint32_t currentGcCounter;
    uint16_t dataEntries[MAX_DATA_ENTRIES]; // 数据映射表
    uint16_t imageBwEntries[MAX_IMAGE_ENTRIES]; // 数据映射表
    uint16_t imageRedEntries[MAX_IMAGE_ENTRIES]; // 数据映射表
    //uint8_t imageSlotColor[MAX_IMAGE_ENTRIES]; // 每个槽的颜色标志：0 = BW, 1 = RED, 0xFF = 未知
    segment_header_t header0;
    segment_header_t header1;
    uint16_t* entries[3u]; // 0 - dataEntries, 1 - imageBwEntries, 2 - imageRedEntries
    uint8_t entriesCountMax[3u]; // 0 - MAX_DATA_ENTRIES, 1 - MAX_IMAGE_ENTRIES, 2 - MAX_IMAGE_ENTRIES
} flash_manager_t;

// 函数声明

/**
 * @brief 初始化Flash管理器
 * @param manager Flash管理器指针
 * @return flash_result_t 操作结果
 */
flash_result_t FM_init(void);

/**
 * @brief 写入数据
 * @param dataId 数据ID
 * @param data 数据指针
 * @param size 数据大小（1-247字节）
 * @return flash_result_t 操作结果
 */
flash_result_t FM_writeData(uint8_t magic, uint16_t dataId, const uint8_t* data, uint16_t size);

/**
 * @brief 读取数据
 * @param dataId 数据ID
 * @param data 数据缓冲区指针
 * @param size 输入：缓冲区大小，输出：实际数据大小
 * @return flash_result_t 操作结果
 */
flash_result_t FM_readData(uint8_t magic, uint16_t dataId, uint8_t* data, uint8_t size);

/**
 * @brief 删除数据
 * @param dataId 数据ID
 * @return flash_result_t 操作结果
 */
flash_result_t FM_deleteData(uint16_t dataId);

/**
 * @brief 执行垃圾回收
 * @param manager Flash管理器指针
 * @return flash_result_t 操作结果
 */
// flash_result_t flash_garbage_collect(flash_manager_t* manager);

/**
 * @brief 获取Flash状态信息
 * @param manager Flash管理器指针
 * @param used_pages 输出：已使用的page数量
 * @param free_pages 输出：剩余的page数量
 * @param data_count 输出：数据条目数量
 * @return flash_result_t 操作结果
 */
// flash_result_t flash_get_status(flash_manager_t* manager, uint32_t* used_pages, uint32_t* free_pages, uint32_t* data_count);

/**
 * @brief 强制执行垃圾回收（用于测试）
 * @return flash_result_t 操作结果
 */
flash_result_t FM_forceGarbageCollect(void);

/**
 * @brief 写入图像头页
 * @param magic 魔法数字（区分数据页类型）
 * @param slotId 数据ID
 * @return flash_result_t 操作结果
 */
flash_result_t FM_writeImageHeader(uint8_t magic, uint8_t slotId);

// /**
//  * @brief 获取图像槽位存储的颜色标志
//  * @param slotId 槽位编号
//  * @return 0 = BW, 1 = RED, 0xFF = 未知/未初始化
//  */
// uint8_t FM_getImageSlotColor(uint8_t slotId);

/**
 * @brief 读取图像数据页
 * @param magic 期望的魔法数字
 * @param slotId 数据ID
 * @param frameNum 输入：帧编号
 * @param data 数据缓冲区指针
 * @return flash_result_t 操作结果
 */
flash_result_t FM_readImage(uint8_t magic, uint8_t slotId, uint8_t frameNum, uint8_t* data);

#endif // FLASH_MANAGER_H
