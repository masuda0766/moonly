#include "FreeRTOS.h"
#include "task.h"
#include "diag.h"
#include "queue.h"
#include "state_manager.h"
#include "at_cmd/log_service.h"
#include "osdep_service.h"
#include "webserver.h"
#include <sntp/sntp.h>
#include "led_util.h"
#include "timers.h"
#include "fast_connect.h"
#include "sys_api.h"
#include "ota_http_client.h"
#include "rf_ctrl.h"
#include "scale.h"
#include "link_key.h"
#include "platform_opts.h"
#include "version.h"
#include "log_record.h"

/* キュー宣言 */
PRIVILEGED_DATA static QueueHandle_t xStateManagerQueue = NULL;
/* 状態遷移用変数 */
static enum StateManager_state StateNo		= STATE_IDLE;
static enum StateManager_state StateNoBkUp	= STATE_IDLE;

/* LED 3分タイマ */
#if CONFIG_AUDREY_LED_BLK3
TimerHandle_t	timer_id_led;
#endif

#if CONFIG_AUDREY_LED_KEY
TimerHandle_t	timer_led_on;

#if CONFIG_AUDREY_FWUP_ALONE
#define FWVER_MAGIC		"CheckVersionInfo"		// マジックナンバー

int	standalone_type = 0;
char	standalone_ip[16];
char	standalone_scale[16];
char	standalone_wireless[16];

typedef struct {
	u8 scale[16];
	u8 wireless[16];
	u8 magic[sizeof(FWVER_MAGIC)];				// マジックナンバー格納領域
} FWVER_VOLATILE;

FWVER_VOLATILE __attribute__((section(".fwver.ver_sa"))) ver_sa;

bool	is_stand_alone_comp = FALSE;
#endif

////////////////////////////////////////////////////////////////////////
///	LED 表示終了
////////////////////////////////////////////////////////////////////////
static void TimerStop_LED_KEY(void)
{
	xTimerStop(timer_led_on, 0);
	led_on_key = FALSE;
	led_off_force();
}
#endif

////////////////////////////////////////////////////////////////////////
///	初期状態処理
////////////////////////////////////////////////////////////////////////
static void StateManager_Idle(void)
{
	/* 不揮発から前回状態取得しLED制御 */
	switch(get_fast_connect_type())
	{
	/* WiFi接続状態 */
	case	FAST_CONNECT_WLAN:
		led_control(LED_TYPE_BLINK_LOW);								/* LED 0.5Hz点滅 */
#if CONFIG_AUDREY_LED_BLK3
		TimerStart_LED();												/* LED 3分タイマ開始 */
#endif
		break;

	/* BLE接続状態 */
	case	FAST_CONNECT_BLE:
		led_control(LED_TYPE_BLINK_LOW);								/* LED 0.5Hz点滅 */
#if CONFIG_AUDREY_LED_BLK3
		TimerStart_LED();												/* LED 3分タイマ開始 */
#endif
		break;

	/* 上記以外 */
	case	FAST_CONNECT_NON:
	default:
		led_control(LED_TYPE_CUST_2);									/* LED 2250msOFF/250ms周期点滅×２ */
		break;
	}
}


