#include "platform_opts.h"
#if CONFIG_BT

#include "FreeRTOS.h"
#include "task.h"
#include <sys.h>
#include <device_lock.h>
#include <sys_api.h>

#include "bt_gap.h"
#include "bt_util.h"
#include "bt_common.h"

#include "timers.h"

#include "state_manager.h"
#include "badge.h"
#include "ble_scan.h"

#define LE_SCAN_ONCE_TIME	(1)
#define LE_SCAN_MAX_TIME	(10)
#define LE_SCAN_MAX_COUNT	(150)

#define BD_ADDR1			0xf1
#define BD_ADDR2			0xd7
#define BD_ADDR3			0x8f
#define AD_SHARP_ID1		0x03
#define AD_SHARP_ID2		0x81

#define AD_AUDREY_NAME		AUDREY_MODEL_BADGE
#if CONFIG_AUDREY_BEACON_DEV
#define AD_AUDREY_NAME2		"HN-PC002"
#endif

#define AD_MANU_DATA_LEN		(14)
#define AD_VERSION_INDEX		(4)
#define AD_LOW_BATTERY_INDEX	(7)
#define AD_BD_ADDR_INDEX		(8)
#ifdef AD_AUDREY_NAME2
#define AD_MANU_DATA_LEN_2		(10)
#define AD_VERSION_INDEX_2		(2)
#define AD_LOW_BATTERY_INDEX_2	(3)
#define AD_BD_ADDR_INDEX_2		(4)
#endif

#define BLE_MAX_ERROR_COUNT 10

//#define DEBUG_PRINT_ENABLE
#ifdef DEBUG_PRINT_ENABLE
#define D(fmt, ...)  Log_Info("\r\n[DEBUG]%s::[%d] " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define DD(fmt, ...)  Log_Info(fmt, ##__VA_ARGS__)
#else
#define D(fmt, ...)
#define DD(fmt, ...)
#endif

#define INFO_PRINT_ENABLE
#ifdef INFO_PRINT_ENABLE
#define DI(fmt, ...)  Log_Info("\r\n[INFO]%s::[%d] " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define DI(fmt, ...)
#endif


//#define EIR_USE_MALLOC
#ifndef EIR_USE_MALLOC
static tUint8 eir_buff[32];
#endif

typedef struct {
	tBT_AddressStru addr;
	tBT_GapDeviceInfoSrc src;
	tUint8 dev_cap;
	tUint8 page_scan_rmode;
	tUint16 clock_offset;
	tUint32 class_of_device;
	tUint16 eir_size;
	tUint8 rssi;
	tUint8* eir;
} tBT_LeScanResult;


// le scan
static tBT_InquiryReqStru req;
static int scan_count = 0;
static int scan_power_mode = 0;
static int BT_GAP_Inquiry_run = 0;

// timer
TimerHandle_t	timer_id_le;
static int ble_scan_timer_cleate = 0;
#if CONFIG_AUDREY_DBG_UPLOAD
static char scan_comp_flag = 0x00;
#endif

// power status
static volatile tBle_ScanPowerStatusEnum scan_power_status = SCAN_POWER_STATUS_OFF; //[TODO]

// error
static int power_on_error_count = 0;
static int power_off_error_count = 0;

static void bt_power_off (void);


int ble_get_bt_status (void)
{
	char state_str[16] = {0};
	tBT_StatusEnum status = BT_GetState();
	if (status == BT_ST_IDLE) {
		strcpy(state_str,"OFF");
	}
	else if (status == BT_ST_INIT) {
		strcpy(state_str,"INITIALIZING");
	}
	else if (status == BT_ST_READY) {
		strcpy(state_str,"ON");
	}
	else if (status == BT_ST_REMOVING) {
		strcpy(state_str,"DEINITIALIZING");
	}
	else if (status == BT_ST_DONE) {
		strcpy(state_str,"RESERVED");
	}

	D(" BT_STATE[%d:%s]\n\r", status, state_str);
	return status;
}

/**
 * BTのスキャン間隔は最低でも1.2秒なので　タイマーで1秒おきにスキャンをやり直す
 */
static void TimerComp_LE(void)
{
	if (timer_id_le) {
		D(" timer stop\r\n");
		xTimerStop(timer_id_le, 0);
#if CONFIG_AUDREY_DBG_UPLOAD
		scan_comp_flag = 0x10;
#else
		if (scan_count != 0) {
			scan_count = 0;
			BT_GAP_Inquiry(&req, 0);
			D(" LeScan stop ... ");
		}
#endif
	}
}


