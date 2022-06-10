#include "platform_opts.h"
#if CONFIG_BT

#include <platform/platform_stdlib.h>
#include "FreeRTOS.h"
#include "task.h"
#include "bt_common.h"
#include "bt_gap.h"
#include "bt_gatt.h"
#include "bt_util.h"

tBT_GattTaskInfoStru gatt_client;

/*--------------------------------------*/
/*            Event Handlers            */
/*--------------------------------------*/

typedef struct context {
	tBT_UuidStru uuid;
	tUint16 att_hdl;
} Context;

Context* create_context(tBT_UuidStru uuid_IN, tUint16 attr_hdl_IN) {
	Context* nContext = (Context*)malloc(sizeof(Context));
	nContext->uuid = uuid_IN;
	nContext->att_hdl = attr_hdl_IN;
	return nContext;
}

void print_context(Context* context) {
	if (context != NULL) {
		printf(" (context: UUID=");
		uuid_print(context->uuid);
		printf(", handle=0x%04x)\n\r",context->att_hdl);
	}
	else
		printf("\n\r");
}

void svc_discovered(tBT_GattEventEnum ev,tBT_GattcSvcDiscoveredStru *in) {
	if (ev==BT_GATTC_EV_SERVICE_DISCOVERED)
		printf("[Service] ");
	else if (ev==BT_GATTC_EV_INCLUDE_SERVICE_FOUND)
		printf("[Included Service] ");

	printf("UUID=");
	uuid_print(in->uuid);
	printf(", handle=0x%04x",in->att_hdl);
	print_context((Context *) in->context);
	
	//printf("[Action] Find Included Service...\n\r");
	gatt_client.context = (void*) create_context(in->uuid,in->att_hdl);
	BT_GATTC_IncludeSvcFind(&gatt_client, in->svc);
	
	//printf("[Action] Do Characteristic Discovery...\n\r");
	gatt_client.context = (void*) create_context(in->uuid,in->att_hdl);
	BT_GATTC_ChrDiscover(&gatt_client, in->svc, NULL);
}

void char_discovered(tBT_GattcChrDiscoveredStru *in) {
	printf("[Characteristic] UUID=");
	uuid_print(in->uuid);
	printf(", handle=0x%04x, properties=0x%02x",in->att_hdl,in->props);
	print_context((Context *) in->context);
	
	//printf("[Action] Do Descriptor Discovery...\n\r");
	gatt_client.context = (void*) create_context(in->uuid,in->att_hdl);
	BT_GATTC_AllDesDiscover(&gatt_client, in->chr);
			
}

void desc_discovered(tBT_GattcDesDiscoveredStru* in) {
	printf("[Descriptor] UUID=");
	uuid_print(in->uuid);
	printf(", handle=0x%04x",in->att_hdl);
	print_context((Context *) in->context);
}

void read_rsp(tBT_GattcReadRspStru *in) {
	printf("[Read Response] handle=0x%04x, len=%d, offset=%d, ",in->att_hdl, in->val_len, in->val_off);			
	tUint8* data = (tUint8*)malloc(in->val_len+1);
	memset(data+in->val_len,0,1);
	memcpy(data,in->val,in->val_len);
	printf("data=%s ( ",data);
	print_buf(data,in->val_len);
	printf(")");
	free(data);
	print_context((Context *) in->context);
}
void ev_complete(tBT_GattEventEnum ev, tBT_GattTaskCmplStru * in) {
	if (ev == BT_GATTC_EV_SERVICE_DISCOVERY_CMPL)
		printf("[Service] Discovery Complete");
	else if (ev == BT_GATTC_EV_INCLUDE_SERVICE_FIND_CMPL)
		printf("[Included Service] Discovery Complete");
	else if (ev == BT_GATTC_EV_CHARACTERISTIC_DISCOVERY_CMPL)
		printf("[Characteristic] Discovery Complete");
	else if (ev == BT_GATTC_EV_DESCRIPTOR_DISCOVERY_CMPL)
		printf("[Descriptor] Discovery Complete");
	else if (ev == BT_GATTC_EV_READ_CMPL)
		printf("[Read] handle=0x%04x Complete",in->att_hdl);
	else if (ev == BT_GATTC_EV_WRITE_CMPL)
		printf("[Write] handle=0x%04x Complete",in->att_hdl);
	
	if (in->res != BT_GATT_OK)
		printf(" [ERROR: 0x%02x]",in->res);
	
	print_context((Context *) in->context);

	if (in->context != NULL)
		free((Context *) in->context);	
}

