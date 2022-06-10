
#include "platform_opts.h"



#include <sys.h>
#include <device_lock.h>
#include "lwip/netdb.h"
#include <osdep_service.h>

#if CONFIG_USE_MBEDTLS

#include "mbedtls/config.h"
#include "mbedtls/platform.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "mbedtls/error.h"
#include "mbedtls/debug.h"

#endif // CONFIG_USE_MBEDTLS

#include "ssl_my_func.h"

//#define DEBUG_PRINT_ENABLE
#ifdef DEBUG_PRINT_ENABLE
#define D(fmt, ...)  Log_Info("\r\n[DEBUG]%s::[%d] " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define DD(fmt, ...)  Log_Info(fmt, ##__VA_ARGS__)
#else
#define D(fmt, ...)
#define DD(fmt, ...)
#endif

#define INFO_PRINT_ENABLE
#ifdef INFO_PRINT_ENABLE
#define DI(fmt, ...)  Log_Info("\r\n[INFO]%s::[%d] " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define DDI(fmt, ...)  Log_Info(fmt, ##__VA_ARGS__)
#else
#define DI(fmt, ...)
#define DDI(fmt, ...)
#endif

#define ERROR_PRINT_ENABLE
#ifdef ERROR_PRINT_ENABLE
#define DE(fmt, ...)  Log_Error("\r\n[ERROR]%s::[%d] " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define DDE(fmt, ...)  Log_Error(fmt, ##__VA_ARGS__)
#else
#define DE(fmt, ...)
#define DDE(fmt, ...)
#endif // ERROR_PRINT_ENABLE

extern int ota_update_task_stop;

int wait_read_select_with_line(int read_fd, int line)
{
	struct timeval tv;
	fd_set readfds;
	int set_fd;
	int count;
	int ret = -1;

	count = 10;
	while(count --)
	{
		FD_ZERO(&readfds);
		FD_SET(read_fd, &readfds);
		tv.tv_sec = 10;
		tv.tv_usec = 0;

		ret = select(read_fd + 1, &readfds, NULL, NULL, &tv);
		D(" [%d] read fd=%d select ret = %x count=%d\n", xTaskGetTickCount(), read_fd, ret, count);
		if(ret > 0)
		{
			if(FD_ISSET(set_fd, &readfds))
			{
				if (read_fd == set_fd) return 0;
			}
			//else for other sockets
		}
		DI(" [%d] read fd=%d select ret = %x count=%d\n", xTaskGetTickCount(), read_fd, ret, count);
		if(ota_update_task_stop == 1) return -1;
	}
	DI(" [%d]select time over  fd=%d", line, read_fd);
	return -1;
}


int wait_write_select_with_line(int write_fd, int line)
{
	struct timeval tv;
	fd_set writefds;
	int set_fd;
	int count;
	int ret = -1;

	count = 10;
	while(count --)
	{
		FD_ZERO(&writefds);
		FD_SET(write_fd, &writefds);
		tv.tv_sec = 10;
		tv.tv_usec = 0;

		ret = select(write_fd + 1, NULL, &writefds, NULL, &tv);
		D(" [%d] write fd=%d select ret = %x count=%d\n", xTaskGetTickCount(), write_fd, ret, count);
		if(ret > 0)
		{
			if(FD_ISSET(set_fd, &writefds))
			{
				if (write_fd == set_fd) return 0;
			}
			//else for other sockets
		}
		DI(" [%d] write fd=%d select ret = %x count=%d\n", xTaskGetTickCount(), write_fd, ret, count);
	}
	DI(" [%d]select time over  fd=%d", line, write_fd);
	return -1;
}
#if CONFIG_SSL_HTTPS_CLIENT

static const  char *getfilename(const char *filepath)
{
	int len;
	int i;
	if (filepath == NULL) return "NULL";

	len = strlen(filepath);

	if (len <= 0) return "len_error";

	for (i = len; i > 0; i--) {
		if (filepath[i] == '/') {
			return  &filepath[i+1];
		}
	}
	return filepath;
}



static size_t min_heap_size = 0;

size_t get_min_heap_size(void)
{
	return min_heap_size;
}
void ssl_my_debug(void *ctx, int level, const char *file, int line, const char *str)
{
	if(level <= DEBUG_LEVEL) {
		Log_Always("\n\r@@%d@@%s[%d] %s", level, getfilename(file), line, str);
	}
}


int ssl_my_random(void *p_rng, unsigned char *output, size_t output_len)
{
	rtw_get_random_bytes(output, output_len);
	return 0;
}

static int my_alloc_count = 0;
static int my_alloc_sdram_count = 0;
static int my_free__count = 0;
static int my_free__sdram_count = 0;
#define MY_ALLOC_LARGE_MAX 4
unsigned int sd_malloc_area_use[MY_ALLOC_LARGE_MAX];
char sd_malloc_area[MY_ALLOC_LARGE_MAX][20032];

