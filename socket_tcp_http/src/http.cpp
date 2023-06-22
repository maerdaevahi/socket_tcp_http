#include <malloc.h>
#include "string.h"
#include "http.h"
#include "base_socket.h"
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/sendfile.h>
#include "common_error.h"
#include "url_codec.h"
#include <dirent.h>
#include <cerrno>


void init_header(kv * hd, const char * name, const char * value) {
    strcpy(hd->name, name);
    int size = strlen(value) + 1;
    hd->value = (char *)malloc(size);
    strcpy(hd->value, value);
}

kv * create_header(const char * name, const char * value) {
    int size = sizeof(kv);
    kv * ptr = (kv *) malloc(size);
    init_header(ptr, name, value);
    return ptr;
}


void destory_header(kv * hd) {
    if (hd && hd->value) {
        free(hd->value);
        free(hd);
    }
}



#define set(x) \
do {\
    memset(hreq->x, 0, sizeof(hreq->x));\
    strcpy(hreq->x, x);\
} while (0)


void add_response_header(http_response *hres, const char *name, const char * value);

void encode_as_pathname(const http_context *hc, http_response *hres);

void encode_as_body(const http_context *hc, http_response *hres);

void handle_login(const void *ptr, const http_context *ctx);

void hand_list(const void *ptr, const char *pathname);

void init_http_request(http_request * hreq) {
    bzero(hreq->method, sizeof(hreq->method));
    bzero(hreq->uri, sizeof(hreq->uri));
    bzero(hreq->protocol, sizeof(hreq->protocol));
    hreq->headers = (kv **)malloc(16 * sizeof(kv *));
    bzero(hreq->headers, 16 * sizeof(kv *));
    hreq->body = NULL;
}

void set_method(http_request * hreq, const char * method) {
    set(method);
}

void set_uri(http_request * hreq, const char * uri) {
    set(uri);
}

void set_protocol(http_request * hreq, const char * protocol) {
    set(protocol);
}

void add_request_header(http_request * hreq, int i, const char * name, const char * value) {
    if (i >= 15) {
        printf("to many headers\n");
        return;
    }
    assert(!hreq->headers[i]);
    hreq->headers[i] = create_header(name, value);
}

void add_response_header(http_response * hres, int i, const char * name, const char * value) {
    if (i >= 15) {
        printf("to many headers\n");
        return;
    }
    assert(!hres->headers[i]);
    hres->headers[i] = create_header(name, value);
}

void destroy_http_request(http_request * hreq) {
    if (hreq && hreq->headers) {
        for (kv ** ptr = hreq->headers; *ptr; ++ptr) {
            destory_header(*ptr);
        }
        free(hreq->headers);
    }
    if (hreq && hreq->body) {
        free(hreq->body);
    }
}

void destroy_http_reqonse(http_response * hres) {
    if (hres && hres->headers) {
        for (kv ** ptr = hres->headers; *ptr; ++ptr) {
            destory_header(*ptr);
        }
        free(hres->headers);
    }
    if (hres && hres->body) {
        free(hres->body);
    }
}

void print(const http_request * hreq) {
    printf("method: %s\n",hreq->method);
    printf("uri: %s\n", hreq->uri);
    printf("protocol: %s\n", hreq->protocol);
    for (kv ** ptr = hreq->headers; *ptr; ++ptr) {
        printf("%s: %s\n", (*ptr)->name, (*ptr)->value);
    }
}

void parse_http_request(http_context * hc) {
    if (hc->status == CLOSED) {
        return;
    }
    int fd = hc->fd;
    http_request * hreq = &hc->hreq;
    char buf[BUFSIZ];
    bzero(buf, sizeof(buf));
    int error = 0;
    int n;
    n = read_ln(fd, buf, sizeof(buf), &error);
    if (error == 1) {
        hc->status = CLOSED;
        return;
    }
    buf[n] = 0;
    sscanf(buf, "%s %s %s\r\n", hreq->method, hreq->uri, hreq->protocol);
    url_decode(hreq->uri, strlen(hreq->uri));
    hc->status = LINE_PARSED;
    int i = 0;
    char buf1[32], buf2[512];
    while (1) {
        n = read_ln(fd, buf, sizeof(buf), &error);
        if (error == 1) {
            hc->status = CLOSED;
            return;
        }
        buf[n] = 0;
        if (!strcmp("\r\n", buf)) {
            hc->status = HEAD_PARSED;
            break;
        }
        bzero(buf1, sizeof(buf1));
        bzero(buf2, sizeof(buf2));
        sscanf(buf, "%[^: ]%*[: ]%[^\r\n]\r\n", buf1, buf2);
#ifdef DEBUG
        printf("%s=%s\n", buf1, buf2);
#endif
        add_request_header(hreq, i, buf1, buf2);
        ++i;
    }
    if (!strcasecmp(hreq->method, "GET")) {

    } else if (!strcasecmp(hreq->method, "POST")) {
        int content_length = 0;
        i = 0;
        for (kv ** ptr = hreq->headers; *ptr; ++i) {
            if (!strcasecmp(ptr[i]->name,"content-length")) {
                content_length = atoi(ptr[i]->value);
                break;
            }
        }
        if (content_length > 0) {
            read_n(fd, buf, content_length, &error);
            if (error == 1) {
                hc->status = CLOSED;
                return;
            }
            url_decode(buf, content_length);
            hreq->body = (char *)malloc(strlen(buf) + 1);
            strcpy(hreq->body, buf);
#ifdef DEBUG
            printf("body========================: %s\n", buf);
#endif
        }
    }
#ifdef DEBUG
    print(hreq);
#endif
}

