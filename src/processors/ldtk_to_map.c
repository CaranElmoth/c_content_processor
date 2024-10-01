#include "processors/ldtk_to_map.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "cJSON.h"

// WARNING: this parser is very rough and WIP, it just extrapolates minimal
//          map data without much care for check or processing.

// Number of valid map tile layers supported
const int MAX_VALID_TILE_LAYERS = 16;
// Number of valid map collision layers supported
const int MAX_VALID_COLLISION_LAYERS = 16;
// Number of valid map entities layers supported
const int MAX_VALID_ENTITIES_LAYERS = 16;

// We'll need this structure to save valid layers
// info before data retrieval.
struct valid_layer
{
    uint16_t index;
    uint16_t order;
};

void register_collision_rectangle(
    collisions_layer_t *layer,
    uint32_t x,
    uint32_t y,
    uint32_t w,
    uint16_t h,
    uint32_t tile_size
)
{
    if(layer->num_rectangles >= MAX_DEFINABLE_COLLISION_RECTS - 1)
        return;

    layer->rectangles[layer->num_rectangles].x = x * tile_size;
    layer->rectangles[layer->num_rectangles].y = y * tile_size;
    layer->rectangles[layer->num_rectangles].w = w * tile_size;
    layer->rectangles[layer->num_rectangles].h = h * tile_size;

    layer->num_rectangles++;
}

uint8_t validate_collisions_layer(
    cJSON *layer_instance,
    uint32_t width,
    uint32_t height
)
{
    // If the layer name starts with "Collision"
    if(strncmp(
        "Collision",
        cJSON_GetStringValue(
            cJSON_GetObjectItemCaseSensitive(layer_instance, "__identifier")),
        9) == 0)
    {
        // Check validity
        // Let's fetch width and height in tiles
        cJSON *width_element =
            cJSON_GetObjectItemCaseSensitive(layer_instance,
            "__cWid");
        cJSON *height_element =
            cJSON_GetObjectItemCaseSensitive(layer_instance,
            "__cHei");
        uint32_t this_width = (uint32_t)cJSON_GetNumberValue(width_element);
        uint32_t this_height = (uint32_t)cJSON_GetNumberValue(height_element);
        
        // The layer is valid if both width and height match
        return (this_width == width && this_height == height);
    }

    // The layer is not valid
    return 0;
}

uint8_t validate_tiles_layer(
    cJSON *layer_instance,
    uint32_t *width,
    uint32_t *height,
    uint32_t *grid_size,
    char **tile_set)
{
    // Let's fetch width and height in tiles and the grid
    // size in pixels (only square tiles)
    cJSON *width_element = cJSON_GetObjectItemCaseSensitive(
        layer_instance,
        "__cWid");
    cJSON *height_element = cJSON_GetObjectItemCaseSensitive(
        layer_instance,
        "__cHei");
    cJSON *grid_size_element = cJSON_GetObjectItemCaseSensitive(
        layer_instance,
        "__gridSize");
    // Fetch the tileset path, too
    cJSON *tileset_element = cJSON_GetObjectItemCaseSensitive(
        layer_instance,
        "__tilesetRelPath");

    uint32_t this_width = (uint32_t)cJSON_GetNumberValue(width_element);
    uint32_t this_height = (uint32_t)cJSON_GetNumberValue(height_element);
    uint32_t this_grid_size = (uint32_t)cJSON_GetNumberValue(grid_size_element);
    // We only allow same size layers at the moment, so
    // did we already set the map width, height and grid size?
    if(*width > 0)
    {
        // Then check if this layer matches the previously
        // set dimensions. If not, then this layer is not valid.
        return (this_width == *width &&
                this_height == *height &&
                this_grid_size == *grid_size);
    }
    else
    {
        // This could be the first valid layer, so if the size
        // values are valid, use them for the whole map.
        if(this_width <= 0 ||
            this_height <= 0 ||
            this_grid_size <= 0)
        {
            return 0;
        }
        else
        {
            *width = this_width;
            *height = this_height;
            *grid_size = this_grid_size;

            // Let's save the tileset name, we will use it
            // to fetch a corresponding image from the bundle.
            // Just one tileset for the whole map, for now.
            char *tileset_file_name =
                cJSON_GetStringValue(tileset_element);
            char *last_dot = rindex(tileset_file_name, '.');
            int tile_set_len = 0;
            if(last_dot == NULL)
                tile_set_len = strlen(tileset_file_name);
            else
                tile_set_len = last_dot - tileset_file_name;
            *tile_set = malloc(tile_set_len + 1);
            strncpy(*tile_set, tileset_file_name, tile_set_len);
            (*tile_set)[tile_set_len] = '\0';

            return 1;
        }
    }
}

cJSON *map_json = NULL;

cJSON *parse_ldtk_file_for_levels(FILE *read_file)
{
    // Calculate the file size
    fseek(read_file, 0, SEEK_END);
    long size = ftell(read_file);
    // Read the whole file into memory
    char* file_content = (char*)malloc(size);
    rewind(read_file);
    fread(file_content, 1, size, read_file);

    // Let cJSON parse the file and build a json tree
    map_json = cJSON_Parse(file_content);
    // We don't need the file in memory anymore
    free(file_content);

    return cJSON_GetObjectItemCaseSensitive(map_json, "levels");
}

