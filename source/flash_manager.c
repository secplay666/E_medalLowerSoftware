/******************************************************************************
 * Include files
 ******************************************************************************/

#include "flash_manager.h"
#include "w25q32.h"
#include "crc_utils.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "uart_interface.h"
#include "crc_utils.h"
#include "ddl.h"


/******************************************************************************
 * Local function prototypes ('static')
 ******************************************************************************/

static flash_result_t readSegmentHeader(uint32_t segmentBase, boolean_t isDstHeaderHigh);
static flash_result_t resetSegment(boolean_t isSetHiSegment, const uint32_t statusMagic, const uint32_t currentGcCounter);
static flash_result_t scanSegmentPages(void);
//static int16_t find_data_entry(flash_manager_t* manager, uint16_t dataId);
//static flash_result_t add_data_entry(flash_manager_t* manager, uint16_t dataId, uint32_t page_address);

static flash_result_t eraseSegment(boolean_t eraseHiSegment);
static flash_result_t copyValidPages(void);
static void readBlock(uint8_t blockAddress);
static flash_result_t garbageCollect(void);

/******************************************************************************
 * Local variable definitions ('static')                                      *
 ******************************************************************************/
static flash_manager_t fmCtx;

// 静态缓冲区，用于Flash读写操作的中间变量
static uint8_t G_buffer1[FLASH_PAGE_SIZE] = {0};
static uint8_t G_buffer2[FLASH_PAGE_SIZE] = {0};

static uint16_t G_imageAddressBuffer[MAX_FRAME_NUM + 1u];

/*****************************************************************************
 * Function implementation - local ('static')
 ******************************************************************************/

/**
 * @brief 辅助函数：重置 segment（可选重置0、1或同时重置两个）
 * @param reset0 是否重置segment0
 * @param reset1 是否重置segment1
 * @param statusMagic0 segment0的magic
 * @param statusMagic1 segment1的magic
 * @param result 返回结果指针
 */
static void resetSegments(
    boolean_t reset0, boolean_t reset1,
    uint32_t statusMagic0, uint32_t statusMagic1,
    flash_result_t* result
) {
    if (reset0 && reset1) {
        *result = resetSegment(FALSE, statusMagic0, 0);
        *result = resetSegment(TRUE, statusMagic1, 0);
        fmCtx.activeSegmentBaseStatus = MAGIC_LOW_ACTIVE;
        fmCtx.gcInProgress = 0;
    } else if (reset0) {
        *result = resetSegment(FALSE, statusMagic0, 0);
        fmCtx.activeSegmentBaseStatus = MAGIC_LOW_ACTIVE;
    } else if (reset1) {
        *result = resetSegment(TRUE, statusMagic1, 0);
        fmCtx.activeSegmentBaseStatus = MAGIC_HIGH_ACTIVE;
    }
}

/** 
 * @brief 读取segment头
 */
static flash_result_t readSegmentHeader(uint32_t segmentBase, boolean_t isDstHeaderHigh)
{
    flash_result_t re = FLASH_OK;
	uint32_t calculatedCrc;
    // 使用静态缓冲区读取数据
    
    segment_header_t* header = NULL;

    if (isDstHeaderHigh) {
        header = &fmCtx.header1;
    } else {
        header = &fmCtx.header0;
    }

    if (W25Q32_ReadData(segmentBase, G_buffer1, sizeof(segment_header_t)) != 0) 
    {
        re = FLASH_ERROR_READ_FAIL;
    }

    // 从缓冲区复制到结构体
    header->headerMagic = G_buffer1[0];
    header->segmentId = G_buffer1[1];
    header->statusMagic = (uint32_t)G_buffer1[2] | ((uint32_t)G_buffer1[3] << 8) | 
                          ((uint32_t)G_buffer1[4] << 16) | ((uint32_t)G_buffer1[5] << 24);
    header->gcCounter = (uint32_t)G_buffer1[6] | ((uint32_t)G_buffer1[7] << 8) | 
                      ((uint32_t)G_buffer1[8] << 16) | ((uint32_t)G_buffer1[9] << 24);
    header->crc32 = (uint32_t)G_buffer1[10] | ((uint32_t)G_buffer1[11] << 8) | 
                   ((uint32_t)G_buffer1[12] << 16) | ((uint32_t)G_buffer1[13] << 24);
    
    // 验证头魔法数字
    if (header->headerMagic != SEGMENT_HEADER_MAGIC) {
        header->headerMagic = 0xE0;
        re = FLASH_ERROR_INIT_FAIL;
    }

    if ((header->statusMagic != SEGMENT_MAGIC_ACTIVE) && (header->statusMagic != SEGMENT_MAGIC_BACKUP))
    {
        header->headerMagic = 0xE0;
        re = FLASH_ERROR_INIT_FAIL;
    }
    
    // 验证CRC32 - 只校验有效部分（不包括crc32字段）
    calculatedCrc = calculate_crc32_default(G_buffer1, 10); // headerMagic(1) + segmentId(1) + statusMagic(4) + gcCounter(4) = 10字节有效数据
    if (calculatedCrc != header->crc32) {
        header->headerMagic = 0xE0;
        re = FLASH_ERROR_CRC_FAIL;
    }
    
    if (re != FLASH_OK) 
    {
        // UARTIF_uartPrintf(0, "buffer 0 is 0x%02x! \n", G_buffer1[0]);
        // UARTIF_uartPrintf(0, "buffer 1 is 0x%02x! \n", G_buffer1[1]);
        // UARTIF_uartPrintf(0, "buffer 2 is 0x%02x! \n", G_buffer1[2]);
        // UARTIF_uartPrintf(0, "buffer 3 is 0x%02x! \n", G_buffer1[3]);
        // UARTIF_uartPrintf(0, "buffer 4 is 0x%02x! \n", G_buffer1[4]);
        // UARTIF_uartPrintf(0, "buffer 5 is 0x%02x! \n", G_buffer1[5]);
        // UARTIF_uartPrintf(0, "buffer 6 is 0x%02x! \n", G_buffer1[6]);
        // UARTIF_uartPrintf(0, "buffer 7 is 0x%02x! \n", G_buffer1[7]);
        // UARTIF_uartPrintf(0, "buffer 8 is 0x%02x! \n", G_buffer1[8]);
        // UARTIF_uartPrintf(0, "buffer 9 is 0x%02x! \n", G_buffer1[9]);
        // UARTIF_uartPrintf(0, "buffer 10 is 0x%02x! \n", G_buffer1[10]);
        // UARTIF_uartPrintf(0, "buffer 11 is 0x%02x! \n", G_buffer1[11]);
        // UARTIF_uartPrintf(0, "buffer 12 is 0x%02x! \n", G_buffer1[12]);
        // UARTIF_uartPrintf(0, "buffer 13 is 0x%02x! \n", G_buffer1[13]);

    }
    return re;
}

