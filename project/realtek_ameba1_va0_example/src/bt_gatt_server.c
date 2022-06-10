/*
 * bt_gatt_server.c
 *
 * copy from example_bt_gatt_server.c
 */

#include "platform_opts.h"
#if 1 // CONFIG_BT && CONFIG_BT_GATT_SERVER

#include <sys.h>

#include <platform/platform_stdlib.h>
#include "FreeRTOS.h"
#include "task.h"
#include "osdep_service.h"
#include "bt_common.h"
#include "bt_gap.h"
#include "bt_gatt.h"
#include "bt_util.h"
#include "lwip_netconf.h"

#include "state_manager.h"

#include "version.h"
#include "scale.h"

#include "bt_gatt_server.h"

#include "rf_ctrl.h"
#include "rf_ctrl_ble.h"
#include "sntp/sntp.h"

#define PET_SVC_UUID				"66d2a3c0d90211e4ac910002a5d5c51b"	/* Pet Healthcare Data Transport Service */

#define DATA_CHR_UUID				"66d2a3c1d90211e4ac910002a5d5c51b"	/* Data Transport Characterisitc */
#define VER_CHR_UUID				"66d2a3c2d90211e4ac910002a5d5c51b"	/* Service Version Characteristic */
#define DATE_CHR_UUID				"66d2a3c3d90211e4ac910002a5d5c51b"	/* Date Setting Characteristic */
#define FW_UPDATE_CHR_UUID			"66d2a3c4d90211e4ac910002a5d5c51b"	/* Firmware Update Characterisitc */
#define CONF_CHR_UUID				"66d2a3c5d90211e4ac910002a5d5c51b"	/* Configuration Characterisitc */
#define MTU_CHR_UUID				"66d2a3c6d90211e4ac910002a5d5c51b"	/* MTU Information Characterisitc */

#define VER_VAL_0 0x01		/* Service Version : 0x0100 */
#define VER_VAL_1 0x00

#define SNTP_DATE_ENDIAN 0

//#define DISABLE_GATT_PERM_ENC	/* デバッグ用：各CharacteristicのAttribute PermissionでEncryptionを無効化する */

static int debug_print_lebel = 3;
static int debug_print_short = 0;
static int debug_print_lebel_auto = 0;

#if 1

/*
static char print_buffer[1024];
static void dbug_printf(int lv, char *format, ...) {
    va_list arg;
    print_buffer[0] = 0;

    if (lv > debug_print_lebel) return;

    va_start(arg, format);
    vsprintf(print_buffer, format, arg);
    va_end(arg);

    printf("%s", print_buf);

    return;;
}
*/


//[TODO] Log_Debug
#define DEBUG_PRINT_ENABLE
#ifdef DEBUG_PRINT_ENABLE
#define D(fmt, ...)  { if (debug_print_lebel >= 5) Log_Info("\r\n[GATT][DEBUG]%s::[%d] " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__); }
#define DD(fmt, ...)  { if (debug_print_lebel >= 5) Log_Info(fmt, ##__VA_ARGS__); }
#define D_BUF(data,len) if (debug_print_lebel >= 5) {\
	for(int i=0; i<len; i++) {\
		DD("%02X ",*((unsigned char *)data+i));\
	}\
}

#define DBG if (debug_print_lebel >= 5)

#else
#define D(fmt, ...)
#define DD(fmt, ...)
#define D_BUF(...)
#define DBG if (0)
#endif

#define INFO_PRINT_ENABLE
#ifdef INFO_PRINT_ENABLE
#define DI(fmt, ...)  if (debug_print_lebel >= 3) Log_Info("\r\n[GATT][INFO]%s::[%d] " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define DDI(fmt, ...)  if (debug_print_lebel >= 3) Log_Info(fmt, ##__VA_ARGS__)
#define DI_BUF(data,len) if (debug_print_lebel >= 3) {\
	for(int i=0; i<len; i++) {\
		Log_Info("%02X ",*((unsigned char *)data+i));\
	}\
}

#else
#define DI(fmt, ...)
#define DDI(fmt, ...)
#define DI_BUF(...)
#endif



#define ERROR_PRINT_ENABLE
#ifdef ERROR_PRINT_ENABLE
#define DE(fmt, ...)  Log_Error("\r\n[GATT][ERROR]%s::[%d] " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define DDE(fmt, ...)  Log_Error(fmt, ##__VA_ARGS__)
#else
#define DE(fmt, ...)
#define DDE(fmt, ...)
#endif // ERROR_PRINT_ENABLE




#define TRACE_PRINT_ENABLE
#ifdef TRACE_PRINT_ENABLE
#define DT(fmt, ...)  if (debug_print_lebel >= 6) Log_Info("\r\n[GATT][TRACE]%s::[%d] " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define DDT(fmt, ...)  if (debug_print_lebel >= 6) Log_Info(fmt, ##__VA_ARGS__)
#else
#define DT(fmt, ...)
#define DDT(fmt, ...)
#endif

#define LOG_PRINT_ENABLE
#ifdef LOG_PRINT_ENABLE
#define DL(lv, fmt, ...)  if (debug_print_lebel >= lv) Log_Info("\r\n[GATT][INFO]%s::[%d] " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define DDL(lv, fmt, ...)  if (debug_print_lebel >= lv) Log_Info(fmt, ##__VA_ARGS__)
#define DL_BUF(lv, data,len) if (debug_print_lebel >= lv) {\
	for(int i=0; i<len; i++) {\
		Log_Info("0x%02X ",*((unsigned char *)data+i));\
	}\
}

#else
#define DL(lv, fmt, ...)
#define DDL(lv, fmt, ...)
#define D_BUF(...)
#endif

#else

//#define DEBUG_PRINT_ENABLE
#ifdef DEBUG_PRINT_ENABLE
#define D(fmt, ...)  Log_Info("\r\n[GATT][DEBUG]%s::[%d] " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define DD(fmt, ...)  Log_Info(fmt, ##__VA_ARGS__)
#define D_BUF(data,len) {\
	for(int i=0; i<len; i++) {\
		DD("0x%02X ",*((unsigned char *)data+i));\
	}\
}

#else
#define D(fmt, ...)
#define DD(fmt, ...)
#define D_BUF(...)
#endif

#define INFO_PRINT_ENABLE
#ifdef INFO_PRINT_ENABLE
#define DI(fmt, ...)  Log_Info("\r\n[GATT][INFO]%s::[%d] " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define DDI(fmt, ...)  Log_Info(fmt, ##__VA_ARGS__)
#define DI_BUF(data,len) {\
	for(int i=0; i<len; i++) {\
		Log_Info("0x%02X ",*((unsigned char *)data+i));\
	}\
}

#else
#define DI(fmt, ...)
#define DDI(fmt, ...)
#define DI_BUF(...)
#endif

#define TRACE_PRINT_ENABLE
#ifdef TRACE_PRINT_ENABLE
#define DT(fmt, ...)  Log_Info("\r\n[GATT][TRACE]%s::[%d] " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define DDT(fmt, ...)  Log_Info(fmt, ##__VA_ARGS__)
#else
#define DT(fmt, ...)
#define DDT(fmt, ...)
#endif


#endif



static char *ble_io_status_string[] = {
	"SERVER_IO_STATUS_OFF",
	"SERVER_IO_STATUS_BUSY",
	"SERVER_IO_STATUS_READY",
	"SERVER_IO_STATUS_NOT_READY",
	"SERVER_IO_STATUS_WRITE",
	"SERVER_IO_STATUS_WRITE_COMPLETE",
	"SERVER_IO_STATUS_READ",
	"SERVER_IO_STATUS_READ_COMPLETE",
	"SERVER_IO_STATUS_DATA_UPLOAD",
	"SERVER_IO_STATUS_DATA_UPLOAD_COMPLETE",
	"SERVER_IO_STATUS_DATA_UPLOAD_RESP_WAIT",
	"SERVER_IO_STATUS_DATA_UPLOAD_RESP_RECV",
	"SERVER_IO_STATUS_DATA_UPLOAD_RESP_COMP",
	"SERVER_IO_STATUS_FW_REQ",
	"SERVER_IO_STATUS_FW_REQ_COMPLETE",
	"SERVER_IO_STATUS_DISC_BEFORE_STOP_SERVER",
};

int gatt_test_wait = 0;

int gat_send_wait_time = 1000;
int gat_send_wait_time_base = 15;
int gat_send_wait_time_write = 485;

unsigned char read_buf[512];

typedef struct {
	tBT_GattConnIF conn;
	AttrStru inc;
	AttrStru svc;
	AttrStru data_char;
	AttrStru ver_char;
	AttrStru date_char;
	AttrStru fwup_char;
	AttrStru conf_char;
	AttrStru mtu_char;
	AttrStru desc_ccc_data;
	AttrStru desc_ccc_fwup;
	AttrStru desc_ccc_conf;
	AttrStru desc_ccc_mtu;
} GattSvcStru;

typedef struct {
	AttrStru svc;
	AttrStru chr_read_write;
	AttrStru chr_write;
	AttrStru chr_notify;
	AttrStru desc_ccc_notify;
} GattIncSvcStru;

static GattSvcStru primary_svc;
static int notify_data_count = 0;

#define PREP_WRITE_BUF_SIZE 256;
struct list_head prep_write_buf_list;

typedef struct {
	struct list_head list;
	AttrStru buffer;
	tUint16 buffer_size;
} GattPrepWriteBuf;


#if 0

void *test_malloc_print(int len, int line)
{
	void *p;
	p = malloc(len);
	D("addr : 0x%x : len : %d : line %d", p, len, line);
	return p;
}
#define malloc(a) test_malloc_print(a, __LINE__)

void *test_malloc_value_stru_print(int len, int line)
{
	void *p;
	p = malloc_value_stru(len);
	D("addr : 0x%x : len : %d : line %d", p, len, line);
	return p;
}
#define malloc_value_stru(a) test_malloc_value_stru_print(a, __LINE__)

void test_free_print(void *p, int line)
{
	D("addr : %p : line =%d", (int *)p, line);
	return free(p);
}
#define free(a) test_free_print(a, __LINE__)
#endif

sys_sem_t gatt_task_wait_sem;

volatile tBle_ServerPowerStatusEnum ble_power_status = SERVER_POWER_STATUS_OFF;
volatile tBle_ServerIoStatusEnum    ble_io_status = SERVER_IO_STATUS_OFF;

tBT_Gatt_cb *connected_cb = NULL;
tBT_Gatt_cb *disconnected_cb = NULL;

extern u8 *bdaddr_to_str(u8 *str, u8 *arr);
extern u8 hex_str_to_bd_addr(u32 str_len, s8 *str, u8 *num_arr);

#if CONFIG_AUDREY_REDUCE_MALLOC
#define MAX_MTU_SIZE							512			/* MTUの最大値 */

/* Data Transport Characterisitic関連定義/宣言 */
#define MAX_DATA_UPLOAD_LEN						5120		/* 受信する応答データの最大長 */
#define DATA_UPLOAD_DATASIZE_LEN				4			/* 応答データのデータサイズ部データ長*/
tUint8  data_upload_resp_buf[MAX_DATA_UPLOAD_LEN + 1];		/* 応答データの受信バッファ */
tUint32 data_upload_resp_len					= 0;		/* 応答データのデータ長 */
tUint32 resp_rcv_len							= 0;		/* 受信した応答データの累積サイズ */

/* Configuration Characteristic関連定義/宣言 */
#define MAX_CONF_DATA_LEN						2048		/* 受信するConfigurationデータの最大長 */
#define CONF_DATA_DATASIZE_LEN					4			/* Configuraitonデータのデータサイズ部データ長*/
tUint8 	conf_data_recv_buf[MAX_CONF_DATA_LEN + 1];			/* Configurationデータの受信バッファ */
tUint32 conf_data_recv_len						= 0;		/* Configurationデータのデータ長 */
tUint32 conf_data_recv_total_len				= 0;		/* 受信したConfigurationデータの累積サイズ */
static bool conf_data_ind_comp					= FALSE;	/* Configurationデータの送信完了フラグ */

#else //#if CONFIG_AUDREY_REDUCE_MALLOC
/* Data Transport Characterisitic関連定義/宣言 */
#define DATA_UPLOAD_DATASIZE_LEN				4			/* データ送信処理 データサイズ部データ長*/
#define DATA_UPLOAD_MTU_MAX						256			/* データ送信処理 MTU上限 */
tUint8 *data_upload_resp_buf					= NULL;		/* データ送信レスポンス受信バッファ用ポインタ */
tUint32 data_upload_resp_len					= 0;		/* レスポンスデータ長 */
tUint32 resp_rcv_len							= 0;		/* レスポンス受信累積サイズ */

/* Configuration Characteristic関連定義/宣言 */
#define CONF_DATA_DATASIZE_LEN					4			/* データサイズ部のデータ長*/
tUint8 *conf_data_recv_buf						= NULL;		/* Configurationデータの受信バッファ用ポインタ */
tUint32 conf_data_recv_len						= 0;		/* Configurationデータのデータ長 */
tUint32 conf_data_recv_total_len				= 0;		/* 受信データの累積サイズ */
static bool conf_data_ind_comp					= FALSE;	/* Configurationデータの送信完了フラグ */
#endif //#if CONFIG_AUDREY_REDUCE_MALLOC

/* MTU information Characterisitic関連定義/宣言 */
#define MTU_INFO_LEN							2			/* MTU情報のデータ長 */
#define MTU_DEFAULT_VALUE						0x17		/* MTUのデフォルト値 (23バイト) */

#if CONFIG_AUDREY_ALWAYS_BT_ON
static bool adv_disable_flg						= FALSE;	/* Advertising無効化フラグ */
#endif //#if CONFIG_AUDREY_ALWAYS_BT_ON

/* NotifyTask関連定義/宣言 */
#if CONFIG_AUDREY_REDUCE_MALLOC
static char		notify_task_istr[MAX_MTU_SIZE + 1];			/* 送信処理用バッファ */
#endif //#if CONFIG_AUDREY_REDUCE_MALLOC
static char		*notify_task_data				= NULL;
static char		*notify_conf_data				= NULL;
static bool		notify_task_running				= 0;
static tUint32	notify_task_len					= 0;
static int		notify_task_slen				= 0;
static tUint32	notify_conf_data_len			= 0;
static char		notify_mtu_data[2]				= {0, 0};
xTaskHandle		g_BT_notify_task				= NULL;

// timer
TimerHandle_t	ble_timer_id_write_cb;
static int     ble_timer_id_write_cb_timer_cleate = 0;

tBT_GattsWriteIndStru *write_cb_param = NULL;
tBT_GattResEnum write_cb_res;

static void TimerComp_Rsp(void)
{
	if (ble_timer_id_write_cb) {
		D(" timer stop\r\n");
		xTimerStop(ble_timer_id_write_cb, 0);
		if (write_cb_param) {
			BT_GATTS_WriteRsp(write_cb_param->conn, write_cb_res);
			DI("BT_GATTS_WriteRsp(timer)");
		}
	}
}


static void TimerStart_Rsp(int t)
{
	xTimerStop(ble_timer_id_write_cb, 0);
	if (!ble_timer_id_write_cb_timer_cleate) {
		DI("cleate timer\r\n");
		ble_timer_id_write_cb = xTimerCreate("timer_wcbRsp",
				(t), // 500ms
				pdTRUE,
				(void *)0,
				(TimerCallbackFunction_t)TimerComp_Rsp);
		ble_timer_id_write_cb_timer_cleate = 1;
	}
	xTimerStart(ble_timer_id_write_cb, 0);
	DI("timer start 0x%x time=%d", ble_timer_id_write_cb, t);
}

static void adv_on_cb(tBT_ResultEnum result, void *param)
{
	if ( result == BT_RES_OK) {
		DI("[bt_gatt_server] Start broadcasting ADV packets\r\n");
	}
	else {
		Log_Error("\r\n[Error] [bt_gatt_server] [adv_on_cb] error code (tBT_ResultEnum): %x\r\n",result);
	}

}

void svr_set_adv_data(void)
{
	tBT_EirDataStru adv_data;
	tBT_ManufacturerDataStru *manu_data;
	tUint8 data[14];
	tUint8 br_bd[6];
	tUint8 ble_bd[6];

	/* BDアドレス情報取得 */
	BT_GAP_LocalBDGet(br_bd,ble_bd);
	/* manufacture dataのデータ部作成 */
	data[0]  = 0x02;			/* Service ID: 0x8002 (LSB first) */
	data[1]  = 0x80;
	data[2]  = audrey_mac[0];	/* MAC address ("audrey_mac" は MSB first)*/
	data[3]  = audrey_mac[1];
	data[4]  = audrey_mac[2];
	data[5]  = audrey_mac[3];
	data[6]  = audrey_mac[4];
	data[7]  = audrey_mac[5];
	data[8]  = ble_bd[5];		/* BD Address ("ble_bd" は LSB first) */
	data[9]  = ble_bd[4];
	data[10] = ble_bd[3];
	data[11] = ble_bd[2];
	data[12] = ble_bd[1];
	data[13] = ble_bd[0];

	memset(&adv_data,0,sizeof(tBT_EirDataStru));
	manu_data = &(adv_data.manufacturer_data);
	manu_data->data_len = sizeof(data);
	manu_data->company_id[0] = 0x81;	/* Company ID: 0x0381 (LSB first) */
	manu_data->company_id[1] = 0x03;
	manu_data->data = data;
	strcpy(adv_data.dev_name, GATT_SERVER_ADV_NAME_STR);
	adv_data.mask = BT_GAP_BLE_ADVDATA_MBIT_NAME | BT_GAP_BLE_ADVDATA_MBIT_MENUDATA;

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
#if CONFIG_AUDREY_ALWAYS_BT_ON
	if (!adv_disable_flg) {
	BT_GAP_BleAdvertisingSet(TRUE, adv_on_cb);
	}
#else
	BT_GAP_BleAdvertisingSet(TRUE, adv_on_cb);
#endif //#if CONFIG_AUDREY_ALWAYS_BT_ON
	DI("[bt_gatt_server] Device name: %s\n\r",adv_data.dev_name);
}