void free_json_data()
{
    cJSON_Delete(map_json);
}

void read_tiles_layer(
    tiles_layer_t *layer,
    cJSON *layer_instance,
    uint16_t layer_order,
    uint32_t tile_size,
    uint32_t map_width
    )
{
    // Fetch the tiles data
    cJSON *tiles_element =
        cJSON_GetObjectItemCaseSensitive(layer_instance, "gridTiles");
    
    // Alloc enough space to store them
    layer->num_tiles = cJSON_GetArraySize(tiles_element);    
    layer->tiles = malloc(sizeof(tile_source_t) * layer->num_tiles);

    layer->order = layer_order;

    cJSON* tile_element;
    cJSON* src_element;
    cJSON* px_element;
    int num_tile = 0;
    // Iterate all tiles
    cJSON_ArrayForEach(tile_element, tiles_element)
    {
        // Fetch the tile position in pixels
        px_element = cJSON_GetObjectItemCaseSensitive(tile_element, "px");
        uint32_t tile_x = 
            (uint32_t)cJSON_GetNumberValue(cJSON_GetArrayItem(px_element, 0));
        uint32_t tile_y = 
            (uint32_t)cJSON_GetNumberValue(cJSON_GetArrayItem(px_element, 1));

        // The tile being defined is the one indexed by its
        // position in pixels divided by tile size, to get
        // the tile position in grig elements.
        layer->tiles[num_tile].tile_idx = 
            ((tile_y / tile_size) * map_width) + (tile_x / tile_size);

        // Fetch the tile source
        src_element = cJSON_GetObjectItemCaseSensitive(tile_element, "src");
        // And then the X and Y position of the tile image in
        // the tileset
        layer->tiles[num_tile].source_x = 
            (uint32_t)cJSON_GetNumberValue(cJSON_GetArrayItem(src_element, 0));

        layer->tiles[num_tile].source_y =
            (uint32_t)cJSON_GetNumberValue(cJSON_GetArrayItem(src_element, 1));

        ++num_tile;
    }
}

#define REGISTER_CURRENT_RECT register_collision_rectangle(                    \
                                    layer,                                     \
                                    current_rect_x,                            \
                                    current_rect_y,                            \
                                    current_rect_w,                            \
                                    current_rect_h,                            \
                                    tile_size                                  \
                                );


