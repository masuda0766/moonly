#include "platform_opts.h"
#if CONFIG_BT

#include "FreeRTOS.h"
#include "task.h"
#include <platform/platform_stdlib.h>
#include <lwip_netconf.h>	// htonl, ntohl
#include "wifi_conf.h"
#include "bt_common.h"
#include "bt_gap.h"
#include "bt_gatt.h"
#include "bt_util.h"
#include "bt_config_api.h"

#define BTCONFIG_SVC_UUID         0xFF01
#define BTCONFIG_CHR_UUID         0x2A0D
char bt_connected = 0;
static char init_done = 0;

typedef struct {
	tBT_GattConnIF connIF;
	AttrStru svc;
	AttrStru chr;
	tBT_SPPSvcPro spp;
	tBT_SppSvcIF spp_svc;
	tBT_SppConnIF spp_connIF;
} BTConfigStru;

BTConfigStru bt_config_profile;

static void adv_on_cb(tBT_ResultEnum result)
{
	if ( result == BT_RES_OK) {
		printf("[BT Config] BT Discoverable (iOS)\n\r");
	}
	else {
		printf("\n\r[BT Config] [adv_on_cb] Error, error code (tBT_ResultEnum): %x\n\r",result);
	}
	
}

static void adv_off_cb(tBT_ResultEnum result)
{
	if (!bt_connected) {
		if ( result == BT_RES_OK) {
			printf("[BT Config] BT Undiscoverable (iOS)\n\r");
		}
		else {
			printf("\n\r[BT Config] [adv_off_cb] Error, error code (tBT_ResultEnum): %x\n\r",result);
		}
	}
}

static void discoverable_on_cb(tBT_ResultEnum result)
{
	if ( result == BT_RES_OK) {
		printf("[BT Config] BT Discoverable (Android)\n\r");
	}
	else {
		printf("\n\r[BT Config] [discoverable_on_cb] Error, error code (tBT_ResultEnum): %x\n\r",result);
	}
	
}

static void discoverable_off_cb(tBT_ResultEnum result)
{
	if (!bt_connected) {
		if ( result == BT_RES_OK) {
			printf("[BT Config] BT Undiscoverable (Android)\n\r");
		}
		else {
			printf("\n\r[BT Config] [discoverable_off_cb] Error, error code (tBT_ResultEnum): %x\n\r",result);
		}
	}
}


/*--------------------------------------*/
/*            Event Handlers            */
/*--------------------------------------*/

void conn_complete_hdl(tBT_GattConnCmplStru *param)
{
	if (param->res== BT_RES_OK) {
		printf("\n\r[BT Config] Bluetooth Connection Established (iOS)\n\r");
		bt_config_profile.connIF = param->conn;
		bt_peer_conn_if = param->conn;

		BT_ConnectionStatus* connStatus = NULL;
		connStatus = get_conn_status(CONN_GATT,(void *)param->conn);
		connStatus->tick_Conn = xTaskGetTickCount();

		bt_connected = 1;
	}
	else {
		printf("\n\r[BT Config] Bluetooth Connect Error, error code (tBT_ResultEnum): %x\n\r",param->res);
	}
}

void disconn_complete_hdl(tBT_GattDisconnCmplStru *param)
{
	printf("\n\r[BT Config] Bluetooth Connection Disconnected (iOS)\n\r");

	BT_ConnectionStatus* connStatus = NULL;
	connStatus = get_conn_status(CONN_GATT,(void *)param->conn);
	connStatus->tick_Disconn = xTaskGetTickCount();
	printf("[BT Config] Bluetooth Connection time: %u secs \n\r\n\r", (connStatus->tick_Disconn-connStatus->tick_Conn)/1000);
	if ( wifi_is_ready_to_transceive(RTW_STA_INTERFACE)== RTW_SUCCESS)
		printf("[BT Config] Bluetooth Undiscoverable (wifi connected)\n\r\n\r");

	connStatus->connIF.gatt= NULL;
	bt_config_profile.connIF = NULL;
	bt_peer_conn_if = NULL;
	bt_connected = 0;
}

void mtu_hdl(tBT_GattMtuNotifStru *param)
{
	BT_ConnectionStatus* connStatus = NULL;
	connStatus = get_conn_status(CONN_GATT,(void *)param->conn);
	connStatus->mtu = param->mtu;
}

