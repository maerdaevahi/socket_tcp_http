#include "base_socket.h"
#include "common_error.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <wait.h>
#include <stdlib.h>
void handle_sigpipe(int signo) {
    printf("%s %d\n", __func__, signo);
}

void handle_sigchild(int signo) {
    int w, wstatus;
    while (w = waitpid(-1, &wstatus, WUNTRACED | WCONTINUED | WNOHANG) > 0) {
        if (WIFEXITED(wstatus)) {
            printf("exited, status=%d\n", WEXITSTATUS(wstatus));
        } else if (WIFSIGNALED(wstatus)) {
            printf("killed by signal %d\n", WTERMSIG(wstatus));
        } else if (WIFSTOPPED(wstatus)) {
            printf("stopped by signal %d\n", WSTOPSIG(wstatus));
        } else if (WIFCONTINUED(wstatus)) {
            printf("continued\n");
        }

    }
    perror_exit_conditional_extra_on_errno(w, "waitpid", ECHILD);
    if (!w) {
        printf("no child exit\n");
    }
}
__attribute__((constructor)) void register_sig_handler() {
    printf("%s\n", __func__);
    signal(SIGPIPE, SIG_IGN);
    signal(SIGCHLD, handle_sigchild);
}

void test_recvfrom(int fd);

void test_getpeername(int fd);

void test_getsockname(int fd);

void test_write_fd_to_peer_closed(int fd);

int accept_connection(int listen_fd) {
    struct sockaddr_in peer_addr;
    socklen_t peer_addr_len = sizeof(peer_addr);
#if 0
    return perror_exit_conditional(accept(listen_fd, (struct sockaddr *)&peer_addr, &peer_addr_len), "accept");
#else
    int client_fd = perror_exit_conditional_extra_on_errno(
            accept(listen_fd, (struct sockaddr *) &peer_addr, &peer_addr_len), "accept", EAGAIN, ECHILD, NULL);
    char peer_ip[16] = {0};
    inet_ntop(AF_INET, &peer_addr.sin_addr.s_addr, peer_ip, sizeof(peer_ip));
    printf("accept connection from %s:%d\n", peer_ip, ntohs(peer_addr.sin_port));
    return client_fd;
#endif
}

void set_noblock(int fd) {
    int flag = perror_exit_conditional(fcntl(fd, F_GETFL), "fcntl");
    flag |= O_NONBLOCK;
    perror_exit_conditional(fcntl(fd, F_SETFL, flag), "fcntl");
}

int open_ipv4_tcp_listen_socket(const char * ip, int port) {
    int fd = open_ipv4_tcp_socket();
    enable_addr_reusable(fd);
    bind_addr(fd, ip, port);
    set_listen(fd);
    return fd;
}

int open_nonblock_ipv4_tcp_listen_socket(const char * ip, int port) {
    int fd = open_ipv4_tcp_listen_socket(ip, port);
    set_noblock(fd);
    return fd;
}



void set_listen(int listen_fd) { perror_exit_conditional(listen(listen_fd, 128), "listen"); }

void bind_addr(int listen_fd, const char * ip, int port) {
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &addr.sin_addr.s_addr);
    addr.sin_port = htons(port);
    perror_exit_conditional(bind(listen_fd, (struct sockaddr *) &addr, sizeof(addr)), "setsockopt");
}

int open_ipv4_tcp_socket() {
    int domain = AF_INET;
    int type = SOCK_STREAM;
    int protocol = 0;
    int listen_fd = perror_exit_conditional(socket(domain, type, protocol), "socket");
    return listen_fd;
}

void enable_addr_reusable(int listen_fd) {
    int level = SOL_SOCKET;
    int optname = SO_REUSEADDR;
    int optval = 1;
    socklen_t optlen = sizeof(optval);
    perror_exit_conditional(setsockopt(listen_fd, level, optname, &optval, optlen), "setsockopt");
}

int readn(int peer_fd, char * buf, int n) {
    char * pos = buf;
    char * end = buf + n;
    while (pos < end) {
        int count = recv(peer_fd, pos, end - pos, 0);
        if (count > 0) {
            pos += count;
        } else if (!count) {
            close(peer_fd);
            break;
        } else if (count == -1 && errno == EAGAIN) {
            continue;
        }
    }
    return pos - buf;
}

