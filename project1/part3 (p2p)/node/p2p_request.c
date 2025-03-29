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
#include "helper.h"

// P2P COMMAND --> REGISTER --> REGISTER ACK
int register_node(int node_port_input, int n_files, char *tracker_ip, int tracker_port)
{
    // start data for the object
    uint8_t identifier = 0;
    const char *node_ip_str = "127.0.0.1";
    uint16_t node_port = node_port_input;
    uint16_t num_files = n_files;

    // Convert the node IP address string to a 32-bit integer
    struct in_addr ip_addr;
    if (inet_pton(AF_INET, node_ip_str, &ip_addr) <= 0)
    {
        perror("Invalid IP address");
        return -1;
    }

    // Create RegisterMessage structure
    RegisterMessage reg_msg;
    reg_msg.type = 0x01;
    reg_msg.identifier = identifier;
    reg_msg.ip_address = ip_addr.s_addr;
    reg_msg.port = htons(node_port);
    reg_msg.num_files = htons(num_files);
    reg_msg.length = htons(sizeof(RegisterMessage) - 3);

    // Create UDP socket
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        perror("Socket creation failed");
        return -1;
    }

    // Define tracker address
    struct sockaddr_in tracker_addr;
    memset(&tracker_addr, 0, sizeof(tracker_addr));
    tracker_addr.sin_family = AF_INET;
    tracker_addr.sin_port = htons(tracker_port);
    if (inet_pton(AF_INET, tracker_ip, &tracker_addr.sin_addr) <= 0)
    {
        perror("Invalid tracker IP address");
        return -1;
    }

    // Send RegisterMessage to the tracker
    ssize_t sent_bytes = sendto(sockfd, &reg_msg, sizeof(reg_msg), 0,
                                (struct sockaddr *)&tracker_addr, sizeof(tracker_addr));
    if (sent_bytes < 0)
    {
        perror("Failed to send message");
        close(sockfd);
        return -1;
    }

    printf("Register message sent successfully to tracker at %s:%d\n", tracker_ip, tracker_port);

    // get REG_ACK from register and print out to the node
    char response[1024];
    ssize_t response_len = recvfrom(sockfd, response, sizeof(response), 0, NULL, NULL);
    if (response_len < 0)
    {
        perror("Failed to receive response");
        close(sockfd);
        return -1;
    }

    // // received bytes --> debugging
    // for (int i = 0; i < response_len; i++)
    // {
    //     printf("Byte %02d: 0x%02X\n", i, (unsigned char)response[i]);
    // }

    // Parse response (skip: type and len)
    int status = response[3];                                                   // 1 byte
    int identifier2 = response[4];                                              // 1 byte
    unsigned char ip[4] = {response[5], response[6], response[7], response[8]}; // 4 bytes

    // correct port and files
    uint16_t nport, nfiles;
    memcpy(&nport, &response[9], sizeof(nport));
    memcpy(&nfiles, &response[11], sizeof(nfiles));

    int port = ntohs(nport);
    int num_files2 = ntohs(nfiles);

    // corrcet the expiration date
    uint32_t n_expiration_time;
    memcpy(&n_expiration_time, &response[13], sizeof(n_expiration_time));

    // Print the parsed response
    printf("Status: %d\n", status);
    printf("Identifier: %d\n", identifier2);
    printf("IP Address: %d.%d.%d.%d\n", ip[0], ip[1], ip[2], ip[3]);
    printf("Port: %d\n", port);
    printf("Number of Files: %d\n", num_files2);
    printf("Expiration Time: %d\n", n_expiration_time);

    // Close socket
    close(sockfd);
    return identifier2;
}

