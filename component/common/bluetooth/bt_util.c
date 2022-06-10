#include <platform/platform_stdlib.h>
#include "bt_gap.h"
#include "bt_util.h"

#define MAX_CONN_COUNT				(7)

BT_ConnectionStatus connList[MAX_CONN_COUNT] = {0};

u16 big_endian_s(u16 n)
{
#if SYSTEM_ENDIAN == PLATFORM_LITTLE_ENDIAN
	return ((n & 0xff) << 8) | ((n & 0xff00) >> 8);
#else
	return n;
#endif
}

void uuid16_create(tBT_UuidStru *uuid, u16 val)
{
	memset(uuid, 0, sizeof(tBT_UuidStru));
	uuid->len = BT_UUID_LEN_16;
	uuid->u.uuid16 = val;
}

u8 ctoi(char c)
{
	if((c >= 'A') && (c <= 'F')) {
		return (c - 'A' + 0x0A);
	}

	if((c >= 'a') && (c <= 'f')) {
		return (c - 'a' + 0x0A);
	}

	if((c >= '0') && (c <= '9')) {
		return (c - '0' + 0x00);
	}

	return 0xFF;
}

u8 halfitoc(char c)
{
	if((c >= 0x0A) && (c <= 0x0F)) {
		return (c - 0x0A + 'A');
	}

	if((c >= 0) && (c <= 9)) {
		return (c + '0');
	}

	return 0xFF;
}

u8 *bdaddr_to_str(u8 *str, u8 *arr)
{
	int i=0;

	for (i=0; i<BD_ADDR_LEN; i++) {
		str[i * 2] = halfitoc(arr[BD_ADDR_LEN - 1 - i] >> 4);
		str[i * 2 + 1] = halfitoc(arr[BD_ADDR_LEN - 1 - i] & 0xF);
	}
	str[i*2] = '\0';

	return str;
}

u8 hex_str_to_num_array(u32 str_len, s8 *str, u8 *num_arr)
{
	u32 n = 0;
	u8 num = 0;

	if (str_len < 2) {
		return FALSE;
	}
	while (n < str_len) {
		if ((num = ctoi(str[n++])) == 0xFF) {
			return FALSE;
		}
		*num_arr = num << 4;
		if ((num = ctoi(str[n++])) == 0xFF) {
			return FALSE;
		}
		*num_arr |= num;
		num_arr++;
	}
	return TRUE;
}

u8 hex_str_to_bd_addr(u32 str_len, s8 *str, u8 *num_arr)
{
	num_arr += str_len/2 -1;
	u32 n = 0;
	u8 num = 0;

	if (str_len < 2) {
		return FALSE;
	}
	while (n < str_len) {
		if ((num = ctoi(str[n++])) == 0xFF) {
			return FALSE;
		}
		*num_arr = num << 4;
		if ((num = ctoi(str[n++])) == 0xFF) {
			return FALSE;
		}
		*num_arr |= num;
		num_arr--;
	}
	return TRUE;
}

BT_ConnectionStatus* get_conn_status(BT_ConnectionType conn_type, void *connIF_in)
{
	BT_ConnectionStatus* connStatus = NULL;
	for(int i=0; i<MAX_CONN_COUNT; i++) {
		if ((conn_type == CONN_GATT && connList[i].connIF.gatt == (tBT_GattConnIF)connIF_in)
			|| (conn_type == CONN_SPP && connList[i].connIF.spp == (tBT_SppConnIF)connIF_in)) {
			connStatus = &connList[i];
			break;
		}
	}
	if (connStatus == NULL) {
		for(int i=0; i<MAX_CONN_COUNT; i++) {
			if (connList[i].connIF.gatt == NULL && connList[i].connIF.spp == NULL) {
				connStatus = &connList[i];
				if (conn_type == CONN_GATT) {
					connStatus->connIF.gatt = connIF_in;
					connStatus->mtu = 23;
				}
				else if (conn_type == CONN_SPP) {
					connStatus->connIF.spp = connIF_in;
					connStatus->mtu = 512;
				}
				break;
			}
		}
	}
	return connStatus;
}

