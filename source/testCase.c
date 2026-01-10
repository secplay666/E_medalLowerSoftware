/******************************************************************************
 * Include files
 ******************************************************************************/
#include <stdio.h>

#include "uart_interface.h"

#include "w25q32.h"
#include "flash_manager.h"
#include <stdlib.h>
#include "ddl.h"

uint8_t buffer[256];

#ifdef 0
uint8_t testData[16] = {0};
uint8_t readData[16] = {0};

void TEST_FlashManagerReadAndWrite(void)
{
    // uint8_t testData[16] = {0};
    // uint8_t readData[16] = {0};
    uint8_t i = 0;
    flash_result_t result;

    for (i = 0;i<16;i++)
    {
        testData[i] = rand()%256;
        UARTIF_uartPrintf(0, "Write byte %d is 0x%x! \n",i,testData[i]);
    }

    result = FM_writeData(DATA_PAGE_MAGIC, 0, testData, 16);
    if (result == FLASH_OK)
    {
        UARTIF_uartPrintf(0, "Flash write data id 0 success! \n");
    }
    else 
    {
        UARTIF_uartPrintf(0, "Flash write data id 0 fail! error code is %d \n",result);
    }

    result = FM_readData(DATA_PAGE_MAGIC, 0, readData, 16);
    if (result == FLASH_OK)
    {
        UARTIF_uartPrintf(0, "Flash read data id 0 success! \n");
        for (i = 0;i<16;i++)
        {
            UARTIF_uartPrintf(0, "Read byte %d is 0x%x! \n",i,readData[i]);
        }
    }
    else 
    {
        UARTIF_uartPrintf(0, "Flash read data id 0 fail! error code is %d \n",result);
    }
}

void TEST_FlashManagerReadAndWrite2(void)
{
    uint8_t i = 0, j = 0;
    flash_result_t result;

    for (j = 0;j<8;j++)
    {
        testData[0] = j;
        testData[1] = 0xff;
        for (i = 2;i<16;i++)
        {
            // testData[i] = rand()%256;
            testData[i] = i;
            UARTIF_uartPrintf(0, "Write byte %d is 0x%x! \n",i,testData[i]);
        }

        result = FM_writeData(DATA_PAGE_MAGIC,j, testData, 16);
        if (result == FLASH_OK)
        {
            UARTIF_uartPrintf(0, "Flash write data id %d success! \n", j);
        }
        else 
        {
            UARTIF_uartPrintf(0, "Flash write data id %d fail! error code is %d \n", j, result);
            break;
        }
    }

    for (j = 0;j<8;j++)
    {
        result = FM_readData(DATA_PAGE_MAGIC, j, readData, 16);
        if (result == FLASH_OK)
        {
            UARTIF_uartPrintf(0, "Flash read data id %d success! \n", j);
            for (i = 0;i<16;i++)
            {
                UARTIF_uartPrintf(0, "Read byte %d is 0x%x! \n",i,readData[i]);
            }
        }
        else 
        {
            UARTIF_uartPrintf(0, "Flash read data id %d fail! error code is %d \n", j, result);
        }
    }
}

void TEST_FlashManagerRead(void)
{
    // uint8_t readData[16] = {0};
    uint8_t i = 0;
    flash_result_t result;


    result = FM_readData(DATA_PAGE_MAGIC, 0, readData, 16);
    if (result == FLASH_OK)
    {
        UARTIF_uartPrintf(0, "Flash read data id 0 success! \n");
        for (i = 0;i<16;i++)
        {
            UARTIF_uartPrintf(0, "Read byte %d is 0x%x! \n",i,readData[i]);
        }
    }
    else 
    {
        UARTIF_uartPrintf(0, "Flash read data id 0 fail! error code is %d \n",result);
    }
}

void TEST_FlashManagerRead2(void)
{
        uint8_t i = 0, j = 0;
    flash_result_t result;

    for (j = 0;j<8;j++)
    {
        result = FM_readData(DATA_PAGE_MAGIC, j, readData, 16);
        if (result == FLASH_OK)
        {
            UARTIF_uartPrintf(0, "Flash read data id %d success! \n", j);
            for (i = 0;i<16;i++)
            {
                UARTIF_uartPrintf(0, "Read byte %d is 0x%x! \n",i,readData[i]);
            }
        }
        else 
        {
            UARTIF_uartPrintf(0, "Flash read data id %d fail! error code is %d \n", j, result);
        }
    }

}

