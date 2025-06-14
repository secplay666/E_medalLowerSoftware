#ifndef FLASH_TEST_H
#define FLASH_TEST_H

#include "base_types.h"

/**
 * @brief Flash管理器功能测试
 * 测试基本的读写、覆盖、删除和批量操作功能
 */
void flash_manager_test(void);

/**
 * @brief 垃圾回收测试
 * 测试垃圾回收功能和数据完整性
 */
void flash_gc_test(void);

#endif // FLASH_TEST_H