void notification_indication(tBT_GattEventEnum ev, tBT_GattcHdlValIndStru * in) {
	if (ev==BT_GATTC_EV_HANDLE_VALUE_NOTIFICATION)
		printf("[Handle Value Notification] ");
	else if (ev==BT_GATTC_EV_HANDLE_VALUE_INDICATION) {
		BT_GATTC_HdlValCfm(in->conn);
		printf("[Handle Value Indication] ");	
	}
	
	printf("handle=0x%04x, ",in->att_hdl);
	tUint8* data = (tUint8*)malloc(in->val_len+1);
	memset(data+in->val_len,0,1);
	memcpy(data,in->val,in->val_len);
	printf("data=%s ( ",data);
	print_buf(data,in->val_len);
	printf(")\n\r");
	free(data);
}


static void gatt_client_cb(tBT_GattEventEnum ev, void *param)
{
	switch (ev) {	
		case BT_GATTC_EV_SERVICE_DISCOVERED:
		case BT_GATTC_EV_INCLUDE_SERVICE_FOUND:
			svc_discovered(ev,(tBT_GattcSvcDiscoveredStru *)param);
			break;
		case BT_GATTC_EV_CHARACTERISTIC_DISCOVERED:
			char_discovered((tBT_GattcChrDiscoveredStru *)param);
			break;
		case BT_GATTC_EV_DESCRIPTOR_DISCOVERED:
			desc_discovered((tBT_GattcDesDiscoveredStru *)param);
			break;
		case BT_GATTC_EV_READ_RSP:
			read_rsp((tBT_GattcReadRspStru *)param);
			break;
		case BT_GATTC_EV_HANDLE_VALUE_INDICATION:	
		case BT_GATTC_EV_HANDLE_VALUE_NOTIFICATION:
			notification_indication(ev,(tBT_GattcHdlValIndStru *)param);
			break;
		case BT_GATTC_EV_SERVICE_DISCOVERY_CMPL:
		case BT_GATTC_EV_INCLUDE_SERVICE_FIND_CMPL:
		case BT_GATTC_EV_CHARACTERISTIC_DISCOVERY_CMPL:
		case BT_GATTC_EV_DESCRIPTOR_DISCOVERY_CMPL:
		case BT_GATTC_EV_READ_CMPL:
		case BT_GATTC_EV_WRITE_CMPL:
			ev_complete(ev,(tBT_GattTaskCmplStru *)param);
			break;
		default:
			printf("[Warning] [example_bt_gatt_client] [gatt_client_cb] Unhandled event: %x\n\r",ev);
			break;
	}
}

static void cli_conn_complete_cb(tBT_GattConnCmplStru *param)
{
	if (param->res== BT_RES_OK) {
		printf("\n\r[example_bt_gatt_client] Bluetooth Connection Established (GATT Client)\n\r");

		BT_ConnectionStatus* connStatus = NULL;
		connStatus = get_conn_status(CONN_GATT,(void *)param->conn);
		connStatus->tick_Conn = xTaskGetTickCount();
		bt_peer_conn_if = param->conn;
		gatt_client.conn = param->conn;
		gatt_client.context = NULL;
		gatt_client.cfm_cbk = gatt_client_cb;
		BT_GATTC_RegisterEvCallback(param->conn, gatt_client_cb);

		//printf("[Action] Do Primary Service Discovery...\n\r");
		BT_GATTC_PrimarySvcDiscover(&gatt_client, NULL);	
	}
	else {
		printf("[Error] [example_bt_gatt_client] Connect Error, error code (tBT_ResultEnum): %x\n\r",param->res);
	}
}