void init_http_response(http_response * hres) {
    strcpy(hres->protocol, "http/1.1");
    strcpy(hres->code, "200");
    strcpy(hres->status, "ok");
    hres->headers = (kv **) malloc(16 * sizeof(kv *));
    bzero(hres->headers, 16 * sizeof(kv *));
    hres->body = NULL;
    bzero(hres->pathname, sizeof(hres->pathname));
}

void * mk_http_context(int fd) {
    http_context * hc = (http_context *) malloc(sizeof(http_context));
    hc->fd = fd;
    init_http_request(&hc->hreq);
    init_http_response(&hc->hres);
    return hc;
}
void free_http_context(void * ptr) {
    if (ptr) {
        http_context * hc = (http_context *)ptr;
        close(hc->fd);
        destroy_http_request(&hc->hreq);
        destroy_http_reqonse(&hc->hres);
        free(hc);
    }
}

void update_http_response(http_response * hres, const char * pathname, const char * content_type, const char * body) {
    add_response_header(hres, "content_type", content_type);
    if (pathname) {
        strcpy(hres->pathname, pathname);
    }
    if (body) {
        int size = strlen(body);
        hres->body = (char *)malloc(size + 1);
        strcpy(hres->body, body);
    }
}

void add_response_header(http_response *hres, const char *name, const char * value) {
    int i = 0;
    while (hres->headers[i]) {
        ++i;
    }
    add_response_header(hres, i, name, value);
}

void encode(void * ptr) {
    http_context * hc = (http_context *)ptr;
    if (hc->status == CLOSED) {
        return;
    }
    http_response * hres = &hc->hres;
    if (hres->body && strlen(hres->body)) {
        encode_as_body(hc, hres);
        return;
    } else if (hres->pathname && strlen(hres->pathname)) {
        encode_as_pathname(hc, hres);
    }
}

void encode_as_body(const http_context *hc, http_response *hres) {
    char buf[BUFSIZ] = "";
    sprintf(buf, "%s %s %s\r\n",hres->protocol, hres->code, hres->status);
    char tmp[32];
    sprintf(tmp, "%ld", strlen(hres->body));
    add_response_header(hres, "content-length", tmp);
    for (kv ** ptr = hres->headers; *ptr; ++ptr) {
        sprintf(buf + strlen(buf), "%s: %s\r\n", (*ptr)->name, (*ptr)->value);
    }
    sprintf(buf + strlen(buf), "\r\n");
    sprintf(buf + strlen(buf), "%s", hres->body);
    write(hc->fd, buf, strlen(buf));
    return;
}

void encode_as_pathname(const http_context *hc, http_response *hres) {
#ifdef DEBUG
    system("pwd; ls static");
#endif
    char buf[BUFSIZ] = "";
    sprintf(buf, "%s %s %s\r\n",hres->protocol, hres->code, hres->status);
    int fd = open(hres->pathname, O_RDONLY);
    perror_exit_conditional(fd, "open");
    struct stat attr;
    fstat (fd, &attr);
    char tmp[32];
    sprintf(tmp, "%ld", attr.st_size);
    add_response_header(hres, "content-length", tmp);
    for (kv ** ptr = hres->headers; *ptr; ++ptr) {
        sprintf(buf + strlen(buf), "%s: %s\r\n", (*ptr)->name, (*ptr)->value);
    }
    sprintf(buf + strlen(buf), "\r\n");
    write(hc->fd, buf, strlen(buf));
    off_t offset = 0;
    while (offset < attr.st_size) {
        int ret = sendfile(hc->fd, fd, &offset, attr.st_size - offset);
        if (ret == -1) {
            perror("sendfile");
            break;
        }
    }
    int ret = close(fd);
    if (ret == -1) {
        perror("close");
    }
}

