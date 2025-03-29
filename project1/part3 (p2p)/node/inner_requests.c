#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include "helper.h"
int counter = 1;

DirectoryResult open_dir(const char *path_to_dir, const char *next_dir)
{
    DIR *dir = opendir(path_to_dir);
    if (!dir)
    {
        printf("Failed to open directory: %s\n", path_to_dir);
        return (DirectoryResult){NULL, 0};
    }

    char **file_paths = malloc(1024 * sizeof(char *)); // Allocate memory for file paths
    if (!file_paths)
    {
        printf("Failed to allocate memory for file paths\n");
        closedir(dir);
        return (DirectoryResult){NULL, 0};
    }

    int file_count = 0;
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL)
    {
        if ((strcmp(entry->d_name, ".") == 0) || (strcmp(entry->d_name, "..") == 0))
        {
            continue;
        }

        char correct_path[1024];
        snprintf(correct_path, sizeof(correct_path), "%s/%s", path_to_dir, entry->d_name);

        struct stat fileStat;
        if (lstat(correct_path, &fileStat) == 0)
        {
            if (S_ISREG(fileStat.st_mode)) // If it's a regular file
            {
                char *file_info = malloc(1024 * sizeof(char)); // Allocate space for the file info
                if (file_info)
                {
                    // get date
                    char date_str[11]; // "YYYY-MM-DD" + null terminator
                    time_t t = time(NULL);
                    struct tm tm = *localtime(&t);
                    strftime(date_str, sizeof(date_str), "%Y-%m-%d", &tm);
                    if (strlen(next_dir) > 1)
                    {
                        snprintf(file_info, 1024, "%d;%s/%s;%lld;%s\n", counter, next_dir, entry->d_name, (long long)fileStat.st_size, date_str);
                    }
                    else
                    {
                        snprintf(file_info, 1024, "%d;%s%s;%lld;%s\n", counter, next_dir, entry->d_name, (long long)fileStat.st_size, date_str);
                    }

                    file_paths[file_count] = file_info; // Store the file info in the buffer
                    file_count++;
                    counter++;
                }
            }
            else if (S_ISDIR(fileStat.st_mode)) // If it's a directory
            {
                char sub_dir[BUFSIZ];
                if (strlen(next_dir) > 1)
                {
                    snprintf(sub_dir, sizeof(sub_dir), "%s/%s", next_dir, entry->d_name);
                }
                else
                {
                    snprintf(sub_dir, sizeof(sub_dir), "%s%s", next_dir, entry->d_name);
                }

                DirectoryResult sub_result = open_dir(correct_path, sub_dir); // Recursively open subdirectory

                // If subdirectory has files, merge them into the main buffer
                for (int i = 0; i < sub_result.file_count; i++)
                {
                    file_paths[file_count] = sub_result.file_paths[i];
                    file_count++;
                }
                free_directory_result(sub_result); // Free subdirectory result
            }
        }
    }

    closedir(dir);

    // Reallocate the array to shrink it to the actual size
    file_paths = realloc(file_paths, file_count * sizeof(char *));

    return (DirectoryResult){file_paths, file_count}; // Return the buffer and file count
}

void free_directory_result(DirectoryResult result)
{
    for (int i = 0; i < result.file_count; i++)
    {
        free(result.file_paths[i]); // Free each file info string
    }

    free(result.file_paths); // Free the array of file paths
}

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
    size_t buffer_size = BUFSIZ * BUFSIZ;
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