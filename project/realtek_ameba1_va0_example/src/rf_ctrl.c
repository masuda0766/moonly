/*
 * Copyright (C) 2017 SHARP Corporation. All rights reserved.
 */

/**
 * 無線通信制御部
 */
#include <platform_opts.h>
#include <lwip_netconf.h>
#include <platform/platform_stdlib.h>
#include <cJSON.h>
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <osdep_service.h>
#include <fast_connect.h>
#include "wifi/wifi_ind.h"
#include "wifi/wifi_conf.h"
#include "wps/wps_defs.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "device.h"
#include "timer_api.h"
#include "osdep_service.h"
#include "pinmap.h"
#include "basic_types.h"
#include "version.h"
#include "state_manager.h"
#include "rf_ctrl.h"
#include "rf_ctrl_wifi.h"
#include "rf_ctrl_ble.h"
#include "data_upload.h"
#include "ota_http_client.h"
#include "ota_ble_update.h"
#include "flash_api.h"
#include "device_lock.h"
#include "webserver.h"
#include "bt_common.h"
#include "bt_gatt_server.h"
#include "ble_scan.h"
#include "bt_gatt.h"
#include "bt_util.h"
#include "led_util.h"
#include "sys_api.h"

/** MACアドレス **/
u8	audrey_mac[6] = {0};
unsigned char AUDREY_API_KEY[40+1] = {0};

#if CONFIG_LWIP_LAYER
extern struct netif xnetif[NET_IF_NUM];
#endif

/* 状態管理 */
static volatile RF_TYPE		rf_state;		/* 無線接続状態 */
static volatile RF_TYPE		saved_rf_type = RF_TYPE_NONE;/* 無線接続種別 (ビーコン受信処理遷移前状態) */
static bool		fFwUpdate = FALSE;			/* フラグ：FW Update処理中 */
static bool		fBeacon   = FALSE;			/* フラグ：Beacon受信処理中 */

/* Wifi APモード関連 */
#define AP_PASSWORD		"k6qpnu8d"
static	rtw_wifi_setting_t	wifi_ap_setting;

/* Wifi STAモード関連 */
static rtw_wifi_setting_t	wifi_sta_setting;
static volatile bool		fDhcpRetry = FALSE;			/* フラグ：DHCPリトライ */
static int					DhcpRetyCount = 0;			/* DHCPリトライ回数 */
#define DHCP_RETRY_MAX		30							/* DHCPリトライ回数上限 */

/* BT関連 */
static tUint8 br_bd[BD_ADDR_LEN];
static tUint8 ble_bd[BD_ADDR_LEN];

/* reconnectデータ保存用callback関数 */
extern int		wlan_wrtie_reconnect_data_to_flash( u8 *data, uint32_t len );

extern u8 *bdaddr_to_str(u8 *str, u8 *arr);

/* Wifi再接続失敗通知 (Wifi -> rf_ctrl) */
void rf_ctrl_wifi_reconnection_failure_cb(void)
{
	Log_Notify("\r\n[RF Ctrl] Wifi Reconnection failed.\r\n");

	SendMessageToStateManager(MSG_WIFI_RECCONECT_FAIL, PARAM_NONE);

	return;
}

/* Wifi接続通知 (lwip_netconf -> rf_ctrl) (DHCPのアドレス取得完了時に通知) */
void rf_ctrl_wifi_connected_cb(void)
{
	Log_Notify("\r\n[RF Ctrl] Wifi STA mode connection is ready.\r\n");
	/* 設定を記憶 */
	wifi_get_setting(WLAN0_NAME, &wifi_sta_setting);
#if AUDREY_LOG_DEBUG
	wifi_show_setting(WLAN0_NAME,&wifi_sta_setting);
#endif

	/* DHCPリトライ回数クリア */
	DhcpRetyCount = 0;

	/* すでに接続済みの場合はStateManagerへの通知は不要 */
	if (rf_state == RF_TYPE_WLAN_STA) {
		return;
	}

	/* 内部状態を遷移 */
	rf_state = RF_TYPE_WLAN_STA;
	Log_Info("[RF Ctrl] rf_state %d\r\n", rf_state);
	SendMessageToStateManager(MSG_WIFI_LINK_COMP, PARAM_NONE);
	return;
}
/* Wifi切断通知 (Wifi -> rf_ctrl) */
void rf_ctrl_wifi_disconnected_cb(RF_CTRL_ERR err)
{
	if (err == RF_ERR_DHCP_FAIL) {
		if (++DhcpRetyCount > DHCP_RETRY_MAX) {
			Log_Notify("[RF Ctrl] DHCP process failed => Abandon\r\n");
			SendMessageToStateManager(MSG_ERR_WIFI_LINK, PARAM_WIFI_ERR_DHCP);
			/* DHCPの失敗をStateManagerへ通知 */
			rf_state = RF_TYPE_NONE;
			Log_Info("[RF Ctrl] rf_state %d\r\n", rf_state);
			DhcpRetyCount = 0;
			SendMessageToStateManager(MSG_WIFI_DHCP_FAIL, PARAM_NONE);
		} else {
			Log_Info("[RF Ctrl] DHCP process failed => Retry [%d]\r\n", DhcpRetyCount);
			fDhcpRetry = TRUE;
		}
		return;
	}

	/* WPS処理中は無視する */
	if (rf_state == RF_TYPE_WLAN_WPS) {
		Log_Info("[RF Ctrl] wps in progress : wifi disconnect event ignored\r\n");
		return;
	}

	Log_Notify("\r\n[RF Ctrl] Wifi STA mode connection is disconnected. err=%d\r\n", err);
	/* 内部状態を遷移 */
	if(rf_state != RF_TYPE_BLE_CONF) {
		rf_state = RF_TYPE_NONE;
		Log_Info("[RF Ctrl] rf_state %d\r\n", rf_state);
	}
	SendMessageToStateManager(MSG_WIFI_LINK_OFF, PARAM_NONE);
	return;
}

/* Wifi接続要求 */
RF_CTRL_ERR rf_ctrl_wifi_connect(rtw_wifi_config_t *conf)
{
	flash_t		flash;
	struct wlan_fast_reconnect *data;
	int ret;

	/* 未接続またはAPモード中の状態以外では受け付けない */
	if ((rf_state != RF_TYPE_NONE) && (rf_state != RF_TYPE_WLAN_AP) && (rf_state != RF_TYPE_BLE_CONF)) {
		Log_Error("[RF Ctrl] wifi_connect : state_eror (%d)\r\n", rf_state);
		SendMessageToStateManager(MSG_ERR_WIFI_LINK, PARAM_COMMON_STATE);
		return RF_ERR_STATE;
	}

	Log_Info("\r\n[RF Ctrl] start Wifi STA mode rf_state:%d\r\n", rf_state);
	saved_rf_type = RF_TYPE_WLAN_STA;	/* Beacon受信からの再接続用無線種別を記憶 */

	/* BLE初期設定中からの場合はBT停止 */
	if(rf_state == RF_TYPE_BLE_CONF) {
#if CONFIG_AUDREY_ALWAYS_BT_ON
		Log_Info("\r\n[RF Ctrl] pause bt gatt server\r\n");
		rf_ctrl_ble_pause_server();
#else
		Log_Info("\r\n[RF Ctrl] stop bt gatt server\r\n");
		stop_bt_gatt_server();
#endif //#if CONFIG_AUDREY_ALWAYS_BT_ON
	}

	data = (struct wlan_fast_reconnect *)rtw_zmalloc(sizeof(struct wlan_fast_reconnect));
	if(data){
		device_mutex_lock(RT_DEV_LOCK_FLASH);
		flash_stream_read(&flash, FAST_RECONNECT_DATA, sizeof(struct wlan_fast_reconnect), (uint8_t *)data);
		device_mutex_unlock(RT_DEV_LOCK_FLASH);

		Log_Info("\r\n[psk_essid     ] %s\r\n", data->psk_essid);
		Log_Info("\r\n[psk_passphrase] %s\r\n", data->psk_passphrase);
		Log_Info("\r\n[channel       ] %d\r\n", data->channel);
	} else {
		Log_Error("\r\n[RF Ctrl] wifi_connect : malloc failed\r\n");
		SendMessageToStateManager(MSG_ERR_REBOOT, PARAM_COMMON_NOMEM);
		return RF_ERR_SYSTEM;
	}

	if ((data->channel < 1) || (data->channel > 13)) {
		/* ReconnectData で channel が未設定の場合、webserverによる設定がされた状態 -> 手動接続 */
		/* APモードからSTAモードへ切り替え */
		if(rf_state != RF_TYPE_BLE_CONF) {
			stop_web_server();
			vTaskDelay(20);
		}
		wifi_off();
		vTaskDelay(20);
		wifi_on(RTW_MODE_STA);

		/* reconnect用情報の保存関数へのポインタ登録 および 自動再接続ON */
		p_write_reconnect_ptr = wlan_wrtie_reconnect_data_to_flash;
		wifi_set_autoreconnect(1);

		ret = wifi_connect(data->psk_essid, data->security_type, data->psk_passphrase, strlen(data->psk_essid), strlen(data->psk_passphrase), 0, NULL);
		if(ret == RTW_SUCCESS){
			LwIP_DHCP(0, DHCP_START);
		} else {
			Log_Error("\r\n[RF Ctrl] wifi_conect() failed\r\n");
			SendMessageToStateManager(MSG_ERR_WIFI_LINK, PARAM_WIFI_ERR_CONNECT + data->channel);
		}
	} else {
		/* ReconnectData の channel が 1～13 の場合、一度接続済 -> 自動接続 */
		/* APモードからSTAモードへ切り替え */
		wifi_off();
		vTaskDelay(20);
		fast_connect();
		wifi_on(RTW_MODE_STA);
	}

	rtw_mfree((u8 *)data, sizeof(struct wlan_fast_reconnect));

	return RF_ERR_SUCCESS;
}

/* BLEペアリング完了通知 */
void rf_ctrl_gap_simple_pairing_complete_cb(tBT_GapSimplePairingCmplStru *param)
{
	char str_bd[BD_ADDR_LEN * 2 + 1];

	bdaddr_to_str(str_bd, param->addr.bd);
	Log_Notify("\r\n[RF Ctrl][BLE][pairingcomp_cb] paired with device %s\r\n", str_bd);

	return;
}

