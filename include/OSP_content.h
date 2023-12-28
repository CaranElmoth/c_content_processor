#ifndef OSP_CONTENT_H
#define OSP_CONTENT_H

#include <stdint.h>

/// @brief Constant representing the content type for PNG images
const uint8_t OSP_CNT_TYPE_PNG = 0;
/// @brief Constant representing the content type for tilemaps
const uint8_t OSP_CNT_TYPE_MAP = 1;
/// @brief Constant representing the maximum number of definable content types
const uint8_t OSP_CNT_MAX_TYPES = 255;

/// @brief Content table entry for asset bundle content table definition
typedef struct _osp_cnt_table_entry
{
    /// @brief Asset name w/o ext and directory prefix relative to bundle root
    char *name;
    /// @brief Asset content type byte ID
    uint8_t type;
    /// @brief Asset data start position in bundle file
    uint64_t start;
    /// @brief Asset data size in bundle file
    uint64_t size;
} osp_cnt_table_entry_t;

#endif