#ifndef BASE_SOCKET
#define BASE_SOCKET

#ifdef __cplusplus
extern "C" {
#endif

int open_ipv4_tcp_listen_socket(const char *ip, int port);

void set_noblock(int listen_fd);

void enable_addr_reusable(int listen_fd);

int open_ipv4_tcp_socket();

void bind_addr(int listen_fd, const char *ip, int port);

void set_listen(int listen_fd);

int accept_connection(int listen_fd);

int readn(int peer_fd, char *buf, int n);

int readln(int peer_fd, char *buf, int n);

int read_ln(int fd, char *buf, int count, int *error);

int read_n(int peer_fd, char *buf, int n, int *error);

int hand_read(int fd);

#ifdef __cplusplus
}
#endif
#endif