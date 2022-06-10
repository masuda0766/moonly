#include "platform_opts.h"
#if CONFIG_BT

#include <platform/platform_stdlib.h>
#include "FreeRTOS.h"
#include "task.h"
#include "osdep_service.h"
#include "bt_common.h"
#include "bt_gap.h"
#include "bt_gatt.h"
#include "bt_util.h"

#define	SVC_UUID					0xFF01
#define CHR_READ_UUID				0xFF02
#define CHR_WRITE_NORSP_UUID		0xFF03
#define CHR_WRITE_UUID				0xFF04
#define CHR_NOTIFY_UUID				0xFF05
#define CHR_INDICATE_UUID			0xFF06
#define CHR_AUTH_WRITE_UUID			0xFF07
#define CHR_ALL_UUID				0xFF08	/* All Property Bits set */

#define	INC_SVC_UUID				0xFFAA
#define	INC_SVC_CHAR_UUID			0xFFBB

typedef struct {
	tBT_GattConnIF conn;
	AttrStru inc;
	AttrStru svc;
	AttrStru chr_read;
	AttrStru chr_write_norsp;
	AttrStru chr_write;
	AttrStru chr_notify;
	AttrStru desc_ccc_notify;
	AttrStru chr_indicate;
	AttrStru desc_ccc_indicate;
	AttrStru chr_auth_write;
	AttrStru chr_all;
	AttrStru desc_all_ext_prop;		// 2900
	AttrStru desc_all_usr_desc;		// 2901
	AttrStru desc_all_ccc;			// 2902
	AttrStru desc_all_scc;			// 2903
	AttrStru desc_all_chr_format;	// 2904
	AttrStru desc_all_aggr_format;	// 2905
} GattSvcStru;

typedef struct {
	AttrStru svc;
	AttrStru chr_read_write;
} GattIncSvcStru;

static GattSvcStru primary_svc;
static GattIncSvcStru included_svc;
static int notify_data = 0;

#define PREP_WRITE_BUF_SIZE 256;
struct list_head prep_write_buf_list;

typedef struct {
	struct list_head list;
	AttrStru buffer;
	tUint16 buffer_size;
} GattPrepWriteBuf;

static void adv_on_cb(tBT_ResultEnum result)
{
	if ( result == BT_RES_OK) {
		printf("[example_bt_gatt_server] Start broadcasting ADV packets\r\n");
	}
	else {
		printf("\r\n[Error] [example_bt_gatt_server] [adv_on_cb] error code (tBT_ResultEnum): %x\r\n",result);		
	}
	
}

static void svr_set_adv_data()
{
	tChar name[DEV_NAME_LEN];
	tUint8 bd[BD_ADDR_LEN] = {0};
	tUChar bd_str[BD_ADDR_LEN * 2 + 1] = {0};
	
	BT_GAP_LocalBDGet(NULL,bd);
	bdaddr_to_str(bd_str, bd);
	strcpy(name,"Ameba_GATT_Svr_");
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
		BT_GAP_BLE_OWN_ADDRESS_TYPE_RANDOM, /* MUST BE RANDOM */
		BT_GAP_BLE_ADV_CHNMAP_CHALL, /* tUint8 chn_map */
		BT_GAP_BLE_ADV_FILTER_ALL, /* tUint8 filter */
	};

	BT_GAP_BleAdvertisingDataUpdate(&adv_data);
	BT_GAP_BleScanResponseDataUpdate(&adv_data);
	BT_GAP_BleAdvertisingParamsSet(&param);
	BT_GAP_BleAdvertisingSet(FALSE, NULL);
	BT_GAP_BleAdvertisingSet(TRUE, adv_on_cb);
	printf("[example_bt_gatt_server] Device name: %s\n\r",name);
}



/*--------------------------------------*/
/*            Event Handlers            */
/*--------------------------------------*/

static void conn_complete_cb(tBT_GattConnCmplStru *param)
{
	if (param->res== BT_RES_OK) {
		printf("\r\n[example_bt_gatt_server] Bluetooth Connection Established (GATT Server)\r\n");
		
		BT_ConnectionStatus* connStatus = NULL;
		connStatus = get_conn_status(CONN_GATT,(void *)param->conn);
		connStatus->tick_Conn = xTaskGetTickCount();
		primary_svc.conn = param->conn;
		bt_peer_conn_if = param->conn;
		notify_data = 0;
	}
	else {
		printf("[Error] [example_bt_gatt_server] Connect Error, error code (tBT_ResultEnum): %x\r\n",param->res);
	}
}

static void disconn_complete_cb(tBT_GattDisconnCmplStru *param)
{
	printf("\r\n[example_bt_gatt_server] Bluetooth Connection Disconnected (GATT Server)\r\n");
				
	BT_ConnectionStatus* connStatus = NULL;
	connStatus = get_conn_status(CONN_GATT,(void *)param->conn);
	connStatus->tick_Disconn = xTaskGetTickCount();
	printf("[example_bt_gatt_server] Bluetooth Connection time: %u secs \r\n", (connStatus->tick_Disconn-connStatus->tick_Conn)/1000);

	connStatus->connIF.gatt= NULL;
	primary_svc.conn = NULL;
	bt_peer_conn_if = NULL;

	//  CCCD init value must be 0, so reset CCCD value after disconnected
	memset(primary_svc.desc_ccc_notify.val->data,0,2);
	memset(primary_svc.desc_ccc_indicate.val->data,0,2);
	memset(primary_svc.desc_all_ccc.val->data,0,2);
	
	BT_GAP_BleAdvertisingSet(TRUE, NULL);
}

