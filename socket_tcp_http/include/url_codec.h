#ifndef UR_CODEC
#define UR_CODEC
#ifdef __cplusplus
extern "C" {
#endif
char *url_encode(char const *s, int len, int *new_length);
int url_decode(char *str, int len);
#ifdef __cplusplus
}
#endif
#endif