/* BLE接続通知 */
void rf_ctrl_ble_connected_cb(tBT_GattConnCmplStru *param)
{
	char str_bd[BD_ADDR_LEN * 2 + 1];

	bdaddr_to_str(str_bd, param->addr.bd);

	if (param->res != BT_RES_OK) {
		Log_Error("\r\n[RF Ctrl][BLE][  connected_cb] result error : 0x%x\r\n", param->res);
		/* GattServer停止, BT切断をStateManagerへ通知 */
		rf_ctrl_ble_stop_server();
		SendMessageToStateManager(MSG_BT_LINK_OFF, PARAM_NONE);
		SendMessageToStateManager(MSG_ERR_BT_LINK, PARAM_COMMON_NETDOWN);
		return;
	} else {
		Log_Notify("\r\n[RF Ctrl][BLE][  connected_cb] connected from device %s\r\n", str_bd);
	}

	/* ペアリングの完了を待つ必要があるか無いかの識別ができないため
	 * 一律接続確立時点で処理をすすめる */

	/* 内部状態を遷移 *//* GattServer起動時点で遷移させるが念のためここでも設定 */
	if(rf_state != RF_TYPE_BLE_CONF) {
		rf_state = RF_TYPE_BLE;
		Log_Info("[RF Ctrl] rf_state %d\r\n", rf_state);
	}

	/* MSG_BT_LINK_COMP を送信 */
	SendMessageToStateManager(MSG_BT_LINK_COMP, PARAM_NONE);

	return;
}
/* BLE切断通知 */
void rf_ctrl_ble_disconnected_cb(tBT_GattDisconnCmplStru *param)
{
	if (param == NULL) {
		Log_Notify("\r\n[RF Ctrl][BLE][disconnected_cb] no param\r\n");
	} else {
		Log_Notify("\r\n[RF Ctrl][BLE][disconnected_cb] result : 0x%x\r\n", param->res);
	}

	/* FW更新処理中は無視する */
	if (fFwUpdate == TRUE) {
		Log_Info("[RF Ctrl][BLE] FW Update in progress : BLE disconnect event ignored\r\n");
		return;
	}

	/* 内部状態を遷移 */
	if(rf_state != RF_TYPE_BLE_CONF) {
		rf_state = RF_TYPE_NONE;
		Log_Info("[RF Ctrl] rf_state %d\r\n", rf_state);
	}
	/* BT切断をStateManagerへ通知 */
	SendMessageToStateManager(MSG_BT_LINK_OFF, PARAM_NONE);

	/* 再接続待ち状態 (GattServer自体は動作を続けている点に注意) */

	return;
}
/* BT接続要求  (GattServer起動&対向側からの接続待ち) */
RF_CTRL_ERR rf_ctrl_ble_connect(void)
{
	/* 未接続またはAPモード中の状態以外では受け付けない */
	if ((rf_state != RF_TYPE_NONE) && (rf_state != RF_TYPE_WLAN_AP) && (rf_state != RF_TYPE_BLE_CONF)) {
		Log_Error("[RF Ctrl] ble_connect  : state_eror (%d)\r\n", rf_state);
		SendMessageToStateManager(MSG_ERR_BT_LINK, PARAM_COMMON_STATE);
		return RF_ERR_STATE;
	}

	Log_Info("\r\n[RF Ctrl] start BLE mode\r\n");
	saved_rf_type = RF_TYPE_BLE;	/* Beacon受信からの再接続用無線種別を記憶 */

	if(rf_state == RF_TYPE_BLE_CONF) {
		rf_state = RF_TYPE_BLE;
		Log_Info("[RF Ctrl] rf_state %d\r\n", rf_state);
	} else {
		/* APモード停止 */
		wifi_off();
		vTaskDelay(20);
//		wifi_on(RTW_MODE_NONE);
//		vTaskDelay(20);
#if CONFIG_AUDREY_ALWAYS_BT_ON
		/* Advertising有効化 */
		rf_ctrl_adv_enable();
#endif //#if CONFIG_AUDREY_ALWAYS_BT_ON
		/* GattServer起動 */
		if (rf_ctrl_ble_start_server() == RF_ERR_SUCCESS) {
			/* GattServerの起動ができた時点で内部状態を遷移 */
			rf_state = RF_TYPE_BLE;
			Log_Info("[RF Ctrl] rf_state %d\r\n", rf_state);
		} else {
			Log_Error("[RF Ctrl] ble_connect  : rf_ctrl_ble_start_server failed\r\n");
			/* BT切断をStateManagerへ通知 *//* [ToDo] BT切断よりFatalErrorを通知すべきか？ */
			SendMessageToStateManager(MSG_BT_LINK_OFF, PARAM_NONE);
			SendMessageToStateManager(MSG_ERR_BT_LINK, PARAM_COMMON_NETDOWN);
			return RF_ERR_SYSTEM;
		}
	}
	/* 接続待ち状態 (接続されたらrf_ctrl_ble_connected_cb()が呼ばれる) */

	return RF_ERR_SUCCESS;
}

/* BT接続要求(初期設定用)  (GattServer起動&対向側からの接続待ち) */
RF_CTRL_ERR rf_ctrl_ble_config(void)
{
	/* 未接続またはwifiモード中の状態以外では受け付けない */
	if ((rf_state != RF_TYPE_NONE) && (rf_state != RF_TYPE_WLAN_AP) && (rf_state != RF_TYPE_WLAN_STA)) {
		Log_Error("[RF Ctrl] ble_config  : state_eror (%d)\r\n", rf_state);
		SendMessageToStateManager(MSG_ERR_BT_LINK, PARAM_COMMON_STATE);
		return RF_ERR_STATE;
	}
	Log_Info("\r\n[RF Ctrl] start BLE config\r\n");
	rf_state = RF_TYPE_BLE_CONF;
	Log_Info("[RF Ctrl] rf_state %d\r\n", rf_state);

	/* wifi停止 */
	wifi_off();
	vTaskDelay(20);
//	wifi_on(RTW_MODE_NONE);
//	vTaskDelay(20);
#if CONFIG_AUDREY_ALWAYS_BT_ON
	/* Advertising有効化 */
	rf_ctrl_adv_enable();
#endif //#if CONFIG_AUDREY_ALWAYS_BT_ON
	/* GattServer起動 */
	if (rf_ctrl_ble_start_server() != RF_ERR_SUCCESS) {
		/* GattServerの起動ができても内部状態はそのまま */
		Log_Error("[RF Ctrl] ble_connect  : rf_ctrl_ble_start_server failed\r\n");
		/* BT切断をStateManagerへ通知 *//* [ToDo] BT切断よりFatalErrorを通知すべきか？ */
		SendMessageToStateManager(MSG_BT_LINK_OFF, PARAM_NONE);
		SendMessageToStateManager(MSG_ERR_BT_LINK, PARAM_COMMON_NETDOWN);
		return RF_ERR_SYSTEM;
	}

	/* 接続待ち状態 (接続されたらrf_ctrl_ble_connected_cb()が呼ばれる) */

	return RF_ERR_SUCCESS;
}

#if CONFIG_AUDREY_ALWAYS_BT_ON
/* BT起動(GattServer起動) */
RF_CTRL_ERR rf_ctrl_ble_init(void)
{
	/* 未接続またはwifiモード中の状態以外では受け付けない */
	if ((rf_state != RF_TYPE_NONE) && (rf_state != RF_TYPE_WLAN_AP) && (rf_state != RF_TYPE_WLAN_STA)) {
		Log_Error("[RF Ctrl] ble_config  : state_eror (%d)\r\n", rf_state);
		SendMessageToStateManager(MSG_ERR_BT_LINK, PARAM_COMMON_STATE);
		return RF_ERR_STATE;
	}
	Log_Info("\r\n[RF Ctrl] start BLE init\r\n");
	Log_Info("[RF Ctrl] rf_state %d\r\n", rf_state);
	/* Advertising無効化 */
	rf_ctrl_adv_disable();
	/* GattServer起動 */
	if (rf_ctrl_ble_start_server() == RF_ERR_SUCCESS) {
		Log_Info("[RF Ctrl] BLE init success\r\n");
	} else {
		Log_Error("[RF Ctrl] ble_init  : rf_ctrl_ble_start_server failed\r\n");
		return RF_ERR_SYSTEM;
	}
	return RF_ERR_SUCCESS;
}
#endif //#if CONFIG_AUDREY_ALWAYS_BT_ON

/* BT経由データアップロード有効化通知 */
RF_CTRL_ERR rf_ctrl_ble_data_upload_enable_cb(void)
{
	SendMessageToStateManager(MSG_BT_DATA_UPLOAD_ENABLE, PARAM_NONE);
	return RF_ERR_SUCCESS;
}

/* Wifi AP 起動要求 */
RF_CTRL_ERR rf_ctrl_start_ap(void)
{
	int ret = 0;

	/* 未接続またはAPモード中の状態以外では受け付けない */
	if ((rf_state != RF_TYPE_NONE) && (rf_state != RF_TYPE_WLAN_AP) && (rf_state != RF_TYPE_BLE_CONF)) {
		Log_Error("[RF Ctrl] start_ap     : state_eror (%d)\r\n", rf_state);
		return RF_ERR_STATE;
	}

	Log_Debug("\r\n[RF Ctrl][start_ap] ssid=%s", wifi_ap_setting.ssid);
	Log_Debug("\r\n[RF Ctrl][start_ap] ssid_len=%d", strlen((char*)wifi_ap_setting.ssid));
	Log_Debug("\r\n[RF Ctrl][start_ap] security_type=%d", wifi_ap_setting.security_type);
	Log_Debug("\r\n[RF Ctrl][start_ap] password=%s", wifi_ap_setting.password);
	Log_Debug("\r\n[RF Ctrl][start_ap] password_len=%d", strlen((char*)wifi_ap_setting.password));
	Log_Debug("\r\n[RF Ctrl][start_ap] channel=%d\r\n", wifi_ap_setting.channel);
	ret = wifi_restart_ap(	wifi_ap_setting.ssid,
							wifi_ap_setting.security_type,
							wifi_ap_setting.password,
							strlen((char*)wifi_ap_setting.ssid),
							strlen((char*)wifi_ap_setting.password),
							wifi_ap_setting.channel);
	Log_Debug("\r\n[RF Ctrl][start_ap] wifi_restart_ap() ret = %d\r\n", ret);

	/* BLE初期設定中からの場合はBT停止 */
	if(rf_state == RF_TYPE_BLE_CONF) {
		Log_Info("\r\n[RF Ctrl] stop bt gatt server\r\n");
		stop_bt_gatt_server();
	}

	rf_state = RF_TYPE_WLAN_AP;
	Log_Info("[RF Ctrl] rf_state %d\r\n", rf_state);
	return RF_ERR_SUCCESS;
}

/* Wifi APモード用 Advertising開始要求 */
//RF_CTRL_ERR rf_ctrl_start_adv(void)
//{
//	if (rf_ctrl_start_adv_on_AP() == FALSE) {
//		Log_Notify("\r\n[RF Ctrl][start_ap] Advertising start failed.\r\n");
//	}
//
//	return RF_ERR_SUCCESS;
//}

/* Wifi APモード用 Advertising停止要求 */
//RF_CTRL_ERR rf_ctrl_stop_adv(void)
//{
//	if (rf_ctrl_stop_adv_on_AP() == FALSE) {
//		Log_Notify("\r\n[RF Ctrl][stop_ap] Advertising stop failed.\r\n");
//	}
//
//	return RF_ERR_SUCCESS;
//}

extern int wps_start(u16 wps_config, char *pin, u8 channel, char *ssid);
extern write_reconnect_ptr p_write_reconnect_ptr;
static volatile bool fWpsStart = FALSE;
/* Wifi WPS開始要求 */
RF_CTRL_ERR rf_ctrl_start_wps(void)
{
	rtw_wifi_setting_t	setting = {0};
	int	ret = 0;

	/* 未接続の状態以外では受け付けない */
	if ((rf_state != RF_TYPE_NONE) && (rf_state != RF_TYPE_WLAN_AP) && (rf_state != RF_TYPE_BLE_CONF)) {
		Log_Error("[RF Ctrl] start_wps    : state_eror (%d)\r\n", rf_state);
		SendMessageToStateManager(MSG_ERR_WIFI_LINK, PARAM_COMMON_STATE);
		return RF_ERR_STATE;
	}

	/* BLE初期設定中からの場合はBT停止 */
	if(rf_state == RF_TYPE_BLE_CONF) {
		Log_Info("\r\n[RF Ctrl] stop bt gatt server\r\n");
		stop_bt_gatt_server();
	}

	Log_Info("\r\n[RF Ctrl] start Wifi WPS\r\n");
	/* BT停止 */
	Log_Info("\r\n[RF Ctrl] stop bt gatt server\r\n");
	stop_bt_gatt_server();

	/* APモードからSTAモードへ切り替え */
	if(rf_state != RF_TYPE_BLE_CONF) {
		stop_web_server();
//		rf_ctrl_stop_adv();
		vTaskDelay(20);
	}
	/* 内部状態遷移 */
	rf_state = RF_TYPE_WLAN_WPS;
	Log_Info("[RF Ctrl] rf_state %d\r\n", rf_state);

	wifi_off();
	vTaskDelay(20);
	wifi_on(RTW_MODE_STA);

	/* reconnect用情報の保存関数へのポインタ登録 および 自動再接続ON */
	p_write_reconnect_ptr = wlan_wrtie_reconnect_data_to_flash;
	wifi_set_autoreconnect(1);

	/* フラグをセット */
	fWpsStart = TRUE;

	saved_rf_type = RF_TYPE_WLAN_STA;	/* Beacon受信からの再接続用無線種別を記憶 */

	return RF_ERR_SUCCESS;
}

