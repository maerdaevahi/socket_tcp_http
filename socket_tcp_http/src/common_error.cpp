#include "common_error.h"
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
int perror_exit_conditional(int ret, const char *msg) {
    if (ret == -1) {
        printf("%d: %s\n", errno, strerror(errno));
        fflush(stdout);
        perror(msg);
        exit(1);
    }
    return ret;
}

int perror_exit_conditional_extra_on_errno(int ret, const char *msg, ...) {
    printf("%s\n", __func__);
    va_list valist;
    va_start(valist, msg);
    int err;
    while ((err = va_arg(valist, int))) {
        if (errno == err) {
            return ret;
        }
    }
    if (ret == -1) {
        perror(msg);
        exit(1);
    }
    return ret;
}

