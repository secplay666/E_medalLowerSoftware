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
 ** @file image_transfer.h
 **
 ** @brief Header file for image transfer functions
 **
 ** @author MADS Team
 **
 ******************************************************************************/

#ifndef IMAGE_TRANSFER_H
#define IMAGE_TRANSFER_H

/******************************************************************************
 * Include files
 ******************************************************************************/
#include "base_types.h"
#include "flash_config.h"

/******************************************************************************
 * Global type definitions
 ******************************************************************************/

/**
 * @brief Image type enumeration
 */
typedef enum {
    IMAGE_TYPE_BW = 0,      // Black/White image
    IMAGE_TYPE_RED = 1      // Red image
} ImageType;

/******************************************************************************
 * Global function prototypes
 ******************************************************************************/

/**
 * @brief Initialize image transfer module
 * @details Resets all transfer state variables and buffers
 */
void ImageTransfer_Init(void);

/**
 * @brief Process image transfer state machine
 * @details Should be called periodically to handle incoming image data
 */
void ImageTransfer_Process(void);

/**
 * @brief Print diagnostic information about image transfer status
 * @details Outputs current state, buffer status, and statistics
 */
void ImageTransfer_PrintDiagnostics(void);

#endif // IMAGE_TRANSFER_H

/******************************************************************************
 * EOF (not truncated)
 ******************************************************************************/