void read_collisions_layer(
    collisions_layer_t *layer,
    cJSON *layer_instance,
    uint16_t layer_order,
    uint32_t width,
    uint32_t height,
    uint32_t tile_size
)
{
    layer->order = layer_order;

    // ...and then the collisions data
    cJSON *int_grid_element =
        cJSON_GetObjectItemCaseSensitive(layer_instance, "intGridCsv");

    // And reset the number of rectangles found
    layer->num_rectangles = 0;

    // Now read and parse the CSV collision grid to build
    // collision rectangles.
    int32_t collision_matrix[width][height];
    
    cJSON* val_element;
    int x = 0;
    int y = 0;
    // Iterate all the csv values and populate the matrix
    cJSON_ArrayForEach(val_element, int_grid_element)
    {
        collision_matrix[x][y] = (int32_t)cJSON_GetNumberValue(val_element);
        ++x;
        if (x >= width)
        {
            x = 0;
            ++y;
        }
    }

    // Let's scan the matrix and build rectangles
    uint32_t num_rectangles = 0;
    // The currently scanned rectangle origin and size in tiles
    uint32_t current_rect_x;
    uint32_t current_rect_y;
    uint32_t current_rect_w;
    uint32_t current_rect_h;
    uint32_t current_scan_line;
    // This is the scan state:
    //  0 - looking for a upper left corner
    //  1 - looking for a upper right corner
    //  2 - looking for current rectangle next line downwards
    //  3 - expanding current rectangle next line downwards
    uint8_t scan_state = 0;
    // If we have already found too many rectangles we stop.
    while(num_rectangles < MAX_DEFINABLE_COLLISION_RECTS)
    {
// Skip here if we have found a rectangle and we want to quickly restart
// and search for the next one.
scan_restart:
        scan_state = 0; // Let's start by looking for the upper left corner.
        // Scan the matrix row by row from left to right.
        for(int y = 0; y < height; ++y)
        {
            for(int x = 0; x < width; ++x)
            {
                switch(scan_state)
                {
                    // Looking for the upper left corner of a rectangle
                    case 0:
                    // Found it (the collision value is != 0)
                    if (collision_matrix[x][y] > 0)
                    {
                        // Reset the collision to 0, so we don't include this
                        // tile in the next scans.
                        collision_matrix[x][y] = 0;
                        // Save the rectangle upper left corner position
                        current_rect_x = x;
                        current_rect_y = y;
                        // Change state to continue scanning the first line
                        scan_state = 1;
                    }
                    break;
                    // Scanning the first rectangle line
                    case 1:
                    // As long as the collision is still != 0
                    if (collision_matrix[x][y] > 0)
                    {
                        // We clear the collision matrix, since the first line
                        // is always valid and we don't want to include it in
                        // the next scans.
                        collision_matrix[x][y] = 0;
                    }
                    else // If the collision ends, we have finished first line
                    {
                        // The rectangle width is defined and we save it.
                        current_rect_w = x - current_rect_x;
                        // If this is the last line, the rectangle can only
                        // end here vertically.
                        if (y == height - 1)
                        {
                            // So it's height must be 1 tile.
                            current_rect_h = 1;
                            // We can save the current rectangle and start
                            // scanning again.
                            REGISTER_CURRENT_RECT;
                            goto scan_restart;
                        }
                        else
                        {
                            // Otherwise, we start checking if there is a 
                            // contiguous collision line just below this one.
                            // Save the next scan line to check.
                            current_scan_line = y + 1;
                            // Change state to look for the start of the next
                            // scan line.
                            scan_state = 2;
                        }
                    }
                    break;
                    case 2:
                    // We went over the scan line we were checking without
                    // finding the collision start we were looking for.
                    if (y > current_scan_line)
                    {
                        // The rectangle height is set to the previous valid
                        // line.
                        current_rect_h = current_scan_line - current_rect_y;
                        // We can save the current rectangle and start
                        // scanning again.
                        REGISTER_CURRENT_RECT;
                        goto scan_restart;
                    }
                    // If we are on the current scan line and at the left
                    // edge of the current rectangle...
                    else if (y == current_scan_line && x == current_rect_x)
                    {
                        // ...and there is a valid collision
                        if (collision_matrix[x][y] > 0)
                        {
                            // Then there is a possible collision line to add
                            // to the rectangle. Let's move to the next state
                            // to check if it's contiguous until the right
                            // rectangle edge.
                            scan_state = 3;
                        }
                        else // Otherwise, this scan line can't be added.
                        {
                            // The rectangle height is set to the previous
                            // valid line.
                            current_rect_h = current_scan_line - current_rect_y;
                            // We can save the current rectangle and start
                            // scanning again.
                            REGISTER_CURRENT_RECT;
                            goto scan_restart;
                        }
                    }
                    break;
                    case 3:
                    // We are only interested in checking the current scan line
                    // from the left rectangle edge onwards.
                    if (y == current_scan_line && x >= current_rect_x)
                    {
                        // If we are still inside the rectangle...
                        if (x < (current_rect_x + current_rect_w))
                        {
                            // ...and there is a hole in the collision
                            if (collision_matrix[x][y] <= 0)
                            {
                                // This line is not contiguous and can't be
                                // added to the current rectangle.
                                // The rectangle height is set to the previous
                                // valid line.
                                current_rect_h = 
                                    current_scan_line - current_rect_y;
                                // We can save the current rectangle and start
                                // scanning again.
                                REGISTER_CURRENT_RECT;
                                goto scan_restart;
                            }
                        }
                        else // If we got over the right edge
                        {
                            // The collision is contiguous and we can add this
                            // line to the rectangle.
                            // Let's clear it so it's not checked in the next
                            // scan.
                            for (int next_x = current_rect_x;
                                 next_x < x;
                                 ++next_x)
                            {
                                collision_matrix[next_x][current_scan_line]
                                    = 0;
                            }

                            // If this is the last line, the rectangle can only
                            // end here vertically.
                            if (y == height - 1)
                            {
                                // The rectangle height is set to the current
                                // valid line.
                                current_rect_h =
                                    current_scan_line - current_rect_y + 1;
                                // We can save the current rectangle and
                                // start scanning again.
                                REGISTER_CURRENT_RECT;
                                goto scan_restart;
                            }
                            else // Otherwise we can check for another line...
                            {
                                // Let's move to the next possible valid line
                                current_scan_line = y + 1;
                                // And start again checking for the line start
                                scan_state = 2;
                            }
                        }
                    }
                    break;
                }
            }

            // If we are here, we are finished scanning a line
            // of the collision matrix. 
            // Let's see if we can continue scanning.

            switch(scan_state)
            {
                // If the line ended and we are still scanning the first
                // rectangle line, then the line can validly end here.
                case 1:
                // We set the width of the rectangle to end at the border
                // of the matrix.
                current_rect_w = width - current_rect_x - 1;
                // If this is the last line, the rectangle can only end here.
                if (y == height - 1)
                {
                    // We set the rectangle height to 1 tile.
                    current_rect_h = 1;
                    // We can save the current rectangle and start
                    // scanning again.
                    REGISTER_CURRENT_RECT;
                    goto scan_restart;
                }
                else
                {
                    // Otherwise, we start checking for the next possibly
                    // valid line to add...
                    current_scan_line = y + 1;
                    scan_state = 2;
                }
                break;
                // If the line ended and we were looking for the start of
                // the next rectangle scan line, means we can stop at
                // the previous line.
                case 2:
                // This makes sense only if we are on the scan line we are
                // checking, otherwise we might be still going
                // after the state change.
                if (y == current_scan_line)
                {
                    // The rectangle height is set to the previous valid
                    // scan line.
                    current_rect_h = current_scan_line - current_rect_y;
                    // We can save the current rectangle and start
                    // scanning again.
                    REGISTER_CURRENT_RECT;
                    goto scan_restart;
                }
                break;
                // If the line ended and we were checking if the current line
                // to add was contiguous, it means this line can be considered
                // valid and added to the rectangle.
                case 3:
                if (y == current_scan_line)
                {
                    // Let's clear it so it's not included in the next scans.
                    for (int next_x = current_rect_x; next_x < x; ++next_x)
                        collision_matrix[next_x][current_scan_line] = 0;

                    // If this is the last line, this rectangle can only end
                    // here vertically.
                    if (y == height - 1)
                    {
                        // The height is set to the current valid scan line.
                        current_rect_h = current_scan_line - current_rect_y + 1;
                        // We can save the current rectangle and start
                        // scanning again.
                        REGISTER_CURRENT_RECT;
                        goto scan_restart;
                    }
                    else
                    {
                        // Otherwise we start checking if the next possibly
                        // valid scan line is contigous and can be added to the
                        // rectangle.
                        current_scan_line = y + 1;
                        scan_state = 2;
                    }
                }
                break;
            }
        }
        // If the scan ended without finding any collision, our job is done and
        // we can break the cycle.
        if (scan_state == 0)
            break;
    }
}