static void mtu_cb(tBT_GattMtuNotifStru *param)
{
	BT_ConnectionStatus* connStatus = NULL;
	connStatus = get_conn_status(CONN_GATT,(void *)param->conn);
	connStatus->mtu = param->mtu;
	
	printf("\r\n[example_bt_gatt_server] MTU: %d\r\n",param->mtu);
}

static void svc_start_cb(tBT_GattSvcStartStru *param)
{
	if (param->res != BT_GATT_OK) {
		printf("\r\n[example_bt_gatt_server] [svc_start_cb] Error, error code (tBT_ResultEnum): %x\r\n",param->res);
		return;
	}
	
	if (param->svc == included_svc.svc.attr_if) {
		included_svc.svc.attr_hdl = param->attr_hdl;
		printf("[svc_start_cb] Included_service started, include it into Primary_service\r\n");
		/* After Included_service is started, include it into Primary_service.
		 * Then, start Primary_service*/
		primary_svc.inc.attr_if = BT_GATTS_IncludeAdd(primary_svc.svc.attr_if,included_svc.svc.attr_if);
		BT_GATTS_ServiceStart(primary_svc.svc.attr_if);
	}
	else if (param->svc == primary_svc.svc.attr_if) {
		primary_svc.svc.attr_hdl = param->attr_hdl;
		printf("[svc_start_cb] Primary_service started\r\n");
	}
	else {
		printf("[Warning] [example_bt_gatt_server] [svc_start_cb] Unhandled service started, attr_hdl=%x, end_hdl=%x\r\n",param->attr_hdl,param->end_hdl);
	}

}

static void svc_stop_cb(tBT_GattSvcStopStru *param)
{
	if (param->res != BT_GATT_OK) {
		printf("\r\n[Error] [example_bt_gatt_server] [svc_stop_cb] error code (tBT_ResultEnum): %x\r\n",param->res);
		return;
	}
	if (param->svc == primary_svc.svc.attr_if) {
		printf("[svc_stop_cb] Primary_service stopped\r\n");
	}
	else if (param->svc == included_svc.svc.attr_if) {
		printf("[svc_stop_cb] Included_service stopped\r\n");
	}
}

static void inc_start_cb(tBT_GattIncStartStru *param)
{
	if (param->res == BT_GATT_OK) {
		if (param->inc == primary_svc.inc.attr_if) {
			primary_svc.inc.attr_hdl = param->attr_hdl;
			printf("[inc_start_cb] Service included successfully \r\n");
		}
	}
	else {
		printf("\r\n[Error] [example_bt_gatt_server] [inc_start_cb] error code (tBT_ResultEnum): %x\r\n",param->res);
		return;
	}
}

static void chr_start_cb(tBT_GattCharStartStru *param)
{
	if (param->res == BT_GATT_OK) {
		if (param->chr == primary_svc.chr_read.attr_if) {
			primary_svc.chr_read.attr_hdl = param->attr_hdl;
			printf("[chr_start_cb] chr_read started\r\n");
		}
		else if (param->chr == primary_svc.chr_write_norsp.attr_if) {
			primary_svc.chr_write_norsp.attr_hdl = param->attr_hdl;
			printf("[chr_start_cb] chr_write_norsp started\r\n");
		}
		else if (param->chr == primary_svc.chr_write.attr_if) {
			primary_svc.chr_write.attr_hdl = param->attr_hdl;
			printf("[chr_start_cb] chr_write started\r\n");
		}
		else if (param->chr == primary_svc.chr_notify.attr_if) {
			primary_svc.chr_notify.attr_hdl = param->attr_hdl;
			printf("[chr_start_cb] chr_notify started\r\n");
		}
		else if (param->chr == primary_svc.chr_indicate.attr_if) {
			primary_svc.chr_indicate.attr_hdl = param->attr_hdl;
			printf("[chr_start_cb] chr_indicate started\r\n");
		}
		else if (param->chr == primary_svc.chr_auth_write.attr_if) {
			primary_svc.chr_auth_write.attr_hdl = param->attr_hdl;
			printf("[chr_start_cb] chr_auth_write started\r\n");
		}
		else if (param->chr == primary_svc.chr_all.attr_if) {
			primary_svc.chr_all.attr_hdl = param->attr_hdl;
			printf("[chr_start_cb] chr_all started\r\n");
		}
		else if (param->chr == included_svc.chr_read_write.attr_if) {
			included_svc.chr_read_write.attr_hdl = param->attr_hdl;
			printf("[chr_start_cb] Included_service Characteristic started\r\n");
		}
		else {
			printf("[Warning] [example_bt_gatt_server] [chr_start_cb] Unhandled characteristic started, attr_hdl=%x, val_hdl=%x\r\n",param->attr_hdl,param->val_hdl);
		}
	}
	else {
		printf("[Error] [example_bt_gatt_server] [chr_start_cb] error code (tBT_ResultEnum): %x\r\n",param->res);
	}
}

