#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <dirent.h>
#include "base64-decode.h"
#include <sys/stat.h>  // For mkdir
#include <sys/types.h> // For mkdir (depending on system)

char download_dir[BUFSIZ];
int quiet_flag = 0; // not activated

// file descriptor for client socket
int client_socket;

void write_to_file(char *decoded_data, char *filename)
{
    // Specify the directory where you want to create the file
    const char *directory_path = download_dir;

    // Open the directory
    DIR *dir = opendir(directory_path);
    if (dir == NULL)
    {
        // If the directory does not exist, create it
        if (mkdir(directory_path, 0755) == -1)
        {
            perror("Failed to create directory");
            exit(EXIT_FAILURE);
        }
    }
    else
    {
        closedir(dir);
    }

    // Construct the path for the new file
    char file_path[BUFSIZ];
    snprintf(file_path, sizeof(file_path), "%s/%s", directory_path, filename);

    // Open the file for writing
    FILE *file = fopen(file_path, "w");
    if (file == NULL)
    {
        perror("Failed to open file for writing");
        exit(EXIT_FAILURE);
    }

    // Write the decoded data to the file
    fprintf(file, "%s", decoded_data);

    // Close the file
    fclose(file);

    // printf("Decoded data has been written to: %s\n", file_path);
}

int main(int argc, char *argv[])
{
    // must take in: SERVER IP || SERVER PORT || DOWNLOAD_PATH || quiet? input_file || input_file
    if (argc < 4)
    {
        printf("not enough arguments\n");
        return 1;
    }

    // store the download adress
    strcpy(download_dir, argv[3]);

    // update quiet flag (when argc is 5)
    if (argc == 5 && strcmp(argv[4], "quiet") == 0)
    {
        quiet_flag = 1; // activate flag
    }
    else if (argc == 6 && strcmp(argv[4], "quiet") == 0)
    {
        quiet_flag = 1; // activate flag
    }

    // Storage items
    struct addrinfo hints, *res; // client preferences and storage unit

    // create client AND fill up its preferences
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;     // type of IPv4
    hints.ai_socktype = SOCK_STREAM; // tcp connection

    // now resolve the servers information
    if (getaddrinfo(argv[1], argv[2], &hints, &res) != 0)
    {
        perror("getaddrinfo");
        return 1;
    }

    // create socket for client
    if ((client_socket = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1)
    {
        perror("socket");
        freeaddrinfo(res);
        return 1;
    }

    // now connect server and client
    if ((connect(client_socket, res->ai_addr, res->ai_addrlen)) == -1)
    {
        perror("connect");
        close(client_socket);
        freeaddrinfo(res);
        return 1;
    }

    // here we have connected successfully
    printf("Connected successfully to server at %s:%s\n", argv[1], argv[2]);
    freeaddrinfo(res);

    // grab the client request --> file or input
    // if argc == 4 then we just leave at stdins
    FILE *user_input = stdin;
    // if 5 input and last one is not quiet --> file is there
    if (argc == 5 && strcmp(argv[4], "quiet") != 0)
    {
        user_input = fopen(argv[4], "r");
        if (user_input == NULL)
        {
            printf("myshell: file cannot be open\n");
            return 1;
        }
    } // if 6 input, then argv[5] == quiet and argv[6] = file_input
    else if (argc == 6)
    {
        user_input = fopen(argv[5], "r");
        if (user_input == NULL)
        {
            printf("myshell: file cannot be open\n");
            return 1;
        }
    }
    // now send multiple requests (from client)
    while (1)
    {

        char request[BUFSIZ];
        if (fgets(request, sizeof(request), user_input) == NULL)
        {
            if (user_input != stdin)
            { // we read a file
                fclose(user_input);
            }
            break; // End the loop if no more input
        }

        // Send the request to the client
        if (send(client_socket, request, strlen(request), 0) == -1)
        {
            perror("send");
            close(client_socket);
            return 1;
        }

        // now wait for server response
        char buffer[BUFSIZ];
        int bytes_of_mess = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        if (bytes_of_mess == -1)
        {
            perror("recv");
        }
        else if (bytes_of_mess == 0)
        {
            printf("Server disconnected.\n");
        }
        else
        {
            // RECEIVE RESPONSE AND BEGIN TO PRINT
            buffer[bytes_of_mess] = '\0';
            char copy_of_buffer[BUFSIZ];
            strcpy(copy_of_buffer, buffer);
            char *status_code = strtok(copy_of_buffer, " ");

            // check request for GET or not
            if (strncmp(request, "HELO", 3) == 0)
            {
                // quiet or not
                if (quiet_flag == 0 || strcmp(status_code, "400") == 0)
                {
                    printf("%s\n", buffer);
                }
                else
                {
                    continue;
                }
            }
            else if (strncmp(request, "GET", 3) == 0)
            {
                // Find the name of the file without modifying `buffer`
                char file_name[256]; // Assuming max filename length
                char *file_name_start = strstr(buffer, "File-Name: ");
                if (file_name_start)
                {
                    file_name_start += 10; // Move past "File-Name: "

                    // Copy file name into a separate buffer
                    char *file_name_end = strchr(file_name_start, '\n');
                    if (file_name_end)
                    {
                        size_t length = file_name_end - file_name_start;
                        strncpy(file_name, file_name_start, length);
                        file_name[length] = '\0'; // Null-terminate
                        // printf("Extracted File Name: %s\n", file_name);
                    }
                }
                // Decode the string --> find the blank line
                char *blank_line_pos = strstr(buffer, "\n\n");
                if (blank_line_pos != NULL)
                {
                    // Get the actual encoded string
                    char *encoded_string = blank_line_pos + 2;
                    size_t decoded_size = 0;
                    char *decoded_data = decode_string(encoded_string, &decoded_size);

                    // Now place the decoded data into a file
                    write_to_file(decoded_data, file_name);
                }
                // quiet or not
                if (quiet_flag == 0 || strcmp(status_code, "400") == 0)
                {
                    printf("%s\n", buffer);
                }
                else
                {
                    continue;
                }
            }
            else if (strncmp(request, "FIND", 3) == 0)
            {
                // quiet or not --> still show
                printf("%s\n", buffer);
            }
            else if (strncmp(request, "END", 3) == 0)
            {
                // quiet or not
                if (quiet_flag == 0 || strcmp(status_code, "400") == 0)
                {
                    printf("%s\n", buffer);
                    break;
                }
                else
                {
                    break;
                }
            }
            else
            {
                // we sent a wrong request
                printf("%s\n", buffer);
            }
        }
    }

    printf("Client exiting succesfully!\n");
    close(client_socket);
    return 0;
}