#ifndef PNG_TO_PNG_H
#define PNG_TO_PNG_H

#include <stdio.h>

/// @brief Png image file to png asset converter (just copies the png data)
/// @param readFile Input FILE containing the png image
/// @param writeFile Output bundle FILE to write data to
/// @param params Optional converter parameters
/// @return 0 on successful conversion, error value otherwise
int png_to_png(FILE* readFile, FILE* writeFile, void* params);

#endif