static void notify_task(void)
{
	printf("\r\n[example_bt_gatt_server] notify_task running...\r\n");
	char istr[20];
	tUint16 *cccd = NULL;
	bool notify_task_running = 1;
	while(notify_task_running) {
		if ( primary_svc.desc_ccc_notify.attr_hdl != 0) {
			cccd = (tUint16 *) primary_svc.desc_ccc_notify.val->data;
			if ((*cccd) == BT_GATT_CCC_VAL_NOTIFICATION) {
				printf("[notify_task] [handle=0x%04x] Send notification: %d\r\n",primary_svc.chr_notify.attr_hdl,notify_data);			
				snprintf( istr, sizeof(istr), "%d", notify_data);
				BT_GATTS_ChrValueSend(primary_svc.conn, primary_svc.chr_notify.attr_if,istr,strlen(istr));
				notify_data++;
			}
		}
		if ( primary_svc.desc_ccc_indicate.attr_hdl != 0) {
			cccd = (tUint16 *) primary_svc.desc_ccc_indicate.val->data;
			if ((*cccd) == BT_GATT_CCC_VAL_INDICATION) {
				printf("[notify_task] [handle=0x%04x] Send indication: %d\r\n",primary_svc.chr_indicate.attr_hdl,notify_data);
				snprintf( istr, sizeof(istr), "%d", notify_data);
				BT_GATTS_ChrValueSend(primary_svc.conn, primary_svc.chr_indicate.attr_if,istr,strlen(istr));
				notify_data++;
			}
		}
		if ( primary_svc.desc_all_ccc.attr_hdl != 0) {
			cccd = (tUint16 *) primary_svc.desc_all_ccc.val->data;
			if ((*cccd) == BT_GATT_CCC_VAL_NOTIFICATION || (*cccd) == BT_GATT_CCC_VAL_INDICATION) {
				if ((*cccd) == BT_GATT_CCC_VAL_NOTIFICATION)
					printf("[notify_task] [handle=0x%04x] Send notification: %d\r\n",primary_svc.desc_all_ccc.attr_hdl,notify_data);
				else if ((*cccd) == BT_GATT_CCC_VAL_INDICATION)
					printf("[notify_task] [handle=0x%04x] Send indication: %d\r\n",primary_svc.desc_all_ccc.attr_hdl,notify_data);

				snprintf( istr, sizeof(istr), "%d", notify_data);
				BT_GATTS_ChrValueSend(primary_svc.conn, primary_svc.chr_all.attr_if,istr,strlen(istr));
				notify_data++;
			}
		}
		vTaskDelay(1000);
	}
	vTaskDelete(NULL);
}

static void desc_start_cb(tBT_GattDescStartStru *param)
{
	if (param->res == BT_GATT_OK) {
		if (param->des == primary_svc.desc_ccc_notify.attr_if) {
			primary_svc.desc_ccc_notify.attr_hdl = param->attr_hdl;
			printf("[desc_start_cb] desc_ccc_notify started\r\n");
			if(xTaskCreate((TaskFunction_t)notify_task, (char const *)"notify", 256, NULL, tskIDLE_PRIORITY + 1, NULL) != pdPASS){
				printf("\n\r[%s] Create notify task failed", __FUNCTION__);
			}				
		}
		else if (param->des == primary_svc.desc_ccc_indicate.attr_if) {
			primary_svc.desc_ccc_indicate.attr_hdl = param->attr_hdl;
			printf("[desc_start_cb] desc_ccc_indicate started\r\n");
		}
		else if (param->des == primary_svc.desc_all_ext_prop.attr_if) {
			primary_svc.desc_all_ext_prop.attr_hdl = param->attr_hdl;
			printf("[desc_start_cb] desc_all_ext_prop started\r\n");
		}
		else if (param->des == primary_svc.desc_all_usr_desc.attr_if) {
			primary_svc.desc_all_usr_desc.attr_hdl = param->attr_hdl;
			printf("[desc_start_cb] desc_all_usr_desc started\r\n");
		}
		else if (param->des == primary_svc.desc_all_ccc.attr_if) {
			primary_svc.desc_all_ccc.attr_hdl = param->attr_hdl;
			printf("[desc_start_cb] desc_all_ccc started\r\n");
		}
		else if (param->des == primary_svc.desc_all_scc.attr_if) {
			primary_svc.desc_all_scc.attr_hdl = param->attr_hdl;
			printf("[desc_start_cb] desc_all_scc started\r\n");
		}
		else if (param->des == primary_svc.desc_all_chr_format.attr_if) {
			primary_svc.desc_all_chr_format.attr_hdl = param->attr_hdl;
			printf("[desc_start_cb] desc_all_chr_format started\r\n");
		}
		else if (param->des == primary_svc.desc_all_aggr_format.attr_if) {
			primary_svc.desc_all_aggr_format.attr_hdl = param->attr_hdl;
			printf("[desc_start_cb] desc_all_aggr_format started\r\n");
		}
		else {
			printf("[Warning] [example_bt_gatt_server] [desc_start_cb] Unhandled descriptor started, attr_hdl=%x\r\n",param->attr_hdl);
		}
	}
	else {
		printf("[Error] [example_bt_gatt_server] [desc_start_cb] error code (tBT_ResultEnum): %x\r\n",param->res);
	}
}

