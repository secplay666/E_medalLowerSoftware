#include "flash_manager.h"
#include "w25q32.h"
#include <string.h>

// 内部函数声明
static uint32_t calculate_crc32(const uint8_t* data, uint32_t length);
static flash_result_t read_segment_header(uint32_t segment_base, segment_header_t* header);
static flash_result_t write_segment_header(uint32_t segment_base, const segment_header_t* header);
static flash_result_t scan_segment_pages(flash_manager_t* manager, uint32_t segment_base);
static int16_t find_data_entry(flash_manager_t* manager, uint16_t data_id);
static flash_result_t add_data_entry(flash_manager_t* manager, uint16_t data_id, uint32_t page_address);
static flash_result_t remove_data_entry(flash_manager_t* manager, uint16_t data_id);
static flash_result_t erase_segment(uint32_t segment_base);
static flash_result_t copy_valid_pages(flash_manager_t* manager);
 
/******************************************************************************
 * Local variable definitions ('static')                                      *
 ******************************************************************************/

// 静态缓冲区，用于Flash读写操作的中间变量
static uint8_t g_flash_buffer[FLASH_PAGE_SIZE];

/**
 * @brief 计算CRC32校验值
 */
static uint32_t calculate_crc32(const uint8_t* data, uint32_t length)
{
    uint32_t crc = 0xFFFFFFFF;
    uint32_t i, j;
    
    for (i = 0; i < length; i++) {
        crc ^= data[i];
        for (j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ CRC32_POLYNOMIAL;
            } else {
                crc >>= 1;
            }
        }
    }
    
    return ~crc;
}

/**
 * @brief 读取segment头
 */
static flash_result_t read_segment_header(uint32_t segment_base, segment_header_t* header)
{
	uint32_t calculated_crc = 0;
    // 使用静态缓冲区读取数据
    if (W25Q32_ReadData(segment_base, g_flash_buffer, sizeof(segment_header_t)) != 0) {
        return FLASH_ERROR_READ_FAIL;
    }
    
    // 从缓冲区复制到结构体
    header->header_magic = g_flash_buffer[0];
    header->segment_id = g_flash_buffer[1];
    header->status_magic = (uint32_t)g_flash_buffer[2] | ((uint32_t)g_flash_buffer[3] << 8) | 
                          ((uint32_t)g_flash_buffer[4] << 16) | ((uint32_t)g_flash_buffer[5] << 24);
    header->gc_count = (uint32_t)g_flash_buffer[6] | ((uint32_t)g_flash_buffer[7] << 8) | 
                      ((uint32_t)g_flash_buffer[8] << 16) | ((uint32_t)g_flash_buffer[9] << 24);
    header->crc32 = (uint32_t)g_flash_buffer[10] | ((uint32_t)g_flash_buffer[11] << 8) | 
                   ((uint32_t)g_flash_buffer[12] << 16) | ((uint32_t)g_flash_buffer[13] << 24);
    
    // 验证头魔法数字
    if (header->header_magic != SEGMENT_HEADER_MAGIC) {
        return FLASH_ERROR_CRC_FAIL;
    }
    
    // 验证CRC32 - 只校验有效部分（不包括crc32字段）
    calculated_crc = calculate_crc32(g_flash_buffer, 10); // header_magic(1) + segment_id(1) + status_magic(4) + gc_count(4) = 10字节有效数据
    if (calculated_crc != header->crc32) {
        return FLASH_ERROR_CRC_FAIL;
    }
    
    return FLASH_OK;
}

/**
 * @brief 写入segment头
 */