static void TimerStart_LE(void)
{
	xTimerStop(timer_id_le, 0);
	if (!ble_scan_timer_cleate) {
		D("cleate timer\r\n");
		timer_id_le = xTimerCreate("timer_LeScan",
#if CONFIG_AUDREY_INDV
				// トイレに入ってから出るまでスキャンを行う為、時間を15分に変更。
				((1000 * 60 * 15) / portTICK_RATE_MS), // 15m = 1000 * 60 * 15
#else	/* CONFIG_AUDREY_INDV */
				(LE_SCAN_MAX_TIME * 1000), // 10S
#endif	/* CONFIG_AUDREY_INDV */
				pdTRUE,
				(void *)0,
				(TimerCallbackFunction_t)TimerComp_LE);
		ble_scan_timer_cleate = 1;
	}
	xTimerStart(timer_id_le, 0);
	D("timer start 0x%x", timer_id_le);
}


static void TimerStop_LE(void)
{
	D("timer stop\r\n");
	xTimerStop(timer_id_le, 0);

	if (scan_count != 0) {
		scan_count = 0;
		BT_GAP_Inquiry(&req, 0);
		vTaskDelay(1000);
		D("LeScan stop");
	}

	for (int i=0; i < 100; i++) {
		vTaskDelay(30);
		if (BT_GAP_Inquiry_run == 0) {
			break;
		}
	}

	if (BT_GAP_Inquiry_run != 0) {
		BT_GAP_Inquiry(&req, 0);
		vTaskDelay(100);
		BT_GAP_Inquiry_run = 0;
		DI(" Inquiry force stop");
	}
}



static char is_bt_ready() {
	tBT_StatusEnum status = BT_GetState();
	if (status != BT_ST_READY) {
		return 0;
	}
	else {
		return 1;
	}
}


/**
 * LEスキャンのBTからのイベントコールバック
 * Audreyバッジを受信した場合には、報告して、さらにスキャンを続ける
 * 現状では、同じBDアドレスを持つスキャンはLIB側でキャンセルされるようなので、
 * バッジを見つけたら、すぐにスキャンを終了させて、終了イベントを出させて、さらに新しくスキャンを開始することで
 * アドバタイジングパケット毎のスキャン報告を出すようにしている
 * LIB側の仕様が変わればこの処理も変更する必要がある
 */
