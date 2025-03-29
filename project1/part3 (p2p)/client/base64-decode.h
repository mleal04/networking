#ifndef BASE64_DECODE_H
#define BASE64_DECODE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h> // For F_OK

// Function to encode the file to base64
char *encode_file(char *path_to_file, size_t *encoded_size);

// Function to decode the base64 string
char *decode_string(const char *encoded_data, size_t *decoded_size);

#endif // BASE64_DECODE_H