static flash_result_t write_segment_header(uint32_t segment_base, const segment_header_t* header)
{
	uint32_t crc32 = 0;
    // 清空缓冲区
    memset(g_flash_buffer, 0, FLASH_PAGE_SIZE);
    
    // 将结构体字段复制到缓冲区
    g_flash_buffer[0] = SEGMENT_HEADER_MAGIC;
    g_flash_buffer[1] = header->segment_id;
    g_flash_buffer[2] = (uint8_t)(header->status_magic & 0xFF);
    g_flash_buffer[3] = (uint8_t)((header->status_magic >> 8) & 0xFF);
    g_flash_buffer[4] = (uint8_t)((header->status_magic >> 16) & 0xFF);
    g_flash_buffer[5] = (uint8_t)((header->status_magic >> 24) & 0xFF);
    g_flash_buffer[6] = (uint8_t)(header->gc_count & 0xFF);
    g_flash_buffer[7] = (uint8_t)((header->gc_count >> 8) & 0xFF);
    g_flash_buffer[8] = (uint8_t)((header->gc_count >> 16) & 0xFF);
    g_flash_buffer[9] = (uint8_t)((header->gc_count >> 24) & 0xFF);
    
    // 计算CRC32 - 只计算有效部分（不包括crc32字段）
    crc32 = calculate_crc32(g_flash_buffer, 10); // header_magic(1) + segment_id(1) + status_magic(4) + gc_count(4) = 10字节有效数据
    g_flash_buffer[10] = (uint8_t)(crc32 & 0xFF);
    g_flash_buffer[11] = (uint8_t)((crc32 >> 8) & 0xFF);
    g_flash_buffer[12] = (uint8_t)((crc32 >> 16) & 0xFF);
    g_flash_buffer[13] = (uint8_t)((crc32 >> 24) & 0xFF);
    
    // 擦除第一个扇区
    if (W25Q32_EraseSector(segment_base) != 0) {
        return FLASH_ERROR_ERASE_FAIL;
    }
    
    // 写入header（写入整个页面以保持256字节对齐）
    if (W25Q32_WritePage(segment_base, g_flash_buffer, FLASH_PAGE_SIZE) != 0) {
        return FLASH_ERROR_WRITE_FAIL;
    }
    
    return FLASH_OK;
}

/**
 * @brief 扫描segment中的所有page，构建内存映射表
 */
static flash_result_t scan_segment_pages(flash_manager_t* manager, uint32_t segment_base)
{
    data_page_t page;
    uint32_t page_address;
    uint32_t i;
     uint32_t calculated_crc = 0;
    manager->data_count = 0;
    
    // 从第1个page开始扫描（第0个是header）
    for (i = 1; i < FLASH_PAGES_PER_SEGMENT; i++) {
        page_address = segment_base + i * FLASH_PAGE_SIZE;
        
        // 读取page
        if (W25Q32_ReadData(page_address, g_flash_buffer, sizeof(data_page_t)) != 0) {
            return FLASH_ERROR_READ_FAIL;
        }
        
        // 从缓冲区复制到结构体
        page.data_id = (uint16_t)g_flash_buffer[0] | ((uint16_t)g_flash_buffer[1] << 8);
        page.data_size = g_flash_buffer[2];
        page.reserved = g_flash_buffer[3];
        memcpy(page.data, &g_flash_buffer[4], 248);
        page.crc32 = (uint32_t)g_flash_buffer[252] | ((uint32_t)g_flash_buffer[253] << 8) | 
                     ((uint32_t)g_flash_buffer[254] << 16) | ((uint32_t)g_flash_buffer[255] << 24);
        
        // 检查是否为空page
        if (page.data_id == INVALID_DATA_ID) {
            // 找到第一个空page，设置下次写入地址
            manager->next_write_address = page_address;
            break;
        }
        
        // 验证CRC32
        calculated_crc = calculate_crc32(page.data, page.data_size);
        if (calculated_crc != page.crc32) {
            // CRC错误，跳过这个page
            continue;
        }
        
        // 添加到映射表
        if (add_data_entry(manager, page.data_id, page_address) != FLASH_OK) {
            return FLASH_ERROR_INIT_FAIL;
        }
    }
    
    // 如果扫描完所有page都没找到空page，说明segment已满
    if (i >= FLASH_PAGES_PER_SEGMENT) {
        manager->next_write_address = INVALID_ADDRESS;
    }
    
    return FLASH_OK;
}

/**
 * @brief 在映射表中查找数据条目
 */
static int16_t find_data_entry(flash_manager_t* manager, uint16_t data_id)
{
    uint16_t i;
    
    for (i = 0; i < manager->data_count; i++) {
        if (manager->data_entries[i].data_id == data_id) {
            return i;
        }
    }
    
    return -1;
}

/**
 * @brief 添加数据条目到映射表
 */
static flash_result_t add_data_entry(flash_manager_t* manager, uint16_t data_id, uint32_t page_address)
{
	int16_t index = 0;
    if (manager->data_count >= MAX_DATA_ENTRIES) {
        return FLASH_ERROR_NO_SPACE;
    }
    
    // 检查是否已存在，如果存在则更新地址
    index = find_data_entry(manager, data_id);
    if (index >= 0) {
        manager->data_entries[index].page_address = page_address;
    } else {
        // 添加新条目
        manager->data_entries[manager->data_count].data_id = data_id;
        manager->data_entries[manager->data_count].page_address = page_address;
        manager->data_count++;
    }
    
    return FLASH_OK;
}

