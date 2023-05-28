#ifndef COMMON_ERROR
#define COMMON_ERROR
#ifdef __cplusplus
extern "C" {
#endif
int perror_exit_conditional(int ret, const char *msg);

int perror_exit_conditional_extra_on_errno(int ret, const char *msg, ...);
#ifdef __cplusplus
}
#endif

#endif