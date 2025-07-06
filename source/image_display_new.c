#include "image_display_new.h"
#include "w25q32.h"
#include "epd.h"
#include "uart_interface.h"
#include <string.h>

// 外部变量
extern uint8_t g_flash_buffer[FLASH_PAGE_SIZE];

// 静态函数声明
static flash_result_t load_complete_image_data(image_display_new_t* display, uint8_t* bw_buffer, uint8_t* red_buffer);
static flash_result_t verify_image_data_page(const uint8_t* page_data, uint8_t expected_magic, uint8_t expected_seq);

/**
 * @brief 初始化图像显示器
 */
flash_result_t image_display_new_init(image_display_new_t* display, flash_manager_t* flash_mgr)
{
    if (!display || !flash_mgr) {
        return FLASH_ERROR_INVALID_PARAM;
    }
    
    memset(display, 0, sizeof(image_display_new_t));
    display->flash_mgr = flash_mgr;
    display->state = DISPLAY_STATE_IDLE;
    display->display_buffer = g_flash_buffer;  // 复用Flash缓冲区
    
    return FLASH_OK;
}

/**
 * @brief 重置图像显示器
 */
void image_display_new_reset(image_display_new_t* display)
{
    if (!display) return;
    
    display->state = DISPLAY_STATE_IDLE;
    display->current_page = 0;
    display->buffer_pos = 0;
    display->processed_bytes = 0;
    memset(display->bw_pages_addresses, 0, sizeof(display->bw_pages_addresses));
    memset(display->red_pages_addresses, 0, sizeof(display->red_pages_addresses));
}

/**
 * @brief 查找指定槽位的图像头页
 */
flash_result_t find_image_headers_by_slot(image_display_new_t* display, uint8_t slot, 
                                         uint16_t* bw_header_id, uint16_t* red_header_id)
{
    // 简化实现：假设头页ID按槽位分配
    // 实际实现中应该遍历Flash查找对应槽位的头页
    
    // 这里使用简单的ID分配策略
    // 槽位0: BW=0x1000, RED=0x1001
    // 槽位1: BW=0x1002, RED=0x1003
    // ...
    
    *bw_header_id = 0x1000 + (slot * 2);
    *red_header_id = 0x1000 + (slot * 2) + 1;
    
    UARTIF_uartPrintf(0, "查找槽位%d的图像头页: BW=0x%04X, RED=0x%04X\n", 
                     slot, *bw_header_id, *red_header_id);
    
    return FLASH_OK;
}

/**
 * @brief 从Flash加载图像头页
 */
flash_result_t load_image_header(image_display_new_t* display, uint16_t header_id, uint8_t is_bw_header)
{
    // 通过Flash管理器查找头页
    uint8_t read_size = FLASH_PAGE_SIZE;
    flash_result_t result = flash_read_data(display->flash_mgr, header_id, g_flash_buffer, &read_size);
    
    if (result != FLASH_OK) {
        UARTIF_uartPrintf(0, "读取图像头页失败: ID=0x%04X, 错误=%d\n", header_id, result);
        return result;
    }
    
    // 解析头页
    image_header_page_t* header = (image_header_page_t*)g_flash_buffer;
    
    // 验证魔法数
    if (header->magic != MAGIC_IMAGE_HEADER) {
        UARTIF_uartPrintf(0, "图像头页魔法数错误: 0x%02X\n", header->magic);
        return FLASH_ERROR_INVALID_PARAM;
    }
    
    // 验证CRC
    uint32_t calculated_crc = calculate_crc32((uint8_t*)&header->entries, 
                                             sizeof(header->entries) + sizeof(header->partner_header_id) + sizeof(header->reserved2));
    if (header->crc32 != calculated_crc) {
        UARTIF_uartPrintf(0, "图像头页CRC错误: 期望=0x%08X, 计算=0x%08X\n", 
                         header->crc32, calculated_crc);
        return FLASH_ERROR_CRC_FAIL;
    }
    
    // 提取页地址
    uint32_t* target_addresses = is_bw_header ? display->bw_pages_addresses : display->red_pages_addresses;
    
    for (int i = 0; i < IMAGE_HEADER_ENTRIES; i++) {
        target_addresses[i] = (header->entries[i].address[0] << 16) |
                             (header->entries[i].address[1] << 8) |
                             header->entries[i].address[2];
    }
    
    // 记录头页ID
    if (is_bw_header) {
        display->bw_header_id = header_id;
        display->red_header_id = header->partner_header_id;
    } else {
        display->red_header_id = header_id;
        display->bw_header_id = header->partner_header_id;
    }
    
    UARTIF_uartPrintf(0, "图像头页加载成功: %s, ID=0x%04X\n", 
                     is_bw_header ? "黑白" : "红白", header_id);
    
    return FLASH_OK;
}

/**
 * @brief 从Flash加载图像数据页
 */
