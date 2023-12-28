#include "processors/ldtk_to_map.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "cJSON.h"

// WARNING: this parser is very rough and WIP, it just extrapolates minimal
//          map data without much care for check or processing.

// Number of valid map layers supported
const int MAX_VALID_LAYERS = 16;

int ldtk_to_map(FILE* readFile, FILE* writeFile, void* params)
{
    // Calculate the file size
    fseek(readFile, 0, SEEK_END);
    long size = ftell(readFile);
    // Read the whole file into memory
    char* fileContent = (char*)malloc(size);
    rewind(readFile);
    fread(fileContent, 1, size, readFile);    

    // Let cJSON parse the file and build a json tree
    cJSON *mapJson = cJSON_Parse(fileContent);
    // We don't need the file in memory anymore
    free(fileContent);

    // Our map structure to fill with the data from the LDTK file
    tilemap_data_t tile_map;

    // Let's find the json levels array element
    cJSON *levelsJson = cJSON_GetObjectItemCaseSensitive(mapJson, "levels");
    // How many are there?
    int numLevels = cJSON_GetArraySize(levelsJson);
    // We only read the first, for now.
    if(numLevels > 0)
    {
        // Let's find this level's layers
        cJSON *layerInstancesJson =
            cJSON_GetObjectItemCaseSensitive(levelsJson->child,
                                             "layerInstances");
        int numLayerInstances = cJSON_GetArraySize(layerInstancesJson);
        if(numLayerInstances > 0)
        {
            uint32_t width = 0;
            uint32_t height = 0;
            uint32_t grid_size = 0;
            uint8_t num_valid_layers = 0;
            uint16_t layer_index = 0;
            uint16_t valid_layers[MAX_VALID_LAYERS];
            cJSON* layerInstance;
            cJSON* widthElement;
            cJSON* heightElement;
            cJSON* tilesetElement;
            cJSON* typeElement;
            cJSON* gridSizeElement;
            // Iterate all layers
            cJSON_ArrayForEach(layerInstance, layerInstancesJson)
            {
                // Wich kind of layer is this?
                typeElement =
                    cJSON_GetObjectItemCaseSensitive(layerInstance, "__type");
                // We only process tiles layer, for now
                if(strncmp(cJSON_GetStringValue(typeElement), "Tiles", 8) == 0)
                {
                    // Let's fetch width and height in tiles and the grid
                    // size in pixels (only square tiles)
                    widthElement =
                        cJSON_GetObjectItemCaseSensitive(layerInstance,
                        "__cWid");
                    heightElement =
                        cJSON_GetObjectItemCaseSensitive(layerInstance,
                        "__cHei");
                    gridSizeElement =
                        cJSON_GetObjectItemCaseSensitive(layerInstance,
                        "__gridSize");
                    // Fetch the tileset path, too
                    tilesetElement =
                        cJSON_GetObjectItemCaseSensitive(layerInstance,
                        "__tilesetRelPath");
                    uint8_t valid = 1;
                    uint32_t current_width =
                        (uint32_t)cJSON_GetNumberValue(widthElement);
                    uint32_t current_height =
                        (uint32_t)cJSON_GetNumberValue(heightElement);
                    uint32_t current_grid_size =
                        (uint32_t)cJSON_GetNumberValue(gridSizeElement);
                    // We only allow same size layers at the moment, so
                    // did we already set the map width, height and grid size?
                    if(width > 0)
                    {
                        // Then check if this layer matches the previously
                        // set dimensions. If not, then this layer is not valid.
                        if(current_width != width ||
                           current_height != height ||
                           current_grid_size != grid_size)
                            valid = 0;
                    }
                    else
                    {
                        // This could be the first valid layer, so if the size
                        // values are valid, use them for the whole map.
                        if(current_width <= 0 ||
                           current_height <= 0 ||
                           current_grid_size <= 0)
                        {
                            valid = 0;
                        }
                        else
                        {
                            width = current_width;
                            height = current_height;
                            grid_size = current_grid_size;

                            // Let's save the tileset name only, we will use it
                            // to fetch a corresponding image from the bundle.
                            // Just one tileset for the whole map, for now.
                            char *tileset_file_name =
                                cJSON_GetStringValue(tilesetElement);
                            char *last_dot = rindex(tileset_file_name, '.');
                            int tile_set_len = 0;
                            if(last_dot == NULL)
                                tile_set_len = strlen(tileset_file_name);
                            else
                                tile_set_len = last_dot - tileset_file_name;
                            tile_map.tile_set = malloc(tile_set_len + 1);
                            strncpy(tile_map.tile_set,
                                    tileset_file_name,
                                    tile_set_len);
                            tile_map.tile_set[tile_set_len] = '\0';
                        }
                    }

                    // If this is a valid layer let's cache its index for
                    // later tiles data retrieval
                    if(valid)
                    {
                        valid_layers[num_valid_layers] = layer_index;
                        ++num_valid_layers;
                    }
                }
                
                ++layer_index;
            }

            // Did we find at least one valid layer?
            if(num_valid_layers > 0)
            {
                // Let's alloc some space to fetch the tiles data.
                cJSON *tilesElement;
                tile_map.tile_size = grid_size;
                alloc_tilemap_layers(&tile_map,
                                     width, height,
                                     num_valid_layers);
                // Let's iterate all valid layers again
                for(int iLayer = 0; iLayer < num_valid_layers; ++iLayer)
                {
                    // Fetch the layer instance...
                    layerInstance =
                        cJSON_GetArrayItem(layerInstancesJson,
                                           valid_layers[iLayer]);
                    // ...and then the tiles data
                    tilesElement =
                        cJSON_GetObjectItemCaseSensitive(layerInstance,
                                                         "gridTiles");
                    cJSON* tileElement;
                    cJSON* srcElement;
                    int iTile = 0;
                    // Iterate all tiles
                    cJSON_ArrayForEach(tileElement, tilesElement)
                    {
                        // Fetch the tile source
                        srcElement =
                            cJSON_GetObjectItemCaseSensitive(tileElement,
                                                             "src");
                        // And then the X and Y position of the tile image in
                        // the tileset
                        tile_map.layers[iLayer].tiles[iTile].source_x = 
                            (uint32_t)cJSON_GetNumberValue(
                                cJSON_GetArrayItem(srcElement, 0)
                                );
                        tile_map.layers[iLayer].tiles[iTile].source_y =
                            (uint32_t)cJSON_GetNumberValue(
                                cJSON_GetArrayItem(srcElement, 1)
                                );
                        ++iTile;
                    }
                }
            }
        }
    }

    // We are done, free the json tree memory.
    cJSON_Delete(mapJson);

    // Now write the tilemap data to file

    // First the tileset name
    size_t tilesetLength = strlen(tile_map.tile_set);
    fwrite(&tilesetLength, sizeof(tilesetLength), 1, writeFile);
    fwrite(tile_map.tile_set, 1, tilesetLength, writeFile);

    // Then the tile size in pixels
    fwrite(&(tile_map.tile_size), sizeof(tile_map.tile_size), 1, writeFile);
    // Then map width and height in tiles
    fwrite(&(tile_map.width), sizeof(tile_map.width), 1, writeFile);
    fwrite(&(tile_map.height), sizeof(tile_map.height), 1, writeFile);
    // Finally the number of layers
    fwrite(&(tile_map.num_layers), sizeof(tile_map.num_layers), 1, writeFile);
    
    int num_tiles = tile_map.width * tile_map.height;
    // Now write all layers data: for each layer
    for(int i_layer = 0; i_layer < tile_map.num_layers; ++i_layer)
    {
        // and for every tile
        for(int i_tile = 0; i_tile < num_tiles; ++i_tile)
        {
            // Write the X, Y couple
            fwrite(&(tile_map.layers[i_layer].tiles[i_tile].source_x),
                   sizeof(tile_map.layers[i_layer].tiles[i_tile].source_x),
                   1,
                   writeFile);
            fwrite(&(tile_map.layers[i_layer].tiles[i_tile].source_y),
                   sizeof(tile_map.layers[i_layer].tiles[i_tile].source_y),
                   1,
                   writeFile);
        }
    }

    // All data written to file, we can free the memory used:
    // First the tileset name
    free((void *)(tile_map.tile_set));
    // Then the rest of the layers data
    free_tilemap_layers(&tile_map);

    return 0;
}

void alloc_tilemap_layers(tilemap_data_t* tile_map,
                          int width,
                          int height,
                          int num_layers)
{
    // Set the layers size info in the tilemap structure
    tile_map->width = width;
    tile_map->height = height;
    tile_map->num_layers = num_layers;

    int num_tiles = width * height;

    // Alloc space for the layers array
    tile_map->layers =
        (tiles_layer_t *)malloc(sizeof(tiles_layer_t) * num_layers);

    // Then, for every layer, alloc space for the tiles data
    for(int i_layer = 0; i_layer < num_layers; ++i_layer)
        tile_map->layers[i_layer].tiles =
            malloc(sizeof(tile_source_t) * num_tiles);
}

void free_tilemap_layers(tilemap_data_t* tile_map)
{
    // Free all tiles data for every layer
    for(int i_layer = 0; i_layer < tile_map->num_layers; ++i_layer)
        free(tile_map->layers[i_layer].tiles);

    // Free the layers array
    tile_map->num_layers = 0;    
    free(tile_map->layers);
}