/* Wifi WPS停止要求 */
extern void wps_stop(void);
RF_CTRL_ERR rf_ctrl_stop_wps(void)
{
	wps_stop();
	return RF_ERR_SUCCESS;
}

/* Wifi切断要求 */
RF_CTRL_ERR rf_ctrl_wifi_disconnect(void)
{
	int ret = 0;

	/* WLAN STAモードの接続状態以外では受け付けない */
	if (rf_state != RF_TYPE_WLAN_STA) {
		Log_Error("[RF Ctrl] wifi_discon  : state_eror (%d)\r\n", rf_state);
		SendMessageToStateManager(MSG_ERR_WIFI_LINK, PARAM_COMMON_STATE);
		return RF_ERR_STATE;
	}

	/* データ送信処理中の場合は完了を待つ */
	while (is_data_upload_in_progress() == TRUE)
	{
		vTaskDelay(200 * portTICK_PERIOD_MS);
	}
	rf_ctrl_wait_http_resp();	/* データ送信応答受信待ち */

	ret = wifi_disconnect();
	if (ret != 0) {
		Log_Error("[RF Ctrl] wifi_discon  : Failed to disconnect\r\n");
		SendMessageToStateManager(MSG_ERR_WIFI_LINK, PARAM_WIFI_ERR_DISC);
		return RF_ERR_NETWORK;
	}

	return RF_ERR_SUCCESS;
}

/* BT切断要求 (自機側からの能動切断＆GattServer停止) */
RF_CTRL_ERR rf_ctrl_ble_disconnect(void)
{
	/* BLEの接続状態以外では受け付けない */
	if ((rf_state != RF_TYPE_BLE) && (rf_state != RF_TYPE_BLE_CONF)) {
		Log_Error("[RF Ctrl] ble_discon   : state_eror (%d)\r\n", rf_state);
		SendMessageToStateManager(MSG_ERR_BT_LINK, PARAM_COMMON_STATE);
		return RF_ERR_STATE;
	}

	/* データ送信処理中の場合は完了を待つ */
	while (is_data_upload_in_progress() == TRUE)
	{
		vTaskDelay(200 * portTICK_PERIOD_MS);
	}

	/* データ送信処理が途中の場合は完了を待ち合わせる */
	rf_ctrl_wait_ble_resp();

	/* Gatt Server停止前に明示的に接続断を要求 */
	gatt_conn_disc(FALSE);
	/* GattServer停止 */
	rf_ctrl_ble_stop_server();

	return RF_ERR_SUCCESS;
}

/* データ送信可否チェック */
RF_CTRL_ERR rf_ctrl_check_connection(void)
{
	uint8_t *ip_addr;

	/* 無線接続が確立していない状態では送信不可 */
	if ((rf_state != RF_TYPE_BLE) && (rf_state != RF_TYPE_WLAN_STA)) {
		Log_Debug("\r\n[RF Ctrl][connection check] no RF connection - state : %d / FWUP : %s / Beacon : %s\r\n",
				rf_state, fFwUpdate == TRUE ? "o" : "x", fBeacon == TRUE ? "o" : "x");
		return RF_ERR_STATE;
	}

	/* FW Update中は送信不可 */
	if (fFwUpdate == TRUE) {
		Log_Info("\r\n[RF Ctrl][connection check] FW Update in progress\r\n");
		return RF_ERR_STATE;
	}

	/* ビーコン受信中は送信不可 */
	if (fBeacon == TRUE) {
		Log_Info("\r\n[RF Ctrl][connection check] Looking for badges\r\n");
		return RF_ERR_STATE;
	}

	/* WLAN接続時、IP addressが取得できていない状態＆HTTP応答待ち中では送信不可*/
	if (rf_state == RF_TYPE_WLAN_STA) {
		/* IP addressチェック */
		ip_addr = LwIP_GetIP(&xnetif[0]);
		// Log_Info("  ipaddr         = %u.%u.%u.%u\r\n", ip_addr[0], ip_addr[1], ip_addr[2], ip_addr[3]);
		if ((ip_addr[0] == 0x00) && (ip_addr[1] == 0x00) && (ip_addr[2] == 0x00) && (ip_addr[3] == 0x00)) {
			Log_Info("\r\n[RF Ctrl][connection check] IP addr is not set\r\n");
			return RF_ERR_STATE;
		}
		/* HTTP応答待ちチェック */
		if (rf_ctrl_wait_http_resp() == FALSE) {
			/* 応答待ちが完了しない場合、強制停止(socketのcloseを試みる) */
			if (rf_ctrl_force_stop_waiting_resp() == FALSE) {
				/* 強制停止も失敗した場合はリセット */
				Log_Error("[RF Ctrl][connection check] wait response eror\r\n");
				SendMessageToStateManager(MSG_ERR_REBOOT, PARAM_COMMON_NETDOWN);
				return RF_ERR_SYSTEM;
			}
		}
	}

	/* BLE接続時、Data Transport Characterisitc の Indication が許可されていなければ送信不可  */
	if (rf_state == RF_TYPE_BLE) {
		if (rf_ctrl_data_transport_char_desc_check() == FALSE) {
			Log_Info("\r\n[RF Ctrl][connection check] Data Transport Characteristic is disabled\r\n");
			return RF_ERR_RETRY;	/* しばらく待てば設定されるはずなのでリトライを要求 */
		}
		/* BLE応答待ちチェック */
		if (rf_ctrl_wait_ble_resp() == FALSE) {
			Log_Info("\r\n[RF Ctrl][connection check] BLE connection is busy\r\n");
			return RF_ERR_STATE;
		}
	}

	return RF_ERR_SUCCESS;
}

#if CONFIG_AUDREY_DBG_UPLOAD
/* デバッグデータ(重量)送信処理 */
RF_CTRL_ERR rf_ctrl_dbg_w_data_upload(dbg_weight_data *data)
{
	cJSON_Hooks memoryHook;
	cJSON *IOTJSObject = NULL, *weightJSObject = NULL, *arrayJSObject = NULL;
	char *iot_json = NULL;
	char buf[13] = {0};
	int i = 0, j = 0;
	RF_CTRL_ERR ret = RF_ERR_SUCCESS;

	memoryHook.malloc_fn = malloc;
	memoryHook.free_fn = free;
	cJSON_InitHooks(&memoryHook);

	Log_Notify("\r\n[RF Ctrl][upload] dbg_w\r\n");
	if((IOTJSObject = cJSON_CreateObject()) != NULL) {
		cJSON_AddItemToObject(IOTJSObject, "id", cJSON_CreateNumber(data->id));
		sprintf(buf, "%02x%02x%02x%02x%02x%02x",
				data->mac[0], data->mac[1], data->mac[2],
				data->mac[3], data->mac[4], data->mac[5] );
		cJSON_AddItemToObject(IOTJSObject, "mac"    , cJSON_CreateString(buf));
		cJSON_AddItemToObject(IOTJSObject, "weight" , weightJSObject = cJSON_CreateArray());
		if (weightJSObject == NULL) {
			Log_Error("\r\n JSON_CreateObject (array of weight) failed\r\n");
			cJSON_Delete(IOTJSObject);
			SendMessageToStateManager(MSG_ERR_DUL_FATAL, PARAM_DUL_ERR_JSON+1);
			ret = RF_ERR_SYSTEM;
			goto exit;
		}
		for (i = 0 ; i < WEIGHT_DBG_MAX ; i++) {
			if ((arrayJSObject = cJSON_CreateObject()) != NULL) {
				cJSON_AddItemToArray(weightJSObject, arrayJSObject);
				cJSON_AddItemToObject(arrayJSObject, "t", cJSON_CreateNumber_u(data->w_dbg[i].time));
				cJSON_AddItemToObject(arrayJSObject, "b", cJSON_CreateNumber(data->w_dbg[i].body));
				cJSON_AddItemToObject(arrayJSObject, "u", cJSON_CreateNumber(data->w_dbg[i].urine));
			} else {
				Log_Error("\r\n JSON_CreateObject (element of weight) failed : idx = %d\r\n", i);
				cJSON_Delete(IOTJSObject);
				SendMessageToStateManager(MSG_ERR_DUL_FATAL, PARAM_DUL_ERR_JSON+2);
				ret = RF_ERR_SYSTEM;
				goto exit;
			}
		}
	} else {
		Log_Error("\r\n JSON_CreateObject failed\r\n");
		SendMessageToStateManager(MSG_ERR_DUL_FATAL, PARAM_DUL_ERR_JSON+3);
		ret = RF_ERR_SYSTEM;
		goto exit;
	}

#if AUDREY_LOG_DEBUG
	iot_json = cJSON_Print(IOTJSObject);
	Log_Debug("\r\n[RF Ctrl] ===== JSON data =====\r\n%s\r\n===============================\r\n", iot_json);
	Log_Debug("\r\n[RF Ctrl] available heap %d : minimum ever free heep %d\r\n", xPortGetFreeHeapSize(), xPortGetMinimumEverFreeHeapSize());
	if (iot_json != NULL) {
		free(iot_json);
	}
#endif
	iot_json = cJSON_PrintUnformatted(IOTJSObject);
	cJSON_Delete(IOTJSObject);
	if (iot_json == NULL) {
		Log_Error("\r\n[RF Ctrl] Generating JSON data failed : available heap %d : minimum ever free heep %d\r\n",
				xPortGetFreeHeapSize(), xPortGetMinimumEverFreeHeapSize());
		SendMessageToStateManager(MSG_ERR_DUL_FATAL, PARAM_DUL_ERR_JSON+4);
		ret = RF_ERR_SYSTEM;
		goto exit;
	}

	switch (rf_state)
	{
	case RF_TYPE_WLAN_STA:
#ifndef RF_CTRL_WIFI_USE_SSL
		ret = rf_ctrl_data_upload_wifi(data->id, iot_json);
#else
		ret = rf_ctrl_data_upload_ssl(data->id, iot_json);
#endif
		break;
#if CONFIG_AUDREY_DBG_BLE
	case RF_TYPE_BLE:
		ret = rf_ctrl_data_upload_ble(data->id, iot_json);
		break;
#endif
	default:
		Log_Error("\r\n[RF Ctrl] rf_state error: %d\r\n", rf_state);
		SendMessageToStateManager(MSG_ERR_DUL, PARAM_COMMON_STATE);
		ret = RF_ERR_STATE;
		break;
	}

exit:
	if (iot_json != NULL) {
		free(iot_json);
	}

	return ret;
}