flash_result_t load_image_data_page(image_display_new_t* display, uint32_t page_address, uint8_t* buffer)
{
    // 直接从Flash地址读取
    if (W25Q32_ReadData(page_address, buffer, FLASH_PAGE_SIZE) != 0) {
        UARTIF_uartPrintf(0, "从Flash读取数据页失败: 地址=0x%08X\n", page_address);
        return FLASH_ERROR_READ_FAIL;
    }
    
    return FLASH_OK;
}

/**
 * @brief 验证图像数据页
 */
static flash_result_t verify_image_data_page(const uint8_t* page_data, uint8_t expected_magic, uint8_t expected_seq)
{
    if (page_data[0] != expected_magic) {
        UARTIF_uartPrintf(0, "数据页魔法数错误: 期望=0x%02X, 实际=0x%02X\n", 
                         expected_magic, page_data[0]);
        return FLASH_ERROR_INVALID_PARAM;
    }
    
    if (page_data[1] != expected_seq) {
        UARTIF_uartPrintf(0, "数据页序列错误: 期望=%d, 实际=%d\n", 
                         expected_seq, page_data[1]);
        return FLASH_ERROR_INVALID_PARAM;
    }
    
    // 验证CRC32
    uint32_t stored_crc = *(uint32_t*)&page_data[4];
    uint32_t data_size = (expected_seq == IMAGE_PAGES_PER_COLOR) ? 
                        IMAGE_LAST_PAGE_DATA_SIZE : IMAGE_DATA_PER_PAGE;
    uint32_t calculated_crc = calculate_crc32(&page_data[8], data_size);
    
    if (stored_crc != calculated_crc) {
        UARTIF_uartPrintf(0, "数据页CRC错误: 期望=0x%08X, 计算=0x%08X\n", 
                         stored_crc, calculated_crc);
        return FLASH_ERROR_CRC_FAIL;
    }
    
    return FLASH_OK;
}

/**
 * @brief 加载完整图像数据
 */
static flash_result_t load_complete_image_data(image_display_new_t* display, uint8_t* bw_buffer, uint8_t* red_buffer)
{
    flash_result_t result;
    uint8_t page_buffer[FLASH_PAGE_SIZE];
    
    // 加载黑白图像数据
    UARTIF_uartPrintf(0, "开始加载黑白图像数据...\n");
    for (int page = 0; page < IMAGE_PAGES_PER_COLOR; page++) {
        uint32_t page_address = display->bw_pages_addresses[page];
        if (page_address == 0) {
            UARTIF_uartPrintf(0, "黑白图像页%d地址无效\n", page + 1);
            return FLASH_ERROR_NOT_FOUND;
        }
        
        // 读取页数据
        result = load_image_data_page(display, page_address, page_buffer);
        if (result != FLASH_OK) {
            return result;
        }
        
        // 验证页数据
        result = verify_image_data_page(page_buffer, MAGIC_BW_IMAGE_DATA, page + 1);
        if (result != FLASH_OK) {
            return result;
        }
        
        // 复制数据到输出缓冲区
        uint32_t data_size = (page == IMAGE_PAGES_PER_COLOR - 1) ? 
                            IMAGE_LAST_PAGE_DATA_SIZE : IMAGE_DATA_PER_PAGE;
        uint32_t dest_offset = page * IMAGE_DATA_PER_PAGE;
        
        memcpy(&bw_buffer[dest_offset], &page_buffer[8], data_size);
        
        // 进度显示
        if ((page + 1) % 10 == 0 || page == IMAGE_PAGES_PER_COLOR - 1) {
            UARTIF_uartPrintf(0, "黑白图像加载进度: %d/%d\n", page + 1, IMAGE_PAGES_PER_COLOR);
        }
    }
    
    // 加载红白图像数据
    UARTIF_uartPrintf(0, "开始加载红白图像数据...\n");
    for (int page = 0; page < IMAGE_PAGES_PER_COLOR; page++) {
        uint32_t page_address = display->red_pages_addresses[page];
        if (page_address == 0) {
            UARTIF_uartPrintf(0, "红白图像页%d地址无效\n", page + 1);
            return FLASH_ERROR_NOT_FOUND;
        }
        
        // 读取页数据
        result = load_image_data_page(display, page_address, page_buffer);
        if (result != FLASH_OK) {
            return result;
        }
        
        // 验证页数据
        result = verify_image_data_page(page_buffer, MAGIC_RED_IMAGE_DATA, page + 1);
        if (result != FLASH_OK) {
            return result;
        }
        
        // 复制数据到输出缓冲区
        uint32_t data_size = (page == IMAGE_PAGES_PER_COLOR - 1) ? 
                            IMAGE_LAST_PAGE_DATA_SIZE : IMAGE_DATA_PER_PAGE;
        uint32_t dest_offset = page * IMAGE_DATA_PER_PAGE;
        
        memcpy(&red_buffer[dest_offset], &page_buffer[8], data_size);
        
        // 进度显示
        if ((page + 1) % 10 == 0 || page == IMAGE_PAGES_PER_COLOR - 1) {
            UARTIF_uartPrintf(0, "红白图像加载进度: %d/%d\n", page + 1, IMAGE_PAGES_PER_COLOR);
        }
    }
    
    UARTIF_uartPrintf(0, "图像数据加载完成\n");
    return FLASH_OK;
}

