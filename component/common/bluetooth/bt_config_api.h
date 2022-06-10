#ifndef __BT_CONFIG_API_H__
#define __BT_CONFIG_API_H__

#include "semphr.h"

typedef enum _BTConfig_ConnType {
	BTCONFIG_CONN_GATT						= 0x01,
	BTCONFIG_CONN_SPP					= 0x02
} tBTConfig_ConnType;

extern xSemaphoreHandle bt_config_buf_mutex;

unsigned char read_connection_ack(unsigned char **pSendBuf, unsigned int *sendBufLen);
void bt_read_handle_cmd(unsigned char **pSendBuf, unsigned int *sendBufLen, tBTConfig_ConnType conn_type);
void bt_config_cmdThread_init();
void send_bt_config_cmd(unsigned char *cmd, unsigned int len);

#endif /* __BT_CONFIG_API_H__ */