#if CONFIG_AUDREY_ALWAYS_BT_ON != 1
static void at_gap_ev_cb(tBT_GapEvEnum ev, void *param)
#else
void scan_gap_ev_cb(tBT_GapEvEnum ev, void *param)
#endif //#if CONFIG_AUDREY_ALWAYS_BT_ON != 1
{

	switch (ev) {
		case BT_GAP_EV_INQUIRY_RESULT: {
			tBT_GapInquiryRetStru *in = param;
			tBT_LeScanResult result;

			memcpy(&result,in,sizeof(tBT_GapInquiryRetStru)-1);// don't copy eir

			//result.eir = NULL;
			if (scan_count > 0) {
				if (
#if 0
					// [TODO] f1,d7,8f ... test only
					((u8*)(result.addr.bd))[5] == BD_ADDR1
					&&  ((u8*)(result.addr.bd))[4] == BD_ADDR2
					&& ((u8*)(result.addr.bd))[3] == BD_ADDR3
#else
					// BDアドレスでのフィルタリングはしない
					1
#endif
					) {
					char info_src[10];
					tUint8 *eir;
					tUint16 size;
					tUint16 total_len;
					tUint16 pos = 0;
					tUint16 offset = 0;
					tUint8  isAudrey = 0;
#ifdef AD_AUDREY_NAME2
					tUint8  isAudrey_2 = 0;
#endif
					tUint16  volBatt = 0;
					tUint8  lowBatt = 0;
					tUint8  audrey_version = 0;
					tUint8  isSharp = 0;
					BT_AdvStru ad;

#ifdef DEBUG_PRINT_ENABLE
					// print beacon info
					char dev_type[10];
					char addr_type[10];


					sprintf(dev_type,"%s",(result.dev_cap==BT_DEVICE_TYPE_BREDR)? "BR/EDR":
										  (result.dev_cap==BT_DEVICE_TYPE_LE)? "LE":
										  (result.dev_cap==BT_DEVICE_TYPE_BOTH_BREDR_LE)? "BOTH":"UNKNOWN");
					sprintf(addr_type,"%s",(result.addr.type==BT_ADDR_TYPE_PUBLIC)? "public":
										   (result.addr.type==BT_ADDR_TYPE_RANDOM)? "random":"unknown");
					sprintf(info_src,"%s",(result.src==BT_GAP_REMOTE_INFO_SRC_INQUIRY)? "INQUIRY":
										  (result.src==BT_GAP_REMOTE_INFO_SRC_ADV_REPORT)? "ADV":
										  (result.src==BT_GAP_REMOTE_INFO_SRC_SCAN_RESPONSE)? "SCAN_RSP":"unknown");
					D("scan %-10s\t%-8s\t"BD_ADDR_FMT"\t%-8s\t RSSI:%d\n\r",
							dev_type,addr_type,BD_ADDR_ARG(result.addr.bd),info_src,(tInt8)result.rssi);
#endif


#ifdef EIR_USE_MALLOC
					result.eir = (tUint8*)malloc(in->eir_size);
					memcpy(result.eir,in->eir,in->eir_size);
					//D(" alloc result.eir(0x%x) in->eir_size=%d", result.eir, in->eir_size);

					eir = result.eir;
					size = result.eir_size;
#else
					eir = &eir_buff[0];
					size = result.eir_size;
					memcpy(eir_buff,in->eir,in->eir_size);
#endif

					if (size > 0) {
						sprintf(info_src,"%s",(result.src==BT_GAP_REMOTE_INFO_SRC_INQUIRY)? "INQUIRY":
												  (result.src==BT_GAP_REMOTE_INFO_SRC_ADV_REPORT)? "ADV":
												  (result.src==BT_GAP_REMOTE_INFO_SRC_SCAN_RESPONSE)? "SCAN_RSP":"unknown");
						DD(BD_ADDR_FMT"\t%-8s\n\r",BD_ADDR_ARG(result.addr.bd),info_src);

						total_len = size;
						pos = 0;
						offset = 0;
						while (pos < total_len) {
							D(" total_len=%d pos=%d\r\n", total_len, pos);
							offset = get_ad_data(eir+pos, total_len-pos,&ad);
							if (offset == 0) {
								Log_Error ("Error when parsing EIR data\n\r");
								break;
							}
							//D(" alloc ad.data(0x%x) ad.ad_len-1=%d", ad.data, ad.ad_len-1);
							pos += offset;
							tUint8 ad_type_str[32];
							get_ad_type_str(ad.ad_type, ad_type_str);
							DD("[%02x][%s] ",ad.ad_type,ad_type_str);
							switch(ad.ad_type) {
#if 0
								case BT_AD_TYPE_FLAGS: {
									int len = ad.ad_len-1;
									for(int i=0; i<len; i++) {
										DD("%02X ",ad.data[i]);
									}
									break;
								}
								case BT_AD_TYPE_INCOMPLETE_SERVICE_UUID_16:
								case BT_AD_TYPE_COMPLETE_SERVICE_UUID_16: {
									int len = ad.ad_len-1;
									u16 *ptr = (u16*)ad.data;
									for(int i=0; i<len; i+=2) {
										DD("%04X,",*ptr);
										ptr++;
									}
									break;
								}
								case BT_AD_TYPE_INCOMPLETE_SERVICE_UUID_32:
								case BT_AD_TYPE_COMPLETE_SERVICE_UUID_32: {
									int len = ad.ad_len-1;
									u32 *ptr = (u32*)ad.data;
									for(int i=0; i<len; i+=4) {
										DD("%08X,",*ptr);
										ptr++;
									}
									break;
								}
								case BT_AD_TYPE_INCOMPLETE_SERVICE_UUID_128:
								case BT_AD_TYPE_COMPLETE_SERVICE_UUID_128: {
									int len = ad.ad_len-1;
									for(int i=0; i*16+15<len; i++) {
										for(int j=15; j>=0; j--) {
											DD("%02X",ad.data[i*16+j]);
										}
										if (i%16==15) {
											DD(",");
										}
									}
									break;
								}
								case BT_AD_TYPE_TX_POWER_LEVEL:
									DD("%d",(char)ad.data[0]);
									break;
								case BT_AD_TYPE_SERVICE_DATA: {
									int len = ad.ad_len-1;
									DD("UUID=%04X, data=",*(u16*)ad.data);
									for(int i=2; i<len; i++) {
										DD("%02X ",ad.data[i]);
									}
									break;
								}
								case BT_AD_TYPE_APPEARANCE:
									DD("%d",*(u16*)ad.data);
									break;
#endif
								case BT_AD_TYPE_SHORT_LOCAL_NAME:
								case BT_AD_TYPE_COMPLETE_LOCAL_NAME:
									DD("%s",ad.data);
									if (strncmp(ad.data,AD_AUDREY_NAME,strlen(AD_AUDREY_NAME)) == 0) {
										DD("[Audrey]");
										isAudrey = 1;
									}
#ifdef AD_AUDREY_NAME2
									else if (strncmp(ad.data,AD_AUDREY_NAME2,strlen(AD_AUDREY_NAME2)) == 0) {
										DD("[Audrey2]");
										isAudrey_2 = 1;
									}
#endif
									break;
								case BT_AD_TYPE_MANUFACTURER_DATA: {
									int len = ad.ad_len-1;
									DD("CompanyID=%04X, ",*(u16*)ad.data);
									// check sharp ID 0x81,0x03
									if (ad.data[0] == AD_SHARP_ID2 && ad.data[1] == AD_SHARP_ID1) {
										isSharp = 1;
#ifdef AD_AUDREY_NAME2
										if (len == AD_MANU_DATA_LEN) {
											audrey_version = ad.data[AD_VERSION_INDEX];
											DD(" version=%d ", audrey_version);
											volBatt = (ad.data[5] * 0x0100) + ad.data[6];
											lowBatt = ad.data[AD_LOW_BATTERY_INDEX];
											DD(" lowBattery=%d ", lowBatt);
											if (len >= 10) {
												DD(" BD="BD_ADDR_FMT" ",BD_ADDR_ARG(&ad.data[AD_BD_ADDR_INDEX]));
											}
										}
										else if (len == AD_MANU_DATA_LEN_2) {
											audrey_version = ad.data[AD_VERSION_INDEX_2];
											DD(" version=%d ", audrey_version);
											lowBatt = ad.data[AD_LOW_BATTERY_INDEX_2];
											DD(" lowBattery=%d ", lowBatt);
											if (len >= 10) {
												DD(" BD="BD_ADDR_FMT" ",BD_ADDR_ARG(&ad.data[AD_BD_ADDR_INDEX_2]));
											}
										}
#else
										audrey_version = ad.data[AD_VERSION_INDEX];
										DD(" version=%d ", audrey_version);
										lowBatt = ad.data[AD_LOW_BATTERY_INDEX];
										DD(" lowBattery=%d ", lowBatt);
										if (len == AD_MANU_DATA_LEN) {
											DD(" BD="BD_ADDR_FMT" ",BD_ADDR_ARG(&ad.data[AD_BD_ADDR_INDEX]));
										}
#endif
									}
									DD(" data=");
									for(int i=0; i<len; i++) {
										DD("%02X ",ad.data[i]); // Version,LowBattery,BD_ADDR
									}

									break;
								}
							}
							DD("\r\n");
							if (ad.data != NULL) {
								//D("free ad.data(0x%x)", ad.data);
								free(ad.data);
								ad.data = NULL;
							}

							if (
#if 1
#ifdef AD_AUDREY_NAME2
									isSharp && (isAudrey || isAudrey_2)
#else
									isSharp && isAudrey
#endif //#ifdef AD_AUDREY_NAME2
#else
									isAudrey
#endif
									) {
								//call notify function
								//void badge_beacon_rcv(u8 *bd_addr, char rssi, char battery);
#if CONFIG_AUDREY_INDV_EMU == 0
#if CONFIG_AUDREY_DBG_UPLOAD
								badge_beacon_rcv( (u8 *)&(result.addr.bd), (char)result.rssi, (!lowBatt ? 0 : 1) + scan_comp_flag);
								DI(" badge_beacon_rcv "BD_ADDR_FMT" RSSI:%d LowBatt:%d %s %dmV\n\r",
										BD_ADDR_ARG(result.addr.bd), (char)result.rssi, (!lowBatt ? 0 : 1), (!scan_comp_flag ? "" : "in 10sec"), volBatt);
#else
								badge_beacon_rcv( (u8 *)&(result.addr.bd), (char)result.rssi, (!lowBatt ? 0 : 1));
								DI(" badge_beacon_rcv "BD_ADDR_FMT" RSSI:%d LowBatt:%d %dmV\n\r",
										BD_ADDR_ARG(result.addr.bd), (char)result.rssi, (!lowBatt ? 0 : 1), volBatt);
#endif
#endif /* CONFIG_AUDREY_INDV_EMU */
#ifdef EIR_USE_MALLOC
								if (result.eir) {
									//D("\r\n free result.eir(%0x%x)", result.eir);
									free(result.eir);
									result.eir = NULL;
								}
#endif
								return;
							}
						}
#ifdef EIR_USE_MALLOC
						if (result.eir) {
							//D("\r\n free result.eir(%0x%x)", result.eir);
							free(result.eir);
							result.eir = NULL;
						}
#endif
						DD("\n\r");
					}
				}
			}

			break;
		} // case
		case BT_GAP_EV_INQUIRY_COMPLETE: {
			tBT_GapInquiryCmpStru *in = param;
			if (in->status == BT_RES_OK) {
				D(" GAP inquiry complete\n\r");
			} else if (in->status == BT_RES_USER_TERMINATED) {
				D(" GAP inquiry stopped \n\r");
			} else {
				Log_Error(" [BT_GAP_EV_INQUIRY_COMPLETE] Error, error code (tBT_ResultEnum): 0x%x\n\r",in->status);
			}

			if (scan_count > 1) {
#ifndef CONFIG_AUDREY_DBG_UPLOAD
				//scan_count--;
#endif
				D("scan_count = %d\r\n", scan_count);
				vTaskDelay(500);
				BT_GAP_Inquiry(&req, 1);
				BT_GAP_Inquiry_run = 1;
				break;
			} else {
				DI(" BT_GAP_Inquiry end\r\n");
				if (scan_power_mode == 1) {
					if (scan_power_status == SCAN_POWER_STATUS_ON) {
						DI(" call bt_power_off()");
						bt_power_off();
					}
				}
				BT_GAP_Inquiry_run = 0;
			}

			break;
		}
		case BT_GAP_REMOTE_DEVICE_NAME_UPDATED: {
			tBT_GapGetRemoteNameCmpStru *in = param;
			D("GAP inquiry device name update: "BD_ADDR_FMT"\t%s",BD_ADDR_ARG(in->bd),in->remote_name);
			break;
		}
		default:
			break;
	}

}