/**
 * @brief 从映射表中移除数据条目
 */
static flash_result_t remove_data_entry(flash_manager_t* manager, uint16_t data_id)
{
    int16_t index = find_data_entry(manager, data_id);
	    uint16_t i;
    if (index < 0) {
        return FLASH_ERROR_NOT_FOUND;
    }
    
    // 将后面的条目前移

    for (i = index; i < manager->data_count - 1; i++) {
        manager->data_entries[i] = manager->data_entries[i + 1];
    }
    
    manager->data_count--;
    return FLASH_OK;
}

/**
 * @brief 擦除整个segment
 */
static flash_result_t erase_segment(uint32_t segment_base)
{
    uint32_t sector_address;
    uint32_t i;
    
    // 按扇区擦除整个segment
    for (i = 0; i < FLASH_SEGMENT_SIZE; i += FLASH_SECTOR_SIZE) {
        sector_address = segment_base + i;
        if (W25Q32_EraseSector(sector_address) != 0) {
            return FLASH_ERROR_ERASE_FAIL;
        }
    }
    
    return FLASH_OK;
}

/**
 * @brief 复制有效页面到备用segment
 */
static flash_result_t copy_valid_pages(flash_manager_t* manager)
{
    uint32_t dest_address = manager->backup_segment_base + FLASH_PAGE_SIZE; // 跳过header
    uint16_t i;
    
    for (i = 0; i < manager->data_count; i++) {
        // 读取源page
        if (W25Q32_ReadData(manager->data_entries[i].page_address, g_flash_buffer, FLASH_PAGE_SIZE) != 0) {
            return FLASH_ERROR_READ_FAIL;
        }
        
        // 写入目标page
        if (W25Q32_WritePage(dest_address, g_flash_buffer, FLASH_PAGE_SIZE) != 0) {
            return FLASH_ERROR_WRITE_FAIL;
        }
        
        // 更新映射表中的地址
        manager->data_entries[i].page_address = dest_address;
        dest_address += FLASH_PAGE_SIZE;
    }
    
    // 更新下次写入地址
    manager->next_write_address = dest_address;
    
    return FLASH_OK;
}

/**
 * @brief 初始化Flash管理器
 */
flash_result_t flash_manager_init(flash_manager_t* manager)
{
    segment_header_t header0, header1;
    flash_result_t result;
    
    if (manager == NULL) {
        return FLASH_ERROR_INVALID_PARAM;
    }
    
    // 初始化管理器
    memset(manager, 0, sizeof(flash_manager_t));
    manager->active_segment_base = FLASH_SEGMENT0_BASE;
    manager->backup_segment_base = FLASH_SEGMENT1_BASE;
    
    // 读取两个segment的header
    result = read_segment_header(FLASH_SEGMENT0_BASE, &header0);
    if (result != FLASH_OK && result != FLASH_ERROR_CRC_FAIL) {
        return result;
    }
    
    result = read_segment_header(FLASH_SEGMENT1_BASE, &header1);
    if (result != FLASH_OK && result != FLASH_ERROR_CRC_FAIL) {
        return result;
    }
    
    // 根据header状态确定激活segment
    if (header0.status_magic == SEGMENT_MAGIC_ACTIVE && header1.status_magic == SEGMENT_MAGIC_BACKUP) {
        // 正常情况：segment0激活，segment1备用
        manager->active_segment_base = FLASH_SEGMENT0_BASE;
        manager->backup_segment_base = FLASH_SEGMENT1_BASE;
    } else if (header1.status_magic == SEGMENT_MAGIC_ACTIVE && header0.status_magic == SEGMENT_MAGIC_BACKUP) {
        // segment1激活，segment0备用
        manager->active_segment_base = FLASH_SEGMENT1_BASE;
        manager->backup_segment_base = FLASH_SEGMENT0_BASE;
    } else if (header0.status_magic == SEGMENT_MAGIC_GC || header1.status_magic == SEGMENT_MAGIC_GC) {
        // 垃圾回收中断，需要恢复
        manager->gc_in_progress = 1;
        // 简单处理：使用非GC状态的segment作为激活segment
        if (header0.status_magic != SEGMENT_MAGIC_GC) {
            manager->active_segment_base = FLASH_SEGMENT0_BASE;
            manager->backup_segment_base = FLASH_SEGMENT1_BASE;
        } else {
            manager->active_segment_base = FLASH_SEGMENT1_BASE;
            manager->backup_segment_base = FLASH_SEGMENT0_BASE;
        }
    } else {
        // 都是未初始化状态，初始化segment0为激活
        segment_header_t init_header;
        memset(&init_header, 0, sizeof(segment_header_t));
        init_header.status_magic = SEGMENT_MAGIC_ACTIVE;
        init_header.segment_id = 0;
        init_header.gc_count = 0;
        
        result = write_segment_header(FLASH_SEGMENT0_BASE, &init_header);
        if (result != FLASH_OK) {
            return result;
        }
        
        manager->active_segment_base = FLASH_SEGMENT0_BASE;
        manager->backup_segment_base = FLASH_SEGMENT1_BASE;
        manager->next_write_address = FLASH_SEGMENT0_BASE + FLASH_PAGE_SIZE;
        return FLASH_OK;
    }
    
    // 扫描激活segment构建映射表
    return scan_segment_pages(manager, manager->active_segment_base);
}

