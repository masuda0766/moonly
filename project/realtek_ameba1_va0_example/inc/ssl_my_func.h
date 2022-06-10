#include "platform_opts.h"

#define DEBUG_LEVEL 1

#define my_free		vPortFree

#define wait_read_select(a) wait_read_select_with_line(a, __LINE__)
#define wait_write_select(a) wait_write_select_with_line(a, __LINE__)

int wait_read_select_with_line(int read_fd, int line);
int wait_write_select_with_line(int write_fd, int line);

#if CONFIG_SSL_HTTPS_CLIENT

void ssl_my_alloc_init(void);
void* ssl_my_malloc(size_t size);
void ssl_my_free(void *p);
void update_my_alloc_init(void);
void* update_my_malloc(size_t size);
void update_my_free(void *p);
void* ssl_my_calloc(size_t nelements, size_t elementSize);
int my_ssl_read( int *fd, unsigned char *buf, size_t len );
int my_ssl_send( int *fd, unsigned char *buf, size_t len );

void ssl_my_debug(void *ctx, int level, const char *file, int line, const char *str);
int ssl_my_random(void *p_rng, unsigned char *output, size_t output_len);

size_t get_min_heap_size(void);

#endif