void TEST_FlashManagerGarbageCollection(void)
{
    // uint8_t testData[16] = {0};
    // uint8_t readData[16] = {0};
    uint16_t i = 0;
    flash_result_t result;
    // volatile uint8_t randomNumber = 0;

    for (i = 0;i<0x1E91;i++)
    {
        // testData[0] = rand()%256;
        // testData[1] = rand()%256;
        // testData[2] = rand()%256;
        // testData[3] = rand()%256;
        testData[0] = (uint8_t)((i & 0xff00) >> 8);
        testData[1] = (uint8_t)(i & 0x00ff);
        testData[2] = 2;
        testData[3] = 3;
        // delay1ms(3);

        UARTIF_uartPrintf(0,"i is %d\n", i);
        UARTIF_uartPrintf(0, "Writing data to flash: 0x%x 0x%x 0x%x 0x%x\n", testData[0], testData[1], testData[2], testData[3]);
        result = FM_writeData(DATA_PAGE_MAGIC, 0, testData, 16);
        if (result == FLASH_OK)
        {
            UARTIF_uartPrintf(0, "Flash write data id 0 success! \n");
        }
        else 
        {
            UARTIF_uartPrintf(0, "Flash write data id 0 fail! error code is %d \n",result);
            break;
        }
        delay1ms(5);
    }

    result = FM_readData(DATA_PAGE_MAGIC, 0, readData, 16);
    if (result == FLASH_OK)
    {
        UARTIF_uartPrintf(0, "Flash read data id 0 success! \n");
        for (i = 0;i<16;i++)
        {
            UARTIF_uartPrintf(0, "Read byte %d is 0x%x! \n",i,readData[i]);
        }
    }
    else 
    {
        UARTIF_uartPrintf(0, "Flash read data id 0 fail! error code is %d \n",result);
    }
}

void TEST_ReadRawData(void)
{
      uint16_t i = 0, j = 0;

   // 读取验证
   UARTIF_uartPrintf(0, "Read page sg 0 header! \n");
   memset(buffer, 0, 256);
   W25Q32_ReadData(FLASH_SEGMENT0_BASE, buffer, 256);
   delay1ms(100);
   for (i = 0;i<256;i++)
   {
       UARTIF_uartPrintf(0, "Byte %d is 0x%x! \n", i,buffer[i]);
       delay1ms(1);
   }

   for (j = 1;j<6;j++)
   {
    UARTIF_uartPrintf(0, "Read sg 0 page %d! \n", j);
    memset(buffer, 0, 256);
    W25Q32_ReadData(FLASH_SEGMENT0_BASE + (j << 8), buffer, 256);
    delay1ms(100);
    for (i = 0;i<256;i++)
    {
        UARTIF_uartPrintf(0, "Byte %d is 0x%x! \n", i,buffer[i]);
        delay1ms(1);
    }

   }

       UARTIF_uartPrintf(0, "Read page sg 1 header! \n");
   memset(buffer, 0, 256);
   W25Q32_ReadData(FLASH_SEGMENT1_BASE, buffer, 256);
   delay1ms(100);
   for (i = 0;i<256;i++)
   {
       UARTIF_uartPrintf(0, "Byte %d is 0x%x! \n", i,buffer[i]);
       delay1ms(1);
   }

   for (j = 1;j<6;j++)
   {
    UARTIF_uartPrintf(0, "Read sg 1 page %d! \n", j);
    memset(buffer, 0, 256);
    W25Q32_ReadData(FLASH_SEGMENT1_BASE + (j << 8), buffer, 256);
    delay1ms(100);
    for (i = 0;i<256;i++)
    {
        UARTIF_uartPrintf(0, "Byte %d is 0x%x! \n", i,buffer[i]);
        delay1ms(1);
    }
   }
}

void TEST_ReadRawDataByAddress(uint32_t address)
{
    uint16_t i = 0;
    UARTIF_uartPrintf(0, "Start to read address 0x%x! \n", address);

    memset(buffer, 0, 256);
    W25Q32_ReadData(address, buffer, 256);
    for (i = 0;i<256;i++)
    {
        UARTIF_uartPrintf(0, "Byte %d is 0x%x! \n", i,buffer[i]);
        delay1ms(1);
    }


}

