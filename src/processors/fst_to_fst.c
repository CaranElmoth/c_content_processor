#include "processors/fst_to_fst.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include "dynarray.h"
#include <errno.h>

/// @brief Frame rectangle data structure
typedef struct _frame_rect
{
    /// @brief Rectangle origin x position in pixels
    uint32_t x;
    /// @brief Rectangle origin y position in pixels
    uint32_t y;
    /// @brief Rectangle width in pixels
    uint32_t w;
    /// @brief Rectangle height in pixels
    uint32_t h;
} frame_rect_t;

typedef struct _frame_sequence
{
    uint32_t num_frames;
    float duration;
    frame_rect_t *frames;
} frame_sequence_t;

struct parser_state
{
    int32_t frame_width;
    int32_t frame_height;
    int32_t grid_width;
    int32_t grid_height;
    int32_t row_offset;
    int32_t column_offset;
    int32_t row;
    int32_t column;

    uint8_t state;
} g_parser_state;

#define MAX_LINE 1024

#define LINE_STATE_START        0
#define LINE_STATE_FWIDTH       1
#define LINE_STATE_FHEIGHT      2
#define LINE_STATE_GRID_W       3
#define LINE_STATE_GRID_H       4
#define LINE_STATE_ROW_OFFSET   5
#define LINE_STATE_COL_OFFSET   6
#define LINE_STATE_ROW          7
#define LINE_STATE_COL          8
#define LINE_STATE_SEQ          9
#define LINE_STATE_SEQ_FRAME_1  10
#define LINE_STATE_SEQ_FRAME_2  11
#define LINE_STATE_SEQ_FRAME_3  12
#define LINE_STATE_SEQ_FRAME_4  13
#define LINE_STATE_SEQ_FRAME_C  14

uint8_t starts_with(const char *prefix, const char *string)
{
    return strncmp(prefix, string, strlen(prefix)) == 0;
}

void skip_prefix(char *prefix, char **string)
{
    while(*prefix == **string)
    {
        ++prefix;
        ++(*string);
    }
}

//#define SKIP_LINE while(*c != '\n' && *c != '\0' && c < end) ++c