void read_decor_entity(cJSON* entity_element, decor_entity_t *entity)
{
    // Fetch the entity position and size in pixels
    cJSON* px_element = cJSON_GetObjectItemCaseSensitive(entity_element, "px");
    entity->x = (uint32_t)cJSON_GetNumberValue(
        cJSON_GetArrayItem(px_element, 0));
    entity->y = (uint32_t)cJSON_GetNumberValue(
        cJSON_GetArrayItem(px_element, 1));
    entity->w = (uint32_t)cJSON_GetNumberValue(
        cJSON_GetObjectItemCaseSensitive(entity_element, "width"));
    entity->h = (uint32_t)cJSON_GetNumberValue(
        cJSON_GetObjectItemCaseSensitive(entity_element, "height"));

    cJSON* tile_element =
        cJSON_GetObjectItemCaseSensitive(entity_element, "__tile");
    entity->source_x = (uint32_t)cJSON_GetNumberValue(
        cJSON_GetObjectItemCaseSensitive(tile_element, "x"));
    entity->source_y = (uint32_t)cJSON_GetNumberValue(
        cJSON_GetObjectItemCaseSensitive(tile_element, "y"));
}

void read_other_entity_data(
    cJSON *extra_data_element,
    entity_data_t *data,
    int num_entities,
    uint8_t *is_decor_values,
    char **entity_iids,
    uint32_t *entity_to_decor_idx,
    uint32_t *entity_to_other_idx
)
{
    // Copy the data name
    char *data_name = cJSON_GetStringValue(
        cJSON_GetObjectItemCaseSensitive(
            extra_data_element,
            "__identifier")
        );
    size_t name_len = strlen(data_name);           
    data->name = malloc(name_len + 1);
    strncpy(data->name, data_name, name_len);
    data->name[name_len] = '\0';

    // Fetch the data type
    char *data_type = cJSON_GetStringValue(
        cJSON_GetObjectItemCaseSensitive(
            extra_data_element,
            "__type")
        );

    data->type = ENTITY_DATA_UNKNOWN;
    data->string_data = NULL;
    if (strncmp(data_type, "Int", 10) == 0)
        data->type = ENTITY_DATA_INT;
    else if (strncmp(data_type, "Float", 10) == 0)
        data->type = ENTITY_DATA_FLOAT;
    else if (strncmp(data_type, "String", 10) == 0)
        data->type = ENTITY_DATA_STRING;
    else if (strncmp(data_type, "EntityRef", 10) == 0)
        data->type = ENTITY_DATA_ENTITY;

    // Fetch the data value
    cJSON* data_value_element =
        cJSON_GetObjectItemCaseSensitive(extra_data_element, "__value");
    switch(data->type)
    {
        case ENTITY_DATA_INT:
            data->int_data = (int32_t)cJSON_GetNumberValue(data_value_element);
        break;
        case ENTITY_DATA_FLOAT:
            data->float_data = (float)cJSON_GetNumberValue(data_value_element);
        break;
        case ENTITY_DATA_STRING:
            char *data_value = cJSON_GetStringValue(data_value_element);
            size_t value_len = strlen(data_value);           
            data->string_data = malloc(value_len + 1);
            strncpy(data->string_data, data_value, value_len);
            data->string_data[value_len] = '\0';
        break;
        case ENTITY_DATA_ENTITY:
            char *entity_iid = cJSON_GetStringValue(
                cJSON_GetObjectItemCaseSensitive(
                    data_value_element, 
                    "entityIid")
            );

            for (int i_entity = 0; i_entity < num_entities; ++i_entity)
            {
                if (strncmp(entity_iid, entity_iids[i_entity], 40) == 0)
                {
                    data->entity_is_decor = is_decor_values[i_entity];
                    if (data->entity_is_decor)
                        data->entity_number = entity_to_decor_idx[i_entity];
                    else
                        data->entity_number = entity_to_other_idx[i_entity];
                }
            }
        break;
    }
}