////////////////////////////////////////////////////////////////////////
///	状態遷移メイン処理
////////////////////////////////////////////////////////////////////////
static void StateManagerMain(enum MessageID MsgID, MSG_Param_t Param)
{
	Log_Notify("\n\r[SM] State:%d MsgID:%04x Param:%d\r\n", StateNo, MsgID, Param);

	switch(StateNo)
	{
	//------------------------------------------------------------------
	//	初期状態
	//------------------------------------------------------------------
	case	STATE_IDLE:
		switch(MsgID)
		{
		/* WiFi初期化完了通知 */
		case	MSG_WIFI_INIT_COMP:
			/* 不揮発から前回状態取得し分岐 */
			switch(get_fast_connect_type())
			{
			/* WiFi接続状態 */
			case	FAST_CONNECT_WLAN:
#if CONFIG_AUDREY_ALWAYS_BT_ON
				rf_ctrl_ble_init();										/* BT起動(Advertising動作なし) */
#endif //#if CONFIG_AUDREY_ALWAYS_BT_ON
				rf_ctrl_wifi_connect(NULL);								/* WiFi接続要求 */
				ChangeState(STATE_WIFI_CONNECT_WAIT);					/* WiFi接続待ち状態へ遷移 */
				break;

			/* BLE接続状態 */
			case	FAST_CONNECT_BLE:
				rf_ctrl_ble_connect();									/* BT接続要求 */
				ChangeState(STATE_BT_CONNECT_WAIT);						/* BT接続待ち状態へ遷移 */
				break;

			/* 上記以外 */
			case	FAST_CONNECT_NON:
			default:
//				start_web_server();										/* WebServer開始 */
				scan_ap();												/* AP検索 */
#if CONFIG_AUDREY_FWUP_ALONE
				if(is_stand_alone_comp) {
					is_stand_alone_comp = FALSE;
					start_web_server();									/* WebServer開始 */
					ChangeState(STATE_WIFI_AP);							/* WiFi AP状態へ遷移 */
					break;
				}
#endif
				rf_ctrl_ble_config();									/* BT接続要求(初期設定用) */
				ChangeState(STATE_BT_CONF);								/* BT接続初期設定状態へ遷移 */
				break;
			}
			break;
		}
		break;

	//------------------------------------------------------------------
	//	BT接続初期設定状態
	//------------------------------------------------------------------
	case	STATE_BT_CONF:
		switch(MsgID)
		{
		/* WebUI WPS実行通知 */
		case	MSG_WEBUI_WPS_START:
			led_control(LED_TYPE_BLINK_LOW);							/* LED 0.5Hz点滅 */
			rf_ctrl_start_wps();										/* Wifi WPS接続要求 */
			ChangeState(STATE_WIFI_WPS);								/* WiFi WPS中状態へ遷移 */
			break;

		/* WebUI SSID入力通知 */
		case	MSG_WEBUI_SET_SSID:
			led_control(LED_TYPE_BLINK_LOW);							/* LED 0.5Hz点滅 */
			rf_ctrl_wifi_connect(NULL);									/* WiFi接続要求 */
#if CONFIG_AUDREY_LED_BLK3
			TimerStart_LED();											/* LED 3分タイマ開始 */
#endif
			ChangeState(STATE_WIFI_CONNECT_WAIT);						/* WiFi接続待ち状態へ遷移 */
			break;

		/* WebUI BLE接続 */
		case	MSG_WEBUI_SET_BLE_MODE:
			led_control(LED_TYPE_ON);									/* LED 点灯 */
			rf_ctrl_ble_connect();										/* BT接続要求 */
			data_upload_rf_state_changed(RF_STATE_BLE);					/* 無線接続状態遷移通知(保留データ送信要求)(→ データ送信タスク) */
			ChangeState(STATE_BT_CONNECT_COMPLETE);						/* BT接続完了状態へ遷移 */
			break;

		/* BT切断通知 */
		case	MSG_BT_LINK_OFF:
			break;

		/* Link-Key５秒押下通知 */
		case	MSG_LINKKEY_PUSH:
			led_control(LED_TYPE_CUST_3);								/* LED 2250msOFF/250ms周期点滅×４ */
			start_web_server();											/* WebServer開始 */
			ChangeState(STATE_WIFI_AP);									/* WiFi AP状態へ遷移 */
			break;
		}
		break;

	//------------------------------------------------------------------
	//	WiFi AP状態
	//------------------------------------------------------------------
	case	STATE_WIFI_AP:
		switch(MsgID)
		{
		/* WebUI WPS実行通知 */
		case	MSG_WEBUI_WPS_START:
			led_control(LED_TYPE_BLINK_LOW);							/* LED 0.5Hz点滅 */
			rf_ctrl_start_wps();										/* Wifi WPS接続要求 */
			ChangeState(STATE_WIFI_WPS);								/* WiFi WPS中状態へ遷移 */
			break;

		/* WebUI SSID入力通知 */
		case	MSG_WEBUI_SET_SSID:
			led_control(LED_TYPE_BLINK_LOW);							/* LED 0.5Hz点滅 */
			rf_ctrl_wifi_connect(NULL);									/* WiFi接続要求 */
#if CONFIG_AUDREY_LED_BLK3
			TimerStart_LED();											/* LED 3分タイマ開始 */
#endif
			ChangeState(STATE_WIFI_CONNECT_WAIT);						/* WiFi接続待ち状態へ遷移 */
			break;

		/* WebUI BLE接続 */
//		case	MSG_WEBUI_SET_BLE_MODE:
//			stop_web_server();											/* WebServer終了 */
//			led_control(LED_TYPE_BLINK_LOW);							/* LED 0.5Hz点滅 */
//			rf_ctrl_ble_connect();										/* BT接続要求 */
//#if CONFIG_AUDREY_LED_BLK3
//			TimerStart_LED();											/* LED 3分タイマ開始 */
//#endif
//			ChangeState(STATE_BT_CONNECT_WAIT);							/* BT接続待ち状態へ遷移 */
//			break;

		/* Link-Key５秒押下通知 */
		case	MSG_LINKKEY_PUSH:
			StateManager_reset();										/* リセット */
			break;

#if CONFIG_AUDREY_FWUP_ALONE
		/* FW update(stand-alone)開始要求 */
		case	MSG_FW_UPDATE_SA_START:
			scale_ota_start();											/* Scaleイベント停止 */
			vTaskDelay(1000);
			stop_web_server();											/* WebServer終了 */
			vTaskDelay(1000);
			standalone_type = Param;
			led_control(LED_TYPE_CUST_1);								/* LED 250msOn/4750msOff */
			ChangeState(STATE_FWUP_SA_AP);								/* BT接続初期設定状態へ遷移 */
			break;
#endif

		}
		break;

#if CONFIG_AUDREY_FWUP_ALONE
	//------------------------------------------------------------------
	//	WiFi ファーム更新(stand-alone) AP状態
	//------------------------------------------------------------------
	case	STATE_FWUP_SA_AP:
		switch(MsgID)
		{
		/* Link-Key５秒押下通知 */
		case	MSG_LINKKEY_PUSH:
			stop_web_server();											/* WebServer終了 */
			led_control(LED_TYPE_CUST_2);								/* LED 2250msOFF/250ms周期点滅×２ */
			rf_ctrl_ble_config();										/* BT接続要求(初期設定用) */
			ChangeState(STATE_BT_CONF);									/* BT接続初期設定状態へ遷移 */
			break;

		/* FW update(stand-alone)完了 */
		case	MSG_FW_UPDATE_SA_OK:
			if(standalone_type == STANDALONE_TYPE_BOTH && Param == 0) {
				Log_Info("[stand-alone update] Change update from Scale to Wireless \r\n");
				standalone_type = STANDALONE_TYPE_WIRELESS;				/* Scale更新後にWirelessを更新 */
				ota_start_stand_alone_ap(STANDALONE_TYPE_WIRELESS, standalone_ip, standalone_scale, standalone_wireless);
			} else {
				Log_Info("[stand-alone update] Complete update \r\n");
				if(standalone_scale[0] == 0) {
					strcpy(ver_sa.scale, scale_ver);
				} else {
					strcpy(ver_sa.scale, standalone_scale);
				}
				if(standalone_wireless[0] == 0) {
					strcpy(ver_sa.wireless, AUDREY_VERSION);
				} else {
					strcpy(ver_sa.wireless, standalone_wireless);
				}
				memcpy(ver_sa.magic, FWVER_MAGIC, sizeof(FWVER_MAGIC));
				StateManager_reset();									/* リセット */
			}
			break;

		/* FW update(stand-alone)失敗 */
		case	MSG_FW_UPDATE_SA_FAIL:
			led_control(LED_TYPE_BLINK_MID);							/* LED 0.5Hz点滅 */
			break;

		}
		break;
#endif

	//------------------------------------------------------------------
	//	WiFi WPS中状態
	//------------------------------------------------------------------
	case	STATE_WIFI_WPS:
		switch(MsgID)
		{
		/* Link-Key５秒押下通知 */
		case	MSG_LINKKEY_PUSH:
			rf_ctrl_stop_wps();											/* Wifi WPS停止要求 */
			vTaskDelay(2000 * portTICK_PERIOD_MS);						/* WPS終了待ち (2秒) */
			Erase_Fastconnect_data();									/* 保存済接続情報クリア */
			StateManager_reset();										/* リセット */
			break;

		/* WiFi接続完了通知 */
		case	MSG_WIFI_LINK_COMP:
			led_control(LED_TYPE_ON);									/* LED 点灯 */
			sntp_init();												/* sntp開始 */
			data_upload_rf_state_changed(RF_STATE_WLAN);				/* 無線接続状態遷移通知(保留データ送信要求)(→ データ送信タスク) */
			ChangeState(STATE_WIFI_CONNECT_COMPLETE);					/* WiFi接続完了状態へ遷移 */
			break;

		/* WifiDHCP失敗通知 */
		case	MSG_WIFI_DHCP_FAIL:
			/* no break */
		/* WiFi切断通知 */
		case	MSG_WIFI_LINK_OFF:
			StateManager_reset();										/* リセット */
			break;
		}
		break;

	//------------------------------------------------------------------
	//	WiFi接続待ち状態
	//------------------------------------------------------------------
	case	STATE_WIFI_CONNECT_WAIT:
		switch(MsgID)
		{
		/* Link-Key５秒押下通知 */
		case	MSG_LINKKEY_PUSH:
			Erase_Fastconnect_data();									/* 保存済接続情報クリア */
			StateManager_reset();										/* リセット */
			break;

		/* WiFi接続完了通知 */
		case	MSG_WIFI_LINK_COMP:
#if CONFIG_AUDREY_LED_BLK3
			TimerStop_LED();											/* LED 3分タイマ停止 */
#endif
			led_control(LED_TYPE_ON);									/* LED 点灯 */
			sntp_init();												/* sntp開始 */
			data_upload_rf_state_changed(RF_STATE_WLAN);				/* 無線接続状態遷移通知(保留データ送信要求)(→ データ送信タスク) */
			ChangeState(STATE_WIFI_CONNECT_COMPLETE);					/* WiFi接続完了状態へ遷移 */
			break;

#if CONFIG_AUDREY_LED_BLK3
		/* LED 3分タイマ満了通知 */
		case	MSG_TIMER_M_3_LED:
			led_control(LED_TYPE_CUST_1);								/* LED 4750msOFF/250msON周期点滅 */
			break;
#endif

		/* RSSI検出開始通知 */
		case	MSG_RSSI_START:
#if CONFIG_AUDREY_LED_BLK3
			TimerStop_LED();											/* LED 3分タイマ停止 */
#endif
			ChangeState(STATE_BT_SCAN);									/* Scan実行中状態へ遷移 */
			break;

		/* WifiDHCP失敗通知 */
		case	MSG_WIFI_DHCP_FAIL:
			/* no break */
		/* Wifi再接続失敗通知 */
		case	MSG_WIFI_RECCONECT_FAIL:
			rf_ctrl_wifi_connect(NULL);									/* WiFi接続要求 */
			break;
		}
		break;

	//------------------------------------------------------------------
	//	WiFi接続完了状態
	//------------------------------------------------------------------
	case	STATE_WIFI_CONNECT_COMPLETE:
		switch(MsgID)
		{
		/* Link-Key５秒押下通知 */
		case	MSG_LINKKEY_PUSH:
			Erase_Fastconnect_data();									/* 保存済接続情報クリア */
			StateManager_reset();										/* リセット */
			break;

		/* WiFi切断通知 */
		case	MSG_WIFI_LINK_OFF:
			led_control(LED_TYPE_BLINK_LOW);							/* LED 0.5Hz点滅 */
#if CONFIG_AUDREY_LED_BLK3
			TimerStart_LED();											/* LED 3分タイマ開始 */
#endif
			data_upload_rf_state_changed(RF_STATE_NONE);				/* 無線接続状態遷移通知(→ データ送信タスク) */
			ChangeState(STATE_WIFI_CONNECT_WAIT);						/* WiFi接続待ち状態へ遷移 */
			break;

		/* RSSI検出開始通知 */
		case	MSG_RSSI_START:
			ChangeState(STATE_BT_SCAN);									/* Scan実行中状態へ遷移 */
			break;

		/* FW update開始要求 */
		case	MSG_FW_UPDATE_START:
			scale_ota_start();											/* Scaleイベント停止 */
			rf_ctrl_fw_update_start();									/* FW update開始 */
			ChangeState(STATE_WIFI_FW_UPDATE);							/* WiFi接続FW update中状態へ遷移 */
			break;

#if CONFIG_AUDREY_FWUP_ALONE
		/* FW update開始要求 */
		case	MSG_FW_UPDATE_SA_START:
			scale_ota_start();											/* Scaleイベント停止 */
			ChangeState(STATE_WIFI_FW_UPDATE);							/* WiFi接続FW update中状態へ遷移 */
			break;
#endif
		}
		break;

	//------------------------------------------------------------------
	//	WiFi接続FW update中状態
	//------------------------------------------------------------------
	case	STATE_WIFI_FW_UPDATE:
		switch(MsgID)
		{
		/* WiFi切断通知 */
		case	MSG_WIFI_LINK_OFF:
			scale_ota_stop();											/* Scaleイベント再開 */
			stop_http_update();										/* WiFi用FW update終了 */
			rf_ctrl_fw_update_finish();								/* FW update 終了状態 */
			led_control(LED_TYPE_BLINK_LOW);							/* LED 0.5Hz点滅 */
#if CONFIG_AUDREY_LED_BLK3
			TimerStart_LED();											/* LED 3分タイマ開始 */
#endif
			data_upload_rf_state_changed(RF_STATE_NONE);				/* 無線接続状態遷移通知(→ データ送信タスク) */
			ChangeState(STATE_WIFI_CONNECT_WAIT);						/* WiFi接続待ち状態へ遷移 */
			break;

		case MSG_FW_UPDATE_AGEIN:
			Log_Info("\r\n ##### fw update agein #####\r\n");
			rf_ctrl_fw_update_start();
			break;

#if CONFIG_AUDREY_FWUP_ALONE
		/* FW update(stand-alone)失敗 */
		case	MSG_FW_UPDATE_SA_FAIL:
#endif
		/* FW update 中止 */
		case   MSG_FW_UPDATE_STOP:

			vTaskDelay(2000);
			rf_ctrl_fw_update_finish();									/* FW update 終了状態 */
			scale_ota_stop();											/* Scaleイベント再開 */
			led_control(LED_TYPE_ON);									/* LED 点灯 */
			sntp_init();												/* sntp開始 */
			data_upload_rf_state_changed(RF_STATE_WLAN);				/* 無線接続状態遷移通知(保留データ送信要求)(→ データ送信タスク) */
			ChangeState(STATE_WIFI_CONNECT_COMPLETE);					/* Scaleイベント再開 */
			break;

#if CONFIG_AUDREY_FWUP_ALONE
		/* FW update(stand-alone)完了 */
		case	MSG_FW_UPDATE_SA_OK:
#endif
		/* FW update完了通知 */
		case	MSG_FW_UPDATE_COMP:
			StateManager_reset();										/* リセット */
			break;
		}
		break;

	//------------------------------------------------------------------
	//	BT接続待ち状態
	//------------------------------------------------------------------
	case	STATE_BT_CONNECT_WAIT:
		switch(MsgID)
		{
		/* Link-Key５秒押下通知 */
		case	MSG_LINKKEY_PUSH:
			Erase_Fastconnect_data();									/* 保存済接続情報クリア */
			StateManager_reset();										/* リセット */
			break;

#if CONFIG_AUDREY_LED_BLK3
		/* LED 3分タイマ満了通知 */
		case	MSG_TIMER_M_3_LED:
			led_control(LED_TYPE_CUST_1);								/* LED 4750msOFF/250msON周期点滅 */
			break;
#endif

		/* BT接続情報取得完了通知 */
		case	MSG_BT_DATA_GET_COMP:
			break;

		/* BT接続完了通知 */
		case	MSG_BT_LINK_COMP:
#if CONFIG_AUDREY_LED_BLK3
			TimerStop_LED();											/* LED 3分タイマ停止 */
#endif
			led_control(LED_TYPE_ON);									/* LED 点灯 */
			data_upload_rf_state_changed(RF_STATE_BLE);					/* 無線接続状態遷移通知(保留データ送信要求)(→ データ送信タスク) */
			ChangeState(STATE_BT_CONNECT_COMPLETE);						/* BT接続完了状態へ遷移 */
			break;

		/* RSSI検出開始通知 */
		case	MSG_RSSI_START:
#if CONFIG_AUDREY_LED_BLK3
			TimerStop_LED();											/* LED 3分タイマ停止 */
#endif
			ChangeState(STATE_BT_SCAN);									/* Scan実行中状態へ遷移 */
			break;
		}
		break;

	//------------------------------------------------------------------
	//	BT接続完了状態
	//------------------------------------------------------------------
	case	STATE_BT_CONNECT_COMPLETE:
		switch(MsgID)
		{
		/* Link-Key５秒押下通知 */
		case	MSG_LINKKEY_PUSH:
			Erase_Fastconnect_data();									/* 保存済接続情報クリア */
			StateManager_reset();										/* リセット */
			break;

		/* BT切断通知 */
		case	MSG_BT_LINK_OFF:
			led_control(LED_TYPE_BLINK_LOW);							/* LED 0.5Hz点滅 */
#if CONFIG_AUDREY_LED_BLK3
			TimerStart_LED();											/* LED 3分タイマ開始 */
#endif
			data_upload_rf_state_changed(RF_STATE_NONE);				/* 無線接続状態遷移通知(→ データ送信タスク) */
			ChangeState(STATE_BT_CONNECT_WAIT);							/* BT接続待ち状態へ遷移 */
			break;

		/* RSSI検出開始通知 */
		case	MSG_RSSI_START:
			ChangeState(STATE_BT_SCAN);									/* Scan実行中状態へ遷移 */
			break;

		/* FW update開始要求 */
		case	MSG_FW_UPDATE_START:
			scale_ota_start();											/* Scaleイベント停止 */
			rf_ctrl_fw_update_start();									/* FW update開始 */
			ChangeState(STATE_BT_FW_UPDATE);							/* BT接続FW update中状態へ遷移 */
			break;

		/* BT経由データアップロード有効化通知 */
		case	MSG_BT_DATA_UPLOAD_ENABLE:
			pending_data_upload_req();									/* 保留データ送信要求 */
		}
		break;

	//------------------------------------------------------------------
	//	BT接続FW update中状態
	//------------------------------------------------------------------
	case	STATE_BT_FW_UPDATE:
		switch(MsgID)
		{
#if 0	/* BLE経由FW更新時のBLE接続・切断はFW更新処理側で統括管理する
		 * そのため、StateManager側ではBT切断イベントに対しては反応してはならない
 	 	 */
		/* BT切断通知 */
		case	MSG_BT_LINK_OFF:
#endif
		/* BT FW update 中断 */
		case   MSG_FW_UPDATE_STOP:
			/* Gatt Server停止前に明示的に接続断を要求 */
			//gatt_conn_disc();

			scale_ota_stop();											/* Scaleイベント再開 */
			led_control(LED_TYPE_BLINK_LOW);							/* LED 0.5Hz点滅 */
#if CONFIG_AUDREY_LED_BLK3
			TimerStart_LED();											/* LED 3分タイマ開始 */
#endif
			data_upload_rf_state_changed(RF_STATE_NONE);				/* 無線接続状態遷移通知(→ データ送信タスク) */
			rf_ctrl_fw_update_finish();									/* FW update 終了状態 */
			rf_ctrl_ble_connect();										/* BT接続要求 */
			ChangeState(STATE_BT_CONNECT_WAIT);							/* BT接続待ち状態へ遷移 */
			break;

		/* FW update完了通知 */
		case	MSG_FW_UPDATE_COMP:
			StateManager_reset();										/* リセット */
			break;
		}
		break;

	//------------------------------------------------------------------
	//	Scan実行中状態
	//------------------------------------------------------------------
	case	STATE_BT_SCAN:
		switch(MsgID)
		{
		/* Link-Key５秒押下通知 */
		case	MSG_LINKKEY_PUSH:
			Erase_Fastconnect_data();									/* 保存済接続情報クリア */
			StateManager_reset();										/* リセット */
			break;

		/* RSSI検出終了通知 */
		case	MSG_RSSI_END:
			/* 前回の状態がWiFi接続中状態ならば */
			if(	(CheckStateBkUp() == STATE_WIFI_CONNECT_WAIT) ||
				(CheckStateBkUp() == STATE_WIFI_CONNECT_COMPLETE))
			{
				led_control(LED_TYPE_BLINK_LOW);						/* LED 0.5Hz点滅 */
#if CONFIG_AUDREY_LED_BLK3
				TimerStart_LED();										/* LED 3分タイマ開始 */
#endif
				ChangeState(STATE_WIFI_CONNECT_WAIT);					/* WiFi接続待ち状態へ遷移 */
			}
			/* 前回の状態がBLE接続中状態ならば */
			else if((CheckStateBkUp() == STATE_BT_CONNECT_WAIT) ||
					(CheckStateBkUp() == STATE_BT_CONNECT_COMPLETE))
			{
				led_control(LED_TYPE_BLINK_LOW);						/* LED 0.5Hz点滅 */
#if CONFIG_AUDREY_LED_BLK3
				TimerStart_LED();										/* LED 3分タイマ開始 */
#endif
				ChangeState(STATE_BT_CONNECT_WAIT);						/* BT接続待ち状態へ遷移 */
			}
			break;
		}
		break;

	}
}