static void read_cb(tBT_GattsReadIndStru *param)
{
	printf("[read_cb] [handle=0x%04x] ",param->att_hdl);
	tUint8 data[23] = {0};
	int max_len;
	int send_len;
	tBT_ValueStru *val = NULL;
	
	switch (param->attr_type) {
		case BT_GATT_ATTR_TYPE_CHARACTERISTIC:
			if (param->att_if == primary_svc.chr_read.attr_if)
				val = primary_svc.chr_read.val;
			else if (param->att_if == primary_svc.chr_all.attr_if)
				val = primary_svc.chr_all.val;
			else if (param->att_if == included_svc.chr_read_write.attr_if)
				val = included_svc.chr_read_write.val;
			
			max_len = BT_GATT_MtuGet(primary_svc.conn) - 1;
			send_len = (val->len-param->off > max_len) ? max_len: val->len-param->off;
				
			printf("[offset=%d] %.*s ( ", param->off,send_len,val->data+param->off);
			print_buf(val->data+param->off,send_len);
			printf(")\n\r");
			BT_GATTS_ReadRsp(param->conn, BT_GATT_OK, val->data+param->off,send_len);
			break;
		case BT_GATT_ATTR_TYPE_CHR_DESCRIPTOR:
			if (param->att_if == primary_svc.desc_all_usr_desc.attr_if) {
				/* Characteristic User Description */
				val = primary_svc.desc_all_usr_desc.val;
				printf("desc_all_usr_desc [len=%d]: %s\r\n",val->len,val->data);
			}
			else if (param->att_if == primary_svc.desc_all_aggr_format.attr_if) {
				/* Characteristic Aggregate Format */
				val = primary_svc.desc_all_aggr_format.val;
				memcpy(val->data,&primary_svc.desc_all_chr_format.attr_hdl,2);
				printf("desc_all_aggr_format [len=%d]: ",val->len);
				if (val->len != 0 && val->data != NULL) {
					tUint16 *hnd = NULL;
					for (int i = 0; i < val->len/2; i++) {
						hnd = (tUint16 *) &val->data[i*2];
						printf("%02x ", *hnd);
					}
				}
				printf("\r\n");
			}
			BT_GATTS_ReadRsp(param->conn, BT_GATT_OK, val->data, val->len);
			break;
		default:
			sprintf(data,"Unhandled attr_type: %x", param->attr_type);
			printf("[Warning] [example_bt_gatt_server] [read_cb] Unhandled attr_type = %x\r\n", param->attr_type);
			BT_GATTS_ReadRsp(param->conn, BT_GATT_OK, data, sizeof(data));
			break;
	}
}

