#ifndef SERVER_CONFIG
#define SERVER_CONFIG
typedef struct server_config {
    char ip[16];
    int port;
    char cwd[128];
} server_config;

server_config * parse_config(const char * pathname);
#endif