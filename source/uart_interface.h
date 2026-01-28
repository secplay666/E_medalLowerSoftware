#ifndef UART_INTERFACE_H
#define UART_INTERFACE_H

#include <stdint.h>
#include "base_types.h"

extern volatile int8_t currentImageSlot;

void UARTIF_uartPrintf(uint8_t uartNumber, const char *format, ...);
void UARTIF_uartPrintfFloat(uint8_t uartNumber, const char *head, const float data);
void UARTIF_uartInit(void);
void UARTIF_lpuartInit(void);
void UARTIF_passThrough(void);
uint8_t UARTIF_passThroughCmd(void);
uint16_t UARTIF_fetchDataFromUart(uint8_t *buf, uint16_t *idx);
void UARTIF_getUartStats(uint32_t *rxCount, uint32_t *overflowCount);
void UARTIF_resetUartStats(void);

// Queue status helpers (expose internal queues safely)
boolean_t UARTIF_isUartRecEmpty(void);
boolean_t UARTIF_isLpUartRecEmpty(void);
// Transfer/display activity query: TRUE when receiving pages or actively displaying
boolean_t UARTIF_isTransferActive(void);


#endif // UART_INTERFACE_H