static void init_buffers(void)
{
#if CONFIG_AUDREY_REDUCE_MALLOC
	memset(data_upload_resp_buf,0, 1);
	memset(conf_data_recv_buf, 0, 1);
	notify_task_data = NULL;
	notify_conf_data = NULL;
#else //#if CONFIG_AUDREY_REDUCE_MALLOC
	if (data_upload_resp_buf)
		free(data_upload_resp_buf);
	if (conf_data_recv_buf)
		free(conf_data_recv_buf);
	if (notify_task_data)
		free(notify_task_data);
	if (notify_conf_data)
		free(notify_conf_data);
#endif //#if CONFIG_AUDREY_REDUCE_MALLOC

	data_upload_resp_len = 0;
	resp_rcv_len = 0;
	conf_data_recv_len = 0;
	conf_data_recv_total_len = 0;
	conf_data_ind_comp = FALSE;
	notify_task_len = 0;
	notify_task_slen = 0;
	notify_conf_data_len = 0;
}

/*--------------------------------------*/
/*            Event Handlers            */
/*--------------------------------------*/
static volatile bool fReconnect = TRUE;	/* 切断後接続待ち実施フラグ (default:ON  再接続したくないときのみOFFにする) */
static void conn_complete_cb(tBT_GattConnCmplStru *param)
{
	if (param->res== BT_RES_OK) {
		DI("\r\n[bt_gatt_server] Bluetooth Connection Established (GATT Server)\r\n");

		BT_ConnectionStatus* connStatus = NULL;
		tUint8 mtu_val[2] = {0, MTU_DEFAULT_VALUE};

		connStatus = get_conn_status(CONN_GATT,(void *)param->conn);
		connStatus->tick_Conn = xTaskGetTickCount();
		primary_svc.conn = param->conn;
		bt_peer_conn_if = param->conn;
		notify_data_count = 0;
		fReconnect = TRUE;
		memcpy(primary_svc.mtu_char.val->data, mtu_val, 2);

		ble_io_status = SERVER_IO_STATUS_READY;
		D("SERVER_IO_STATUS_READY");
		if (connected_cb) {
			connected_cb(ble_io_status);
		} else {
			rf_ctrl_ble_conn_complete_cb(param);
		}
	}
	else {
		Log_Error("[Error] [bt_gatt_server] Connect Error, error code (tBT_ResultEnum): %x\r\n",param->res);
	}
}

static void disconn_complete_cb(tBT_GattDisconnCmplStru *param)
{
	DI("\r\n[bt_gatt_server] Bluetooth Connection Disconnected (GATT Server)\r\n");

	BT_ConnectionStatus* connStatus = NULL;
	connStatus = get_conn_status(CONN_GATT,(void *)param->conn);
	connStatus->tick_Disconn = xTaskGetTickCount();
	D("[bt_gatt_server] Bluetooth Connection time: %u secs \r\n", (connStatus->tick_Disconn-connStatus->tick_Conn)/1000);

	connStatus->connIF.gatt= NULL;
	primary_svc.conn = NULL;
	bt_peer_conn_if = NULL;

	ble_io_status = SERVER_IO_STATUS_NOT_READY;

	//  CCCD init value must be 0, so reset CCCD value after disconnected
	memset(primary_svc.desc_ccc_data.val->data,0,2);
	memset(primary_svc.desc_ccc_fwup.val->data,0,2);
	memset(primary_svc.desc_ccc_conf.val->data,0,2);
	memset(primary_svc.desc_ccc_mtu.val->data,0,2);

	init_buffers();

	if (fReconnect == TRUE) {
		BT_GAP_BleAdvertisingSet(TRUE, adv_on_cb);
	}

	if (disconnected_cb) {
		disconnected_cb(ble_io_status);
	} else {
		rf_ctrl_ble_disconn_complete_cb(param);
	}
}

static void mtu_cb(tBT_GattMtuNotifStru *param)
{
	BT_ConnectionStatus* connStatus = NULL;
	
	DI("\r\n[bt_gatt_server] MTU: %d\r\n", param->mtu);
	connStatus = get_conn_status(CONN_GATT,(void *)param->conn);
	connStatus->mtu = param->mtu;

	notify_mtu_data[0] = (tUint8)((param->mtu & 0xFF00) >> 8);
	notify_mtu_data[1] = (tUint8) (param->mtu & 0x00FF);
	/* MTU Information CharacterisiticのValue更新 */
	memcpy(primary_svc.mtu_char.val->data, notify_mtu_data, MTU_INFO_LEN);
}

static void svc_start_cb(tBT_GattSvcStartStru *param)
{
	if (param->res != BT_GATT_OK) {
		Log_Error("\r\n[bt_gatt_server] [svc_start_cb] Error, error code (tBT_ResultEnum): %x\r\n",param->res);
		return;
	}
	if (param->svc == primary_svc.svc.attr_if) {
		primary_svc.svc.attr_hdl = param->attr_hdl;
		DI("[svc_start_cb] Primary_service started\r\n");
	}
	else {
		Log_Error("[Warning] [bt_gatt_server] [svc_start_cb] Unhandled service started, attr_hdl=%x, end_hdl=%x\r\n",param->attr_hdl,param->end_hdl);
	}
}

static void svc_stop_cb(tBT_GattSvcStopStru *param)
{
	if (param->res != BT_GATT_OK) {
		D("\r\n[Error] [bt_gatt_server] [svc_stop_cb] error code (tBT_ResultEnum): %x\r\n",param->res);
		return;
	}
	if (param->svc == primary_svc.svc.attr_if) {
		DI("[svc_stop_cb] Primary_service stopped\r\n");
	}
	else {
		Log_Error("[svc_stop_cb] Invalid service stopped\r\n");
	}

}

static void inc_start_cb(tBT_GattIncStartStru *param)
{
	if (param->res == BT_GATT_OK) {
		if (param->inc == primary_svc.inc.attr_if) {
			primary_svc.inc.attr_hdl = param->attr_hdl;
			D("[inc_start_cb] Service included successfully \r\n");
		}
	}
	else {
		Log_Error("\r\n[Error] [bt_gatt_server] [inc_start_cb] error code (tBT_ResultEnum): %x\r\n",param->res);
		return;
	}
}

static void chr_start_cb(tBT_GattCharStartStru *param)
{
	if (param->res == BT_GATT_OK) {
		if (param->chr == primary_svc.data_char.attr_if) {
			primary_svc.data_char.attr_hdl = param->attr_hdl;
			D("[chr_start_cb] data_char started\r\n");
		}
		else if (param->chr == primary_svc.ver_char.attr_if) {
			primary_svc.ver_char.attr_hdl = param->attr_hdl;
			D("[chr_start_cb] ver_char started\r\n");
		}
		else if (param->chr == primary_svc.date_char.attr_if) {
			primary_svc.date_char.attr_hdl = param->attr_hdl;
			D("[chr_start_cb] date_char started\r\n");
		}
		else if (param->chr == primary_svc.fwup_char.attr_if) {
			primary_svc.fwup_char.attr_hdl = param->attr_hdl;
			D("[chr_start_cb] fwup_char started\r\n");
		}
		else if (param->chr == primary_svc.conf_char.attr_if) {
			primary_svc.conf_char.attr_hdl = param->attr_hdl;
			D("[chr_start_cb] conf_char started\r\n");
		}
		else if (param->chr == primary_svc.mtu_char.attr_if) {
			primary_svc.mtu_char.attr_hdl = param->attr_hdl;
			D("[chr_start_cb] mtu_char started\r\n");
		}
		else {
			Log_Error("[Warning] [bt_gatt_server] [chr_start_cb] Unhandled characteristic started, attr_hdl=%x, val_hdl=%x\r\n",param->attr_hdl,param->val_hdl);
		}
	}
	else {
		Log_Error("[Error] [bt_gatt_server] [chr_start_cb] error code (tBT_ResultEnum): %x\r\n",param->res);
	}
}


static void notify_task(void)
{
#if CONFIG_AUDREY_REDUCE_MALLOC != 1
	char istr[DATA_UPLOAD_MTU_MAX];
#endif //#if CONFIG_AUDREY_REDUCE_MALLOC != 1

	int  max_len = 0;
	int  send_len = 0;
	tUint16 *cccd = NULL;
	AttrStru *attr_stru = NULL;
	AttrStru *ccc_stru  = NULL;

	DI("\n\r[%s] start\n\r", __FUNCTION__);
	D("\r\n[bt_gatt_server] notify_task running...\r\n");
	notify_task_running = 1;
	while(notify_task_running) {

		/* MTU情報通知用データの確認 */
		if ((notify_mtu_data[0] != 0) || (notify_mtu_data[1] !=0)) {
			/* CCCのハンドラのNULLチェック */
			if ( primary_svc.desc_ccc_mtu.attr_hdl != 0) {
				cccd = (tUint16 *) primary_svc.desc_ccc_mtu.val->data;
				/* Notificationが有効となっていることの確認 */
				if ((*cccd) == BT_GATT_CCC_VAL_NOTIFICATION) {
#if CONFIG_AUDREY_REDUCE_MALLOC
					/* 送信するデータの準備 */
					memcpy(notify_task_istr, notify_mtu_data, MTU_INFO_LEN);
					/* データ送信 */
					BT_GATTS_ChrValueSend(primary_svc.conn, primary_svc.mtu_char.attr_if, notify_task_istr, MTU_INFO_LEN);
#else //#if CONFIG_AUDREY_REDUCE_MALLOC
					/* 送信するデータの準備 */
					memcpy(istr, notify_mtu_data, MTU_INFO_LEN);
					/* データ送信 */
					BT_GATTS_ChrValueSend(primary_svc.conn, primary_svc.mtu_char.attr_if, istr, MTU_INFO_LEN);
#endif //#if CONFIG_AUDREY_REDUCE_MALLOC
					vTaskDelay(gat_send_wait_time_write);
					/* 送信バッファをクリア */
					notify_mtu_data[0] = 0;
					notify_mtu_data[1] = 0;
				}
			}
		} // if ((notify_mtu_data[0] != 0) || (notify_mtu_data[1] !=0))

		/* Configuration用データの確認 */
		if (0 < notify_conf_data_len) {
			/* CCCのハンドラのNULLチェック */
			if ( primary_svc.desc_ccc_conf.attr_hdl != 0) {
				cccd = (tUint16 *) primary_svc.desc_ccc_conf.val->data;
				/* Indicationが有効となっていることのチェック */
				if ((*cccd) == BT_GATT_CCC_VAL_INDICATION) {
					/* Indicationで通知するデータのNULLチェック */
					if ((notify_conf_data != NULL) && (0 < notify_conf_data_len)) {
						/* 送信データサイズの設定 */
						int send_conf_data_len;
						max_len = BT_GATT_MtuGet(primary_svc.conn) - 3;
						send_conf_data_len = (max_len < notify_conf_data_len) ? max_len : notify_conf_data_len;
#if CONFIG_AUDREY_REDUCE_MALLOC
						if (send_conf_data_len > MAX_MTU_SIZE) {
							send_conf_data_len = MAX_MTU_SIZE;
						}
						/* 今回送信するデータの準備 */
						memcpy(notify_task_istr, notify_conf_data, send_conf_data_len);
#else //#if CONFIG_AUDREY_REDUCE_MALLOC
						if (send_conf_data_len > DATA_UPLOAD_MTU_MAX) {
							send_conf_data_len = DATA_UPLOAD_MTU_MAX;
						}
						/* 今回送信するデータの準備 */
						memcpy(istr, notify_conf_data, send_conf_data_len);
#endif //#if CONFIG_AUDREY_REDUCE_MALLOC
						/* 送信データの残存確認 */
						notify_conf_data_len -= send_conf_data_len;
						/* 残データが有る場合 */
						if (0 < notify_conf_data_len) {
							/* 次回送信するデータを準備 */
							notify_conf_data += send_conf_data_len;
						}
						/* 残データが無い場合 */
						else if (notify_conf_data_len == 0) {
							/* Configurationデータ送信完了フラグにTRUEをセット */
							conf_data_ind_comp = TRUE;
							/* バッファのポインタをクリア */
							notify_conf_data = NULL;
						}
						else {
							Log_Error("\n\r[%d] error: notify conf data length become under 0\r\n", __LINE__);
							notify_conf_data_len = 0;
							notify_conf_data = NULL;
						}
						/* データ送信 */
#if CONFIG_AUDREY_REDUCE_MALLOC
						BT_GATTS_ChrValueSend(primary_svc.conn, primary_svc.conf_char.attr_if, notify_task_istr, send_conf_data_len);
#else //#if CONFIG_AUDREY_REDUCE_MALLOC
						BT_GATTS_ChrValueSend(primary_svc.conn, primary_svc.conf_char.attr_if, istr, send_conf_data_len);
#endif //#if CONFIG_AUDREY_REDUCE_MALLOC
						vTaskDelay(gat_send_wait_time_write);
						continue;
					}
				}
			}
		} // if (0 < notify_conf_data_len)

		/* Data Transport用データの確認 */
		if (ble_io_status == SERVER_IO_STATUS_DATA_UPLOAD) {
			attr_stru = &(primary_svc.data_char);
			ccc_stru  = &(primary_svc.desc_ccc_data);
			if ((attr_stru != NULL) && (ccc_stru != NULL)) {
				cccd = (tUint16 *) ccc_stru->val->data;
				if ((*cccd & BT_GATT_CCC_VAL_INDICATION) == BT_GATT_CCC_VAL_INDICATION) {
					if ((notify_task_data != NULL) && (notify_task_len > 0)) {
						max_len = BT_GATT_MtuGet(primary_svc.conn) - 3;
						send_len = (notify_task_len > max_len) ? max_len : notify_task_len;
#if CONFIG_AUDREY_REDUCE_MALLOC
						if (send_len > MAX_MTU_SIZE) {
							send_len = MAX_MTU_SIZE;
						}
						memcpy(notify_task_istr, notify_task_data, send_len);
#else // #if CONFIG_AUDREY_REDUCE_MALLOC
						if (send_len > DATA_UPLOAD_MTU_MAX) {
							send_len = DATA_UPLOAD_MTU_MAX;
						}
						memcpy( istr, notify_task_data, send_len);
#endif //#if CONFIG_AUDREY_REDUCE_MALLOC
						notify_task_data += send_len;
						notify_task_len -= send_len;
						if (notify_task_len <= 0) {
							if (ble_io_status == SERVER_IO_STATUS_DATA_UPLOAD) {
								ble_io_status = SERVER_IO_STATUS_DATA_UPLOAD_COMPLETE;
								Log_Info(" SERVER_IO_STATUS_DATA_UPLOAD_COMPLETE");
							} else if (ble_io_status == SERVER_IO_STATUS_FW_REQ) {
								ble_io_status = SERVER_IO_STATUS_FW_REQ_COMPLETE;
								Log_Info(" SERVER_IO_STATUS_FW_REQ_COMPLETE");
							}
						}
						DI("\r\n[bt_gatt_server] data_char send : %d byte(s)\r\n", send_len);
#if CONFIG_AUDREY_REDUCE_MALLOC
						BT_GATTS_ChrValueSend(primary_svc.conn, attr_stru->attr_if, notify_task_istr, send_len);
#else //#if CONFIG_AUDREY_REDUCE_MALLOC
						BT_GATTS_ChrValueSend(primary_svc.conn, attr_stru->attr_if,istr,send_len);
#endif //#if CONFIG_AUDREY_REDUCE_MALLOC
						vTaskDelay(gat_send_wait_time_write);
					}
					notify_data_count++;
				}
			}
			continue;
		} // if (ble_io_status == SERVER_IO_STATUS_DATA_UPLOAD)

		/* Firmware Update用データの確認 */
		else if (ble_io_status == SERVER_IO_STATUS_WRITE) {
			/* CCCのハンドラチェック */
			if ( primary_svc.desc_ccc_fwup.attr_hdl != 0) {
				cccd = (tUint16 *) primary_svc.desc_ccc_fwup.val->data;
				/* Indicationが有効となっていることのチェック */
				if ((*cccd) == BT_GATT_CCC_VAL_INDICATION) {
					if (notify_task_data != NULL && notify_task_slen > 0 && ble_io_status == SERVER_IO_STATUS_WRITE) {
#if CONFIG_AUDREY_REDUCE_MALLOC
						int send_length = MAX_MTU_SIZE;
#else //#if CONFIG_AUDREY_REDUCE_MALLOC
						int send_length = sizeof(istr);
#endif //#if CONFIG_AUDREY_REDUCE_MALLOC
						int max_len = BT_GATT_MtuGet(primary_svc.conn) - 3;
						if (send_length > max_len) {
							send_length = max_len;
						}
						if (send_length > notify_task_slen) {
							send_length = notify_task_slen;
						}
#if CONFIG_AUDREY_REDUCE_MALLOC
						memcpy(notify_task_istr, notify_task_data, send_length);
#else //#if CONFIG_AUDREY_REDUCE_MALLOC
						memcpy(istr,notify_task_data, send_length);
						istr[send_length] = 0;
#endif //#if CONFIG_AUDREY_REDUCE_MALLOC
						notify_task_data += send_length; // sizeof(istr);
						notify_task_slen -= send_length;
						if (notify_task_slen <= 0) {
							notify_task_slen = 0;
							ble_io_status = SERVER_IO_STATUS_WRITE_COMPLETE;
							DI(" SERVER_IO_STATUS_WRITE_COMPLETE");
						}
						DI("BT_GATTS_ChrValueSend[Mtu:%d] fwup_char: len=%d",max_len, send_length);
#if CONFIG_AUDREY_REDUCE_MALLOC
						BT_GATTS_ChrValueSend(primary_svc.conn, primary_svc.fwup_char.attr_if, notify_task_istr, send_length);
#else //#if CONFIG_AUDREY_REDUCE_MALLOC
						D("send");  D("["); D_BUF(istr,send_length); D("]\r\n");
						BT_GATTS_ChrValueSend(primary_svc.conn, primary_svc.fwup_char.attr_if,istr,send_length);
#endif //#if CONFIG_AUDREY_REDUCE_MALLOC
						vTaskDelay(gat_send_wait_time_write);
					}
					vTaskDelay(15);
				}
			}
			vTaskDelay(gat_send_wait_time_base);
		} // if (ble_io_status == SERVER_IO_STATUS_WRITE)

		else {
			vTaskDelay(1000);
			DD("-n-");
		}
	}
	DI("\n\r[%s] task delete \n\r", __FUNCTION__);	
	g_BT_notify_task = NULL;
	vTaskDelete(NULL);
	DI("\n\r[%s] end\n\r", __FUNCTION__);
}

