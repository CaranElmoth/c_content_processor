#ifndef LDTK_TO_MAP_H
#define LDTK_TO_MAP_H

#include <stdio.h>
#include <stdint.h>

/// @brief Tile data structure
typedef struct _tile_source
{
    /// @brief X position in pixels in tileset image
    uint32_t source_x;
    /// @brief Y position in pixels in tileset image
    uint32_t source_y;
} tile_source_t;

/// @brief Tilemap layer data structure
typedef struct _tiles_layer
{
    /// @brief Tiles data array (width x height size)
    tile_source_t* tiles;
} tiles_layer_t;

/// @brief Tilemap asset data structure
typedef struct _tilemap_data
{
    /// @brief Tileset image asset name
    char* tile_set;
    /// @brief Tile size in pixels (only square tiles are supported)
    uint32_t tile_size;
    /// @brief Map width in tiles (only same size layers are supported)
    uint32_t width;
    /// @brief Map height in tiles (only same size layers are supported)
    uint32_t height;
    /// @brief Number of map layers
    uint8_t num_layers;
    /// @brief Layers data array
    tiles_layer_t* layers;
} tilemap_data_t;

/// @brief Allocates memory for successive tile map data reading
/// @param tile_map Tilemap data structure to fill with the new data pointers
/// @param width Tilemap width in tiles
/// @param height Tilemap height in tiles
/// @param num_layers Tilemap number of layers
void alloc_tilemap_layers(tilemap_data_t* tile_map,
                          int width,
                          int height,
                          int num_layers);
/// @brief Frees previously allocated tilemap data memory
/// @param tile_map Tilemap data structure to be freed
void free_tilemap_layers(tilemap_data_t* tile_map);
/// @brief LDTK tile map file to tile map MAP asset converter
/// @param readFile Input FILE containing the LDTK map
/// @param writeFIle Output bundle FILE to write data to
/// @param params Optional converter parameters
/// @return 0 on successful conversion, error value otherwise
int ldtk_to_map(FILE* readFile, FILE* writeFIle, void* params);

#endif