void TEST_WriteImage4(void)
{
    uint16_t i = 0, j = 0;
    // uint8_t magic = IMAGE_HEADER_MAGIC;
    uint16_t id = 0;
    flash_result_t result = FLASH_OK;
    for (j = 0;j<3;j++)
    {
        for (i = 0;i< MAX_FRAME_NUM + 1;i++)
        {
            memset(buffer, i, PAYLOAD_SIZE);
            buffer[0u] = MAGIC_BW_IMAGE_DATA + 0x20;
            buffer[1u] = (uint8_t)(j & 0x00ff);
            id = i | (j << 8);
            result = FM_writeData(MAGIC_BW_IMAGE_DATA,id, buffer, PAYLOAD_SIZE);


            if (result == FLASH_OK)
            {
                // UARTIF_uartPrintf(0, "Flash write image data id 0x%04x success! \n", id);
            }
            else 
            {
                UARTIF_uartPrintf(0, "Flash write image data id 0x%04x fail! error code is %d \n", id, result);
                break;
            }
        }

        if (result == FLASH_OK)
        {
            result = FM_writeImageHeader(MAGIC_BW_IMAGE_HEADER, j, 0u);
        }

        if (result == FLASH_OK)
        {
            UARTIF_uartPrintf(0, "Write BW image %d header success! \n", j);
        }
        else
        {
            UARTIF_uartPrintf(0, "Write BW image %d header fail! error code is %d \n", j, result);
        }

        for (i = 0;i< MAX_FRAME_NUM + 1;i++)
        {
            memset(buffer, i, PAYLOAD_SIZE);
            buffer[0u] = MAGIC_RED_IMAGE_DATA + 0x20;
            buffer[1u] = (uint8_t)(j & 0x00ff);
            id = i | (j << 8);
            result = FM_writeData(MAGIC_RED_IMAGE_DATA,id, buffer, PAYLOAD_SIZE);


            if (result == FLASH_OK)
            {
                // UARTIF_uartPrintf(0, "Flash write image data id 0x%04x success! \n", id);
            }
            else 
            {
                UARTIF_uartPrintf(0, "Flash write image data id 0x%04x fail! error code is %d \n", id, result);
                break;
            }
        }

        if (result == FLASH_OK)
        {
            result = FM_writeImageHeader(MAGIC_RED_IMAGE_HEADER, j, 1u);
        }

        if (result == FLASH_OK)
        {
            UARTIF_uartPrintf(0, "Write RED image %d header success! \n", j);
        }
        else
        {
            UARTIF_uartPrintf(0, "Write RED image %d header fail! error code is %d \n", j, result);
        }
    }
}

void TEST_ReadImage(void)
{
    uint16_t i = 0;
    flash_result_t result = FLASH_OK;

    for (i = 0;i< MAX_FRAME_NUM + 1;i++)
    {
        memset(buffer, 0, PAYLOAD_SIZE);
        result = FM_readImage(MAGIC_BW_IMAGE_DATA, 0x01, i, buffer);
        if (result == FLASH_OK)
        {
            UARTIF_uartPrintf(0, "Read image data id %d success! \n", i);
            UARTIF_uartPrintf(0, "Byte 0 is 0x%x! \n",buffer[0]);
        }
        else
        {
            UARTIF_uartPrintf(0, "Read image data id %d fail! error code is %d \n", i, result);
        }
    }
}

