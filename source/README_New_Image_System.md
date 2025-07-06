# 新图像传输系统说明文档

## 系统概述

本系统实现了一个完整的图像传输和存储方案，支持从上位机通过串口传输图像数据到下位机，并存储到Flash中，最后显示到EPD屏幕上。系统采用了全新的协议设计和存储结构，支持黑白和红白双色图像。

### 主要特性

- **双色图像支持**: 支持黑白和红白两种颜色的图像数据
- **分页存储**: 图像数据分页存储，支持高效的随机访问
- **头页索引**: 使用头页记录数据页地址，便于快速检索
- **CRC32校验**: 完整的数据完整性校验
- **协议化传输**: 定义完整的传输协议，支持错误重传
- **槽位管理**: 支持多个图像槽位，便于管理多张图像

## 系统架构

### 核心模块

1. **图像传输管理器** (`image_transfer_manager`)
   - 处理上位机传输协议
   - 管理数据接收和存储
   - 处理错误重传机制

2. **图像显示器** (`image_display_new`)
   - 从Flash读取图像数据
   - 重组完整图像
   - 显示到EPD屏幕

3. **Flash管理器** (`flash_manager`)
   - 底层Flash存储管理
   - 数据页分配和回收
   - 存储空间管理

4. **协议定义** (`image_protocol`)
   - 定义传输协议格式
   - 定义存储数据结构
   - 定义常量和枚举

## 存储结构设计

### 图像参数

- **图像尺寸**: 400×300像素
- **图像大小**: 15000字节 (400×300÷8)
- **每页数据量**: 248字节
- **总页数**: 61页 (前60页248字节，第61页120字节)

### 数据页格式

#### 1. 普通数据页
```
byte0:     魔法数字 0xA5
byte1-2:   数据块ID
byte3:     数据大小
byte4-7:   CRC32校验
byte8-255: 数据内容 (248字节)
```

#### 2. 黑白图像数据页
```
byte0:     魔法数字 0xA6
byte1:     帧数据序列ID (1-61)
byte2-3:   头数据块ID
byte4-7:   CRC32校验
byte8-255: 图像数据 (248字节)
```

#### 3. 红白图像数据页
```
byte0:     魔法数字 0xA7
byte1:     帧数据序列ID (1-61)
byte2-3:   头数据块ID
byte4-7:   CRC32校验
byte8-255: 图像数据 (248字节)
```

#### 4. 图像头页 (黑白/红白通用)
```
byte0:     魔法数字 0xA8
byte1-2:   数据块ID
byte3:     保留字节
byte4-7:   CRC32校验
byte8-251: 地址条目 (61个条目，每个4字节)
           每个条目格式:
           - byte0: 帧序列ID
           - byte1-3: 数据页绝对地址
byte252-253: 伙伴头页ID (黑白↔红白)
byte254-255: 保留字节
```

## 传输协议设计

### 协议常量

- **上位机魔法数**: 0xA5A5
- **下位机魔法数**: 0x5A5A
- **上位机结束标志**: 0xA5A5AFAF
- **下位机结束标志**: 0x5A5A5F5F

### 传输流程

#### 1. 首帧传输

**上位机发送首帧**:
```
byte0-1:   魔法数 0xA5A5
byte2:     命令 0xC0 (图像传输)
byte3:     槽位(低4位) + 颜色(高4位)
byte4-7:   结束魔法数 0xA5A5AFAF
```

**下位机回复首帧**:
```
byte0-1:   魔法数 0x5A5A
byte2-3:   复制上位机的命令和槽位
byte4:     状态 (1=成功, 2=忙碌, 0xFF=错误)
byte5:     保留字节
byte6-9:   结束魔法数 0x5A5A5F5F
```

#### 2. 数据帧传输

每页数据分5帧传输:
- **前4帧**: 每帧54字节数据
- **第5帧**: 32字节数据 + 4字节CRC32

**数据帧格式**:
```
byte0-1:   魔法数 0xA5A5
byte2:     命令 0xD0 (数据传输)
byte3:     槽位(低4位) + 颜色(高4位)
byte4:     页序列 (1-61)
byte5:     帧序列 (1-5)
byte6-59:  数据 (54字节或更少)
byte60-63: 结束魔法数 0xA5A5AFAF
```

**下位机回复数据帧**:
```
byte0-1:   魔法数 0x5A5A
byte2-3:   复制上位机的命令和槽位
byte4:     页序列
byte5:     状态:
           - 0x00: 成功
           - 0x10: CRC错误，全部重传
           - 0x2x: 缺失第x帧，重传第x帧
           - 0x30: 超时，全部重传
```

#### 3. 尾帧传输

**上位机发送尾帧**:
```
byte0-1:   魔法数 0xA5A5
byte2-3:   命令 0xC1 (传输结束)
byte4-7:   结束魔法数 0xA5A5AFAF
```

**下位机回复尾帧**:
```
byte0-1:   魔法数 0x5A5A
byte2-3:   命令 0xC1 (传输结束)
byte4-7:   结束魔法数 0x5A5A5F5F
```

## 使用方法

### 下位机使用

1. **初始化系统**:
```c
// 初始化Flash管理器
flash_manager_init(&g_flash_manager);

// 初始化图像传输管理器
image_transfer_init(&g_image_transfer_manager, &g_flash_manager, &lpUartRecdata);

// 初始化图像显示器
image_display_new_init(&g_image_display, &g_flash_manager);
```

