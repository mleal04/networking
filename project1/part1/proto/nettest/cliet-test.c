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

int client_socket; // file descriptor for client socket

int main(int argc, char *argv[])
{
    // must take in: SERVER IP || SERVER PORT || STRING
    if (argc != 4)
    {
        printf("not enough arguments\n");
        return 1;
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

    // now send request
    char request[BUFSIZ];
    snprintf(request, sizeof(request), "INVERT %s", argv[3]);
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
        buffer[bytes_of_mess] = '\0'; // Null-terminate received message
        printf("Received a response\n");
        printf("%s\n", buffer);
    }

    printf("Client exiting succesfully!\n");
    close(client_socket);
    return 0;
}