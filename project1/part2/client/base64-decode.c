#include "base64-decode.h"
#include <fcntl.h> // For F_OK
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *decode_string(const char *encoded_data, size_t *decoded_size)
{
    // Create a temporary file to store the encoded data for decoding
    char temp_file_name[] = "/tmp/encoded_data_XXXXXX";
    int temp_fd = mkstemp(temp_file_name);
    if (temp_fd == -1)
    {
        perror("Failed to create temporary file");
        exit(EXIT_FAILURE);
    }

    // Write the encoded data to the temporary file
    FILE *temp_file = fdopen(temp_fd, "w");
    if (!temp_file)
    {
        perror("Failed to open temporary file");
        exit(EXIT_FAILURE);
    }

    fprintf(temp_file, "%s", encoded_data);
    fclose(temp_file);

    // Ensure the file name is valid
    if (temp_file_name == NULL || strlen(temp_file_name) == 0)
    {
        perror("Invalid file name");
        exit(EXIT_FAILURE);
    }

    // Create a command to decode the file using base64 -d
    char command[BUFSIZ];
    snprintf(command, sizeof(command), "base64 -d \"%s\"", temp_file_name);

    // Run the command to decode and capture the output
    FILE *answer = popen(command, "r");
    if (!answer)
    {
        perror("popen failed");
        exit(EXIT_FAILURE);
    }

    // Create a buffer to store the decoded content
    char *decoded_data = malloc(BUFSIZ);
    if (!decoded_data)
    {
        perror("Failed to allocate memory for decoded data");
        exit(EXIT_FAILURE);
    }

    // Read the decoded data into the buffer
    size_t total_len = 0;
    char buffer[BUFSIZ];
    while (fgets(buffer, sizeof(buffer), answer))
    {
        size_t buffer_len = strlen(buffer);
        if (total_len + buffer_len >= BUFSIZ)
        {
            // Reallocate memory if necessary
            decoded_data = realloc(decoded_data, total_len + buffer_len + 1);
            if (!decoded_data)
            {
                perror("Failed to reallocate memory for decoded data");
                exit(EXIT_FAILURE);
            }
        }
        strcpy(decoded_data + total_len, buffer);
        total_len += buffer_len;
    }

    *decoded_size = total_len;

    // Close the pipe and remove the temporary file
    pclose(answer);
    remove(temp_file_name);

    return decoded_data;
}