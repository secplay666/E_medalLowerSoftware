void UARTIF_uartPrintf(uint8_t uartNumber, const char *format, ...);
void UARTIF_uartPrintfFloat(uint8_t uartNumber, const char *head, const float data);
void UARTIF_uartInit(void);
void UARTIF_lpuartInit(void);
void UARTIF_passThrough(void);
uint8_t UARTIF_passThroughCmd(void);