void read_other_entity(
    cJSON* entity_element,
    entity_t *entity,
    int num_entities,
    uint8_t *is_decor_values,
    char **entity_iids,
    uint32_t *entity_to_decor_idx,
    uint32_t *entity_to_other_idx
)
{
    // Fetch the entity position and size in pixels
    cJSON *px_element = cJSON_GetObjectItemCaseSensitive(entity_element, "px");
    entity->x = (uint32_t)cJSON_GetNumberValue(
        cJSON_GetArrayItem(px_element, 0));
    entity->y = (uint32_t)cJSON_GetNumberValue(
        cJSON_GetArrayItem(px_element, 1));
    entity->w = (uint32_t)cJSON_GetNumberValue(
        cJSON_GetObjectItemCaseSensitive(entity_element, "width"));
    entity->h = (uint32_t)cJSON_GetNumberValue(
        cJSON_GetObjectItemCaseSensitive(entity_element, "height"));

    // Copy the entity type string
    char *entity_type = cJSON_GetStringValue(
        cJSON_GetObjectItemCaseSensitive(entity_element,"__identifier"));
    size_t type_len = strlen(entity_type);           
    entity->type = malloc(type_len + 1);
    strncpy(entity->type, entity_type, type_len);
    entity->type[type_len] = '\0';

    // Fetch entity extra data
    cJSON* extra_data_array_element = cJSON_GetObjectItemCaseSensitive(
        entity_element,
        "fieldInstances");
    entity->num_data = cJSON_GetArraySize(extra_data_array_element);
    if (entity->num_data > 0)
    {
        entity->data = malloc(sizeof(entity_data_t) * entity->num_data);
        int data_idx = 0;
        cJSON* extra_data_element;
        cJSON_ArrayForEach(extra_data_element, extra_data_array_element)
        {
            read_other_entity_data(
                extra_data_element,
                &(entity->data[data_idx]),
                num_entities,
                is_decor_values,
                entity_iids,
                entity_to_decor_idx,
                entity_to_other_idx);
            ++data_idx;
        }
    }
    else
        entity->data = NULL;
}

void read_entities_layer(
    entities_layer_t *layer,
    cJSON *layer_instance,
    uint16_t layer_order
    )
{
    // Fetch the entities data
    cJSON *entities_element =
        cJSON_GetObjectItemCaseSensitive(layer_instance, "entityInstances");
    
    cJSON* entity_element;
    cJSON* tags_item;
    cJSON* tag_item;
    cJSON* iid_item;
    int num_entities = cJSON_GetArraySize(entities_element);
    uint8_t is_decor_values[num_entities];
    uint32_t entity_to_decor_idx[num_entities];
    uint32_t entity_to_other_idx[num_entities];
    char* entity_iids[num_entities];
    int num_entity = 0;
    layer->num_decor_entities = 0;
    layer->num_entities = 0;
    // Count decor and other entities and fetch iids
    cJSON_ArrayForEach(entity_element, entities_element)
    {
        // Fetch the entity tags
        tags_item = cJSON_GetObjectItemCaseSensitive(entity_element, "__tags");
        is_decor_values[num_entity] = 0;
        cJSON_ArrayForEach(tag_item, tags_item)
        {
            if(strncmp(cJSON_GetStringValue(tag_item), "decor", strlen("decor")) 
                == 0)
            {
                is_decor_values[num_entity] = 1;
            }
        }

        if(is_decor_values[num_entity])
        {
            entity_to_decor_idx[num_entity] = layer->num_decor_entities;
            ++layer->num_decor_entities;
        }
        else
        {
            entity_to_other_idx[num_entity] = layer->num_entities;
            ++layer->num_entities;
        }

        // Fetch the entity iid
        iid_item = cJSON_GetObjectItemCaseSensitive(entity_element, "iid");
        entity_iids[num_entity] = cJSON_GetStringValue(iid_item);

        ++num_entity;
    }

    // Alloc enough space for storing them
    layer->entities = malloc(sizeof(entity_t) * layer->num_entities);
    layer->decor_entities = 
        malloc(sizeof(decor_entity_t) * layer->num_decor_entities);
    layer->order = layer_order;

    num_entity = 0;
    int decor_entity_idx = 0;
    int other_entity_idx = 0;
    // Iterate all entities
    cJSON_ArrayForEach(entity_element, entities_element)
    {
        if(is_decor_values[num_entity])
        {
            read_decor_entity(
                entity_element,
                &(layer->decor_entities[decor_entity_idx])
            );
            ++decor_entity_idx;
        }
        else
        {
            read_other_entity(
                entity_element,
                &(layer->entities[other_entity_idx]),
                num_entities,
                is_decor_values,
                entity_iids,
                entity_to_decor_idx,
                entity_to_other_idx
            );
            ++other_entity_idx;
        }

        ++num_entity;
    }
}