static void write_cb(tBT_GattsWriteIndStru *param, BT_GattWriteEnum write_type)
{
	tUint8 write_type_str[16];
	tBT_GattResEnum res = BT_GATT_OK;
	
	if (write_type==WRITE_CMD)
		strcpy(write_type_str,"write_cmd");
	else if (write_type==WRITE_REQ)
		strcpy(write_type_str,"write_req");
	else if (write_type==PREP_WRITE)
		strcpy(write_type_str,"prep_write");
	
	printf("[write_cb] [handle=0x%04x] [%s] ",param->att_hdl,write_type_str);
	
	tUint8* data = (tUint8*)malloc(param->val_len+1);
	memset(data+param->val_len,0,1);
	memcpy(data,param->val,param->val_len);
	
	switch (param->attr_type) {
		case BT_GATT_ATTR_TYPE_CHARACTERISTIC: {
			printf("Characteristic value [len=%d]: %s ( ",param->val_len,data);
			print_buf(data,param->val_len);
			printf(")\n\r");

			if (write_type==PREP_WRITE) {
				GattPrepWriteBuf *entry = NULL;
				char buf_exist = 0;
				list_for_each_entry (entry, &prep_write_buf_list, list, GattPrepWriteBuf) {
				  	if (entry->buffer.attr_if == param->att_if) {
						buf_exist = 1;
						int buf_size_need = entry->buffer.val->len + param->val_len;
						if (entry->buffer_size >= buf_size_need) {
							memcpy(entry->buffer.val->data+entry->buffer.val->len,data, param->val_len);
							entry->buffer.val->len += param->val_len;
						}
						else { // buf size not enough
							int new_buf_size = entry->buffer_size + PREP_WRITE_BUF_SIZE;
							while (new_buf_size < buf_size_need) {
								new_buf_size += PREP_WRITE_BUF_SIZE;
							}
							entry->buffer_size = new_buf_size;
							tBT_ValueStru *val_stru = malloc_value_stru(new_buf_size);
							memcpy(val_stru->data,entry->buffer.val->data, entry->buffer.val->len);
							memcpy(val_stru->data+entry->buffer.val->len,data, param->val_len);
							val_stru->len = entry->buffer.val->len + param->val_len;
							free(entry->buffer.val);
							entry->buffer.val = val_stru;
						}
						break;	
					}
				}
				if (!buf_exist) {
					entry = malloc(sizeof(GattPrepWriteBuf));
					entry->buffer.attr_hdl = param->att_hdl;
					entry->buffer.attr_if = param->att_if;
					entry->buffer_size = param->val_len+PREP_WRITE_BUF_SIZE;
					entry->buffer.val= malloc_value_stru(entry->buffer_size);
					memcpy(entry->buffer.val->data,data, param->val_len);
					entry->buffer.val->len = param->val_len;
					list_add_tail(&entry->list, &prep_write_buf_list);
				}
			}
			else {
				tBT_ValueStru **pVal = NULL;
				if (param->att_if == primary_svc.chr_write_norsp.attr_if)
					pVal = &primary_svc.chr_write_norsp.val;
				else if (param->att_if == primary_svc.chr_write.attr_if)
					pVal = &primary_svc.chr_write.val;
				else if (param->att_if == primary_svc.chr_all.attr_if)
					pVal = &primary_svc.chr_all.val;
				else if (param->att_if == included_svc.chr_read_write.attr_if)
					pVal = &included_svc.chr_read_write.val;
				
				free(*pVal);
				*pVal = malloc_value_stru(param->val_len);
				memcpy((*pVal)->data,data, param->val_len);
			}
			break;
		}	
		case BT_GATT_ATTR_TYPE_CHR_DESCRIPTOR : {
			printf("Characteristic descriptor: ");
			if (param->att_if == primary_svc.desc_all_usr_desc.attr_if) {
				/* Characteristic User Description */
				free(primary_svc.desc_all_usr_desc.val);
				
				primary_svc.desc_all_usr_desc.val = malloc_value_stru(param->val_len);
				memcpy(primary_svc.desc_all_usr_desc.val->data,param->val,param->val_len);
				printf("desc_all_usr_desc [len=%d]: %s\r\n",param->val_len,data);
			}
			else if (param->att_if == primary_svc.desc_ccc_notify.attr_if
				|| param->att_if == primary_svc.desc_ccc_indicate.attr_if
				|| param->att_if == primary_svc.desc_all_ccc.attr_if) {
				/* Client Characteristic Configuration */
				tUint16 *cccd = NULL;
				tUint16 *cccd_in = (tUint16 *) param->val;
				
				if (param->att_if == primary_svc.desc_ccc_notify.attr_if) {
					cccd = (tUint16 *) primary_svc.desc_ccc_notify.val->data;
					printf("desc_ccc_notify: ");
					if ((*cccd_in) != BT_GATT_CCC_VAL_NOTIFICATION && (*cccd_in) != BT_GATT_CCC_VAL_NONE) {
						res = BT_GATT_RES_CCC_IMPROPERLY_CONFIGURED;
						printf("%x [INVALID]\r\n",*cccd_in);
						goto exit;
					}
				}
				else if (param->att_if == primary_svc.desc_ccc_indicate.attr_if) {
					cccd = (tUint16 *) primary_svc.desc_ccc_indicate.val->data;
					printf("desc_ccc_indicate: ");
					if ((*cccd_in) != BT_GATT_CCC_VAL_INDICATION && (*cccd_in) != BT_GATT_CCC_VAL_NONE) {
						res = BT_GATT_RES_CCC_IMPROPERLY_CONFIGURED;
						printf("%x [INVALID]\r\n",*cccd_in);
						goto exit;
					}
				}
				else if (param->att_if == primary_svc.desc_all_ccc.attr_if) {
					cccd = (tUint16 *) primary_svc.desc_all_ccc.val->data;
					printf("desc_all_ccc: ");
					if ((*cccd_in) != BT_GATT_CCC_VAL_NOTIFICATION && (*cccd_in) != BT_GATT_CCC_VAL_INDICATION && (*cccd_in) != BT_GATT_CCC_VAL_NONE) {
						res = BT_GATT_RES_CCC_IMPROPERLY_CONFIGURED;
						printf("%x [INVALID]\r\n",*cccd_in);
						goto exit;
					}
				}
				memcpy(cccd,cccd_in,CLI_CONF_LEN);
				printf("%x\r\n",*cccd);
			}
			else if (param->att_if == primary_svc.desc_all_scc.attr_if) {
				/* Server Characteristic Configuration */
				tUint16 *sccd = NULL;
				sccd = (tUint16 *) primary_svc.desc_all_scc.val->data;
				memcpy(sccd,param->val,SVR_CONF_LEN);
				printf("desc_all_scc: %x\r\n",*sccd);
			}
			break;
		}
		default:
			break;
	}
exit:
	free(data);
	if (write_type == WRITE_REQ || write_type == PREP_WRITE)
		BT_GATTS_WriteRsp(param->conn, res);
}