RF_CTRL_ERR rf_ctrl_dbg_b_data_upload(dbg_beacon_data *data)
{
	cJSON_Hooks memoryHook;
	cJSON *IOTJSObject = NULL, *inJSObject = NULL, *outJSObject = NULL, *arrayJSObject = NULL, *rssiJSObject = NULL;
	char *iot_json = NULL;
	char buf[13] = {0};
	int i = 0, j = 0;
	RF_CTRL_ERR ret = RF_ERR_SUCCESS;

	memoryHook.malloc_fn = malloc;
	memoryHook.free_fn = free;
	cJSON_InitHooks(&memoryHook);

	Log_Notify("\r\n[RF Ctrl][upload] dbg_b\r\n");
	if((IOTJSObject = cJSON_CreateObject()) != NULL) {
		cJSON_AddItemToObject(IOTJSObject, "id", cJSON_CreateNumber(data->id));
		sprintf(buf, "%02x%02x%02x%02x%02x%02x",
				data->mac[0], data->mac[1], data->mac[2],
				data->mac[3], data->mac[4], data->mac[5] );
		cJSON_AddItemToObject(IOTJSObject, "mac"     , cJSON_CreateString(buf));
		cJSON_AddItemToObject(IOTJSObject, "time_in" , cJSON_CreateNumber_u(data->time_in));
		cJSON_AddItemToObject(IOTJSObject, "time_out", cJSON_CreateNumber_u(data->time_out));
		if ((inJSObject = cJSON_CreateArray()) == NULL) {
			Log_Error("\r\n JSON_CreateObject (array of in) failed\r\n");
			cJSON_Delete(IOTJSObject);
			SendMessageToStateManager(MSG_ERR_DUL_FATAL, PARAM_DUL_ERR_JSON+5);
			ret = RF_ERR_SYSTEM;
			goto exit;
		}
		cJSON_AddItemToObject(IOTJSObject, "in", inJSObject);
		for (i = 0 ; i < data->badge_cnt ; i++) {
			if ((arrayJSObject = cJSON_CreateObject()) != NULL) {
				cJSON_AddItemToArray(inJSObject, arrayJSObject);
				sprintf(buf, "%02x%02x%02x%02x%02x%02x",
					data->bdg_dat[i].conf.bd_addr[5], data->bdg_dat[i].conf.bd_addr[4], data->bdg_dat[i].conf.bd_addr[3],
					data->bdg_dat[i].conf.bd_addr[2], data->bdg_dat[i].conf.bd_addr[1], data->bdg_dat[i].conf.bd_addr[0] );
				cJSON_AddItemToObject(arrayJSObject, "bd", cJSON_CreateString(buf));
				if ((rssiJSObject = cJSON_CreateArray()) == NULL) {
					Log_Error("\r\n JSON_CreateObject (array of in/rssi) failed\r\n");
					cJSON_Delete(IOTJSObject);
					SendMessageToStateManager(MSG_ERR_DUL_FATAL, PARAM_DUL_ERR_JSON+6);
					ret = RF_ERR_SYSTEM;
					goto exit;
				}
				cJSON_AddItemToObject(arrayJSObject, "rssi", rssiJSObject);
				cJSON_AddItemToArray(rssiJSObject, cJSON_CreateNumber((double)(data->bdg_dat[i].conf.body)));
				for (j = 0 ; j < BADGE_RCV_MAX ; j++) {
					if(data->bdg_dat[i].buf[BADGE_TYPE_ENTRY].rssi[j] == 0) break;
					cJSON_AddItemToArray(rssiJSObject, cJSON_CreateNumber((double)(data->bdg_dat[i].buf[BADGE_TYPE_ENTRY].rssi[j])));
				}
			} else {
				Log_Error("\r\n JSON_CreateObject (element of in) failed : idx = %d\r\n", i);
				cJSON_Delete(IOTJSObject);
				SendMessageToStateManager(MSG_ERR_DUL_FATAL, PARAM_DUL_ERR_JSON+7);
				ret = RF_ERR_SYSTEM;
				goto exit;
			}
		}
		if ((outJSObject = cJSON_CreateArray()) == NULL) {
			Log_Error("\r\n JSON_CreateObject (array of out) failed\r\n");
			cJSON_Delete(IOTJSObject);
			SendMessageToStateManager(MSG_ERR_DUL_FATAL, PARAM_DUL_ERR_JSON+8);
			ret = RF_ERR_SYSTEM;
			goto exit;
		}
		cJSON_AddItemToObject(IOTJSObject, "out", outJSObject);
		for (i = 0 ; i < data->badge_cnt ; i++) {
			if ((arrayJSObject = cJSON_CreateObject()) != NULL) {
				cJSON_AddItemToArray(outJSObject, arrayJSObject);
				sprintf(buf, "%02x%02x%02x%02x%02x%02x",
					data->bdg_dat[i].conf.bd_addr[5], data->bdg_dat[i].conf.bd_addr[4], data->bdg_dat[i].conf.bd_addr[3],
					data->bdg_dat[i].conf.bd_addr[2], data->bdg_dat[i].conf.bd_addr[1], data->bdg_dat[i].conf.bd_addr[0] );
				cJSON_AddItemToObject(arrayJSObject, "bd", cJSON_CreateString(buf));
				if ((rssiJSObject = cJSON_CreateArray()) == NULL) {
					Log_Error("\r\n JSON_CreateObject (array of out/rssi) failed\r\n");
					cJSON_Delete(IOTJSObject);
					SendMessageToStateManager(MSG_ERR_DUL_FATAL, PARAM_DUL_ERR_JSON+9);
					ret = RF_ERR_SYSTEM;
					goto exit;
				}
				cJSON_AddItemToObject(arrayJSObject, "rssi", rssiJSObject);
				cJSON_AddItemToArray(rssiJSObject, cJSON_CreateNumber((double)(data->bdg_dat[i].conf.body)));
				for (j = 0 ; j < BADGE_RCV_MAX ; j++) {
					if(data->bdg_dat[i].buf[BADGE_TYPE_EXIT].rssi[j] == 0) break;
					cJSON_AddItemToArray(rssiJSObject, cJSON_CreateNumber((double)(data->bdg_dat[i].buf[BADGE_TYPE_EXIT].rssi[j])));
				}
			} else {
				Log_Error("\r\n JSON_CreateObject (element of out) failed : idx = %d\r\n", i);
				cJSON_Delete(IOTJSObject);
				SendMessageToStateManager(MSG_ERR_DUL_FATAL, PARAM_DUL_ERR_JSON+10);
				ret = RF_ERR_SYSTEM;
				goto exit;
			}
		}
	} else {
		Log_Error("\r\n JSON_CreateObject failed\r\n");
		SendMessageToStateManager(MSG_ERR_DUL_FATAL, PARAM_DUL_ERR_JSON+11);
		ret = RF_ERR_SYSTEM;
		goto exit;
	}

#if AUDREY_LOG_DEBUG
	iot_json = cJSON_Print(IOTJSObject);
	Log_Debug("\r\n[RF Ctrl] ===== JSON data =====\r\n%s\r\n===============================\r\n", iot_json);
	Log_Debug("\r\n[RF Ctrl] available heap %d : minimum ever free heep %d\r\n", xPortGetFreeHeapSize(), xPortGetMinimumEverFreeHeapSize());
	if (iot_json != NULL) {
		free(iot_json);
	}
#endif
	iot_json = cJSON_PrintUnformatted(IOTJSObject);
	cJSON_Delete(IOTJSObject);
	if (iot_json == NULL) {
		Log_Error("\r\n[RF Ctrl] Generating JSON data failed : available heap %d : minimum ever free heep %d\r\n",
				xPortGetFreeHeapSize(), xPortGetMinimumEverFreeHeapSize());
		SendMessageToStateManager(MSG_ERR_DUL_FATAL, PARAM_DUL_ERR_JSON+12);
		ret = RF_ERR_SYSTEM;
		goto exit;
	}

	switch (rf_state)
	{
	case RF_TYPE_WLAN_STA:
#ifndef RF_CTRL_WIFI_USE_SSL
		ret = rf_ctrl_data_upload_wifi(data->id, iot_json);
#else
		ret = rf_ctrl_data_upload_ssl(data->id, iot_json);
#endif
		break;
#if CONFIG_AUDREY_DBG_BLE
	case RF_TYPE_BLE:
		ret = rf_ctrl_data_upload_ble(data->id, iot_json);
		break;
#endif
	default:
		Log_Error("\r\n[RF Ctrl] rf_state error: %d\r\n", rf_state);
		SendMessageToStateManager(MSG_ERR_DUL, PARAM_COMMON_STATE);
		ret = RF_ERR_STATE;
		break;
	}

exit:
	if (iot_json != NULL) {
		free(iot_json);
	}

	return ret;
}

/* デバッグデータ(rssi)送信処理 */
RF_CTRL_ERR rf_ctrl_dbg_r_data_upload(dbg_rssi_data *data)
{
	cJSON_Hooks memoryHook;
	cJSON *IOTJSObject = NULL, *weightJSObject = NULL, *arrayJSObject = NULL;
	char *iot_json = NULL;
	char buf[13] = {0};
	int i = 0, j = 0;
	RF_CTRL_ERR ret = RF_ERR_SUCCESS;

	memoryHook.malloc_fn = malloc;
	memoryHook.free_fn = free;
	cJSON_InitHooks(&memoryHook);

	Log_Notify("\r\n[RF Ctrl][upload] dbg_r\r\n");
	if((IOTJSObject = cJSON_CreateObject()) != NULL) {
		cJSON_AddItemToObject(IOTJSObject, "id", cJSON_CreateNumber(data->id));
		sprintf(buf, "%02x%02x%02x%02x%02x%02x",
				data->mac[0], data->mac[1], data->mac[2],
				data->mac[3], data->mac[4], data->mac[5] );
		cJSON_AddItemToObject(IOTJSObject, "mac"    , cJSON_CreateString(buf));
		sprintf(buf, "%02x%02x%02x%02x%02x%02x",
				data->bd[0], data->bd[1], data->bd[2],
				data->bd[3], data->bd[4], data->bd[5] );
		cJSON_AddItemToObject(IOTJSObject, "bd"    , cJSON_CreateString(buf));
		cJSON_AddItemToObject(IOTJSObject, "rssi" , weightJSObject = cJSON_CreateArray());
		if (weightJSObject == NULL) {
			Log_Error("\r\n JSON_CreateObject (array of rssi) failed\r\n");
			cJSON_Delete(IOTJSObject);
			SendMessageToStateManager(MSG_ERR_DUL_FATAL, PARAM_DUL_ERR_JSON+13);
			ret = RF_ERR_SYSTEM;
			goto exit;
		}
		for (i = 0 ; i < data->cnt ; i++) {
			if ((arrayJSObject = cJSON_CreateObject()) != NULL) {
				cJSON_AddItemToArray(weightJSObject, arrayJSObject);
				cJSON_AddItemToObject(arrayJSObject, "t", cJSON_CreateNumber_u(data->bdg_dat[i].time));
				cJSON_AddItemToObject(arrayJSObject, "r", cJSON_CreateNumber(data->bdg_dat[i].rssi));
			} else {
				Log_Error("\r\n JSON_CreateObject (element of rssi) failed : idx = %d\r\n", i);
				cJSON_Delete(IOTJSObject);
				SendMessageToStateManager(MSG_ERR_DUL_FATAL, PARAM_DUL_ERR_JSON+14);
				ret = RF_ERR_SYSTEM;
				goto exit;
			}
		}
	} else {
		Log_Error("\r\n JSON_CreateObject failed\r\n");
		SendMessageToStateManager(MSG_ERR_DUL_FATAL, PARAM_DUL_ERR_JSON+15);
		ret = RF_ERR_SYSTEM;
		goto exit;
	}

#if AUDREY_LOG_DEBUG
	iot_json = cJSON_Print(IOTJSObject);
	Log_Debug("\r\n[RF Ctrl] ===== JSON data =====\r\n%s\r\n===============================\r\n", iot_json);
	Log_Debug("\r\n[RF Ctrl] available heap %d : minimum ever free heep %d\r\n", xPortGetFreeHeapSize(), xPortGetMinimumEverFreeHeapSize());
	if (iot_json != NULL) {
		free(iot_json);
	}
#endif
	iot_json = cJSON_PrintUnformatted(IOTJSObject);
	cJSON_Delete(IOTJSObject);
	if (iot_json == NULL) {
		Log_Error("\r\n[RF Ctrl] Generating JSON data failed : available heap %d : minimum ever free heep %d\r\n",
				xPortGetFreeHeapSize(), xPortGetMinimumEverFreeHeapSize());
		SendMessageToStateManager(MSG_ERR_DUL_FATAL, PARAM_DUL_ERR_JSON+16);
		ret = RF_ERR_SYSTEM;
		goto exit;
	}

	switch (rf_state)
	{
	case RF_TYPE_WLAN_STA:
#ifndef RF_CTRL_WIFI_USE_SSL
		ret = rf_ctrl_data_upload_wifi(data->id, iot_json);
#else
		ret = rf_ctrl_data_upload_ssl(data->id, iot_json);
#endif
		break;
#if CONFIG_AUDREY_DBG_BLE
	case RF_TYPE_BLE:
		ret = rf_ctrl_data_upload_ble(data->id, iot_json);
		break;
#endif
	default:
		Log_Error("\r\n[RF Ctrl] rf_state error: %d\r\n", rf_state);
		SendMessageToStateManager(MSG_ERR_DUL, PARAM_COMMON_STATE);
		ret = RF_ERR_STATE;
		break;
	}

exit:
	if (iot_json != NULL) {
		free(iot_json);
	}

	return ret;
}
#endif // CONFIG_AUDREY_DBG_UPLOAD


