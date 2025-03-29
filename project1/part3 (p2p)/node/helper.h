#ifndef INDEX_TEST_H
#define INDEX_TEST_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
// where do we use it: node.c,base64-test.c p2p_request.c

// OBJECTS
typedef struct // to store indexed results
{
    char **file_paths; // Array of file paths (stored in strings)
    int file_count;    // Number of files found
} DirectoryResult;

#pragma pack(push, 1) // register request object
typedef struct
{
    uint8_t type;        // 1 byte (type of request)
    uint16_t length;     // 2 bytes (len of data)
    uint8_t identifier;  // 1 byte  (node identifier)
    uint32_t ip_address; // 4 bytes (node IP)
    uint16_t port;       // 2 bytes  (node port)
    uint16_t num_files;  // 2 bytes  (node num files)
} RegisterMessage;
#pragma pack(pop)

// list-node message object
#pragma pack(push, 1)
typedef struct
{
    uint8_t type;     // 1 byte
    uint16_t length;  // 2 bytes
    uint8_t MaxCount; // 1 byte
} RegisterMessage2;
#pragma pack(pop)

// struct for the node_curr_list objects
#pragma pack(push, 1)
typedef struct
{
    int identifier;
    unsigned char ip_address[4];
    int port;
    int files;
    int lastHB;
} node_known_objects;
#pragma pack(pop)

// DEFINITIONS
DirectoryResult open_dir(const char *path_to_dir, const char *next_dir);
void free_directory_result(DirectoryResult result);
char **fix_name(char *curr_file_name);                       // Function to fix the file name and path
char *encode_file(char *path_to_file, size_t *encoded_size); // Function to encode a file and return the encoded string and its size
int register_node(int node_port_input, int n_files, char *tracker_ip, int tracker_port);
char *list_nodes(int tracker_port, char *tracker_ip, node_known_objects nodes_list[10]);
char *p2p_request(char *ip_add, char *port, char *type, char *request);
#endif