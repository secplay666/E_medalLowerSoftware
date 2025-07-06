#ifndef IMAGE_PROTOCOL_H
#define IMAGE_PROTOCOL_H

#include "base_types.h"
#include "flash_config.h"

// 图像相关常量定义
#define IMAGE_WIDTH                 400
#define IMAGE_HEIGHT                300
#define IMAGE_BW_BYTES              (IMAGE_WIDTH * IMAGE_HEIGHT / 8)  // 15000字节
#define IMAGE_RED_BYTES             (IMAGE_WIDTH * IMAGE_HEIGHT / 8)  // 15000字节
#define IMAGE_TOTAL_BYTES           (IMAGE_BW_BYTES + IMAGE_RED_BYTES) // 30000字节

// 图像数据页相关常量
#define IMAGE_DATA_PER_PAGE         248     // 每页实际数据字节数
#define IMAGE_PAGES_PER_COLOR       61      // 每种颜色需要的页数
#define IMAGE_LAST_PAGE_DATA_SIZE   120     // 最后一页的数据大小
#define IMAGE_HEADER_ENTRIES        61      // 头页中的地址条目数

// 魔法数字定义
#define MAGIC_NORMAL_DATA           0xA5    // 普通数据页
#define MAGIC_BW_IMAGE_DATA         0xA6    // 黑白图像数据页
#define MAGIC_RED_IMAGE_DATA        0xA7    // 红白图像数据页
#define MAGIC_IMAGE_HEADER          0xA8    // 图像头页

// 传输协议魔法数字
#define PROTOCOL_MAGIC_HOST         0xA5A5  // 上位机发送魔法数
#define PROTOCOL_MAGIC_MCU          0x5A5A  // MCU回复魔法数
#define PROTOCOL_END_HOST           0xA5A5AFAF  // 上位机结束魔法数
#define PROTOCOL_END_MCU            0x5A5A5F5F  // MCU结束魔法数

// 命令类型定义
#define CMD_IMAGE_TRANSFER          0xC0    // 图像传输命令
#define CMD_IMAGE_DATA              0xD0    // 图像数据命令
#define CMD_TRANSFER_END            0xC1    // 传输结束命令

// 颜色类型定义
#define COLOR_TYPE_BW               0x00    // 黑白
#define COLOR_TYPE_RED              0x10    // 红白

// MCU回复状态
#define MCU_STATUS_OK               0x01    // 确认无误
#define MCU_STATUS_BUSY             0x02    // 忙碌
#define MCU_STATUS_ERROR            0xFF    // 错误

// 数据帧回复状态
#define DATA_STATUS_OK              0x00    // 无错误
#define DATA_STATUS_CRC_ERROR       0x10    // CRC错误，全部重传
#define DATA_STATUS_FRAME_MISSING   0x20    // 丢失帧，低4位表示帧号
#define DATA_STATUS_TIMEOUT         0x30    // 超时中断

// 传输帧大小定义
#define FRAME_DATA_SIZE             54      // 前4帧每帧数据大小
#define FRAME_LAST_DATA_SIZE        32      // 第5帧数据大小
#define FRAMES_PER_PAGE             5       // 每页传输帧数

// 普通数据页结构（256字节）
typedef struct {
    uint8_t magic;              // 魔法数字 0xA5
    uint16_t data_id;           // 数据块ID
    uint8_t size;               // 数据大小
    uint32_t crc32;             // CRC32校验
    uint8_t data[248];          // 数据内容
} __attribute__((packed)) normal_data_page_t;

// 黑白图像数据页结构（256字节）
typedef struct {
    uint8_t magic;              // 魔法数字 0xA6
    uint8_t frame_seq_id;       // 帧数据序列ID (1-61)
    uint16_t header_id;         // 头数据块ID
    uint32_t crc32;             // CRC32校验
    uint8_t data[248];          // 图像数据
} __attribute__((packed)) bw_image_data_page_t;

// 红白图像数据页结构（256字节）
typedef struct {
    uint8_t magic;              // 魔法数字 0xA7
    uint8_t frame_seq_id;       // 帧数据序列ID (1-61)
    uint16_t header_id;         // 头数据块ID
    uint32_t crc32;             // CRC32校验
    uint8_t data[248];          // 图像数据
} __attribute__((packed)) red_image_data_page_t;

// 图像头页地址条目（4字节）
typedef struct {
    uint8_t frame_seq_id;       // 帧内数据序列ID
    uint8_t address[3];         // 24位绝对地址
} __attribute__((packed)) image_address_entry_t;

