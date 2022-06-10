#include "platform_opts.h"
#if CONFIG_BT

#include "FreeRTOS.h"
#include "task.h"
#include <platform/platform_stdlib.h>
#include <lwip_netconf.h>	// htonl, ntohl
#include "wifi_conf.h"
#include "bt_common.h"
#include "bt_gap.h"
#include "bt_util.h"

typedef struct {
	tBT_SPPSvcPro param;
	tBT_SppSvcIF spp_svcIF;
	tBT_SppConnIF spp_connIF;
	uint16_t conn_mfs;
} SppStru;

SppStru spp_svc;


static void spp_discoverable_on_cb(tBT_ResultEnum result)
{
	if ( result == BT_RES_OK) {
		printf("[example_bt_spp] BR EDR discoverable on\n\r");
	}
	else {
		printf("\n\r[example_bt_spp] [spp_discoverable_on_cb] Error, error code (tBT_ResultEnum): %x\n\r",result);
	}
	
}

static void spp_discoverable_off_cb(tBT_ResultEnum result)
{
	if ( result == BT_RES_OK) {
		printf("[example_bt_spp] BR EDR discoverable off\n\r");
	}
	else {
		printf("\n\r[example_bt_spp] [spp_discoverable_off_cb] Error, error code (tBT_ResultEnum): %x\n\r",result);
	}
}


/*--------------------------------------*/
/*            Event Handlers            */
/*--------------------------------------*/

void ex_spp_conn_ind(tBT_SppConnInd *param)
{
	spp_svc.spp_connIF = param->conn;
	
	BT_SPP_ConnectRsp(spp_svc.spp_connIF, BT_SPP_RET_SUCCESS);
}

void ex_spp_conn_complete(tBT_SppConnInd *param)
{
	if (param->result == BT_SPP_RET_SUCCESS) {
		printf("\n\r[example_bt_spp] SPP Connection Established (%s)\n\r",(param->conn==spp_client_conn)? "Client":"Server");
		spp_svc.spp_connIF = param->conn;
		spp_conn_if = param->conn;

		BT_ConnectionStatus* connStatus = NULL;
		connStatus = get_conn_status(CONN_SPP,(void *)param->conn);
		connStatus->tick_Conn = xTaskGetTickCount();
		spp_svc.conn_mfs = param->cur_mfs;
		connStatus->mtu = param->cur_mfs;
	}
	else {
		printf("\n\r[example_bt_spp] Connect Error, error code (tBT_SppResult): %x\n\r",param->result);
	}
}

void ex_spp_disconn_complete(tBT_SppConnInd *param)
{
	printf("\n\r[example_bt_spp] SPP Connection Disconnected (%s)\n\r",(param->conn==spp_client_conn)? "Client":"Server");

	BT_ConnectionStatus* connStatus = NULL;
	connStatus = get_conn_status(CONN_SPP,(void *)param->conn);
	connStatus->tick_Disconn = xTaskGetTickCount();
	printf("[example_bt_spp] SPP Connection time: %u secs \n\r\n\r", (connStatus->tick_Disconn-connStatus->tick_Conn)/1000);

	connStatus->connIF.spp= NULL;
	spp_conn_if = NULL;

	if (BT_GAP_BrScanEnableGet() == BT_GAP_BR_NO_SCAN_ENABLE)	// BR/EDR discoverable off -> turn on 
		BT_GAP_BrScanEnableSet(BT_GAP_BR_INQUIRY_SCAN_ENABLE_PAGE_SCAN_ENABLE, spp_discoverable_on_cb);
}
void ex_spp_data(tBT_SppDataInd *param)
{
	BT_ConnectionStatus* connStatus = NULL;
	connStatus = get_conn_status(CONN_SPP,(void *)param->conn);

	tUint8* data = (tUint8*)malloc(param->length+1);
	memset(data+param->length,0,1);
	memcpy(data,param->data,param->length);

	printf("Data received [%d]: %s ( ",param->length,data);
	print_buf(data,param->length);
	printf(")\n\r");
	
	free(data);
}