int read_n(int peer_fd, char * buf, int n, int * error) {
    char * pos = buf;
    char * end = buf + n;
    while (pos < end) {
        int count = recv(peer_fd, pos, end - pos, 0);
        if (count > 0) {
            pos += count;
        } else if (!count) {
            *error = 1;
            break;
        } else if (count == -1 && errno == EAGAIN) {
            continue;
        }
    }
    return pos - buf;
}

int readln(int fd, char * buf, int count) {
    char * pos = buf;
    char * end = buf + count;
    while (pos < end) {
        ssize_t ret = recv(fd, pos, end - pos, MSG_PEEK);
        if (ret == -1) {
            if (errno == EINTR) {
                continue;
            } else if (errno == EAGAIN) {
                break;
            }
            perror("recv");
            return -1;
        } else if (!ret) {
            printf("fd ends\n");
            close(fd);
            break;
        }
        int i = 0;
        for (; i < ret; ++i) {
            if (pos[i] == '\n') {
                break;
            }
        }
        int mount = i < ret ? i + 1 : ret;
        int n = readn(fd, pos, mount);
        if (n == -1 || n != mount) {
            return -1;
        }
        pos += n;
        if (i < ret) {
            break;
        }
    }
    return pos - buf;
}

int read_ln(int fd, char * buf, int count, int * error) {
    char * pos = buf;
    char * end = buf + count;
    while (pos < end) {
        ssize_t ret = recv(fd, pos, end - pos, MSG_PEEK);
        if (ret == -1) {
            if (errno == EINTR) {
                continue;
            } else if (errno == EAGAIN) {
                break;
            }
            perror("recv");
            return -1;
        } else if (!ret) {
            printf("fd ends\n");
            *error = 1;
            break;
        }
        int i = 0;
        for (; i < ret; ++i) {
            if (pos[i] == '\n') {
                break;
            }
        }
        int mount = i < ret ? i + 1 : ret;
        int n = read_n(fd, pos, mount, error);
        if (n == -1 || n != mount) {
            return -1;
        }
        pos += n;
        if (i < ret) {
            break;
        }
    }
    return pos - buf;
}

int hand_read(int fd) {
    return 0;
}

void test_write_fd_to_peer_closed(int fd) {
    //register_sigpipe_handler();
    char a = 'a';
    char buf[1024] = {0};
    recv(fd, buf, sizeof(buf), 0);
    printf("recv: %s", buf);
    perror_exit_conditional(write(fd, &a, sizeof(a)), "write");
    perror_exit_conditional(write(fd, &a, sizeof(a)), "write");
}

void test_getsockname(int fd) {
    struct sockaddr_in peer_addr;
    socklen_t peer_addr_len = sizeof(peer_addr);
    getsockname(fd, (struct sockaddr *)&peer_addr, &peer_addr_len);
    char peer_ip[16] = {0};
    inet_ntop(AF_INET, &peer_addr.sin_addr.s_addr, peer_ip, sizeof(peer_ip));
    printf("getsockname %s:%d\n", peer_ip, ntohs(peer_addr.sin_port));
}

void test_getpeername(int fd) {
    struct sockaddr_in peer_addr;
    socklen_t peer_addr_len = sizeof(peer_addr);
    getpeername(fd, (struct sockaddr *)&peer_addr, &peer_addr_len);
    char peer_ip[16] = {0};
    inet_ntop(AF_INET, &peer_addr.sin_addr.s_addr, peer_ip, sizeof(peer_ip));
    printf("accept connection from %s:%d\n", peer_ip, ntohs(peer_addr.sin_port));
}

void test_recvfrom(int fd) {
    char buf[BUFSIZ];
    struct sockaddr_in peer_addr;
    socklen_t peer_addr_len = sizeof(peer_addr);
    int n = recvfrom(fd, buf, sizeof(buf), 0, (struct sockaddr *)&peer_addr, &peer_addr_len);
    char peer_ip[16] = {0};
    inet_ntop(AF_INET, &peer_addr.sin_addr.s_addr, peer_ip, sizeof(peer_ip));
    printf("accept connection from %s:%d\n", peer_ip, ntohs(peer_addr.sin_port));
    printf("%d\n", n);
}