int is_state_ble_conf(void)
{
	if(StateNo == STATE_BT_CONF) {
		return 1;
	} else {
		return 0;
	}
}


////////////////////////////////////////////////////////////////////////
///	全状態でのメッセージ処理
////////////////////////////////////////////////////////////////////////
static void AnalysisMessage(enum MessageID MsgID, MSG_Param_t Param)
{
	switch(MsgID)
	{
	/* ダミー */
	case	MSG_NONE:
		break;

	/* Diagコマンド要求 */
	case	MSG_DIAG_COMMAND:
		StateManagerMain(Param, PARAM_NONE);
		break;

	/* 強制状態遷移要求(試験用) */
	case	MSG_CHANGE_STATE:
		ChangeState(Param);
		break;

#if CONFIG_AUDREY_LED_KEY
	// キー短押し時、定期送信済みでLED消灯していれば一定時間LED表示
	case	MSG_LINKKEY_SHORT:
		if(led_off_connect) {
			xTimerReset(timer_led_on, 0);
			if(!led_on_key) {
				led_on_key = TRUE;
				led_revert();
			}
		}
		break;

	// 定期送信完了時にLED消灯
	case	MSG_UPLOAD_OK:
		led_off_connect = TRUE;
		TimerStop_LED_KEY();
		break;
#endif

#if CONFIG_AUDREY_FWUP_ALONE
	/* FW update(stand-alone)Ver確認 */
	case	MSG_FW_UPDATE_SA_VER:
		if(memcmp(ver_sa.magic, FWVER_MAGIC, sizeof(FWVER_MAGIC)) == 0) {
			Log_Info("[stand-alone update] Check version info\r\n");
			if(strcmp(ver_sa.scale, scale_ver) == 0 && strcmp(ver_sa.wireless, AUDREY_VERSION) == 0) {
				Log_Info("[stand-alone update] Version OK\r\n");
				led_control(LED_TYPE_CUST_4);						/* LED 2250msON/250ms周期点滅×２ */
				if(StateNo == STATE_IDLE) {
					is_stand_alone_comp = TRUE;
				}
			} else {
				led_control(LED_TYPE_BLINK_MID);					/* LED 0.5Hz点滅 */
			}
		}
		memset(&ver_sa, 0x00, sizeof(FWVER_VOLATILE));
		break;
#endif


	/* 通常メッセージ */
	default:
		StateManagerMain(MsgID, Param);									/* 各状態でのメッセージ処理 */
		break;
	}
}


