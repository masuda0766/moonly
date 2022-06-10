/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*
* Copyright (c) 1999-2016 IVT Wireless
*
* All rights reserved.
*
---------------------------------------------------------------------------*/
/****************************************************************************
 * @file ring_buffer.h
 * This module is the sample application of bluetooth.
 *
 * None
 *
 * @version 1.0
 * 
 * @author Asher
 *
 ***************************************************************************/

#ifndef _RING_BUFFER_H_
#define _RING_BUFFER_H_

void *ring_buffer_alloc(unsigned int buf_len);
void ring_buffer_free(void *p_buf);
void ring_buffer_push(void *p_buf, unsigned char *p_data, unsigned int len);
unsigned int ring_buffer_pull(void *p_buf, unsigned char *pt, unsigned int len);
void ring_buffer_clean(void *p_buf);

#endif /* _RING_BUFFER_H_ */

