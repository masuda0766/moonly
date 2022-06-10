#include "platform_opts.h"
#if CONFIG_BT

#include "FreeRTOS.h"
#include "task.h"
#include <platform/platform_stdlib.h>
#include "bt_common.h"
#include "bt_gap.h"
#include "bt_util.h"

typedef enum {
	BEACON_TYPE_IBEACON = 1,
	BEACON_TYPE_ALTBEACON
} BT_BeaconTypeEnum;

#define BEACON_TYPE BEACON_TYPE_IBEACON

static void bt_beacon_on_cb(tBT_ResultEnum result)
{
	if ( result == BT_RES_OK) {
		if (BEACON_TYPE == BEACON_TYPE_IBEACON) {
			printf("[iBeacon] Start broadcasting iBeacon\r\n");
		}
		else if (BEACON_TYPE == BEACON_TYPE_ALTBEACON) {
			printf("[AltBeacon] Start broadcasting AltBeacon\r\n");
		}
	}
	else {
		printf("\r\n[example_bt_beacon] [bt_beacon_on_cb] Error, error code (tBT_ResultEnum): %d\r\n",result);
	}
}

void bt_iBeacon(void)
{
	tBT_EirDataStru adv_data;
	tBT_ManufacturerDataStru *p_manu;
	BT_iBeaconData beacon_data;
	u16 major, minor;
	char uuidHexStr[32] = "00112233445566778899aabbccddeeff";

	major = 0xAA;
	minor = 0xBB;

	p_manu = &(adv_data.manufacturer_data);
	memset(&adv_data, 0, sizeof(tBT_EirDataStru));
	
	adv_data.mask = BT_GAP_BLE_ADVDATA_MBIT_MENUDATA;

	p_manu->company_id[0] = 0x4C; // Apple = 0x004C
	p_manu->company_id[1] = 0x00;
	p_manu->data_len = sizeof(beacon_data);
	beacon_data.beacon_code = 0x02;		// iBeacon identifier
	beacon_data.length = 0x15;		// iBeacon data length 0x15 (21) = UUID (16) + major (2) + minor (2) + RSSI (1) 
	hex_str_to_num_array(strlen(uuidHexStr), uuidHexStr, beacon_data.uuid);
	beacon_data.major = big_endian_s(major);
	beacon_data.minor = big_endian_s(minor);
	beacon_data.rssi = -65;
	
	p_manu->data = (tUint8*) (&beacon_data);


	tBT_GapLEAdvParamStru param = {
		160, /* tUint16 int_min, 160->100 ms, Time = N * 0.625 ms, Time Range: 20 ms to 10.24 s */
		160, /* tUint16 int_max */
		{0}, /* tBT_AddressStru peer_addr */
		BT_GAP_BLE_ADV_TYPE_NONCONN, /* tUint8 type */
		BT_GAP_BLE_OWN_ADDRESS_TYPE_RANDOM, /* MUST BE RANDOM */
		BT_GAP_BLE_ADV_CHNMAP_CHALL, /* tUint8 chn_map */
		BT_GAP_BLE_ADV_FILTER_ALL, /* tUint8 filter */
	};

	printf("[iBeacon] Current Setting: major=%d, minor=%d, tx_power:%d, UUID=",major, minor, beacon_data.rssi);
	for (int i = 0; i < 16; i++) {
		printf("%x%x", beacon_data.uuid[i]>>4, beacon_data.uuid[i]&0xF);
	}
	printf("\r\n");

	BT_GAP_BleAdvertisingDataUpdate(&adv_data);
	BT_GAP_BleAdvertisingParamsSet(&param);
	/* MUST stop before restart */
	BT_GAP_BleAdvertisingSet(FALSE, NULL);
	BT_GAP_BleAdvertisingSet(TRUE, bt_beacon_on_cb);

}

