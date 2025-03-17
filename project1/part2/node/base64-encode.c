#include "base64-encode.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char **fix_name(char *curr_file_name)
{
    // Allocate space for the result (an array of two string pointers)
    char **buffer = malloc(2 * sizeof(char *));
    if (!buffer)
    {
        perror("Failed to allocate memory");
        exit(EXIT_FAILURE);
    }

    // Allocate enough space for clean_path to hold the path string
    char *clean_path = malloc(BUFSIZ);
    if (!clean_path)
    {
        perror("Failed to allocate memory for clean_path");
        exit(EXIT_FAILURE);
    }

    // Check if we start with ~
    if (curr_file_name[0] == '~')
    {
        curr_file_name++; // Move past the '~'
    }

    // Copy the cleaned path
    strcpy(clean_path, curr_file_name);
    buffer[0] = clean_path;

    // Find the last occurrence of '/'
    const char *last_time = strrchr(clean_path, '/');
    if (last_time && last_time[0] == '/')
    {
        last_time++; // Move past the '/'
    }

    // Allocate memory for curr_name (the name of the file with .encoded suffix)
    char *curr_name = malloc(BUFSIZ);
    if (!curr_name)
    {
        perror("Failed to allocate memory for curr_name");
        exit(EXIT_FAILURE);
    }

    snprintf(curr_name, BUFSIZ, "%s.encoded", last_time);
    buffer[1] = curr_name;

    return buffer;
}

char *encode_file(char *path_to_file, size_t *encoded_size)
{
    char command[BUFSIZ];
    snprintf(command, sizeof(command), "base64 %s", path_to_file);

    // Run the base64 command to encode the file and capture the output
    FILE *answer = popen(command, "r");
    if (!answer)
    {
        perror("popen failed");
        exit(EXIT_FAILURE);
    }

    // Create a buffer to store the encoded content
    size_t buffer_size = BUFSIZ;
    char *encoded_data = malloc(buffer_size);
    if (!encoded_data)
    {
        perror("Failed to allocate memory for encoded data");
        exit(EXIT_FAILURE);
    }

    // Read the encoded data into the buffer
    size_t total_len = 0;
    char buffer[BUFSIZ];
    while (fgets(buffer, sizeof(buffer), answer))
    {
        size_t buffer_len = strlen(buffer);
        if (total_len + buffer_len >= buffer_size)
        {
            // Reallocate memory if necessary
            buffer_size = total_len + buffer_len + 1;
            char *temp = realloc(encoded_data, buffer_size);
            if (!temp)
            {
                perror("Failed to reallocate memory for encoded data");
                free(encoded_data);
                exit(EXIT_FAILURE);
            }
            encoded_data = temp;
        }
        strcpy(encoded_data + total_len, buffer);
        total_len += buffer_len;
    }

    *encoded_size = total_len;

    // Close the pipe
    pclose(answer);

    return encoded_data;
}
