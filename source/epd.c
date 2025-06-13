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
 ** @file epd.c
 **
 ** @brief Source file for epd functions
 **
 ** @author MADS Team 
 **
 ******************************************************************************/

/******************************************************************************
 * Include files
 ******************************************************************************/
#include <stdio.h>
#include <string.h>
#include "gpio.h"
#include "spi.h"
#include "epd.h"
#include "uart_interface.h"

/******************************************************************************
 * Local pre-processor symbols/macros ('#define')                            
 ******************************************************************************/
#define DC_H    Gpio_SetIO(0, 1, 1) //DC输出高
#define DC_L    Gpio_SetIO(0, 1, 0) //DC输出低
#define RST_H   Gpio_SetIO(0, 3, 1) //RST输出高
#define RST_L   Gpio_SetIO(0, 3, 0) //RST输出低

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
static void spiWriteCmd(uint8_t cmd)
{
    DC_L;
    Spi_SetCS(TRUE);
    Spi_SetCS(FALSE);
    Spi_SendData(cmd);
    Spi_SetCS(TRUE);
}

static void spiWriteData(uint8_t data)
{
    DC_H;
    Spi_SetCS(TRUE);
    Spi_SetCS(FALSE);
    Spi_SendData(data);
    Spi_SetCS(TRUE);
    DC_L;
}

static void spiInit(void)
{
	  stc_spi_config_t  SPIConfig;

    Clk_SetPeripheralGate(ClkPeripheralSpi,TRUE); //SPI外设时钟打开
    Gpio_InitIO(0, 3, GpioDirOut);
    Gpio_SetIO(0, 3, 1);               //RST输出高
    Gpio_InitIO(0, 1, GpioDirOut);
    Gpio_SetIO(0, 1, 0);               //DC输出高

    Gpio_SetFunc_SPI_CS_P02();
    // Gpio_SetFunc_SPICS_P14();
    Gpio_SetFunc_SPIMOSI_P24();
    Gpio_SetFunc_SPIMISO_P23();
    Gpio_SetFunc_SPICLK_P15();
    
    Spi_SetCS(TRUE);
    //配置SPI
    SPIConfig.bCPHA = Spicphafirst;
    SPIConfig.bCPOL = Spicpollow;
    SPIConfig.bIrqEn = FALSE;
    SPIConfig.bMasterMode = SpiMaster;
    SPIConfig.u8BaudRate = SpiClkDiv2;
    SPIConfig.pfnIrqCb = NULL;

    Spi_Init(&SPIConfig);

}

void EPD_UpdateGDEY042Z98ALL(void)
{   
  spiWriteCmd(0x22); //Display Update Control
  spiWriteData(0xF7);   
  spiWriteCmd(0x20); //Activate Display Update Sequence
  delay1ms(1000);

}

void EPD_UpdateGDEY042Z98ALL_fast(void)
{
  spiWriteCmd(0x22); //Display Update Control 
	spiWriteData(0xC7);   
  spiWriteCmd(0x20); //Activate Display Update Sequence
}

void EPD_WhiteScreenGDEY042Z98ALLBlack(void)
{
    unsigned int i;
	    unsigned int j;

	  //Write Data
		spiWriteCmd(0x24);	       //Transfer old data
	//   for(i=0;i<EPD_ARRAY;i++)	  
    // {	
	//     spiWriteData(datasBW[i]);  //Transfer the actual displayed data
    // }	
    DC_H;
    Spi_SetCS(TRUE);
    Spi_SetCS(FALSE);
    for (i = 0; i < 150; i++)
    {
        for (j = 0; j < 25; j++)
        {
            // Spi_SendData(data[i][j]);
            Spi_SendData(0xF0);
        }
        for (; j < 50; j++)
        {
            // Spi_SendData(data[i][j]);
            Spi_SendData(0xFF);
        }
    }

    for (; i < 300; i++)
    {
        for (j = 0; j < 50; j++)
        {
            // Spi_SendData(data[i][j]);
            Spi_SendData(0xFF);
        }
    }

    Spi_SetCS(TRUE);
    DC_L;

    delay1ms(2);

		spiWriteCmd(0x26);		     //Transfer new data
	//   for(i=0;i<EPD_ARRAY;i++)	     
	//   {
	//     spiWriteData(~datasRW[i]);  //Transfer the actual displayed data
	//   }
        DC_H;
    Spi_SetCS(TRUE);
    Spi_SetCS(FALSE);
    for (i = 0; i < 300; i++)
    {
        for (j = 0; j < 50; j++)
        {
            // Spi_SendData(data[i][j]);
            Spi_SendData(0x00);
        }
    }

    Spi_SetCS(TRUE);
    DC_L;
    delay1ms(2);

    EPD_UpdateGDEY042Z98ALL_fast();	    

}