static void indication_cb(tBT_GattsIndValueCfmStru *param)
{
	switch (ble_io_status) {
		case SERVER_IO_STATUS_DATA_UPLOAD:
			DI("[indication_cb][upload] %d byte(s) data to indicate remaining\r\n", notify_task_len);
			break;
		case SERVER_IO_STATUS_DATA_UPLOAD_COMPLETE:
			DI("[indication_cb][upload] all data sent\r\n");
			notify_task_data = NULL;
			notify_task_len = 0;
			ble_io_status = SERVER_IO_STATUS_DATA_UPLOAD_RESP_WAIT;
			resp_rcv_len = 0;
			break;
		case SERVER_IO_STATUS_FW_REQ:
			DI("[indication_cb][FW_Req] %d byte(s) data to indicate remaining\r\n", notify_task_slen);
			break;
		case SERVER_IO_STATUS_FW_REQ_COMPLETE:
			DI("[indication_cb][FW_Req] all data sent\r\n");
			//notify_task_data = NULL;
			//notify_task_slen = 0;
			//ble_io_status = SERVER_IO_STATUS_READY;
			/* [ToDo] OTA側の通知用関数呼び出し */
			break;
		default:
			D(" default: ble_io_status=%d [%s]", ble_io_status, ble_io_status_string[ble_io_status]);
			break;
	}

	/* 送信予定のConfigurationデータが残っている場合 */
	if (0 < notify_conf_data_len) {
		DI("[indication_cb][Conf] %d byte(s) data to indicate remaining\r\n", notify_conf_data_len);
	}
	/* Configurationデータの送信が完了した場合 */
	else if (conf_data_ind_comp) {
		DI("[indication_cb][Conf] all data sent\r\n");
		rf_ctrl_conf_data_send_comp_cb();
		conf_data_ind_comp = FALSE;
	}

	return;
}


void delete_notify_task(void)
{
	D("\r\nATBs notify_task delete start\r\n");
	vTaskDelete(g_BT_notify_task);
	D("\r\nATBs notify_task delete end\r\n");
	g_BT_notify_task = NULL;
}

static void desc_start_cb(tBT_GattDescStartStru *param)
{
	if (param->res == BT_GATT_OK) {
		if (param->des == primary_svc.desc_ccc_data.attr_if) {
			primary_svc.desc_ccc_data.attr_hdl = param->attr_hdl;
			D("[desc_start_cb] desc_ccc_data started\r\n");
			if (!g_BT_notify_task) {
				if(xTaskCreate((TaskFunction_t)notify_task, (char const *)"notify", 2048, NULL, tskIDLE_PRIORITY + 1, &g_BT_notify_task) != pdPASS){
					Log_Error("\n\r[%s] Create notify task failed", __FUNCTION__);
				}
			}
		}
		else if (param->des == primary_svc.desc_ccc_fwup.attr_if) {
			primary_svc.desc_ccc_fwup.attr_hdl = param->attr_hdl;
			D("[desc_start_cb] desc_ccc_fwup started\r\n");
			if (!g_BT_notify_task) {
				if(xTaskCreate((TaskFunction_t)notify_task, (char const *)"notify", 2048, NULL, tskIDLE_PRIORITY + 1, &g_BT_notify_task) != pdPASS){
					Log_Error("\n\r[%s] Create notify task failed", __FUNCTION__);
				}
			}
		}
		else if (param->des == primary_svc.desc_ccc_conf.attr_if) {
			primary_svc.desc_ccc_conf.attr_hdl = param->attr_hdl;
			D("[desc_start_cb] desc_ccc_conf started\r\n");
			if (!g_BT_notify_task) {
				if(xTaskCreate((TaskFunction_t)notify_task, (char const *)"notify", 2048, NULL, tskIDLE_PRIORITY + 1, &g_BT_notify_task) != pdPASS){
					Log_Error("\n\r[%s] Create notify task failed", __FUNCTION__);
				}
			}
		}
		else if (param->des == primary_svc.desc_ccc_mtu.attr_if) {
			primary_svc.desc_ccc_mtu.attr_hdl = param->attr_hdl;
			D("[desc_start_cb] desc_ccc_mtu started\r\n");
			if (!g_BT_notify_task) {
				if(xTaskCreate((TaskFunction_t)notify_task, (char const *)"notify", 2048, NULL, tskIDLE_PRIORITY + 1, &g_BT_notify_task) != pdPASS){
					Log_Error("\n\r[%s] Create notify task failed", __FUNCTION__);
				}
			}
		}
		else {
			Log_Error("[Warning] [bt_gatt_server] [desc_start_cb] Unhandled descriptor started, attr_hdl=%x\r\n",param->attr_hdl);
		}
	}
	else {
		Log_Error("[Error] [bt_gatt_server] [desc_start_cb] error code (tBT_ResultEnum): %x\r\n",param->res);
	}
}

static char *attr_type_string[] = {
	"BT_GATT_ATTR_TYPE_SVC_PRIMARY",
	"BT_GATT_ATTR_TYPE_SVC_SECONDARY",
	"BT_GATT_ATTR_TYPE_INCLUDE",
	"BT_GATT_ATTR_TYPE_CHARACTERISTIC",
	"BT_GATT_ATTR_TYPE_CHR_DESCRIPTOR",
};

#define READ_BUF_LENGTH 1024
unsigned char ble_read_buf[READ_BUF_LENGTH];
int ble_read_len = 0;
int ble_read_len_max = 0;

static void read_cb(tBT_GattsReadIndStru *param)
{
	tUint8 data[23] = {0};
	int max_len;
	int send_len;
	tBT_ValueStru *val = NULL;

	D(" [handle=%d] attr_type[%d : %s]",param->att_hdl, param->attr_type, attr_type_string[param->attr_type]);

	switch (param->attr_type) {
		case BT_GATT_ATTR_TYPE_CHARACTERISTIC:
			if (param->att_if == primary_svc.ver_char.attr_if) {
				DI(" primary_svc.ver_char.attr_if");
				val = primary_svc.ver_char.val;
			}
			else if (param->att_if == primary_svc.mtu_char.attr_if) {
				DI(" primary_svc.mtu_char.attr_if");
				val = primary_svc.mtu_char.val;
			}
			else {
				sprintf(data,"Unhandled attr_type: %x", param->attr_type);
				Log_Error("[Warning] [bt_gatt_server] [read_cb] Unhandled attr_type = %x\r\n", param->attr_type);
				BT_GATTS_ReadRsp(param->conn, BT_GATT_OK, data, sizeof(data));
				break;
			}

			max_len = BT_GATT_MtuGet(primary_svc.conn) - 1;
			send_len = (val->len-param->off > max_len) ? max_len: val->len-param->off;

			DI("[offset=%d] len=%d [%.022s] ( ", param->off,send_len,val->data+param->off);
			DI_BUF(val->data+param->off,send_len);
			DDI(")\n\r");

			BT_GATTS_ReadRsp(param->conn, BT_GATT_OK, val->data+param->off,send_len);

			if (ble_io_status == SERVER_IO_STATUS_WRITE) {
				param->off += send_len;
				if (val->len-param->off <= 0) {
					val->len = 0;
					ble_io_status = SERVER_IO_STATUS_WRITE_COMPLETE;
					DI("SERVER_IO_STATUS_WRITE_COMPLETE");
				}
			}
			break;
		case BT_GATT_ATTR_TYPE_CHR_DESCRIPTOR:
			if (param->att_if == primary_svc.desc_ccc_data.attr_if) {
				/* Characteristic User Description */
				val = primary_svc.desc_ccc_data.val;
				DI("desc_ccc_data [len=%d]: %s\r\n",val->len,val->data);
				if (ble_io_status == SERVER_IO_STATUS_READ) {
					memcpy(&ble_read_buf[ble_read_len], val->data, val->len);
					ble_read_len += val->len;
					ble_io_status = SERVER_IO_STATUS_READ_COMPLETE;
				}
			}
			else if (param->att_if == primary_svc.desc_ccc_fwup.attr_if) {
				/* Characteristic User Description */
				val = primary_svc.desc_ccc_fwup.val;
				DI("desc_ccc_fwup [len=%d]: %s\r\n",val->len,val->data);
				if (ble_io_status == SERVER_IO_STATUS_READ) {
					memcpy(&ble_read_buf[ble_read_len], val->data, val->len);
					ble_read_len += val->len;
					ble_io_status = SERVER_IO_STATUS_READ_COMPLETE;
					D("desc_all_aggr_format [len=%d]: ",val->len);
					D_BUF(val->data, val->len); DD("\r\n");
				}
			}
			else if (param->att_if == primary_svc.desc_ccc_conf.attr_if) {
				/* Characteristic User Description */
				val = primary_svc.desc_ccc_conf.val;
				DI("desc_ccc_conf [len=%d]: %s\r\n",val->len,val->data);
				if (ble_io_status == SERVER_IO_STATUS_READ) {
					memcpy(&ble_read_buf[ble_read_len], val->data, val->len);
					ble_read_len += val->len;
					ble_io_status = SERVER_IO_STATUS_READ_COMPLETE;
				}
			}
			else if (param->att_if == primary_svc.desc_ccc_mtu.attr_if) {
				/* Characteristic User Description */
				val = primary_svc.desc_ccc_mtu.val;
				DI("desc_ccc_mtu [len=%d]: %s\r\n",val->len,val->data);
				if (ble_io_status == SERVER_IO_STATUS_READ) {
					memcpy(&ble_read_buf[ble_read_len], val->data, val->len);
					ble_read_len += val->len;
					ble_io_status = SERVER_IO_STATUS_READ_COMPLETE;
				}
			}
			BT_GATTS_ReadRsp(param->conn, BT_GATT_OK, val->data, val->len);
			break;
		default:
			sprintf(data,"Unhandled attr_type: %x", param->attr_type);
			Log_Error("[Warning] [bt_gatt_server] [read_cb] Unhandled attr_type = %x\r\n", param->attr_type);
			BT_GATTS_ReadRsp(param->conn, BT_GATT_OK, data, sizeof(data));
			break;
	}
}

typedef union {
	uint32_t time;
	unsigned char t[4];
} _snpt_time_t;

static int write_cb_count = 0;
static int write_cb_sum_len = 0;
static int write_cb_check_count = 0;

//static int write_cb_rsp_drop_count = 0;
int write_cb_rsp_time = 0;
static int write_cb_timer_enable = 0;

unsigned int write_cb_sum = 0;
unsigned int write_cb_len = 0;

// available buffer
#define WRITW_CB_RING_BUF_AVAIL (write_cb_ring_buf_write >= write_cb_ring_buf_read? \
		write_cb_ring_buf_write - write_cb_ring_buf_read : \
		WRITE_CB_RING_BUF_LENGTH - write_cb_ring_buf_read + write_cb_ring_buf_write)

//[TODO]

#define WRITW_CB_RING_BUF_FREE (write_cb_ring_buf_write > write_cb_ring_buf_read? \
		WRITE_CB_RING_BUF_LENGTH - write_cb_ring_buf_write + write_cb_ring_buf_read : \
		(write_cb_ring_buf_write < write_cb_ring_buf_read? \
				write_cb_ring_buf_read - write_cb_ring_buf_write : \
				WRITE_CB_RING_BUF_LENGTH ))


#define WRITE_CB_RING_BUF_LENGTH 32768
static int write_cb_ring_buf_read = 0;
static int write_cb_ring_buf_write = 0;
unsigned char write_cb_ring_buf[WRITE_CB_RING_BUF_LENGTH + 16];

void gatt_clear_write_cb_buf(void)
{
	for (int i = 0; i < WRITE_CB_RING_BUF_LENGTH; i++) {
		write_cb_ring_buf[i] = 0;
	}
	write_cb_ring_buf_read = 0;
	write_cb_ring_buf_write = 0;
}

static void write_cb_check(int left)
{
	write_cb_check_count ++;

	if (ble_read_len_max > 0
			&& write_cb_check_count > 4
			&&
			 (
			 (WRITW_CB_RING_BUF_AVAIL >= ble_read_len_max)
			 //||
			 //(write_cb_check_count > 40)
			 )
		) {
		D("write_cb_ring_buf_write=%d write_cb_ring_buf_read=%d", write_cb_ring_buf_write, write_cb_ring_buf_read);
		DI("in write_cb_check_count=%d ble_read_len_max=%d WRITW_CB_RING_BUF_AVAIL=%d left=%d",
				write_cb_check_count, ble_read_len_max,( WRITW_CB_RING_BUF_AVAIL), left);

		//[TODO] need lock
		if (write_cb_ring_buf_write > write_cb_ring_buf_read) {
			if ((write_cb_ring_buf_write - write_cb_ring_buf_read) < ble_read_len_max) {
				Log_Error("[ERROR]%s:%d program error %d %d %d", __FUNCTION__, __LINE__,
							write_cb_ring_buf_read, write_cb_ring_buf_write, ble_read_len_max);
			}

			ble_read_len = ble_read_len_max;
			DI("copy %d %d ", write_cb_ring_buf_read, ble_read_len);
			memcpy(ble_read_buf,  &write_cb_ring_buf[write_cb_ring_buf_read], ble_read_len);
			write_cb_ring_buf_read += ble_read_len;
			if (write_cb_ring_buf_write < write_cb_ring_buf_read) {
				Log_Error("[ERROR]%s:%d program error %d %d %d", __FUNCTION__, __LINE__,
								write_cb_ring_buf_read, write_cb_ring_buf_write, ble_read_len);
			}
			D("read["); D_BUF(ble_read_buf, ble_read_len); D("]\r\n");
		}
		else if (write_cb_ring_buf_write != write_cb_ring_buf_read) { // write_cb_ring_buf_write < write_cb_ring_buf_read
			DI("write_cb_ring_buf_write=%d write_cb_ring_buf_read=%d",
					write_cb_ring_buf_write, write_cb_ring_buf_read);
			ble_read_len = WRITE_CB_RING_BUF_LENGTH - write_cb_ring_buf_read;
			if (ble_read_len_max <= ble_read_len) {
				ble_read_len = ble_read_len_max;
				DI("copy %d %d ", write_cb_ring_buf_read, ble_read_len);
				memcpy(&ble_read_buf[0],  &write_cb_ring_buf[write_cb_ring_buf_read], ble_read_len);
				D("read["); D_BUF(ble_read_buf, ble_read_len); D("] len=%d\r\n", ble_read_len);
				//write_cb_ring_buf_read = 0;
				write_cb_ring_buf_read += ble_read_len;
				if (write_cb_ring_buf_read == WRITE_CB_RING_BUF_LENGTH) {
					write_cb_ring_buf_read = 0;
				}
			} else {
				//  ble_read_len < ble_read_len_max
				DI("copy %d %d ", write_cb_ring_buf_read, ble_read_len);
				memcpy(&ble_read_buf[0],  &write_cb_ring_buf[write_cb_ring_buf_read], ble_read_len);
				D("read["); D_BUF(ble_read_buf, ble_read_len); D("] len=%d\r\n", ble_read_len);
				write_cb_ring_buf_read = 0;
				if (ble_read_len < ble_read_len_max) {
					int copy_len;
					copy_len = ble_read_len_max - ble_read_len;
					if (copy_len <= write_cb_ring_buf_write) { //[TODO] check
						DI("copy %d %d ", write_cb_ring_buf_read, copy_len);
						memcpy(&ble_read_buf[ble_read_len],  &write_cb_ring_buf[0], copy_len);
						write_cb_ring_buf_read += copy_len;
					} else {
						Log_Error("[ERROR]%s:%d program error %d %d %d %d", __FUNCTION__, __LINE__,
								write_cb_ring_buf_read, write_cb_ring_buf_write, ble_read_len, copy_len);
					}
					D("read["); D_BUF(&ble_read_buf[ble_read_len], copy_len); D("] len=%d %d\r\n", ble_read_len, copy_len);
					ble_read_len += copy_len;
					if (ble_read_len > ble_read_len_max) {
						Log_Error("[ERROR]%s:%d program error %d %d %d %d", __FUNCTION__, __LINE__,
							write_cb_ring_buf_read, write_cb_ring_buf_write, ble_read_len, copy_len);
					}
				} else {
					Log_Error("[ERROR]%s:%d program error %d %d %d %d", __FUNCTION__, __LINE__,
						write_cb_ring_buf_read, write_cb_ring_buf_write, ble_read_len, ble_read_len_max);
				}
			} //
			D("read["); D_BUF(ble_read_buf, ble_read_len); D("] len=%d\r\n", ble_read_len);
			DI("write_cb_ring_buf_write=%d write_cb_ring_buf_read=%d",
					write_cb_ring_buf_write, write_cb_ring_buf_read);
		} else {
			ble_read_len = 0;
			Log_Error("[ERROR]%s program error buffer over run  %d %d", __FUNCTION__,
					write_cb_ring_buf_read, write_cb_ring_buf_write);
		}
		ble_read_len_max = 0;

		ble_io_status = SERVER_IO_STATUS_READ_COMPLETE;
		DI("read ring buf[%d] len=%d ", WRITW_CB_RING_BUF_AVAIL, ble_read_len);
		//return;
		DI("out write_cb_check_count=%d ble_read_len_max=%d WRITW_CB_RING_BUF_AVAIL=%d left=%d",
				write_cb_check_count, ble_read_len_max,( WRITW_CB_RING_BUF_AVAIL), left);
	 } else if (write_cb_check_count > (WRITE_CB_WAIT_TIME_SECOND * 2)) {
		 // [TODO] 指定されたバイト数以下の場合は捨てる
			ble_read_len = 0;
			ble_io_status = SERVER_IO_STATUS_READ_COMPLETE;
			DI("read ring buf[%d] len=%d ", WRITW_CB_RING_BUF_AVAIL, ble_read_len);
			Log_Error("\r\n[ERROR]%s time over  %d %d", __FUNCTION__,
					write_cb_ring_buf_read, write_cb_ring_buf_write);
	 }
}

