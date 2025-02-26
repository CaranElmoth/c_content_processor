#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/resource.h>

#include "OSP_content.h"
#include "processors/png_to_png.h"
#include "processors/ldtk_to_map.h"
#include "processors/fst_to_fst.h"

// Some constants for path management
const uint32_t MAX_PATH = 4096;
const uint32_t MAX_FILENAME = 128;
const uint32_t MAX_EXTENSION = 8;

// Dynamic sized content table data, starting with four elements and doubling
// its size on every realloc.
const uint32_t INITIAL_CONTENT_TABLE_CAPACITY = 4;
uint32_t content_table_capacity = INITIAL_CONTENT_TABLE_CAPACITY;
uint32_t content_table_count = 0;
osp_cnt_table_entry_t *content_table = NULL;

// Content processor function type definition
typedef int (*processor_t)(FILE* readFile, FILE* writeFile, void* params);

// Structure defining each supported content type with its processor
typedef struct _supported_processor
{
    // Asset file extension
    const char *extension;
    // Associated processor function
    processor_t processor;
    // Byte output asset type ID
    uint8_t outputType;
} supported_processor_t;

// Currently supported processors table
const int NUM_PROCESSORS = 3;
supported_processor_t supported_processors[] =
{
    {
        .extension = "png",
        .processor = &png_to_png,
        .outputType = OSP_CNT_TYPE_PNG
    },
    {
        .extension = "ldtk",
        .processor = &ldtk_to_map,
        .outputType = OSP_CNT_TYPE_MAP
    },
    {
        .extension = "fst",
        .processor = &fst_to_fst,
        .outputType = OSP_CNT_TYPE_FST
    }
};

/// @brief Find supported type table entry index by extension
/// @param extension File extension string to check
/// @return Processors table index if successful, -1 otherwise
int32_t find_supported_type(const char* extension);
/// @brief Parse directory and write bundle data to FILE
/// @param path Path of the bundle root directory
/// @param prefix Currently calculated asset prefix, relative to root
/// @param writeFile FILE pointer to write bundle data to
void parse_directory(const char* path, char* prefix, FILE* writeFile);
/// @brief Add new content table entry
/// @param name Asset name
/// @param type Byte asset type ID
/// @param start Asset data start position in bundle file
/// @param size Asset data size in bundle file
void add_content_table_entry(const char* name,
                             uint8_t type,
                             uint64_t start,
                             uint64_t size);
/// @brief Realloc content table, doubling its size
void realloc_content_table();
/// @brief Free previously allocated content table
void free_content_table();
/// @brief Write content table to bundle file
/// @param writeFile FILE pointer to write bundle data to
void write_content_table(FILE* writeFile);

int main(int argc, char **argv)
{
    // Initial content table allocation
    realloc_content_table();

    // Some path working strings
    char startingPath[MAX_PATH + 1];
    char workingPath[MAX_PATH + 1];
    char outputPath[MAX_PATH + 1];

    // Cache the initial path to go back to upon exiting
    getcwd(startingPath, MAX_PATH);

    // Default output filename
    strncpy(outputPath, "./bundle.cnt", MAX_PATH);

    if(argc > 1)
    {
        // If present, firs argument is the directory to parse for assets
        strncpy(workingPath, argv[1], MAX_PATH);
        if(argc > 3)
        {
            // Second argument is the optional "-o" option
            if(strncmp(argv[2], "-o", 4) == 0)
            {
                // In this case, the third argument is the output
                // bundle file name
                strncpy(outputPath, workingPath, MAX_PATH);
                strncat(outputPath, "/", MAX_PATH);
                strncat(outputPath, argv[3], MAX_PATH);
            }
        }
    }
    else // Parse the current directory by default
        strncpy(workingPath, ".", MAX_PATH);

    // Open output file for writing and writing
    FILE* writeFile = fopen(outputPath, "wb+");
    // Write placeholder content table position
    uint64_t tablePos = 0;
    fwrite(&tablePos, sizeof(tablePos), 1, writeFile);

    // Parse the working path directory for supported files
    // with a starting empty asset name prefix
    // and write processed content to writeFile
    parse_directory(workingPath, "", writeFile);

    // Cache current position as real content table position
    tablePos = ftell(writeFile);
    // Write the content table
    write_content_table(writeFile);
    // Write the real content table position at the start of
    // the file and close it.
    rewind(writeFile);
    fwrite(&tablePos, sizeof(tablePos), 1, writeFile);
    fclose(writeFile);
    // Free the content table memory
    free_content_table();

    // Go back to the initial directory
    chdir(startingPath);

    return 0;
}

