#ifndef FILE_OPERATIONS_H
#define FILE_OPERATIONS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Function to fix the file name and path
char **fix_name(char *curr_file_name);

// Function to encode a file and return the encoded string and its size
char *encode_file(char *path_to_file, size_t *encoded_size);

#endif // FILE_OPERATIONS_H