/**
 * @brief 写入segment头
 */
static flash_result_t resetSegment(boolean_t isSetHiSegment, const uint32_t statusMagic, const uint32_t currentGcCounter)
{
	uint32_t crc32;
    flash_result_t re = FLASH_OK;

    // 清空缓冲区
    memset(G_buffer1, 0x00u, 256);

    // 将结构体字段复制到缓冲区
    G_buffer1[0] = SEGMENT_HEADER_MAGIC;
    G_buffer1[1] = isSetHiSegment ? 1 : 0;
    G_buffer1[2] = (uint8_t)(statusMagic & 0xFF);
    G_buffer1[3] = (uint8_t)((statusMagic >> 8) & 0xFF);
    G_buffer1[4] = (uint8_t)((statusMagic >> 16) & 0xFF);
    G_buffer1[5] = (uint8_t)((statusMagic >> 24) & 0xFF);
    G_buffer1[6] = (uint8_t)(currentGcCounter & 0xFF);
    G_buffer1[7] = (uint8_t)((currentGcCounter >> 8) & 0xFF);
    G_buffer1[8] = (uint8_t)((currentGcCounter >> 16) & 0xFF);
    G_buffer1[9] = (uint8_t)((currentGcCounter >> 24) & 0xFF);

    // 计算CRC32 - 只计算有效部分（不包括crc32字段）
    crc32 = calculate_crc32_default(G_buffer1, 10); // headerMagic(1) + segmentId(1) + statusMagic(4) + gcCounter(4) = 10字节有效数据
    G_buffer1[10] = (uint8_t)(crc32 & 0xFF);
    G_buffer1[11] = (uint8_t)((crc32 >> 8) & 0xFF);
    G_buffer1[12] = (uint8_t)((crc32 >> 16) & 0xFF);
    G_buffer1[13] = (uint8_t)((crc32 >> 24) & 0xFF);

    // 擦除一个sg
    eraseSegment( isSetHiSegment);

    UARTIF_uartPrintf(0, "flash_manager: reset segment %s! \n", isSetHiSegment ? "1" : "0");
    // UARTIF_uartPrintf(0, "buffer 0 is 0x%02x! \n", G_buffer1[0]);
    // UARTIF_uartPrintf(0, "buffer 1 is 0x%02x! \n", G_buffer1[1]);
    // UARTIF_uartPrintf(0, "buffer 2 is 0x%02x! \n", G_buffer1[2]);
    // UARTIF_uartPrintf(0, "buffer 3 is 0x%02x! \n", G_buffer1[3]);
    // UARTIF_uartPrintf(0, "buffer 4 is 0x%02x! \n", G_buffer1[4]);
    // UARTIF_uartPrintf(0, "buffer 5 is 0x%02x! \n", G_buffer1[5]);
    // UARTIF_uartPrintf(0, "buffer 6 is 0x%02x! \n", G_buffer1[6]);
    // UARTIF_uartPrintf(0, "buffer 7 is 0x%02x! \n", G_buffer1[7]);
    // UARTIF_uartPrintf(0, "buffer 8 is 0x%02x! \n", G_buffer1[8]);
    // UARTIF_uartPrintf(0, "buffer 9 is 0x%02x! \n", G_buffer1[9]);
    // UARTIF_uartPrintf(0, "buffer 10 is 0x%02x! \n", G_buffer1[10]);
    // UARTIF_uartPrintf(0, "buffer 11 is 0x%02x! \n", G_buffer1[11]);
    // UARTIF_uartPrintf(0, "buffer 12 is 0x%02x! \n", G_buffer1[12]);
    // UARTIF_uartPrintf(0, "buffer 13 is 0x%02x! \n", G_buffer1[13]);

    // 写入header（写入整个页面以保持256字节对齐）
    if (W25Q32_WritePage(isSetHiSegment ? FLASH_SEGMENT1_BASE : FLASH_SEGMENT0_BASE , G_buffer1, FLASH_PAGE_SIZE) != 0) 
    {
        re = FLASH_ERROR_WRITE_FAIL;
    }
    
    return re;
}

static void readBlock(uint8_t blockAddress)
{
    uint16_t i = 0;
    uint32_t addr = 0;
    uint8_t magic;
    uint8_t dataId;

    for (i = 0;i<256;i++)
    {
        addr = 0;
        addr |= (uint32_t) (i << 8u);
        addr |= (uint32_t) (blockAddress << 16u);
        if ((addr != FLASH_SEGMENT0_BASE) && (addr != FLASH_SEGMENT1_BASE))
        {
            // UARTIF_uartPrintf(0, "Read page addr 0x%06lx! \n",addr);
            memset(G_buffer1, 0, 256);
            if (W25Q32_ReadData(addr, G_buffer1, 256) == 0)
            {
            // delay1ms(1);
                magic = G_buffer1[0];
                if (magic == DATA_PAGE_MAGIC || magic == MAGIC_BW_IMAGE_HEADER || magic == MAGIC_RED_IMAGE_HEADER)
                {
                    dataId = G_buffer1[1];
                    if (fmCtx.entriesCountMax[magic & 0x03] > dataId)
                    {
                        fmCtx.entries[magic & 0x03][dataId] = (uint16_t)(addr >> 8u);
                    }
                    else
                    {
                        /* 只打印一次警告，避免刷屏 */
                        // UARTIF_uartPrintf(0, "WARN: dataId %d out of range (max=%d) at addr 0x%06lx magic=0x%02x\n", 
                        //                  dataId, fmCtx.entriesCountMax[magic & 0x03], addr, magic);
                    }
                }
                else if (magic == MAGIC_BW_IMAGE_DATA || magic == MAGIC_RED_IMAGE_DATA)
                {
                    // do nothing
                }
                else if (magic == 0xff)
                {
                    if ((G_buffer1[1] == 0xff) && (G_buffer1[3] == 0xff))
                    {
                        UARTIF_uartPrintf(0, "flash_manager found last block! \n");
                    }
                    else 
                    {
                        UARTIF_uartPrintf(0, "ERR: flash_manager 0x07! last block error\n");
                    }
                    fmCtx.nextWriteAddress = blockAddress << 8u;
                    fmCtx.nextWriteAddress |= i;
                    // UARTIF_uartPrintf(0, "flash_manager found next write address 0x%04x!!! \n", fmCtx.nextWriteAddress);
                    break;
                }
                else
                {
                    UARTIF_uartPrintf(0, "ERR: flash_manager 0x06! unknow magic\n");
                }
            }
        }
    }
}