#if CONFIG_AUDREY_LOG_UPLOAD
/* エラーログ送信処理 */
RF_CTRL_ERR rf_ctrl_log_e_upload(log_error_data *data)
{
	cJSON_Hooks memoryHook;
	cJSON *IOTJSObject = NULL, *weightJSObject = NULL, *arrayJSObject = NULL;
	char *iot_json = NULL;
	char buf[13] = {0};
	int i = 0, j = 0;
	RF_CTRL_ERR ret = RF_ERR_SUCCESS;

	memoryHook.malloc_fn = malloc;
	memoryHook.free_fn = free;
	cJSON_InitHooks(&memoryHook);

	Log_Notify("\r\n[RF Ctrl][upload] log\r\n");
	if((IOTJSObject = cJSON_CreateObject()) != NULL) {
		cJSON_AddItemToObject(IOTJSObject, "id", cJSON_CreateNumber(TYPE_ID_LOG_ERROR));
		sprintf(buf, "%02x%02x%02x%02x%02x%02x",
				data->mac[0], data->mac[1], data->mac[2],
				data->mac[3], data->mac[4], data->mac[5] );
		cJSON_AddItemToObject(IOTJSObject, "mac"    , cJSON_CreateString(buf));
		cJSON_AddItemToObject(IOTJSObject, "err" , weightJSObject = cJSON_CreateArray());
		if (weightJSObject == NULL) {
			Log_Error("\r\n JSON_CreateObject (array of rssi) failed\r\n");
			cJSON_Delete(IOTJSObject);
			SendMessageToStateManager(MSG_ERR_DUL_FATAL, PARAM_DUL_ERR_JSON+19);
			ret = RF_ERR_SYSTEM;
			goto exit;
		}
		for (i = 0 ; i < data->v_cnt ; i++) {
			if ((arrayJSObject = cJSON_CreateObject()) != NULL) {
				cJSON_AddItemToArray(weightJSObject, arrayJSObject);
				cJSON_AddItemToObject(arrayJSObject, "t", cJSON_CreateNumber_u(data->ver[i].time));
				cJSON_AddItemToObject(arrayJSObject, "e", cJSON_CreateString(data->ver[i].info));
			} else {
				Log_Error("\r\n JSON_CreateObject (element of ver log) failed : idx = %d\r\n", i);
				cJSON_Delete(IOTJSObject);
				SendMessageToStateManager(MSG_ERR_DUL_FATAL, PARAM_DUL_ERR_JSON+20);
				ret = RF_ERR_SYSTEM;
				goto exit;
			}
		}
		for (i = 0 ; i < data->e_cnt ; i++) {
			if ((arrayJSObject = cJSON_CreateObject()) != NULL) {
				cJSON_AddItemToArray(weightJSObject, arrayJSObject);
				cJSON_AddItemToObject(arrayJSObject, "t", cJSON_CreateNumber_u(data->err[i].time));
				cJSON_AddItemToObject(arrayJSObject, "e", cJSON_CreateNumber(
				(data->err[i].id < MSG_ERR ? (unsigned int)((data->err[i].id - MSG_ERR_RSP + 70) * 100)
				 : (unsigned int)((data->err[i].id - MSG_ERR) * 100))
				 + (unsigned int)data->err[i].param));
			} else {
				Log_Error("\r\n JSON_CreateObject (element of error log) failed : idx = %d\r\n", i);
				cJSON_Delete(IOTJSObject);
				SendMessageToStateManager(MSG_ERR_DUL_FATAL, PARAM_DUL_ERR_JSON+21);
				ret = RF_ERR_SYSTEM;
				goto exit;
			}
		}
		for (i = 0 ; i < data->f_cnt ; i++) {
			if ((arrayJSObject = cJSON_CreateObject()) != NULL) {
				cJSON_AddItemToArray(weightJSObject, arrayJSObject);
				cJSON_AddItemToObject(arrayJSObject, "t", cJSON_CreateNumber_u(data->fault[i].time));
				cJSON_AddItemToObject(arrayJSObject, "e", cJSON_CreateString(data->fault[i].pc));
			} else {
				Log_Error("\r\n JSON_CreateObject (element of fault log) failed : idx = %d\r\n", i);
				cJSON_Delete(IOTJSObject);
				SendMessageToStateManager(MSG_ERR_DUL_FATAL, PARAM_DUL_ERR_JSON+24);
				ret = RF_ERR_SYSTEM;
				goto exit;
			}
		}
	} else {
		Log_Error("\r\n JSON_CreateObject failed\r\n");
		SendMessageToStateManager(MSG_ERR_DUL_FATAL, PARAM_DUL_ERR_JSON+22);
		ret = RF_ERR_SYSTEM;
		goto exit;
	}

#if AUDREY_LOG_DEBUG
	iot_json = cJSON_Print(IOTJSObject);
	Log_Debug("\r\n[RF Ctrl] ===== JSON data =====\r\n%s\r\n===============================\r\n", iot_json);
	Log_Debug("\r\n[RF Ctrl] available heap %d : minimum ever free heep %d\r\n", xPortGetFreeHeapSize(), xPortGetMinimumEverFreeHeapSize());
	if (iot_json != NULL) {
		free(iot_json);
	}
#endif
	iot_json = cJSON_PrintUnformatted(IOTJSObject);
	cJSON_Delete(IOTJSObject);
	if (iot_json == NULL) {
		Log_Error("\r\n[RF Ctrl] Generating JSON data failed : available heap %d : minimum ever free heep %d\r\n",
				xPortGetFreeHeapSize(), xPortGetMinimumEverFreeHeapSize());
		SendMessageToStateManager(MSG_ERR_DUL_FATAL, PARAM_DUL_ERR_JSON+23);
		ret = RF_ERR_SYSTEM;
		goto exit;
	}

	switch (rf_state)
	{
	case RF_TYPE_WLAN_STA:
#ifndef RF_CTRL_WIFI_USE_SSL
		ret = rf_ctrl_data_upload_wifi(data->id, iot_json);
#else
		ret = rf_ctrl_data_upload_ssl(data->id, iot_json);
#endif
		break;
	case RF_TYPE_BLE:
		ret = rf_ctrl_data_upload_ble(data->id, iot_json);
		break;
	default:
		Log_Error("\r\n[RF Ctrl] rf_state error: %d\r\n", rf_state);
		SendMessageToStateManager(MSG_ERR_DUL, PARAM_COMMON_STATE);
		ret = RF_ERR_STATE;
		break;
	}

exit:
	if (iot_json != NULL) {
		free(iot_json);
	}

	return ret;
}
#endif // CONFIG_AUDREY_LOG_UPLOAD