static void exec_write_cb(tBT_GattsExcWriteIndStru *param)
{
	printf("[exec_write_cb] Execute Write Request: ");
	char found_exec_write_buf = 0;
	GattPrepWriteBuf *entry = NULL;				
	list_for_each_entry (entry, &prep_write_buf_list, list, GattPrepWriteBuf) {
		if (entry->buffer.attr_if == param->att_if) {
			found_exec_write_buf = 1;
			if (param->flag == BT_GATT_EWRF_IMMEDIATELY_WRITE_ALL) {
				tBT_ValueStru **pVal = NULL;
				if (param->att_if == primary_svc.chr_write_norsp.attr_if)
					pVal = &primary_svc.chr_write_norsp.val;
				else if (param->att_if == primary_svc.chr_write.attr_if)
					pVal = &primary_svc.chr_write.val;
				else if (param->att_if == primary_svc.chr_all.attr_if)
					pVal = &primary_svc.chr_all.val;
				else if (param->att_if == included_svc.chr_read_write.attr_if)
					pVal = &included_svc.chr_read_write.val;
				
				free(*pVal);
				*pVal = malloc_value_stru(entry->buffer.val->len);
				memcpy((*pVal)->data,entry->buffer.val->data, entry->buffer.val->len);
				printf("Immediately Write All\r\n");
			}
			else if (param->flag == BT_GATT_EWRF_CANCEL_ALL) {
				printf("Cancel All Prepared Writes\r\n");
			}
			list_del_init(&entry->list);
			free(entry->buffer.val);
			free(entry);
			break;
		}
	}
	
	if (found_exec_write_buf) {
		BT_GATTS_ExecuteWriteRsp(param->conn, BT_GATT_OK);
	}
	else {
		printf("Error!\r\n");
	}
}

static void indication_cb(tBT_GattsIndValueCfmStru *param)
{
	// ACK of indication
	printf("[indication_cb] Handle Value Confirmation\r\n");
}

static void prim_svc_cb(tBT_GattEventEnum ev, void *param)
{
	switch (ev) {			
		case BT_GATTS_EV_SERVICE_START:
			svc_start_cb((tBT_GattSvcStartStru *)param);
			break;
		case BT_GATTS_EV_SERVICE_STOP:
			svc_stop_cb((tBT_GattSvcStopStru *)param);
			break;
		case BT_GATTS_EV_INCLUDE_START:
			inc_start_cb((tBT_GattIncStartStru *)param);
			break;
		case BT_GATTS_EV_CHARACTERISTIC_START:
			chr_start_cb((tBT_GattCharStartStru *)param);
			break;
		case BT_GATTS_EV_DESCRIPTOR_START:
			desc_start_cb((tBT_GattDescStartStru *)param);
			break;
		case BT_GATTS_EV_READ:
			read_cb((tBT_GattsReadIndStru *)param);
			break;
		case BT_GATTS_EV_WRITE:
			write_cb((tBT_GattsWriteIndStru *)param, WRITE_CMD);
			break;
		case BT_GATTS_EV_WRITE_REQ:
			write_cb((tBT_GattsWriteIndStru *)param, WRITE_REQ);
			break;
		case BT_GATTS_EV_PREPARE_WRITE:
			write_cb((tBT_GattsWriteIndStru *)param, PREP_WRITE);
			break;
		case BT_GATTS_EV_EXCUETE_WRITE:
			exec_write_cb((tBT_GattsExcWriteIndStru *)param);
			break;
		case BT_GATTS_EV_HANDLE_VALUE_CFM:
			indication_cb((tBT_GattsIndValueCfmStru *)param);
			break;
		default:
			printf("[Warning] [example_bt_gatt_server] [prim_svc_cb] Unhandled event: %x\r\n",ev);
			break;
	}
}

static void inc_svc_cb(tBT_GattEventEnum ev, void *param)
{
	switch (ev) {	
		case BT_GATTS_EV_SERVICE_START:
			svc_start_cb((tBT_GattSvcStartStru *)param);
			break;
		case BT_GATTS_EV_SERVICE_STOP:
			svc_stop_cb((tBT_GattSvcStopStru *)param);
			break;
		case BT_GATTS_EV_CHARACTERISTIC_START:
			chr_start_cb((tBT_GattCharStartStru *)param);
			break;
		case BT_GATTS_EV_DESCRIPTOR_START:
			desc_start_cb((tBT_GattDescStartStru *)param);
			break;
		case BT_GATTS_EV_READ:
			read_cb((tBT_GattsReadIndStru *)param);
			break;
		case BT_GATTS_EV_WRITE:
			write_cb((tBT_GattsWriteIndStru *)param,WRITE_CMD);
			break;
		case BT_GATTS_EV_EXCUETE_WRITE:
			exec_write_cb((tBT_GattsExcWriteIndStru *)param);
			break;
		default:
			printf("[Warning] [example_bt_gatt_server] [inc_svc_cb] Unhandled event: %x\r\n",ev);
			break;
	}
}

static void gatt_ev_cb(tBT_GattEventEnum ev, void *param)
{
	switch (ev) {
		case BT_GATT_EV_CONNECT_COMPLETE:
			conn_complete_cb((tBT_GattConnCmplStru *)param);
			break;
		case BT_GATT_EV_DISCONNECT_COMPLETE:
			disconn_complete_cb((tBT_GattDisconnCmplStru *)param);
			break;
		case BT_GATT_EV_MTU_NOTIFICATION:
			mtu_cb((tBT_GattMtuNotifStru *)param);
			break;
		case BT_GATT_EV_READ_RSSI_CFM: 
			printf("\r\n[example_bt_gatt_server] RSSI of connected device: %d\n\r",((tBT_GattReadRssiInd *)param)->rssi);
			break;
		case BT_GATT_EV_CONN_INTERVAL_UADATE:
			printf("\r\n[example_bt_gatt_server] [BT_GATT_EV_CONN_INTERVAL_UADATE] addr_type = %s, peer_addr = "BD_ADDR_FMT"\n\r", (((tBT_GattUpdateStru *)param)->addr.type == BT_ADDR_TYPE_PUBLIC)? "public":"random",BD_ADDR_ARG(((tBT_GattUpdateStru *)param)->addr.bd));			
			printf("[example_bt_gatt_server] [BT_GATT_EV_CONN_INTERVAL_UADATE] conn_interval of connected device: %d\n\r", ((tBT_GattUpdateStru *)param)->conn_interval);
			break;
		default:
			printf("[Warning] [example_bt_gatt_server] [gatt_ev_cb] Unhandled event: %x\r\n",ev);
			break;
	}
}