// /**
//  * @brief 返回指定槽位的颜色标志（0 = BW, 1 = RED, 0xFF = 未知）
//  */
// uint8_t FM_getImageSlotColor(uint8_t slotId)
// {
//     if (slotId >= MAX_IMAGE_ENTRIES) return 0xFFu;
//     return fmCtx.imageSlotColor[slotId];
// }

/**
 * @brief 扫描segment中的所有page，构建内存映射表
 */
static flash_result_t scanSegmentPages(void)
{
    uint8_t endBlockAddr = 0x20;
    uint8_t blockAddr = 0x00;
 
    // 从第1个page开始扫描（第0个是header）
    blockAddr = (fmCtx.activeSegmentBaseStatus == MAGIC_LOW_ACTIVE) ? 0x00 : 0x20;
    endBlockAddr = blockAddr + 0x20;
    for (; blockAddr < endBlockAddr; blockAddr++)
    {
        readBlock(blockAddr);
        if (fmCtx.nextWriteAddress != 0xffff)
        {
            break;
        }
    }
    if (fmCtx.nextWriteAddress == 0xffff)
    {
        // 如果扫描完所有page都没找到空page，说明segment已满
        fmCtx.gcInProgress = 1;
    }
    else 
    {
        UARTIF_uartPrintf(0, "flash_manager next write address is 0x%04x\n", fmCtx.nextWriteAddress);
    }
    return FLASH_OK;
}

static flash_result_t scanImageDataPages(uint8_t magic, uint8_t slotId)
{
    uint32_t currentAddr = 0x00;
    uint32_t endAddr = 0x00;
    uint8_t frameNum = 0;
    uint64_t frameIsFull = 0x00;
    flash_result_t re = FLASH_OK;
    uint8_t pageMagic;
    uint8_t pageSlotId;

    currentAddr = (uint32_t)((fmCtx.nextWriteAddress - 1) << 8u);
    endAddr = (fmCtx.activeSegmentBaseStatus == MAGIC_LOW_ACTIVE) ? FLASH_SEGMENT0_BASE : FLASH_SEGMENT1_BASE;
    for (; currentAddr > endAddr; currentAddr -= FLASH_PAGE_SIZE)
    {
        memset(G_buffer1, 0, 256);
        if (W25Q32_ReadData(currentAddr, G_buffer1, 256) == 0)
        {
            pageMagic = G_buffer1[0];
            pageSlotId = G_buffer1[2];
            
            // 只要 magic 或 slotId 不匹配，立即停止扫描
            if (pageMagic != magic || pageSlotId != slotId)
            {
                break;
            }
            
            frameNum = G_buffer1[1];
            if ((frameIsFull & ((uint64_t)1u << frameNum)) != 0u)
            {
                // 重复的 frame 号，跳过
                continue;
            }
 
            if (frameNum > MAX_FRAME_NUM)
            {
                UARTIF_uartPrintf(0, "ERR: flash_manager 0x09! image frame num out of range: %d\n", frameNum);
                re = FLASH_ERROR_IMAGE_FRAME_LOST;
                break;
            }
            G_imageAddressBuffer[frameNum] = (uint16_t)((currentAddr & 0x00ffff00) >> 8u);


            frameIsFull |= ((uint64_t)1u << frameNum);
            if (frameIsFull == 0x1FFFFFFFFFFFFFFF)
            {
                // 所有 61 个 frame 都找到了，完成
                break;
            }
        }
    }
    if (re == FLASH_OK)
    {
        if (frameIsFull != 0x1FFFFFFFFFFFFFFF)
        {
            UARTIF_uartPrintf(0, "ERR: scanImageDataPages frame lost\r\n");
            re = FLASH_ERROR_IMAGE_FRAME_LOST;
        }
    }
    return re;
}

static flash_result_t checkAndDoGarbageCollection(void)
{
    flash_result_t re = FLASH_OK;
    uint32_t nextWriteAddress = (uint32_t)(fmCtx.nextWriteAddress << 8u);
    if (fmCtx.gcInProgress)
    {
        // UARTIF_uartPrintf(0, "go to gc 0\n");
        // re = garbageCollect();
        UARTIF_uartPrintf(0, "Gc on going! \n");
    }
    else 
    {
        if (fmCtx.activeSegmentBaseStatus == MAGIC_LOW_ACTIVE)
        {
            if (nextWriteAddress >= FLASH_SEGMENT1_BASE)
            {
                fmCtx.gcInProgress = 1;
                UARTIF_uartPrintf(0, "go to gc 1\n");

                re = garbageCollect();
            }
        }
        else 
        {
            if (nextWriteAddress >= FLASH_TOTAL_SIZE)
            {
                fmCtx.gcInProgress = 1;
                UARTIF_uartPrintf(0, "go to gc 2\n");

                re = garbageCollect();
            }
        }
    }
    return re;
}

/**
 * @brief 擦除整个segment
 */
static flash_result_t eraseSegment(boolean_t eraseHiSegment)
{
    uint32_t sectorAddress = 0x00;
    uint32_t i;
    uint8_t startAddr = 0;

    // 按扇区擦除整个segment
    startAddr = (eraseHiSegment) ? 0x20 : 0x00;

    UARTIF_uartPrintf(0, "flash_manager: start to erase block 0x%02x to 0x%02x! \n", startAddr, startAddr + 0x1f);
    for (i = startAddr; i < (startAddr + 0x1f); i++) 
    {
        sectorAddress = 0x00;
        sectorAddress |= i << 16;
        W25Q32_Erase64k(sectorAddress);
    }
    return FLASH_OK;
}