/* 測定データ送信 */
RF_CTRL_ERR rf_ctrl_data_upload(upload_data *data)
{
	cJSON_Hooks memoryHook;
	cJSON *IOTJSObject = NULL, *lowJSObject = NULL;
	char *iot_json = NULL;
	char buf[13] = {0};
	int i = 0;
	RF_CTRL_ERR ret = RF_ERR_SUCCESS;

	memoryHook.malloc_fn = malloc;
	memoryHook.free_fn = free;
	cJSON_InitHooks(&memoryHook);

	/* JSONデータ編集 */
	if((IOTJSObject = cJSON_CreateObject()) != NULL) {
		cJSON_AddItemToObject(IOTJSObject, "id", cJSON_CreateNumber(data->id));
		switch (data->id)
		{
		case TYPE_ID_ENTERING:
#if CONFIG_AUDREY_DBG_UPLOAD
		case TYPE_ID_ENTERING_DBG:
#endif
			Log_Notify("\r\n[RF Ctrl][upload] entry\r\n");
			cJSON_AddItemToObject(IOTJSObject, "body" , cJSON_CreateNumber(data->body));
			cJSON_AddItemToObject(IOTJSObject, "urine", cJSON_CreateNumber(data->urine));
			cJSON_AddItemToObject(IOTJSObject, "tem"  , cJSON_CreateNumber(data->tem));
			sprintf(buf, "%02x%02x%02x%02x%02x%02x",
					data->mac[0], data->mac[1], data->mac[2],
					data->mac[3], data->mac[4], data->mac[5] );
			cJSON_AddItemToObject(IOTJSObject, "mac"  , cJSON_CreateString(buf));
			cJSON_AddItemToObject(IOTJSObject, "stay" , cJSON_CreateNumber(data->stay));
			cJSON_AddItemToObject(IOTJSObject, "time" , cJSON_CreateNumber_u(data->time));
			sprintf(buf, "%02x%02x%02x%02x%02x%02x",
					data->bd[0], data->bd[1], data->bd[2],
					data->bd[3], data->bd[4], data->bd[5] );
			if (strcmp(buf, "000000000000")) {
				cJSON_AddItemToObject(IOTJSObject, "bd"   , cJSON_CreateString(buf));
			}
			if (data->low.num > 0) {
				cJSON_AddItemToObject(IOTJSObject, "low"  , lowJSObject = cJSON_CreateArray());
				for (i = 0 ; i < BD_NUM_MAX ; i++) {
					sprintf(buf, "%02x%02x%02x%02x%02x%02x",
							data->low.bd[i][0], data->low.bd[i][1], data->low.bd[i][2],
							data->low.bd[i][3], data->low.bd[i][4], data->low.bd[i][5] );
					if (strcmp(buf, "000000000000")) {
						cJSON_AddItemToArray(lowJSObject, cJSON_CreateString(buf));
					}
				}
			}
			if(data->flag == 1) {
				cJSON_AddItemToObject(IOTJSObject, "flag", cJSON_CreateNumber(data->flag));
			}
			break;
		case TYPE_ID_PERIODIC:
			Log_Notify("\r\n[RF Ctrl][upload] period\r\n");
			cJSON_AddItemToObject(IOTJSObject, "body" , cJSON_CreateNumber(data->body));
			cJSON_AddItemToObject(IOTJSObject, "urine", cJSON_CreateNumber(data->urine));
			cJSON_AddItemToObject(IOTJSObject, "tem"  , cJSON_CreateNumber(data->tem));
			sprintf(buf, "%02x%02x%02x%02x%02x%02x",
					data->mac[0], data->mac[1], data->mac[2],
					data->mac[3], data->mac[4], data->mac[5] );
			cJSON_AddItemToObject(IOTJSObject, "mac"  , cJSON_CreateString(buf));
			cJSON_AddItemToObject(IOTJSObject, "time" , cJSON_CreateNumber_u(data->time));
			cJSON_AddItemToObject(IOTJSObject, "ver"  , cJSON_CreateString(data->ver));
			cJSON_AddItemToObject(IOTJSObject, "conn" , cJSON_CreateNumber(data->conn));
			if(data->init == 1) {
				cJSON_AddItemToObject(IOTJSObject, "init", cJSON_CreateNumber(data->init));
			}
			break;
		case TYPE_ID_ALERT:
			Log_Notify("\r\n[RF Ctrl][upload] alert\r\n");
			sprintf(buf, "%02x%02x%02x%02x%02x%02x",
					data->mac[0], data->mac[1], data->mac[2],
					data->mac[3], data->mac[4], data->mac[5] );
			cJSON_AddItemToObject(IOTJSObject, "mac"  , cJSON_CreateString(buf));
			cJSON_AddItemToObject(IOTJSObject, "time" , cJSON_CreateNumber_u(data->time));
			cJSON_AddItemToObject(IOTJSObject, "msg"  , cJSON_CreateString(data->msg));
			break;
		default:
			Log_Error("\r\n[RF Ctrl] invalid data type : %d\r\n", data->id);
			SendMessageToStateManager(MSG_ERR_DUL_FATAL, PARAM_COMMON_PARAM);
			ret = RF_ERR_PARAM;
			goto exit;
		}
	} else {
		Log_Error("\r\n JSON_CreateObject failed\r\n");
		SendMessageToStateManager(MSG_ERR_DUL_FATAL, PARAM_DUL_ERR_JSON+17);
		ret = RF_ERR_SYSTEM;
		goto exit;
	}

#if AUDREY_LOG_DEBUG
	iot_json = cJSON_Print(IOTJSObject);
	Log_Debug("\r\n[RF Ctrl] ===== JSON data =====\r\n%s\r\n===============================\r\n", iot_json);
	Log_Debug("\r\n[RF Ctrl] available heap %d : minimum ever free heep %d\r\n", xPortGetFreeHeapSize(), xPortGetMinimumEverFreeHeapSize());
	if (iot_json != NULL) {
		free(iot_json);
	}
#endif
	iot_json = cJSON_PrintUnformatted(IOTJSObject);
	cJSON_Delete(IOTJSObject);
	if (iot_json == NULL) {
		Log_Error("\r\n[RF Ctrl] Generating JSON data failed : available heap %d : minimum ever free heep %d\r\n",
				xPortGetFreeHeapSize(), xPortGetMinimumEverFreeHeapSize());
		SendMessageToStateManager(MSG_ERR_DUL_FATAL, PARAM_DUL_ERR_JSON+18);
		ret = RF_ERR_SYSTEM;
		goto exit;
	}

	switch (rf_state)
	{
	case RF_TYPE_WLAN_STA:
#ifndef RF_CTRL_WIFI_USE_SSL
		ret = rf_ctrl_data_upload_wifi(data->id, iot_json);
#else
		ret = rf_ctrl_data_upload_ssl(data->id, iot_json);
#endif
		break;
	case RF_TYPE_BLE:
		ret = rf_ctrl_data_upload_ble(data->id, iot_json);
		break;
	default:
		Log_Error("\r\n[RF Ctrl] rf_state error: %d\r\n", rf_state);
		SendMessageToStateManager(MSG_ERR_DUL, PARAM_COMMON_STATE);
		ret = RF_ERR_STATE;
		break;
	}

exit:
	if (iot_json != NULL) {
		free(iot_json);
	}

	return ret;
}

/* 応答データ解析処理 */
RF_CTRL_ERR rf_ctrl_parse_upload_resp(int id, char *data)
{
	cJSON_Hooks	memoryHook;
	cJSON		*IOTJSObject, *dataJSObject, *petArrayJSObject;
//	cJSON		*returnJSObject;
	cJSON		*petJSObject, *bdJSObject, *bodyJSObject, *verJSObject;
#if CONFIG_AUDREY_DBG_UPLOAD
	cJSON		*dbgJSObject;
#endif
	int			result = 0;
	char		*bd, *body, *ver;
	int			petnum = 0;
	PET_DATA	petdata[BADGE_PET_MAX];
	int			i, j;
	char		buf[3];		/* BD_ADDRの1byte分の文字列(2文字)の格納用 */
	upload_resp	resp_data;

	/* 応答受信失敗時はパラメータをNULLとしてエラーのみ通知 */
	if (data == NULL) {
		switch (id) {
		case TYPE_ID_ENTERING:
#if CONFIG_AUDREY_DBG_UPLOAD
		case TYPE_ID_ENTERING_DBG:
#endif
			data_upload_entering_result(NULL);
			break;
		case TYPE_ID_PERIODIC:
			data_upload_periodic_result(NULL);
			break;
		case TYPE_ID_ALERT:
			data_upload_alert_result(NULL);
			break;
#if CONFIG_AUDREY_DBG_UPLOAD
		case TYPE_ID_DBG_WEIGHT:
			data_upload_debug_weight_result(NULL);
			break;
		case TYPE_ID_DBG_BEACON:
			data_upload_debug_beacon_result(NULL);
			break;
		case TYPE_ID_DBG_RSSI:
			data_upload_debug_rssi_result(NULL);
			break;
#endif // CONFIG_AUDREY_DBG_UPLOAD
#if CONFIG_AUDREY_LOG_UPLOAD
		case TYPE_ID_LOG_ERROR:
			data_upload_log_error_result(NULL);
			break;
		case TYPE_ID_LOG_HF:
			data_upload_log_hf_result(NULL);
			break;
#endif // CONFIG_AUDREY_LOG_UPLOAD
		default:
			break;
		}
		data_up_req_end();
		return RF_ERR_SUCCESS;
	}

	memoryHook.malloc_fn = malloc;
	memoryHook.free_fn = free;
	cJSON_InitHooks(&memoryHook);

	memset(petdata, 0, sizeof(petdata));
	memset(&resp_data, 0, sizeof(resp_data));

	if((IOTJSObject = cJSON_Parse(data)) != NULL) {
		/* result_codeによる再送は行わない */
		resp_data.result = UPLOAD_RESULT_SUCCESS;
//		returnJSObject = cJSON_GetObjectItem(IOTJSObject, "result_code");
//		if (returnJSObject) {
//			resp_data.result = returnJSObject->valueint;
//		} else {
//			Log_Error("\r\n[RF Ctrl] no result_code in response\r\n");
//			SendMessageToStateManager(MSG_ERR_DUL, PARAM_DUL_ERR_RSP);
//			resp_data.result = UPLOAD_RESULT_FAIL;
//		}
		/* 定期送信 以外での "data" の 内容については用途が無いため無視する → 必要が生じた時点で処理を追加すること */
		resp_data.id = id;
		if (id == TYPE_ID_PERIODIC) {
			dataJSObject= cJSON_GetObjectItem(IOTJSObject, "data");
			if (dataJSObject) {
				verJSObject = cJSON_GetObjectItem(dataJSObject, "ver");
				if (verJSObject) {
					strcpy(resp_data.ver, verJSObject->valuestring);
				}
				petArrayJSObject = cJSON_GetObjectItem(dataJSObject, "pet");
				if (petArrayJSObject) {
					resp_data.petnum = cJSON_GetArraySize(petArrayJSObject);
					if (resp_data.petnum > BADGE_PET_MAX) {
						resp_data.petnum = BADGE_PET_MAX;
					}
					for (i = 0 ; i < resp_data.petnum ; i++) {
						petJSObject = cJSON_GetArrayItem(petArrayJSObject, i);
						if (petJSObject) {
							bdJSObject = cJSON_GetObjectItem(petJSObject, "bd");
							if (strlen(bdJSObject->valuestring) != (BD_ADDR_LEN * 2)) {
								Log_Info("\r\n[RF Ctrl] unknown BD_ADDR : %s", bdJSObject->valuestring);
								continue;
							}
							for (j = 0 ; j < BD_ADDR_LEN ; j++) {
								strncpy(buf, (bdJSObject->valuestring)+(j*2), 2);
								resp_data.petdata[i].bd[j] = strtol(buf, NULL, 16);
							}
							bodyJSObject = cJSON_GetObjectItem(petJSObject, "body");
							resp_data.petdata[i].body = bodyJSObject->valueint;
						}
					}
				}
#if CONFIG_AUDREY_DBG_UPLOAD
				dbgJSObject = cJSON_GetObjectItem(dataJSObject, "dbg");
				if (dbgJSObject) {
					resp_data.dbg = dbgJSObject->valueint;
				} else {
					resp_data.dbg = 0;
				}
#endif
			}
		}
	} else {
		Log_Error("\r\n[RF Ctrl] JSON parsing failed\r\n");
		SendMessageToStateManager(MSG_ERR_DUL, PARAM_DUL_ERR_RSP);
		/* 解析失敗による再送は行わない */
		resp_data.result = UPLOAD_RESULT_SUCCESS;
//		resp_data.result = UPLOAD_RESULT_FAIL;
	}

	cJSON_Delete(IOTJSObject);

	switch (id) {
	case TYPE_ID_ENTERING:
#if CONFIG_AUDREY_DBG_UPLOAD
	case TYPE_ID_ENTERING_DBG:
#endif
		data_upload_entering_result(&resp_data);
		break;
	case TYPE_ID_PERIODIC:
		data_upload_periodic_result(&resp_data);
		break;
	case TYPE_ID_ALERT:
		data_upload_alert_result(&resp_data);
		break;
#if CONFIG_AUDREY_DBG_UPLOAD
	case TYPE_ID_DBG_WEIGHT:
		data_upload_debug_weight_result(&resp_data);
		break;
	case TYPE_ID_DBG_BEACON:
		data_upload_debug_beacon_result(&resp_data);
		break;
	case TYPE_ID_DBG_RSSI:
		data_upload_debug_rssi_result(&resp_data);
		break;
#endif // CONFIG_AUDREY_DBG_UPLOAD
#if CONFIG_AUDREY_LOG_UPLOAD
	case TYPE_ID_LOG_ERROR:
		data_upload_log_error_result(&resp_data);
		break;
	case TYPE_ID_LOG_HF:
		data_upload_log_hf_result(&resp_data);
		break;
#endif // CONFIG_AUDREY_LOG_UPLOAD
	default:
		break;
	}

	data_up_req_end();
	return RF_ERR_SUCCESS;
}

/* FW Update開始要求 */
RF_CTRL_ERR rf_ctrl_fw_update_start(void)
{
	/* 状態チェック */
	if ((rf_state != RF_TYPE_BLE) && (rf_state != RF_TYPE_WLAN_STA)) {
		Log_Error("[RF Ctrl] fw_update_st : state_eror (%d)\r\n", rf_state);
		SendMessageToStateManager(MSG_ERR_FWUPDATE, PARAM_COMMON_STATE);
		return RF_ERR_STATE;
	}

	/* ビーコン受信中はFW更新開始不可 */
	if (fBeacon == TRUE) {
		Log_Error("[RF Ctrl] fw_update_st : state_eror (Beacon)\r\n");
		SendMessageToStateManager(MSG_ERR_FWUPDATE, PARAM_COMMON_STATE);
		return RF_ERR_STATE;
	}

	/* 抑止用のフラグをセット */
	fFwUpdate = TRUE;

	/* ファーム更新処理部に対して更新開始を要求 */
	if (rf_state == RF_TYPE_WLAN_STA) {
		Log_Notify("\r\n[RF Ctrl] Start FW Update on Wifi.\r\n");
		/* Wifi経由 FW update開始 */
		start_http_update();
	} else {
		Log_Notify("\r\n[RF Ctrl] Start FW Update on BLE.\r\n");
		/* 応答受信待ちチェック */
		rf_ctrl_wait_ble_resp();
		/* Gatt Server停止前に明示的に接続断を要求 */
		gatt_conn_disc(FALSE);
		/* Gatt Server停止 */
		rf_ctrl_ble_stop_server();
		/* BLE経由 FW update開始 */
		start_ota_ble_update();
		/* FW更新処理部に制御を渡すため、内部状態を遷移 */
		rf_state = RF_TYPE_NONE;
		Log_Info("[RF Ctrl] rf_state %d\r\n", rf_state);
	}

	return RF_ERR_SUCCESS;
}