/**
 * LEスキャンを開始する
 */
void setup_le_scan (void) {

	if (!is_bt_ready()) {
		DI("\r\nsetup_le_scan: BT is not ready\r\n");
		// error notify
		DI(" PARAM_SCAN_NOT_READY"); //[TODO]
		SendMessageToStateManager(MSG_ERR_SCAN, PARAM_SCAN_NOT_READY);

		if (scan_power_mode) {
			if (scan_power_status == SCAN_POWER_STATUS_ON) {
				bt_power_off();
			}
		}
		return;
	}

#if CONFIG_AUDREY_ALWAYS_BT_ON != 1
	BT_GAP_CallbackReg(at_gap_ev_cb);
#endif //#if CONFIG_AUDREY_ALWAYS_BT_ON != 1

	scan_count = LE_SCAN_MAX_COUNT;


	req.dev_type = BT_DEVICE_TYPE_LE;

	req.length = 0x00;

	req.num_rsp = 10;

	req.enable_ble_duplicate_filtering = 0;
	req.enable_ble_scan_req = 1;
	req.le_scan_interval = 0x0640;	// Range: 0x0004 to 0x4000, Time = N * 0.625 msec, Time Range: 2.5 msec to 10.24 Sec
	req.le_scan_window = 0x0640;	// Range: 0x0004 to 0x4000, Time = N * 0.625 msec, Time Range: 2.5 msec to 10.24 Sec


	BT_GAP_Inquiry(&req, 1);

	BT_GAP_Inquiry_run = 1;

	DI(" BT_GAP_Inquiry start!\r\n");

	TimerStart_LE();

}

