/******************************************************************************
 * Copyright (C) 2021, Xiaohua Semiconductor Co., Ltd. All rights reserved.
 *
 * This software component is licensed by XHSC under BSD 3-Clause license
 * (the "License"); You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *                    opensource.org/licenses/BSD-3-Clause
 *
 ******************************************************************************/
 
/******************************************************************************
** @file system_hc32l110c6ua.c
**
** @brief Headerfile for SYSTEM functions
**
** @author MADS Team
**
** - 2022-01-26.
**
******************************************************************************/

/******************************************************************************/
/* Include files                                                              */
/******************************************************************************/
#include "base_types.h"
#include "hc32l110.h"
#include "system_hc32l110.h"
#include "clk.h"


/**
 ******************************************************************************
 ** System Clock Frequency (Core Clock) Variable according CMSIS
 ******************************************************************************/
volatile uint32_t SystemCoreClock = 24000000;


//add clock source.
void SystemCoreClockUpdate (void) // Update SystemCoreClock variable
{
   SystemCoreClock = Clk_GetHClkFreq();

}

/**
 ******************************************************************************
 ** \brief  Setup the microcontroller system. Initialize the System and update
 ** the SystemCoreClock variable.
 **
 ** \param  none
 ** \return none
 ******************************************************************************/
void SystemInit(void)
{
    // TODO load trim from flash
    //hcr 4MHz manual trim.
	  stc_clk_systickcfg_t stcCfg;

    // M0P_CLOCK->RCH_CR_f.TRIM = (*((volatile uint16_t*) (0x00100C08ul)));    //Loader 4MHz Trimming value                                            
    // Clk_SwitchTo(ClkRCL);
    Clk_SetRCHFreq(ClkFreq24Mhz);
    Clk_Enable(ClkRCH, TRUE);
    // Clk_SwitchTo(ClkRCH);
    SystemCoreClockUpdate();
    DDL_ZERO_STRUCT(stcCfg);
    stcCfg.enClk = ClkRCH;
    stcCfg.bNoRef = TRUE;
    stcCfg.u32LoadVal = 0xFFFFFF;
    Clk_SysTickConfig(&stcCfg);
}


