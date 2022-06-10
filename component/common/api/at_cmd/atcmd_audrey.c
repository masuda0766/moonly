#include <platform_stdlib.h>
#include <platform_opts.h>
#include <led_util.h>
#include <temperature.h>
#include <sntp/sntp.h>
#include <build_info.h>
#include <wifi/wifi_conf.h>
#include "FreeRTOS.h"
#include "log_service.h"
#include "sys_api.h"
#include "rtc_api.h"
#include "atcmd_audrey.h"
#include "main.h"
#include "version.h"
#include "scale.h"
#include "state_manager.h"
#include "log_record.h"
#include "link_key.h"
#if CONFIG_AUDREY_WDT
#include "wdt_api.h"
#endif
#if CONFIG_AUDREY_INDV_EMU == 1
#include "badge.h"
#include "scale.h"
#include "bt_util.h"
#endif /* CONFIG_AUDREY_INDV_EMU */
#include "sh_system.h"

extern int Erase_Fastconnect_data();
extern int Erase_Scale_Update_flg();

#if CONFIG_AUDREY_FWUP_ALONE
void fATA0(void *arg)
{
    char *argv[MAX_ARGC] = {0};
    int argc;
    int type;

	printf("[ATA0] stand-alone update\r\n");
	if(arg && strlen((const char *)arg) != 0) {
		argc = parse_param(arg, argv);
		if(argc == 3) {
			unsigned int type = atoi((const char *)(arg));
			if(type >= 1 && type <= 4 && strlen(argv[2]) <= 15) {
				ota_start_stand_alone(type, argv[2]);
				return;
			}
		}
		printf("Invalid parameter\n\r");
	}
}
#endif

void fATAR(void *arg)
{
	printf("[ATAR] System Reset\r\n");
	rtw_mdelay_os(500);
	sys_reset();
}

void fATAY(void *arg)
{
	printf("[ATAY] Factory Rseset\r\n");
	Erase_Fastconnect_data();
	Erase_Scale_Update_flg();
	rtw_mdelay_os(500);
	// reboot
	sys_reset();
}

#include "flash_api.h"
#include "device_lock.h"

void fATA2(void *arg)
{
	flash_t flash;

	printf("[ATA2] Erase 2nd firmware image area\r\n");
	device_mutex_lock(RT_DEV_LOCK_FLASH);
	flash_erase_sector(&flash, 0x00100000);
	device_mutex_unlock(RT_DEV_LOCK_FLASH);
	rtw_mdelay_os(500);
	// reboot
	sys_reset();
}

#if CONFIG_AUDREY_SCL_OFF
#include "flash_api.h"
#include "device_lock.h"

void fATAA(void *arg)
{
	flash_t flash;
	u8 data[2];
	u8 type;

	if(!arg || strlen((const char *)arg) == 0) {
		printf("[ATAA]\n\r");
		return;
	} else if(!strcmp(arg, "enable")) {
		type = 'E';
		printf("[ATAA]Enable Scale UART\r\n");
	} else if(!strcmp(arg, "disable")) {
		type = 'D';
		printf("[ATAA]Disable Scale UART\r\n");
	} else if(!strcmp(arg, "isp")) {
		type = 'I';
		printf("[ATAA]Disable Scale UART & ISP low\r\n");
	} else {
		printf("[ATAA]\n\r");
		return;
	}
	device_mutex_lock(RT_DEV_LOCK_FLASH);
	flash_stream_read(&flash, SCALE_IS_UPDATE_FLG,  2, (uint8_t *)data);
	data[1] = type;
	flash_erase_sector(&flash, SCALE_IS_UPDATE_FLG);
	flash_stream_write(&flash, SCALE_IS_UPDATE_FLG, 2, (uint8_t *)data);
	device_mutex_unlock(RT_DEV_LOCK_FLASH);

	rtw_mdelay_os(500);
	sys_reset();
}
#endif