void config_gatt_read_hdl(tBT_GattsReadIndStru *param)
{
	unsigned char *sendBuf = NULL;
	unsigned int sendBufLen = 0;

	if (read_connection_ack(&sendBuf, &sendBufLen))
	{
		BT_GATTS_ReadRsp(param->conn, BT_GATT_OK, sendBuf, sendBufLen);
	}
	else
	{
		xSemaphoreTake(bt_config_buf_mutex, portMAX_DELAY);
		bt_read_handle_cmd(&sendBuf,&sendBufLen, BTCONFIG_CONN_GATT);		

		BT_GATTS_ReadRsp(param->conn, BT_GATT_OK, sendBuf, sendBufLen);
		xSemaphoreGive(bt_config_buf_mutex);
	}
}

void config_gatt_write_hdl(tBT_GattsWriteIndStru *param)
{
	send_bt_config_cmd(param->val,param->val_len);
	BT_GATTS_WriteRsp(param->conn, BT_GATT_OK);
}

void bt_config_cb(tBT_GattEventEnum ev, void *param)
{
	switch (ev) {
		case BT_GATT_EV_CONNECT_COMPLETE: {			/* tBT_GattConnCmplStru */
			conn_complete_hdl((tBT_GattConnCmplStru *)param);
			break;
		}
		case BT_GATT_EV_DISCONNECT_COMPLETE: {		/* tBT_GattDisconnCmplStru */
			disconn_complete_hdl((tBT_GattDisconnCmplStru *)param);
			break;
		}
		case BT_GATT_EV_MTU_NOTIFICATION: {			/* tBT_GattMtuNotifStru */
			mtu_hdl((tBT_GattMtuNotifStru *)param);
			break;								
		}
		case BT_GATTS_EV_SERVICE_START: {			/* tBT_GattSvcStartStru */
			tBT_GattSvcStartStru *in = param;
			if (bt_config_profile.svc.attr_if == in->svc) {
				bt_config_profile.svc.attr_hdl = in->attr_hdl;
			}
			break;
		}
		case BT_GATTS_EV_CHARACTERISTIC_START: {	/* tBT_GattCharStartStru */
			tBT_GattCharStartStru *in = param;
			if (bt_config_profile.chr.attr_if == in->chr) {
				bt_config_profile.chr.attr_hdl = in->attr_hdl;
			}
			init_done += 1;
			break;
		}
		case BT_GATTS_EV_READ: {					/* tBT_GattsReadIndStru */
			config_gatt_read_hdl((tBT_GattsReadIndStru *)param);
			break;
		}
		case BT_GATTS_EV_WRITE_REQ: {				/* tBT_GattsWriteIndStru */
			config_gatt_write_hdl((tBT_GattsWriteIndStru *)param);
			break;
		}
		default:
			printf("[BT Config] [bt_config_cb] Unhandled event: %x\n\r",ev);
			break;

	}
	
}

void conn_ind_android(tBT_SppConnInd *param)
{
	bt_config_profile.spp_connIF = param->conn;
	
	BT_SPP_ConnectRsp(bt_config_profile.spp_connIF, BT_SPP_RET_SUCCESS);
}

void conn_complete_android(tBT_SppConnInd *param)
{
	if (param->result == BT_SPP_RET_SUCCESS) {
		printf("\n\r[BT Config] Bluetooth Connection Established (Android)\n\r");
		bt_config_profile.spp_connIF = param->conn;
		spp_conn_if = param->conn;		

		BT_ConnectionStatus* connStatus = NULL;
		connStatus = get_conn_status(CONN_SPP,(void *)param->conn);
		connStatus->tick_Conn = xTaskGetTickCount();

		bt_connected = 1;
	}
	else {
		printf("\n\r[BT Config] Connect Error, error code (tBT_SppResult): %x\n\r",param->result);
	}
}

void disconn_complete_android(tBT_SppConnInd *param)
{
	printf("\n\r[BT Config] Bluetooth Connection Disconnected (Android)\n\r");

	BT_ConnectionStatus* connStatus = NULL;
	connStatus = get_conn_status(CONN_SPP,(void *)param->conn);
	connStatus->tick_Disconn = xTaskGetTickCount();
	printf("[BT Config] Bluetooth Connection time: %u secs \n\r\n\r", (connStatus->tick_Disconn-connStatus->tick_Conn)/1000);
	if ( wifi_is_ready_to_transceive(RTW_STA_INTERFACE)== RTW_SUCCESS)
		printf("[BT Config] Bluetooth Undiscoverable (wifi connected)\n\r\n\r");

	connStatus->connIF.spp= NULL;
	bt_config_profile.spp_connIF = NULL;
	spp_conn_if = NULL;		
	bt_connected = 0;
}