void ssl_my_alloc_init(void)
{
	for (int i = 0; i < MY_ALLOC_LARGE_MAX; i++) {
		sd_malloc_area_use[i] = 0;
	}
}
void* ssl_my_malloc(size_t size)
{
	void *ptr = NULL;
	int i;

	my_alloc_count++;
	i = -1;
	if (size > 10000 && size < 20000) {
		for (i = 0; i < MY_ALLOC_LARGE_MAX; i++) {
			if (sd_malloc_area_use[i] == 0) {
				ptr = sd_malloc_area[i] + 32;
				sd_malloc_area_use[i] = (unsigned int)ptr;
				DI("ssl_my_malloc sd_malloc_area_use[%d] 0x%x size=%d", i, ptr, size);
				my_alloc_sdram_count ++;
				return ptr;
			}
		}
	}
	ptr = pvPortMalloc(size);


	if (size > 10000) {
		DI("ssl_my_malloc size=%d ptr=%x (%d)", size, ptr, rtw_getFreeHeapSize());
	}

	size_t current_heap_size = xPortGetFreeHeapSize();
	//D("my_malloc size=%d current_heap_size=%d", size, current_heap_size);

	if((current_heap_size < min_heap_size) || (min_heap_size == 0))
		min_heap_size = current_heap_size;

	return ptr;
}

void ssl_my_free(void *p)
{
	int i;

	my_free__count++;

	for (i = 0; i<MY_ALLOC_LARGE_MAX; i++) {
		if (sd_malloc_area_use[i] == (unsigned int)p) {
			sd_malloc_area_use[i] = 0;
			D("ssl_my_free sd_malloc_area_use[%d] 0x%x", i, p);
			my_free__sdram_count ++;
			return;
		}
	}

	vPortFree(p);

}


static int my_update_alloc_count = 0;
static int my_update_alloc_sdram_count = 0;
static int my_update_free_count = 0;
static int my_update_free_sdram_count = 0;
#define MY_UPDATE_ALLOC_LARGE_MAX 4
unsigned int sd_update_malloc_area_use[MY_UPDATE_ALLOC_LARGE_MAX];
char sd_update_malloc_area[MY_UPDATE_ALLOC_LARGE_MAX][4096+32+8];


void update_my_alloc_init(void)
{
	for (int i = 0; i < MY_UPDATE_ALLOC_LARGE_MAX; i++) {
		sd_update_malloc_area_use[i] = 0;
	}
}
void* update_my_malloc(size_t size)
{
	void *ptr = NULL;
	int i;

	my_update_alloc_count++;
	i = -1;
	if (size > 500 && size <= 4096) {
		for (i = 0; i < MY_UPDATE_ALLOC_LARGE_MAX; i++) {
			if (sd_update_malloc_area_use[i] == 0) {
				ptr = sd_update_malloc_area[i] + 32;
				sd_update_malloc_area_use[i] = (unsigned int)ptr;
				DI(" ++ sd_update_malloc_area_use[%d] 0x%x size=%d", i, ptr, size);
				my_update_alloc_sdram_count ++;
				return ptr;
			}
		}
		DI(" over");
	}
	ptr = pvPortMalloc(size);


	if (size > 500) {
		DI(" size=%d ptr=%x (%d)", size, ptr, rtw_getFreeHeapSize());
	}

	size_t current_heap_size = xPortGetFreeHeapSize();
	//D("my_malloc size=%d current_heap_size=%d", size, current_heap_size);

	if((current_heap_size < min_heap_size) || (min_heap_size == 0))
		min_heap_size = current_heap_size;

	return ptr;
}

void update_my_free(void *p)
{
	int i;

	my_update_free_count++;

	for (i = 0; i<MY_UPDATE_ALLOC_LARGE_MAX; i++) {
		if (sd_update_malloc_area_use[i] == (unsigned int)p) {
			sd_update_malloc_area_use[i] = 0;
			DI(" -- sd_update_malloc_area_use[%d] 0x%x", i, p);
			my_update_free_sdram_count ++;
			return;
		}
	}

	vPortFree(p);

}

void* ssl_my_calloc(size_t nelements, size_t elementSize)
{
	size_t current_heap_size, size;
	void *ptr = NULL;

	size = nelements * elementSize;
	ptr = ssl_my_malloc(size); // pvPortMalloc(size);

	if(ptr) {
		memset(ptr, 0, size);
	}

	current_heap_size = xPortGetFreeHeapSize();

	if((current_heap_size < min_heap_size) || (min_heap_size == 0)) {
		min_heap_size = current_heap_size;
	}

	return ptr;
}

int my_ssl_read( int *fd, unsigned char *buf, size_t len )
{
	if (wait_read_select(*fd)) {
		DI(" select time out MBEDTLS_ERR_SSL_TIMEOUT");
		return MBEDTLS_ERR_SSL_TIMEOUT; //-1;
	}
#if CONFIG_USE_MBEDTLS
	return mbedtls_net_recv(fd, buf, len);
#else
	return net_recv(fd, buf, len);
#endif
}

int my_ssl_send( int *fd, unsigned char *buf, size_t len )
{
	if (wait_write_select(*fd)) {
		DI(" select error");
		return -1;
	}
#if CONFIG_USE_MBEDTLS
	return mbedtls_net_send(fd, buf, len);
#else
	return net_send(fd, buf, len);
#endif
}

#endif // CONFIG_SSL_HTTPS_CLIENT
