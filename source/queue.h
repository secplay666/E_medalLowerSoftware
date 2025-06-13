#ifndef QUEUE_H
#define QUEUE_H

#include <stdint.h>
#include <stdbool.h>

#define QUEUE_SIZE 128  // 队列的最大大小

// 队列结构定义
typedef struct {
    uint8_t buffer[QUEUE_SIZE];  // 存储数据的缓冲区
    uint16_t head;               // 队列头部
    uint16_t tail;               // 队列尾部
    uint16_t size;               // 当前队列中的数据个数
    bool queue_access;           // 访问标志，防止并发读写
} Queue;

// 函数声明

// 初始化队列
void Queue_Init(Queue *q);

// 判断队列是否为空
bool Queue_IsEmpty(Queue *q);

// 判断队列是否已满
bool Queue_IsFull(Queue *q);

// 将数据写入队列
bool Queue_Enqueue(Queue *q, uint8_t data);

// 从队列中读取数据
bool Queue_Dequeue(Queue *q, uint8_t *data);

// 从队列尾部读取数据
bool Queue_DequeueTail(Queue *q, uint8_t *data);

// 将整个字符串写入队列
bool Queue_EnqueueString(Queue *q, const char *str);


#endif // QUEUE_H
