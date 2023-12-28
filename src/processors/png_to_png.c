#include <stdlib.h>

#include "processors/png_to_png.h"

// Simply copy the png block to the content bundle,
// we don't need any processing here
int png_to_png(FILE* readFile, FILE* writeFile, void* params)
{
    fseek(readFile, 0, SEEK_END);
    long size = ftell(readFile);
    rewind(readFile);

    unsigned char *buffer = malloc(size);
    fread(buffer, 1, size, readFile);
    fwrite(buffer, 1, size, writeFile);
    free(buffer);

    return 0;
}