static void register_gatt_service(void)
{
	BT_GATT_CallbackRegister(gatt_ev_cb);
	BT_GATT_DefaultMtuSet(256);
	INIT_LIST_HEAD(&prep_write_buf_list);

	tBT_UuidStru uuid;
	uuid16_create(&uuid,INC_SVC_UUID);
	included_svc.svc.attr_if = BT_GATTS_ServiceCreate(&uuid, inc_svc_cb, TRUE);
	uuid16_create(&uuid,INC_SVC_CHAR_UUID);
	included_svc.chr_read_write.attr_if= BT_GATTS_CharacteristicAdd(included_svc.svc.attr_if, &uuid,  (BT_GATT_PROP_READ | BT_GATT_PROP_WRITE_CMD), (BT_GATT_PERM_READ| BT_GATT_PERM_WRITE));
	tUint8 chr_read_write_val[] = "chr_read_write";
	included_svc.chr_read_write.val = malloc_value_stru((tUint16)strlen(chr_read_write_val));
	memcpy(included_svc.chr_read_write.val->data,chr_read_write_val,(tUint16)strlen(chr_read_write_val));
	BT_GATTS_ServiceStart(included_svc.svc.attr_if);

	uuid16_create(&uuid,SVC_UUID);
	primary_svc.svc.attr_if = BT_GATTS_ServiceCreate(&uuid, prim_svc_cb, FALSE);

	uuid16_create(&uuid,CHR_READ_UUID);
	primary_svc.chr_read.attr_if = BT_GATTS_CharacteristicAdd(primary_svc.svc.attr_if, &uuid, BT_GATT_PROP_READ, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE);
	tUint8 read_val[] = "chr_read";
	primary_svc.chr_read.val = malloc_value_stru((tUint16)strlen(read_val));
	memcpy(primary_svc.chr_read.val->data,read_val,(tUint16)strlen(read_val));	

	uuid16_create(&uuid,CHR_WRITE_NORSP_UUID);
	primary_svc.chr_write_norsp.attr_if = BT_GATTS_CharacteristicAdd(primary_svc.svc.attr_if, &uuid, BT_GATT_PROP_WRITE_CMD, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE);

	uuid16_create(&uuid,CHR_WRITE_UUID);
	primary_svc.chr_write.attr_if = BT_GATTS_CharacteristicAdd(primary_svc.svc.attr_if, &uuid, BT_GATT_PROP_WRITE_REQ, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE);
	
	uuid16_create(&uuid,CHR_NOTIFY_UUID);
	primary_svc.chr_notify.attr_if = BT_GATTS_CharacteristicAdd(primary_svc.svc.attr_if, &uuid, BT_GATT_PROP_NOTIFY, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE);
	primary_svc.desc_ccc_notify.attr_if = BT_GATTS_DesCCCAdd(primary_svc.chr_notify.attr_if, BT_GATT_PROP_NOTIFY, (BT_GATT_PERM_READ | BT_GATT_PERM_WRITE), BT_GATT_CCC_VAL_NONE);
	primary_svc.desc_ccc_notify.val = malloc_value_stru(CLI_CONF_LEN);

	uuid16_create(&uuid,CHR_INDICATE_UUID);
	primary_svc.chr_indicate.attr_if = BT_GATTS_CharacteristicAdd(primary_svc.svc.attr_if, &uuid, BT_GATT_PROP_INDICATE, (BT_GATT_PERM_READ | BT_GATT_PERM_WRITE));
	primary_svc.desc_ccc_indicate.attr_if = BT_GATTS_DesCCCAdd(primary_svc.chr_indicate.attr_if, BT_GATT_PROP_INDICATE, (BT_GATT_PROP_READ | BT_GATT_PROP_WRITE_REQ), BT_GATT_CCC_VAL_NONE);
	primary_svc.desc_ccc_indicate.val = malloc_value_stru(CLI_CONF_LEN);

	uuid16_create(&uuid,CHR_AUTH_WRITE_UUID);
	primary_svc.chr_auth_write.attr_if = BT_GATTS_CharacteristicAdd(primary_svc.svc.attr_if, &uuid, BT_GATT_PROP_WRITE_REQ, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE | BT_GATT_PERM_WRITE_AUTHEN_REQUEIRED);

	uuid16_create(&uuid,CHR_ALL_UUID);
	primary_svc.chr_all.attr_if = BT_GATTS_CharacteristicAdd(primary_svc.svc.attr_if, &uuid, (	BT_GATT_PROP_BROADCAST | BT_GATT_PROP_READ | BT_GATT_PROP_WRITE_CMD | BT_GATT_PROP_WRITE_REQ | BT_GATT_PROP_NOTIFY | BT_GATT_PROP_INDICATE | BT_GATT_PROP_SIGNWRITE
 | BT_GATT_PROP_EXTENDED), (BT_GATT_PERM_READ | BT_GATT_PERM_WRITE));
	tUint8 chr_all_val[] = "chr_all";
	primary_svc.chr_all.val = malloc_value_stru((tUint16)strlen(chr_all_val));
	memcpy(primary_svc.chr_all.val->data,chr_all_val,(tUint16)strlen(chr_all_val));
	primary_svc.desc_all_usr_desc.attr_if = BT_GATTS_DesUserDescriptionAdd(primary_svc.chr_all.attr_if, (BT_GATT_PERM_WRITE | BT_GATT_PERM_READ));
	primary_svc.desc_all_ccc.attr_if = BT_GATTS_DesCCCAdd(primary_svc.chr_all.attr_if, BT_GATT_PROP_NOTIFY | BT_GATT_PROP_INDICATE, (BT_GATT_PERM_WRITE | BT_GATT_PERM_READ), BT_GATT_CCC_VAL_NONE);
	primary_svc.desc_all_scc.attr_if = BT_GATTS_DesSCCAdd(primary_svc.chr_all.attr_if, (BT_GATT_PERM_WRITE | BT_GATT_PERM_READ));
	primary_svc.desc_all_chr_format.attr_if = BT_GATTS_DesPresFormatAdd(primary_svc.chr_all.attr_if, 1, 2, 3, 4, 5);
	primary_svc.desc_all_aggr_format.attr_if = BT_GATTS_DesAggrFormatAdd(primary_svc.chr_all.attr_if, NULL, 0);

	
	tUint8 usr_desc[] = "desc_all_usr_desc";
	primary_svc.desc_all_usr_desc.val = malloc_value_stru((tUint16)strlen(usr_desc));
	memcpy(primary_svc.desc_all_usr_desc.val->data,usr_desc,(tUint16)strlen(usr_desc));
	primary_svc.desc_all_ccc.val = malloc_value_stru(CLI_CONF_LEN);
	primary_svc.desc_all_scc.val = malloc_value_stru(SVR_CONF_LEN);
	primary_svc.desc_all_chr_format.val = malloc_value_stru(CHR_FMT_LEN);
	primary_svc.desc_all_aggr_format.val = malloc_value_stru(2);

}