////////////////////////////////////////////////////////////////////////
///	状態変更
////////////////////////////////////////////////////////////////////////
static void ChangeState(enum StateManager_state state)
{
	if((state >= STATE_IDLE) && (state < STATE_MAX))
	{
		Log_Info("\n\r[SM] Change State[%d]\r\n", state);
		StateNoBkUp	= StateNo;
		StateNo		= state;
	}
}


////////////////////////////////////////////////////////////////////////
///	前回の状態取得
////////////////////////////////////////////////////////////////////////
static enum StateManager_state CheckStateBkUp(void)
{
	return(StateNoBkUp);
}


////////////////////////////////////////////////////////////////////////
///	リセット(システム再起動)
////////////////////////////////////////////////////////////////////////
static void StateManager_reset(void)
{
	Log_Notify("\n\r[SM] Reboot!\r\n");
	rtw_msleep_os(THREAD_DELAY_TIME);
	sys_reset();														/* リセット */
}


#if CONFIG_AUDREY_LED_BLK3
////////////////////////////////////////////////////////////////////////
///	LED 3分タイマ開始
////////////////////////////////////////////////////////////////////////
static void TimerStart_LED(void)
{
	xTimerReset(timer_id_led, 0);
}


////////////////////////////////////////////////////////////////////////
///	LED 3分タイマ停止
////////////////////////////////////////////////////////////////////////
static void TimerStop_LED(void)
{
	xTimerStop(timer_id_led, 0);
}