static flash_result_t readImageHeaderIntoBuffer(uint8_t magic, uint8_t slotId)
{
    // uint8_t i = 0;
    flash_result_t result = FLASH_OK;
    /* Read addresses plus one-byte color flag (if present).
     * FM_readData returns FLASH_OK only if the page exists and CRC matches.
     */
    memset(G_buffer2, 0xff, FLASH_PAGE_SIZE);
    result = FM_readData(magic, slotId, G_buffer2, (MAX_FRAME_NUM + 1) * 2);
    
    if (result != FLASH_OK)
    {
        return result;
    }
    
    /* copy addresses - only copy actual valid data */
    memcpy(G_imageAddressBuffer, G_buffer2, (MAX_FRAME_NUM + 1) * 2);
    /* copy stored color flag if present */
    // if (slotId < MAX_IMAGE_ENTRIES)
    // {
    //     fmCtx.imageSlotColor[slotId] = G_buffer2[(MAX_FRAME_NUM + 1) * 2];
    // }
    return result;
}

static flash_result_t copyPage(uint16_t srcAddr, uint16_t destAddr, boolean_t isDestNext)
{
    uint32_t srcAddress = 0;
    uint32_t destAddress = 0;
    flash_result_t result = FLASH_OK;

    srcAddress |= (uint32_t) (srcAddr << 8u);

    if (isDestNext)
    {
        destAddress |= (uint32_t) (fmCtx.nextWriteAddress << 8u);
    }
    else 
    {
        destAddress |= (uint32_t) (destAddr << 8u);
    }
    // UARTIF_uartPrintf(0, "Copy data from 0x%06lx to 0x%06lx! \n", srcAddress, destAddress);

    // 读取源page
    if (result == FLASH_OK)
    {
        memset(G_buffer1, 0, 256);
        if (W25Q32_ReadData(srcAddress, G_buffer1, FLASH_PAGE_SIZE) != 0) 
        {
            result =  FLASH_ERROR_READ_FAIL;
        }
    }
    // 写入目标page
    if (result == FLASH_OK)
    {
        if (W25Q32_WritePage(destAddress, G_buffer1, FLASH_PAGE_SIZE) != 0) 
        {
            return FLASH_ERROR_WRITE_FAIL;
        }
    }
    return result;
}

/**
 * @brief 复制有效页面到备用segment
 */
static flash_result_t copyValidPages(void)
{
	uint8_t i, j, k;

    flash_result_t result = FLASH_OK;

    for (i = 0; i < MAX_DATA_ENTRIES; i++) 
    {
        if (fmCtx.dataEntries[i] != 0xffff)
        {
            result = copyPage(fmCtx.dataEntries[i], 0, TRUE);
            if (result != FLASH_OK)
            {
                UARTIF_uartPrintf(0, "ERR: flash_manager 0x10! copy data page fail entry %d\n", i);
                // 删除映射表中的地址
                fmCtx.dataEntries[i] = 0xffff;
            }
            else
            {
                // 更新映射表中的地址
                fmCtx.dataEntries[i] = fmCtx.nextWriteAddress;
                // 更新Next Write Address
                fmCtx.nextWriteAddress++;
            }
        }
    }

    for (k = 1; k < 3; k++)
    {
        for (i = 0; i < MAX_IMAGE_ENTRIES; i++) 
        {
            if (fmCtx.entries[k][i] != 0xffff)
            {
                memset(G_imageAddressBuffer, 0xff, sizeof(G_imageAddressBuffer));
                result = readImageHeaderIntoBuffer(DATA_PAGE_MAGIC + k, i);
                if (result != FLASH_OK)
                {
                    // 删除映射表中的地址
                    UARTIF_uartPrintf(0, "ERR: flash_manager 0x10! read image header into buffer fail\n");
                    fmCtx.entries[k][i] = 0xffff;
                    continue;
                }
                for (j = 0; j < MAX_FRAME_NUM + 1; j++)
                {
                    if (G_imageAddressBuffer[j] != 0xffff)
                    {
                        result = copyPage(G_imageAddressBuffer[j], 0, TRUE);
                    }
                    if (result == FLASH_OK)
                    {
                        fmCtx.nextWriteAddress++;
                    }
                    else
                    {
                        UARTIF_uartPrintf(0, "ERR: flash_manager 0x10! copy image data page fail entry %d frame %d\n", i, j);
                        break;
                    }
                }
                if (result != FLASH_OK)
                {
                    // 删除映射表中的地址
                    fmCtx.entries[k][i] = 0xffff;
                    continue;
                }
                // else
                // {
                //     result = FM_writeImageHeader(DATA_PAGE_MAGIC + k, i);
                // }
            }
        }
    }
        
    return result;
}

static flash_result_t judgeWhichSegmentIsActive(void)
{
    flash_result_t result = FLASH_OK;
    uint8_t sg0Tail, sg1Tail;

    memset(G_buffer1, 0, 256);

    if (W25Q32_ReadData(FLASH_SEGMENT1_BASE - 0x100, G_buffer1, sizeof(segment_header_t)) != 0) 
    {
        result = FLASH_ERROR_READ_FAIL;
    }
    else
    {
        sg0Tail = G_buffer1[0];
    }

    if (result == FLASH_OK)
    {
        memset(G_buffer1, 0, 256);

        if (W25Q32_ReadData(FLASH_TOTAL_SIZE - 0x100, G_buffer1, sizeof(segment_header_t)) != 0) 
        {
            result = FLASH_ERROR_READ_FAIL;
        }
        else
        {
            sg1Tail = G_buffer1[0];
        }
    }

    if (result == FLASH_OK)
    {
        if ((sg0Tail == DATA_PAGE_MAGIC ||
            sg0Tail == MAGIC_BW_IMAGE_DATA ||
            sg0Tail == MAGIC_RED_IMAGE_DATA ||
            sg0Tail == MAGIC_BW_IMAGE_HEADER ||
            sg0Tail == MAGIC_RED_IMAGE_HEADER) && 
            (sg1Tail != DATA_PAGE_MAGIC ||
            sg1Tail != MAGIC_BW_IMAGE_DATA ||
            sg1Tail != MAGIC_RED_IMAGE_DATA ||
            sg1Tail != MAGIC_BW_IMAGE_HEADER ||
            sg1Tail != MAGIC_RED_IMAGE_HEADER))
        {
            fmCtx.activeSegmentBaseStatus = MAGIC_LOW_ACTIVE;
            UARTIF_uartPrintf(0, "regc flash_manager low active\n");
        }
        else if ((sg1Tail == DATA_PAGE_MAGIC ||
            sg1Tail == MAGIC_BW_IMAGE_DATA ||
            sg1Tail == MAGIC_RED_IMAGE_DATA ||
            sg1Tail == MAGIC_BW_IMAGE_HEADER ||
            sg1Tail == MAGIC_RED_IMAGE_HEADER) && 
            (sg0Tail != DATA_PAGE_MAGIC ||
            sg0Tail != MAGIC_BW_IMAGE_DATA ||
            sg0Tail != MAGIC_RED_IMAGE_DATA ||
            sg0Tail != MAGIC_BW_IMAGE_HEADER ||
            sg0Tail != MAGIC_RED_IMAGE_HEADER))
        {
            fmCtx.activeSegmentBaseStatus = MAGIC_HIGH_ACTIVE;
            UARTIF_uartPrintf(0, "regc flash_manager high active\n");
        }
        else if (sg0Tail == 0xff && sg1Tail == 0xff)
        {
            // to do: scan all pages to find last write page
            // if not found, reset 2 segments
            UARTIF_uartPrintf(0, "flash_manager no active segment, reset 2 segments\n");
            result = FLASH_ERROR_INIT_FAIL;
        }
        else
        {
            UARTIF_uartPrintf(0, "ERR: flash_manager 0x09! regc error, reset 2 segments\n");
            result = FLASH_ERROR_INIT_FAIL;
        }
    }

    return result;
}

