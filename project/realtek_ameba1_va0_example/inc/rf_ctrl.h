#ifndef __RF_CTRL_H__
#define __RF_CTRL_H__

/*
 * Copyright (C) 2017 SHARP Corporation. All rights reserved.
 */
#include "FreeRTOS.h"
#include "semphr.h"
#include "osdep_api.h"
#include <wifi_structures.h>
#include <bt_gap.h>
#include "data_upload.h"
#include "bt_gatt_server.h"
#include "bt_gatt.h"

#define RF_CTRL_WIFI_USE_SSL								/* HTTPSで通信する場合に定義 */

/* 無線通信種別 */
typedef enum rf_type {
	RF_TYPE_NONE = 0,			/* 未接続 */
	RF_TYPE_WLAN_AP,			/* WLAN AP モード */
	RF_TYPE_WLAN_WPS,			/* WLAN WPS処理中 */
	RF_TYPE_WLAN_STA,			/* WLAN STAモード */
	RF_TYPE_BLE,				/* BLE */
	RF_TYPE_BLE_CONF,			/* BLE 初期設定中 */
} RF_TYPE;

/** エラーコード **/
typedef enum rf_ctrl_err {
	RF_ERR_SUCCESS,		/* 正常 */
	RF_ERR_DONE,		/* 実施済(要求された状態にすでになっている場合) */
	RF_ERR_STATE,		/* ステータスエラー */
	RF_ERR_NETWORK,		/* ネットワークエラー (応答が正常に受信できない等) */
	RF_ERR_PARAM,		/* 各種パラメータ不正 */
	RF_ERR_SYSTEM,		/* システムエラー (SDKのAPIがerror return 等) */
	RF_ERR_DHCP_FAIL,	/* DHCP失敗 */
	RF_ERR_RETRY,		/* 要リトライ */
} RF_CTRL_ERR;

/** MACアドレス **/
extern u8	audrey_mac[6];

/** 外部公開API **/
void rf_ctrl_init(void);									/* 初期化・タスク起動処理 */
RF_CTRL_ERR rf_ctrl_wifi_connect(rtw_wifi_config_t *conf);	/* Wifi接続要求 */
RF_CTRL_ERR rf_ctrl_ble_connect(void);						/* BT接続要求 */
RF_CTRL_ERR rf_ctrl_ble_config(void);						/* BT接続要求(初期設定用) */
#if CONFIG_AUDREY_ALWAYS_BT_ON
RF_CTRL_ERR rf_ctrl_ble_init(void);							/* BT起動(Advertising動作なし) */
#endif //#if CONFIG_AUDREY_ALWAYS_BT_ON
RF_CTRL_ERR rf_ctrl_ble_data_upload_enable_cb(void);		/* BT経由データアップロード有効化通知 */
RF_CTRL_ERR rf_ctrl_start_ap(void);							/* Wifi AP 起動要求 */
RF_CTRL_ERR rf_ctrl_start_wps(void);						/* Wifi WPS開始要求 */
RF_CTRL_ERR rf_ctrl_stop_wps(void);							/* Wifi WPS停止要求 */
RF_CTRL_ERR rf_ctrl_wifi_disconnect(void);					/* Wifi切断要求 */
RF_CTRL_ERR rf_ctrl_ble_disconnect(void);					/* BT切断要求 */
RF_CTRL_ERR rf_ctrl_check_connection(void);					/* データ送信可否チェック */
RF_CTRL_ERR rf_ctrl_data_upload(upload_data *data);			/* 測定データ送信 */
RF_CTRL_ERR rf_ctrl_parse_upload_resp(int id, char *data);	/* 応答データ解析処理 */
RF_CTRL_ERR rf_ctrl_fw_update_start(void);					/* FW Update開始要求 */
RF_CTRL_ERR rf_ctrl_fw_update_finish(void);					/* FW Update完了通知 */
RF_CTRL_ERR rf_ctrl_start_beacon(void);						/* ビーコン受信開始要求 */
RF_CTRL_ERR rf_ctrl_stop_beacon(void);						/* ビーコン受信停止要求 */
bool rf_ctrl_wait_http_resp(void);							/* HTTP通信 応答待ち状態チェック */
bool rf_ctrl_force_stop_waiting_resp(void);					/* HTTP通信 応答待ち強制停止 */
//RF_CTRL_ERR rf_ctrl_start_adv(void);						/* Wifi APモード用 Advertising開始要求 */
//RF_CTRL_ERR rf_ctrl_stop_adv(void);							/* Wifi APモード用 Advertising停止要求 */

#if CONFIG_AUDREY_DBG_UPLOAD
RF_CTRL_ERR rf_ctrl_dbg_w_data_upload(dbg_weight_data *data);		/* デバッグデータ(重量)送信処理 */
RF_CTRL_ERR rf_ctrl_dbg_b_data_upload(dbg_beacon_data *data);		/* デバッグデータ(beacon)送信要求 */
RF_CTRL_ERR rf_ctrl_dbg_r_data_upload(dbg_rssi_data *data);			/* デバッグデータ(rssi)送信要求 */
#endif
#if CONFIG_AUDREY_LOG_UPLOAD
RF_CTRL_ERR rf_ctrl_log_e_upload(log_error_data *data);				/* ログ(error)送信要求 */
#endif
void rf_ctrl_wifi_mac(void);										/* MACアドレス取得用wifi ON */

/* 他タスクからのCallbak受信用関数 */
void rf_ctrl_wifi_connected_cb(void);								/* Wifi接続完了通知 */
void rf_ctrl_ble_connected_cb(tBT_GattConnCmplStru *param);			/* BLE接続完了通知 */
void rf_ctrl_wifi_disconnected_cb(RF_CTRL_ERR err);					/* Wifi接続切断通知 */
void rf_ctrl_ble_disconnected_cb(tBT_GattDisconnCmplStru *param);	/* BLE接続切断通知 */
void rf_ctrl_wifi_reconnection_failure_cb(void);					/* Wifi再接続失敗通知 */
void rf_ctrl_get_bd_addr(tUint8 *br, tUint8 *ble);					/* BD ADDR取得関数 */
																	/* BLEペアリング完了通知 */
void rf_ctrl_gap_simple_pairing_complete_cb(tBT_GapSimplePairingCmplStru *param);

RF_CTRL_ERR rf_ctrl_parse_config_info(char *data);					/* BLE初期設定情報解析処理 */
void rf_ctrl_config_resp_comp(void);								/* 初期設定応答送信完了処理 */


#endif //#ifndef __RF_CTRL_H__