tBT_ValueStru* malloc_value_stru(tUint16 val_len)
{
	tBT_ValueStru *val;
	val = (tBT_ValueStru *)malloc(sizeof(tBT_ValueStru) + val_len);
	val->len = val_len;
	memset(val->data,0,val->len+1);
	
	return val;
}

u8 get_ad_data(tUint8* eir, tUint8 eir_len, BT_AdvStru* adv_stru) {
	if (eir_len < 2) {
		adv_stru->ad_len = 0;
		adv_stru->ad_type = 0;
		return 0;
	}
	else {
		adv_stru->ad_len = eir[0];
		adv_stru->ad_type = eir[1];
		
		if (eir_len < adv_stru->ad_len + 1) {	// error: data size incorrect
			return 0;
		}
		else {
			if (adv_stru->ad_len != 1) {
				adv_stru->data = (tUint8*)malloc(adv_stru->ad_len); // actual data length: ad_len-1; reserve 1 byte for '\0' of string
				memcpy(adv_stru->data,eir+2,adv_stru->ad_len-1);
				adv_stru->data[adv_stru->ad_len-1] = 0;
			} // else: no data 
			return adv_stru->ad_len + 1;
		}
	}
}

void get_ad_type_str(tUint8 ad_type, char* str) {
	switch(ad_type) {
		case BT_AD_TYPE_FLAGS:
			strcpy(str,"FLAGS");
			break;
		case BT_AD_TYPE_INCOMPLETE_SERVICE_UUID_16:
			strcpy(str,"INCOMPLETE_SERVICE_UUID_16");
			break;
		case BT_AD_TYPE_COMPLETE_SERVICE_UUID_16:
			strcpy(str,"COMPLETE_SERVICE_UUID_16");
			break;
		case BT_AD_TYPE_INCOMPLETE_SERVICE_UUID_32:
			strcpy(str,"INCOMPLETE_SERVICE_UUID_32");
			break;
		case BT_AD_TYPE_COMPLETE_SERVICE_UUID_32:
			strcpy(str,"COMPLETE_SERVICE_UUID_32");
			break;
		case BT_AD_TYPE_INCOMPLETE_SERVICE_UUID_128:
			strcpy(str,"INCOMPLETE_SERVICE_UUID_128");
			break;
		case BT_AD_TYPE_COMPLETE_SERVICE_UUID_128:
			strcpy(str,"COMPLETE_SERVICE_UUID_128");
			break;
		case BT_AD_TYPE_SHORT_LOCAL_NAME:
			strcpy(str,"SHORT_LOCAL_NAME");
			break;
		case BT_AD_TYPE_COMPLETE_LOCAL_NAME:
			strcpy(str,"COMPLETE_LOCAL_NAME");
			break;
		case BT_AD_TYPE_TX_POWER_LEVEL:
			strcpy(str,"TX_POWER_LEVEL");
			break;
		case BT_AD_TYPE_SERVICE_DATA:
			strcpy(str,"BT_AD_TYPE_SERVICE_DATA");
			break;
		case BT_AD_TYPE_APPEARANCE:
			strcpy(str,"APPEARANCE");
			break;
		case BT_AD_TYPE_MANUFACTURER_DATA:
			strcpy(str,"MANUFACTURER_DATA");
			break;
		default:
			sprintf(str,"unknown:0x%02X",ad_type);
			break;
	}
}

void uuid_print(tBT_UuidStru uuid) {
	if (uuid.len == BT_UUID_LEN_16) {
		printf("%04X",uuid.u.uuid16);
	}
	else if (uuid.len == BT_UUID_LEN_32) {
		printf("%08X",uuid.u.uuid32);
	}
	else if (uuid.len == BT_UUID_LEN_128) {
		for(int i=15; i>=0; i--)
			printf("%02X",uuid.u.uuid128[i]);
	}
}

void print_buf(tUint8* data, tUint16 len) {
	for(int i=0; i<len; i++) {
		printf("0x%02X ",data[i]);
	}
}