/* FW Update完了通知 */
RF_CTRL_ERR rf_ctrl_fw_update_finish(void)
{
	/* 抑止用のフラグをクリア */
	fFwUpdate = FALSE;

	return RF_ERR_SUCCESS;
}

/* ビーコン受信開始 */
RF_CTRL_ERR rf_ctrl_start_beacon(void)
{
	if ((rf_state != RF_TYPE_BLE) && (rf_state != RF_TYPE_WLAN_STA) && (rf_state != RF_TYPE_NONE)) {
		Log_Error("[RF Ctrl] start_beadcon : state_eror (%d)\r\n", rf_state);
		SendMessageToStateManager(MSG_ERR_SCAN, PARAM_COMMON_STATE);
		return RF_ERR_STATE;
	}

	/* FW更新中はビーコン受信開始不可 */
	if (fFwUpdate == TRUE) {
		Log_Error("[RF Ctrl] start_beadcon : state_eror (FW Update)\r\n");
		SendMessageToStateManager(MSG_ERR_SCAN, PARAM_COMMON_STATE);
		return RF_ERR_STATE;
	}

	/* Wifi接続時 HTTP通信中であれば応答受信が完了するまで待ち合わせ */
	if (rf_state == RF_TYPE_WLAN_STA) {
		if (rf_ctrl_wait_http_resp() == FALSE) {
			/* 応答待ちが完了しない場合、強制停止(socketのcloseを試みる) */
			if (rf_ctrl_force_stop_waiting_resp() == FALSE) {
				/* 失敗した場合は state_managerにエラーを通知 */
				SendMessageToStateManager(MSG_ERR_DUL, PARAM_COMMON_NETDOWN);
			}
		}
	}

	/* すでにフラグONの場合は何もせずreturn */
	if (fBeacon == TRUE) {
		Log_Info("\r\n[RF Ctrl][beacon_] already started\r\n");
		return RF_ERR_DONE;
	}

	/* データ送信処理中の場合は完了を待つ */
	while (is_data_upload_in_progress() == TRUE)
	{
		vTaskDelay(200 * portTICK_PERIOD_MS);
	}
	if (rf_state == RF_TYPE_WLAN_STA) {
		rf_ctrl_wait_http_resp();	/* データ送信応答受信待ち */
	} else if (rf_state == RF_TYPE_BLE) {
		rf_ctrl_wait_ble_resp();	/* データ送信応答受信待ち */
	}

	/* 抑止用のフラグをセット */
	fBeacon = TRUE;

	/* RSSI検出開始通知 送信 */
	SendMessageToStateManager(MSG_RSSI_START, PARAM_NONE);

	if (rf_state == RF_TYPE_WLAN_STA) {
		/* Wifi接続中の場合は Wifi Off */
		wifi_off();
	} else if (rf_state == RF_TYPE_BLE) {
		/* 応答受信待ちチェック */
		rf_ctrl_wait_ble_resp();
		/* Gatt Server停止前に明示的に接続断を要求 */
		gatt_conn_disc(FALSE);
#if CONFIG_AUDREY_ALWAYS_BT_ON
		/* BLE接続中の場合はBLE Server一時停止 */
		rf_ctrl_ble_pause_server();
#else
		/* BLE接続中の場合はBLE Server停止 */
		rf_ctrl_ble_stop_server();
#endif //#if CONFIG_AUDREY_ALWAYS_BT_ON
		/* BLE接続中はRTW_MODE_NONEで起動済みなので Wifi OFF/ON は不要 */
	} else {
		/* その他の状態の場合、一旦Wifi OffしてRTW_MODE_NONEで起動しなおす */
		wifi_off();
//		vTaskDelay(20);
//		wifi_on(RTW_MODE_NONE);
	}
	vTaskDelay(20);

	/* BT ON */
	power_le_scan(1);

	return RF_ERR_SUCCESS;
}