volatile int ble_time_wait_count = 0;
volatile int ble_time_wait_work = 123;
void ble_time_wait(int n)
{
	int i = 0;
	for (i=0; i<n; i++) {
		ble_time_wait_work += (ble_time_wait_work + i) * 11 / 7;
	}
}

static date_char_handler(tUint8* data, tUint16 len)
{
	if (len == 4) {
#if SNTP_DATE_ENDIAN
		sntp_set_time_by_bt((time_t *)data);
		D(" time data =0x%x", *((time_t *)data));
#else
		_snpt_time_t t;
		t.t[0] = data[3];
		t.t[1] = data[2];
		t.t[2] = data[1];
		t.t[3] = data[0];
		sntp_set_time_by_bt(t.time);
		D(" time data =0x%x", t.time);
#endif
	}
	else {
		Log_Error("[ERROR]%s:%d length of time data is not match\r\n", __FUNCTION__, __LINE__);
	}
}

static transport_data_char_handler(tUint8* data, tUint16 len)
{
#if CONFIG_AUDREY_REDUCE_MALLOC
	/* 応答データ待ち状態の場合、最初の応答データとして処理 */
	if (ble_io_status == SERVER_IO_STATUS_DATA_UPLOAD_RESP_WAIT) {
		resp_rcv_len = 0;
		data_upload_resp_len = 0;
		data_upload_resp_buf[0] = '\0';
		/* 受信したデータがデータサイズ部データ長以下である場合 */
		if (len <= DATA_UPLOAD_DATASIZE_LEN) {
			Log_Error("[ERROR]%s:%d recv data length = %d\r\n", __FUNCTION__, __LINE__, len);
			/* ステータスを応答データ受信完了に遷移させる */
			ble_io_status = SERVER_IO_STATUS_DATA_UPLOAD_RESP_COMP;
		}
		else {
			/* 応答データのデータ長をセット */
			data_upload_resp_len  = data[0] << 24;
			data_upload_resp_len += data[1] << 16;
			data_upload_resp_len += data[2] << 8;
			data_upload_resp_len += data[3];
			DI("recv transport data\r\n");
			DI("transport data length: %d\r\n", data_upload_resp_len);
			/* 応答データがバッファのサイズを超える場合 */
			if (data_upload_resp_len > MAX_DATA_UPLOAD_LEN) {
				Log_Error("\r\n [ERROR] %s:%d transport data %d bytes is too long\r\n", __FUNCTION__, __LINE__, data_upload_resp_len);
				/* ステータスを応答データ受信完了に遷移させる */
				ble_io_status = SERVER_IO_STATUS_DATA_UPLOAD_RESP_COMP;
			}
			else {
				/* 受信バッファに受信したデータを格納 */
				resp_rcv_len = len - DATA_UPLOAD_DATASIZE_LEN;
				memcpy(data_upload_resp_buf, data + DATA_UPLOAD_DATASIZE_LEN, resp_rcv_len);
				DI("recv data: %d  total recv:%d\r\n", resp_rcv_len, resp_rcv_len);
				/* 1回のWriteでデータの受信が完了しなかった場合 */
				if (resp_rcv_len < data_upload_resp_len) {
					/* ステータスを応答データ受信中に遷移させ次のデータを待つ */
					ble_io_status = SERVER_IO_STATUS_DATA_UPLOAD_RESP_RECV;
				}
				/* 1回のWriteでデータの受信が完了した場合 */
				else {
					/* ステータスを応答データ受信完了に遷移 */
					ble_io_status = SERVER_IO_STATUS_DATA_UPLOAD_RESP_COMP;
				}
			}
		}
	}
	/* 応答データ受信中の場合、続きのデータとして処理 */
	else if (ble_io_status == SERVER_IO_STATUS_DATA_UPLOAD_RESP_RECV) {
		DI("recv data: %d  total recv: %d\r\n", len, resp_rcv_len + len);
		/* 受信したデータを追加するとデータ長を超える場合 */
		if (resp_rcv_len + len > data_upload_resp_len) {
			Log_Error("\r\n [ERROR] %s:%d resp_buf over flow\r\n", __FUNCTION__, __LINE__);
			/* ステータスを応答データ受信完了に遷移 */
			ble_io_status = SERVER_IO_STATUS_DATA_UPLOAD_RESP_COMP;
		}
		/* 受信したデータを追加するとデータ長以下になる場合 */
		else {
			/* 受信したデータをTransportデータ用受信バッファに追加 */
			memcpy(&data_upload_resp_buf[resp_rcv_len], data, len);
			resp_rcv_len += len;
			/* 累積サイズと応答データのデータ長が一致した場合 */
			if (resp_rcv_len == data_upload_resp_len) {
				/* ステータスを応答データ受信完了に遷移 */
				ble_io_status = SERVER_IO_STATUS_DATA_UPLOAD_RESP_COMP;
			}
		}
	}
	/* 応答データ受信完了状態の場合、余分なデータとして無視 */
	else if (ble_io_status == SERVER_IO_STATUS_DATA_UPLOAD_RESP_COMP) {
		DI("[%s] drop error len=%d", data, len);
		return;
	}

	/* 受信データを処理した結果、応答データ受信完了状態となった場合 */
	if (ble_io_status == SERVER_IO_STATUS_DATA_UPLOAD_RESP_COMP){
		/* 応答データが存在し、累積サイズと応答データのデータ長が一致している場合 */
		if ((data_upload_resp_len != 0) && (resp_rcv_len == data_upload_resp_len)) {
			/* 送信データにEOFを追加 */
			data_upload_resp_buf[data_upload_resp_len] = '\0';
			/* 応答データをRF Ctrl部に通知 */
			rf_ctrl_data_upload_ble_cb(BT_GATT_OK, data_upload_resp_buf);
		}
		else {
			/* エラー応答をRF Ctrl部に通知 */
			rf_ctrl_data_upload_ble_cb(BT_GATT_RES_INVALID_OFFSET, data_upload_resp_buf);
		}
	}
#else //#if CONFIG_AUDREY_REDUCE_MALLOC
	/* 応答データ待ち状態の場合、最初のデータとして処理 */
	if (ble_io_status == SERVER_IO_STATUS_DATA_UPLOAD_RESP_WAIT) {
		resp_rcv_len = len;
		/* 受信したデータがデータサイズ格納領域の4バイト以下である場合 */
		if (resp_rcv_len <= DATA_UPLOAD_DATASIZE_LEN) {
			Log_Error("[ERROR]%s:%d recv data length = %d\r\n", __FUNCTION__, __LINE__, resp_rcv_len);
		}
		else {
			/* Transportデータのデータ長をセット */
			data_upload_resp_len  = data[0] << 24;
			data_upload_resp_len += data[1] << 16;
			data_upload_resp_len += data[2] << 8;
			data_upload_resp_len += data[3];
			DI("recv transport data\r\n");
			DI("transport data length: %d\r\n", data_upload_resp_len);
			/* データ長よりも大きなデータを受信した場合 */
			if (len - DATA_UPLOAD_DATASIZE_LEN > data_upload_resp_len) {
				Log_Error("\r\n [ERROR] %s:%d receive data is too long\r\n", __FUNCTION__, __LINE__);
				Log_Error("\r\n [ERROR] recv data: %d\r\n", (len - DATA_UPLOAD_DATASIZE_LEN));
			}
			/* データ長以下のデータを受信した場合 */
			else {
				/* 受信バッファ用にConfigurationデータのデータ長＋１バイト分のメモリを確保 */
				data_upload_resp_buf = malloc(data_upload_resp_len + 1);
				if (!data_upload_resp_buf) {
					Log_Error("\r\n [ERROR] %s:%d memory alloc error\r\n", __FUNCTION__, __LINE__);
					return;
				}
				memset(data_upload_resp_buf, 0, (data_upload_resp_len + 1));
				/* Transportデータの受信バッファに受信したデータを格納 */
				resp_rcv_len -= DATA_UPLOAD_DATASIZE_LEN;
				memcpy(data_upload_resp_buf, data + DATA_UPLOAD_DATASIZE_LEN, resp_rcv_len);
				DI("recv data: %d  total recv:%d\r\n", resp_rcv_len, resp_rcv_len);
				/* 1回のWriteでデータの受信が完了しなかった場合 */
				if (resp_rcv_len < data_upload_resp_len) {
					/* ステータスを応答データ受信中に遷移させ次のデータを待つ */
					ble_io_status = SERVER_IO_STATUS_DATA_UPLOAD_RESP_RECV;
				}
			}
		}
	/* 応答データ受信中の場合、続きのデータとして処理 */
	} else if (ble_io_status == SERVER_IO_STATUS_DATA_UPLOAD_RESP_RECV) {
		DI("recv data: %d  total recv: %d\r\n", len, resp_rcv_len + len);
		/* 受信したデータを追加するとデータ長を超える場合 */
		if (resp_rcv_len + len > data_upload_resp_len) {
			Log_Error("\r\n [ERROR] %s:%d resp_buf over flow\r\n", __FUNCTION__, __LINE__);
			/* エラー応答をRF Ctrl部に通知 */
			rf_ctrl_data_upload_ble_cb(BT_GATT_RES_INVALID_OFFSET, data_upload_resp_buf);
			ble_io_status = SERVER_IO_STATUS_DATA_UPLOAD_RESP_COMP;
		}
		/* 受信したデータを追加するとデータ長以下になる場合 */
		else {
			/* 受信したデータをTransportデータ用受信バッファに追加 */
			memcpy(data_upload_resp_buf + resp_rcv_len, data, len);
			resp_rcv_len += len;
		}
	/* 応答データ受信完了状態の場合、余分なデータとして無視 */
	} else if (ble_io_status == SERVER_IO_STATUS_DATA_UPLOAD_RESP_COMP) {
		 DI("[%s] drop error len=%d", data, len);
	}
	/* データを処理した結果、応答データ受信が完了した場合 */
	if (resp_rcv_len == data_upload_resp_len) {
		/* ステータスを応答データ受信完了に遷移 */
		ble_io_status = SERVER_IO_STATUS_DATA_UPLOAD_RESP_COMP;
		/* RF Ctrl部にデータを渡す */
		rf_ctrl_data_upload_ble_cb(BT_GATT_OK, data_upload_resp_buf);
	}
#endif //#if CONFIG_AUDREY_REDUCE_MALLOC
}

