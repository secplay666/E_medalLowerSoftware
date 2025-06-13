
 void E104_ioInit(void);

boolean_t E104_getLinkState(void); // 返回 TRUE 链接成功； FALSE 链接断开
// boolean_t E104_getDataState(void); // 返回 TRUE 数据传输中； FALSE 无数据
void E104_setSleepMode(void);

void E104_setWakeUpMode(void);

// void E104_setTransmitMode(void);

// void E104_setConfigMode(void);

// void E104_setDisconnect(void);

void E104_executeCommand(void);