/* ビーコン受信停止要求 */
RF_CTRL_ERR rf_ctrl_stop_beacon(void)
{
	int cnt = 0, ret = 0;
	tUint8 mode = 0;

	if (fBeacon == FALSE) {
		/* フラグOFF (= Beacon受信中でない) の場合は何もせずreturn */
		Log_Info("\r\n[RF Ctrl][beacon_] already stopped\r\n");
		return RF_ERR_DONE;
	}

	/* BT OFF */
	power_le_scan(0);

	/* RSSI検出終了通知 送信 */
	SendMessageToStateManager(MSG_RSSI_END, PARAM_NONE);

	/* Beacon受信処理前の状態に応じて復旧処理 */
	switch (saved_rf_type) {
	case RF_TYPE_WLAN_STA:
		wifi_on(RTW_MODE_STA);
		wifi_get_autoreconnect(&mode);
		if (!mode) {
			Log_Info("\r\n[RF Ctrl] Autoreconnect is not ready. Start connect to AP manually.\r\n");
			/* reconnect用情報の保存関数へのポインタ登録 および 自動再接続ON */
			p_write_reconnect_ptr = wlan_wrtie_reconnect_data_to_flash;
			wifi_set_autoreconnect(1);

			ret = wifi_connect( wifi_sta_setting.ssid,
								wifi_sta_setting.security_type,
								wifi_sta_setting.password,
								strlen(wifi_sta_setting.ssid),
								strlen(wifi_sta_setting.password), 0, NULL);
			if(ret == RTW_SUCCESS){
				LwIP_DHCP(0, DHCP_START);
			} else {
				Log_Error("\r\n[RF Ctrl] wifi_conect() failed\r\n");
				SendMessageToStateManager(MSG_ERR_WIFI_LINK, PARAM_WIFI_ERR_BEACON);
			}
		}
		break;
	case RF_TYPE_BLE:
		/* 手前の power_le_scan(0) が完了するのを待ってから GattServer起動 */
		cnt = 0;
		while (status_le_scan() != SCAN_POWER_STATUS_OFF) {
			vTaskDelay(200 * portTICK_PERIOD_MS);
			if (++cnt > 20) {
				Log_Error("\r\n[RF Ctrl][beacon_] wait BT OFF Timeout\r\n");
				break;
			}
		}
#if CONFIG_AUDREY_ALWAYS_BT_ON
		/* Advertising有効化(念のため) */
		rf_ctrl_adv_enable();
		/* GattServer再開 */
		if (rf_ctrl_ble_resume_server() == RF_ERR_SUCCESS) {
#else
		/* GattServer起動 */
		if (rf_ctrl_ble_start_server() == RF_ERR_SUCCESS) {
#endif //#if CONFIG_AUDREY_ALWAYS_BT_ON
			/* GattServerの起動ができた時点で内部状態を遷移 */
			rf_state = RF_TYPE_BLE;
			Log_Info("[RF Ctrl] rf_state %d\r\n", rf_state);
		} else {
			Log_Error("[RF Ctrl][beacon_] rf_ctrl_ble_start_server failed\r\n");
			/* BT切断をStateManagerへ通知 */
			SendMessageToStateManager(MSG_BT_LINK_OFF, PARAM_NONE);
			return RF_ERR_SYSTEM;
		}
		break;
	default:
		/* 通常は上のどちらかなので、FatalErrorを通知して再起動させる */
		SendMessageToStateManager(MSG_ERR_SCAN_FATAL, PARAM_COMMON_STATE);
		break;
	}

	/* 抑止用のフラグをクリア */
	fBeacon = FALSE;

	return RF_ERR_SUCCESS;
}

void rf_ctrl_get_bd_addr(tUint8 *br, tUint8 *ble)
{
	if (br != NULL) {
		memcpy(br, br_bd, sizeof(br_bd));
	}
	if (ble != NULL) {
		memcpy(ble, ble_bd, sizeof(ble_bd));
	}

	return;
}

static void rf_ctrl_bt_done_cb(tBT_ResultEnum result, void *param)
{
	u8	mac[6] = {0};					/* MACアドレス */

	if (result == BT_RES_OK) {
		Log_Info("\r\n[RF Ctrl][bt_done_cb] BT deinitialized\r\n");
	}
	else {
		Log_Error("\r\n[RF Ctrl][bt_done_cb] Error, error code (tBT_ResultEnum): %x\r\n",result);
		SendMessageToStateManager(MSG_ERR_BT, PARAM_COMMON_NETDOWN);
	}

	/* Wifi APモード用設定値 生成 */
	memset( &wifi_ap_setting, 0, sizeof(wifi_ap_setting) );
	sprintf( wifi_ap_setting.ssid, "%s-%02X%02X%02X",AUDREY_MODEL_SCALE , audrey_mac[3], audrey_mac[4], audrey_mac[5] );
	strcpy( wifi_ap_setting.password, AP_PASSWORD );
	wifi_ap_setting.mode = RTW_MODE_AP;
	wifi_ap_setting.channel = 1;
	wifi_ap_setting.security_type = RTW_SECURITY_WPA2_AES_PSK;

	/* send message  to tateManager */
	SendMessageToStateManager(MSG_WIFI_INIT_COMP, PARAM_NONE);
	Log_Info("\r\n[RF Ctrl] wlan intialization complete\r\n");

	return;
}

static void rf_ctrl_bt_init_cb(tBT_ResultEnum result, void *param)
{
	char str_br_bd[BD_ADDR_LEN * 2 + 1], str_ble_bd[BD_ADDR_LEN * 2 + 1];

	if (result == BT_RES_OK) {
		Log_Info("\r\n[RF Ctrl][bt_init_cb] BT initialized\r\n");
		memset( br_bd, 0, sizeof(br_bd) );
		memset( ble_bd, 0, sizeof(ble_bd) );
		BT_GAP_LocalBDGet(br_bd, ble_bd);
		bdaddr_to_str(str_br_bd, br_bd);
		bdaddr_to_str(str_ble_bd, ble_bd);
		Log_Info("\r\n[RF Ctrl][bt_init_cb] br_bd=%s ble_bd=%s\r\n", str_br_bd, str_ble_bd);
	}
	else {
		Log_Error("\r\n[RF Ctrl][bt_init_cb] Error, error code (tBT_ResultEnum): %x\r\n",result);
		SendMessageToStateManager(MSG_ERR_BT, PARAM_COMMON_NETDOWN);
	}

	BT_Done(rf_ctrl_bt_done_cb, 1);
	return;
}

int config_mode;
struct wlan_fast_reconnect config_info;

/* 初期設定情報解析処理 */
RF_CTRL_ERR rf_ctrl_parse_config_info(char *data)
{
	cJSON_Hooks	memoryHook;
	cJSON		*IOTJSObject, *dataJSObject;
	char *iot_json;
	RF_CTRL_ERR result = RF_ERR_SUCCESS;
	cJSON		*ssidJSObject = NULL;
	u8 cStrSsid[SSID_LEN +1];

	memoryHook.malloc_fn = malloc;
	memoryHook.free_fn = free;
	cJSON_InitHooks(&memoryHook);

	Log_Info("[RF Ctrl] BLE config data\r\n%s\r\n",data);
	config_mode = 0;
	if((IOTJSObject = cJSON_Parse(data)) != NULL) {
		dataJSObject = cJSON_GetObjectItem(IOTJSObject, "type");
		if(!strcmp(dataJSObject->valuestring, "ap_list")) {
			// SSID一覧
			config_mode = 2;
		} else if(!strcmp(dataJSObject->valuestring, "setMode")) {
			// 接続種別設定
			config_mode = 1;
			dataJSObject = cJSON_GetObjectItem(IOTJSObject, "1");
			switch( dataJSObject->valueint ){
				case 1:  // WPS
					config_info.type = FAST_CONNECT_NON;
					break;
				case 2:  // wifi
					config_info.type = FAST_CONNECT_WLAN;
					break;
				case 3:  // BLE
					config_info.type = FAST_CONNECT_BLE;
					break;
				default:
					config_mode = 0;
					Log_Error( "\r\n[RF Ctrl] Invalid Connect type %d\n", dataJSObject->valueint);
					break;
			}
			if(config_mode == 1 && config_info.type == FAST_CONNECT_WLAN) {
				dataJSObject = cJSON_GetObjectItem(IOTJSObject, "2");
				strcpy(config_info.psk_essid, dataJSObject->valuestring);
				dataJSObject = cJSON_GetObjectItem(IOTJSObject, "3");
				switch( dataJSObject->valueint ){
					case 1:  // なし
						config_info.security_type = RTW_SECURITY_OPEN;
						break;
					case 2:  // WAP/WPA2
						config_info.security_type = RTW_SECURITY_WPA_WPA2_MIXED;
						break;
					case 3:  // WEP
						config_info.security_type = RTW_SECURITY_WEP_PSK;
						break;
					default:
						config_mode = 0;
						Log_Error( "\r\n[RF Ctrl] Invalid Security type %d\n", dataJSObject->valueint);
						break;
				}
				if(config_mode == 1 && config_info.security_type != RTW_SECURITY_OPEN) {
					dataJSObject = cJSON_GetObjectItem(IOTJSObject, "5");
					strcpy(config_info.psk_passphrase, dataJSObject->valuestring);
				}
			}
		} else {
			config_mode = 0;
			Log_Error("\r\n[RF Ctrl] Incorrect type\r\n");
		}
	} else {
		config_mode = 0;
		Log_Error("\r\n[RF Ctrl] JSON parsing failed\r\n");
	}

	cJSON_Delete(IOTJSObject);

	switch(config_mode) {
		case 1:
			memoryHook.malloc_fn = malloc;
			memoryHook.free_fn = free;
			cJSON_InitHooks(&memoryHook);

			/* JSONデータ編集 */
			if((IOTJSObject = cJSON_CreateObject()) != NULL) {

				// type
				cJSON_AddItemToObject( IOTJSObject, "type", cJSON_CreateString("setMode") );
				// result
				cJSON_AddItemToObject( IOTJSObject, "1", cJSON_CreateNumber(0) );

				// MAC address
				char    cStrMACAdr[13];
				sprintf( cStrMACAdr, "%02x%02x%02x%02x%02x%02x"
				, audrey_mac[0], audrey_mac[1], audrey_mac[2], audrey_mac[3], audrey_mac[4], audrey_mac[5] );
				cJSON_AddItemToObject( IOTJSObject, "2", cJSON_CreateString(cStrMACAdr) );

				// get BLE BD address
				tUint8 br_bd[6];
				tUint8 ble_bd[6];

				memset( &br_bd, 0, sizeof(br_bd) );
				memset( &ble_bd, 0, sizeof(ble_bd) );
				rf_ctrl_get_bd_addr( br_bd, ble_bd );

				// BD address
				char    cStrBDAdr[13];
				sprintf( cStrBDAdr, "%02x%02x%02x%02x%02x%02x", ble_bd[5], ble_bd[4], ble_bd[3], ble_bd[2], ble_bd[1], ble_bd[0] );
				cJSON_AddItemToObject( IOTJSObject, "3", cJSON_CreateString(cStrBDAdr) );

				// farmware version Wireless & Scale
				char    cStrVersion[32];
				memset( cStrVersion, 0, sizeof(cStrVersion) );
				sprintf( cStrVersion, "%s_%s", AUDREY_VERSION, scale_ver );
				cJSON_AddItemToObject( IOTJSObject, "4", cJSON_CreateString(cStrVersion) );

				iot_json = cJSON_Print( IOTJSObject );
				Log_Info("\r\n[RF Ctrl] ===== JSON data =====\r\n%s\r\n===============================\r\n", iot_json);
			}
			cJSON_Delete(IOTJSObject);

			// 応答データ送信
			result = rf_ctrl_conf_data_send(iot_json);
			break;

		case 2:
			memoryHook.malloc_fn = malloc;
			memoryHook.free_fn = free;
			cJSON_InitHooks(&memoryHook);

			/* JSONデータ編集 */
			if((IOTJSObject = cJSON_CreateObject()) != NULL) {
				// type
				cJSON_AddItemToObject( IOTJSObject, "type", cJSON_CreateString("ap_list") );
				// result
				cJSON_AddItemToObject( IOTJSObject, "1", cJSON_CreateNumber(0) );
				// SSID
				if(webserver_ap_num) {
					if ((ssidJSObject = cJSON_CreateArray()) != NULL) {
						cJSON_AddItemToObject( IOTJSObject, "5", ssidJSObject );
						for(int i = 0; i < webserver_ap_num; i++) {
							sprintf( cStrSsid, "%s", webserver_ap_list[i] );
							cJSON_AddItemToArray(ssidJSObject, cJSON_CreateString(cStrSsid));
						}
					}
				}
				iot_json = cJSON_Print( IOTJSObject );
				Log_Info("\r\n[RF Ctrl] ===== JSON data =====\r\n%s\r\n===============================\r\n", iot_json);
			}
			cJSON_Delete(IOTJSObject);

			// 応答データ送信
			result = rf_ctrl_conf_data_send(iot_json);
			break;

		default:
			memoryHook.malloc_fn = malloc;
			memoryHook.free_fn = free;
			cJSON_InitHooks(&memoryHook);
			// result
			cJSON_AddItemToObject( IOTJSObject, "1", cJSON_CreateNumber(1) );
			cJSON_Delete(IOTJSObject);
			SendMessageToStateManager(MSG_ERR_DUL, PARAM_DUL_ERR_RSP);
			return RF_ERR_PARAM;
	}
	return result;
}


extern int wlan_wrtie_reconnect_data_to_flash( u8 *data, uint32_t len );

/* 初期設定応答送信完了処理 */
void rf_ctrl_config_resp_comp(void)
{
	Log_Info("[RF Ctrl] Responce complete rf_state:%d\r\n", rf_state);
	if(config_mode == 1) {
		switch( config_info.type )
		{
			case    FAST_CONNECT_WLAN:  // Manual settings
				// Write to flash
				wlan_wrtie_reconnect_data_to_flash( (u8*)&config_info, (uint32_t)sizeof(config_info) );
				// Notice to the StateManager
				SendMessageToStateManager( MSG_WEBUI_SET_SSID, PARAM_NONE );
				break;
			case    FAST_CONNECT_BLE:   // BLE mode
				 // Write to flash
				wlan_wrtie_reconnect_data_to_flash( (u8*)&config_info, (uint32_t)sizeof(config_info) );
				// Notice to the StateManager
				SendMessageToStateManager( MSG_WEBUI_SET_BLE_MODE, PARAM_NONE );
				break;
			case    FAST_CONNECT_NON:   // WPS connect mode
				// Notice to the StateManager
				SendMessageToStateManager( MSG_WEBUI_WPS_START, PARAM_NONE );
				break;
			default:                    // error
				Log_Error( "\r\nWEB:Invalid fast_reconnect.type\n" );
				break;
		}
	}
}

const u8 wrong_mac[] = {0x00,0xe0,0x4c};

void rf_ctrl_wifi_mac(void)
{
	/*** wlan intialization ***/
	/* Initilaize the LwIP stack */
	LwIP_Init();
	/* Enable Wifi */
	wifi_on(RTW_MODE_NONE);
	vTaskDelay(20);
	memcpy(audrey_mac, LwIP_GetMAC(&xnetif[0]), 6);

	memcpy(AUDREY_API_KEY+10, AUDREY_API_KEY_2, 10);
	memcpy(AUDREY_API_KEY+30, AUDREY_API_KEY_4, 10);
	memcpy(AUDREY_API_KEY+20, AUDREY_API_KEY_3, 10);
	memcpy(AUDREY_API_KEY, AUDREY_API_KEY_1, 10);

	/* 上位00:e0:4cは不正MACアドレスとしてシステム再起動 */
	if(memcmp(audrey_mac, wrong_mac, 3) == 0) {
		led_control(LED_TYPE_BLINK_MID);
		Log_Error("[RF Ctrl] wrong MAC address : %02x:%02x:%02x:%02x:%02x:%02x\r\n"
		, audrey_mac[0], audrey_mac[1], audrey_mac[2], audrey_mac[3], audrey_mac[4], audrey_mac[5]);
		vTaskDelay(3000);
		sys_reset();
	}
}

static void rf_ctrl_thread(void *param)
{
	int	ret = 0;

	rtw_msleep_os(20);

	/* Bluetooth初期化 ： BD_ADDR取得用 */
	BT_Init(rf_ctrl_bt_init_cb);

	while (1) {
		if ((rf_state == RF_TYPE_NONE) || (rf_state == RF_TYPE_WLAN_AP) || (rf_state == RF_TYPE_BLE_CONF)) {
			/* WPSを実行する可能性がある場合のみ短めのdelayにする */
			vTaskDelay(200 * portTICK_PERIOD_MS);
		} else {
			vTaskDelay(1000 * portTICK_PERIOD_MS);
		}

		if (fWpsStart == TRUE) {
			fWpsStart = FALSE;
			/* WPS処理開始 */
			ret = wps_start(WPS_CONFIG_PUSHBUTTON, NULL, 0, NULL);
			if (ret != 0) {
				Log_Error("[RF Ctrl] wps failed : %d\r\n", ret);
				rf_state = RF_TYPE_NONE;
				Log_Info("[RF Ctrl] rf_state %d\r\n", rf_state);
				SendMessageToStateManager(MSG_WIFI_LINK_OFF, PARAM_NONE);
			} else {
				Log_Notify("[RF Ctrl] wps successed : %d\r\n", ret);
			}
		} else if (fDhcpRetry == TRUE) {
			fDhcpRetry = FALSE;
			/* DHCP開始 */
			ret = wifi_is_connected_to_ap();
			if(ret == RTW_SUCCESS){
				LwIP_DHCP(0, DHCP_START);
			} else {
				Log_Error("\r\n[RF Ctrl] DHCP process failed & disconnected => Abandon\r\n");
				SendMessageToStateManager(MSG_WIFI_DHCP_FAIL, PARAM_NONE);
			}
		}
	}

	vTaskDelete(NULL);
}

void rf_ctrl_init(void)
{
	if(xTaskCreate(rf_ctrl_thread, ((const char*)"rf_ctrl_thread"), 1024, NULL, tskIDLE_PRIORITY + 2 , NULL) != pdPASS) {
		Log_Error("\n\r%s xTaskCreate(rf_ctrl_thread) failed", __FUNCTION__);
		SendMessageToStateManager(MSG_ERR_REBOOT, PARAM_COMMON_TASK_CREATE_FAIL);
	}
}