static flash_result_t checkArguments(uint8_t magic, uint16_t dataId, const uint8_t* data, uint16_t size)
{
    flash_result_t result = FLASH_OK;
    uint8_t slotId, frameNum;

    if (magic != MAGIC_BW_IMAGE_DATA && magic != MAGIC_RED_IMAGE_DATA && magic != DATA_PAGE_MAGIC
        && magic != MAGIC_BW_IMAGE_HEADER && magic != MAGIC_RED_IMAGE_HEADER)
    {
        result = FLASH_ERROR_INVALID_PARAM;
    }

    if (data == NULL || size == 0 || size > PAYLOAD_SIZE)
    {
        result = FLASH_ERROR_INVALID_PARAM;
    }
    
    if (magic == DATA_PAGE_MAGIC && dataId >= MAX_DATA_ENTRIES)
    {
        result = FLASH_ERROR_INVALID_PARAM;
    }

    if (magic == MAGIC_BW_IMAGE_DATA || magic == MAGIC_RED_IMAGE_DATA)
    {
        slotId = (uint8_t)((dataId & 0xff00u) >> 8u);
        frameNum = (uint8_t)(dataId & 0xffu);
        if (slotId >= MAX_IMAGE_ENTRIES || frameNum > MAX_FRAME_NUM)
        {
            result = FLASH_ERROR_INVALID_PARAM;
        }
    }
    return result;
}

/**
 * @brief 执行垃圾回收
 */
static flash_result_t garbageCollect(void)
{
    uint8_t i, k;
    flash_result_t result = FLASH_OK;
    UARTIF_uartPrintf(0, "flash_manager start garbage collecting! \n");

    fmCtx.currentGcCounter ++;
    // 1. 将备用segment标记为激活
    if (result == FLASH_OK)
    {
        UARTIF_uartPrintf(0, "flash_manager garbage collecting step one \n");
        result = resetSegment((fmCtx.activeSegmentBaseStatus == MAGIC_LOW_ACTIVE), SEGMENT_MAGIC_ACTIVE, fmCtx.currentGcCounter);
        fmCtx.nextWriteAddress = (fmCtx.activeSegmentBaseStatus == MAGIC_LOW_ACTIVE) ? 0x2001 : 0x0001;
    }

    // 2. 复制有效数据
    if (result == FLASH_OK)
    {
        UARTIF_uartPrintf(0, "flash_manager garbage collecting step two \n");
        result = copyValidPages();
    }
    
    // 3. 将原激活segment标记为备用
    if (result == FLASH_OK)
    {
        UARTIF_uartPrintf(0, "flash_manager garbage collecting step three \n");
        if (fmCtx.activeSegmentBaseStatus == MAGIC_LOW_ACTIVE)
        {
            fmCtx.activeSegmentBaseStatus = MAGIC_HIGH_ACTIVE;
        }
        else 
        {
            fmCtx.activeSegmentBaseStatus = MAGIC_LOW_ACTIVE;
        }
        result = resetSegment((fmCtx.activeSegmentBaseStatus == MAGIC_LOW_ACTIVE), SEGMENT_MAGIC_BACKUP, 0);
    }
    
    // 4. Update image header if needed
    for (k = 1; k < 3; k++)
    {
        for (i = 0; i < MAX_IMAGE_ENTRIES; i++) 
        {
            if (fmCtx.entries[k][i] != 0xffff)
            {
                result = FM_writeImageHeader(DATA_PAGE_MAGIC + k, i);
                if (result != FLASH_OK)
                {
                    UARTIF_uartPrintf(0, "ERR: flash_manager 0x08! write image header fail entry %d, type %d\n", i, k);
                }
            }
        }
    }

    if (result == FLASH_OK)
    {
        UARTIF_uartPrintf(0, "flash_manager garbage collecting finished successfully! \n");

        fmCtx.gcInProgress = 0;
    }
    else
    {
        UARTIF_uartPrintf(0, "ERR: flash_manager 0x08! gc error: %d\n", result);
    }
    
    return result;
}




/*****************************************************************************
 * Function implementation - global ('extern')
 ******************************************************************************/

/**
 * @brief 初始化Flash管理器
 */

