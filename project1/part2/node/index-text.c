#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <time.h>
#include "index-test.h"

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
