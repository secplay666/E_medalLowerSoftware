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
 ** @file e104.c
 **
 ** @brief Source file for e104 functions
 **
 ** @author MADS Team 
 **
 ******************************************************************************/

/******************************************************************************
 * Include files
 ******************************************************************************/
#include "gpio.h"
#include "uart_interface.h"

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

/******************************************************************************
 * Local pre-processor symbols/macros ('#define')                             
 ******************************************************************************/

/*****************************************************************************
 * Function implementation - global ('extern') and local ('static')
 ******************************************************************************/

void E104_ioInit(void)
{
    Gpio_InitIO(3, 1, GpioDirIn);  // Link
    Gpio_InitIOExt(3,2,GpioDirOut,TRUE,FALSE,FALSE,FALSE);   // Wake up
    // Gpio_InitIO(0, 2, GpioDirIn);   // Data
    // Gpio_InitIOExt(2,7,GpioDirOut,TRUE,FALSE,FALSE,FALSE);   // Mod
    // Gpio_InitIOExt(3,2,GpioDirOut,TRUE,FALSE,FALSE,FALSE);   // Disc
    Gpio_InitIOExt(2,7,GpioDirOut,TRUE,FALSE,FALSE,FALSE);   // reset

    Gpio_SetIO(3,2, FALSE);  // 设置唤醒
    Gpio_SetIO(2,7, TRUE);  // 拉高reset
    delay1ms(50);
    Gpio_SetIO(2,7, FALSE);
    delay1ms(50);
    Gpio_SetIO(2,7, TRUE);
    // Gpio_SetIO(3,2, TRUE);  // 

}

boolean_t E104_getLinkState(void) // 返回 TRUE 链接成功； FALSE 链接断开
{
    static boolean_t preState = TRUE;
    static boolean_t currentState = TRUE;
    currentState = Gpio_GetIO(3,1);  
    if ((currentState == FALSE) && (preState == TRUE))
    {
        UARTIF_uartPrintf(0, "Link start! \n");
        // Gpio_SetIO(3,2, TRUE);  // 
    }
    else if ((currentState == TRUE) && (preState == FALSE))
    {
        UARTIF_uartPrintf(0, "Link end! \n");
    }
    else
    {
        // do nothing
    }
    preState = currentState;
    return !currentState;
}

// boolean_t E104_getDataState(void) // 返回 TRUE 数据传输中； FALSE 无数据
// {
//     static boolean_t preState = TRUE;
//     static boolean_t currentState = TRUE;
//     currentState = Gpio_GetIO(0,2);  
//     if ((currentState == FALSE) && (preState == TRUE))
//     {
//         UARTIF_uartPrintf(0, "Data on comming! \n");
//     }
//     else if ((currentState == TRUE) && (preState == FALSE))
//     {
//         UARTIF_uartPrintf(0, "Data end! \n");
//     }
//     else
//     {
//         // do nothing
//     }
//     preState = currentState;
//     return !currentState;
// }

void E104_setSleepMode(void)
{
    Gpio_SetIO(3,2, TRUE);
    UARTIF_uartPrintf(0, "Set sleep mode! \n");
}

void E104_setWakeUpMode(void)
{
    Gpio_SetIO(3,2, FALSE);
    UARTIF_uartPrintf(0, "Set wake up mode! \n");

}

// void E104_setTransmitMode(void)
// {
//     Gpio_SetIO(2,7, TRUE);
//     UARTIF_uartPrintf(0, "Set transmit mode! \n");

// }

// void E104_setConfigMode(void)
// {
//     Gpio_SetIO(2,7, FALSE);
//     UARTIF_uartPrintf(0, "Set config mode! \n");

// }

// void E104_setDisconnect(void)
// {
//     Gpio_SetIO(3,2, FALSE);
//     UARTIF_uartPrintf(0, "Set disconnect! \n");
// }

void E104_executeCommand(void)
{
    switch (UARTIF_passThroughCmd())
    {
        case '0':
        E104_setSleepMode();
        break;
        case '1':
        E104_setWakeUpMode();
        break;
        case '2':
//        E104_setTransmitMode();
//        break;
//        case '3':
//        E104_setConfigMode();
//        break;
//        case '4':
//        E104_setDisconnect();
//        break;
        default:

        break;
    }

}

/******************************************************************************
 * EOF (not truncated)
 ******************************************************************************/