/**
 * @brief 写入数据
 */
flash_result_t flash_write_data(flash_manager_t* manager, uint16_t data_id, const uint8_t* data, uint8_t size)
{
    flash_result_t result;
    uint32_t crc32;
    uint32_t segment_end = 0;
    if (manager == NULL || data == NULL || size == 0 || size > 248 || data_id == INVALID_DATA_ID) {
        return FLASH_ERROR_INVALID_PARAM;
    }
    
    // 检查是否需要垃圾回收
    if (manager->next_write_address == INVALID_ADDRESS) {
        result = flash_garbage_collect(manager);
        if (result != FLASH_OK) {
            return result;
        }
    }
    
    // 清空缓冲区
    memset(g_flash_buffer, 0, FLASH_PAGE_SIZE);
    
    // 将数据页字段复制到缓冲区
    g_flash_buffer[0] = (uint8_t)(data_id & 0xFF);
    g_flash_buffer[1] = (uint8_t)((data_id >> 8) & 0xFF);
    g_flash_buffer[2] = size;
    g_flash_buffer[3] = 0; // reserved
    memcpy(&g_flash_buffer[4], data, size);
    
    // 计算CRC32（只计算数据部分）
    crc32 = calculate_crc32(&g_flash_buffer[4], size);
    g_flash_buffer[252] = (uint8_t)(crc32 & 0xFF);
    g_flash_buffer[253] = (uint8_t)((crc32 >> 8) & 0xFF);
    g_flash_buffer[254] = (uint8_t)((crc32 >> 16) & 0xFF);
    g_flash_buffer[255] = (uint8_t)((crc32 >> 24) & 0xFF);
    
    // 写入Flash
    if (W25Q32_WritePage(manager->next_write_address, g_flash_buffer, FLASH_PAGE_SIZE) != 0) {
        return FLASH_ERROR_WRITE_FAIL;
    }
    
    // 更新映射表
    result = add_data_entry(manager, data_id, manager->next_write_address);
    if (result != FLASH_OK) {
        return result;
    }
    
    // 更新下次写入地址
    manager->next_write_address += FLASH_PAGE_SIZE;
    
    // 检查是否到达segment末尾
    segment_end = manager->active_segment_base + FLASH_SEGMENT_SIZE;
    if (manager->next_write_address >= segment_end) {
        manager->next_write_address = INVALID_ADDRESS;
    }
    
    return FLASH_OK;
}

/**
 * @brief 读取数据
 */
flash_result_t flash_read_data(flash_manager_t* manager, uint16_t data_id, uint8_t* data, uint8_t* size)
{
    int16_t index;
    uint32_t calculated_crc, stored_crc;
    uint8_t page_data_size;
    
    if (manager == NULL || data == NULL || size == NULL || data_id == INVALID_DATA_ID) {
        return FLASH_ERROR_INVALID_PARAM;
    }
    
    // 在映射表中查找
    index = find_data_entry(manager, data_id);
    if (index < 0) {
        return FLASH_ERROR_NOT_FOUND;
    }
    
    // 读取数据页到缓冲区
    if (W25Q32_ReadData(manager->data_entries[index].page_address, g_flash_buffer, FLASH_PAGE_SIZE) != 0) {
        return FLASH_ERROR_READ_FAIL;
    }
    
    // 从缓冲区解析数据页字段
    page_data_size = g_flash_buffer[2];
    stored_crc = (uint32_t)g_flash_buffer[252] | ((uint32_t)g_flash_buffer[253] << 8) | 
                 ((uint32_t)g_flash_buffer[254] << 16) | ((uint32_t)g_flash_buffer[255] << 24);
    
    // 验证CRC32（只验证数据部分）
    calculated_crc = calculate_crc32(&g_flash_buffer[4], page_data_size);
    if (calculated_crc != stored_crc) {
        return FLASH_ERROR_CRC_FAIL;
    }
    
    // 检查缓冲区大小
    if (*size < page_data_size) {
        *size = page_data_size;
        return FLASH_ERROR_INVALID_PARAM;
    }
    
    // 复制数据
    memcpy(data, &g_flash_buffer[4], page_data_size);
    *size = page_data_size;
    
    return FLASH_OK;
}