////////////////////////////////////////////////////////////////////////
///	LED 3分タイマ満了
////////////////////////////////////////////////////////////////////////
static void TimerComp_LED(void)
{
	xTimerStop(timer_id_led, 0);
	SendMessageToStateManager(MSG_TIMER_M_3_LED, PARAM_NONE);			/* LED 3分タイマ満了通知送信 */
}
#endif

////////////////////////////////////////////////////////////////////////
///	状態管理タスク向けのメッセージ送信
////////////////////////////////////////////////////////////////////////
void SendMessageToStateManager(MSG_ID_t MsgID, MSG_Param_t Param)
{
	struct MainMessage SndMsg;

	log_record_msg((u8)MsgID, (u8)Param, (u8)StateNo);

	if(MsgID >= MSG_ERR_REBOOT && MsgID < MSG_ERR_MAX) {
		StateManager_reset();
	} else {
		if(xStateManagerQueue == 0)
		{
			Log_Error("[SM] Queue Create Error(2).\r\n");
		}
		else
		{
			SndMsg.MessageID	= MsgID;
			SndMsg.Param		= Param;
			xQueueSend(xStateManagerQueue, (void *)&SndMsg, QUEUE_TIME_OUT);
		}
	}
}


////////////////////////////////////////////////////////////////////////
///	メッセージ受信
////////////////////////////////////////////////////////////////////////
static xMainMessage ReceiveMessage(void)
{
	struct MainMessage RecMsg;

	if(xStateManagerQueue == 0)
	{
		Log_Error("[SM] Queue Create Error.\r\n");
	}
	else
	{
		if(xQueueReceive(xStateManagerQueue, (void *)&RecMsg, QUEUE_TIME_OUT))
		{
			Log_Debug("\n\r[SM] Receive Message:%04x, %d\r\n", RecMsg.MessageID, RecMsg.Param);
			return(RecMsg);
		}
	}

	RecMsg.MessageID = ERROR_CODE;
	return(RecMsg);
}