/**
 * BT ON のコールバック
 * setup_le_scan()を実行してLEスキャンを開始する
 */
static void bt_on_cb (tBT_ResultEnum result, void *param)
{
	tBle_ScanPowerStatusEnum status = scan_power_status;

	if (result == BT_RES_OK) {
		scan_power_status = SCAN_POWER_STATUS_ON;
#if CONFIG_AUDREY_ALWAYS_BT_ON != 1
		DI("BT initialized OK\n\r");
#endif //#if CONFIG_AUDREY_ALWAYS_BT_ON != 1
		if (status == SCAN_POWER_STATUS_ON_PEND || status == SCAN_POWER_STATUS_ON) {

			setup_le_scan();
		}

		power_on_error_count = 0;
	}
	else {
		scan_power_status = SCAN_POWER_STATUS_OFF;
		Log_Error("\n\rERROR: bt_on_cb::Init BT Failed! (%x)\n\r",result);

		// error notify
		SendMessageToStateManager(MSG_ERR_SCAN, PARAM_SCAN_CB_ERROR);

		power_on_error_count ++;
		if (power_on_error_count > BLE_MAX_ERROR_COUNT) {
			//  reset
			Log_Error("\r\nbt_on_cb:: to be reset now!");
			SendMessageToStateManager(MSG_ERR_SCAN_FATAL, PARAM_SCAN_FATAL_ERROR);
			//vTaskDelay(3000);
			//sys_reset();
		}

	}
}

