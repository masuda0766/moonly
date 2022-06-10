#ifndef __BT_CONF_H__
#define __BT_CONF_H__

#include "PinNames.h"
#include <platform_opts.h>

#define TL_RX_BUF_SIZE (4096)
#define TL_TX_DMA_BUF_SIZE (1536) // 1.5k
 
#define BT_UART_TX					PC_9
#define BT_UART_RX					PC_6

#ifndef CONFIG_AUDREY
#if defined(__GNUC__)
#define FLASH_USER_CONFIG_BT_BASE			(0x86800 - 0x2000)
#else
#define FLASH_USER_CONFIG_BT_BASE			(0x80000 - 0x2000)
#endif /* defined(__GNUC__) */
#endif

#endif /* __BT_CONF_H__ */
