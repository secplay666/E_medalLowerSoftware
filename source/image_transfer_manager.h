#ifndef IMAGE_TRANSFER_MANAGER_H
#define IMAGE_TRANSFER_MANAGER_H

#include "image_protocol.h"
#include "flash_manager.h"
#include "queue.h"

// 图像传输管理器结构
typedef struct {
    image_transfer_context_t context;   // 传输上下文
    flash_manager_t* flash_mgr;         // Flash管理器指针
    Queue* rx_queue;                    // 接收队列指针
    uint8_t protocol_buffer[64];        // 协议缓冲区
    uint16_t protocol_pos;              // 协议缓冲区位置
    uint32_t last_activity_time;        // 最后活动时间
} image_transfer_manager_t;

// 函数声明

/**
 * @brief 初始化图像传输管理器
 * @param manager 传输管理器指针
 * @param flash_mgr Flash管理器指针
 * @param rx_queue 接收队列指针
 * @return flash_result_t 操作结果
 */
flash_result_t image_transfer_init(image_transfer_manager_t* manager, 
                                  flash_manager_t* flash_mgr, 
                                  Queue* rx_queue);

/**
 * @brief 处理图像传输
 * @param manager 传输管理器指针
 * @return flash_result_t 操作结果
 */
flash_result_t image_transfer_process(image_transfer_manager_t* manager);

/**
 * @brief 重置传输状态
 * @param manager 传输管理器指针
 */
void image_transfer_reset(image_transfer_manager_t* manager);

/**
 * @brief 发送回复帧
 * @param frame_data 帧数据指针
 * @param frame_size 帧大小
 */
void send_reply_frame(const uint8_t* frame_data, uint16_t frame_size);

/**
 * @brief 计算CRC32
 * @param data 数据指针
 * @param length 数据长度
 * @return uint32_t CRC32值
 */
uint32_t calculate_crc32(const uint8_t* data, uint32_t length);

/**
 * @brief 获取下一个可用的数据ID
 * @param manager 传输管理器指针
 * @return uint16_t 数据ID
 */
uint16_t get_next_data_id(image_transfer_manager_t* manager);

/**
 * @brief 分配Flash页地址
 * @param manager 传输管理器指针
 * @return uint32_t Flash页地址
 */
uint32_t allocate_flash_page_address(image_transfer_manager_t* manager);

/**
 * @brief 创建并写入图像头页
 * @param manager 传输管理器指针
 * @param is_bw_header 是否为黑白头页
 * @return flash_result_t 操作结果
 */
flash_result_t create_image_header_page(image_transfer_manager_t* manager, uint8_t is_bw_header);

/**
 * @brief 验证接收到的页数据完整性
 * @param manager 传输管理器指针
 * @return uint8_t 1=完整，0=不完整
 */
uint8_t verify_received_pages(image_transfer_manager_t* manager);

#endif // IMAGE_TRANSFER_MANAGER_H