void TEST_ReadImage4(void)
{
    uint16_t i = 0;
    uint8_t j = 0;
    flash_result_t result = FLASH_OK;
    for (j = 0;j<3;j++)
    {
        UARTIF_uartPrintf(0, "Start to read BW image %d ! \n", j);
        for (i = 0;i< MAX_FRAME_NUM + 1;i++)
        {
            memset(buffer, 0, PAYLOAD_SIZE);
            result = FM_readImage(MAGIC_BW_IMAGE_DATA, j, i, buffer);
            if (result == FLASH_OK)
            {
                UARTIF_uartPrintf(0, "Read BW image data id %d success! \n", i);
                UARTIF_uartPrintf(0, "Byte 0 is 0x%x!, Byte 1 is 0x%x!, Byte 2 is 0x%x! \n",buffer[0], buffer[1], buffer[2]);
            }
            else
            {
                UARTIF_uartPrintf(0, "Read BW image data id %d fail! error code is %d \n", i, result);
            }
        }
        UARTIF_uartPrintf(0, "Start to read RED image %d ! \n", j);
        for (i = 0;i< MAX_FRAME_NUM + 1;i++)
        {
            memset(buffer, 0, PAYLOAD_SIZE);
            result = FM_readImage(MAGIC_RED_IMAGE_DATA, j, i, buffer);
            if (result == FLASH_OK)
            {
                UARTIF_uartPrintf(0, "Read RED image data id %d success! \n", i);
                UARTIF_uartPrintf(0, "Byte 0 is 0x%x!, Byte 1 is 0x%x!, Byte 2 is 0x%x! \n",buffer[0], buffer[1], buffer[2]);
            }
            else
            {
                UARTIF_uartPrintf(0, "Read RED image data id %d fail! error code is %d \n", i, result);
            }
        }
    }
}

void TEST_writeBlockTest(uint8_t blockAddress)
{
    uint16_t i = 0;
    uint32_t addr = 0;
    for (i = 0;i<256;i++)
    {
        addr = 0;
        addr |= (uint32_t) (i << 8u);
        addr |= (uint32_t) (blockAddress << 16u);
        UARTIF_uartPrintf(0, "Write page addr 0x%06lx as 0x%x! \n", addr, i);
        if (i != 0xff)
        {
            memset(buffer, i, 256);  // 填充测试数据
        }
        else 
        {
            memset(buffer, 0x67, 256);  // 填充测试数据
        }
        W25Q32_WritePage(addr, buffer, 256);
    }
}

void TEST_readBlockTest(uint8_t blockAddress)
{
    uint16_t i = 0;
    uint32_t addr = 0;
    for (i = 0;i<256;i++)
    {
        addr = 0;
        addr |= (uint32_t) (i << 8u);
        addr |= (uint32_t) (blockAddress << 16u);
        UARTIF_uartPrintf(0, "Read page addr 0x%06lx! \n",addr);
        memset(buffer, 0, 256);
        W25Q32_ReadData(addr, buffer, 256);
        delay1ms(1);
        UARTIF_uartPrintf(0, "Byte 0 is 0x%x! \n",buffer[0]);
    }
}

void TEST_WriteImageOnePage(uint8_t frameNum, uint8_t pattern)
{
    uint16_t id = 0;
    flash_result_t result = FLASH_OK;

    memset(buffer, pattern, PAYLOAD_SIZE);
    id = frameNum | (0x01 << 8);
    result = FM_writeData(MAGIC_BW_IMAGE_DATA,id, buffer, PAYLOAD_SIZE);

    if (result == FLASH_OK)
    {
        UARTIF_uartPrintf(0, "Flash write image data id 0x%04x success! \n", id);
    }
    else 
    {
        UARTIF_uartPrintf(0, "Flash write image data id 0x%04x fail! error code is %d \n", id, result);
    }
}

#endif
void TEST_WriteImage(void)
{
    uint16_t i = 0;
    // uint8_t magic = IMAGE_HEADER_MAGIC;
    uint16_t id = 0;
    flash_result_t result = FLASH_OK;

    for (i = 0;i< MAX_FRAME_NUM + 1;i++)
    {
        memset(buffer, 0XCC, PAYLOAD_SIZE);
        id = i | (0x01 << 8);
        result = FM_writeData(MAGIC_BW_IMAGE_DATA,id, buffer, PAYLOAD_SIZE);

        if (result == FLASH_OK)
        {
            UARTIF_uartPrintf(0, "Flash write image data id 0x%04x success! \n", id);
        }
        else 
        {
            UARTIF_uartPrintf(0, "Flash write image data id 0x%04x fail! error code is %d \n", id, result);
            break;
        }
    }

    if (result == FLASH_OK)
    {
        result = FM_writeImageHeader(MAGIC_BW_IMAGE_HEADER, 0x01, 0u);
    }

    if (result == FLASH_OK)
    {
        UARTIF_uartPrintf(0, "Write image header success! \n");
    }
    else
    {
        UARTIF_uartPrintf(0, "Write image header fail! error code is %d \n", result);
    }
}
