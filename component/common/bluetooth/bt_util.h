#ifndef __BT_UTIL_H__
#define __BT_UTIL_H__

#include "FreeRTOS.h"
#include <basic_types.h>
#include "bt_gatt.h"
#include "bt_spp.h"

extern tBT_GattConnIF	bt_peer_conn_if;
extern tBT_SppConnIF	spp_conn_if;
extern tBT_GattConnIF	gatt_initiator_conn;
extern tBT_SppConnIF	spp_client_conn;
extern tBT_AddressStru	bt_paring_dev;

#define BD_ADDR_LEN							6
#define DEV_NAME_LEN						31
#define BD_ADDR_FMT "%02x:%02x:%02x:%02x:%02x:%02x"
#define BD_ADDR_ARG(x) ((u8*)(x))[5],((u8*)(x))[4],((u8*)(x))[3],((u8*)(x))[2],((u8*)(x))[1],((u8*)(x))[0]

#define EXT_PROP_LEN				2
#define CLI_CONF_LEN				2
#define SVR_CONF_LEN				2
#define CHR_FMT_LEN					7

typedef enum {
	WRITE_CMD = 1,	/* Write Command */
	WRITE_REQ,		/* Write Request */
	PREP_WRITE		/* Prepare Write */
} BT_GattWriteEnum;

typedef enum {
	BT_AD_TYPE_FLAGS						= 0x01,
	BT_AD_TYPE_INCOMPLETE_SERVICE_UUID_16	= 0x02,
	BT_AD_TYPE_COMPLETE_SERVICE_UUID_16		= 0x03,
	BT_AD_TYPE_INCOMPLETE_SERVICE_UUID_32	= 0x04,
	BT_AD_TYPE_COMPLETE_SERVICE_UUID_32		= 0x05,
	BT_AD_TYPE_INCOMPLETE_SERVICE_UUID_128	= 0x06,
	BT_AD_TYPE_COMPLETE_SERVICE_UUID_128	= 0x07,
	BT_AD_TYPE_SHORT_LOCAL_NAME				= 0x08,
	BT_AD_TYPE_COMPLETE_LOCAL_NAME			= 0x09,
	BT_AD_TYPE_TX_POWER_LEVEL				= 0x0A,
	BT_AD_TYPE_SERVICE_DATA					= 0x16,
	BT_AD_TYPE_APPEARANCE					= 0x19,
	BT_AD_TYPE_MANUFACTURER_DATA			= 0xFF,
} BT_AdTypeEnum;

typedef struct {
	 void *attr_if;
	 tUint16 attr_hdl;
	 tBT_ValueStru *val;
 } AttrStru;

typedef enum _connection_type {
	CONN_GATT					= 0x01,
	CONN_SPP					= 0x02
} BT_ConnectionType;

typedef struct {
	BT_ConnectionType conn_type;
	union {
		tBT_GattConnIF	gatt;
		tBT_SppConnIF spp;
	} connIF;
	tUint16	mtu;
	TickType_t tick_Conn;
	TickType_t tick_Disconn;
} BT_ConnectionStatus;

typedef struct {
	u8	ad_len;
	u8	ad_type;
	u8  *data;
} BT_AdvStru;

typedef struct {
	u16	beacon_code;		// big endian 0xBEAC
	u8	uuid[16];
	u16	major;
	u16	minor;
	s8	rssi;
	u8	mfg_rsvd;
} BT_AltBeaconData;

RTW_PACK_STRUCT_BEGIN
typedef struct {
	u8	beacon_code;		// 0x02
	u8	length;				// 0x15 = 21 = uuid + major + minor + rssi
	u8	uuid[16];
	u16	major;
	u16	minor;
	s8	rssi;
} RTW_PACK_STRUCT_STRUCT
BT_iBeaconData;
RTW_PACK_STRUCT_END

void uuid16_create(tBT_UuidStru *uuid, u16 val);
u16 big_endian_s(u16 n);
u8 hex_str_to_num_array(u32 str_len, s8 *str, u8 *num_arr);
BT_ConnectionStatus* get_conn_status(BT_ConnectionType conn_type, void *connIF);
tBT_ValueStru* malloc_value_stru(tUint16 val_len);
u8 get_ad_data(tUint8* eir, tUint8 eir_len, BT_AdvStru* adv_stru);
void get_ad_type_str(tUint8 ad_type, char* str);
void uuid_print(tBT_UuidStru uuid);
void print_buf(tUint8* data, tUint16 len);

#endif /* __BT_UTIL_H__ */