static fwup_char_handler(tUint8* data, tUint16 len)
{
	write_cb_sum_len +=  len;
	write_cb_count++;
	write_cb_check_count = 0;
	D(" <fwup_char> val_len=%d", len);
	if (ble_io_status == SERVER_IO_STATUS_READ ||
		ble_io_status == SERVER_IO_STATUS_READY ||
		ble_io_status == SERVER_IO_STATUS_READ_COMPLETE ||
		ble_io_status == SERVER_IO_STATUS_WRITE_COMPLETE
		) {
		/*
		memcpy(ble_read_buf, data, len);
		ble_read_len = len;
		ble_io_status = SERVER_IO_STATUS_READ_COMPLETE;
		D("read len=%d d[", len);
		//D_BUF(data, len); DD("\r\n");
		//D(" SERVER_IO_STATUS_READ_COMPLETE");
		*/
		DI("(%d:%d:%d/%d %d)",write_cb_ring_buf_write, write_cb_ring_buf_read,len,
				len, WRITW_CB_RING_BUF_FREE);
		if (WRITW_CB_RING_BUF_FREE >= len ) {
			int buf_left = WRITE_CB_RING_BUF_LENGTH - write_cb_ring_buf_write;
			if (buf_left >= len) {
				memcpy(&write_cb_ring_buf[write_cb_ring_buf_write], data, len);
				write_cb_ring_buf_write += len;
			} else {
				memcpy(&write_cb_ring_buf[write_cb_ring_buf_write], data, buf_left);
				memcpy(&write_cb_ring_buf[0], data+buf_left, len - buf_left);
				write_cb_ring_buf_write = len - buf_left;
				DI("buf_left=%d len=%d write_cb_ring_buf_write=%d",
						buf_left, len, write_cb_ring_buf_write);
				DI("write_cb_ring_buf[0--%d] [",write_cb_ring_buf_write);
				DI_BUF(write_cb_ring_buf,write_cb_ring_buf_write); DDI("]\r\n");
			}
			if (write_cb_ring_buf_write == WRITE_CB_RING_BUF_LENGTH) {
				write_cb_ring_buf_write = 0;
			} else if (write_cb_ring_buf_write > WRITE_CB_RING_BUF_LENGTH) {
				Log_Error("[ERROR]%s:%d ring buffer over run %d %d", __FUNCTION__, __LINE__,
						write_cb_ring_buf_write, WRITE_CB_RING_BUF_LENGTH);
				write_cb_ring_buf_write = 0;
			}
			//int max_len = ble_read_buf_len<WRITE_CB_BUF_LENGTH?ble_read_buf_len:WRITE_CB_BUF_LENGTH;
			if (ble_io_status == SERVER_IO_STATUS_READ) {
				if (ble_read_len_max > 0 && WRITW_CB_RING_BUF_AVAIL >= ble_read_len_max) {
					//[TODO] need lock
					if (write_cb_ring_buf_write > write_cb_ring_buf_read) {
						if ((write_cb_ring_buf_write - write_cb_ring_buf_read) < ble_read_len_max) {
							Log_Error("[ERROR]%s:%d program error %d %d %d", __FUNCTION__, __LINE__,
										write_cb_ring_buf_read, write_cb_ring_buf_write, ble_read_len_max);
						}
						ble_read_len = ble_read_len_max;
						DI("copy %d %d ", write_cb_ring_buf_read, ble_read_len);
						memcpy(ble_read_buf,  &write_cb_ring_buf[write_cb_ring_buf_read], ble_read_len);
						write_cb_ring_buf_read += ble_read_len;
						if (write_cb_ring_buf_write < write_cb_ring_buf_read) {
							Log_Error("[ERROR]%s:%d program error %d %d %d", __FUNCTION__, __LINE__,
											write_cb_ring_buf_read, write_cb_ring_buf_write, ble_read_len);
						}
						D("read["); D_BUF(ble_read_buf, ble_read_len); D("]\r\n");
					}
					else if (write_cb_ring_buf_write != write_cb_ring_buf_read) { // write_cb_ring_buf_write < write_cb_ring_buf_read
						DI("write_cb_ring_buf_write=%d write_cb_ring_buf_read=%d",
								write_cb_ring_buf_write, write_cb_ring_buf_read);
						ble_read_len = WRITE_CB_RING_BUF_LENGTH - write_cb_ring_buf_read;
						if (ble_read_len_max <= ble_read_len) {
							ble_read_len = ble_read_len_max;
							DI("copy %d %d ", write_cb_ring_buf_read, ble_read_len);
							memcpy(&ble_read_buf[0],  &write_cb_ring_buf[write_cb_ring_buf_read], ble_read_len);
							D("read["); D_BUF(ble_read_buf, ble_read_len); DD("] len=%d\r\n", ble_read_len);
							//write_cb_ring_buf_read = 0;
							write_cb_ring_buf_read += ble_read_len;
							if (write_cb_ring_buf_read == WRITE_CB_RING_BUF_LENGTH) {
								write_cb_ring_buf_read = 0;
							}
						} else {
							//  ble_read_len < ble_read_len_max
							DI("copy %d %d ", write_cb_ring_buf_read, ble_read_len);
							memcpy(&ble_read_buf[0],  &write_cb_ring_buf[write_cb_ring_buf_read], ble_read_len);
							D("read["); D_BUF(ble_read_buf, ble_read_len); DD("] len=%d\r\n", ble_read_len);
							write_cb_ring_buf_read = 0;
							if (ble_read_len < ble_read_len_max) {
								int copy_len;
								copy_len = ble_read_len_max - ble_read_len;
								if (copy_len <= write_cb_ring_buf_write) { //[TODO] check
									DI("copy %d %d ", write_cb_ring_buf_read, copy_len);
									memcpy(&ble_read_buf[ble_read_len],  &write_cb_ring_buf[0], copy_len);
									write_cb_ring_buf_read += copy_len;
								} else {
									Log_Error("[ERROR]%s:%d program error %d %d %d %d", __FUNCTION__, __LINE__,
											write_cb_ring_buf_read, write_cb_ring_buf_write, ble_read_len, copy_len);
								}
								D("read["); D_BUF(&ble_read_buf[ble_read_len], copy_len); DD("] len=%d %d\r\n",
										ble_read_len, copy_len);
								ble_read_len += copy_len;
								if (ble_read_len > ble_read_len_max) {
									Log_Error("[ERROR]%s:%d program error %d %d %d %d", __FUNCTION__, __LINE__,
										write_cb_ring_buf_read, write_cb_ring_buf_write, ble_read_len, copy_len);
								}
							} else {
								Log_Error("[ERROR]%s:%d program error %d %d %d %d", __FUNCTION__, __LINE__,
									write_cb_ring_buf_read, write_cb_ring_buf_write, ble_read_len, ble_read_len_max);
							}
						} //
						D("read["); D_BUF(ble_read_buf, ble_read_len); DD("] len=%d\r\n", ble_read_len);
						DI("write_cb_ring_buf_write=%d write_cb_ring_buf_read=%d",
								write_cb_ring_buf_write, write_cb_ring_buf_read);
					} else {
						ble_read_len = 0;
						Log_Error("[ERROR]%s program error buffer over run  %d %d %d", __FUNCTION__,
								write_cb_ring_buf_read, write_cb_ring_buf_write, len);
					}
					ble_read_len_max = 0;
					//[TODO] unlock
					if (ble_io_status != SERVER_IO_STATUS_READ) {
						Log_Error("[ERROR]%s program error io buffer over run  %d %d %d ble_io_status=%d", __FUNCTION__,
									write_cb_ring_buf_read, write_cb_ring_buf_write, ble_read_len, ble_io_status);
					}
					ble_io_status = SERVER_IO_STATUS_READ_COMPLETE;
					DI("read ring buf left[%d] len=%d ", WRITW_CB_RING_BUF_AVAIL, ble_read_len);
				}
			} // if (ble_io_status == SERVER_IO_STATUS_READ)
		} else {
			 ble_read_len = 0;
			 Log_Error("[ERROR]%s:%d ######### program error buffer over run FREE(%d)  %d %d %d ###########", __FUNCTION__, __LINE__,
					 WRITW_CB_RING_BUF_FREE, write_cb_ring_buf_read, write_cb_ring_buf_write, len);
		}
	}
	else {
		Log_Error("\r\n[ERROR][%s:%d] ######## drop error len=%d ble_io_status=%d[%s]#############", __FUNCTION__, __LINE__,
				len, ble_io_status, ble_io_status_string[ble_io_status]);
	}
	//else if (ble_io_status == SERVER_IO_STATUS_READ_COMPLETE) {
	//	 Log_Error("\r\n[%s] drop error len=%d", __FUNCTION__, len);
	//}
}

static conf_char_handler(tUint8* data, tUint16 len)
{
#if CONFIG_AUDREY_REDUCE_MALLOC
	/* 受信データの累積サイズがクリアされている場合、最初のデータとして受け付ける */
	if (conf_data_recv_total_len == 0) {
		conf_data_recv_len = 0;
		conf_data_recv_buf[0] = '\0';
		/* 受信したデータがデータサイズ格納領域の4バイト以下である場合 */
		if (len <= CONF_DATA_DATASIZE_LEN) {
			Log_Error("[ERROR]%s:%d recv data length = %d\r\n", __FUNCTION__, __LINE__, len);
		}
		else {
			/* Configurationデータのデータ長をセット */
			conf_data_recv_len  = data[0] << 24;
			conf_data_recv_len += data[1] << 16;
			conf_data_recv_len += data[2] << 8;
			conf_data_recv_len += data[3];
			DI("recv configuration data\r\n");
			DI("configuration data length: %d\r\n", conf_data_recv_len);
			/* Configurationデータがバッファのサイズを超える場合 */
			if (conf_data_recv_len > MAX_CONF_DATA_LEN) {
				Log_Error("\r\n [ERROR] %s:%d configuration data %d bytes is too long\r\n", __FUNCTION__, __LINE__, conf_data_recv_len);
			}
			else {
				/* Configurationデータの受信バッファに受信したデータを格納 */
				conf_data_recv_total_len = len - CONF_DATA_DATASIZE_LEN;
				memcpy(conf_data_recv_buf, data + CONF_DATA_DATASIZE_LEN, conf_data_recv_total_len);
				DI("recv data: %d  total recv:%d\r\n", conf_data_recv_total_len, conf_data_recv_total_len);
			}
		}
	}
	/* 受信データの累積サイズが0より大きい場合、続きのデータとして処理 */
	else if (conf_data_recv_total_len > 0) {
		/* 受信したデータを追加するとデータ長を超える場合 */
		if (conf_data_recv_total_len + len > conf_data_recv_len) {
			Log_Error("\r\n [ERROR] %s:%d receive data is too long\r\n", __FUNCTION__, __LINE__);
			Log_Error("\r\n [ERROR] recv data: %d  total recv: %d\r\n", len, conf_data_recv_total_len);
			/* 受信データの累積サイズをクリア */
			conf_data_recv_total_len = 0;
		}
		/* 受信したデータを追加するとデータ長以下になる場合 */
		else {
			/* 今回受信したデータを受信バッファに追加 */
			memcpy(&conf_data_recv_buf[conf_data_recv_total_len], data, len);
			conf_data_recv_total_len += len;
			DI("recv data: %d  total recv: %d\r\n", len, conf_data_recv_total_len);
		}
	}

	/* 受信データが存在し、Configurationデータのデータ長と一致している場合 */
	if ((conf_data_recv_total_len != 0) && (conf_data_recv_total_len == conf_data_recv_len)) {
		/* 受信完了として受信データの累積サイズをクリア */
		conf_data_recv_total_len = 0;
		/* 送信データにEOFを追加 */
		conf_data_recv_buf[conf_data_recv_len] = '\0';
		/* RF Ctrl部にデータを渡す */
		rf_ctrl_conf_data_recv_cb(conf_data_recv_buf);
	}
#else //#if CONFIG_AUDREY_REDUCE_MALLOC
	/* 受信データの累積サイズがクリアされている場合、最初のデータとして受け付ける */
	if (conf_data_recv_total_len == 0) {
		/* 受信したデータがデータサイズ格納領域の4バイト以下である場合 */
		if (len <= DATA_UPLOAD_DATASIZE_LEN) {
			Log_Error("[ERROR]%s:%d recv data length = %d\r\n", __FUNCTION__, __LINE__, len);
		}
		else {
			/* Configurationデータのデータ長をセット */
			conf_data_recv_len  = data[0] << 24;
			conf_data_recv_len += data[1] << 16;
			conf_data_recv_len += data[2] << 8;
			conf_data_recv_len += data[3];
			DI("recv configuration data\r\n");
			DI("configuration data length: %d\r\n", conf_data_recv_len);
			/* データ長よりも大きなデータを受信した場合 */
			if (len - CONF_DATA_DATASIZE_LEN > conf_data_recv_len) {
				Log_Error("\r\n [ERROR] %s:%d receive data is too long\r\n", __FUNCTION__, __LINE__);
				Log_Error("\r\n [ERROR] recv data: %d\r\n", (len - CONF_DATA_DATASIZE_LEN));
			}
			/* データ長以下のデータを受信した場合 */
			else {
				/* 受信バッファ用にConfigurationデータのデータ長＋１バイト分のメモリを確保 */
				conf_data_recv_buf = malloc(conf_data_recv_len + 1);
				if (!conf_data_recv_buf) {
					Log_Error("\r\n [ERROR] %s:%d memory alloc error\r\n", __FUNCTION__, __LINE__);
					return;
				}
				memset(conf_data_recv_buf, 0, (conf_data_recv_len + 1));
				/* Configurationデータの受信バッファに受信したデータを格納 */
				conf_data_recv_total_len = len - CONF_DATA_DATASIZE_LEN;
				memcpy(conf_data_recv_buf, data + CONF_DATA_DATASIZE_LEN, conf_data_recv_total_len);
				DI("recv data: %d  total recv:%d\r\n", conf_data_recv_total_len, conf_data_recv_total_len);
			}
		}
	}
	/* 受信データの累積サイズが0より大きい場合、続きのデータとして処理 */
	else if (0 < conf_data_recv_total_len) {
		/* 受信したデータを追加するとデータ長を超える場合 */
		if (conf_data_recv_len < conf_data_recv_total_len + len ) {
			Log_Error("\r\n [ERROR] %s:%d receive data is too long\r\n", __FUNCTION__, __LINE__);
			Log_Error("\r\n [ERROR] recv data: %d  total recv: %d\r\n", len, conf_data_recv_total_len);
			/* 受信したデータを破棄し、受信データの累積サイズをクリア */
			free(conf_data_recv_buf);
			conf_data_recv_total_len = 0;
		}
		/* 受信したデータを追加するとデータ長以下になる場合 */
		else {
			/* 今回受信したデータを受信バッファに追加 */
			memcpy(conf_data_recv_buf + conf_data_recv_total_len, data, len);
			conf_data_recv_total_len += len;
			DI("recv data: %d  total recv: %d\r\n", len, conf_data_recv_total_len);
		}
	}

	/* Configurationデータの受信が完了した場合 */
	if (conf_data_recv_total_len == conf_data_recv_len) {
		/* 受信完了として受信データの累積サイズをクリア */
		conf_data_recv_total_len = 0;
		/* RF Ctrl部にデータを渡す */
		rf_ctrl_conf_data_recv_cb(conf_data_recv_buf);
	}
#endif //#if CONFIG_AUDREY_REDUCE_MALLOC
}

static void write_cb(tBT_GattsWriteIndStru *param, BT_GattWriteEnum write_type)
{
	tUint8 write_type_str[16];
	tBT_GattResEnum res = BT_GATT_OK;

	if (write_type==WRITE_CMD) {
		strcpy(write_type_str,"write_cmd");
	}
	else if (write_type==WRITE_REQ) {
		strcpy(write_type_str,"write_req");
	}
	else if (write_type==PREP_WRITE) {
		strcpy(write_type_str,"prep_write");
	}

	D(" [handle=%d] write_type[%s] attr_type[%d : %s]",param->att_hdl,write_type_str, param->attr_type, attr_type_string[param->attr_type]);

#if CONFIG_AUDREY_REDUCE_MALLOC
	if (param->val_len > MAX_MTU_SIZE) {
		Log_Error("\r\n[write_cb] %d: write data is too long\r\n", __LINE__);
		res = BT_GATT_RES_UNLIKELY_ERROR;
		goto exit;
	}
	tUint8 data[MAX_MTU_SIZE + 1];
	memset(&data[param->val_len], 0, 1);
#else //#if CONFIG_AUDREY_REDUCE_MALLOC
	tUint8* data = (tUint8*)malloc(param->val_len+1);
	if (!data) {
		Log_Error("\r\n[write_cb] %d: data memory alloc error ######## \r\n", __LINE__);
		return;
	}
	memset(data+param->val_len,0,1);
#endif //#if CONFIG_AUDREY_REDUCE_MALLOC
	memcpy(data,param->val,param->val_len);
	int data_len = param->val_len;

	DBG {
		for (int i = 0; i< data_len; i++) {
			write_cb_sum += data[i];
		}
		write_cb_len += data_len;
		DI(" write_cb sum=%u len=%u", write_cb_sum, write_cb_len);
		if (debug_print_lebel_auto > 0 && debug_print_lebel_auto < write_cb_len) {
			debug_print_lebel_auto = 0;
			debug_print_lebel -= 2;
			if (debug_print_lebel < 0) {
				debug_print_lebel = 0;
			}
			DI(" debug_print_lebel=%d", 3);
		}
	} // DBG

	switch (param->attr_type) {
		case BT_GATT_ATTR_TYPE_CHARACTERISTIC: {
			if (debug_print_short) {
				DDI("[%d]",param->val_len);
			} else {
				DI("Characteristic value [len=%d]",param->val_len);
				DD(" [")
				D_BUF(data,param->val_len);
				DD("]");
			}
			if (write_type==PREP_WRITE) {
				DI(" PREP_WRITE");
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
					if (!entry) {
						Log_Error("\r\n[write_cb] %d: entry memory alloc error ######## \r\n", __LINE__);
						goto exit;
					}
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
				if (param->att_if == primary_svc.date_char.attr_if) {
					D("\r\n[write_cb] primary_svc.date_char");
					pVal = &primary_svc.date_char.val;
					date_char_handler(data, param->val_len);
				}
				else if (param->att_if == primary_svc.data_char.attr_if) {
					D("\r\n[write_cb] primary_svc.data_char");
					pVal = &primary_svc.data_char.val;
					transport_data_char_handler(data, param->val_len);
				}
				else if (param->att_if == primary_svc.fwup_char.attr_if) {
					D("\r\n[write_cb] primary_svc.fwup_char");
					pVal = &primary_svc.fwup_char.val;
					fwup_char_handler(data, param->val_len);
				}
				else if (param->att_if == primary_svc.conf_char.attr_if) {
					D("\r\n[write_cb] primary_svc.conf_char");
					pVal = &primary_svc.conf_char.val;
					conf_char_handler(data, param->val_len);
				}
				free(*pVal);
				*pVal = malloc_value_stru(param->val_len);
				memcpy((*pVal)->data,data, param->val_len);
			}
			break;
		}
		case BT_GATT_ATTR_TYPE_CHR_DESCRIPTOR : {
			DI("Characteristic descriptor:[%s] ",
				(param->att_if == primary_svc.desc_ccc_data.attr_if) ? "desc_ccc_data" :
				(param->att_if == primary_svc.desc_ccc_fwup.attr_if) ? "desc_ccc_fwup" :
				(param->att_if == primary_svc.desc_ccc_conf.attr_if) ? "desc_ccc_conf" :
				(param->att_if == primary_svc.desc_ccc_mtu.attr_if)  ? "desc_ccc_mtu"  : "?"
				);

			if (param->att_if == primary_svc.desc_ccc_data.attr_if
				|| param->att_if == primary_svc.desc_ccc_fwup.attr_if
				|| param->att_if == primary_svc.desc_ccc_conf.attr_if
				|| param->att_if == primary_svc.desc_ccc_mtu.attr_if) {
				/* Client Characteristic Configuration */
				tUint16 *cccd = NULL;
				tUint16 *cccd_in = (tUint16 *) param->val;

				if (param->att_if == primary_svc.desc_ccc_data.attr_if) {
					cccd = (tUint16 *) primary_svc.desc_ccc_data.val->data;
					D("desc_ccc_data: (0x%x)", *cccd_in);
					if ((*cccd_in) != BT_GATT_CCC_VAL_INDICATION && (*cccd_in) != BT_GATT_CCC_VAL_NONE) {
						res = BT_GATT_RES_CCC_IMPROPERLY_CONFIGURED;
						DI("%x [INVALID]\r\n",*cccd_in);
						goto exit;
					}
					else if ((*cccd_in) == BT_GATT_CCC_VAL_INDICATION) {
						/* BLE経由でのデータアップロードが有効となったことを通知 */
						rf_ctrl_data_upload_ble_enable_cb();
					}
				}
				else if (param->att_if == primary_svc.desc_ccc_fwup.attr_if) {
					cccd = (tUint16 *) primary_svc.desc_ccc_fwup.val->data;
					D("desc_ccc_fwup: (0x%x)", *cccd_in);
					if ((*cccd_in) != BT_GATT_CCC_VAL_INDICATION && (*cccd_in) != BT_GATT_CCC_VAL_NONE) {
						res = BT_GATT_RES_CCC_IMPROPERLY_CONFIGURED;
						DI("%x [INVALID]\r\n",*cccd_in);
						goto exit;
					}
				}
				else if (param->att_if == primary_svc.desc_ccc_conf.attr_if) {
					cccd = (tUint16 *) primary_svc.desc_ccc_conf.val->data;
					D("desc_ccc_conf: (0x%x)", *cccd_in);
					if ((*cccd_in) != BT_GATT_CCC_VAL_INDICATION && (*cccd_in) != BT_GATT_CCC_VAL_NONE) {
						res = BT_GATT_RES_CCC_IMPROPERLY_CONFIGURED;
						DI("%x [INVALID]\r\n",*cccd_in);
						goto exit;
					}
				}
				else if (param->att_if == primary_svc.desc_ccc_mtu.attr_if) {
					cccd = (tUint16 *) primary_svc.desc_ccc_mtu.val->data;
					D("desc_ccc_mtu: (0x%x)", *cccd_in);
					if ((*cccd_in) != BT_GATT_CCC_VAL_NOTIFICATION && (*cccd_in) != BT_GATT_CCC_VAL_NONE) {
						res = BT_GATT_RES_CCC_IMPROPERLY_CONFIGURED;
						DI("%x [INVALID]\r\n",*cccd_in);
						goto exit;
					}
				}
				memcpy(cccd,cccd_in,CLI_CONF_LEN);
				D(" 0x%x\r\n",*cccd);
			}
			break;
		}
		default:
			break;
	}
exit:
#if CONFIG_AUDREY_REDUCE_MALLOC != 1
	if (data) {
	free(data);
	}
#endif //#if CONFIG_AUDREY_REDUCE_MALLOC
	if (write_type == WRITE_REQ || write_type == PREP_WRITE) {
		BT_GATTS_WriteRsp(param->conn, res);
		DI("BT_GATTS_WriteRsp\r\n");
	}
}

