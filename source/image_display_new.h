#ifndef IMAGE_DISPLAY_NEW_H
#define IMAGE_DISPLAY_NEW_H

#include <stdint.h>
#include "flash_manager.h"
#include "image_protocol.h"

/**
 * @brief 图像显示状态枚举
 */
typedef enum {
    DISPLAY_STATE_IDLE = 0,        // 空闲状态
    DISPLAY_STATE_LOADING,         // 加载图像数据
    DISPLAY_STATE_DECOMPRESSING,   // 解压缩数据
    DISPLAY_STATE_DISPLAYING,      // 显示到EPD
    DISPLAY_STATE_COMPLETE,        // 显示完成
    DISPLAY_STATE_ERROR           // 错误状态
} display_state_t;

/**
 * @brief 图像显示器结构体
 */
typedef struct {
    display_state_t state;              // 当前状态
    flash_manager_t* flash_mgr;         // Flash管理器指针
    uint8_t current_slot;               // 当前显示的槽位
    uint16_t bw_header_id;              // 黑白图像头页ID
    uint16_t red_header_id;             // 红白图像头页ID
    uint32_t bw_pages_addresses[IMAGE_HEADER_ENTRIES];   // 黑白图像页地址
    uint32_t red_pages_addresses[IMAGE_HEADER_ENTRIES];  // 红白图像页地址
    uint8_t current_page;               // 当前处理的页
    uint8_t* display_buffer;            // 显示缓冲区指针
    uint32_t buffer_pos;                // 缓冲区位置
    uint32_t total_bytes;               // 总字节数
    uint32_t processed_bytes;           // 已处理字节数
} image_display_new_t;

/**
 * @brief 初始化图像显示器
 * 
 * @param display 图像显示器指针
 * @param flash_mgr Flash管理器指针
 * @return flash_result_t 操作结果
 */
flash_result_t image_display_new_init(image_display_new_t* display, flash_manager_t* flash_mgr);

/**
 * @brief 显示指定槽位的图像
 * 
 * @param display 图像显示器指针
 * @param slot 图像槽位 (0-15)
 * @return flash_result_t 操作结果
 */
flash_result_t image_display_new_show(image_display_new_t* display, uint8_t slot);

/**
 * @brief 处理图像显示过程
 * 
 * @param display 图像显示器指针
 * @return flash_result_t 操作结果
 */
flash_result_t image_display_new_process(image_display_new_t* display);

/**
 * @brief 重置图像显示器
 * 
 * @param display 图像显示器指针
 */
void image_display_new_reset(image_display_new_t* display);

/**
 * @brief 从Flash加载图像头页
 * 
 * @param display 图像显示器指针
 * @param header_id 头页ID
 * @param is_bw_header 是否为黑白头页
 * @return flash_result_t 操作结果
 */
flash_result_t load_image_header(image_display_new_t* display, uint16_t header_id, uint8_t is_bw_header);

/**
 * @brief 从Flash加载图像数据页
 * 
 * @param display 图像显示器指针
 * @param page_address 页地址
 * @param buffer 输出缓冲区
 * @return flash_result_t 操作结果
 */
flash_result_t load_image_data_page(image_display_new_t* display, uint32_t page_address, uint8_t* buffer);

/**
 * @brief 将图像数据显示到EPD
 * 
 * @param bw_data 黑白图像数据
 * @param red_data 红白图像数据
 * @return flash_result_t 操作结果
 */
flash_result_t display_image_to_epd(const uint8_t* bw_data, const uint8_t* red_data);

/**
 * @brief 查找指定槽位的图像头页
 * 
 * @param display 图像显示器指针
 * @param slot 图像槽位
 * @param bw_header_id 输出黑白头页ID
 * @param red_header_id 输出红白头页ID
 * @return flash_result_t 操作结果
 */
flash_result_t find_image_headers_by_slot(image_display_new_t* display, uint8_t slot, 
                                         uint16_t* bw_header_id, uint16_t* red_header_id);

#endif // IMAGE_DISPLAY_NEW_H