int ldtk_to_map(FILE* read_file, FILE* write_file, void* params)
{
    // Our map structure to fill with the data from the LDTK file
    tilemap_data_t tile_map;

    // Let's find the json levels array element
    cJSON *levels_json = parse_ldtk_file_for_levels(read_file);
    // If there is at least one level, we only read the first, for now.
    if(cJSON_GetArraySize(levels_json) > 0)
    {
        // Let's find this level's layers
        cJSON *layer_instances_json =
            cJSON_GetObjectItemCaseSensitive(levels_json->child,
                                             "layerInstances");
        int numLayerInstances = cJSON_GetArraySize(layer_instances_json);
        if(numLayerInstances > 0)
        {
            uint32_t width = 0;
            uint32_t height = 0;
            uint32_t grid_size = 0;
            uint8_t num_valid_tile_layers = 0;
            uint8_t num_valid_collision_layers = 0;
            uint8_t num_valid_entities_layers = 0;
            uint16_t layer_index = 0;
            uint16_t total_layers = 0;
            struct valid_layer
                valid_tile_layers[MAX_VALID_TILE_LAYERS];
            struct valid_layer
                valid_collision_layers[MAX_VALID_COLLISION_LAYERS];
            struct valid_layer
                valid_entities_layers[MAX_VALID_ENTITIES_LAYERS];
            cJSON* layer_instance;
            cJSON* typeElement;
            // Iterate all layers
            cJSON_ArrayForEach(layer_instance, layer_instances_json)
            {
                // Wich kind of layer is this?
                typeElement =
                    cJSON_GetObjectItemCaseSensitive(layer_instance, "__type");
                // This is a tiles layer
                if(strncmp(cJSON_GetStringValue(typeElement),
                           "Tiles",
                           10) == 0)
                {
                    // If this is a valid layer let's cache its index for
                    // later data retrieval
                    if(validate_tiles_layer(
                        layer_instance,
                        &width,
                        &height,
                        &grid_size,
                        &(tile_map.tile_set)))
                    {
                        valid_tile_layers[num_valid_tile_layers++] =
                            (struct valid_layer)
                            {
                                .index = layer_index,
                                .order = total_layers++
                            };
                    }
                } // This is an entities layer
                else if(strncmp(cJSON_GetStringValue(typeElement),
                                "Entities",
                                10) == 0)
                {
                    // Entity layers should always be valid, so let's cache
                    // its index for later data retrieval
                    valid_entities_layers[num_valid_entities_layers++] = 
                        (struct valid_layer)
                        {
                            .index = layer_index,
                            .order = total_layers++
                        };
                } // This is an int grid layer, could be a collision layer
                else if(strncmp(cJSON_GetStringValue(typeElement),
                                "IntGrid",
                                10) == 0)
                {
                    // If this is a valid layer let's cache its index for
                    // later data retrieval
                    if(validate_collisions_layer(layer_instance, width, height))
                    {
                        valid_collision_layers[num_valid_collision_layers++] =
                            (struct valid_layer)
                            {
                                .index = layer_index,
                                .order = total_layers++
                            };
                    }
                }
                
                ++layer_index;
            }

            tile_map.width = width;
            tile_map.height = height;
            tile_map.tile_size = grid_size;

            // Did we find at least one valid tile layer?
            if(num_valid_tile_layers > 0)
            {
                tile_map.num_tile_layers = num_valid_tile_layers;

                // Alloc space for the layers array
                tile_map.tile_layers =
                    (tiles_layer_t *)malloc(sizeof(tiles_layer_t) * 
                                            num_valid_tile_layers);

                // Let's iterate all valid layers again
                for(int i_layer = 0; i_layer < num_valid_tile_layers; ++i_layer)
                {
                    // Read this layer's data
                    read_tiles_layer(
                        &(tile_map.tile_layers[i_layer]),
                        cJSON_GetArrayItem(layer_instances_json,
                                           valid_tile_layers[i_layer].index),
                        total_layers - valid_tile_layers[i_layer].order - 1,
                        tile_map.tile_size,
                        tile_map.width
                    );
                }
            }

            // Did we find at least one valid collision layer?
            if(num_valid_collision_layers > 0)
            {
                tile_map.num_collision_layers = num_valid_collision_layers;

                // Alloc space for the layers array
                tile_map.collision_layers =
                    (collisions_layer_t *)malloc(sizeof(collisions_layer_t) * 
                                            num_valid_collision_layers);

                // Let's iterate all valid layers again
                for(int i_layer = 0;
                    i_layer < num_valid_collision_layers;
                    ++i_layer)
                {
                    // Read the layer's data
                    read_collisions_layer(
                        &(tile_map.collision_layers[i_layer]),
                        cJSON_GetArrayItem(layer_instances_json,
                                        valid_collision_layers[i_layer].index),
                        total_layers-valid_collision_layers[i_layer].order-1,
                        tile_map.width,
                        tile_map.height,
                        tile_map.tile_size
                    );
                }
            }

            // Did we find at least one valid entities layer?
            if(num_valid_entities_layers > 0)
            {
                tile_map.num_entity_layers = num_valid_entities_layers;

                // Alloc space for the layers array
                tile_map.entity_layers =
                    (entities_layer_t *)malloc(sizeof(entities_layer_t) * 
                                            num_valid_entities_layers);

                // Let's iterate all valid layers again
                cJSON *entitiesElement;
                for(int i_layer = 0;
                    i_layer < num_valid_entities_layers;
                    ++i_layer)
                {
                    // Read this layer's data.
                    read_entities_layer(
                        &(tile_map.entity_layers[i_layer]),
                        cJSON_GetArrayItem(layer_instances_json,
                                        valid_entities_layers[i_layer].index),
                        total_layers - valid_entities_layers[i_layer].order - 1
                    );
                }
            }
        }
    }

    // We are done, free the json tree memory.
    free_json_data();

    // Now write the tilemap data to file

    // First the tileset name
    size_t tilesetLength = strlen(tile_map.tile_set);
    fwrite(&tilesetLength, sizeof(tilesetLength), 1, write_file);
    fwrite(tile_map.tile_set, 1, tilesetLength, write_file);

    // Then the tile size in pixels
    fwrite(&(tile_map.tile_size), sizeof(tile_map.tile_size), 1, write_file);
    // Then map width and height in tiles
    fwrite(&(tile_map.width), sizeof(tile_map.width), 1, write_file);
    fwrite(&(tile_map.height), sizeof(tile_map.height), 1, write_file);

    // Finally the number of tile layers
    fwrite(&(tile_map.num_tile_layers), sizeof(tile_map.num_tile_layers),
           1, write_file);    
    // Now write all tile layers data: for each layer
    for(int i_layer = 0; i_layer < tile_map.num_tile_layers; ++i_layer)
    {
        // Write the layer order
        fwrite(&(tile_map.tile_layers[i_layer].order),
                sizeof(tile_map.tile_layers[i_layer].order),
                1,
                write_file);
        // Write the number of defined tiles for this layer
        fwrite(&(tile_map.tile_layers[i_layer].num_tiles),
                sizeof(tile_map.tile_layers[i_layer].num_tiles),
                1,
                write_file);
        // and for every tile
        for(int i_tile = 0;
            i_tile < tile_map.tile_layers[i_layer].num_tiles;
            ++i_tile)
        {
            // Write the tile idx
            fwrite(&(tile_map.tile_layers[i_layer].tiles[i_tile].tile_idx),
                   sizeof(tile_map.tile_layers[i_layer].tiles[i_tile].tile_idx),
                   1,
                   write_file);
            // Write the X, Y couple
            fwrite(&(tile_map.tile_layers[i_layer].tiles[i_tile].source_x),
                   sizeof(tile_map.tile_layers[i_layer].tiles[i_tile].source_x),
                   1,
                   write_file);
            fwrite(&(tile_map.tile_layers[i_layer].tiles[i_tile].source_y),
                   sizeof(tile_map.tile_layers[i_layer].tiles[i_tile].source_y),
                   1,
                   write_file);
        }
    }

    // Finally the number of collision layers
    fwrite(&(tile_map.num_collision_layers),
           sizeof(tile_map.num_collision_layers),
           1, write_file);    
    // Now write all collisions layers data: for each layer
    for(int i_layer = 0; i_layer < tile_map.num_collision_layers; ++i_layer)
    {
        // Write the layer order
        fwrite(&(tile_map.collision_layers[i_layer].order),
                sizeof(tile_map.collision_layers[i_layer].order),
                1,
                write_file);
        // Write the number of collision rectangles for this layer
        fwrite(&(tile_map.collision_layers[i_layer].num_rectangles),
                sizeof(tile_map.collision_layers[i_layer].num_rectangles),
                1,
                write_file);
        // and for every rectangle
        for(int i_rect = 0;
            i_rect < tile_map.collision_layers[i_layer].num_rectangles;
            ++i_rect)
        {
            // Write the rectangle position and size
            fwrite(
                &(tile_map.collision_layers[i_layer].rectangles[i_rect].x),
                sizeof(tile_map.collision_layers[i_layer].rectangles[i_rect].x),
                1, write_file);
            fwrite(
                &(tile_map.collision_layers[i_layer].rectangles[i_rect].y),
                sizeof(tile_map.collision_layers[i_layer].rectangles[i_rect].y),
                1, write_file);
            fwrite(
                &(tile_map.collision_layers[i_layer].rectangles[i_rect].w),
                sizeof(tile_map.collision_layers[i_layer].rectangles[i_rect].w),
                1, write_file);
            fwrite(
                &(tile_map.collision_layers[i_layer].rectangles[i_rect].h),
                sizeof(tile_map.collision_layers[i_layer].rectangles[i_rect].h),
                1, write_file);
        }
    }

    // Finally the number of entity layers
    fwrite(&(tile_map.num_entity_layers), sizeof(tile_map.num_entity_layers),
           1, write_file);    
    // Now write all entity layers data: for each layer
    for(int i_layer = 0; i_layer < tile_map.num_entity_layers; ++i_layer)
    {
        entities_layer_t *layer = &(tile_map.entity_layers[i_layer]);

        // Write the layer order
        fwrite(&(layer->order), sizeof(layer->order), 1, write_file);

        // Write the number of decor entities for this layer
        fwrite(&(layer->num_decor_entities),
                sizeof(layer->num_decor_entities),
                1,
                write_file);
        // and for every decor entity
        for(int i_entity = 0;
            i_entity < layer->num_decor_entities;
            ++i_entity)
        {
            // Write the entity source position
            fwrite(
                &(layer->decor_entities[i_entity].source_x),
                sizeof(layer->decor_entities[i_entity].source_x),
                1,
                write_file);
            fwrite(
                &(layer->decor_entities[i_entity].source_y),
                sizeof(layer->decor_entities[i_entity].source_y),
                1,
                write_file);

            // Write the entity position and size
            fwrite(
                &(layer->decor_entities[i_entity].x),
                sizeof(layer->decor_entities[i_entity].x),
                1,
                write_file);
            fwrite(&(layer->decor_entities[i_entity].y),
                   sizeof(layer->decor_entities[i_entity].y),
                   1,
                   write_file);
            fwrite(&(layer->decor_entities[i_entity].w),
                   sizeof(layer->decor_entities[i_entity].w),
                   1,
                   write_file);
            fwrite(&(layer->decor_entities[i_entity].h),
                   sizeof(layer->decor_entities[i_entity].h),
                   1,
                   write_file);
        }

        // Write the number of entities for this layer
        fwrite(&(layer->num_entities),
                sizeof(layer->num_entities),
                1,
                write_file);
        // and for every entity
        for(int i_entity = 0;
            i_entity < layer->num_entities;
            ++i_entity)
        {
            // Write the entity type
            size_t typeLength = 
                strlen(layer->entities[i_entity].type);
            fwrite(&typeLength, sizeof(typeLength), 1, write_file);
            fwrite(layer->entities[i_entity].type,
                1, typeLength, write_file);

            // Write the entity position and size
            fwrite(&(layer->entities[i_entity].x),
                   sizeof(layer->entities[i_entity].x),
                   1,
                   write_file);
            fwrite(&(layer->entities[i_entity].y),
                   sizeof(layer->entities[i_entity].y),
                   1,
                   write_file);
            fwrite(&(layer->entities[i_entity].w),
                   sizeof(layer->entities[i_entity].w),
                   1,
                   write_file);
            fwrite(&(layer->entities[i_entity].h),
                   sizeof(layer->entities[i_entity].h),
                   1,
                   write_file);

            // Write the entity extra data if present
            fwrite(&(layer->entities[i_entity].num_data),
                   sizeof(layer->entities[i_entity].num_data),
                   1,
                   write_file);

            for(int i_data = 0;
                i_data < layer->entities[i_entity].num_data;
                ++i_data)
            {
                // Write the data name
                size_t name_length = 
                    strlen(layer->entities[i_entity].data[i_data].name);
                fwrite(&name_length, sizeof(name_length), 1, write_file);
                fwrite(layer->entities[i_entity].data[i_data].name,
                    1, name_length, write_file);
                
                // Write the data type
                fwrite(&(layer->entities[i_entity].data[i_data].type),
                    sizeof(layer->entities[i_entity].data[i_data].type),
                    1,
                    write_file);

                // Write the data value if known
                switch(layer->entities[i_entity].data[i_data].type)
                {
                    case ENTITY_DATA_INT:
                    fwrite(&(layer->entities[i_entity].data[i_data].int_data),
                        sizeof(layer->entities[i_entity].data[i_data].int_data),
                        1,
                        write_file);
                    break;
                    case ENTITY_DATA_FLOAT:
                    fwrite(&(layer->entities[i_entity].data[i_data].float_data),
                        sizeof(
                            layer->entities[i_entity].data[i_data].float_data),
                        1,
                        write_file);
                    break;
                    case ENTITY_DATA_STRING:
                    size_t data_length = 
                        strlen(
                            layer->entities[i_entity].data[i_data].string_data);
                    fwrite(&data_length, sizeof(data_length), 1, write_file);
                    fwrite(layer->entities[i_entity].data[i_data].string_data,
                        1, data_length, write_file);
                    break;
                    case ENTITY_DATA_ENTITY:
                    fwrite(
                        &(layer->entities[i_entity].data[i_data].entity_is_decor),
                        sizeof(
                            layer->entities[i_entity].data[i_data].entity_is_decor),
                        1,
                        write_file);
                    fwrite(
                        &(layer->entities[i_entity].data[i_data].entity_number),
                        sizeof(
                            layer->entities[i_entity].data[i_data].entity_number),
                        1,
                        write_file);
                    break;
                }
            }
        }
    }

    // All data written to file, we can free the memory used:
    // First the tileset name
    free((void *)(tile_map.tile_set));
    // Then the rest of the layers data
    free_tilemap_layers(&tile_map);

    return 0;
}

