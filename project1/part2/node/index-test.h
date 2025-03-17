#ifndef INDEX_TEST_H
#define INDEX_TEST_H

#include <stdio.h>

typedef struct
{
    char **file_paths; // Array of file paths (stored in strings)
    int file_count;    // Number of files found
} DirectoryResult;

DirectoryResult open_dir(const char *path_to_dir, const char *next_dir);

void free_directory_result(DirectoryResult result);

#endif