# Flash管理器模块

## 概述

本Flash管理器模块为W25Q32 Flash芯片提供了一个轻量级的磨损均衡和数据管理解决方案。该模块采用双segment循环存储架构，以page为单位进行数据管理，有效延长Flash芯片的使用寿命。

## 主要特性

### 1. 双Segment架构
- 将4MB Flash分为两个2MB的segment
- 一个segment作为激活区域，另一个作为备用区域
- 当激活segment空间不足时，自动执行垃圾回收

### 2. 磨损均衡
- 采用循环写入策略，避免频繁写入同一区域
- 垃圾回收时整个segment轮换，实现磨损均衡
- Segment header只在切换时写入，减少磨损

### 3. 数据完整性
- 每个数据page都有CRC32校验
- Segment header也有CRC32保护
- 支持意外断电恢复机制

### 4. 资源优化
- 内存占用约4.3KB（包含512个数据条目映射表）
- 垃圾回收只需256字节临时缓冲区
- 代码空间约3-4KB

## 文件结构

```
flash_config.h      - 配置文件，定义Flash参数
flash_manager.h     - 头文件，定义数据结构和API
flash_manager.c     - 主实现文件
flash_test.h        - 测试函数声明
flash_test.c        - 测试代码实现
```

## 数据结构

### Segment Header (256字节)
```c
typedef struct {
    uint32_t status_magic;      // 状态魔法数字
    uint32_t segment_id;        // Segment ID (0或1)
    uint32_t crc32;            // 头部CRC32校验
    uint8_t  padding[244];      // 填充区域
} segment_header_t;
```

### 数据Page (256字节)
```c
typedef struct {
    uint32_t data_id;          // 数据块ID
    uint32_t data_size;        // 数据大小（1-240字节）
    uint32_t crc32;           // 数据CRC32校验
    uint8_t  reserved[4];      // 保留字段
    uint8_t  data[240];       // 实际数据
} data_page_t;
```

## API接口

### 初始化
```c
flash_result_t flash_manager_init(flash_manager_t* manager);
```
- 扫描Flash，确定激活和备用segment
- 构建内存映射表
- 处理意外断电恢复

### 数据操作
```c
// 写入数据
flash_result_t flash_write_data(flash_manager_t* manager, uint32_t data_id, 
                               const uint8_t* data, uint32_t size);

// 读取数据
flash_result_t flash_read_data(flash_manager_t* manager, uint32_t data_id, 
                              uint8_t* data, uint32_t* size);

// 删除数据
flash_result_t flash_delete_data(flash_manager_t* manager, uint32_t data_id);
```

### 状态查询
```c
flash_result_t flash_get_status(flash_manager_t* manager, uint32_t* used_pages, 
                               uint32_t* free_pages, uint32_t* data_count);
```

### 垃圾回收
```c
// 自动垃圾回收（空间不足时自动触发）
flash_result_t flash_garbage_collect(flash_manager_t* manager);

// 强制垃圾回收（用于测试）
flash_result_t flash_force_garbage_collect(flash_manager_t* manager);
```

## 使用示例

```c
#include "flash_manager.h"

flash_manager_t g_flash_manager;

int main(void)
{
    // 初始化W25Q32驱动
    W25Q32_Init();
    
    // 初始化Flash管理器
    flash_result_t result = flash_manager_init(&g_flash_manager);
    if (result != FLASH_OK) {
        // 处理初始化错误
        return -1;
    }
    
    // 写入数据
    uint8_t data[] = "Hello World!";
    result = flash_write_data(&g_flash_manager, 0x1001, data, sizeof(data));
    
    // 读取数据
    uint8_t buffer[64];
    uint32_t size = sizeof(buffer);
    result = flash_read_data(&g_flash_manager, 0x1001, buffer, &size);
    
    // 获取状态
    uint32_t used, free, count;
    flash_get_status(&g_flash_manager, &used, &free, &count);
    
    return 0;
}
```

## 配置参数

在`flash_config.h`中可以调整以下参数：

- `MAX_DATA_ENTRIES`: 最大数据条目数（默认512）
- `FLASH_SEGMENT_SIZE`: Segment大小（默认2MB）
- `FLASH_PAGE_SIZE`: Page大小（固定256字节）

## 性能特性

- **写入性能**: O(1) - 顺序写入
- **读取性能**: O(log n) - 内存映射表查找
- **空间利用率**: 约98%（除去header和管理开销）
- **磨损均衡**: 整个Flash芯片均匀磨损

## 错误处理

所有API函数返回`flash_result_t`枚举值：

- `FLASH_OK`: 操作成功
- `FLASH_ERROR_INVALID_PARAM`: 无效参数
- `FLASH_ERROR_NOT_FOUND`: 数据未找到
- `FLASH_ERROR_NO_SPACE`: 空间不足
- `FLASH_ERROR_CRC_FAIL`: CRC校验失败
- `FLASH_ERROR_WRITE_FAIL`: 写入失败
- `FLASH_ERROR_READ_FAIL`: 读取失败
- `FLASH_ERROR_ERASE_FAIL`: 擦除失败
- `FLASH_ERROR_GC_FAIL`: 垃圾回收失败
- `FLASH_ERROR_INIT_FAIL`: 初始化失败

## 注意事项

1. **数据ID**: 不能使用0xFFFFFFFF作为数据ID
2. **数据大小**: 单个数据块最大240字节
3. **内存需求**: 需要约4.3KB RAM空间
4. **断电保护**: 支持意外断电恢复，但建议在关键操作前确保电源稳定
5. **并发访问**: 当前版本不支持多线程并发访问

## 测试

使用`flash_test.c`中的测试函数验证功能：

```c
// 基本功能测试
flash_manager_test();

// 垃圾回收测试
flash_gc_test();
```

测试覆盖：
- 基本读写操作
- 数据覆盖和删除
- 批量操作
- 垃圾回收功能
- 数据完整性验证