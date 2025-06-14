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
    flash_result_t result;
    uint8_t test_data[240];
    uint8_t read_buffer[240];
    uint32_t read_size;
    uint32_t used_pages, free_pages, data_count;
    uint32_t i;
    
    UARTIF_uartPrintf(0, "\n=== Flash Manager Test Start ===\n");
    
    // 测试1：写入和读取小数据
    UARTIF_uartPrintf(0, "Test 1: Small data write/read\n");
    strcpy((char*)test_data, "Small test data");
    result = flash_write_data(&g_flash_manager, 0x2001, test_data, strlen((char*)test_data) + 1);
    if (result == FLASH_OK) {
        UARTIF_uartPrintf(0, "Small data written successfully\n");
        
        read_size = sizeof(read_buffer);
        result = flash_read_data(&g_flash_manager, 0x2001, read_buffer, &read_size);
        if (result == FLASH_OK) {
            UARTIF_uartPrintf(0, "Small data read: %s\n", read_buffer);
        } else {
            UARTIF_uartPrintf(0, "Failed to read small data: %d\n", result);
        }
    } else {
        UARTIF_uartPrintf(0, "Failed to write small data: %d\n", result);
    }
    
    // 测试2：写入和读取大数据
    UARTIF_uartPrintf(0, "Test 2: Large data write/read\n");
    for (i = 0; i < 200; i++) {
        test_data[i] = (uint8_t)(i & 0xFF);
    }
    result = flash_write_data(&g_flash_manager, 0x2002, test_data, 200);
    if (result == FLASH_OK) {
        UARTIF_uartPrintf(0, "Large data written successfully\n");
        
        read_size = sizeof(read_buffer);
        result = flash_read_data(&g_flash_manager, 0x2002, read_buffer, &read_size);
        if (result == FLASH_OK) {
            UARTIF_uartPrintf(0, "Large data read successfully, size: %d\n", read_size);
            // 验证数据
            boolean_t data_ok = TRUE;
            for (i = 0; i < 200; i++) {
                if (read_buffer[i] != (uint8_t)(i & 0xFF)) {
                    data_ok = FALSE;
                    break;
                }
            }
            UARTIF_uartPrintf(0, "Data verification: %s\n", data_ok ? "PASS" : "FAIL");
        } else {
            UARTIF_uartPrintf(0, "Failed to read large data: %d\n", result);
        }
    } else {
        UARTIF_uartPrintf(0, "Failed to write large data: %d\n", result);
    }
    
    // 测试3：数据覆盖
    UARTIF_uartPrintf(0, "Test 3: Data overwrite\n");
    strcpy((char*)test_data, "Updated data");
    result = flash_write_data(&g_flash_manager, 0x2001, test_data, strlen((char*)test_data) + 1);
    if (result == FLASH_OK) {
        UARTIF_uartPrintf(0, "Data overwritten successfully\n");
        
        read_size = sizeof(read_buffer);
        result = flash_read_data(&g_flash_manager, 0x2001, read_buffer, &read_size);
        if (result == FLASH_OK) {
            UARTIF_uartPrintf(0, "Updated data read: %s\n", read_buffer);
        }
    } else {
        UARTIF_uartPrintf(0, "Failed to overwrite data: %d\n", result);
    }
    
    // 测试4：数据删除
    UARTIF_uartPrintf(0, "Test 4: Data deletion\n");
    result = flash_delete_data(&g_flash_manager, 0x2002);
    if (result == FLASH_OK) {
        UARTIF_uartPrintf(0, "Data deleted successfully\n");
        
        read_size = sizeof(read_buffer);
        result = flash_read_data(&g_flash_manager, 0x2002, read_buffer, &read_size);
        if (result == FLASH_ERROR_NOT_FOUND) {
            UARTIF_uartPrintf(0, "Data deletion verified\n");
        } else {
            UARTIF_uartPrintf(0, "Data deletion failed, still readable\n");
        }
    } else {
        UARTIF_uartPrintf(0, "Failed to delete data: %d\n", result);
    }
    
    // 测试5：批量写入测试
    UARTIF_uartPrintf(0, "Test 5: Batch write test\n");
    for (i = 0x3000; i < 0x3010; i++) {
        sprintf((char*)test_data, "Data ID: 0x%04X", i);
        result = flash_write_data(&g_flash_manager, i, test_data, strlen((char*)test_data) + 1);
        if (result != FLASH_OK) {
            UARTIF_uartPrintf(0, "Failed to write data ID 0x%04X: %d\n", i, result);
            break;
        }
    }
    if (i >= 0x3010) {
        UARTIF_uartPrintf(0, "Batch write completed successfully\n");
    }
    
    // 获取最终状态
    result = flash_get_status(&g_flash_manager, &used_pages, &free_pages, &data_count);
    if (result == FLASH_OK) {
        UARTIF_uartPrintf(0, "Final Status: Used=%d, Free=%d, Data=%d\n", used_pages, free_pages, data_count);
    }
    
    UARTIF_uartPrintf(0, "=== Flash Manager Test End ===\n\n");
}

/**
 * @brief 垃圾回收测试
 */
void flash_gc_test(void)
{
    flash_result_t result;
    uint8_t test_data[240];
    uint32_t used_pages, free_pages, data_count;
    uint32_t i;
    
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
        uint8_t read_buffer[240];
        uint32_t read_size = sizeof(read_buffer);
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