static void exec_write_cb(tBT_GattsExcWriteIndStru *param)
{
	DI("[exec_write_cb] Execute Write Request: ");

	char found_exec_write_buf = 0;
	GattPrepWriteBuf *entry = NULL;
	list_for_each_entry (entry, &prep_write_buf_list, list, GattPrepWriteBuf) {
		if (entry->buffer.attr_if == param->att_if) {
			found_exec_write_buf = 1;
			if (param->flag == BT_GATT_EWRF_IMMEDIATELY_WRITE_ALL) {
				tBT_ValueStru **pVal = NULL;
#if CONFIG_AUDREY_REDUCE_MALLOC
				tUint8 data[MAX_MTU_SIZE + 1];
#else //#if CONFIG_AUDREY_REDUCE_MALLOC
				tUint8* data;
#endif //#if CONFIG_AUDREY_REDUCE_MALLOC
				tUint16 len = entry->buffer.val->len;

#if CONFIG_AUDREY_REDUCE_MALLOC
				if (len > MAX_MTU_SIZE) {
					Log_Error("\r\n[exec_write_cb] write data is too long\r\n");
					goto exit_exec_write_cb;
				}
				memset(&data[len],0,1);
#else //#if CONFIG_AUDREY_REDUCE_MALLOC
				data = (tUint8*)malloc(len+1);
				if (!data) {
					Log_Error("\r\n[exec_write_cb] memory alloc error\r\n");
					goto exit_exec_write_cb;
				}
				memset(data+len,0,1);
#endif //#if CONFIG_AUDREY_REDUCE_MALLOC
				memcpy(data,entry->buffer.val->data,len);

				if (param->att_if == primary_svc.date_char.attr_if) {
					DI("%d: recv date data\r\n", __LINE__);
					pVal = &primary_svc.date_char.val;
					date_char_handler(data, len);
				}
				else if (param->att_if == primary_svc.data_char.attr_if) {
					DI("%d: recv transport data\r\n", __LINE__);
					pVal = &primary_svc.data_char.val;
					transport_data_char_handler(data, len);
				}
				else if (param->att_if == primary_svc.fwup_char.attr_if) {
					DI("%d: recv firmware update data\r\n", __LINE__);
					pVal = &primary_svc.fwup_char.val;
					fwup_char_handler(data, len);
				}
				else if (param->att_if == primary_svc.conf_char.attr_if) {
					DI("%d: recv configuration data\r\n", __LINE__);
					pVal = &primary_svc.conf_char.val;
					conf_char_handler(data, len);
				}
				free(*pVal);
				*pVal = malloc_value_stru(entry->buffer.val->len);
				memcpy((*pVal)->data,entry->buffer.val->data, entry->buffer.val->len);
				DI("Immediately Write All\r\n");
#if CONFIG_AUDREY_REDUCE_MALLOC != 1
				if (!data) {
					free(data);
				}
#endif //#if CONFIG_AUDREY_REDUCE_MALLOC != 1

				 if (ble_io_status == SERVER_IO_STATUS_READ) {
					 memcpy(ble_read_buf, entry->buffer.val->data, entry->buffer.val->len);
					 ble_read_len = entry->buffer.val->len;
					 ble_io_status = SERVER_IO_STATUS_READ_COMPLETE;
					 D("read len=%d [%s]\r\n", entry->buffer.val->len, entry->buffer.val->data);
					 D(" SERVER_IO_STATUS_READ_COMPLETE");
				 } else if (ble_io_status == SERVER_IO_STATUS_READ_COMPLETE) {
					 D("drop error len=%d\r\n", entry->buffer.val->len);
				 }

			}
			else if (param->flag == BT_GATT_EWRF_CANCEL_ALL) {
				D("Cancel All Prepared Writes\r\n");
			}
			list_del_init(&entry->list);
			free(entry->buffer.val);
			free(entry);
			break;
		}
	}
exit_exec_write_cb:
	if (found_exec_write_buf) {
		BT_GATTS_ExecuteWriteRsp(param->conn, BT_GATT_OK);
	}
	else {
		Log_Error("Error!\r\n");
	}
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
			Log_Error("[Warning] [bt_gatt_server] [prim_svc_cb] Unhandled event: %x\r\n",ev);
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
		case BT_GATT_EV_CONN_INTERVAL_UADATE:
			D("Connection interval updated\r\n");
			break;
		default:
			Log_Error("[Warning] [bt_gatt_server] [gatt_ev_cb] Unhandled event: %x\r\n",ev);
			break;
	}
}

void puuid_print(tBT_UuidStru *uuid) {
	D("UUID=");
	if (uuid->len == BT_UUID_LEN_16) {
		DD("%04X",uuid->u.uuid16);
	}
	else if (uuid->len == BT_UUID_LEN_32) {
		DD("%08X",uuid->u.uuid32);
	}
	else if (uuid->len == BT_UUID_LEN_128) {
		for(int i=15; i>=0; i--)
			DD("%02X",uuid->u.uuid128[i]);
	}
}

void uuid128_create(tBT_UuidStru *uuid, char * str)
{
	memset(uuid, 0, sizeof(tBT_UuidStru));
	uuid->len = BT_UUID_LEN_128;
	hex_str_to_bd_addr(strlen(str), str, uuid->u.uuid128);
	//puuid_print(uuid);
}


static void register_gatt_service(void)
{
	BT_GATT_CallbackRegister(gatt_ev_cb);
	BT_GATT_DefaultMtuSet(256);
	INIT_LIST_HEAD(&prep_write_buf_list);
	tBT_UuidStru uuid;

	// サービス登録
	uuid128_create(&uuid,PET_SVC_UUID);
	D("\r\n Pet Healthcare Data Transport Service="); D_BUF(&uuid,16); DD("\r\n");
	primary_svc.svc.attr_if = BT_GATTS_ServiceCreate(&uuid, prim_svc_cb, FALSE);

	// Data Transport Characterisitc
	uuid128_create(&uuid,DATA_CHR_UUID);
	D("\r\n Data Transport Characterisitc="); D_BUF(&uuid,16+4); DD("\r\n");
#ifdef DISABLE_GATT_PERM_ENC
	primary_svc.data_char.attr_if = BT_GATTS_CharacteristicAdd(primary_svc.svc.attr_if, &uuid, ( BT_GATT_PROP_WRITE_REQ | BT_GATT_PROP_INDICATE), BT_GATT_PERM_WRITE);
#else
	primary_svc.data_char.attr_if = BT_GATTS_CharacteristicAdd(primary_svc.svc.attr_if, &uuid, ( BT_GATT_PROP_WRITE_REQ | BT_GATT_PROP_INDICATE), (BT_GATT_PERM_WRITE | BT_GATT_PERM_WRITE_ENCRYPT_REQUEIRED));
#endif //DISABLE_GATT_PERM_ENC
#if CONFIG_AUDREY_REDUCE_MALLOC
	primary_svc.data_char.val = malloc_value_stru(MAX_MTU_SIZE + 1);
#else //#if CONFIG_AUDREY_REDUCE_MALLOC
	primary_svc.data_char.val = malloc_value_stru(20);
#endif //#if CONFIG_AUDREY_REDUCE_MALLOC
	primary_svc.desc_ccc_data.attr_if = BT_GATTS_DesCCCAdd(primary_svc.data_char.attr_if, BT_GATT_PROP_INDICATE, (BT_GATT_PROP_READ | BT_GATT_PROP_WRITE_REQ), BT_GATT_CCC_VAL_NONE);
	primary_svc.desc_ccc_data.val = malloc_value_stru(CLI_CONF_LEN);

	// Service Version Characteristic
	uuid128_create(&uuid,VER_CHR_UUID);
	D("\r\n Service Version Characteristic="); D_BUF(&uuid,16+4); DD("\r\n");
#ifdef DISABLE_GATT_PERM_ENC
	primary_svc.ver_char.attr_if = BT_GATTS_CharacteristicAdd(primary_svc.svc.attr_if, &uuid, BT_GATT_PROP_READ, BT_GATT_PERM_READ);
#else
	primary_svc.ver_char.attr_if = BT_GATTS_CharacteristicAdd(primary_svc.svc.attr_if, &uuid, BT_GATT_PROP_READ, (BT_GATT_PERM_READ | BT_GATT_PERM_READ_ENCRYPT_REQUEIRED));
#endif //DISABLE_GATT_PERM_ENC
	tUint8 read_val[2] = { VER_VAL_0, VER_VAL_1 };
	D("\r\n Service Version = 0x%02x%02x\r\n", read_val[0], read_val[1]);
	primary_svc.ver_char.val = malloc_value_stru(2);
	memcpy(primary_svc.ver_char.val->data,read_val, 2); // 2 byte

	// Date Setting Characteristic
	uuid128_create(&uuid,DATE_CHR_UUID);
	D("\r\n Date Setting Characteristic="); D_BUF(&uuid,16+4); DD("\r\n");
#ifdef DISABLE_GATT_PERM_ENC
	primary_svc.date_char.attr_if = BT_GATTS_CharacteristicAdd(primary_svc.svc.attr_if, &uuid, BT_GATT_PROP_WRITE_REQ, BT_GATT_PERM_WRITE);
#else
	primary_svc.date_char.attr_if = BT_GATTS_CharacteristicAdd(primary_svc.svc.attr_if, &uuid, BT_GATT_PROP_WRITE_REQ, (BT_GATT_PERM_WRITE | BT_GATT_PERM_WRITE_ENCRYPT_REQUEIRED));
#endif //DISABLE_GATT_PERM_ENC
	primary_svc.date_char.val = malloc_value_stru(4);

	// Firmware Update Characterisitc
	uuid128_create(&uuid,FW_UPDATE_CHR_UUID);
	D("\r\n Firmware Update Characterisitc="); D_BUF(&uuid,16+4); DD("\r\n");
#ifdef DISABLE_GATT_PERM_ENC
	primary_svc.fwup_char.attr_if = BT_GATTS_CharacteristicAdd(primary_svc.svc.attr_if, &uuid, ( BT_GATT_PROP_WRITE_REQ | BT_GATT_PROP_INDICATE), BT_GATT_PERM_WRITE);
#else
	primary_svc.fwup_char.attr_if = BT_GATTS_CharacteristicAdd(primary_svc.svc.attr_if, &uuid, ( BT_GATT_PROP_WRITE_REQ | BT_GATT_PROP_INDICATE), (BT_GATT_PERM_WRITE | BT_GATT_PERM_WRITE_ENCRYPT_REQUEIRED));
#endif //DISABLE_GATT_PERM_ENC
#if CONFIG_AUDREY_REDUCE_MALLOC
	primary_svc.fwup_char.val = malloc_value_stru(MAX_MTU_SIZE + 1);
#else //#if CONFIG_AUDREY_REDUCE_MALLOC
	primary_svc.fwup_char.val = malloc_value_stru(20);
#endif //#if CONFIG_AUDREY_REDUCE_MALLOC
	primary_svc.desc_ccc_fwup.attr_if = BT_GATTS_DesCCCAdd(primary_svc.fwup_char.attr_if, BT_GATT_PROP_INDICATE, (BT_GATT_PROP_READ | BT_GATT_PROP_WRITE_REQ), BT_GATT_CCC_VAL_NONE);
	primary_svc.desc_ccc_fwup.val = malloc_value_stru(CLI_CONF_LEN);

	// Configuration Characterisitc
	uuid128_create(&uuid,CONF_CHR_UUID);
	D("\r\n Configuration Characterisitc="); D_BUF(&uuid,16+4); DD("\r\n");
#ifdef DISABLE_GATT_PERM_ENC
	primary_svc.conf_char.attr_if = BT_GATTS_CharacteristicAdd(primary_svc.svc.attr_if, &uuid, ( BT_GATT_PROP_WRITE_REQ | BT_GATT_PROP_INDICATE), BT_GATT_PERM_WRITE);
#else
	primary_svc.conf_char.attr_if = BT_GATTS_CharacteristicAdd(primary_svc.svc.attr_if, &uuid, ( BT_GATT_PROP_WRITE_REQ | BT_GATT_PROP_INDICATE), (BT_GATT_PERM_WRITE | BT_GATT_PERM_WRITE_ENCRYPT_REQUEIRED));
#endif //DISABLE_GATT_PERM_ENC
#if CONFIG_AUDREY_REDUCE_MALLOC
	primary_svc.conf_char.val = malloc_value_stru(MAX_MTU_SIZE + 1);
#else //#if CONFIG_AUDREY_REDUCE_MALLOC
	primary_svc.conf_char.val = malloc_value_stru(20);
#endif //#if CONFIG_AUDREY_REDUCE_MALLOC
	primary_svc.desc_ccc_conf.attr_if = BT_GATTS_DesCCCAdd(primary_svc.conf_char.attr_if, BT_GATT_PROP_INDICATE, (BT_GATT_PROP_READ | BT_GATT_PROP_WRITE_REQ), BT_GATT_CCC_VAL_NONE);
	primary_svc.desc_ccc_conf.val = malloc_value_stru(CLI_CONF_LEN);

	// MTU information Characterisitc
	uuid128_create(&uuid,MTU_CHR_UUID);
	D("\r\n MTU information Characterisitc="); D_BUF(&uuid,16+4); DD("\r\n");
#ifdef DISABLE_GATT_PERM_ENC
	primary_svc.mtu_char.attr_if = BT_GATTS_CharacteristicAdd(primary_svc.svc.attr_if, &uuid, ( BT_GATT_PROP_READ | BT_GATT_PROP_NOTIFY), BT_GATT_PERM_READ);
#else
	primary_svc.mtu_char.attr_if = BT_GATTS_CharacteristicAdd(primary_svc.svc.attr_if, &uuid, ( BT_GATT_PROP_READ | BT_GATT_PROP_NOTIFY), (BT_GATT_PERM_READ | BT_GATT_PERM_READ_ENCRYPT_REQUEIRED));
#endif //DISABLE_GATT_PERM_ENC
	tUint8 mtu_val[2] = {0, MTU_DEFAULT_VALUE};
	primary_svc.mtu_char.val = malloc_value_stru(2);
	memcpy(primary_svc.mtu_char.val->data, mtu_val, 2);
	primary_svc.desc_ccc_mtu.attr_if = BT_GATTS_DesCCCAdd(primary_svc.mtu_char.attr_if, BT_GATT_PROP_NOTIFY, (BT_GATT_PROP_READ | BT_GATT_PROP_WRITE_REQ), BT_GATT_CCC_VAL_NONE);
	primary_svc.desc_ccc_mtu.val = malloc_value_stru(CLI_CONF_LEN);

	BT_GATTS_ServiceStart(primary_svc.svc.attr_if);

}

static void stop_gatt_service()
{
	D(" start\n\r");
	BT_GATTS_ServiceStop(primary_svc.svc.attr_if);
	//BT_GATTS_ServiceStop(included_svc.svc.attr_if);
	D(" end\n\r");
}