void free_tilemap_entity(entity_t *entity)
{
    if (entity->num_data > 0)
    {
        for (int i_data = 0; i_data < entity->num_data; ++i_data)
        {
            if (entity->data[i_data].string_data != NULL)
                free(entity->data[i_data].string_data);
            free(entity->data[i_data].name);
        }

        free(entity->data);
    }

    free(entity->type);
}

void free_tilemap_entities_layer(entities_layer_t *layer)
{
    for(int i_entity = 0; i_entity < layer->num_entities; ++i_entity)
    {
        free_tilemap_entity(&(layer->entities[i_entity]));
    }

    free(layer->entities);
    free(layer->decor_entities);
}

void free_tilemap_layers(tilemap_data_t* tile_map)
{
    // Free all tiles data for every layer
    for(int i_layer = 0; i_layer < tile_map->num_tile_layers; ++i_layer)
        free(tile_map->tile_layers[i_layer].tiles);

    // Free the tile layers array
    tile_map->num_tile_layers = 0;    
    free(tile_map->tile_layers);

    // Free the collision layers array (layer data is not dynamically allocated
    // in this case).
    tile_map->num_collision_layers = 0;
    free(tile_map->collision_layers);

    // Free all entities data for every layer
    for(int i_layer = 0; i_layer < tile_map->num_entity_layers; ++i_layer)
        free_tilemap_entities_layer(&(tile_map->entity_layers[i_layer]));

    // Free the entity layers array
    tile_map->num_entity_layers = 0;    
    free(tile_map->entity_layers);
}