// 图像头页结构（256字节）
typedef struct {
    uint8_t magic;              // 魔法数字 0xA8
    uint16_t data_id;           // 数据块ID
    uint8_t reserved1;          // 保留字段
    uint32_t crc32;             // CRC32校验
    image_address_entry_t entries[61];  // 61个地址条目 (244字节)
    uint16_t partner_header_id; // 对应的另一种颜色头页ID
    uint16_t reserved2;         // 保留字段
} __attribute__((packed)) image_header_page_t;

// 传输协议帧结构

// 首帧结构（8字节）
typedef struct {
    uint16_t magic;             // 魔法数 0xA5A5
    uint8_t command;            // 命令 0xC0
    uint8_t slot_color;         // 低4位：slot(0-F), 高4位：颜色(0/1)
    uint32_t end_magic;         // 结束魔法数 0xA5A5AFAF
} __attribute__((packed)) start_frame_t;

// 首帧回复结构（10字节）
typedef struct {
    uint16_t magic;             // 魔法数 0x5A5A
    uint8_t command;            // 命令 0xC0
    uint8_t slot_color;         // 复制上位机的slot_color
    uint8_t status;             // 状态：1=OK, 2=BUSY, 0xFF=ERROR
    uint8_t reserved;           // 保留字段
    uint32_t end_magic;         // 结束魔法数 0x5A5A5F5F
} __attribute__((packed)) start_reply_frame_t;

// 数据帧结构（64字节）
typedef struct {
    uint16_t magic;             // 魔法数 0xA5A5
    uint8_t command;            // 命令 0xD0
    uint8_t slot_color;         // 低4位：slot(0-F), 高4位：颜色(0/1)
    uint8_t page_seq;           // 页序列 (1-61)
    uint8_t frame_seq;          // 帧序列 (1-5)
    uint8_t data[54];           // 数据（前4帧54字节，第5帧32字节+4字节CRC）
    uint32_t end_magic;         // 结束魔法数 0xA5A5AFAF
} __attribute__((packed)) data_frame_t;

// 数据帧回复结构
typedef struct {
    uint16_t magic;             // 魔法数 0x5A5A
    uint8_t command;            // 命令 0xD0
    uint8_t slot_color;         // 复制上位机的slot_color
    uint8_t page_seq;           // 页序列 (1-61)
    uint8_t status;             // 状态码
} __attribute__((packed)) data_reply_frame_t;

// 尾帧结构（8字节）
typedef struct {
    uint16_t magic;             // 魔法数 0xA5A5
    uint8_t command;            // 命令 0xC1
    uint8_t slot_color;         // 低4位：slot(0-F), 高4位：颜色(0/1)
    uint32_t end_magic;         // 结束魔法数 0xA5A5AFAF
} __attribute__((packed)) end_frame_t;

// 尾帧回复结构（8字节）
typedef struct {
    uint16_t magic;             // 魔法数 0x5A5A
    uint8_t command;            // 命令 0xC1
    uint8_t slot_color;         // 复制上位机的slot_color
    uint32_t end_magic;         // 结束魔法数 0x5A5A5F5F
} __attribute__((packed)) end_reply_frame_t;

// 图像传输状态枚举
typedef enum {
    IMG_TRANSFER_IDLE = 0,      // 空闲状态
    IMG_TRANSFER_RECEIVING_BW,  // 接收黑白数据
    IMG_TRANSFER_RECEIVING_RED, // 接收红白数据
    IMG_TRANSFER_PROCESSING,    // 处理数据
    IMG_TRANSFER_COMPLETE,      // 传输完成
    IMG_TRANSFER_ERROR          // 传输错误
} image_transfer_state_t;

// 图像传输上下文
typedef struct {
    image_transfer_state_t state;       // 当前状态
    uint8_t current_slot;               // 当前slot (0-15)
    uint8_t current_color;              // 当前颜色 (0=BW, 1=RED)
    uint8_t current_page;               // 当前页序列 (1-61)
    uint8_t current_frame;              // 当前帧序列 (1-5)
    uint8_t page_buffer[256];           // 页缓冲区
    uint16_t buffer_pos;                // 缓冲区位置
    uint32_t received_crc;              // 接收到的CRC
    uint32_t calculated_crc;            // 计算的CRC
    uint32_t bw_pages_addresses[61];    // 黑白页地址数组
    uint32_t red_pages_addresses[61];   // 红白页地址数组
    uint16_t bw_header_id;              // 黑白头页ID
    uint16_t red_header_id;             // 红白头页ID
    uint8_t timeout_counter;            // 超时计数器
    uint8_t retry_count;                // 重试计数器
} image_transfer_context_t;

#endif // IMAGE_PROTOCOL_H