static void delete_gatt_service()
{
	D(" start\n\r");
	stop_gatt_service();
	free(primary_svc.data_char.val);
	free(primary_svc.ver_char.val);
	free(primary_svc.date_char.val);
	free(primary_svc.fwup_char.val);
	free(primary_svc.conf_char.val);
	free(primary_svc.mtu_char.val);
	free(primary_svc.desc_ccc_data.val);
	free(primary_svc.desc_ccc_fwup.val);
	free(primary_svc.desc_ccc_conf.val);
	free(primary_svc.desc_ccc_mtu.val);
	GattPrepWriteBuf *entry = NULL;
	list_for_each_entry (entry, &prep_write_buf_list, list, GattPrepWriteBuf) {
		free(entry->buffer.val);
		free(entry);
	}
	INIT_LIST_HEAD(&prep_write_buf_list);

	// service SHOULD BE stopped or not started, otherwise delete will fail.
	BT_GATTS_ServiceDelete(primary_svc.svc.attr_if);
	D(" end\n\r");
}

void unregister_gatt_service(void)
{
	DI(" start\n\r");
#if 1
	notify_task_running = 0;
#endif
	vTaskDelay(1100);
	//void BT_GATT_CallbackUnregister(tBT_GattEvCbkFunc *ev_cbk);
	BT_GATT_CallbackUnregister(gatt_ev_cb);

	delete_gatt_service();
//	ble_power_status = SERVER_POWER_STATUS_OFF;	/* 後続の BT_Done() の処理が終わってから STATUS_OFF にする */
	DI(" end\n\r");
}

static void gap_ev_cb(tBT_GapEvEnum ev, void *param)
{
	switch (ev) {
		case BT_GAP_EV_USER_CONFIRM_REQUEST:				/* tBT_GapUserConfirmReqStru. Call BT_UserConfirmReply to accept/refuse */
			DI("[bt_gatt_server] GAP event: BT_GAP_EV_USER_CONFIRM_REQUEST\r\n");
			break;
		case BT_GAP_EV_USER_PASSKEY_REQUEST:				/* tBT_GapUserPasskeyReqStru. Call BT_PasskeyEntryReply to accept/refuse */
			DI("[bt_gatt_server] GAP event: BT_GAP_EV_USER_PASSKEY_REQUEST\r\n");
			break;
		case BT_GAP_EV_USER_PASSKEY_NOTIFICATION:			/* tBT_GapUserPasskeyNotifStru. Display the passkey to end user */
			DI("[bt_gatt_server] GAP event: BT_GAP_EV_USER_PASSKEY_NOTIFICATION\r\n");
			break;
		case BT_GAP_EV_SIMPLE_PAIRING_COMPLETE:				/* tBT_GapSimplePairingCmplStru. Kill the window to display passkey */
			rf_ctrl_gap_simple_pairing_complete_cb((tBT_GapSimplePairingCmplStru *)param);
			break;
		case BT_GAP_EV_INQUIRY_RESULT:
		case BT_GAP_EV_INQUIRY_COMPLETE:
		case BT_GAP_REMOTE_DEVICE_NAME_UPDATED:
#if CONFIG_AUDREY_ALWAYS_BT_ON
			scan_gap_ev_cb(ev, param);
#endif //#if CONFIG_AUDREY_ALWAYS_BT_ON
			break;	// Inquiry related event, handled by at_gap_ev_cb
		default:
			Log_Error("[Warning] [bt_gatt_server] [gap_ev_cb] Unhandled GAP event: %x\r\n",ev);
			break;
	}
}
static void bt_init_done_cb(tBT_ResultEnum result, void *param)
{
	if (result == BT_RES_OK) {
		DI("[bt_gatt_server] BT initialized\r\n");

		DI("[bt_gatt_server] Register GAP event callback\r\n");
		BT_GAP_PairingFeatureSet(BT_GAP_IOCAP_NO_INPUT_NO_OUTPUT, BT_GAP_SECU_REQ_BOND);
		BT_GAP_CallbackReg(gap_ev_cb);
		svr_set_adv_data();
		DI("[bt_gatt_server] Register GATT Service\r\n");
		register_gatt_service();
		init_buffers();
		ble_power_status = SERVER_POWER_STATUS_ON;
	}
	else {
		Log_Error("\r\n[bt_gatt_server] [init_done_cb] Error, error code (tBT_ResultEnum): %x\r\n",result);
	}
}

static void bt_off_cb (tBT_ResultEnum result, void *param)
{
	if (result == BT_RES_OK) {
		DI(" [bt_gatt_server] BT deinitialized\n\r");
		ble_power_status = SERVER_POWER_STATUS_OFF;
	}
	else {
		Log_Error("\n\r [bt_gatt_server] ERROR: Deinit BT Failed! (%x)\n\r",result);
	}
}

static void bt_gatt_server_task(void)
{
	DI("\n\r[%s] start\n\r", __FUNCTION__);
	D("\r\n[bt_gatt_server] Initializing BT ...\r\n");
	BT_Init(bt_init_done_cb);
	D("\n\r[%s] BT_Init exit\n\r", __FUNCTION__);
	//while(gatt_server_state) {
	//	sys_sem_wait(&dns_wait_sem);
	//}
	vTaskDelay(3000);
	DI("\n\r[%s] end\n\r", __FUNCTION__);
	vTaskDelete(NULL);
}

bool is_enabele_data_transport_desc(void)
{
	tUint16 *cccd = NULL;
	cccd = (tUint16 *) primary_svc.desc_ccc_data.val->data;
	if (*cccd == BT_GATT_CCC_VAL_INDICATION) {
		return TRUE;
	}

	return FALSE;
}

void bt_send_data_transport_char(char *data, tUint32 len)
{
	Log_Debug("\r\n[data_transport][ind] %02x%02x%02x%02x%s\r\n", *data, *(data+1), *(data+2), *(data+3), (data+4));

	ble_io_status	 = SERVER_IO_STATUS_DATA_UPLOAD;
	notify_task_data = data;
	notify_task_len	 = len;
	return;
}

bool is_enabele_configuration_desc(void)
{
	tUint16 *cccd = NULL;
	cccd = (tUint16 *) primary_svc.desc_ccc_conf.val->data;
	if (*cccd == BT_GATT_CCC_VAL_INDICATION) {
		return TRUE;
	}
	return FALSE;
}

void bt_send_configuration_char(char *data, tUint32 len)
{
	notify_conf_data	 = data;
	notify_conf_data_len = len;
	return;
}

#if 1

/**
 * BLE で　テキストデータを送信する
 *  [TODO] 今の実装では、notify_taskから送信する
 * @param char *data 送信データ
 * @param int len 送信長さ
 *
 * @retval 1以上 lenと同じ値　成功
 * @retval -1 エラー
 */
int gatt_write_notify(char *data, int len)
{
	DT("start");
#if 1
	int ret = len;

	DI(" start");
	D(" len=%d data[%s]\r\n", len, data);

	if (len == 0) {
		Log_Error("\r\n length error");
		return -1;
	}
	if (data == NULL) {
		Log_Error("\r\n data NULL error");
		return -1;
	}
	if (ble_power_status != SERVER_POWER_STATUS_ON) {
		Log_Error("\r\n ble not ready");
		return -1;
	}

	if (notify_task_running == 0) {
		Log_Error("\r\n ble not notify_task running");
		return -1;
	}

#if 1
	if (ble_io_status != SERVER_IO_STATUS_READY) {
		Log_Error("\r\n ble not ready(%d)", ble_io_status);
		return -1;
	}
#else
	while (ble_io_status != SERVER_IO_STATUS_READY) {
		if (ble_io_status == SERVER_IO_STATUS_OFF) {
			Log_Error("\r\n ble not connected");
			return -1;
		}
		if (notify_task_running == 0) {
			Log_Error("\r\n ble not notify_task running");
			return -1;
		}
		vTaskDelay(1000); // [TODO]
		DD(".");
	}
#endif

	// notify task send
	notify_task_data = data;
	notify_task_slen = len;
	D(" notify_task_slen=%d [%s]", notify_task_slen, notify_task_data);

	ble_io_status = SERVER_IO_STATUS_WRITE;
	D(" wait ... SERVER_IO_STATUS_WRITE_COMPLETE");
	while (ble_io_status != SERVER_IO_STATUS_WRITE_COMPLETE) {
		if (ble_io_status == SERVER_IO_STATUS_OFF) {
			Log_Error("\r\n ble wirte cancel");
			ret = -1;
			break;
		}
		if (notify_task_running == 0) {
			Log_Error("\r\n ble not notify_task running");
			return -1;
		}
		vTaskDelay(500); // [TODO]
		DD(".");
	}

	vTaskDelay(100);
	ble_io_status = SERVER_IO_STATUS_READY;
	notify_task_data = NULL;

	DI(" WRITE NOTIFY end ret=%d", ret);
	DT("end");
	return ret;
#else
	return -1;
#endif
}


static int cmd_write_notify_len;
static char  cmd_write_notify_data[1024];

static void cmd_wirte_notify_task(void *param)
{
	DT("start");
	gatt_write_notify(cmd_write_notify_data, cmd_write_notify_len);
	vTaskDelete(NULL);
	DT("endt");
}
static void cmd_start_gatt_write_notify()
{
	DT("start");
	if(xTaskCreate((TaskFunction_t)cmd_wirte_notify_task, (char const *)"cmd_wn_task", 2048, NULL, tskIDLE_PRIORITY + 1, NULL) != pdPASS){
		Log_Error("\n\r[%s][ERROR] Create gatt server cmd_wirte_notify_task failed", __FUNCTION__);
		return;
	}
	DT("end");
}


/**
 * BLE で　テキストデータを送信する
 *  [TODO] 今の実装では、2種類の送信方法で処理する
 *   (1) クライアントからのread要求に対するレスポンスとして送信する
 *   (2) notify_taskから連続送信する
 * @param char *data 送信データ
 * @param int len 送信長さ
 *
 * @retval 1以上 lenと同じ値　成功
 * @retval -1 エラー
 */
int gatt_write(char *data, int len)
{
	int ret = len;

	DI(" start");
	D(" len=%d \r\n", len);

	if (len == 0) {
		Log_Error("\r\n length error");
		return -1;
	}
	if (data == NULL) {
		Log_Error("\r\n data NULL error");
		return -1;
	}
	if (ble_power_status != SERVER_POWER_STATUS_ON) {
		Log_Error("\r\n ble not ready");
		return -1;
	}

	if (ble_io_status != SERVER_IO_STATUS_READY) {
		Log_Error("\r\n ble not ready(%d)", ble_io_status);
		return -1;
	}

	// indication
	D(" wait indication enable")
	tUint16 *cccd;
	int cnt = 0;
	cccd = (tUint16 *) primary_svc.desc_ccc_fwup.val->data;
	while (((*cccd) != BT_GATT_CCC_VAL_INDICATION) && (cnt < 120)) {
		vTaskDelay(500); // [TODO]
		DDI(".i(%d)", *cccd);
		cnt++;
	}

	if ((*cccd) != BT_GATT_CCC_VAL_INDICATION) {
		Log_Error("\r\n indication does not enable");
		return -1;
	}

	notify_task_data = data;
	notify_task_slen = len;
	DI(" notify_task_slen=%d ", notify_task_slen);
	D("["); D_BUF(notify_task_data,notify_task_slen); D("]\r\n");

	// read response
	ble_io_status = SERVER_IO_STATUS_WRITE;
	D(" wait ... SERVER_IO_STATUS_WRITE_COMPLETE");
	while (ble_io_status != SERVER_IO_STATUS_WRITE_COMPLETE) {
		if (ble_io_status == SERVER_IO_STATUS_OFF) {
			Log_Error("\r\n ble wirte cancel\r\n");
			ret = -1;
			break;
		}
		vTaskDelay(500); // [TODO]
		DD(".w(%d)", notify_task_slen);
	}

	vTaskDelay(10);
	ble_io_status = SERVER_IO_STATUS_READY;
	notify_task_data = NULL;

	DI(" WRITE end ret=%d", ret);
	return ret;
}

static int cmd_write_len;
static char  cmd_write_data[1024];

static void cmd_wirte_task(void *param)
{
	gatt_write(cmd_write_data, cmd_write_len);
	vTaskDelete(NULL);
}
static void cmd_start_gatt_write()
{
	if(xTaskCreate((TaskFunction_t)cmd_wirte_task, (char const *)"cmd_w_task", 2048, NULL, tskIDLE_PRIORITY + 1, NULL) != pdPASS){
		Log_Error("\n\r[%s][ERROR] Create gatt server cmd_wirte_task failed", __FUNCTION__);
		return;
	}
}

int gatt_read_left = 0;
void gatt_set_read_left(int left)
{
	gatt_read_left = left;
	D("left = %d", gatt_read_left);
}

/**
 * BLE で　テキストデータを受信する
 *  【TODO] 実装では、クライアントからのwriteのデータを取得した段階で read()の戻り値とする。
 *
 * @param char *buf
 * @param int max_len 受信最大長さ
 *
 * @retval 1以上 lenと同じ値　成功
 * @retval 0
 * @retval -1 エラー
 */



int gatt_read(char *buf, int max_len)
{
	int read_len;

	DI(" start %d", max_len);
	if (max_len == 0) {
		Log_Error("\r\n length error");
		return -1;
	}
	if (buf == NULL) {
		Log_Error("\r\n buf NULL error");
		return -1;
	}
	if (ble_power_status != SERVER_POWER_STATUS_ON) {
		Log_Error("\r\n[ERROR]%s:%d ble power status not ready(%d)", __FUNCTION__, __LINE__, ble_power_status);
		return -1;
	}
	if (ble_io_status != SERVER_IO_STATUS_READY) {
		Log_Error("\r\n[ERROR]%s:%d ble io status not ready(%d)", __FUNCTION__, __LINE__, ble_io_status);
		return -1;
	}

	ble_io_status = SERVER_IO_STATUS_READ; //[TODO] OK?
	ble_read_len = 0; //[TODO] ok?
	if (max_len > (WRITE_CB_RING_BUF_LENGTH / 4)) {
		read_len = WRITE_CB_RING_BUF_LENGTH / 4;
	} else {
		read_len = max_len;
	}
	if (read_len > READ_BUF_LENGTH) {
		read_len = READ_BUF_LENGTH;
	}
	ble_read_len_max = read_len;

	D(" read wait(%d)", read_len);
	vTaskDelay(20); // [TODO]
	write_cb_check_count = 0;
	while (ble_io_status != SERVER_IO_STATUS_READ_COMPLETE) {
		if (ble_io_status == SERVER_IO_STATUS_OFF) {
			Log_Error("\r\n[ERROR] ble not connect power off");
					return -1;
		}
		if (ble_io_status == SERVER_IO_STATUS_NOT_READY) {
			Log_Error("\r\n[ERROR] ble not connect disconnect");
					return -2;
		}
		vTaskDelay(500); // [TODO]
		write_cb_check(read_len);
		DD(".");
	}

	ble_read_len_max = 0;
	if (ble_read_len > max_len) { //[TODO]
		DI("length over error ble_read_len(%d) > max_len(%d)", ble_read_len, max_len);
		ble_read_len = max_len;
	}

	// test wait
	if (gatt_test_wait & 1) {
		vTaskDelay(20);
	}
	if (gatt_test_wait & 2) {
			vTaskDelay(40);
		}
	if (gatt_test_wait & 4) {
				vTaskDelay(80);
			}

	memcpy(buf, ble_read_buf, ble_read_len);

	if (debug_print_short) {
		DI("read (%d)",ble_read_len );
	} else {
		if (ble_read_len < 32) {
			DI("read len=%d data [",ble_read_len );
			DI_BUF(buf, ble_read_len);
			DDI("]\r\n");
		} else {
			DI("read len=%d data top [",ble_read_len );
			DI_BUF(buf, 16); DDI("]");
			DI("data tail [");
			DI_BUF(&buf[ble_read_len-16], 16);
			DDI("]\r\n");
		}
	}

	vTaskDelay(10); //[TODO]

	ble_io_status = SERVER_IO_STATUS_READY;
	DI(" READ end len=%d", ble_read_len);
	return ble_read_len;
}

static int cmd_read_len;
static char  *cmd_read_buff;

static void cmd_read_task(void *param)
{
	gatt_read(cmd_read_buff, cmd_read_len);
	vTaskDelete(NULL);
}
static void cmd_start_gatt_read()
{
	if(xTaskCreate((TaskFunction_t)cmd_read_task, (char const *)"cmd_r_task", 2048, NULL, tskIDLE_PRIORITY + 1, NULL) != pdPASS){
		Log_Error("\n\r[%s][ERROR] Create gatt server cmd_read_task failed", __FUNCTION__);
		return;
	}
}


/**
 * writeしてからreadした結果を返す
 * @param wlen ... write data length
 * @param wdata ... write data
 * @param rdata ... read data buffer
 * @param max_len ... read data max length
 * @retval int ... read length
 */
int gatt_write_and_read(char *wdata, int wlen, char *rdata, int max_len)
{
	int ret;

	DI("start");
	ret = gatt_write(wdata, wlen);
	if (ret == wlen) {
		ret = gatt_read(rdata, max_len);
	}
	DI("end ret=%d", ret);
	return ret;
}