void service(void * ptr) {
    http_context * ctx = (http_context *)ptr;
    if (ctx->status == CLOSED) {
        return;
    }
    char * uri = ctx->hreq.uri;
    if (!strcmp(uri, "") || !strcmp(uri, "/")) {
        update_http_response(&((http_context *)ptr)->hres, "static/index.html", "text/html", NULL);
    } else if (!strcmp(uri, "/login")) {
        handle_login(ptr, ctx);
    } else if (!strcmp(uri, "/list")) {
        hand_list(ptr, "static");
    } else {
            char pathname[160];
            sprintf(pathname, "%s%s", "static", uri);
#ifdef DEBUG
            printf("%s\n", pathname);
#endif
            int access_res = access(pathname, F_OK);
            if (access_res == -1 && errno == ENOENT) {
                update_http_response(&((http_context *)ptr)->hres, "static/404.html", "text/html", NULL);
                return;
            }
            struct stat attr;
            stat(pathname,&attr);
            if (S_ISREG(attr.st_mode)) {
                char * suffix = strrchr(uri, '.');
                if (!strcasecmp(suffix, ".mp4")) {
                    update_http_response(&((http_context *)ptr)->hres, pathname, "video/mpeg4", NULL);
                } else if (!strcasecmp(suffix, ".png")) {
                    update_http_response(&((http_context *)ptr)->hres, pathname, "application/x-png", NULL);
                } else if (!strcasecmp(suffix, ".ico")) {
                    update_http_response(&((http_context *)ptr)->hres, pathname, "image/x-icon", NULL);
                } else if (!strcasecmp(suffix, ".html")) {
                    update_http_response(&((http_context *)ptr)->hres, pathname, "text/html", NULL);
                } else {
                    update_http_response(&((http_context *)ptr)->hres, pathname, "text/plain", NULL);
                }
            } else if (S_ISDIR(attr.st_mode)) {
                hand_list(ptr, pathname);
            }
        }
}

void hand_list(const void *ptr, const char *pathname) {
    char table[1024] = "";
    struct dirent **namelist;
    int n = scandir(pathname, &namelist, NULL, alphasort);
    sprintf(table, "<!DOCTYPE html>\n"
                   "<html>\n"
                   "<head>\n"
                   "<meta charset=\"utf-8\">\n"
                   "<title>test http 测试</title>\n"
                   "</head>\n"
                   "<body>\n"
                   "<table>"
                   "<th><td>文件<td></th>");
    for (int i = 0; i < n; ++i) {
        if (namelist[i]->d_type & DT_REG || namelist[i]->d_type & DT_DIR) {
            const char * href_prefix = pathname + strlen("static");
            if (href_prefix[strlen(href_prefix) - 1] == '/') {
                sprintf(table + strlen(table), "<tr><td><a href=\"%s%s\">%s</a><td></tr>", href_prefix, namelist[i]->d_name, namelist[i]->d_name);
            } else {
                sprintf(table + strlen(table), "<tr><td><a href=\"%s/%s\">%s</a><td></tr>", href_prefix, namelist[i]->d_name, namelist[i]->d_name);
            }
        }
    }
    sprintf(table + strlen(table), "</table>" "</body>" "</html>");
    update_http_response(&((http_context *)ptr)->hres,NULL, "text/html", table);
}

void handle_login(const void *ptr, const http_context *ctx) {
#ifdef DEBUG
    if (ctx->hreq.body) {
        printf("%s\n", ctx->hreq.body);
    }
#endif
    const char * str = "<!DOCTYPE html>\n"
                       "<html>\n"
                       "<head>\n"
                       "<meta charset=\"utf-8\">\n"
                       "<title>test http</title>\n"
                       "</head>\n"
                       "<body>\n"
                       "    <h1>%s</h1>\n"
                       "    <p>%s</p>\n"
                       "</body>\n"
                       "</html>";

    char buf[512], username[64], password[64];
    int length;
    if (ctx->hreq.body && (length = strlen(ctx->hreq.body)) && length <= sizeof(username) && length <= sizeof(password)) {
        sscanf(ctx->hreq.body, "username=%[^=&]&password=%[^=]", username, password);
        if (!strlen(username) || !strlen(password)) {
            fprintf(stderr, "username or password is lack\n");
            sprintf(buf, "<html><head></head><body><p style=\"color:red\">username or password is lack</p>"
                         "<a href='/'>jump to index</a></body></html>");
        } else {
            sprintf(buf, str, username, password);
        }
    } else {
        fprintf(stderr, "username or password length is out of limit\n");
        sprintf(buf, "<html><head></head><body><p style=\"color:red\">username or password length is out of limit</p>"
                     "<a href='/'>jump to index</a></body></html>");
    }
    update_http_response(&((http_context *)ptr)->hres,NULL, "text/html", buf);
}