////////////////////////////////////////////////////////////////////////
///	状態管理タスク
////////////////////////////////////////////////////////////////////////
static void StateManagerMain_thread(void *param)
{
	struct MainMessage 	Msg;

#if CONFIG_AUDREY_LED_BLK3
	/* LED 3分タイマ生成*/
	timer_id_led = xTimerCreate("timer_LED",
								(TIMER_M_3_LED / portTICK_RATE_MS),
								pdTRUE,
								(void *)0,
								(TimerCallbackFunction_t)TimerComp_LED);
#endif
#if CONFIG_AUDREY_LED_KEY
	/* LED 30秒タイマ生成*/
	timer_led_on = xTimerCreate("timer_LED_On",
								(30000 / portTICK_RATE_MS),
								pdTRUE,
								(void *)0,
								(TimerCallbackFunction_t)TimerStop_LED_KEY);
#endif

	/* メッセージキュー登録 */
	xStateManagerQueue = xQueueCreate(10, sizeof(xMainMessage));

	/* 初期状態処理 */
	StateManager_Idle();

	/* 状態管理タスク処理の無限ループ */
	while(1)
	{
		Msg = ReceiveMessage();											/* メッセージ受信 */
		if(Msg.MessageID != ERROR_CODE)
		{
			AnalysisMessage(Msg.MessageID, Msg.Param);					/* メッセージに対する処理 */
		}
		rtw_msleep_os(THREAD_DELAY_TIME);
	}

	vTaskDelete(NULL);
}


////////////////////////////////////////////////////////////////////////
///	状態管理タスク登録
////////////////////////////////////////////////////////////////////////
void StateManagerInit(void)
{
	if(xTaskCreate(StateManagerMain_thread, ((const char*)"StateManagerMain_thread"), 512, NULL, tskIDLE_PRIORITY + 2, NULL) != pdPASS)
	{
		Log_Info("\n\r%s xTaskCreate(init_thread) failed\r\n", __FUNCTION__);
	}
}
