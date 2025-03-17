#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include "index-test.h"
#include "base64-encode.h"

int server_socket; // file descriptor for server socket
#define BACKLOG 1
char dir_path2[BUFSIZ];
DirectoryResult result;

char *find_func(char *token)
{
    static char answer[BUFSIZ * 2]; // Use static so the memory persists after function returns
    char curr_list[BUFSIZ] = "";    // Stores matching files
    int matched = 0;

    token = strtok(NULL, " ");
    if (token == NULL)
    {
        snprintf(answer, sizeof(answer), "400 Bad Request\n");
        return answer;
    }

    char *copy_pattern = strdup(token); // Make a copy to avoid strtok modifying it

    if (token[0] == '*')
    {
        token++; // Remove leading '*' for pattern
        for (int i = 0; i < result.file_count; i++)
        {
            if (strstr(result.file_paths[i], token) != NULL)
            {
                // Append the file path with a newline
                if (strlen(curr_list) + strlen(result.file_paths[i]) + 2 < sizeof(curr_list))
                {
                    strcat(curr_list, result.file_paths[i]);
                }
                matched++;
            }
        }
    }

    if (matched > 0)
    {
        snprintf(answer, sizeof(answer), "200 OK\nSearch Pattern: %s\nMatched Files: %d\n\n%s\n", copy_pattern, matched, curr_list);
    }
    else
    {
        snprintf(answer, sizeof(answer), "404 No files matched the pattern.\n");
    }

    free(copy_pattern);
    return answer;
}

char *get_func(char *token, int new_client)
{
    static char answer[BUFSIZ * 2]; // Use static so the memory persists after function returns
    token = strtok(NULL, " ");
    int index = atoi(token);

    // can we index into the file?
    if (index > result.file_count)
    {
        snprintf(answer, sizeof(answer), "400 Bad Request\n");
        send(new_client, answer, strlen(answer), 0);
        return "1";
    }

    // grab the file we need
    char *correct_file = result.file_paths[index - 1];

    // Make a copy of the correct_file string to safely use strtok
    char file_copy[BUFSIZ];
    strncpy(file_copy, correct_file, sizeof(file_copy));

    // Parse the file path, size, and date
    char *first_path = strtok(file_copy, ";");
    first_path = strtok(NULL, ";");
    char *file_size = strtok(NULL, ";");
    char *file_date = strtok(NULL, ";");

    char base64_path[BUFSIZ];
    // now create the full correct path to send to base64
    snprintf(base64_path, sizeof(base64_path), "%s%s", dir_path2, first_path);

    // begin base64
    char **buffer = fix_name(base64_path);
    size_t encoded_size = 0;
    char *encoded_data = encode_file(buffer[0], &encoded_size);

    // Send the response with the encoded file data
    snprintf(answer, sizeof(answer),
             "200 OK\nRequest File: %d\nFile-Name: %s\nFile-Size: %s\nFile-Date: %sEncoded-Size: %zu\n\n%s",
             index, first_path, file_size, file_date, encoded_size, encoded_data);

    // Free allocated memory
    free(encoded_data);
    free(buffer[0]);
    free(buffer[1]);
    free(buffer);
    return answer;
}

void handle_signal(int sig)
{
    (void)sig; // Suppress unused warning
    printf("\nControl-C detected - shutting down server\n");
    close(server_socket);
    exit(0);
}

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        printf("Not enough arguments\n");
        return 1;
    }

    // Get the index_struct
    strcpy(dir_path2, argv[2]);
    char *dir_path = argv[2];
    result = open_dir(dir_path, "");

    // Handle Ctrl+C gracefully
    signal(SIGINT, handle_signal);

    // Begin building server
    struct addrinfo hints, *res;

    memset(&hints, 0, sizeof hints); // Clear the memory for hints
    hints.ai_family = AF_UNSPEC;     // IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP connection
    hints.ai_flags = AI_PASSIVE;     // Use the server's IP address

    // Resolve for the server address
    if (getaddrinfo(NULL, argv[1], &hints, &res) != 0)
    {
        perror("getaddrinfo");
        return 1;
    }

    // Create socket
    if ((server_socket = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1)
    {
        perror("socket");
        freeaddrinfo(res);
        return 1;
    }

    // Bind socket
    if (bind(server_socket, res->ai_addr, res->ai_addrlen) == -1)
    {
        perror("bind");
        close(server_socket);
        freeaddrinfo(res);
        return 1;
    }

    freeaddrinfo(res);

    // Listen for incoming connections
    if (listen(server_socket, BACKLOG) == -1)
    {
        perror("listen");
        close(server_socket);
        return 1;
    }

    printf("Server ready and listening on port: %s\n", argv[1]);

    int new_client; // File descriptor for every client connection
    struct sockaddr_storage client_addr;
    socklen_t addr_size;
    char buffer[BUFSIZ];

    // Accept multiple client connections
    while (1)
    {
        printf("Waiting for new client connections...\n");
        addr_size = sizeof client_addr;
        new_client = accept(server_socket, (struct sockaddr *)&client_addr, &addr_size);
        if (new_client == -1)
        {
            perror("accept");
            continue;
        }
        printf("New client connected!\n");

        // Handle client communication
        while (1)
        {
            int recv_size = recv(new_client, buffer, sizeof(buffer) - 1, 0);
            if (recv_size == -1)
            {
                perror("recv");
                close(new_client);
                continue;
            }

            buffer[recv_size] = '\0';               // Null terminate the received data
            buffer[strcspn(buffer, "\r\n")] = '\0'; // Remove trailing newlines
            printf("Received: '%s'\n", buffer);

            char answer[BUFSIZ * 2]; // Increased buffer size for the answer
            char *token = strtok(buffer, " ");

            // Handle HELO command
            if (token != NULL && strcmp(token, "HELO") == 0)
            {
                snprintf(answer, sizeof(answer), "200 OK\nPort: %s\nContent-Directory: %s\nIndexed-Files: %d\n", argv[1], argv[2], result.file_count);
            }
            // Handle FIND command
            else if (token != NULL && strcmp(token, "FIND") == 0)
            {
                strcpy(answer, find_func(token));
            }
            else if (token != NULL && strcmp(token, "GET") == 0)
            {
                char answer[BUFSIZ * 2];
                strcpy(answer, get_func(token, new_client));
                if (strcmp(answer, "1") == 0)
                {
                    continue;
                }
            }
            // Handle END command
            else if (token != NULL && strcmp(token, "END") == 0)
            {
                snprintf(answer, sizeof(answer), "200 OK\nClosing connection.\n");
                send(new_client, answer, strlen(answer), 0);
                close(new_client);
                break;
            }
            else
            {
                printf("we got a bad request here\n");
                snprintf(answer, sizeof(answer), "400 Bad Request\n");
                send(new_client, answer, strlen(answer), 0);
                continue;
            }

            send(new_client, answer, strlen(answer), 0);
        }
    }

    close(server_socket);
    return 0;
}