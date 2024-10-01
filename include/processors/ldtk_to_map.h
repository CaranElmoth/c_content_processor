#ifndef LDTK_TO_MAP_H
#define LDTK_TO_MAP_H

#include <stdio.h>
#include <stdint.h>

/// @brief Tile data structure
typedef struct _tile_source
{
    /// @brief Index of the tile in the linear representation of the tiles grid
    uint32_t tile_idx;
    /// @brief X position in pixels in tileset image
    uint32_t source_x;
    /// @brief Y position in pixels in tileset image
    uint32_t source_y;
} tile_source_t;

/// @brief Collision rectangle data structure
typedef struct _collision_rect
{
    /// @brief Rectangle origin x position in pixels
    uint32_t x;
    /// @brief Rectangle origin y position in pixels
    uint32_t y;
    /// @brief Rectangle width in pixels
    uint32_t w;
    /// @brief Rectangle height in pixels
    uint32_t h;
} collision_rect_t;

typedef enum
{
    ENTITY_DATA_INT,
    ENTITY_DATA_FLOAT,
    ENTITY_DATA_STRING,
    ENTITY_DATA_ENTITY,
    ENTITY_DATA_UNKNOWN
} entity_data_type_t;

typedef struct _entity_data
{
    /// @brief Data type as enum
    entity_data_type_t type;
    /// @brief Data name as string
    char *name;
    /// @brief String data
    char *string_data;
    /// @brief Integer data
    int32_t int_data;
    /// @brief Float data
    float float_data;
    /// @brief Referenced entity number
    uint32_t entity_number;
    /// @brief Referenced entity type
    uint8_t entity_is_decor;
} entity_data_t;

/// @brief Entity data structure
typedef struct _entity
{
    /// @brief Entity type as string
    char *type;
    /// @brief Entity x position in pixels
    uint32_t x;
    /// @brief Entity y position in pixels
    uint32_t y;
    /// @brief Entity width in pixels
    uint32_t w;
    /// @brief Entity height in pixels
    uint32_t h;
    /// @brief Number of extra data elements
    uint32_t num_data;
    /// @brief Array of extra data elements
    entity_data_t *data;
} entity_t;

/// @brief Entity data structure
typedef struct _decor_entity
{
    /// @brief Decor x source position
    uint32_t source_x;
    /// @brief Decor y source position
    uint32_t source_y;
    /// @brief Entity x position in pixels
    uint32_t x;
    /// @brief Entity y position in pixels
    uint32_t y;
    /// @brief Entity width in pixels
    uint32_t w;
    /// @brief Entity height in pixels
    uint32_t h;
} decor_entity_t;

/// @brief Tilemap entities layer data structure
typedef struct _entities_layer
{
    /// @brief Layer order
    uint16_t order;
    /// @brief Number of entities defined in this layer
    uint32_t num_decor_entities;
    /// @brief Entities data array
    decor_entity_t* decor_entities;
    /// @brief Number of entities defined in this layer
    uint32_t num_entities;
    /// @brief Entities data array
    entity_t* entities;
} entities_layer_t;

#define MAX_DEFINABLE_COLLISION_RECTS 4096

/// @brief Tilemap collisions layer data structure
typedef struct _collisions_layer
{
    /// @brief Layer order
    uint16_t order;
    /// @brief Number of collision rectangles defined in this layer
    uint32_t num_rectangles;
    /// @brief Collisions data array
    collision_rect_t rectangles[MAX_DEFINABLE_COLLISION_RECTS];
} collisions_layer_t;

/// @brief Tilemap tiles layer data structure
typedef struct _tiles_layer
{
    /// @brief Layer order
    uint16_t order;
    /// @brief Number of tiles defined in this layer
    uint32_t num_tiles;
    /// @brief Tiles data array
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
    /// @brief Number of map tile layers
    uint8_t num_tile_layers;
    /// @brief Tile layers data array
    tiles_layer_t* tile_layers;
    /// @brief Number of map collision layers
    uint8_t num_collision_layers;
    /// @brief Layers data array
    collisions_layer_t* collision_layers;
    /// @brief Number of map entity layers
    uint8_t num_entity_layers;
    /// @brief Layers data array
    entities_layer_t* entity_layers;
} tilemap_data_t;

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