2. **主循环处理**:
```c
while(1) {
    // 处理图像传输
    image_transfer_process(&g_image_transfer_manager);
    
    // 检查传输完成
    if (g_image_transfer_manager.context.state == IMG_TRANSFER_COMPLETE) {
        // 自动显示图像
        image_display_new_show(&g_image_display, slot);
        image_transfer_reset(&g_image_transfer_manager);
    }
    
    // 处理图像显示
    image_display_new_process(&g_image_display);
}
```

### 上位机使用

1. **安装依赖**:
```bash
pip install pyserial
```

2. **运行测试程序**:
```bash
python test_image_sender.py
```

3. **配置串口**:
```python
# 修改test_image_sender.py中的串口配置
PORT = 'COM3'  # Windows
# PORT = '/dev/ttyUSB0'  # Linux
BAUDRATE = 115200
```

## 性能参数

### 传输性能

- **图像大小**: 15000字节
- **总页数**: 122页 (61页黑白 + 61页红白)
- **每页帧数**: 5帧
- **总帧数**: 610帧
- **传输时间**: 约30-60秒 (取决于串口速度和重传次数)

### 存储性能

- **Flash页大小**: 256字节
- **数据页利用率**: 96.9% (248/256)
- **头页开销**: 2页 (黑白头页 + 红白头页)
- **总存储开销**: 124页 (122数据页 + 2头页)

### 内存使用

- **协议缓冲区**: 64字节
- **Flash缓冲区**: 256字节 (复用)
- **图像缓冲区**: 15000字节 × 2 (显示时临时分配)

## 错误处理

### 传输错误

1. **CRC错误**: 自动重传整页数据
2. **帧丢失**: 重传指定帧
3. **超时错误**: 重传整页数据
4. **协议错误**: 重置传输状态

### 存储错误

1. **Flash写入失败**: 返回错误码，上位机重传
2. **Flash读取失败**: 显示错误，重置显示器
3. **CRC校验失败**: 标记数据损坏，请求重传

### 显示错误

1. **图像数据缺失**: 显示错误信息
2. **EPD通信失败**: 重试显示操作
3. **内存不足**: 分段加载图像数据

## 扩展功能

### 1. 图像压缩

可以在传输前对图像数据进行压缩，减少传输时间:

```c
// 添加压缩函数
uint32_t compress_image_data(const uint8_t* input, uint8_t* output, uint32_t input_size);
uint32_t decompress_image_data(const uint8_t* input, uint8_t* output, uint32_t input_size);
```

### 2. 多槽位管理

支持存储和管理多张图像:

```c
// 槽位管理函数
flash_result_t list_image_slots(uint8_t* slot_list, uint8_t* slot_count);
flash_result_t delete_image_slot(uint8_t slot);
flash_result_t get_slot_info(uint8_t slot, image_slot_info_t* info);
```

### 3. 图像预览

支持传输小尺寸预览图:

```c
// 预览图传输
flash_result_t send_preview_image(uint8_t slot, const uint8_t* preview_data, uint16_t size);
flash_result_t load_preview_image(uint8_t slot, uint8_t* preview_data, uint16_t* size);
```

## 故障排除

### 常见问题

1. **传输中断**:
   - 检查串口连接
   - 检查波特率设置
   - 检查队列大小配置

2. **CRC错误频繁**:
   - 检查串口数据完整性
   - 降低传输速度
   - 检查电磁干扰

3. **Flash写入失败**:
   - 检查Flash芯片连接
   - 检查Flash芯片容量
   - 执行Flash芯片测试

4. **显示异常**:
   - 检查EPD连接
   - 检查图像数据格式
   - 检查显示缓冲区大小

### 调试方法

1. **启用调试输出**:
```c
#define DEBUG_IMAGE_TRANSFER 1
#define DEBUG_IMAGE_DISPLAY 1
```

2. **使用串口监控**:
```bash
# Linux
screen /dev/ttyUSB0 115200

# Windows
# 使用串口调试助手
```

3. **Flash状态检查**:
```c
flash_get_status(&g_flash_manager, &used_pages, &free_pages, &data_count);
UARTIF_uartPrintf(0, "Flash Status: Used=%d, Free=%d, Data=%d\n", 
                 used_pages, free_pages, data_count);
```

## 代码结构

```
source/
├── image_protocol.h          # 协议定义
├── image_transfer_manager.h  # 传输管理器头文件
├── image_transfer_manager.c  # 传输管理器实现
├── image_display_new.h       # 显示器头文件
├── image_display_new.c       # 显示器实现
├── test_image_sender.py      # 上位机测试程序
├── main.c                    # 主程序 (已修改)
├── queue.h                   # 队列定义 (已修改)
└── README_New_Image_System.md # 本文档
```

## 版本信息

- **版本**: 2.0.0
- **发布日期**: 2024年
- **兼容性**: 基于原有Flash管理器和UART接口
- **依赖**: HC32L110 MCU, W25Q32 Flash, EPD显示屏

## 技术支持

如有问题或建议，请参考以下资源:

1. **代码注释**: 详细的函数和结构体注释
2. **调试输出**: 完整的运行时状态信息
3. **错误码**: 详细的错误码定义和说明
4. **测试程序**: 完整的上位机测试程序

---

**注意**: 本系统为嵌入式图像传输的完整解决方案，在实际使用中请根据具体硬件配置调整相关参数。