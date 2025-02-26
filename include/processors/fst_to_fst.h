#ifndef FST_TO_FST_H
#define FST_TO_FST_H

#include <stdio.h>
#include <stdint.h>

/// FST frame sequence text file format.
/// The parser will work with states and will parse the text file line by line trimming every white leading or trailing
/// white space.
/// The parser state will hold:
/// - Current frame width and height, they can both be -1. If width and/or height are specified (> 0), in the next
///   frame sequences the third and/or fourth elements per tuple will be optional but you must specify both to
///   override. For example, if current width is set to 16, a tuple like (32, 32, 18) will mean a rectangle starting
///   at (32, 32) and 16x18 pixels wide. If you want just one rectangle of 18x18 pixels, you will need to write
///   (32, 32, 18, 18). If both width and height are set to 16, to achieve the same result as the first example, you
///   will need to specify all the elements of the tuple: (32, 32, 16, 18). This doesn't need to be the same as the
///   grid size.
/// - Current grid size or no grid (-1, -1). If a grid size is set, every subsequent frame sequence definition will
///   be specified in grid positions. If no grid size is set, the frame sequences are expected to be expressed in
///   pixels. This doesn't need to be the same as the frame width and height.
/// - Current row or current column on none (both are -1). If no row or column is set, every subsequent frame sequence
///   will expect two leading elements for the tuple, specifying the x and y origin position of the rectangle. If a
///   column or row is specified, then the elements of a frame sequence will expect only one leading parameter,
///   specifying, respectively, the missing row or column. You cannot set both the current row and column, setting one
///   will unset the other. If a grid is set, these values are expected in grid positions, if not, these are pixel
///   values.
/// - Current row and column offsets, which both default to 0. If a grid is set, these values are expected in grid
///   positions, if not, these are pixel values. These offsets apply only to frame definition, not to the globally
///   set row or column (see previous paragraph).
///
/// Every line in the text file can be one of the following:
/// - White space. Will be discarded.
/// - width w: set the current frame width. Everithing <= 0 will unset the current width.
/// - height h: set the current frame height. Everithing <= 0 will unset the current height.
/// - row r: set the current row. Everything < 0 will unset the current row. Setting the current row will unset the
///   current column.
/// - col c: set the current column. Everything < 0 will unset the current column. Setting the current column will
///   unset the current row.
/// - grid x y: set the current grid size. Everything <= 0 will unset the current grid.
/// - row_off r: set the current row offset.
/// - col_off c: set the current column offset.
/// - A frame sequence definition: t (x y w h)(x y w h)...(x y w h)
///   Where t is the frame sequence duration in seconds and every tuple between the round parentheses specifies a
///   single frame rectangle. The x, y, w and h parameters for every frame will be optional and expressed in pixels or 
///   grid positions depending on the parser state defined above and set with the previous commands.

#define WIDTH_CMD           "width"
#define HEIGHT_CMD          "height"
#define ROW_CMD             "row"
#define COLUMN_CMD          "col"
#define GRID_CMD            "grid"
#define ROW_OFFSET_CMD      "row_off"
#define COLUMN_OFFSET_CMD   "col_off"

/// @brief FST text definition file to frame sequence data asset converter.
/// @param readFile Input FILE containing the FST text
/// @param writeFIle Output bundle FILE to write data to
/// @param params Optional converter parameters
/// @return 0 on successful conversion, error value otherwise
int fst_to_fst(FILE* readFile, FILE* writeFIle, void* params);

#endif