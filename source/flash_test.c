#include "flash_manager.h"
#include "uart_interface.h"
#include <string.h>

// 全局Flash管理器实例
extern flash_manager_t g_flash_manager;

/**
 * @brief Flash管理器功能测试
 */
void flash_manager_test(void)
{
    // 1. 基本写入测试
    UARTIF_uartPrintf(0, "\n=== Flash Manager Test ===\n");
    
    uint8_t test_data[] = "Test data for flash manager";
    uint8_t read_buffer[256];
    uint8_t read_size;
    flash_result_t result;
    
    UARTIF_uartPrintf(0, "1. Basic write test...\n");
    result = flash_write_data(&g_flash_manager, 0x2001, test_data, (uint8_t)(strlen((char*)test_data) + 1));
    if (result == FLASH_OK) {
        UARTIF_uartPrintf(0, "   Write success!\n");
        
        read_size = sizeof(read_buffer);
        result = flash_read_data(&g_flash_manager, 0x2001, read_buffer, &read_size);
        if (result == FLASH_OK) {
            UARTIF_uartPrintf(0, "   Read success: %s (size: %d)\n", read_buffer, read_size);
        } else {
            UARTIF_uartPrintf(0, "   Read failed: %d\n", result);
        }
    } else {
        UARTIF_uartPrintf(0, "   Write failed: %d\n", result);
    }
    
    // 2. 大数据写入测试
    UARTIF_uartPrintf(0, "\n2. Large data write test...\n");
    uint8_t large_data[200];
    memset(large_data, 0xAA, sizeof(large_data));
    
    result = flash_write_data(&g_flash_manager, 0x2002, large_data, 200);
    if (result == FLASH_OK) {
        UARTIF_uartPrintf(0, "   Large write success!\n");
        
        read_size = sizeof(read_buffer);
        result = flash_read_data(&g_flash_manager, 0x2002, read_buffer, &read_size);
        if (result == FLASH_OK) {
            UARTIF_uartPrintf(0, "   Large read success (size: %d)\n", read_size);
            // 验证数据
            uint8_t verify_ok = 1;
            uint16_t i;
            for (i = 0; i < read_size; i++) {
                if (read_buffer[i] != 0xAA) {
                    verify_ok = 0;
                    break;
                }
            }
            UARTIF_uartPrintf(0, "   Data verification: %s\n", verify_ok ? "PASS" : "FAIL");
        } else {
            UARTIF_uartPrintf(0, "   Large read failed: %d\n", result);
        }
    } else {
        UARTIF_uartPrintf(0, "   Large write failed: %d\n", result);
    }
    
    // 3. 数据覆盖测试
    UARTIF_uartPrintf(0, "\n3. Data overwrite test...\n");
    uint8_t new_data[] = "Updated test data";
    
    result = flash_write_data(&g_flash_manager, 0x2001, new_data, (uint8_t)(strlen((char*)new_data) + 1));
    if (result == FLASH_OK) {
        UARTIF_uartPrintf(0, "   Overwrite success!\n");
        
        read_size = sizeof(read_buffer);
        result = flash_read_data(&g_flash_manager, 0x2001, read_buffer, &read_size);
        if (result == FLASH_OK) {
            UARTIF_uartPrintf(0, "   Read after overwrite: %s\n", read_buffer);
        } else {
            UARTIF_uartPrintf(0, "   Read after overwrite failed: %d\n", result);
        }
    } else {
        UARTIF_uartPrintf(0, "   Overwrite failed: %d\n", result);
    }
    
    // 4. 数据删除测试
    UARTIF_uartPrintf(0, "\n4. Data delete test...\n");
    result = flash_delete_data(&g_flash_manager, 0x2002);
    if (result == FLASH_OK) {
        UARTIF_uartPrintf(0, "   Delete success!\n");
        
        // 尝试读取已删除的数据
        read_size = sizeof(read_buffer);
        result = flash_read_data(&g_flash_manager, 0x2002, read_buffer, &read_size);
        if (result == FLASH_ERROR_NOT_FOUND) {
            UARTIF_uartPrintf(0, "   Deleted data not found (correct)\n");
        } else {
            UARTIF_uartPrintf(0, "   ERROR: Deleted data still readable!\n");
        }
    } else {
        UARTIF_uartPrintf(0, "   Delete failed: %d\n", result);
    }
    
    // 5. 批量写入测试
    UARTIF_uartPrintf(0, "\n5. Batch write test...\n");
    uint8_t batch_data[] = "Batch test data";
    for (i = 0x3001; i <= 0x3005; i++) {
        result = flash_write_data(&g_flash_manager, i, batch_data, (uint8_t)(strlen((char*)batch_data) + 1));
        if (result == FLASH_OK) {
            UARTIF_uartPrintf(0, "   Batch write ID 0x%04X: success\n", i);
        } else {
            UARTIF_uartPrintf(0, "   Batch write ID 0x%04X: failed (%d)\n", i, result);
        }
    }
    
    // 6. 状态查询测试
    UARTIF_uartPrintf(0, "\n6. Status query test...\n");
    uint32_t used_pages, free_pages, data_count;
    result = flash_get_status(&g_flash_manager, &used_pages, &free_pages, &data_count);
    if (result == FLASH_OK) {
        UARTIF_uartPrintf(0, "   Used pages: %d\n", used_pages);
        UARTIF_uartPrintf(0, "   Free pages: %d\n", free_pages);
        UARTIF_uartPrintf(0, "   Data count: %d\n", data_count);
    } else {
        UARTIF_uartPrintf(0, "   Status query failed: %d\n", result);
    }
    
    UARTIF_uartPrintf(0, "\n=== Flash Manager Test Complete ===\n");
}

/**
 * @brief 垃圾回收测试
 */
void flash_gc_test(void)
{
    flash_result_t result;
    uint32_t used_pages, free_pages, data_count;
    
    UARTIF_uartPrintf(0, "\n=== Garbage Collection Test Start ===\n");
    
    // 获取当前状态
    result = flash_get_status(&g_flash_manager, &used_pages, &free_pages, &data_count);
    if (result == FLASH_OK) {
        UARTIF_uartPrintf(0, "Before GC: Used=%d, Free=%d, Data=%d\n", used_pages, free_pages, data_count);
    }
    
    // 执行垃圾回收
    UARTIF_uartPrintf(0, "Starting garbage collection...\n");
    result = flash_force_garbage_collect(&g_flash_manager);
    if (result == FLASH_OK) {
        UARTIF_uartPrintf(0, "Garbage collection completed successfully\n");
        
        // 获取GC后状态
        result = flash_get_status(&g_flash_manager, &used_pages, &free_pages, &data_count);
        if (result == FLASH_OK) {
            UARTIF_uartPrintf(0, "After GC: Used=%d, Free=%d, Data=%d\n", used_pages, free_pages, data_count);
        }
        
        // 验证数据是否仍然可读
        UARTIF_uartPrintf(0, "Verifying data after GC...\n");
        uint8_t read_buffer[248];
        uint8_t read_size = sizeof(read_buffer);
        result = flash_read_data(&g_flash_manager, 0x2001, read_buffer, &read_size);
        if (result == FLASH_OK) {
            UARTIF_uartPrintf(0, "Data verification after GC: %s\n", read_buffer);
        } else {
            UARTIF_uartPrintf(0, "Data lost after GC: %d\n", result);
        }
        
    } else {
        UARTIF_uartPrintf(0, "Garbage collection failed: %d\n", result);
    }
    
    UARTIF_uartPrintf(0, "=== Garbage Collection Test End ===\n\n");
}