void parse_directory(const char* path, char *prefix, FILE* writeFile)
{
    printf("Opening dir %s for parsing\n", path);
    // Move to the directory to parse
    if(chdir(path) != 0)
    {
        printf("\tUnable to move to directory %s\n", path);
        return;
    }

    // Open directory to parse
    DIR* directory = opendir(".");
    if(directory == NULL)
    {
        printf("\tUnable to open %s\n", path);
        return;
    }

    printf("\tParsing directory with prefix %s\n", prefix);

    struct dirent *entry;
    struct stat entry_stat;
    // Cycle all current directory entries
    while((entry = readdir(directory)))
    {
        // Find info about the current directory
        stat(entry->d_name, &entry_stat);
        // If the current entry is a subdirectory
        if(S_ISDIR(entry_stat.st_mode))
        {
            // Exclude the special "." and ".." directories
            if(strncmp(entry->d_name, ".", 8) != 0 &&
               strncmp(entry->d_name, "..", 8) != 0)
            {
                // Add the current subdirectory name to the asset prefix
                char prefixBuffer[MAX_PATH];
                strcpy(prefixBuffer, prefix);
                strncat(prefixBuffer, entry->d_name, 128);
                // Don't forget the trailing slash
                strncat(prefixBuffer, "/", 4);
                // Recursively parse the subdirectory
                parse_directory(entry->d_name, prefixBuffer, writeFile);
            }
        }
        else if(S_ISREG(entry_stat.st_mode))
        {
            // This is a file, let's see if its something we can process
            printf("Trying to process file %s\n", entry->d_name);
            
            char fileName[MAX_FILENAME];
            char extension[MAX_EXTENSION];
            char assetName[MAX_PATH];

            // Find the postition of last dot to the right in the filename
            char *lastDot = rindex(entry->d_name, '.');
            int nameLength = 0;
            if(lastDot == NULL) // This file has no extension
            {
                nameLength = strlen(entry->d_name);
                extension[0] = '\0';
            }
            else
            {
                // Find the file name length without extension...
                nameLength = lastDot - entry->d_name;
                // ...and the extension length
                int extensionLength = strlen(entry->d_name) - nameLength - 1;
                // Save the extension separately
                strncpy(extension, lastDot + 1, extensionLength);
                extension[extensionLength] = '\0';
            }
            // Save the file name without extension
            strncpy(fileName, entry->d_name, nameLength);
            fileName[nameLength] = '\0';
            // Save the asset name prefixing the file name with the path
            // relative to the root.
            strncpy(assetName, prefix, MAX_PATH - 1);
            strncat(assetName, fileName, MAX_PATH - 1);

            printf("\tFile name: %s\n", fileName);
            printf("\tExtension: %s\n", extension);
            printf("\tAsset name: %s\n", assetName);

            // Check if this extension is in the supported types array
            int32_t supported_type_idx = find_supported_type(extension);
            if(supported_type_idx < 0)
            {
                printf("\tUnsupported file type %s, skipping\n", extension);
            }
            else // Supported asset type, let's process it
            {
                // Open the input asset file
                FILE* readFile = fopen(entry->d_name, "rb");
                // Save the current write file position for the content table
                uint64_t start = ftell(writeFile);
                // Call the supported processor
                if(supported_processors[supported_type_idx].processor(readFile,
                   writeFile, NULL) == 0)
                {
                    // Processing went fine, check how much data was written
                    uint64_t size = ftell(writeFile) - start;
                    // Save all the data into the content table
                    add_content_table_entry(assetName,
                        supported_processors[supported_type_idx].outputType,
                        start, size);
                }

                // We can close the input asset file, now
                fclose(readFile);
            }
        }
    }

    // Done with the current subdirectory, let's go back up one level
    chdir("..");
    // Close the open subdirectory
    closedir(directory);
}

void add_content_table_entry(const char *name,
                             uint8_t type,
                             uint64_t start,
                             uint64_t size)
{
    if(name == NULL)
        return;

    int nameLength = strlen(name);
    if(nameLength <= 0)
        return;

    // Check if we need to alloc more space for the content table array
    if(content_table_count >= content_table_capacity)
        realloc_content_table();

    // Copy the asset name and data to the new content table entry
    content_table[content_table_count].name = malloc(nameLength + 1);
    strncpy(content_table[content_table_count].name, name, nameLength);
    content_table[content_table_count].name[nameLength] = '\0';
    content_table[content_table_count].type = type;
    content_table[content_table_count].start = start;
    content_table[content_table_count].size = size;

    // Increase the entries count
    ++content_table_count;
}

void realloc_content_table()
{
    // Let's double the capacity
    content_table_capacity *= 2;

    // Realloc the table memory
    // (If this is the first table allocation, content_table is null and realloc
    // will do a normal allocation)
    content_table = realloc(content_table,
        content_table_capacity*sizeof(osp_cnt_table_entry_t));
}

void free_content_table()
{
    // Free the memory allocated for every asset name string
    for(int iEntry = 0; iEntry < content_table_count; ++iEntry)
        free(content_table[iEntry].name);

    // Free the memory allocated for the entries array
    free(content_table);
}

void write_content_table(FILE* writeFile)
{
    // Write the content table size, only the actually used entries,
    // not the table capacity
    fwrite(&content_table_count, sizeof(content_table_count), 1, writeFile);
    // Write each entry sequentially
    for(int iEntry = 0; iEntry < content_table_count; ++iEntry)
    {
        size_t nameLength = strlen(content_table[iEntry].name);
        fwrite(&nameLength, sizeof(nameLength), 1, writeFile);
        fwrite(content_table[iEntry].name,
            sizeof(unsigned char), nameLength, writeFile);
        fwrite(&(content_table[iEntry].type),
            sizeof(content_table[iEntry].type), 1, writeFile);
        fwrite(&(content_table[iEntry].start),
            sizeof(content_table[iEntry].start), 1, writeFile);
        fwrite(&(content_table[iEntry].size),
            sizeof(content_table[iEntry].size), 1, writeFile);
    }
}

int32_t find_supported_type(const char* extension)
{
    // Cycle the supported processors array to checking the passed extension
    for(int32_t iProcessor = 0; iProcessor < NUM_PROCESSORS; ++iProcessor)
        if(strncmp(supported_processors[iProcessor].extension,
            extension, MAX_EXTENSION - 1) == 0)
            return iProcessor;

    return -1;
}