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
    FLASH_ERROR_INIT_FAIL       // 初始化失败
} flash_result_t;

// Segment头结构（14字节）
typedef struct {
    uint8_t header_magic;       // Segment头魔法数字 区分数据page
    uint8_t segment_id;         // Segment ID (0或1)
    uint32_t status_magic;      // 状态魔法数字
    uint32_t gc_count;          // 垃圾回收次数
    uint32_t crc32;            // 头部CRC32校验
} segment_header_t;

// 数据Page结构（256字节）
typedef struct {
    uint16_t data_id;          // 数据块ID
    uint8_t  data_size;        // 数据大小（1-240字节）
    uint8_t  reserved;         // 保留字段
    uint32_t crc32;           // 数据CRC32校验
    uint8_t  data[248];       // 实际数据
} data_page_t;

// 数据条目映射
typedef struct {
    uint16_t data_id;          // 数据ID
    uint32_t page_address;     // 对应的page地址
} data_entry_t;

// Flash管理器上下文
typedef struct {
    uint32_t active_segment_base;      // 激活segment基地址
    uint32_t backup_segment_base;      // 备用segment基地址
    uint32_t next_write_address;       // 下次写入地址
    uint16_t data_count;               // 当前数据条目数
    uint8_t  gc_in_progress;           // 垃圾回收进行标志
    uint8_t  reserved;                 // 保留字段
    data_entry_t data_entries[MAX_DATA_ENTRIES]; // 数据映射表
} flash_manager_t;

// 函数声明

/**
 * @brief 初始化Flash管理器
 * @param manager Flash管理器指针
 * @return flash_result_t 操作结果
 */
flash_result_t flash_manager_init(flash_manager_t* manager);

/**
 * @brief 写入数据
 * @param manager Flash管理器指针
 * @param data_id 数据ID
 * @param data 数据指针
 * @param size 数据大小（1-248字节）
 * @return flash_result_t 操作结果
 */
flash_result_t flash_write_data(flash_manager_t* manager, uint16_t data_id, const uint8_t* data, uint8_t size);

/**
 * @brief 读取数据
 * @param manager Flash管理器指针
 * @param data_id 数据ID
 * @param data 数据缓冲区指针
 * @param size 输入：缓冲区大小，输出：实际数据大小
 * @return flash_result_t 操作结果
 */
flash_result_t flash_read_data(flash_manager_t* manager, uint16_t data_id, uint8_t* data, uint8_t* size);

/**
 * @brief 删除数据
 * @param manager Flash管理器指针
 * @param data_id 数据ID
 * @return flash_result_t 操作结果
 */
flash_result_t flash_delete_data(flash_manager_t* manager, uint16_t data_id);

/**
 * @brief 执行垃圾回收
 * @param manager Flash管理器指针
 * @return flash_result_t 操作结果
 */
flash_result_t flash_garbage_collect(flash_manager_t* manager);

/**
 * @brief 获取Flash状态信息
 * @param manager Flash管理器指针
 * @param used_pages 输出：已使用的page数量
 * @param free_pages 输出：剩余的page数量
 * @param data_count 输出：数据条目数量
 * @return flash_result_t 操作结果
 */
flash_result_t flash_get_status(flash_manager_t* manager, uint32_t* used_pages, uint32_t* free_pages, uint32_t* data_count);

/**
 * @brief 强制执行垃圾回收（用于测试）
 * @param manager Flash管理器指针
 * @return flash_result_t 操作结果
 */
flash_result_t flash_force_garbage_collect(flash_manager_t* manager);

#endif // FLASH_MANAGER_H