flash_result_t FM_init()
{
    boolean_t needToInitList = FALSE;
    flash_result_t result = FLASH_OK;
    memset(fmCtx.dataEntries, 0xff, sizeof(uint16_t) * MAX_DATA_ENTRIES);
    memset(fmCtx.imageBwEntries, 0xff, sizeof(uint16_t) * MAX_IMAGE_ENTRIES);
    memset(fmCtx.imageRedEntries, 0xff, sizeof(uint16_t) * MAX_IMAGE_ENTRIES);
    // memset(fmCtx.imageSlotColor, 0xFF, sizeof(fmCtx.imageSlotColor));
    fmCtx.nextWriteAddress = 0xffff;

    fmCtx.entries[0] = fmCtx.dataEntries;
    fmCtx.entries[1] = fmCtx.imageBwEntries;
    fmCtx.entries[2] = fmCtx.imageRedEntries;
    fmCtx.entriesCountMax[0] = MAX_DATA_ENTRIES;
    fmCtx.entriesCountMax[1] = MAX_IMAGE_ENTRIES;
    fmCtx.entriesCountMax[2] = MAX_IMAGE_ENTRIES;
    
    // 读取两个segment的header
    if (result == FLASH_OK)
    {
   
        // 初始化管理器：读取两个 segment 的 header（segment0 -> header0, segment1 -> header1）
        {
            flash_result_t r0;
            flash_result_t r1;

            r0 = readSegmentHeader(FLASH_SEGMENT0_BASE, FALSE);
            if (r0 == FLASH_ERROR_READ_FAIL)
            {
                UARTIF_uartPrintf(0, "ERR: flash_manager 0x04! header0 error\n");
            }

            r1 = readSegmentHeader(FLASH_SEGMENT1_BASE, TRUE);
            if (r1 == FLASH_ERROR_READ_FAIL)
            {
                UARTIF_uartPrintf(0, "ERR: flash_manager 0x04! header1 error\n");
            }

            /* If both reads failed, consider init fail; otherwise continue */
            if (r0 != FLASH_OK && r1 != FLASH_OK) {
                result = FLASH_ERROR_READ_FAIL;
            } else {
                result = FLASH_OK;
            }
        }
    }

    if (result == FLASH_OK)
    {
        if ((fmCtx.header0.headerMagic != SEGMENT_HEADER_MAGIC) && (fmCtx.header1.headerMagic != SEGMENT_HEADER_MAGIC)) {
            resetSegments(TRUE, TRUE, SEGMENT_MAGIC_ACTIVE, SEGMENT_MAGIC_BACKUP, &result);
        }
        else if ((fmCtx.header0.headerMagic != SEGMENT_HEADER_MAGIC) && (fmCtx.header1.headerMagic == SEGMENT_HEADER_MAGIC)) {
            resetSegments(TRUE, FALSE, (fmCtx.header1.statusMagic == SEGMENT_MAGIC_BACKUP) ? SEGMENT_MAGIC_ACTIVE : SEGMENT_MAGIC_BACKUP, 0, &result);
            fmCtx.activeSegmentBaseStatus = (fmCtx.header1.statusMagic == SEGMENT_MAGIC_BACKUP) ? MAGIC_LOW_ACTIVE : MAGIC_HIGH_ACTIVE;
        }
        else if ((fmCtx.header0.headerMagic == SEGMENT_HEADER_MAGIC) && (fmCtx.header1.headerMagic != SEGMENT_HEADER_MAGIC)) {
            resetSegments(FALSE, TRUE, 0, (fmCtx.header0.statusMagic == SEGMENT_MAGIC_BACKUP) ? SEGMENT_MAGIC_ACTIVE : SEGMENT_MAGIC_BACKUP, &result);
            fmCtx.activeSegmentBaseStatus = (fmCtx.header0.statusMagic == SEGMENT_MAGIC_BACKUP) ? MAGIC_HIGH_ACTIVE : MAGIC_LOW_ACTIVE;
        }
        else 
        {
            // 根据header状态确定激活segment
            if (fmCtx.header0.statusMagic == SEGMENT_MAGIC_ACTIVE && fmCtx.header1.statusMagic == SEGMENT_MAGIC_BACKUP)
            {
                // 正常情况：segment0激活，segment1备用
                fmCtx.activeSegmentBaseStatus = MAGIC_LOW_ACTIVE;
                UARTIF_uartPrintf(0, "flash_manager low active\n");
                needToInitList = TRUE;
                fmCtx.currentGcCounter = fmCtx.header0.gcCounter;
            } 
            else if (fmCtx.header1.statusMagic == SEGMENT_MAGIC_ACTIVE && fmCtx.header0.statusMagic == SEGMENT_MAGIC_BACKUP)
            {
                // segment1激活，segment0备用
                fmCtx.activeSegmentBaseStatus = MAGIC_HIGH_ACTIVE;
                UARTIF_uartPrintf(0, "flash_manager high active\n");
                needToInitList = TRUE;
                fmCtx.currentGcCounter = fmCtx.header1.gcCounter;
            } 
            else if (fmCtx.header1.statusMagic == SEGMENT_MAGIC_ACTIVE && fmCtx.header0.statusMagic == SEGMENT_MAGIC_ACTIVE)
            {
                // 垃圾回收中断，需要恢复
                // TO DO：
                fmCtx.gcInProgress = 1;
                UARTIF_uartPrintf(0, "flash_manager GC redo! \n");
                result = judgeWhichSegmentIsActive();
                if (result != FLASH_OK) {
                    resetSegments(TRUE, TRUE, SEGMENT_MAGIC_ACTIVE, SEGMENT_MAGIC_BACKUP, &result);
                }
                else
                {
                    needToInitList = TRUE;
                }
            } 
            else
            {
                UARTIF_uartPrintf(0, "flash_manager start to init sg 0 else! \n");
                // segment1激活，segment0备用
                resetSegments(TRUE, FALSE, SEGMENT_MAGIC_ACTIVE, 0, &result);
                UARTIF_uartPrintf(0, "flash_manager low active\n");
            }
        }
    }

    // 扫描激活segment构建映射表

    if (result == FLASH_OK) 
    {
        if (needToInitList)
        {
            result = scanSegmentPages();
        }
        else 
        {
            if (fmCtx.activeSegmentBaseStatus == MAGIC_LOW_ACTIVE)
            {
                fmCtx.nextWriteAddress = 0x0001;
            }
            else 
            {
                fmCtx.nextWriteAddress = 0x2001;
            }
            fmCtx.gcInProgress = 0;
        }
    }

    if ((result == FLASH_OK) && (fmCtx.gcInProgress == 1))
    {
        result = garbageCollect();
    }

    /* Populate per-slot color flags by reading existing image headers (if present) */
    // Disabled to prevent automatic loading of image headers on boot which may trigger unwanted display updates
    // if ((result == FLASH_OK) && (needToInitList))
    // {
    //     uint8_t kk;
    //     uint8_t jj;
    //     for (kk = 1; kk < 3; kk++)
    //     {
    //         for (jj = 0; jj < MAX_IMAGE_ENTRIES; jj++)
    //         {
    //             if (fmCtx.entries[kk][jj] != 0xffff)
    //             {
    //                 (void)readImageHeaderIntoBuffer((DATA_PAGE_MAGIC + kk), jj);
    //             }
    //         }
    //     }
    // }

    UARTIF_uartPrintf(0, "flash_manager FM_init about to return\n");
    return result;
}