int fst_to_fst(FILE* read_file, FILE* write_file, void* params)
{
    osp_dynarray_t sequences_array = osp_dynarray_new(sizeof(frame_sequence_t), 16, 16);
    osp_dynarray_t frames_array = osp_dynarray_new(sizeof(frame_rect_t), 16, 16);

    frame_sequence_t current_sequence;
    frame_rect_t current_frame;

    g_parser_state.frame_width = -1;
    g_parser_state.frame_height = -1;
    g_parser_state.grid_width = -1;
    g_parser_state.grid_height = -1;
    g_parser_state.row_offset = 0;
    g_parser_state.column_offset = 0;
    g_parser_state.row = -1;
    g_parser_state.column = -1;

    char line[1024];
    uint32_t line_num = 0;
    while(fgets(line, MAX_LINE, read_file))
    {
        g_parser_state.state = LINE_STATE_START;
        int32_t line_len = MAX_LINE;
        char *c = line;
        char *end = line + MAX_LINE;
        char *after_conv = NULL;
        uint8_t line_over = 0;
        
        for(; c < end; ++c)
        {
            if(line_over)
                break;

            switch(g_parser_state.state)
            {
                case LINE_STATE_START:
                if(isspace(*c))
                {
                    --line_len;
                }
                else if(*c == '.' || isdigit(*c))
                {
                    osp_dynarray_clear(frames_array);
                    current_sequence.frames = NULL;
                    current_sequence.num_frames = 0;
                    errno = 0;
                    current_sequence.duration = strtof(c, &after_conv);
                    if(errno != 0)
                    {
                        printf("Error converting frame sequence duration on line %d. Skipping line.\n", line_num);
                        line_over = 1;
                    }
                    else
                    {
                        c = after_conv;
                        if(g_parser_state.frame_width > 0)
                        {
                            current_frame.w = g_parser_state.frame_width;
                            if(g_parser_state.grid_width > 0)
                                current_frame.w *= g_parser_state.grid_width;
                        }
                        if(g_parser_state.frame_height > 0)
                        {
                            current_frame.h = g_parser_state.frame_height;
                            if(g_parser_state.grid_height > 0)
                                current_frame.h *= g_parser_state.grid_height;
                        }
                        if(g_parser_state.column >= 0)
                        {
                            current_frame.x = g_parser_state.column;
                            if(g_parser_state.grid_width > 0)
                                current_frame.x *= g_parser_state.grid_width;
                        }
                        else if(g_parser_state.row >= 0)
                        {
                            current_frame.y = g_parser_state.row;
                            if(g_parser_state.grid_height > 0)
                                current_frame.y *= g_parser_state.grid_height;
                        }
                        g_parser_state.state = LINE_STATE_SEQ;
                    }
                }
                else if(starts_with(WIDTH_CMD, c))
                {
                    skip_prefix(WIDTH_CMD, &c);
                    g_parser_state.state = LINE_STATE_FWIDTH;
                }
                else if(starts_with(HEIGHT_CMD, c))
                {
                    skip_prefix(HEIGHT_CMD, &c);
                    g_parser_state.state = LINE_STATE_FHEIGHT;
                }
                else if(starts_with(ROW_CMD, c))
                {
                    skip_prefix(ROW_CMD, &c);
                    g_parser_state.state = LINE_STATE_ROW;
                }
                else if(starts_with(COLUMN_CMD, c))
                {
                    skip_prefix(COLUMN_CMD, &c);
                    g_parser_state.state = LINE_STATE_COL;
                }
                else if(starts_with(GRID_CMD, c))
                {
                    skip_prefix(GRID_CMD, &c);
                    g_parser_state.state = LINE_STATE_GRID_W;
                }
                else if(starts_with(ROW_OFFSET_CMD, c))
                {
                    skip_prefix(ROW_OFFSET_CMD, &c);
                    g_parser_state.state = LINE_STATE_ROW_OFFSET;
                }
                else if(starts_with(COLUMN_OFFSET_CMD, c))
                {
                    skip_prefix(COLUMN_OFFSET_CMD, &c);
                    g_parser_state.state = LINE_STATE_COL_OFFSET;
                }
                else
                {
                    printf("Skipping invalid line %d.\n", line_num);
                    line_over = 1;
                }
                break;
                case LINE_STATE_FWIDTH:
                if(isdigit(*c))
                {
                    errno = 0;
                    int32_t prev_fwidth = g_parser_state.frame_width;
                    g_parser_state.frame_width = strtol(c, &after_conv, 10);
                    if (errno != 0)
                    {
                        g_parser_state.frame_width = prev_fwidth;
                        printf("Error converting frame width on line %d. Skipping line.\n", line_num);
                    }
                    line_over = 1;
                }
                else if(*c == '\n' || *c == '\0')
                    line_over = 1;
                break;
                case LINE_STATE_FHEIGHT:
                if(isdigit(*c))
                {
                    errno = 0;
                    int32_t prev_fheight = g_parser_state.frame_height;
                    g_parser_state.frame_height = strtol(c, &after_conv, 10);
                    if (errno != 0)
                    {
                        g_parser_state.frame_height = prev_fheight;
                        printf("Error converting frame height on line %d. Skipping line.\n", line_num);
                    }
                    line_over = 1;
                }
                else if(*c == '\n' || *c == '\0')
                    line_over = 1;
                break;
                case LINE_STATE_GRID_W:
                if(isdigit(*c) || *c == '-')
                {
                    errno = 0;
                    int32_t prev_grid_w = g_parser_state.grid_width;
                    g_parser_state.grid_width = strtol(c, &after_conv, 10);
                    if (errno != 0)
                    {
                        g_parser_state.grid_width = prev_grid_w;
                        printf("Error converting grid width on line %d. Skipping line.\n", line_num);
                        line_over = 1;
                    }
                    else
                    {
                        c = after_conv;
                        g_parser_state.state = LINE_STATE_GRID_H;
                    }
                }
                else if(*c == '\n' || *c == '\0')
                    line_over = 1;
                break;
                case LINE_STATE_GRID_H:
                if(isdigit(*c) || *c == '-')
                {
                    errno = 0;
                    int32_t prev_grid_h = g_parser_state.grid_height;
                    g_parser_state.grid_height = strtol(c, &after_conv, 10);
                    if (errno != 0)
                    {
                        g_parser_state.grid_height = prev_grid_h;
                        printf("Error converting grid height on line %d. Skipping line.\n", line_num);
                    }
                    line_over = 1;
                }
                else if(*c == '\n' || *c == '\0')
                    line_over = 1;
                break;
                case LINE_STATE_ROW_OFFSET:
                if(isdigit(*c))
                {
                    errno = 0;
                    int32_t prev_row = g_parser_state.row_offset;
                    g_parser_state.row_offset = strtol(c, &after_conv, 10);
                    if (errno != 0)
                    {
                        g_parser_state.row_offset = prev_row;
                        printf("Error converting row offset on line %d. Skipping line.\n", line_num);
                    }
                    line_over = 1;
                }
                else if(*c == '\n' || *c == '\0')
                    line_over = 1;
                break;
                case LINE_STATE_COL_OFFSET:
                if(isdigit(*c))
                {
                    errno = 0;
                    int32_t prev_col = g_parser_state.column_offset;
                    g_parser_state.column_offset = strtol(c, &after_conv, 10);
                    if (errno != 0)
                    {
                        g_parser_state.column_offset = prev_col;
                        printf("Error converting column offset on line %d. Skipping line.\n", line_num);
                    }
                    line_over = 1;
                }
                else if(*c == '\n' || *c == '\0')
                    line_over = 1;
                break;
                case LINE_STATE_ROW:
                if(isdigit(*c))
                {
                    errno = 0;
                    int32_t prev_row = g_parser_state.row;
                    g_parser_state.row = strtol(c, &after_conv, 10);
                    if (errno != 0)
                    {
                        g_parser_state.row = prev_row;
                        printf("Error converting row on line %d. Skipping line.\n", line_num);
                    }
                    else
                        g_parser_state.column = -1;
                    line_over = 1;
                }
                else if(*c == '\n' || *c == '\0')
                    line_over = 1;
                break;
                case LINE_STATE_COL:
                if(isdigit(*c))
                {
                    errno = 0;
                    int32_t prev_col = g_parser_state.column;
                    g_parser_state.column = strtol(c, &after_conv, 10);
                    if (errno != 0)
                    {
                        g_parser_state.column = prev_col;
                        printf("Error converting column on line %d. Skipping line.\n", line_num);
                    }
                    else
                        g_parser_state.row = -1;
                    line_over = 1;
                }
                else if(*c == '\n' || *c == '\0')
                    line_over = 1;
                break;
                case LINE_STATE_SEQ:
                // If all these parser states are set, the frame sequence is specified by single values, for the
                // x or y position of the frame.
                if(g_parser_state.frame_width > 0 && g_parser_state.frame_height > 0 && 
                   (g_parser_state.column >= 0 || g_parser_state.row >= 0))
                {
                    if(isdigit(*c))
                    {
                        errno = 0;
                        uint32_t frame_val = strtol(c, &after_conv, 10);
                        if(errno != 0)
                        {
                            printf("Error converting single frame value on line %d. Skipping line.\n", line_num);
                            line_over = 1;
                        }
                        else
                        {
                            c = after_conv;
    
                            // If the current column (x pos) is set, the number is the y frame position.
                            if(g_parser_state.column >= 0)
                            {
                                current_frame.y = frame_val + g_parser_state.row_offset;
                                if(g_parser_state.grid_height > 0)
                                    current_frame.y *= g_parser_state.grid_height;
                            }
                            else // This must be the frame x position.
                            {
                                current_frame.x = frame_val + g_parser_state.column_offset;
                                if(g_parser_state.grid_width > 0)
                                    current_frame.x *= g_parser_state.grid_width;
                            }

                            osp_dynarray_add(frames_array, &current_frame);
                        }
                    }
                    else if(*c == '\n' || *c == '\0')
                    {
                        // Sequence is over, add to the array
                        current_sequence.num_frames = osp_dynarray_get_count(frames_array);
                        current_sequence.frames = calloc(current_sequence.num_frames, sizeof(frame_rect_t));
                        memcpy(current_sequence.frames, osp_dynarray_get_data(frames_array), current_sequence.num_frames * sizeof(frame_rect_t));
                        osp_dynarray_clear(frames_array);

                        osp_dynarray_add(sequences_array, &current_sequence);
                        line_over = 1;
                    }
                    else if(!isspace(*c))
                    {
                        printf("Invalid character %c on line %d. Skipping line.\n", *c, line_num);
                        line_over = 1;
                    }
                }
                else if(*c == '(')
                {
                    g_parser_state.state = LINE_STATE_SEQ_FRAME_1;
                }
                else if(*c == '\n' || *c == '\0')
                {
                    // Sequence is over, add to the array
                    current_sequence.num_frames = osp_dynarray_get_count(frames_array);
                    current_sequence.frames = calloc(current_sequence.num_frames, sizeof(frame_rect_t));
                    memcpy(current_sequence.frames, osp_dynarray_get_data(frames_array), current_sequence.num_frames);
                    osp_dynarray_clear(frames_array);

                    osp_dynarray_add(sequences_array, &current_sequence);
                }
                break;
                case LINE_STATE_SEQ_FRAME_1:
                if(isdigit(*c))
                {
                    errno = 0;
                    uint32_t frame_val = strtol(c, &after_conv, 10);
                    if(errno != 0)
                    {
                        printf("Error converting first frame value on line %d. Skipping line.\n", line_num);
                        line_over = 1;
                    }
                    else
                    {
                        c = after_conv;

                        // If the current column (x pos) is set, the first number is the y frame position.
                        if(g_parser_state.column >= 0)
                        {
                            current_frame.y = frame_val + g_parser_state.row_offset;
                            if(g_parser_state.grid_height > 0)
                                current_frame.y *= g_parser_state.grid_height;

                            // If both width and height are set, we're done here.
                            if(g_parser_state.frame_width > 0 && g_parser_state.frame_height > 0)
                                g_parser_state.state = LINE_STATE_SEQ_FRAME_C;
                            else
                                g_parser_state.state = LINE_STATE_SEQ_FRAME_2;
                        }
                        else // This must be the frame x position.
                        {
                            current_frame.x = frame_val + g_parser_state.column_offset;
                            if(g_parser_state.grid_width > 0)
                                current_frame.x *= g_parser_state.grid_width;

                            // If row and both width and height are set, we're done here.
                            if(g_parser_state.row >= 0 &&
                               g_parser_state.frame_width > 0 &&
                               g_parser_state.frame_height > 0)
                                g_parser_state.state = LINE_STATE_SEQ_FRAME_C;
                            else
                                g_parser_state.state = LINE_STATE_SEQ_FRAME_2;
                        }
                    }
                }
                else if(*c == ')')
                {
                    printf("Early closed parentheses on line %d. Skipping line.\n", line_num);
                    line_over = 1;
                }
                else if(*c == '\n' || *c == '\0')
                    line_over = 1;
                break;
                case LINE_STATE_SEQ_FRAME_2:
                if(isdigit(*c))
                {
                    errno = 0;
                    uint32_t frame_val = strtol(c, &after_conv, 10);
                    if(errno != 0)
                    {
                        printf("Error converting second frame value on line %d. Skipping line.\n", line_num);
                        line_over = 1;
                    }
                    else
                    {
                        c = after_conv;

                        // If the current column (x pos) or row (y pos) is set,
                        // the second number is the frame width or height.
                        if(g_parser_state.column >= 0 || g_parser_state.row >= 0)
                        {
                            // If the frame width is set, this must be the frame height.
                            if(g_parser_state.frame_width > 0)
                            {
                                current_frame.h = frame_val;
                                if(g_parser_state.grid_height > 0)
                                    current_frame.h *= g_parser_state.grid_height;

                                // After setting the height, we're done. Look for a closed parentheses.
                                g_parser_state.state = LINE_STATE_SEQ_FRAME_C;
                            }
                            else
                            {
                                current_frame.w = frame_val;
                                if(g_parser_state.grid_width > 0)
                                    current_frame.w *= g_parser_state.grid_width;

                                // If the frame height is already set, we're done here.
                                if(g_parser_state.frame_height > 0)
                                    g_parser_state.state = LINE_STATE_SEQ_FRAME_C;
                                else
                                    g_parser_state.state = LINE_STATE_SEQ_FRAME_3;
                            }
                        }
                        else // This must be the frame y position.
                        {
                            current_frame.y = frame_val + g_parser_state.row_offset;
                            if(g_parser_state.grid_height > 0)
                                current_frame.y *= g_parser_state.grid_height;

                            // If both frame width and height are set, we're done here.
                            if(g_parser_state.frame_width > 0 && g_parser_state.frame_height > 0)
                                g_parser_state.state = LINE_STATE_SEQ_FRAME_C;
                            else
                                g_parser_state.state = LINE_STATE_SEQ_FRAME_3;
                        }
                    }
                }
                else if(*c == ')')
                {
                    printf("Early closed parentheses on line %d. Skipping line.\n", line_num);
                    line_over = 1;
                }
                else if(*c == '\n' || *c == '\0')
                    line_over = 1;
                break;
                case LINE_STATE_SEQ_FRAME_3:
                if(isdigit(*c))
                {
                    errno = 0;
                    uint32_t frame_val = strtol(c, &after_conv, 10);
                    if(errno != 0)
                    {
                        printf("Error converting third frame value on line %d. Skipping line.\n", line_num);
                        line_over = 1;
                    }
                    else
                    {
                        c = after_conv;

                        // If the frame width is set, this must be the frame height.
                        if(g_parser_state.frame_width > 0)
                        {
                            current_frame.h = frame_val;
                            if(g_parser_state.grid_height > 0)
                                current_frame.h *= g_parser_state.grid_height;

                            // After setting the height, we're done. Look for a closed parentheses.
                            g_parser_state.state = LINE_STATE_SEQ_FRAME_C;
                        }
                        else
                        {
                            current_frame.w = frame_val;
                            if(g_parser_state.grid_width > 0)
                                current_frame.w *= g_parser_state.grid_width;

                            // If the frame heigth is set, we're done here.
                            if(g_parser_state.frame_height > 0)
                                g_parser_state.state = LINE_STATE_SEQ_FRAME_C;
                            else
                                g_parser_state.state = LINE_STATE_SEQ_FRAME_4;
                        }
                    }
                }
                else if(*c == ')')
                {
                    printf("Early closed parentheses on line %d. Skipping line.\n", line_num);
                    line_over = 1;
                }
                else if(*c == '\n' || *c == '\0')
                    line_over = 1;
                break;
                case LINE_STATE_SEQ_FRAME_4:
                if(isdigit(*c))
                {
                    errno = 0;
                    uint32_t frame_val = strtol(c, &after_conv, 10);
                    if(errno != 0)
                    {
                        printf("Error converting fourth frame value on line %d. Skipping line.\n", line_num);
                        line_over = 1;
                    }
                    else
                    {
                        c = after_conv;

                        current_frame.h = frame_val;
                        if(g_parser_state.grid_height > 0)
                            current_frame.h *= g_parser_state.grid_height;

                        // After setting the height, we're done. Look for a closed parentheses.
                        g_parser_state.state = LINE_STATE_SEQ_FRAME_C;
                    }
                }
                else if(*c == ')')
                {
                    printf("Early closed parentheses on line %d. Skipping line.\n", line_num);
                    line_over = 1;
                }
                else if(*c == '\n' || *c == '\0')
                    line_over = 1;
                break;
                case LINE_STATE_SEQ_FRAME_C:
                if(!isspace(*c) && *c != ')')
                {
                    printf(
                        "Invalid character %c while looking for closed parentheses on line %d. Skipping line.\n",
                        *c,
                        line_num
                        );
                    line_over = 1;
                }
                else if(*c == ')')
                {
                    osp_dynarray_add(frames_array, &current_frame);
                    g_parser_state.state = LINE_STATE_SEQ;
                }                
                break;
            }
        }

        ++line_num;
    }

    size_t num_elements = osp_dynarray_get_count(sequences_array);
    fwrite(&num_elements, sizeof(num_elements), 1, write_file);
    for(
        frame_sequence_t *sequence = (frame_sequence_t *)osp_dynarray_fwd_iter_start(sequences_array);
        osp_dynarray_iter_check(sequences_array);
        osp_dynarray_fwd_iter_next(sequences_array, (void **)&sequence)
    )
    {
        fwrite(&(sequence->duration), sizeof(sequence->duration), 1, write_file);
        fwrite(&(sequence->num_frames), sizeof(sequence->num_frames), 1, write_file);
        for(int i_frame = 0; i_frame < sequence->num_frames; ++i_frame)
        {
            fwrite(&(sequence->frames[i_frame].x), sizeof(sequence->frames[i_frame].x), 1, write_file);
            fwrite(&(sequence->frames[i_frame].y), sizeof(sequence->frames[i_frame].y), 1, write_file);
            fwrite(&(sequence->frames[i_frame].w), sizeof(sequence->frames[i_frame].w), 1, write_file);
            fwrite(&(sequence->frames[i_frame].h), sizeof(sequence->frames[i_frame].h), 1, write_file);
        }
        free(sequence->frames);
    }

    osp_dynarray_delete(sequences_array);
    osp_dynarray_delete(frames_array);

    return 0;
}