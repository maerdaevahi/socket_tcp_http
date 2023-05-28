#ifndef HTTP
#define HTTP

typedef struct header {
    char name[32];
    char * value;
} kv;

typedef struct http_request {
    char method[8];
    char uri[128];
    char protocol[16];
    kv ** headers;
    char * body;

} http_request;

typedef struct http_response {
    char protocol[16];
    char code[4];
    char status[16];
    kv ** headers;
    char * body;
    char pathname[64];
} http_response;

typedef enum state {
    INIT, LINE_PARSED, HEAD_PARSED, PARSED, CLOSED
} state;

typedef struct http_context {
    int fd;
    http_request hreq;
    http_response hres;
    int status;
} http_context;

void parse_http_request(http_context * hc);
void * mk_http_context(int);
void free_http_context(void *);
void service(void * ptr);
void encode(void * ptr);
#endif