#if 0
static void bt_on_cb_only (tBT_ResultEnum result)
{
	if (result == BT_RES_OK) {
		scan_power_status = SCAN_POWER_STATUS_ON;
		DI("BT initialized OK\n\r");

	}
	else {
		scan_power_status = SCAN_POWER_STATUS_OFF;
		Log_Error("\n\rERROR: bt_on_cb_only::Init BT Failed! (%x)\n\r",result);
		// error notify
		SendMessageToStateManager(MSG_ERR_SCAN, PARAM_SCAN_CB_ERROR);

	}
}
#endif // 0

/**
 * BT OFF のコールバック
 */
static void bt_off_cb (tBT_ResultEnum result, void *param)
{
	if (result == BT_RES_OK) {
		scan_power_status = SCAN_POWER_STATUS_OFF;
#if CONFIG_AUDREY_ALWAYS_BT_ON != 1
		DI("BT deinitialized OK\n\r");
#endif //#if CONFIG_AUDREY_ALWAYS_BT_ON != 1
	}
	else {
		power_off_error_count ++;
		// notify
		SendMessageToStateManager(MSG_ERR_SCAN, PARAM_SCAN_CB_ERROR);

		scan_power_status = SCAN_POWER_STATUS_OFF;
		Log_Error("\n\rERROR: bt_off_cb::Deinit BT Failed! (%x)\n\r",result);
	}
}

/**
 * BT ON の処理
 */
static void bt_power_on (void) {

	if (scan_power_status != SCAN_POWER_STATUS_OFF) {
		Log_Error("bt_power_off:: status(%d) error ", scan_power_status);
		SendMessageToStateManager(MSG_ERR_SCAN, PARAM_SCAN_STATUS_ERROR+30);
	}
	scan_power_status = SCAN_POWER_STATUS_ON_PEND;
#if CONFIG_AUDREY_ALWAYS_BT_ON != 1
	DI("BT_POWER ON...\n\r");

	BT_Init(bt_on_cb);
#else
	bt_on_cb(BT_RES_OK, NULL);
#endif //#if CONFIG_AUDREY_ALWAYS_BT_ON != 1
}

/**
 * BT OFF の処理
 */