/**
 * @brief 删除数据
 */
flash_result_t flash_delete_data(flash_manager_t* manager, uint16_t data_id)
{
    if (manager == NULL || data_id == INVALID_DATA_ID) {
        return FLASH_ERROR_INVALID_PARAM;
    }
    
    return remove_data_entry(manager, data_id);
}

/**
 * @brief 执行垃圾回收
 */
flash_result_t flash_garbage_collect(flash_manager_t* manager)
{
    segment_header_t header;
    flash_result_t result;
    segment_header_t active_header;
	uint32_t new_gc_count = 0;
	uint32_t temp = 0;
    if (manager == NULL) {
        return FLASH_ERROR_INVALID_PARAM;
    }
    
    manager->gc_in_progress = 1;
    
    // 1. 将备用segment标记为垃圾回收中
    memset(&header, 0, sizeof(segment_header_t));
    header.status_magic = SEGMENT_MAGIC_GC;
    header.segment_id = (manager->backup_segment_base == FLASH_SEGMENT0_BASE) ? 0 : 1;
    header.gc_count = 0; // 垃圾回收计数将在完成后更新
    
    result = write_segment_header(manager->backup_segment_base, &header);
    if (result != FLASH_OK) {
        manager->gc_in_progress = 0;
        return result;
    }
    
    // 2. 复制有效数据
    result = copy_valid_pages(manager);
    if (result != FLASH_OK) {
        manager->gc_in_progress = 0;
        return result;
    }
    
    // 3. 擦除原激活segment
    result = erase_segment(manager->active_segment_base);
    if (result != FLASH_OK) {
        manager->gc_in_progress = 0;
        return result;
    }
    
    // 4. 将原激活segment标记为备用
    memset(&header, 0, sizeof(segment_header_t));
    header.status_magic = SEGMENT_MAGIC_BACKUP;
    header.segment_id = (manager->active_segment_base == FLASH_SEGMENT0_BASE) ? 0 : 1;
    header.gc_count = 0;
    
    result = write_segment_header(manager->active_segment_base, &header);
    if (result != FLASH_OK) {
        manager->gc_in_progress = 0;
        return result;
    }
    
    // 5. 读取当前激活segment的gc_count并递增

    result = read_segment_header(manager->active_segment_base, &active_header);
    new_gc_count = (result == FLASH_OK) ? active_header.gc_count + 1 : 1;
    
    // 将新segment标记为激活
    memset(&header, 0, sizeof(segment_header_t));
    header.status_magic = SEGMENT_MAGIC_ACTIVE;
    header.segment_id = (manager->backup_segment_base == FLASH_SEGMENT0_BASE) ? 0 : 1;
    header.gc_count = new_gc_count;
    
    result = write_segment_header(manager->backup_segment_base, &header);
    if (result != FLASH_OK) {
        manager->gc_in_progress = 0;
        return result;
    }
    
    // 6. 交换激活和备用segment
    temp = manager->active_segment_base;
    manager->active_segment_base = manager->backup_segment_base;
    manager->backup_segment_base = temp;
    
    manager->gc_in_progress = 0;
    return FLASH_OK;
}

/**
 * @brief 获取Flash状态信息
 */
flash_result_t flash_get_status(flash_manager_t* manager, uint32_t* used_pages, uint32_t* free_pages, uint32_t* data_count)
{
    if (manager == NULL) {
        return FLASH_ERROR_INVALID_PARAM;
    }
    
    if (used_pages != NULL) {
        *used_pages = manager->data_count + 1; // +1 for header page
    }
    
    if (free_pages != NULL) {
        *free_pages = FLASH_DATA_PAGES_PER_SEGMENT - manager->data_count;
    }
    
    if (data_count != NULL) {
        *data_count = manager->data_count;
    }
    
    return FLASH_OK;
}

/**
 * @brief 强制执行垃圾回收
 */
flash_result_t flash_force_garbage_collect(flash_manager_t* manager)
{
    return flash_garbage_collect(manager);
}