void data_ind_android(tBT_SppDataInd *param)
{
	BT_ConnectionStatus* connStatus = NULL;
	connStatus = get_conn_status(CONN_SPP,(void *)param->conn);
	unsigned char *sendBuf = NULL;
	unsigned int sendBufLen = 0;
	unsigned int dataRemain;
	unsigned int sendLen;
	unsigned int seg = 0;
	unsigned char* pSendBuf = NULL;

	send_bt_config_cmd(param->data,param->length);

	if (read_connection_ack(&sendBuf, &sendBufLen))
	{
		dataRemain = sendBufLen;
		pSendBuf = sendBuf;

		while(dataRemain > 0) {
			if (dataRemain > connStatus->mtu)
				sendLen = connStatus->mtu;
			else
				sendLen = dataRemain;

			BT_SPP_SendData(param->conn, pSendBuf,sendLen);
			
			dataRemain -= sendLen;
			seg++;
			pSendBuf += sendLen;
		}
	}
	else
	{
		xSemaphoreTake(bt_config_buf_mutex, portMAX_DELAY);
		bt_read_handle_cmd(&sendBuf,&sendBufLen, BTCONFIG_CONN_SPP);

		dataRemain = sendBufLen;
		pSendBuf = sendBuf;

		while(dataRemain > 0) {
			if (dataRemain > connStatus->mtu)
				sendLen = connStatus->mtu;
			else
				sendLen = dataRemain;

			BT_SPP_SendData(param->conn, pSendBuf,sendLen);
			
			dataRemain -= sendLen;
			seg++;
			pSendBuf += sendLen;
		}

		xSemaphoreGive(bt_config_buf_mutex);
	}

}


void bt_config_android_cb(tBT_SppEventEnum ev, void *param)
{
	switch (ev) {
		case BT_SPP_EV_SERVICE_START:
			init_done += 1;
			break;
		case BT_SPP_EV_SERVICE_STOP:
			break;
		case BT_SPP_EV_CONNECT_IND:
			conn_ind_android((tBT_SppConnInd *)param);
			break;
		case BT_SPP_EV_CONNECT_COMPLETE:
			conn_complete_android((tBT_SppConnInd *)param);
			break;
		case BT_SPP_EV_DISCONNECT_COMPLETE:
			disconn_complete_android((tBT_SppConnInd *)param);
			break;
		case BT_SPP_EV_DATA_IND:
			data_ind_android((tBT_SppDataInd *)param);
			break;
		default:
			printf("[BT Config] [bt_config_android_cb] Unhandled event: %x\n\r",ev);
			break;
	}
}


void register_bt_config(void)
{
	/* GATT (for iOS) */
	BT_GATT_DefaultMtuSet(512);
	tBT_UuidStru uuid;
	uuid16_create(&uuid,BTCONFIG_SVC_UUID);
	bt_config_profile.svc.attr_if = BT_GATTS_ServiceCreate(&uuid, bt_config_cb, FALSE);

	uuid16_create(&uuid,BTCONFIG_CHR_UUID);
	bt_config_profile.chr.attr_if = BT_GATTS_CharacteristicAdd(bt_config_profile.svc.attr_if, &uuid, (BT_GATT_PROP_READ | BT_GATT_PROP_WRITE_REQ), 0);

	BT_GATT_CallbackRegister(bt_config_cb);
	BT_GATTS_ServiceStart(bt_config_profile.svc.attr_if);
	
	/* SPP (for Android) */
	memset(&bt_config_profile.spp,0,sizeof(bt_config_profile.spp));
	bt_config_profile.spp.mfs = 512;	// MTU
	bt_config_profile.spp.init_credit = 7;
	
	bt_config_profile.spp_svc = BT_SPP_ServiceCreate(&bt_config_profile.spp,bt_config_android_cb);
	BT_SPP_ServiceStart(bt_config_profile.spp_svc);
	
}