static void bt_power_off (void) {

	if (scan_power_status != SCAN_POWER_STATUS_ON) {
		Log_Error("bt_power_off:: status(%d) error ", scan_power_status);
		SendMessageToStateManager(MSG_ERR_SCAN, PARAM_SCAN_STATUS_ERROR+31);
	}
	scan_power_status = SCAN_POWER_STATUS_OFF_PEND;
#if CONFIG_AUDREY_ALWAYS_BT_ON != 1
	DI("BT_POWER OFF...\n\r");
	BT_GAP_CallbackUnreg(at_gap_ev_cb);
	vTaskDelay(1000);
	BT_Done(bt_off_cb, 1);
#else
	bt_off_cb(BT_RES_OK, NULL);
#endif //#if CONFIG_AUDREY_ALWAYS_BT_ON != 1

	// OFFになるまで待つ
	for(int i = 0; i<100; i++ ){
		if (scan_power_status == SCAN_POWER_STATUS_OFF) {
			break;
		}
		vTaskDelay(30);
	}
	if (scan_power_status != SCAN_POWER_STATUS_OFF) {
		Log_Error("bt_power_off:: status(%d) error ", scan_power_status);
		SendMessageToStateManager(MSG_ERR_SCAN, PARAM_SCAN_STATUS_ERROR+32);
	}
	vTaskDelay(300); // wait BT_EXT_TLDone()

#if CONFIG_AUDREY_ALWAYS_BT_ON != 1
	DI(" end\n\r");
#endif //#if CONFIG_AUDREY_ALWAYS_BT_ON != 1
}

/**
 * LEスキャンを停止する
 * これに先立つ start_le_scan()内部でBT　ONの処理が実行された場合のみ　BT　OFF　にする
 */
void stop_le_scan (void)
{
	D("start mode=%d status=%d\n\r", scan_power_mode, scan_power_status);
	TimerStop_LE();

	vTaskDelay(100);

	if (scan_power_mode == 1) {
		D("ble power off\r\n");
		power_le_scan(0);
	}
	D(" end mode=%d status=%d\n\r", scan_power_mode, scan_power_status);
}



/**
 * LEスキャンを開始する
 * 最大10秒間継続する
 * LEスキャン中での呼び出しにも対応する　その場合はさらに10秒間継続する
 * BTがOFF状態で呼び出された場合には、BTをONにしてから開始する
 *     BT　ONの処理には3秒程度必要で、その後スキャン状態に移行する
 */
void start_le_scan(void)
{
#if CONFIG_AUDREY_DBG_UPLOAD
	scan_comp_flag = 0x00;
#endif
	D("start mode=%d status=%d\n\r", scan_power_mode, scan_power_status);
	if (is_bt_ready()) {
		scan_power_mode = 0;
		scan_power_status = SCAN_POWER_STATUS_ON;
		setup_le_scan();
	} else {
		if (scan_power_status == SCAN_POWER_STATUS_OFF) {
			scan_power_mode = 1;
		} else {
			D(" PARAM_SCAN_NOT_READY scan_power_status=%d", scan_power_status);
			//SendMessageToStateManager(MSG_ERR_SCAN, PARAM_SCAN_NOT_READY);
		}
		power_le_scan(2); // ON and setup_le_scan()
	}
	D(" end mode=%d status=%d\n\r", scan_power_mode, scan_power_status);
}

/**
 * 初期化処理
 * 最初に呼び出されることが望ましいが、通常使用では実行しなくても問題ない
 */
void ble_scan_init(void)
{
	D("\r\n start\n\r");
	switch(BT_GetState()) {
	case BT_ST_IDLE:
	case BT_ST_READY:
		break;
	default:
		vTaskDelay(3000);
		break;
	}
	switch(BT_GetState()) {
	case BT_ST_IDLE:
		scan_power_status = SCAN_POWER_STATUS_OFF;
		break;
	case BT_ST_READY:
		scan_power_status = SCAN_POWER_STATUS_ON;
		break;
	default:
		Log_Error("[FATAL ERROR] : ble init error\r\n");
		SendMessageToStateManager(MSG_ERR_SCAN_FATAL, PARAM_SCAN_INITIALIZING_FATAL_ERROR);
		break;
	}
	scan_power_mode = 0;
	scan_count = 0;
	D("\r\n end  scan_power_status=%d\n\r", scan_power_status);
}

/**
 * BTのON/OFFを行う
 * OFFへの移行段階でのON呼び出しでは、OFFになるまで待ってからONに移行する
 * ONの処理には3秒程度必要
 * BTの状態で　INITIALIZING又は　DEINITIALIZINGが10秒続いた場合にはリセットする
 * @param[in] sw 0..OFF  1..ON 2..ON and start le_scan
 */