void EPD_WhiteScreenGDEY042Z98ALLWrite(void)
{
    unsigned int i;
	    unsigned int j;

	  //Write Data
		spiWriteCmd(0x24);	       //Transfer old data
	//   for(i=0;i<EPD_ARRAY;i++)	  
    // {	
	//     spiWriteData(datasBW[i]);  //Transfer the actual displayed data
    // }	
    DC_H;
    Spi_SetCS(TRUE);
    Spi_SetCS(FALSE);
    for (i = 0; i < 300; i++)
    {
        for (j = 0; j < 50; j++)
        {
            // Spi_SendData(data[i][j]);
            Spi_SendData(0xff);
        }
    }

    Spi_SetCS(TRUE);
    DC_L;

    delay1ms(2);

		spiWriteCmd(0x26);		     //Transfer new data
	//   for(i=0;i<EPD_ARRAY;i++)	     
	//   {
	//     spiWriteData(~datasRW[i]);  //Transfer the actual displayed data
	//   }
        DC_H;
    Spi_SetCS(TRUE);
    Spi_SetCS(FALSE);
    for (i = 0; i < 300; i++)
    {
        for (j = 0; j < 50; j++)
        {
            // Spi_SendData(data[i][j]);
            Spi_SendData(0x00);
        }
    }

    Spi_SetCS(TRUE);
    DC_L;
    delay1ms(2);

    EPD_UpdateGDEY042Z98ALL_fast();	    

}

void EPD_WhiteScreenGDEY042Z98ALLRed(void)
{
    unsigned int i;
	    unsigned int j;

	  //Write Data
		spiWriteCmd(0x24);	       //Transfer old data
	//   for(i=0;i<EPD_ARRAY;i++)	  
    // {	
	//     spiWriteData(datasBW[i]);  //Transfer the actual displayed data
    // }	
    DC_H;
    Spi_SetCS(TRUE);
    Spi_SetCS(FALSE);

    for (j = 0; i < 300; i++)
    {
        for (j = 0; j < 50; j++)
        {
            // Spi_SendData(data[i][j]);
            Spi_SendData(0xFF);
        }
    }

    Spi_SetCS(TRUE);
    DC_L;

    delay1ms(2);

		spiWriteCmd(0x26);		     //Transfer new data
	//   for(i=0;i<EPD_ARRAY;i++)	     
	//   {
	//     spiWriteData(~datasRW[i]);  //Transfer the actual displayed data
	//   }
        DC_H;
    Spi_SetCS(TRUE);
    Spi_SetCS(FALSE);
    for (i = 0; i < 150; i++)
    {
        for (j = 0; j < 25; j++)
        {
            // Spi_SendData(data[i][j]);
            Spi_SendData(0xF0);
        }
        for (; j < 50; j++)
        {
            // Spi_SendData(data[i][j]);
            Spi_SendData(0x00);
        }
    }

    for (; i < 300; i++)
    {
        for (j = 0; j < 50; j++)
        {
            // Spi_SendData(data[i][j]);
            Spi_SendData(0x00);
        }
    }

    Spi_SetCS(TRUE);
    DC_L;
    delay1ms(2);

    EPD_UpdateGDEY042Z98ALL_fast();	    

}


void EPD_poweroff(void)
{
    spiWriteCmd(0x02);
    spiWriteCmd(0x07);
    spiWriteData(0xA5); 
}

void EPD_display(unsigned char data[][BYTES_PER_ROW])
{
    int i = 0,j = 0;
    spiWriteCmd(0x13);
    delay1ms(2);

    DC_H;
    Spi_SetCS(TRUE);
    Spi_SetCS(FALSE);
    for (i = 0; i < 152; i++)
    {
        for (j = 0; j < 19; j++)
        {
            Spi_SendData(data[i][j]);
        }
    }

    Spi_SetCS(TRUE);
    DC_L;
    delay1ms(2);

    spiWriteCmd(0x12);
}

void EPD_displaywft0420cz15(void)
{
    int i = 0,j = 0;
    spiWriteCmd(0x10);
    delay1ms(2);

    DC_H;
    Spi_SetCS(TRUE);
    Spi_SetCS(FALSE);
    for (i = 0; i < 300; i++)
    {
        for (j = 0; j < 50; j++)
        {
            // Spi_SendData(data[i][j]);
            Spi_SendData(0xFF);
        }
    }

    Spi_SetCS(TRUE);
    DC_L;
    delay1ms(2);

    spiWriteCmd(0x13);
    delay1ms(2);

    DC_H;
    Spi_SetCS(TRUE);
    Spi_SetCS(FALSE);
    for (i = 0; i < 300; i++)
    {
        for (j = 0; j < 50; j++)
        {
            // Spi_SendData(data[i][j]);
            Spi_SendData(0x00);
        }
    }

    Spi_SetCS(TRUE);
    DC_L;
    delay1ms(2);

    spiWriteCmd(0x12);
    // spiWriteData(0xF7);
    // spiWriteCmd(0x20);
}


