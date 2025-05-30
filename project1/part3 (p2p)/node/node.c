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
#include <dirent.h>
#include <sys/stat.h>  // For mkdir
#include <sys/types.h> // For mkdir (depending on system)
#include "helper.h"

#define BACKLOG 1
int server_socket;
char dir_path2[BUFSIZ];
DirectoryResult result;
int node_id = 0;
node_known_objects nodes_list[10];

// fucntion to find the correct node on the list
int find_node_info(int token_id)
{
    for (int i = 0; i < 10; i++)
    {
        if (token_id == nodes_list[i].identifier)
        {
            return i;
        }
    }
    return 0;
}

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
    static char answer[BUFSIZ * BUFSIZ]; // Use static so the memory persists after function returns
    int index = atoi(token);             // file number

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
    // New commands inputs: ./name node_port , Index_dir, tracker_ip, tracker_port, stage_dir
    if (argc != 6)
    {
        printf("Not enough arguments\n");
        return 1;
    }

    // open index_dir AND get_results
    strcpy(dir_path2, argv[2]);
    char *dir_path = argv[2];
    result = open_dir(dir_path, "");

    // handle Ctrl+C gracefully
    signal(SIGINT, handle_signal);

    // begin building the server
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

    // REGISTER THE NODE/SERVER
    node_id = register_node(atoi(argv[1]), result.file_count, argv[3], atoi(argv[4]));

    // server can now start listening for connections
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

            buffer[recv_size] = '\0';               // null terminate the request
            buffer[strcspn(buffer, "\r\n")] = '\0'; // remove trailing new lines from request
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
            else if (token != NULL && strcmp(token, "FIND.") == 0)
            {
                strcpy(answer, find_func(token));
                send(new_client, answer, strlen(answer), 0);
                close(new_client);
                break;
            }
            else if (token != NULL && strcmp(token, "GET") == 0)
            {
                char answer[BUFSIZ * 2];
                token = strtok(NULL, " "); // file number
                strcpy(answer, get_func(token, new_client));
                if (strcmp(answer, "1") == 0)
                {
                    continue;
                }
            }
            else if (token != NULL && strcmp(token, "GET.") == 0)
            {
                // here we receive GET. 3 --> we need to send back buffer (whole integrated response)
                // in server-client side parse
                char answer[BUFSIZ * 2];
                token = strtok(NULL, " "); // the file number (3)
                strcpy(answer, get_func(token, new_client));
                if (strcmp(answer, "1") == 0)
                {
                    send(new_client, answer, strlen(answer), 0);
                    close(new_client);
                    break;
                }
                send(new_client, answer, strlen(answer), 0);
                close(new_client);
                break;
            }
            // Handle END command
            else if (token != NULL && strcmp(token, "END") == 0)
            {
                snprintf(answer, sizeof(answer), "200 OK\nClosing connection.\n");
                send(new_client, answer, strlen(answer), 0);
                close(new_client);
                break;
            }
            // Handle the REFRESHR command (for the server to update its knowledge)
            else if (token != NULL && strcmp(token, "REFRESHR") == 0)
            {
                // 1. we have a global node_list for the server to be aware of all other nodes
                // 2. we send the tracker info + the list to this function
                // 3. in the function --> we make the request to the tracker by making a object message
                // 4. we get bytes as the response which we have to parse
                // 5. as we parse we create node_objects and we neter the info, once done we append the object to the Nodes_list
                // 6. once done, we should come back here and have an updated list of nodes
                char buffer[BUFSIZ];
                strcpy(buffer, list_nodes(atoi(argv[4]), argv[3], nodes_list));
                // list_nodes(atoi(argv[4]), argv[3], nodes_list);
                printf("done updating the node list\n");
                strcpy(answer, buffer);
                // snprintf(answer, sizeof(answer), "\n"); // WE SEND BACK AN EMPTY THING BACK TO CLIENT
            }
            else if (token != NULL && strcmp(token, "FINDR") == 0)
            {
                // get the node ID
                token = strtok(NULL, " ");
                if (atoi(token) == node_id) // itself can resolve p2p request
                {
                    strcpy(answer, find_func(token));
                }
                else // p2p request to other node
                {
                    // 1. estalblish a UDP connection with the other node
                    // 2. node will receive FIND request
                    // 3. node will answer back with buffer to the clinet-node
                    // 4. client node (here) will send back buffer to client
                    int correct_index = find_node_info(atoi(token));
                    char ip_add[BUFSIZ];
                    char port[BUFSIZ];
                    snprintf(ip_add, sizeof(ip_add), "%u.%u.%u.%u", nodes_list[correct_index].ip_address[0],
                             nodes_list[correct_index].ip_address[1],
                             nodes_list[correct_index].ip_address[2],
                             nodes_list[correct_index].ip_address[3]);
                    snprintf(port, sizeof(port), "%d", nodes_list[correct_index].port);
                    // now client-node ready to send request to other node
                    token = strtok(NULL, " "); // pattern
                    char request[BUFSIZ];      // make the request line
                    snprintf(request, sizeof(request), "FIND. %s", token);

                    // SEND REQUEST TO ANOTHER NODE
                    char *response = p2p_request(ip_add, port, "find_p2p", request);
                    if (response)
                    {
                        strcpy(answer, response);
                        free(response); // Free allocated memory after copying
                    }
                    else
                    {
                        strcpy(answer, "Error: Request failed.");
                    }
                }
            }
            else if (token != NULL && strcmp(token, "GETR") == 0)
            {
                // 1. check itself --> if not continue
                // 2. place request to other node
                // 3. get back buffer and file pointer
                // 4. place file in staging directory
                // 5. send to client [buffer,file]
                // 6. happy client

                // get the node id
                char n_id[64];             // id
                char file[256];            // file
                token = strtok(NULL, " "); // file
                strcpy(file, token);
                token = strtok(NULL, " "); // node
                strcpy(n_id, token);
                if (atoi(n_id) == node_id)
                {
                    // itself can resolve p2p request
                    // we dont need staging directory
                    char answer[BUFSIZ * 2];
                    strcpy(answer, get_func(file, new_client));
                    if (strcmp(answer, "1") == 0)
                    {
                        continue; // this is an error
                    }
                }
                else
                {
                    // we need p2p request and staging directory
                    int correct_index = find_node_info(atoi(n_id)); // node id
                    char ip_add[BUFSIZ];
                    char port[BUFSIZ];
                    snprintf(ip_add, sizeof(ip_add), "%u.%u.%u.%u", nodes_list[correct_index].ip_address[0],
                             nodes_list[correct_index].ip_address[1],
                             nodes_list[correct_index].ip_address[2],
                             nodes_list[correct_index].ip_address[3]);
                    snprintf(port, sizeof(port), "%d", nodes_list[correct_index].port);
                    // now client-node ready to send request to other node
                    // token = strtok(NULL, " "); // file number we need
                    char request[BUFSIZ];                                // make the request line
                    snprintf(request, sizeof(request), "GET. %s", file); // file index
                    // full response string that needs to be parsed as (client-node)
                    char *response = p2p_request(ip_add, port, "get_p2p", request);
                    // send back response string to client
                    strcpy(answer, response);
                    // send(new_client, answer, strlen(answer), 0);
                    // continue;
                }
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