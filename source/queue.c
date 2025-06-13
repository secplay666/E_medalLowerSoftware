/******************************************************************************
 * Copyright (C) 2021, 
 *
 *  
 *  
 *  
 *  
 *
 ******************************************************************************/
 
/******************************************************************************
 ** @file queue.c
 **
 ** @brief Source file for draw functions
 **
 ** @author MADS Team 
 **
 ******************************************************************************/

/******************************************************************************
 * Include files
 ******************************************************************************/
#include "base_types.h"
#include "queue.h"

/******************************************************************************
 * Local pre-processor symbols/macros ('#define')                            
 ******************************************************************************/

/******************************************************************************
 * Global variable definitions (declared in header file with 'extern')
 ******************************************************************************/

/******************************************************************************
 * Local type definitions ('typedef')                                         
 ******************************************************************************/

/******************************************************************************
 * Local function prototypes ('static')
 ******************************************************************************/

/******************************************************************************
 * Local variable definitions ('static')                                      *
 ******************************************************************************/


// 初始化队列
void Queue_Init(Queue *q) {
    q->head = 0;
    q->tail = 0;
    q->size = 0;
    q->queue_access = false;  // 初始化时，队列不被访问
}

// 判断队列是否为空
bool Queue_IsEmpty(Queue *q) {
    return q->size == 0;
}

// 判断队列是否已满
bool Queue_IsFull(Queue *q) {
    return q->size == QUEUE_SIZE;
}

// 将数据写入队列
bool Queue_Enqueue(Queue *q, uint8_t data) {
    // 如果队列正在被访问，直接返回失败
    if (q->queue_access) {
        return false;
    }

    // 设置队列为被访问状态
    q->queue_access = true;

    if (Queue_IsFull(q)) {
        q->queue_access = false;  // 操作完成后释放访问锁
        return false;  // 队列满了，无法写入
    }

    q->buffer[q->tail] = data;
    q->tail = (q->tail + 1) % QUEUE_SIZE;  // 环形队列
    q->size++;

    q->queue_access = false;  // 操作完成后释放访问锁
    return true;
}

// 从队列中读取数据
bool Queue_Dequeue(Queue *q, uint8_t *data) {
    // 如果队列正在被访问，直接返回失败
    if (q->queue_access) {
        return false;
    }

    // 设置队列为被访问状态
    q->queue_access = true;

    if (Queue_IsEmpty(q)) {
        q->queue_access = false;  // 操作完成后释放访问锁
        return false;  // 队列为空，无法读取
    }

    *data = q->buffer[q->head];
    q->head = (q->head + 1) % QUEUE_SIZE;  // 环形队列
    q->size--;

    q->queue_access = false;  // 操作完成后释放访问锁
    return true;
}

// 从队列尾部输出数据
bool Queue_DequeueTail(Queue *q, uint8_t *data) {
    uint16_t tailIndex;
    // 如果队列正在被访问，直接返回失败
    if (q->queue_access) {
        return false;
    }

    // 设置队列为被访问状态
    q->queue_access = true;

    if (Queue_IsEmpty(q)) {
        q->queue_access = false;  // 操作完成后释放访问锁
        return false;  // 如果队列为空，返回false
    }

    // 计算队列尾部的位置
    tailIndex = (q->tail - 1 + QUEUE_SIZE) % QUEUE_SIZE;
    *data = q->buffer[tailIndex];

    // 更新队列尾部
    q->tail = tailIndex;
    q->size--;

    q->queue_access = false;  // 操作完成后释放访问锁
    return true;
}

// 将整个字符串写入队列
bool Queue_EnqueueString(Queue *q, const char *str) {
    if (str == NULL) {
        return false;  // 空字符串指针，返回失败
    }

    // 遍历字符串，将每个字符写入队列
    while (*str != '\0') {
        if (!Queue_Enqueue(q, (uint8_t)*str)) {
            return false;  // 如果队列满了，返回失败
        }
        str++;  // 移动到下一个字符
    }

    return true;  // 字符串成功写入队列
}