void EPD_initGDEY042Z98(void)
{
//    int i = 0,j = 0;

    spiInit();
	RST_L;  // Module reset   
	delay1ms(10);//At least 10ms delay 
	RST_H;
	delay1ms(10); //At least 10ms delay 
	delay1ms(1000);

	spiWriteCmd(0x12);  //SWRESET
	delay1ms(1000);
		
		// spiWriteCmd(0x74);
		// spiWriteData(0x54);
		// spiWriteCmd(0x7E);
		// spiWriteData(0x3B);
		// spiWriteCmd(0x2B);  // Reduce glitch under ACVCOM	
		// spiWriteData(0x04);           
		// spiWriteData(0x63);
		// spiWriteCmd(0x03);  // Soft start setting
		// spiWriteData(0x03);           

		// spiWriteCmd(0x04);  // Soft start setting
		// spiWriteData(0x41);           
		// spiWriteData(0xA8);           
		// spiWriteData(0x12);           

		spiWriteCmd(0x0C);  // Soft start setting
		spiWriteData(0x8B);           
		spiWriteData(0x8C);
		spiWriteData(0x96);
		spiWriteData(0x0F);

		spiWriteCmd(0x01);  // Set MUX as 300
		spiWriteData(0x2B);           
		spiWriteData(0x01);
		spiWriteData(0x01);     

		spiWriteCmd(0x11);  // Data entry mode
		spiWriteData(0x01);   
		
		spiWriteCmd(0x44); 
		spiWriteData(0x00); // RAM x address start at 0
		spiWriteData(0x31); // RAM x address end at 31h(49+1)*8->400
		spiWriteCmd(0x45); 
		spiWriteData(0x2B);   // RAM y address start at 12Bh     
		spiWriteData(0x01);
		spiWriteData(0x00); // RAM y address end at 00h     
		spiWriteData(0x00);
		spiWriteCmd(0x3C); // board
		spiWriteData(0x01); // HIZ
		spiWriteCmd(0x18);
		spiWriteData(0X80);

        spiWriteCmd(0x1A); // Write to temperature register
        spiWriteData(0x5A);		// 90
        // spiWriteData(0x00);	
	    spiWriteCmd(0x22); // Load temperature value
	    spiWriteData(0x91);		
        spiWriteCmd(0x20);	
	    delay1ms(500);
		
		// spiWriteCmd(0x4E); 
		// spiWriteData(0x00);
		// spiWriteCmd(0x4F); 
		// spiWriteData(0x2B);
		// spiWriteData(0x01);
	// delay1ms(1000);

}

void EPD_WhiteScreenGDEY042Z98ALL(unsigned char data[][BYTES_PER_ROW])
{
    unsigned int i;
	    unsigned int j;

	  //Write Data
		spiWriteCmd(0x24);	       //Transfer old data
	//   for(i=0;i<EPD_ARRAY;i++)	  
    // {	
	//     spiWriteData(datasBW[i]);  //Transfer the actual displayed data
    // }	
    DC_H;
    Spi_SetCS(TRUE);
    Spi_SetCS(FALSE);
    for (i = 0; i < 300; i++)
    {
        for (j = 0; j < 50; j++)
        {
            // Spi_SendData(data[i][j]);
            Spi_SendData(0xFF);
        }
    }

    Spi_SetCS(TRUE);
    DC_L;

    delay1ms(2);

		spiWriteCmd(0x26);		     //Transfer new data
	//   for(i=0;i<EPD_ARRAY;i++)	     
	//   {
	//     spiWriteData(~datasRW[i]);  //Transfer the actual displayed data
	//   }
        DC_H;
    Spi_SetCS(TRUE);
    Spi_SetCS(FALSE);
    for (i = 0; i < 150; i++)
    {
        for (j = 0; j < 20; j++)
        {
            Spi_SendData(data[i][j]);
            // Spi_SendData(0x0F);
        }
        for (; j < 50; j++)
        {
            // Spi_SendData(data[i][j]);
            Spi_SendData(0x00);
        }
    }
    for (; i < 300; i++)
    {
        for (j = 0; j < 50; j++)
        {
            // Spi_SendData(data[i][j]);
            Spi_SendData(0x00);
        }
    }

    Spi_SetCS(TRUE);
    DC_L;
    delay1ms(2);

    EPD_UpdateGDEY042Z98ALL_fast();	    

}

/******************************************************************************
 * EOF (not truncated)
 ******************************************************************************/