static void set_adv_data()
{	
	tChar name[DEV_NAME_LEN];
	tUint8 bd[BD_ADDR_LEN] = {0};
	tUChar bd_str[BD_ADDR_LEN * 2 + 1] = {0};

	BT_GAP_LocalBDGet(bd,NULL);
	bdaddr_to_str(bd_str, bd);
	strcpy(name,"Ameba_");
	int idx = strlen(name);
	memcpy(name+idx,&(bd_str[6]),7);

	tBT_EirDataStru adv_data;
	memset(&adv_data,0,sizeof(tBT_EirDataStru));
	strcpy(adv_data.dev_name,name);
	adv_data.mask = BT_GAP_BLE_ADVDATA_MBIT_NAME;

	tBT_GapLEAdvParamStru param = {
		320, /* tUint16 int_min */
		400, /* tUint16 int_max */
		{0}, /* tBT_AddressStru peer_addr */
		BT_GAP_BLE_ADV_TYPE_UNDIRECT, /* tUint8 type */
		BT_GAP_BLE_OWN_ADDRESS_TYPE_RANDOM, /* tUint8 own_address_type */
		BT_GAP_BLE_ADV_CHNMAP_CHALL, /* tUint8 chn_map */
		BT_GAP_BLE_ADV_FILTER_ALL, /* tUint8 filter */
	};

	BT_GAP_BleAdvertisingDataUpdate(&adv_data);
	BT_GAP_BleScanResponseDataUpdate(&adv_data);
	BT_GAP_BleAdvertisingParamsSet(&param);
	/* BLE ADV MUST stop before restart */
	BT_GAP_BleAdvertisingSet(FALSE, NULL);
	BT_GAP_BleAdvertisingSet(TRUE, adv_on_cb);

	BT_GAP_BrScanEnableSet(BT_GAP_BR_NO_SCAN_ENABLE, NULL);
	BT_GAP_BrScanEnableSet(BT_GAP_BR_INQUIRY_SCAN_ENABLE_PAGE_SCAN_ENABLE, discoverable_on_cb);
	BT_GAP_BrDeviceNameSet(name, NULL);
	BT_GAP_BrWriteEIRData(&adv_data, NULL);
}

void at_gap_ev_cb(tBT_GapEvEnum ev, void *param);
void init_done_cb(tBT_ResultEnum result)
{
	if (result == BT_RES_OK) {
		printf("\n\r[BT Config] BT Initialized\n\r");
		printf("[BT Config] Register BT Config service\n\r");

		BT_GAP_PairingFeatureSet(BT_GAP_IOCAP_NO_INPUT_NO_OUTPUT, BT_GAP_SECU_REQ_BOND);
		BT_GAP_CallbackReg(at_gap_ev_cb);
		BT_GAP_BrInquiryScanActivitySet(1024, 18, NULL);
		BT_GAP_BrPageScanActivitySet(1024, 18, NULL);
		BT_GAP_BrAuthEnable(0, NULL);
		BT_GAP_BrSSPSet(1, NULL);
		BT_GAP_BrClassOfDevice(0x001F00, NULL);
	
		register_bt_config();
		set_adv_data();
	}
	else {
		printf("\n\r[BT Config] [init_done_cb] Error, error code (tBT_ResultEnum): %x\n\r",result);
	}
}

void bt_config_task(void)
{
	printf("\n\r[BT Config] Initializing BT ...\n\r");
	BT_Init(init_done_cb);
        
	while (init_done != 2) {
		// wait until bt is initialized & btconfig service (Android & iOS) is running
		rtw_msleep_os(1000);
	}

	bt_config_cmdThread_init();
	printf("\n\r[BT Config] BT Config ready\n\r");
	
	bool monitor_wifi_bt_status = 1;
	while (monitor_wifi_bt_status)
	{
		rtw_msleep_os(1000);
		if ( bt_connected ||( wifi_is_ready_to_transceive(RTW_STA_INTERFACE) == RTW_SUCCESS)) {	// BT or WIFI connected
			if (BT_GAP_BleAdvertisingGet()== TRUE)	// BLE ADV ON -> turn off
				BT_GAP_BleAdvertisingSet(FALSE, adv_off_cb);
			if (BT_GAP_BrScanEnableGet() == BT_GAP_BR_INQUIRY_SCAN_ENABLE_PAGE_SCAN_ENABLE)	// BR/EDR discoverable on -> turn off 
				BT_GAP_BrScanEnableSet(BT_GAP_BR_NO_SCAN_ENABLE, discoverable_off_cb);
		}
		else {	// BT and WIFI not connected
			if (BT_GAP_BleAdvertisingGet()== FALSE)	// BLE ADV OFF -> turn on
				BT_GAP_BleAdvertisingSet(TRUE, adv_on_cb);
			if (BT_GAP_BrScanEnableGet() == BT_GAP_BR_NO_SCAN_ENABLE)	// BR/EDR discoverable off -> turn on 
				BT_GAP_BrScanEnableSet(BT_GAP_BR_INQUIRY_SCAN_ENABLE_PAGE_SCAN_ENABLE, discoverable_on_cb);
		}
	}
	
	vTaskDelete(NULL);
}

void example_bt_config(void){
	if(xTaskCreate((TaskFunction_t)bt_config_task, (char const *)"bt_config_task", 512, NULL, tskIDLE_PRIORITY + 1, NULL) != pdPASS){
		printf("\n\r[%s] Create update task failed", __FUNCTION__);
	}
}

#endif /* CONFIG_BT */
