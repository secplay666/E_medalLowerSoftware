/******************************************************************************
 * @file image_transfer_v2.h
 * @brief Image transfer protocol V2 header
 ******************************************************************************/

#ifndef IMAGE_TRANSFER_V2_H
#define IMAGE_TRANSFER_V2_H

#include <stdint.h>

/******************************************************************************
 * Types
 ******************************************************************************/

typedef struct {
    uint8_t state;
    uint32_t frames_received;
    uint64_t frame_bitmap;
    uint8_t current_slot_id;
} image_transfer_stats_t;

/******************************************************************************
 * Public Functions
 ******************************************************************************/

/**
 * @brief Initialize image transfer V2 protocol
 */
void ImageTransferV2_Init(void);

/**
 * @brief Process image transfer (call periodically, e.g., 1ms)
 */
void ImageTransferV2_Process(void);

/**
 * @brief Get transfer statistics
 */
void ImageTransferV2_GetStats(image_transfer_stats_t *stats);

/**
 * @brief Reset transfer state
 */
void ImageTransferV2_Reset(void);

#endif // IMAGE_TRANSFER_V2_H