void bt_altBeacon(void)
{
	tBT_EirDataStru adv_data;
	tBT_ManufacturerDataStru *p_manu;
	BT_AltBeaconData beacon_data;
	unsigned short major, minor;
	char uuidHexStr[32] = "00112233445566778899aabbccddeeff";

	major = 123;
	minor = 456;

	p_manu = &(adv_data.manufacturer_data);
	memset(&adv_data, 0, sizeof(tBT_EirDataStru));
	
	adv_data.mask = BT_GAP_BLE_ADVDATA_MBIT_MENUDATA;

	p_manu->company_id[0] = 0x5D; // Realtek = 0x005D
	p_manu->company_id[1] = 0x00;
	p_manu->data_len = sizeof(beacon_data);	// 24  (0x1B)
	beacon_data.beacon_code = big_endian_s(0xBEAC);		// AltBeacon identifier
	hex_str_to_num_array(strlen(uuidHexStr), uuidHexStr, beacon_data.uuid);
	beacon_data.major = big_endian_s(major);
	beacon_data.minor = big_endian_s(minor);
	beacon_data.rssi = -65;
	beacon_data.mfg_rsvd = 0xCD;	// Can be any value
	
	p_manu->data = (tUint8*) (&beacon_data);
	

	tBT_GapLEAdvParamStru param = {
		160, /* tUint16 int_min, 160->100 ms, Time = N * 0.625 ms, Time Range: 20 ms to 10.24 s */
		160, /* tUint16 int_max */
		{0}, /* tBT_AddressStru peer_addr */
		BT_GAP_BLE_ADV_TYPE_NONCONN, /* tUint8 type */
		BT_GAP_BLE_OWN_ADDRESS_TYPE_RANDOM, /* MUST BE RANDOM */
		BT_GAP_BLE_ADV_CHNMAP_CHALL, /* tUint8 chn_map */
		BT_GAP_BLE_ADV_FILTER_ALL, /* tUint8 filter */
	};

	printf("[AltBeacon] Current Setting: major=%d, minor=%d, tx_power:%d, UUID=",major, minor, beacon_data.rssi);
	for (int i = 0; i < 16; i++) {
		printf("%x%x", beacon_data.uuid[i]>>4, beacon_data.uuid[i]&0xF);
	}
	printf("\r\n");

	BT_GAP_BleAdvertisingDataUpdate(&adv_data);
	BT_GAP_BleAdvertisingParamsSet(&param);
	/* MUST stop before restart */
	BT_GAP_BleAdvertisingSet(FALSE, NULL);
	BT_GAP_BleAdvertisingSet(TRUE, bt_beacon_on_cb);
}

void at_gap_ev_cb(tBT_GapEvEnum ev, void *param);
void beacon_init_done_cb(tBT_ResultEnum result)
{
	if (result == BT_RES_OK) {
		BT_GAP_CallbackReg(at_gap_ev_cb);
		printf("\r\n[example_bt_beacon] BT initialized\r\n");
		if (BEACON_TYPE == BEACON_TYPE_IBEACON)
			bt_iBeacon();
		else if (BEACON_TYPE == BEACON_TYPE_ALTBEACON)
			bt_altBeacon();
	}
	else {
		printf("\r\n[example_bt_beacon] [beacon_init_done_cb] Error, error code (tBT_ResultEnum): %d\r\n",result);
	}
}

void bt_beacon_task(void)
{
	printf("\r\n[example_bt_beacon] Initializing BT ...\r\n");
	BT_Init(beacon_init_done_cb);
	
	vTaskDelete(NULL);
}

void example_bt_beacon(void){
	if(xTaskCreate((TaskFunction_t)bt_beacon_task, (char const *)"bt_beacon_task", 512, NULL, tskIDLE_PRIORITY + 1, NULL) != pdPASS){
		printf("\n\r[%s] Create bt beacon task failed", __FUNCTION__);
	}
}

#endif /* CONFIG_BT */
