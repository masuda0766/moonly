/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*
* Copyright (c) 1999-2016 IVT Wireless
*
* All rights reserved.
*
---------------------------------------------------------------------------*/
/****************************************************************************
 * @file ring_buffer.c
 * This module is the sample application of bluetooth.
 *
 * None
 *
 * @version 1.0
 * 
 * @author Asher
 *
 ***************************************************************************/

#include "FreeRTOS.h"
#include "ring_buffer.h"

typedef struct tag_ring_buffer
{
	unsigned char *rcvbuf;
	unsigned int rcvmax;
	unsigned int rcvstart;
	unsigned int rcvend;
	unsigned int data_len;
} ring_buffer_t;

void *ring_buffer_alloc(unsigned int buf_len)
{
    ring_buffer_t *buf = pvPortMalloc(sizeof(ring_buffer_t));

    memset(buf, 0, sizeof(ring_buffer_t));
    buf->rcvmax = buf_len;
    buf->rcvbuf = pvPortMalloc(buf_len);
    buf->rcvstart = buf->rcvend = 0;

    return (void *)buf;
}

void ring_buffer_free(void *p_buf)
{
	ring_buffer_t *buf = (ring_buffer_t *)p_buf;
	if (buf != NULL) {
		if (buf->rcvbuf != NULL) {
			vPortFree(buf->rcvbuf);
		}
		vPortFree(buf);
	}
}

void ring_buffer_push(void *p_buf, unsigned char *p_data, unsigned int len)
{
	ring_buffer_t *buf = (ring_buffer_t *)p_buf;
	if (buf == NULL || p_data == NULL || len == 0) {
		return;
	}

	while (len-- > 0) {
		if ((buf->rcvend + 1) == buf->rcvstart) {
			break; /* ring buffer is full */
		}
		buf->rcvbuf[buf->rcvend++] = *p_data++;
		if (buf->rcvend >= buf->rcvmax) {
			buf->rcvend = 0;
		}
		buf->data_len++;
	}
}

unsigned int ring_buffer_pull(void *p_buf, unsigned char *pt, unsigned int len)
{
	unsigned long i = 0;
	ring_buffer_t *buf = (ring_buffer_t *)p_buf;

	if (buf == NULL || pt == NULL || len == 0) {
		return 0;
	}

	while (i < len) {
		if(buf->rcvstart == buf->rcvend) {
			break;
		}
		pt[i++] = buf->rcvbuf[buf->rcvstart++];
		if (buf->rcvstart >= buf->rcvmax) {
			buf->rcvstart = 0;
		}
		buf->data_len--;
	}
	return i;
}

void ring_buffer_clean(void *p_buf)
{
	ring_buffer_t *buf = (ring_buffer_t *)p_buf;
	if (buf != NULL) {
		buf->rcvstart = buf->rcvend = 0;
	}
}