/**
 * @brief 写入数据
 */
flash_result_t FM_writeData(uint8_t magic, uint16_t dataId, const uint8_t* data, uint16_t size)
{
    flash_result_t result = FLASH_OK;
    uint32_t crc32;
    uint32_t nextWriteAddress = 0;
    // uint8_t slotId;

    // 检查是否需要垃圾回收
    result = checkArguments(magic, dataId, data, size);
    if (result == FLASH_OK)
    {
        result = checkAndDoGarbageCollection();
        nextWriteAddress |= (uint32_t) (fmCtx.nextWriteAddress << 8u);

        if (fmCtx.activeSegmentBaseStatus == MAGIC_LOW_ACTIVE)
        {
            if (nextWriteAddress >= FLASH_SEGMENT1_BASE)
            {
                result = FLASH_ERROR_NO_SPACE;
            }
        }
        else 
        {
            if (nextWriteAddress >= FLASH_TOTAL_SIZE || nextWriteAddress < FLASH_SEGMENT1_BASE)
            {
                result = FLASH_ERROR_NO_SPACE;
            }
        }
        if (nextWriteAddress == (FLASH_SEGMENT0_BASE) || nextWriteAddress == (FLASH_SEGMENT1_BASE))
        {
            result = FLASH_ERROR_INVALID_PARAM;
        }
    }
    else
    {
        result = FLASH_ERROR_INVALID_PARAM;
    }

    if (result == FLASH_OK)
    {
        // 清空缓冲区
        memset(G_buffer1, 0, FLASH_PAGE_SIZE);

        // 将数据页字段复制到缓冲区
        G_buffer1[0] = magic; // 魔法数字
        G_buffer1[1] = (uint8_t)(dataId & 0xFF); // if image data, this byte is frameNum
        // slotId = G_buffer1[2] = (uint8_t)((dataId >> 8) & 0xFF); // if image data, this byte is slotId
        G_buffer1[2] = (uint8_t)((dataId >> 8) & 0xFF); // if image data, this byte is slotId
        G_buffer1[3] = size;
    
        // 计算CRC32（只计算数据部分）
        crc32 = calculate_crc32_default(data, size);
        G_buffer1[4] = (uint8_t)(crc32 & 0xFF);
        G_buffer1[5] = (uint8_t)((crc32 >> 8) & 0xFF);
        G_buffer1[6] = (uint8_t)((crc32 >> 16) & 0xFF);
        G_buffer1[7] = (uint8_t)((crc32 >> 24) & 0xFF);
    
        memcpy(&G_buffer1[8], data, size);

        // CRITICAL: DISABLE debug output during image transfer
        // This interferes with UART protocol communication (ACK/NAK responses)
        // UARTIF_uartPrintf(0, "flash_manager: write data to flash nextWriteAddress is 0x%08x! \n", nextWriteAddress);

        // 写入Flash
        if (W25Q32_WritePage(nextWriteAddress, G_buffer1, FLASH_PAGE_SIZE) != 0)
        {
            result = FLASH_ERROR_WRITE_FAIL;
        }
        else
        {
            // Write successful
        }
    }
    // 更新映射表
    if (result == FLASH_OK)
    {
        if (magic == DATA_PAGE_MAGIC || magic == MAGIC_BW_IMAGE_HEADER || magic == MAGIC_RED_IMAGE_HEADER)
        {
            fmCtx.entries[magic & 0x03][dataId] = fmCtx.nextWriteAddress;
            if (magic == MAGIC_BW_IMAGE_HEADER || magic == MAGIC_RED_IMAGE_HEADER)
            {
                // UARTIF_uartPrintf(0, "FM_writeData: Updated entries[%d][%d] = 0x%04x (magic=0x%02x)\r\n", 
                //                 magic & 0x03, dataId, fmCtx.nextWriteAddress, magic);
            }
        }
        else if (magic == MAGIC_BW_IMAGE_DATA || magic == MAGIC_RED_IMAGE_DATA)
        {
            // Image data written
        }
        else
        {
            result = FLASH_ERROR_INVALID_PARAM;
        }
        fmCtx.nextWriteAddress++;
    }
    
    return result;
}

/**
 * @brief 读取数据
 */
flash_result_t FM_readData(uint8_t magic, uint16_t dataId, uint8_t* data, uint8_t size)
{
    flash_result_t result = FLASH_OK;
    uint32_t calculatedCrc, storedCrc;
    uint8_t pageDataSize;
    uint32_t destAddress = 0;
    uint8_t readSize = size;
    // uint8_t slotId = 0u;
    uint8_t frameNum = 0u;

    // 在映射表中查找
    result = checkArguments(magic, dataId, data, size);
    if (result == FLASH_OK)
    {
        // UARTIF_uartPrintf(0, "flash_manager: read data from flash! \n");
        if (magic == DATA_PAGE_MAGIC || magic == MAGIC_BW_IMAGE_HEADER || magic == MAGIC_RED_IMAGE_HEADER)
        {
            if (fmCtx.entries[magic & 0x03][dataId] == 0xffff)
            {
                result = FLASH_ERROR_NOT_FOUND;
            }
            else
            {
                destAddress |= (uint32_t) (fmCtx.entries[magic & 0x03][dataId] << 8u);
            }
        }
        else if (magic == MAGIC_BW_IMAGE_DATA || magic == MAGIC_RED_IMAGE_DATA)
        {
            // slotId = (uint8_t)((dataId & 0xff00u) >> 8u);
            frameNum = (uint8_t)(dataId & 0xffu);
            if (G_imageAddressBuffer[frameNum] == 0xffff)
            {
                result = FLASH_ERROR_NOT_FOUND;
            }
            else
            {
                destAddress |= (uint32_t) (G_imageAddressBuffer[frameNum] << 8u);
            }
        }
        else
        {
            // do nothing
        }
    }
    else
    {
        result = FLASH_ERROR_INVALID_PARAM;
    }

    // 读取数据页
    if (result == FLASH_OK)
    {
        memset(G_buffer1, 0, FLASH_PAGE_SIZE);
        // 读取数据页到缓冲区
        if (W25Q32_ReadData(destAddress, G_buffer1, FLASH_PAGE_SIZE) != 0) 
        {
            result = FLASH_ERROR_READ_FAIL;
        }
    }
    
    if (result == FLASH_OK)
    {
        // 验证魔法数字
        if (G_buffer1[0] != magic)
        {
            result = FLASH_ERROR_INVALID_PARAM;
        }
    }
    
    // 验证CRC32（只验证数据部分）
    if (result == FLASH_OK)
    {
        // 从缓冲区解析数据页字段
        pageDataSize = G_buffer1[3];
        storedCrc = (uint32_t)G_buffer1[4] | ((uint32_t)G_buffer1[5] << 8) | 
                     ((uint32_t)G_buffer1[6] << 16) | ((uint32_t)G_buffer1[7] << 24);

        calculatedCrc = calculate_crc32_default(&G_buffer1[8], pageDataSize);
        if (calculatedCrc != storedCrc) 
        {
            result = FLASH_ERROR_CRC_FAIL;
        }
    }

    // 检查缓冲区大小
    if (result == FLASH_OK)
    {
        if (readSize > pageDataSize) 
        {
            readSize = pageDataSize;
            // 复制数据

            result = FLASH_ERROR_INVALID_PARAM;
        }

        memcpy(data, &G_buffer1[8], readSize);
    }
    
    return result;
}