#endif/**
 * BT GATT サーバー開始
 * BT ON にしてから
 * サーバーを開始する
 * ADV送信を開始する
 * 接続又は切断されたら以下のコールバックを呼ぶ
 * @param connect_cb ... (tBT_Gatt_cb)(tBle_ServerIoStatusEnum result);
 *     BTでのペアリングされて、通信可能状態になった時にコールされる
 *
 * @param disconnect_cb ... (tBT_Gatt_cb)(tBle_ServerIoStatusEnum result);
 *　　　　　BTでのペアリングが解除された時にコールされる
 *     (gattサーバー動作は継続しますので、再度ペアリング可能です)
 */
int start_bt_gatt_server(tBT_Gatt_cb *connect_cb, tBT_Gatt_cb *disconnect_cb)
{
	int err;

	gat_send_wait_time = 100; //[TODO]

	ble_power_status = SERVER_POWER_STATUS_OFF;
	ble_io_status = SERVER_IO_STATUS_OFF;

	fReconnect = TRUE;

	err = sys_sem_new(&gatt_task_wait_sem, 0);
	if (err != ERR_OK) {
		Log_Error("\r\n[%s][ERROR] sys_sem_new gatt_task_wait_sem ", __FUNCTION__);
		return -1;
	}

	connected_cb = connect_cb;
	disconnected_cb = disconnect_cb;
	D("\r\ngat_send_wait_time=%d",gat_send_wait_time);
	if(xTaskCreate((TaskFunction_t)bt_gatt_server_task, (char const *)"bt_gatt_server_task", 2048, NULL, tskIDLE_PRIORITY + 1, NULL) != pdPASS){
		Log_Error("\n\r[%s][ERROR] Create gatt server task failed", __FUNCTION__);
		return -1;
	}

	return 0;

}

/**
 * BT GATT サーバー停止
 * サーバーを停止する
 * BT　OFFにする
 */
void stop_bt_gatt_server(void)
{
	int cnt = 0;
	DI(" start");
	if (ble_power_status == SERVER_POWER_STATUS_OFF) {
		DI(" not started return");
		return;
	}
	ble_io_status = SERVER_IO_STATUS_OFF;
	unregister_gatt_service();
	vTaskDelay(1000);
	BT_Done(bt_off_cb, 1);
	while (ble_power_status != SERVER_POWER_STATUS_OFF) {
		vTaskDelay(100 * portTICK_PERIOD_MS);
		if (++cnt > 50) {
			Log_Error("\r\n wait BT_Done timeout\r\n");
			sys_sem_free(&gatt_task_wait_sem);
			/* 以降のBT関連処理の正常動作が期待できないためアラート通知でリセットさせる */
			SendMessageToStateManager(MSG_ERR_BT_LINK_FATAL, PARAM_BT_LINK_POWER_OFF_INCOMPLETE);
			return;
		}
	}
	sys_sem_free(&gatt_task_wait_sem);
	DI(" end");
}

#if CONFIG_AUDREY_ALWAYS_BT_ON
void disable_advertising(void)
{
	DI("[bt_gatt_server] Disable Advertising\r\n");
	adv_disable_flg = TRUE;
}

void enable_advertising(void)
{
	DI("[bt_gatt_server] Enable Advertising\r\n");
	adv_disable_flg = FALSE;
}
#endif //#if CONFIG_AUDREY_ALWAYS_BT_ON

#if 0
void stop_bt_gatt_server_fname_line(const char *fname, int line)
{
	DI("called func=%s ine=%d", fname, line);
	_stop_bt_gatt_server();
}
#endif

#define DISC_BEFORE_STOP_SERVER_WAIT_MAX	200
void gatt_conn_disc(bool reconnect)
{
	int cnt = 0;
	if (primary_svc.conn == NULL) {
		return;
	}
	fReconnect = reconnect;
	BT_GATT_DisconnectReq(primary_svc.conn);
	ble_io_status = SERVER_IO_STATUS_DISC_BEFORE_STOP_SERVER;
	do {
		vTaskDelay(100);
		if (++cnt > DISC_BEFORE_STOP_SERVER_WAIT_MAX) {
			DI(" wait gatt disconnection before stop server timeout : state=%d", ble_io_status);
			/* 以降のBT関連処理の正常動作が期待できないためアラート通知でリセットさせる */
			SendMessageToStateManager(MSG_ERR_BT_LINK_FATAL, PARAM_BT_LINK_DISC_INCOMPLETE);
			return;
		}
	} while (ble_io_status != SERVER_IO_STATUS_NOT_READY);
	return;
}

/**
 * BT GATT サーバー状態取得
 */
int status_bt_gatt_server(void)
{
	D(" ble_io_status=%d", ble_io_status);
	return ble_io_status;
}
#if 1
#define GATT_CONNECT_WAIT_TIME_SECOND 120
int gatt_open_ready = 0;
void bt_gatt_open_connect_cb(tBle_ServerIoStatusEnum result)
{

	Log_Always("\r\n[%s] ##### connected(%d)#####",__FUNCTION__, result);
	gatt_open_ready = 1;
	SendMessageToStateManager(MSG_BT_LINK_COMP, 0);
}

void bt_gatt_open_disconnect_cb(tBle_ServerIoStatusEnum result)
{
	Log_Always("\r\n[%s] ##### disconnected(%d)#####", __FUNCTION__, result);
	gatt_open_ready = 0;
	ble_io_status = SERVER_IO_STATUS_OFF; //[TODO]
	//SendMessageToStateManager(MSG_BT_LINK_OFF, 0); // 必要なし
}

void gatt_open_task(void *param)
{
	gatt_open_ready = 0;
	start_bt_gatt_server(bt_gatt_open_connect_cb, bt_gatt_open_disconnect_cb);
	//while(gatt_open_ready == 0) {
	//	vTaskDelay(100);
	//}
	vTaskDelete(NULL);
}
/**
 * BT　をONにしてから、gattサーバーを立ち上げてペアリングされるまで待つ
 * 接続時に　MSG_BT_LINK_COMPメッセージを送信
 * 切断時に MSG_BT_LINK_OFFメッセージを送信
 */
int gatt_open(void)
{
	int ret = 0;
	int count = 0;
	DI(" start");
	gatt_open_ready = 0;
	gatt_clear_write_cb_buf();
	gatt_set_read_left(2048); //[TODO]
	if(xTaskCreate((TaskFunction_t)gatt_open_task, (char const *)"gatt_open_task", 2048, NULL, tskIDLE_PRIORITY + 1, NULL) != pdPASS){
		Log_Always("\n\r[%s] Create gatt_open_task failed", __FUNCTION__);
		return -1;
	}
	while(gatt_open_ready == 0 && count++ < (GATT_CONNECT_WAIT_TIME_SECOND*10)) {
		vTaskDelay(100);
		DD(".");
	}
	//TimerStart_Rsp(500);

	//[TODO] test
	DBG write_cb_sum = 0;
	DBG write_cb_len = 0;
	if (gatt_open_ready == 1) {
		ret = 0;
		D("opened")
	} else {
		stop_bt_gatt_server();
		ret = -1;
		D("open time out");
	}
	DI("end");
	return ret;
}

void gatt_close_task(void *param)
{
	DI(" start");
	gatt_open_ready = -1; // gatt_open待ちから抜ける
	vTaskDelay(110);
	if (ble_io_status != SERVER_IO_STATUS_OFF || ble_power_status != SERVER_POWER_STATUS_OFF) {
		stop_bt_gatt_server();
	} else {
		Log_Always("\r\n[%s] not opened ",__FUNCTION__);
	}
	DI(" end");
	vTaskDelete(NULL);
}

/**
 * gattサーバーを停止してBTをOFFにする
 */
void gatt_close(void)
{
	D(" start");
	// スタックオーバーフローを回避する為にタスクで処理する
	if(xTaskCreate((TaskFunction_t)gatt_close_task, (char const *)"gatt_close",2048, NULL, tskIDLE_PRIORITY + 1, NULL) != pdPASS){
		Log_Always("\n\r[%s] Create gatt_close_task failed", __FUNCTION__);
	}
	vTaskDelay(500); //[TODO]
	D(" end");
}

// ++++++++++++++++++++++  AT command +++++++++++++++++++++++++++
void bt_gatt_start_cb(tBle_ServerIoStatusEnum result)
{
	Log_Always("\r\n[%s] ##### connected(%d)#####",__FUNCTION__, result);
}

void bt_gatt_stop_cb(tBle_ServerIoStatusEnum result)
{
	Log_Always("\r\n[%s] ##### disconnected(%d)#####", __FUNCTION__, result);
}
#endif

static char *ble_power_status_string[] = {
	"SERVER_POWER_STATUS_OFF",
	"SERVER_POWER_STATUS_ON_PEND",
	"SERVER_POWER_STATUS_ON_ONLY_PEND",
	"SERVER_POWER_STATUS_ON",
	"SERVER_POWER_STATUS_OFF_PEND"
};



/**
 * ATコマンド
 * ATAG=0 停止
 * ATAG=1 開始
 */
void cmd_bt_gatt_server(int argc, char **argv){
#if 1
	Log_Always("\r\n[%s] start", __FUNCTION__);

	if (argc == 2 && strcmp(argv[1],"READ")==0) {
		Log_Always("\r\nREAD");
		//gatt_read(read_buf, 512);
		cmd_read_len = 51;
		cmd_read_buff = read_buf;
		cmd_start_gatt_read();
		return;
	} else if (argc == 3 && strcmp(argv[1],"WRITE")==0) {
		Log_Always("\r\b WRITE [%s]", argv[2]);
		cmd_write_len = strlen(argv[2]);
		strcpy(cmd_write_data,argv[2]);
		//gatt_write(strlen(argv[2]), argv[2]);
		cmd_start_gatt_write();
		return;
	} else if (argc == 3 && strcmp(argv[1],"NOTIFY")==0) {
		Log_Always("\r\b WRITE NOTIFY [%s]", argv[2]);
		//gatt_write_notify(strlen(argv[2]), argv[2]);
		cmd_write_notify_len = strlen(argv[2]);
		strcpy(cmd_write_notify_data,argv[2]);
		cmd_start_gatt_write_notify();
		return;
	} else if (argc == 3 && strcmp(argv[1],"WANDR")==0) {
		Log_Always("\r\b WRITE and READ [%s]", argv[2]);
		gatt_write_and_read(read_buf, strlen(argv[2]), read_buf, 512);
		return;
	} else if (argc == 3 && strcmp(argv[1],"TIME")==0) {
		Log_Always("\r\b TIME [%d]", atoi(argv[2]));
		sntp_set_time_by_bt(atoi(argv[2]));
		return;
	}  else if (argc == 2 && strcmp(argv[1],"STAT")==0) {
		tUint16 *cccd = NULL;
		Log_Always("\r\n ble_io_status   = %d %s", ble_io_status, ble_io_status_string[ble_io_status]);
		Log_Always("\r\n ble_power_status= %d %s", ble_power_status, ble_power_status_string[ble_power_status]);
		cccd = (tUint16 *) primary_svc.desc_ccc_data.val->data;
		if (cccd && (*cccd) == BT_GATT_CCC_VAL_INDICATION) {
			Log_Always("\r\ndesc_ccc_data cccd=%d BT_GATT_CCC_VAL_INDICATION", *cccd);
		}
		cccd = (tUint16 *) primary_svc.desc_ccc_fwup.val->data;
		if ((*cccd) == BT_GATT_CCC_VAL_INDICATION) {
			Log_Always("\r\ndesc_ccc_fwup   cccd=%d BT_GATT_CCC_VAL_INDICATION", *cccd);
		}
#if CHR_NOTIFY
		cccd = (tUint16 *) primary_svc.desc_ccc_notify.val->data;
		if ((*cccd) == BT_GATT_CCC_VAL_NOTIFICATION) {
			Log_Always("\r\ndesc_ccc_notify   cccd=%d BT_GATT_CCC_VAL_NOTIFICATION", *cccd);
		}
#endif
#if CHR_ALL
		cccd = (tUint16 *) primary_svc.desc_all_ccc.val->data;
		if ((*cccd) == BT_GATT_CCC_VAL_NOTIFICATION) {
			Log_Always("\r\ndesc_all_ccc      cccd=%d BT_GATT_CCC_VAL_NOTIFICATION", *cccd);
		} else if ((*cccd) == BT_GATT_CCC_VAL_INDICATION) {
			Log_Always("\r\ndesc_all_ccc      cccd=%d BT_GATT_CCC_VAL_INDICATION", *cccd);
		}
#endif
		Log_Always("\r\n write_cb_count=%d  write_cb_sum_len=%d", write_cb_count, write_cb_sum_len);
		Log_Always("\r\n");
		return;
	}  else if (argc == 2 && strcmp(argv[1],"CLR")==0) {
		write_cb_count = 0;
		write_cb_sum_len = 0;
		return;
	}  else if (argc == 2 && strcmp(argv[1],"START")==0) {
		start_bt_gatt_server(bt_gatt_start_cb, bt_gatt_stop_cb);
		return;
	}  else if (argc == 2 && strcmp(argv[1],"STOP")==0) {
		stop_bt_gatt_server();
		return;
	}  else if (argc == 2 && strcmp(argv[1],"OPEN")==0) {
		gatt_open();
		return;
	} else if (argc == 2 && strcmp(argv[1],"CLOSE")==0) {
		gatt_close();
		return;
	} else if (argc == 3 && strcmp(argv[1],"DBLEV")==0) {
		debug_print_lebel = atoi(argv[2]);
		printf("\r\n debug_print_lebel=%d", debug_print_lebel);
		return;
	} else if (argc == 2 && strcmp(argv[1],"D3")==0) {
		debug_print_lebel = 3;
		printf("\r\n debug_print_lebel=%d", debug_print_lebel);
		return;
	} else if (argc == 2 && strcmp(argv[1],"D5")==0) {
		debug_print_lebel = 5;
		printf("\r\n debug_print_lebel=%d", debug_print_lebel);
		return;
	} else if (argc == 3 && strcmp(argv[1],"SHORT")==0) {
		debug_print_short = atoi(argv[2]);
		printf("\r\n debug_print_short=%d", debug_print_short);
		return;
	} else if (strcmp(argv[1],"OTA")==0) {
		extern void cmd_ota_ble_update(int argc, char **argv);
		cmd_ota_ble_update(argc,argv);
		return;
	}  else if (argc == 3 && strcmp(argv[1],"ADINT")==0) {
		debug_print_lebel = atoi(argv[2]);
		printf("\r\n debug_print_lebel=%d", debug_print_lebel);
		return;
	}
	 else if (argc == 3 && strcmp(argv[1],"DBAUTO")==0) {
		 debug_print_lebel_auto = atoi(argv[2]);
			printf("\r\n debug_print_lebel_auto=%d", debug_print_lebel_auto);
			return;
		}
	 else if (argc == 3 && strcmp(argv[1],"RSPTM")==0) {
		 write_cb_timer_enable = atoi(argv[2]);
			printf("\r\n write_cb_timer_enable=%d", write_cb_timer_enable);
			return;
		}
	 else if (argc == 3 && strcmp(argv[1],"WAIT")==0) {
	 		 gatt_test_wait = atoi(argv[2]);
	 			printf("\r\n gatt_test_wait=0x%x", gatt_test_wait);
	 			return;
	 		}
	//write_cb_timer_enable

	//debug_print_lebel


	if (argc >= 2 && strcmp(argv[1],"1") ==0) {
		if (argc == 3) {
			Log_Always("\r\n argv[2]=%s",argv[2]);
			if (argv[2]) {
				gat_send_wait_time = atoi(argv[2]);
			}
		} else {
			gat_send_wait_time = 1000;
		}
		Log_Always("\r\ngat_send_wait_time=%d",gat_send_wait_time);
		if(xTaskCreate((TaskFunction_t)bt_gatt_server_task, (char const *)"bt_gatt_server_task", 2048, NULL, tskIDLE_PRIORITY + 1, NULL) != pdPASS){
			Log_Always("\n\r[%s] Create gatt server task failed", __FUNCTION__);
		}
	} else {
		Log_Always("\r\nATAG=0/1,n  n...packet wait time");
		Log_Always("\r\nATAG=1[,n] ... gatt server start  n...send wait time (ms)");
		Log_Always("\r\nATAG=0 ... gatt server stop");
		Log_Always("\r\nATAG=OPEN ... gatt server open");
		Log_Always("\r\nATAG=CLOSE ... gatt server close");
		Log_Always("\r\nATAG=READ ... read data");
		Log_Always("\r\nATAG=WRITE, xxxxx  ... write data");
		Log_Always("\r\nATAG=NOTIFY, xxxxx  ... write notify task data");
		Log_Always("\r\nATAG=WANDR, xxxxx  ... write data and read data");
		Log_Always("\r\nATAG=STAT ... display status");
		Log_Always("\r\nATAG=DBLEV,n ... print debug lebel ");
		Log_Always("\r\nATAG=D3 ... print debug lebel=3 ");
		Log_Always("\r\nATAG=SHORT,n ... print short msg 0...off  1...on(short msg) ");
		Log_Always("\r\nATAG=OTA,{SC/AM/SCO/AMO} ... ble fw update");
	}
#endif
}
#endif /* CONFIG_BT && CONFIG_BT_GATT_SERVER */