/**
 * @brief 将图像数据显示到EPD
 */
flash_result_t display_image_to_epd(const uint8_t* bw_data, const uint8_t* red_data)
{
    UARTIF_uartPrintf(0, "开始显示图像到EPD...\n");
    
    // 这里应该调用实际的EPD显示函数
    // 由于EPD函数可能需要特定的数据格式，这里提供一个框架
    
    // 示例：假设有一个函数可以显示黑白和红色数据
    // EPD_DisplayImage(bw_data, red_data, IMAGE_WIDTH, IMAGE_HEIGHT);
    
    // 临时实现：显示一些统计信息
    uint32_t bw_pixels = 0, red_pixels = 0;
    for (int i = 0; i < IMAGE_SIZE; i++) {
        uint8_t bw_byte = bw_data[i];
        uint8_t red_byte = red_data[i];
        
        // 计算像素数量（简化统计）
        for (int bit = 0; bit < 8; bit++) {
            if (bw_byte & (1 << bit)) bw_pixels++;
            if (red_byte & (1 << bit)) red_pixels++;
        }
    }
    
    UARTIF_uartPrintf(0, "图像统计: 黑色像素=%d, 红色像素=%d\n", bw_pixels, red_pixels);
    
    // 这里可以调用实际的EPD刷新函数
    // EPD_WhiteScreenGDEY042Z98ALLBlack();  // 清屏
    // EPD_DisplayImageData(bw_data, red_data);  // 显示图像
    
    UARTIF_uartPrintf(0, "图像显示完成\n");
    return FLASH_OK;
}

/**
 * @brief 显示指定槽位的图像
 */
flash_result_t image_display_new_show(image_display_new_t* display, uint8_t slot)
{
    if (!display) {
        return FLASH_ERROR_INVALID_PARAM;
    }
    
    if (display->state != DISPLAY_STATE_IDLE) {
        UARTIF_uartPrintf(0, "图像显示器忙碌，当前状态=%d\n", display->state);
        return FLASH_ERROR_NO_SPACE;
    }
    
    display->current_slot = slot;
    display->state = DISPLAY_STATE_LOADING;
    display->current_page = 0;
    display->buffer_pos = 0;
    display->processed_bytes = 0;
    
    UARTIF_uartPrintf(0, "开始显示槽位%d的图像\n", slot);
    
    return FLASH_OK;
}

/**
 * @brief 处理图像显示过程
 */
flash_result_t image_display_new_process(image_display_new_t* display)
{
    if (!display) {
        return FLASH_ERROR_INVALID_PARAM;
    }
    
    flash_result_t result = FLASH_OK;
    
    switch (display->state) {
        case DISPLAY_STATE_IDLE:
            // 空闲状态，无需处理
            break;
            
        case DISPLAY_STATE_LOADING:
        {
            // 查找图像头页
            uint16_t bw_header_id, red_header_id;
            result = find_image_headers_by_slot(display, display->current_slot, 
                                              &bw_header_id, &red_header_id);
            if (result != FLASH_OK) {
                display->state = DISPLAY_STATE_ERROR;
                break;
            }
            
            // 加载黑白图像头页
            result = load_image_header(display, bw_header_id, 1);
            if (result != FLASH_OK) {
                display->state = DISPLAY_STATE_ERROR;
                break;
            }
            
            // 加载红白图像头页
            result = load_image_header(display, red_header_id, 0);
            if (result != FLASH_OK) {
                display->state = DISPLAY_STATE_ERROR;
                break;
            }
            
            display->state = DISPLAY_STATE_DECOMPRESSING;
            break;
        }
        
        case DISPLAY_STATE_DECOMPRESSING:
        {
            // 分配图像数据缓冲区
            static uint8_t bw_image_data[IMAGE_SIZE];
            static uint8_t red_image_data[IMAGE_SIZE];
            
            // 加载完整图像数据
            result = load_complete_image_data(display, bw_image_data, red_image_data);
            if (result != FLASH_OK) {
                display->state = DISPLAY_STATE_ERROR;
                break;
            }
            
            display->state = DISPLAY_STATE_DISPLAYING;
            break;
        }
        
        case DISPLAY_STATE_DISPLAYING:
        {
            // 显示图像到EPD
            static uint8_t bw_image_data[IMAGE_SIZE];
            static uint8_t red_image_data[IMAGE_SIZE];
            
            result = display_image_to_epd(bw_image_data, red_image_data);
            if (result != FLASH_OK) {
                display->state = DISPLAY_STATE_ERROR;
                break;
            }
            
            display->state = DISPLAY_STATE_COMPLETE;
            break;
        }
        
        case DISPLAY_STATE_COMPLETE:
            // 显示完成，保持状态直到被重置
            break;
            
        case DISPLAY_STATE_ERROR:
            // 错误状态，保持直到被重置
            break;
            
        default:
            display->state = DISPLAY_STATE_ERROR;
            break;
    }
    
    return result;
}