char *list_nodes(int tracker_port, char *tracker_ip, node_known_objects nodes_list[10])
{
    uint8_t MaxCount = 10;
    RegisterMessage2 reg_msg;
    reg_msg.type = 0x03;
    reg_msg.MaxCount = MaxCount;
    reg_msg.length = htons(sizeof(RegisterMessage2) - 3);

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        perror("Socket creation failed");
        return NULL;
    }

    struct sockaddr_in tracker_addr;
    memset(&tracker_addr, 0, sizeof(tracker_addr));
    tracker_addr.sin_family = AF_INET;
    tracker_addr.sin_port = htons(tracker_port);
    if (inet_pton(AF_INET, tracker_ip, &tracker_addr.sin_addr) <= 0)
    {
        perror("Invalid tracker IP address");
        close(sockfd);
        return NULL;
    }

    ssize_t sent_bytes = sendto(sockfd, &reg_msg, sizeof(reg_msg), 0,
                                (struct sockaddr *)&tracker_addr, sizeof(tracker_addr));
    if (sent_bytes < 0)
    {
        perror("Failed to send message");
        close(sockfd);
        return NULL;
    }

    printf("List-Nodes message sent successfully to tracker at %s:%d\n", tracker_ip, tracker_port);

    char response[1024];
    ssize_t response_len = recvfrom(sockfd, response, sizeof(response), 0, NULL, NULL);
    if (response_len < 0)
    {
        perror("Failed to receive response");
        close(sockfd);
        return NULL;
    }

    int type = response[0];
    int length = (response[1] << 8) | response[2];
    int MaxCount2 = response[4];
    int count = response[5];

    printf("Type = 0x%02X\n", type);
    printf("Length = 0x%04X\n", length);
    printf("MaxCount = 0x%02X\n", MaxCount2);
    printf("Count = 0x%02X\n", count);

    int byte = 6;
    int total_nodes = (response_len - 5) / 13;

    // 🔹 Allocate memory on heap instead of local stack buffer
    char *buffer = (char *)malloc(BUFSIZ);
    if (!buffer)
    {
        perror("Memory allocation failed");
        close(sockfd);
        return NULL;
    }
    buffer[0] = '\0'; // Ensure it's an empty string
    size_t buffer_len = 0;

    for (int i = 0; i < total_nodes; i++)
    {
        int identifier = response[byte++];

        unsigned char ip[4] = {response[byte++], response[byte++], response[byte++], response[byte++]};

        uint16_t nport;
        memcpy(&nport, &response[byte], sizeof(nport));
        byte += sizeof(nport);
        int port = ntohs(nport);

        uint16_t nfiles;
        memcpy(&nfiles, &response[byte], sizeof(nfiles));
        byte += sizeof(nfiles);
        int files = ntohs(nfiles);

        uint32_t n_last_hb;
        memcpy(&n_last_hb, &response[byte], sizeof(n_last_hb));
        byte += sizeof(n_last_hb);
        int last_hb = ntohl(n_last_hb);

        node_known_objects curr_node;
        curr_node.identifier = identifier;
        memcpy(curr_node.ip_address, ip, 4);
        curr_node.port = port;
        curr_node.files = files;
        curr_node.lastHB = last_hb;

        nodes_list[i] = curr_node;

        printf("Identifier = 0x%02X\n", identifier);
        printf("IP = %d.%d.%d.%d\n", ip[0], ip[1], ip[2], ip[3]);
        printf("Port = %d\n", port);
        printf("Files = %d\n", files);
        printf("Last HB = %d\n", last_hb);

        char curr_item[128];
        snprintf(curr_item, sizeof(curr_item), "%d;%d.%d.%d.%d;%d;%d;%d\n",
                 identifier, ip[0], ip[1], ip[2], ip[3], port, files, last_hb);

        size_t curr_len = strlen(curr_item);
        if (buffer_len + curr_len < BUFSIZ - 1)
        {
            strcat(buffer, curr_item);
            buffer_len += curr_len;
        }
        else
        {
            fprintf(stderr, "Buffer overflow risk! Data truncated.\n");
            break;
        }
    }

    close(sockfd);
    return buffer; //
}

// WHERE COMMUNICATION WITH OTHER NODES HAPPENS
// send back char_buffer to client node
char *p2p_request(char *ip_add, char *port, char *type, char *request)
{
    // establish regualar connection
    int client_socket_p2p;
    struct addrinfo hints, *res;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(ip_add, port, &hints, &res) != 0)
    {
        perror("getaddrinfo");
        return NULL;
    }

    if ((client_socket_p2p = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1)
    {
        perror("socket");
        freeaddrinfo(res);
        return NULL;
    }

    if ((connect(client_socket_p2p, res->ai_addr, res->ai_addrlen)) == -1)
    {
        perror("connect");
        close(client_socket_p2p);
        freeaddrinfo(res);
        return NULL;
    }

    // printf("Connected successfully to server at %s:%s\n", ip_add, port);
    freeaddrinfo(res);

    // distinguish the requests
    if (strcmp(type, "find_p2p") == 0)
    {

        if (send(client_socket_p2p, request, strlen(request), 0) == -1)
        {
            perror("send");
            close(client_socket_p2p);
            return NULL;
        }

        // Allocate memory for response buffer
        char *buffer = malloc(BUFSIZ);
        if (!buffer)
        {
            perror("malloc");
            close(client_socket_p2p);
            return NULL;
        }

        int bytes_of_mess = recv(client_socket_p2p, buffer, BUFSIZ - 1, 0);
        if (bytes_of_mess == -1)
        {
            perror("recv");
            free(buffer);
            buffer = NULL;
        }
        else if (bytes_of_mess == 0)
        {
            printf("Server disconnected.\n");
            free(buffer);
            buffer = NULL;
        }
        else
        {
            buffer[bytes_of_mess] = '\0'; // Ensure null termination
        }

        close(client_socket_p2p);
        return buffer;
    }
    else if (strcmp(type, "get_p2p") == 0)
    {
        // send the request to our server-node
        if (send(client_socket_p2p, request, strlen(request), 0) == -1)
        {
            perror("send");
            close(client_socket_p2p);
            return NULL;
        }

        // Allocate memory for response buffer
        char *buffer = malloc(BUFSIZ);
        if (!buffer)
        {
            perror("malloc");
            close(client_socket_p2p);
            return NULL;
        }

        // receive a response --> full together string that needs to be parsed
        int bytes_of_mess = recv(client_socket_p2p, buffer, BUFSIZ - 1, 0);
        if (bytes_of_mess == -1)
        {
            perror("recv");
            free(buffer);
            buffer = NULL;
        }
        else if (bytes_of_mess == 0)
        {
            printf("Server disconnected.\n");
            free(buffer);
            buffer = NULL;
        }
        else
        {
            buffer[bytes_of_mess] = '\0'; // Ensure null termination
        }

        close(client_socket_p2p);
        return buffer;
    }
    return "failed p2p";
}