#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>

int counter = 1;

void open_dir(char *path_to_dir, char next_dir[], int amount)
{
    // open the directory
    DIR *dir = opendir(path_to_dir);
    if (dir == NULL)
    {
        printf("Failed to open directory: %s\n", path_to_dir);
        return;
    }

    // go through the directory items
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if ((strcmp(entry->d_name, ".") == 0) || (strcmp(entry->d_name, "..") == 0))
        {
            continue;
        }

        // correct the path with new entry
        char correct_path[1024];
        snprintf(correct_path, sizeof(correct_path), "%s/%s", path_to_dir, entry->d_name);

        // check if new path is a directory or file
        struct stat fileStat;
        if (lstat(correct_path, &fileStat) == 0)
        {
            if (S_ISREG(fileStat.st_mode)) // Regular file
            {
                char file_path[BUFSIZ];

                // Avoid unnecessary leading slash
                if (strlen(next_dir) > 1)
                {
                    snprintf(file_path, sizeof(file_path), "%s/%s", next_dir, entry->d_name);
                }
                else
                {
                    snprintf(file_path, sizeof(file_path), "%s%s", next_dir, entry->d_name);
                }

                printf("%d;%s;%lld\n", counter, file_path, (long long)fileStat.st_size);
                counter++;
            }
            else if (S_ISDIR(fileStat.st_mode)) // Directory
            {
                char sub_dir[BUFSIZ];

                // Avoid unnecessary leading slash
                if (strlen(next_dir) > 1)
                {
                    snprintf(sub_dir, sizeof(sub_dir), "%s/%s", next_dir, entry->d_name);
                }
                else
                {
                    snprintf(sub_dir, sizeof(sub_dir), "%s%s", next_dir, entry->d_name);
                }

                open_dir(correct_path, sub_dir, amount);
            }
        }
        amount = 1;
    }

    closedir(dir);
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf("Usage: %s <directory>\n", argv[0]);
        return 1;
    }

    char *dir_path = argv[1];
    char next_dir[BUFSIZ] = "";
    int amount = 1;

    open_dir(dir_path, next_dir, amount);
    return 0;
}