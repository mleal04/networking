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

void encode_file(char *path_to_file, char *output_file)
{
    char command[BUFSIZ];
    snprintf(command, sizeof(command), "base64 %s\n", path_to_file);

    // open the output file --> to store the info
    FILE *fd = fopen(output_file, "w");
    if (!fd)
    {
        perror("Failed to open output file");
        exit(EXIT_FAILURE);
    }

    // activate the command --> get a file pointer with answer
    FILE *answer = popen(command, "r");
    if (!answer)
    {
        perror("popen failed");
        exit(EXIT_FAILURE);
    }

    // go through answer (in answer) --> store in buffer --> put in file
    char buffer[BUFSIZ]; // for fgets buffer
    size_t total_len = 0;
    while (fgets(buffer, sizeof(buffer), answer))
    {
        total_len += strlen(buffer); // update len
        fprintf(fd, "%s", buffer);   // update file
    }
    fclose(fd);
    // all output should be in (.encode) file  + print out to terminal
    printf("Content-Size: %zu\n", total_len);
    char final_string[BUFSIZ];
    FILE *fd2 = fopen(output_file, "r");
    while (fgets(final_string, sizeof(final_string), fd2))
    {
        printf("%s", final_string);
    }

    // Close file pointers
    fclose(fd2);
    pclose(answer);
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        printf("Did not enter correct amount of inputs\n");
        return 0;
    }

    if (strcmp(argv[1], "encode") == 0)
    {
        // Call fix_name and receive the two parts of the path
        char **buffer = fix_name(argv[2]);
        encode_file(buffer[0], buffer[1]);
    }

    return 0;
}