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
#include <string.h>

int server_socket; // file descriptor for server socket
#define BACKLOG 1

void handle_signal(int sig)
{
    (void)sig; // Suppress unused warning
    printf("\nControl-C detected - shutting down server\n");
    close(server_socket);
    exit(0);
}

void reverse_string(char *string)
{
    int left_p = 0;
    int right_p = strlen(string) - 1;
    while (right_p > left_p)
    {
        char temp = string[left_p];
        string[left_p] = string[right_p];
        string[right_p] = temp;
        left_p++;
        right_p--;
    }
}

int main(int argc, char *argv[])
{
    // check that we got the server port
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <PORT>\n", argv[0]);
        return 1;
    }

    // Handle Ctrl+C gracefully
    signal(SIGINT, handle_signal);

    // storage items
    struct addrinfo hints, *res; // structs for server info

    // create server preferences
    memset(&hints, 0, sizeof hints); // this will clean out hints memory
    hints.ai_family = AF_UNSPEC;     // IPv4 address family
    hints.ai_socktype = SOCK_STREAM; // TCP connection
    hints.ai_flags = AI_PASSIVE;     // use localhost (fill in IP for me)

    // now resolve for the server itself
    if (getaddrinfo(NULL, argv[1], &hints, &res) != 0)
    {
        perror("getaddrinfo");
        return 1;
    }

    // now create the socket --> specify types and protocols
    if ((server_socket = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1)
    {
        perror("socket");
        freeaddrinfo(res);
        return 1;
    }
    if ((bind(server_socket, res->ai_addr, res->ai_addrlen)) == -1)
    {
        perror("bind");
        close(server_socket);
        freeaddrinfo(res);
        return 1;
    }

    freeaddrinfo(res);

    // now we have: server (ready) port(ready and binded) --> now ready to listen and accept
    if ((listen(server_socket, BACKLOG)) == -1)
    {
        perror("listen");
        close(server_socket);
        return 1;
    }

    printf("Server ready and listening on port: %s\n", argv[1]);

    // now do a while to keep accepting connections until terminated
    int new_client; // file descriptor for every client connection
    struct sockaddr_storage client_addr;
    socklen_t addr_size;
    char buffer[BUFSIZ];

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

        // receive the client message
        int recv_size = recv(new_client, buffer, sizeof(buffer) - 1, 0);
        if (recv_size == -1)
        {
            perror("recv");
            close(new_client);
            continue;
        }
        else if (recv_size == 0)
        {
            printf("Client disconnected.\n");
            close(new_client);
            continue;
        }

        // null terminate the string received
        buffer[recv_size] = '\0';
        printf("Received: %s\n", buffer);

        // tokenize and reverse the string
        char *token = strtok(buffer, " ");
        char response[BUFSIZ];
        int flag = 1;
        if (strcmp(token, "INVERT") == 0)
        {
            // we reverse
            token = strtok(NULL, " "); // the actual word we need to reverse
            reverse_string(token);
            // place answer into response buffer
            snprintf(response, sizeof(response), "200 OK\nContent-Length: %lu\nInversion: %s\n", strlen(token), token);
        }
        else
        {
            // we send back the error
            snprintf(response, sizeof(response), "501 NOT IMPLEMENTED\n");
            flag = 0;
        }

        // send response to client

        if (send(new_client, response, strlen(response), 0) == -1)
        {
            perror("send");
        }
        if (flag == 1)
        {
            printf("Sent: 200 OK\nSent: Content-Length: %lu\nSent: Inversion: %s\n", strlen(token), token);
        }
        else
        {
            printf("Sent: 501 NOT IMPLEMENTED\n");
        }
        // close the connection
        printf("Client connection finished!\n");
        close(new_client);
    }

    close(server_socket);
    return 0;
}