void ex_spp_cb(tBT_SppEventEnum ev, void *param)
{
	//printf("[example_bt_spp SPP], %x\n\r", ev, param);
	switch (ev) {
		case BT_SPP_EV_SERVICE_START:
			printf("[example_bt_spp] SPP service started\n\r");	
			break;
		case BT_SPP_EV_SERVICE_STOP:
			break;
		case BT_SPP_EV_CONNECT_IND:
			ex_spp_conn_ind((tBT_SppConnInd *)param);
			break;
		case BT_SPP_EV_CONNECT_COMPLETE:
			ex_spp_conn_complete((tBT_SppConnInd *)param);
			break;
		case BT_SPP_EV_DISCONNECT_COMPLETE:
			ex_spp_disconn_complete((tBT_SppConnInd *)param);
			break;
		case BT_SPP_EV_DATA_IND:
			ex_spp_data((tBT_SppDataInd *)param);
			break;
		default:
			printf("[example_bt_spp] [bt_spp_cb] Unhandled event: %x\n\r",ev);
			break;
	}
}

void at_gap_ev_cb(tBT_GapEvEnum ev, void *param);
void init_done_cb_spp(tBT_ResultEnum result)
{
	if (result == BT_RES_OK) {
		printf("\n\r[example_bt_spp] BT Initialized\n\r");

		BT_GAP_PairingFeatureSet(BT_GAP_IOCAP_NO_INPUT_NO_OUTPUT, BT_GAP_SECU_REQ_BOND);
		BT_GAP_CallbackReg(at_gap_ev_cb);
		BT_GAP_BrInquiryScanActivitySet(1024, 32, NULL);
		BT_GAP_BrPageScanActivitySet(1024, 32, NULL);
		BT_GAP_BrAuthEnable(0, NULL);
		BT_GAP_BrSSPSet(1, NULL);
		BT_GAP_BrClassOfDevice(0x001F00, NULL);

		/* Device Name */
		tChar name[DEV_NAME_LEN];
		tUint8 bd[BD_ADDR_LEN] = {0};
		tUChar bd_str[BD_ADDR_LEN * 2 + 1] = {0};
		
		BT_GAP_LocalBDGet(bd,NULL);
		bdaddr_to_str(bd_str, bd);
		strcpy(name,"Ameba_SPP_");
		int idx = strlen(name);
		memcpy(name+idx,&(bd_str[6]),7);
		
		tBT_EirDataStru eir_data;
		memset(&eir_data,0,sizeof(tBT_EirDataStru));
		strcpy(eir_data.dev_name,name);
		eir_data.mask = BT_GAP_BLE_ADVDATA_MBIT_NAME;
		
		/* BT_GAP_BrDeviceNameSet must be called "AFTER" BT_GAP_BrScanEnableSet*/
		BT_GAP_BrScanEnableSet(BT_GAP_BR_NO_SCAN_ENABLE, NULL);
		BT_GAP_BrScanEnableSet(BT_GAP_BR_INQUIRY_SCAN_ENABLE_PAGE_SCAN_ENABLE, spp_discoverable_on_cb);
		BT_GAP_BrDeviceNameSet(name, NULL);
		BT_GAP_BrWriteEIRData(&eir_data, NULL);

		/* SPP Service */
		memset(&spp_svc.param,0,sizeof(spp_svc.param));
		spp_svc.param.mfs = 512;	// MTU
		spp_svc.param.init_credit = 7;
		spp_svc.spp_svcIF = BT_SPP_ServiceCreate(&spp_svc.param,ex_spp_cb);
		BT_SPP_ServiceStart(spp_svc.spp_svcIF);
		
	}
	else {
		printf("\n\r[example_bt_spp] [init_done_cb] Error, error code (tBT_ResultEnum): %x\n\r",result);
	}
}

void bt_spp_task(void)
{
	printf("\n\r[example_bt_spp] Initializing BT ...\n\r");
	BT_Init(init_done_cb_spp);
	vTaskDelete(NULL);
}

void example_bt_spp(void){
	if(xTaskCreate((TaskFunction_t)bt_spp_task, (char const *)"bt_spp_task", 512, NULL, tskIDLE_PRIORITY + 1, NULL) != pdPASS){
		printf("\n\r[%s] Create update task failed", __FUNCTION__);
	}
}

#endif /* CONFIG_BT */