static void cli_disconn_complete_cb(tBT_GattDisconnCmplStru *param)
{
	printf("\n\r[example_bt_gatt_client] Bluetooth Connection Disconnected (GATT Client)\n\r");			
	BT_ConnectionStatus* connStatus = NULL;
	connStatus = get_conn_status(CONN_GATT,(void *)param->conn);
	connStatus->tick_Disconn = xTaskGetTickCount();
	printf("[example_bt_gatt_client] Bluetooth Connection time: %u secs\n\r", (connStatus->tick_Disconn-connStatus->tick_Conn)/1000);

	connStatus->connIF.gatt= NULL;
	bt_peer_conn_if = NULL;
}

static void cli_mtu_cb(tBT_GattMtuNotifStru *param)
{
	BT_ConnectionStatus* connStatus = NULL;
	connStatus = get_conn_status(CONN_GATT,(void *)param->conn);
	connStatus->mtu = param->mtu;
	
	printf("\n\r[example_bt_gatt_client] MTU: %d\n\r",param->mtu);
}

static void gatt_general_cb(tBT_GattEventEnum ev, void *param)
{
	switch (ev) {
		case BT_GATT_EV_CONNECT_COMPLETE:					/* 0x01 tBT_GattConnCmplStru */
			cli_conn_complete_cb((tBT_GattConnCmplStru *)param);
			break;
		case BT_GATT_EV_DISCONNECT_COMPLETE:				/* 0x02 tBT_GattDisconnCmplStru */
			cli_disconn_complete_cb((tBT_GattDisconnCmplStru *)param);
			break;
		case BT_GATT_EV_MTU_NOTIFICATION:					/* 0x03 tBT_GattMtuNotifStru */
			cli_mtu_cb((tBT_GattMtuNotifStru *)param);
			break;			
		case BT_GATT_EV_READ_RSSI_CFM:
			printf("\n\r[example_bt_gatt_client] RSSI of connected device: %d\n\r",((tBT_GattReadRssiInd *)param)->rssi);
			break;
		case BT_GATT_EV_CONN_INTERVAL_UADATE:
			printf("\r\n[example_bt_gatt_client] [BT_GATT_EV_CONN_INTERVAL_UADATE] addr_type = %s, peer_addr = "BD_ADDR_FMT"\n\r", (((tBT_GattUpdateStru *)param)->addr.type == BT_ADDR_TYPE_PUBLIC)? "public":"random",BD_ADDR_ARG(((tBT_GattUpdateStru *)param)->addr.bd));			
			printf("[example_bt_gatt_client] [BT_GATT_EV_CONN_INTERVAL_UADATE] conn_interval of connected device: %d\n\r", ((tBT_GattUpdateStru *)param)->conn_interval);
			break;
		default:
			printf("[Warning] [example_bt_gatt_client] [gatt_general_cb] Unhandled event: %x\n\r",ev);
			break;
	}
}

void at_gap_ev_cb(tBT_GapEvEnum ev, void *param);
static void cli_init_done_cb(tBT_ResultEnum result)
{
	if (result == BT_RES_OK) {
		printf("\n\r[example_bt_gatt_client] BT initialized\n\r");

		BT_GAP_PairingFeatureSet(BT_GAP_IOCAP_KEYBOARD_DISPLAY, BT_GAP_SECU_REQ_BOND);
		BT_GAP_CallbackReg(at_gap_ev_cb);
		BT_GATT_CallbackRegister(gatt_general_cb);
		BT_GATT_DefaultMtuSet(100);
		printf("[example_bt_gatt_client] Use ATBC to connect\n\r");
	}
	else {
		printf("\n\r[example_bt_gatt_client] [init_done_cb] Error, error code (tBT_ResultEnum): %x\n\r",result);
	}
}

static void bt_gatt_client_task(void)
{
	printf("\n\r[example_bt_gatt_client] Initializing BT ...\n\r");
	BT_Init(cli_init_done_cb);
	
	vTaskDelete(NULL);
}

void example_bt_gatt_client(void){
	if(xTaskCreate((TaskFunction_t)bt_gatt_client_task, (char const *)"bt_gatt_client_task", 2048, NULL, tskIDLE_PRIORITY + 1, NULL) != pdPASS){
		printf("\n\r[%s] Create gatt client task failed", __FUNCTION__);
	}
}

#endif /* CONFIG_BT */