static void stop_gatt_service()
{
	BT_GATTS_ServiceStop(primary_svc.svc.attr_if);
	BT_GATTS_ServiceStop(included_svc.svc.attr_if);
}

static void delete_gatt_service()
{
	stop_gatt_service();
	free(included_svc.chr_read_write.val);
	free(primary_svc.chr_read.val);
	free(primary_svc.chr_write_norsp.val);
	free(primary_svc.chr_write.val);
	free(primary_svc.chr_all.val);
	free(primary_svc.desc_ccc_notify.val);
	free(primary_svc.desc_ccc_indicate.val);
	free(primary_svc.desc_all_ccc.val);
	free(primary_svc.desc_all_scc.val);
	free(primary_svc.desc_all_chr_format.val);
	free(primary_svc.desc_all_aggr_format.val);
	GattPrepWriteBuf *entry = NULL;				
	list_for_each_entry (entry, &prep_write_buf_list, list, GattPrepWriteBuf) {
		free(entry->buffer.val);
		free(entry);
	}
	INIT_LIST_HEAD(&prep_write_buf_list);
	
	// service SHOULD BE stopped or not started, otherwise delete will fail.
	BT_GATTS_ServiceDelete(primary_svc.svc.attr_if);
	BT_GATTS_ServiceDelete(included_svc.svc.attr_if);
}

void at_gap_ev_cb(tBT_GapEvEnum ev, void *param);
static void bt_init_done_cb(tBT_ResultEnum result)
{
	if (result == BT_RES_OK) {
		printf("\r\n[example_bt_gatt_server] BT initialized\r\n");

		printf("\r\n[example_bt_gatt_server] Register GAP event callback\r\n");
		BT_GAP_PairingFeatureSet(BT_GAP_IOCAP_KEYBOARD_DISPLAY, BT_GAP_SECU_REQ_BOND);
		BT_GAP_CallbackReg(at_gap_ev_cb);
		svr_set_adv_data();
		printf("\r\n[example_bt_gatt_server] Register GATT Service\r\n");
		register_gatt_service();
	}
	else {
		printf("\r\n[example_bt_gatt_server] [init_done_cb] Error, error code (tBT_ResultEnum): %x\r\n",result);
	}
}

static void bt_gatt_server_task(void)
{
	printf("\r\n[example_bt_gatt_server] Initializing BT ...\r\n");
	BT_Init(bt_init_done_cb);
	
	vTaskDelete(NULL);
}

void example_bt_gatt_server(void){
	if(xTaskCreate((TaskFunction_t)bt_gatt_server_task, (char const *)"bt_gatt_server_task", 2048, NULL, tskIDLE_PRIORITY + 1, NULL) != pdPASS){
		printf("\n\r[%s] Create gatt server task failed", __FUNCTION__);
	}
}

#endif /* CONFIG_BT */