void power_le_scan(int sw)
{
	D(" start sw=%d status=%d", sw, scan_power_status);
	if (sw == 0) {
		// turn OFF
		if (scan_count > 0) {
			TimerStop_LE();
			vTaskDelay(100); //[TODO]
		}
		switch(scan_power_status)
		{
		case SCAN_POWER_STATUS_OFF:
			Log_Error("power_le_scan:: status error already off ");
			break;
		case SCAN_POWER_STATUS_ON_ONLY_PEND:
		case SCAN_POWER_STATUS_ON_PEND:

			for(int i = 0; i<30; i++ ){
				if (scan_power_status == SCAN_POWER_STATUS_ON) {
					bt_power_off();
					break;
				}
				vTaskDelay(100);
			}
			if (scan_power_status != SCAN_POWER_STATUS_OFF_PEND) {
				int bt_status;
				Log_Error("power_le_scan:: status error ");

				bt_status = ble_get_bt_status();
				if (bt_status == BT_ST_INIT) {
					for( int i=0; i<100; i++) {
						bt_status = ble_get_bt_status();
						if (bt_status != BT_ST_INIT) {
							break;
						}
						vTaskDelay(100);
					}
				}
				if (bt_status == BT_ST_INIT) {
					Log_Error("[FATAL ERROR] : BT INITIALIZING will not end\r\n");
					Log_Error("to be reset now!");
					SendMessageToStateManager(MSG_ERR_SCAN_FATAL, PARAM_SCAN_INITIALIZING_FATAL_ERROR);
					//vTaskDelay(3000);
					//sys_reset();
				}
			}
			break;
		case 	SCAN_POWER_STATUS_ON:
			bt_power_off();
			break;
		case SCAN_POWER_STATUS_OFF_PEND:
			break;

		}
	} else {
		// turn ON
		switch(scan_power_status)
		{
		case SCAN_POWER_STATUS_OFF:
#if CONFIG_AUDREY_ALWAYS_BT_ON != 1
			BT_Init(bt_on_cb);
#endif //#if CONFIG_AUDREY_ALWAYS_BT_ON != 1
			if (sw == 1) {
			scan_power_status = SCAN_POWER_STATUS_ON_ONLY_PEND;
			} else {
				scan_power_status = SCAN_POWER_STATUS_ON_PEND;
			}
#if CONFIG_AUDREY_ALWAYS_BT_ON != 1
			DI("BT_POWER ON...\n\r");
#endif //#if CONFIG_AUDREY_ALWAYS_BT_ON != 1
			break;
		case SCAN_POWER_STATUS_ON_ONLY_PEND:
			if (sw == 2) {
				scan_power_status = SCAN_POWER_STATUS_ON_PEND;
			}
			break;
		case SCAN_POWER_STATUS_ON_PEND:
			break;
		case 	SCAN_POWER_STATUS_ON:
			Log_Error("power_le_scan:: status error already on ");
			if (sw == 2 && scan_count == 0) {
				setup_le_scan();
			}
			break;
		case SCAN_POWER_STATUS_OFF_PEND:

			for(int i = 0; i<30; i++ ){
				if (scan_power_status == SCAN_POWER_STATUS_OFF) {
					bt_power_on();
					break;
				}
				vTaskDelay(100);
			}
			if (scan_power_status != SCAN_POWER_STATUS_ON_PEND) {
				int bt_status;
				Log_Error("power_le_scan:: status(%d) error ", scan_power_status);
				SendMessageToStateManager(MSG_ERR_SCAN, PARAM_SCAN_STATUS_ERROR+33);

				bt_status = ble_get_bt_status();
				if (bt_status == BT_ST_REMOVING) {
					for( int i=0; i<100; i++) {
						bt_status = ble_get_bt_status();
						if (bt_status != BT_ST_REMOVING) {
							break;
						}
						vTaskDelay(100);
					}
				}
				if (bt_status == BT_ST_REMOVING) {
					Log_Error("[FATAL ERROR] : BT DEINITIALIZING will not end\r\n");
					Log_Error("to be reset now!");
					SendMessageToStateManager(MSG_ERR_SCAN_FATAL, PARAM_SCAN_DEINITIALIZING_FATAL_ERROR);
					//vTaskDelay(3000);
					//sys_reset();
				}

			}
			break;

		}

	}

	D(" end sw=%d status=%d", sw, scan_power_status);
}

/**
 * ステータスを返す
 */
int status_le_scan(void)
{
	DI(" mode=%d status=%d BT_GetState=%d", scan_power_mode, scan_power_status, BT_GetState());
	return (int)scan_power_status;
}
#endif // CONFIG_BT