void fATAS(void *arg)
{
    char *argv[MAX_ARGC] = {0};
    int argc;
#if CONFIG_AUDREY_SCL_OFF
	flash_t flash;
	u8 data[2];

	device_mutex_lock(RT_DEV_LOCK_FLASH);
	flash_stream_read(&flash, SCALE_IS_UPDATE_FLG,  2, (uint8_t *)data);
	device_mutex_unlock(RT_DEV_LOCK_FLASH);
	if(data[1] == 'D' || data[1] == 'I') {
		printf("[ATAS]Scable UART is disabled\n\r");
		return;
	}
#endif
	if(!arg || strlen((const char *)arg) == 0) {
		printf("[ATAS]\n\r");
	} else if(!strcmp(arg, "sfw")) {
		printf("[ATAS] Send Scale firmware image\r\n");
		scale_xmodem_start();
	} else if((argc = parse_param(arg, argv)) == 3 && !strcmp(argv[1], "grv")) {
//		printf("[ATAS] Gravity for Scale\r\n");
		scale_chk_gravity(argv[2]);
	} else {
		scale_uart_send_string((char*)arg);
		scale_uart_send_string("\r");
//		printf("[ATAS]Send command to Scale side\n\r");
	}
}

void fATAT(void *arg)
{
	struct tm *t;
	time_t curr_time;

	curr_time = rtc_read();
	t = get_local_tm(&curr_time);
	printf("[ATAT]Time:%04d-%02d-%02d %02d:%02d:%02d\n\r"
	, t->tm_year+1900, t->tm_mon+1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
}

extern u8	audrey_mac[];
extern u8	wrong_mac[];

void fATAx(void *arg)
{
	printf("[ATA?] Version\r\n");
	printf("    DATE: %s\n\r", RTL8195AFW_COMPILE_TIME);
	printf("WIRELESS: %s\n\r", AUDREY_VERSION);
	printf("   SCALE: %s\n\r", scale_ver);
	printf("     MAC: %02x:%02x:%02x:%02x:%02x:%02x"
	, audrey_mac[0], audrey_mac[1], audrey_mac[2], audrey_mac[3], audrey_mac[4], audrey_mac[5]);
	if(memcmp(audrey_mac, wrong_mac, 3) == 0) {
		printf(" ERROR");
	} else {
		printf(" OK");
	}
	printf("\n\r[DONE]\n\r");
}

#if CONFIG_AUDREY_WDT
void fATAW(void *arg)
{
    if(!arg){
		printf("[ATAW] Usage: ATAW=0/1\n\r");
		return;
	}
	unsigned int param = atoi((const char *)(arg));
	if (param == 1) {
		watchdog_start();
		watchdog_refresh();
		printf("[ATAW] Watchdog Start\r\n");
	} else if (param == 0) {
		watchdog_stop();
		printf("[ATAW] Watchdog Stop\r\n");
	} else {
		printf("[ATAW] Usage: ATAW=0/1\n\r");
	}
}
#endif /* CONFIG_AUDREY_WDT */

void fATAL(void *arg){
    char *argv[MAX_ARGC] = {0};
    int argc;
    int cmd;

    if(!arg){
        printf("ATAL=LED_MODE\n\r");
        return;
    }
    if((argc = parse_param(arg, argv)) != 2){
        printf("ATAL=LED_MODE\n\r");
        return;
    }

    cmd = atoi(argv[1]);
    switch(cmd){
        case 0:
            led_control(LED_TYPE_OFF);
            break;
        case 1:
            led_control(LED_TYPE_ON);
            break;
        case 2:
            led_control(LED_TYPE_BLINK_LOW);
            break;
        case 3:
            led_control(LED_TYPE_BLINK_MID);
            break;
        case 4:
            led_control(LED_TYPE_CUST_1);
            break;
        default:
            printf("[ATAL] parameter error\n\r");
            break;
    }
}

void fATAO(void *arg){
    float temp = get_temperature();
    printf("temperature = %f\n\r", temp);
}

void fATAI(void *arg)
{
	sntp_init();
	printf("[ATAI] run sntp_init\n\r");
}

void fATAK(void *arg){
	char *argv[MAX_ARGC] = {0};
	int argc;
	int cmd;

	if(!arg){
		printf("[ATAK] Usage:ATAK={0|1} (0:disable, 1:enable)\n\r");
		return;
	}
	if((argc = parse_param(arg, argv)) != 2){
		printf("[ATAK] Usage:ATAK={0|1} (0:disable, 1:enable)\n\r");
		return;
	}

	cmd = atoi(argv[1]);
	switch(cmd){
		case 0:
			link_key_disable();
			break;
		case 1:
			link_key_enable();
			break;
		default:
			break;
	}
}

void fATAF(void *arg)
{
    char *argv[MAX_ARGC] = {0};
    int argc;

	if(!arg || strlen((const char *)arg) == 0) {
		return;
	}
	argc = parse_param(arg, argv);
	if(argc == 3 && !strcmp(argv[1], "erase")) {
		if(!strcmp(argv[2], "msg")) {
			log_record_erase_msg();
		} else if(!strcmp(argv[2], "fault")) {
			log_record_erase_fault();
		} else if(!strcmp(argv[2], "all")) {
			log_record_erase_msg();
			log_record_erase_fault();
		}
	} else if(argc == 3 && !strcmp(argv[1], "show")) {
		if(!strcmp(argv[2], "msg")) {
			log_record_show_msg();
		} else if(!strcmp(argv[2], "fault")) {
			log_record_show_fault();
		} else if(!strcmp(argv[2], "all")) {
			log_record_show_msg();
			log_record_show_fault();
		}
	} else if(argc == 2 && !strcmp(argv[1], "error")) {
		printf("TIME:%s\r\n", rtc_read());		// For Debug (Forced Hard Fault Error)
	}
}

void fATAH(void *arg)
{
	printf("[ATAH]\r\n Available %d\r\n minimum ever free %d\r\n", xPortGetFreeHeapSize(), xPortGetMinimumEverFreeHeapSize());
}

void fATAM(void *arg)
{
    char *argv[MAX_ARGC] = {0};
    int argc;
    int MsgID;
    int Param;

    if((argc = parse_param(arg, argv)) > 3){
        printf("USAGE: ATAM=MessageID <,Param>\n\r");
        return;
    }

    MsgID = atoi(argv[1]);
    Param = atoi(argv[2]);
    SendMessageToStateManager(MsgID, Param);
}

void fATAC(void *arg)
{
    char *argv[MAX_ARGC] = {0};
    int argc;
    int state;

    if((argc = parse_param(arg, argv)) != 2){
        printf("USAGE: ATAC=state\n\r");
        return;
    }

    state = atoi(argv[1]);
    SendMessageToStateManager(MSG_CHANGE_STATE, state);
}
#if AUDREY_ATCMD_ATAU
#if CONFIG_OTA_HTTP_UPDATE
extern void cmd_http_update(int argc, char **argv);
extern void start_http_update(void);
void fATAU(void *arg)
{
	int argc = 0;
	char *argv[MAX_ARGC] = {0};
	printf("[ATAU]: _AT_AUDREY_OTA_HTTP UPDATE_\n\r");
	// check wifi connect first
	if(wifi_is_connected_to_ap()==0){
		if(!arg){
			start_http_update();
			return;
		} else {
			argc = parse_param(arg, argv);
		}
		argv[0] = "update";

		cmd_http_update(argc, argv);
	} else {
		printf("\r\nwifi is not coneccted!\r\n");
	}
}
#endif // CONFIG_OTA_HTTP_UPDATE
#endif // AUDREY_ATCMD_ATAU

extern void periodic_timeout_handler(xTimerHandle pxTimer);
extern void data_upload_req(void);
extern void alert_notification_req(unsigned int id);
#if CONFIG_AUDREY_DBG_UPLOAD
extern void dbg_weight_data_upload_req(void);
extern void dbg_beacon_data_upload_req(void);
#endif
void fATAD(void *arg)
{
	char *argv[MAX_ARGC] = {0};
	int argc;
	int cmd;

	if(!arg){
		alert_notification_req(0);
		return;
	}
	if((argc = parse_param(arg, argv)) == 2){
		if (!strcmp(argv[1], "p")) {
			periodic_timeout_handler(NULL);
		} else if (!strcmp(argv[1], "e")) {
			data_upload_req();
#if CONFIG_AUDREY_DBG_UPLOAD
		}else if (!strcmp(argv[1], "dw")) {
			dbg_weight_data_upload_req();
		}else if (!strcmp(argv[1], "db")) {
			dbg_beacon_data_upload_req();
#endif
		} else {
			cmd = atoi(argv[1]);
			if ((cmd > 0) && (cmd <= 9999)) {
				alert_notification_req(cmd);
			} else {
				printf("\r\n[ATAD] Usage:ATAD={1-9999|p|e}\r\n");
				printf("          1-9999 : alert notification\n\r");
				printf("          p      : periodic data\n\r");
				printf("          e      : entry data\n\r");
#if CONFIG_AUDREY_DBG_UPLOAD
				printf("          dw     : debug data (weight)\n\r");
				printf("          db     : debug data (beacon)\n\r");
#endif
				printf("       ATAD only : alert notification with message 0000\n\r");
			}
		}
	}
	return;
}

#if CONFIG_BT_GATT_SERVER
extern void cmd_bt_gatt_server(int argc, char **argv);
extern void stop_bt_gatt_server(void);
void fATAG (void *arg) {
	int argc = 0;
	char *argv[MAX_ARGC] = {0};

	argc = parse_param(arg, argv);

	if (argc == 2) {
		if (strcmp(argv[1],"0") == 0) {
			stop_bt_gatt_server(); // and  notify_task deleted
			return;
		}
	}

	if (argc > 1) {
		cmd_bt_gatt_server(argc, argv);
		return;
	}

	printf("\r\nATAG=0/1,n  n...packet wait time");
	printf("\r\nATAG=1 ... gatt server start");
	printf("\r\nATAG=0 ... gatt server stop");
	printf("\r\nATAG=READ ... read data");
	printf("vATAG=WRITE, xxxxx  ... write data");
	printf("\r\nATAG=STAT ... display status");
}
#endif

#if CONFIG_AUDREY_INDV_EMU == 1
extern void badge_init(void);
extern void badge_config(BADGE_CONF *buf);
extern int weight_update(void);
extern void badge_beacon_rcv(u8 *bd_addr, char rssi, char battery);
extern u8 hex_str_to_bd_addr(u32 str_len, s8 *str, u8 *num_arr);
extern void weight_init(void);
typedef enum {
	STATE_NOPET_STABLE = 0,				// 退室中安定
	STATE_PET_UNSTABLE,					// 入室中不安定
	STATE_PET_STABLE,					// 入室中安定
	STATE_NOPET_UNSTABLE,				// 退室中不安定
	STATE_NOPET_URINE,					// 尿量取得待ち
	STATE_DOWN_UNSTABLE,				// 重量低下後不安定
	STATE_NOPET_NOCAB,					// 上部ユニット未挿入
	STATE_WEIGHT_MAX
} WEIGHT_STATE;
extern WEIGHT_STATE mainState;
extern WEIGHT_STATE prevState;
extern void recordWeight( void );
extern int weight_curr_body;				// 現在の体重計値
extern int weight_prev_body;				// 前回の体重計値

#define BT_ADDR_IDX					2
#define BT_BODY_IDX					3

#define SCALE_TIME_IDX				2
#define SCALE_BODY_IDX				3
#define SCALE_URINE_IDX				4

#define RSSI_ADDR_IDX				2
#define RSSI_RSSI_IDX				3
#define RSSI_BATT_IDX				4

#if CONFIG_AUDREY_INDV_EMU_LOG
extern char badge_time_tmp[40];
extern badge_time_flag;
boolean badge_judge_through = FALSE;
boolean first_wieght = FALSE;
#endif /* CONFIG_AUDREY_INDV_EMU_LOG */

void fATAV(void *arg)
{

	int					i;
	int					argc = 0;
	char				*argv[MAX_ARGC] = {0};
	BADGE_CONF			badge_conf = {0};
//	u8					*bd_addr = NULL;
	u8					bd_addr[BD_ADDR_LEN] = {};
	char				rssi = 0;
	char				battery = 0;

//	printf("*arg:%s \r\n", arg);

	argc = parse_param(arg, argv);

//	for (i = 0; i <= argc; i++) {
//		printf("argv[%d]:%s \r\n", i, argv[i]);
//	}

	// サーバーからデータ（バッジアドレス、体重）取得する前に、バッジ数を初期化
	//--------------------------------------------------------------------------
	if (strcmp(argv[1],"INI_START") == 0) {
#if CONFIG_AUDREY_INDV_EMU_LOG
		if (badge_judge_through == FALSE) {
			recordWeight();
		}
		badge_judge_through = FALSE;
		first_wieght = TRUE;
#endif /* CONFIG_AUDREY_INDV_EMU_LOG */
		badge_init();

#if CONFIG_AUDREY_INDV_EMU_LOG
		badge_time_flag = TRUE;
#endif /* CONFIG_AUDREY_INDV_EMU_LOG */
	// サーバーからデータ（バッジアドレス、体重）セット
	//--------------------------------------------------------------------------
	} else if (strcmp(argv[1],"INI") == 0) {
		hex_str_to_bd_addr(strlen(argv[BT_ADDR_IDX]), argv[BT_ADDR_IDX], badge_conf.bd_addr);
		badge_conf.body = atoi(argv[BT_BODY_IDX]);
		badge_config(&badge_conf);

		printf("badge_conf.bd_addr: "BD_ADDR_FMT"\r\n", BD_ADDR_ARG(badge_conf.bd_addr));
		printf("badge_conf.body:%d \r\n", badge_conf.body);
	// スケールから、重さセット
	//--------------------------------------------------------------------------
	} else if (strcmp(argv[1],"DATA_WEIGHT") == 0) {
//		scale_info.time = argv[SCALE_TIME_IDX];
#if CONFIG_AUDREY_INDV_EMU_LOG
		strcpy(badge_time_tmp, (const char*)argv[SCALE_TIME_IDX]);
		if (first_wieght == TRUE) {
			// 各図の最初からデータ算出開始する為の処理
			weight_init();
			mainState = STATE_NOPET_STABLE;
			prevState = STATE_NOPET_STABLE;
			weight_curr_body = atoi(argv[SCALE_BODY_IDX]);				// 現在の体重計値
			weight_prev_body = atoi(argv[SCALE_BODY_IDX]);				// 前回の体重計値
			first_wieght = FALSE;
		}
#endif /* CONFIG_AUDREY_INDV_EMU_LOG */
		scale_info.body = atoi(argv[SCALE_BODY_IDX]);
		scale_info.urine = atoi(argv[SCALE_URINE_IDX]);
		weight_update();

//		printf("scale_info.body:%d \r\n", scale_info.body);
//		printf("scale_info.urine:%d \r\n", scale_info.urine);
	// BTから、RSSIセット
	//--------------------------------------------------------------------------
	} else if (strcmp(argv[1],"DATA_RSSI") == 0) {
		hex_str_to_bd_addr(strlen(argv[RSSI_ADDR_IDX]), argv[RSSI_ADDR_IDX], bd_addr);
		rssi = (char)atoi(argv[RSSI_RSSI_IDX]);
		battery = (char)atoi(argv[RSSI_BATT_IDX]);
		badge_beacon_rcv(bd_addr, rssi, battery);

		printf("bd_addr: "BD_ADDR_FMT"\r\n", BD_ADDR_ARG(bd_addr));
		printf("rssi:%d \r\n", rssi);
		printf("battery:%d \r\n", battery);
	}

	printf("ATAV OK");
	printf("\r\n");


}
#endif /* CONFIG_AUDREY_INDV_EMU */

#include <fast_connect.h>

void fATAB(void *arg)
{
	int					argc = 0;
	char				*argv[MAX_ARGC] = {0};
	flash_t 			flash;
	struct wlan_fast_reconnect 		read_data = {0};


	argc = parse_param(arg, argv);

	device_mutex_lock(RT_DEV_LOCK_FLASH);
	flash_stream_read(&flash, FAST_RECONNECT_DATA, (uint32_t)sizeof(read_data), (u8 *) &read_data);


	if (strcmp(argv[1],"type") == 0) {
		if (atoi(argv[2]) != 0) {
			read_data.type = 1;
		} else {
			read_data.type = 0;
		}
	} else if (strcmp(argv[1],"ssid") == 0) {
		read_data.psk_essid[0] = 0;
		printf(("[ATAB]Write ssid:0\r\n"));
	} else if (strcmp(argv[1],"pass") == 0) {
		read_data.psk_passphrase[0] = 0;
		printf(("[ATAB]Write pass:0\r\n"));
	} else if (strcmp(argv[1],"wpa") == 0) {
		read_data.wpa_global_PSK[0] = 0;
		printf(("[ATAB]Write wpa:0\r\n"));
	} else if (strcmp(argv[1],"channel") == 0) {
		read_data.channel = 0;
		printf(("[ATAB]Write channel:0\r\n"));
	} else if (strcmp(argv[1],"security") == 0) {
		if (atoi(argv[2]) != 0) {
			read_data.security_type = 0xFFFFFFFF;
			printf(("[ATAB]Write security_type:0xFFFFFFFF\r\n"));
		} else {
			read_data.security_type = 0;
			printf(("[ATAB]Write security_type:0\r\n"));
		}
	}

	flash_erase_sector(&flash, FAST_RECONNECT_DATA);
	flash_stream_write(&flash, FAST_RECONNECT_DATA, (uint32_t)sizeof(read_data), (uint8_t *) &read_data);

	device_mutex_unlock(RT_DEV_LOCK_FLASH);

}


#if (CONFIG_AUDREY_EXT_INFO == 1)
void fATSK(void *arg)
{
	printf(("\t<<STACK INFO>>\r\n"));
	printf(("\tID\tStackSize\tFreeStack\tPriority\tName\r\n"));
	printf(("\t--\t---------\t---------\t--------\t--------------\r\n"));

	sh_sys_all_tasks_info_t *tskInfo = sh_sys_task_get_info();
	if(tskInfo!=NULL)
	{
		for(int i=0;i<tskInfo->task_count;i++)
		{
			printf("\t%2d", tskInfo->tasks_info[i].id);
			printf("\t%8d", tskInfo->tasks_info[i].stack_size);
			printf("\t%8d", tskInfo->tasks_info[i].stack_free_on_hwm);
			printf("\t%8d", tskInfo->tasks_info[i].priority);
			printf("\t%s\r\n", tskInfo->tasks_info[i].name);

		}
		free(tskInfo);
	}

	printf(("\t-------------------------------------------------------\r\n"));
}
#endif


void fATHF(void *arg)
{
	sh_sys_force_hardfault(1);
}

extern void vApplicationStackOverflowHook( xTaskHandle, signed char * );
void fATDC(void *arg)
{
	int		argc, param;
	char	*argv[MAX_ARGC] = {0};

	argc = parse_param(arg, argv);
	if (argc != 2)
	{
		printf("number of argument is incorrect.\n");
		return;
	}

	param = atoi(argv[1]);
	switch (param)
	{
		case 0:
			/* 強制スタックオーバーフロー */
			vApplicationStackOverflowHook(NULL, NULL);
			break;
		case 1:
			/* 強制malloc失敗 */
			vApplicationMallocFailedHook();
			break;
	}
}


log_item_t at_audrey_items[] = {
	{"ATAR", fATAR,},	// System Reset
	{"ATAY", fATAY,},	// Factory Reset
	{"ATA?", fATAx,},	// Version
	{"ATAF", fATAF,},	// 記録メッセージの表示
	{"ATAS", fATAS,},	// Send command to Scale side
#if CONFIG_AUDREY_SCL_OFF
	{"ATAA", fATAA,},	// Scale通信用UART無効化
#endif
	{"ATAO", fATAO,},	// Temperature
	{"ATAH", fATAH,},	// available heap
#if (CONFIG_AUDREY_EXT_INFO == 1)
	{"ATSK", fATSK,},	// タスク情報一覧
#endif
#if CONFIG_AUDREY_DEV_CMD
	{"ATA2", fATA2,},	// 2面目signature Earse
#if CONFIG_AUDREY_FWUP_ALONE
	{"ATA0", fATA0,},	// System Reset
#endif
	{"ATAT", fATAT,},	// Show time
#if CONFIG_AUDREY_WDT
	{"ATAW", fATAW,},	// Watchdog
#endif /* CONFIG_AUDREY_WDT */
	{"ATAL", fATAL,},	// LDE
	{"ATAK", fATAK,},	// link Key
	{"ATAI", fATAI,},	// stnp_init実行
	{"ATAM", fATAM,},	// 状態遷移タスクへのメッセージ送信
	{"ATAC", fATAC,},	// 状態遷移タスクの状態遷移
#if AUDREY_ATCMD_ATAU
#if CONFIG_OTA_HTTP_UPDATE
	{"ATAU", fATAU,},	// http FW update
#endif
#endif // AUDREY_ATCMD_ATAU
	{"ATAD", fATAD,},	// Data upload
#if AUDREY_ATCMD_ATAG
#if CONFIG_BT_GATT_SERVER
	{"ATAG", fATAG,},
#endif
#endif // AUDREY_ATCMD_ATAG
#if CONFIG_AUDREY_INDV_EMU == 1
	{"ATAV", fATAV,},
#endif /* CONFIG_AUDREY_INDV_EMU */
	{"ATHF", fATHF,},	// 強制Hard Fault
	{"ATDC", fATDC,},	// システム関連各種デバッグ用コマンド
	{"ATAB", fATAB,},	// システム関連各種デバッグ用コマンド
#endif /* CONFIG_AUDREY_DEV_CMD */
};

void at_audrey_init(void)
{
    log_service_add_table(at_audrey_items, sizeof(at_audrey_items) / sizeof(at_audrey_items[0]) );
}

#if SUPPORT_LOG_SERVICE
log_module_init(at_led_init);
#endif