/**
 * @brief 删除数据
 */
flash_result_t FM_deleteData(uint16_t dataId)
{
    flash_result_t result = FLASH_OK;
    if (dataId >= MAX_DATA_ENTRIES)
    {
        result = FLASH_ERROR_INVALID_PARAM;
    }

    if (result == FLASH_OK)
    {
        fmCtx.dataEntries[dataId] = 0xffff;
        fmCtx.gcInProgress = 1;
        result = garbageCollect();
    }
    return result;
}

/**
 * @brief 强制执行垃圾回收
 */
flash_result_t FM_forceGarbageCollect(void)
{
    return garbageCollect();
}

/**
 * @brief 写入图像头页
 */
flash_result_t FM_writeImageHeader(uint8_t magic, uint8_t slotId)
{
    flash_result_t result = FLASH_OK;
	uint16_t buf_addr1;
	uint16_t buf_addr0;

    if (magic != MAGIC_BW_IMAGE_HEADER && magic != MAGIC_RED_IMAGE_HEADER)
    {
        result = FLASH_ERROR_INVALID_PARAM;
    }

    if (slotId >= MAX_IMAGE_ENTRIES)
    {
        result = FLASH_ERROR_INVALID_PARAM;
    }

    if (result == FLASH_OK)
    {
        memset(G_imageAddressBuffer, 0xff, sizeof(G_imageAddressBuffer));
        result = scanImageDataPages(magic + 2u, slotId);
    }

    if (result == FLASH_OK)
    {
        // 清空缓冲区
        memset(G_buffer2, 0, FLASH_PAGE_SIZE);
        memcpy(G_buffer2, G_imageAddressBuffer, (MAX_FRAME_NUM + 1) * 2);
        
        // /* Append 1-byte color flag */
        // G_buffer2[(MAX_FRAME_NUM + 1) * 2] = (uint8_t)(lastIsRed);
        /* 写入 addresses + color flag */
        result = FM_writeData(magic, slotId, G_buffer2, (MAX_FRAME_NUM + 1) * 2);
        // if (result == FLASH_OK)
        // {
        //     if (slotId < MAX_IMAGE_ENTRIES)
        //     {
        //         fmCtx.imageSlotColor[slotId] = lastIsRed;
        //     }
        // }
    }
    return result;
}

/**
 * @brief 读取图像数据页
 */
flash_result_t FM_readImage(uint8_t magic, uint8_t slotId, uint8_t frameNum, uint8_t* data)
{
    static uint8_t lastMagicInBuffer = 0xff;
    static uint8_t lastSlotIdInBuffer = 0xff;
    flash_result_t result = FLASH_OK;
    uint16_t dataId = 0;
	uint8_t entriesIndex;
	uint16_t headerAddr;

    if (magic != MAGIC_BW_IMAGE_DATA && magic != MAGIC_RED_IMAGE_DATA)
    {
        result = FLASH_ERROR_INVALID_PARAM;
    }
    if (slotId >= MAX_IMAGE_ENTRIES || frameNum > MAX_FRAME_NUM)
    {
        result = FLASH_ERROR_INVALID_PARAM;
    }
    if (data == NULL)
    {
        result = FLASH_ERROR_INVALID_PARAM;
    }

    if (result == FLASH_OK)
    {
        if (magic != lastMagicInBuffer || slotId != lastSlotIdInBuffer)
        {
            memset(G_imageAddressBuffer, 0xff, sizeof(G_imageAddressBuffer));
            entriesIndex = (magic - 2u) & 0x03;
            headerAddr = fmCtx.entries[entriesIndex][slotId];
            // UARTIF_uartPrintf(0, "FM_readImage: magic=0x%02x idx=%d hdr=0x%04x\r\n", magic, entriesIndex, headerAddr);
            if (headerAddr == 0xffff)
            {
                result = FLASH_ERROR_NOT_FOUND;
            }
            else
            {
                result = readImageHeaderIntoBuffer((magic - 2u), slotId);
            }
            if (result == FLASH_OK)
            {
                lastMagicInBuffer = magic;
                lastSlotIdInBuffer = slotId;
            }
        }
    }

    if (result == FLASH_OK)
    {
        dataId = (uint16_t)slotId;
        dataId = dataId << 8u;
        dataId |= (uint16_t)frameNum;
        result = FM_readData(magic, dataId, data, PAYLOAD_SIZE);
        if (result != FLASH_OK)
        {
            // if (frameNum == 0) UARTIF_uartPrintf(0, "FM_readImage DATA: magic=0x%02x err=%d\r\n", magic, result);
        }
        else if (frameNum == 0)
        {
            // UARTIF_uartPrintf(0, "FM_readImage DATA OK: magic=0x%02x byte[0]=0x%02x\r\n", magic, data[0]);
        }
    }

    return result;
}
