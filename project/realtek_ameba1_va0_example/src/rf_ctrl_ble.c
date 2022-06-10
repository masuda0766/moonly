/*
 * Copyright (C) 2017 SHARP Corporation. All rights reserved.
 */

/**
 * 無線通信制御部 (BLE接続固有処理)
 */
#include <platform_opts.h>
#include <lwip_netconf.h>
#include <platform/platform_stdlib.h>
#include <cJSON.h>
#include "FreeRTOS.h"
#include "task.h"
#include "rf_ctrl.h"
#include "rf_ctrl_ble.h"
#include "state_manager.h"
#include "timers.h"

//#define DISABLE_BLE_SLEEP							/* デバッグ用：BLE利用時のSLEEPモードを抑止 */

/**
 * BLE経由 データ送信処理関連
 */

#if CONFIG_AUDREY_REDUCE_MALLOC
/* データ送信処理関連 */
#define DATA_LENGTH_SIZE			4				/* 送信データの頭につけるデータ長のサイズ (4byte) */
#define MAX_UPLOAD_DATA_BUF_LEN		5120			/* 送信データ用バッファの最大長 */
static volatile int					type_id = 0;	/* 送信データ種別 */
static char					upload_data_buf[MAX_UPLOAD_DATA_BUF_LEN + 1];	/* 送信データ用バッファ */
static tUint32				upload_data_len = 0;	/* 送信データサイズ（データ長部分を含む） */
TimerHandle_t				wait_resp = NULL;		/* 応答受信待ちタイマハンドラ */
#define	BLE_WAIT_RESP_TOUT			90000			/* 応答受信待ちTimeout時間：90秒間 (単位：ms) */
#define BLE_RESP_CHECK_PERIOD_MS	100				/* 応答受信待ち チェック間隔 */
#define BLE_RESP_CHECK_RETRY_MAX	100				/* 応答受信待ち チェック回数上限 */

/* Configurationデータ関連 */
#define CONF_DATA_LEN_SIZE	4						/* configurationデータの頭につけるデータ長のサイズ (4byte) */
#define MAX_CONF_DATA_BUF_LEN		2048			/* configurationデータ用バッファの最大長 */
static char					conf_data_buf[MAX_CONF_DATA_BUF_LEN + 1];	/* configurationデータ用バッファ */
static tUint32				conf_data_len = 0;		/* configurationデータサイズ(データ長分を含む) */
#else //#if CONFIG_AUDREY_REDUCE_MALLOC
/* データ送信処理関連 */
#define DATA_LENGTH_SIZE	4						/* 送信データの頭につけるデータ長のサイズ (4byte) */
static volatile int			type_id = 0;			/* 送信データ種別 */
static char *				upload_data_buf = NULL;	/* 送信データ用バッファ */
static tUint32				upload_data_len = 0;	/* 送信データサイズ（データ長部分を含む） */
TimerHandle_t				wait_resp = NULL;		/* 応答受信待ちタイマハンドラ */
#define	BLE_WAIT_RESP_TOUT			180000			/* 応答受信待ちTimeout時間：3分間 (単位：ms) */
#define BLE_RESP_CHECK_PERIOD_MS	100				/* 応答受信待ち チェック間隔 */
#define BLE_RESP_CHECK_RETRY_MAX	100				/* 応答受信待ち チェック回数上限 */

/* Configurationデータ関連 */
#define CONF_DATA_LEN_SIZE	4
static char *				conf_data_buf = NULL;	/* configurationデータ用バッファ */
static tUint32				conf_data_len = 0;		/* configurationデータサイズ(データ長分を含む) */
#endif //#if CONFIG_AUDREY_REDUCE_MALLOC

/* BLE休眠処理関連 */
typedef enum {
	BLE_SLEEP_STATE_OFF = 0,						/* BLEサービス未起動 */
	BLE_SLEEP_STATE_ACTIVE,							/* BLEサービス動作中 */
	BLE_SLEEP_STATE_WAIT_SLEEP,						/* SLEEP状態移行中 */
	BLE_SLEEP_STATE_SLEEP,							/* SLEEP状態 */
	BLE_SLEEP_STATE_WAKEUP,							/* SLEEP状態解除中 */
	BLE_SLEEP_STATE_WAKEUP_FAILED,					/* SLEEP状態解除失敗 */
} BLE_SLEEP_STATE;
static BLE_SLEEP_STATE		BleSleepState = BLE_SLEEP_STATE_OFF;	/* BLE休眠モード状態 */
TimerHandle_t				ble_sleep = NULL;		/* BLE休眠タイマハンドラ */
#define BLE_SLEEP_DELAY		90000					/* BLE休眠待ち時間：90秒間 (単位：ms) */
static volatile bool		fBleWakeup = FALSE;		/* BLE休眠状態解除フラグ (休眠移行中の復帰要求) */

TimerHandle_t				wait_wakeup = NULL;		/* BLE休眠状態解除待ちタイマハンドラ */
#define BLE_SLEEP_WAKEUP_LIMIT	300000				/* BLE休眠状態解除待機時間：5分間 (単位：ms) */
static tBT_GattDisconnCmplStru	SaveDiscCompParam;	/* 切断完了通知パラメータ（一時保存用） */

/* Advertising停止完了通知 */
static void rf_ctrl_adv_off_cb(tBT_ResultEnum result, void *param)
{
	if (result == BT_RES_OK) {
		Log_Info("\r\n[RF Ctrl][ble_sleep] advertising stopped\r\n");
	}
	else {
		Log_Error("\r\n[Error][RF Ctrl] stop advertising failed. error code(tBT_ResultEnum): %x\r\n", result);
	}

}

/* BLE接続完了通知 */
void rf_ctrl_ble_conn_complete_cb(tBT_GattConnCmplStru *param)
{
	if (BleSleepState == BLE_SLEEP_STATE_WAKEUP) {
		/* 休眠モード解除完了 */
		BleSleepState = BLE_SLEEP_STATE_ACTIVE;
		Log_Info("\r\n[RF Ctrl][ble_sleep] returned to normal mode\r\n");

		/* BLE休眠状態解除待ちタイマ停止 */
		if ((wait_wakeup != NULL) && (xTimerIsTimerActive(wait_wakeup) != pdFALSE)) {
			xTimerStop(wait_wakeup, 0);
		}
	} else {
		/* 通常時は rf_ctrl.c へ通知を転送 */
		rf_ctrl_ble_connected_cb(param);
	}

	/* 切断完了通知パラメータ（一時保存用） をクリア */
	memset(&SaveDiscCompParam, 0, sizeof(SaveDiscCompParam));

	return;
}

/* BLE切断完了通知 */
void rf_ctrl_ble_disconn_complete_cb(tBT_GattDisconnCmplStru *param)
{
	/* 応答受信待ち中の切断通知受信時 */
	if ((wait_resp != NULL) && (xTimerIsTimerActive(wait_resp) != pdFALSE)) {
		/* 応答受信待ちタイマ停止 */
		xTimerStop(wait_resp, 0);

		/* データ送信タスクへ応答受信失敗を通知 */
		rf_ctrl_parse_upload_resp(type_id, NULL);

#if CONFIG_AUDREY_REDUCE_MALLOC != 1
		/* 送信データ用バッファ開放 */
		free(upload_data_buf);
		upload_data_buf = NULL;
#endif //#if CONFIG_AUDREY_REDUCE_MALLOC != 1
	}

	/* BLE休眠タイマ動作中の場合 */
	if ((xTimerIsTimerActive(ble_sleep) != pdFALSE)) {
		/* Advertising停止 */
		BT_GAP_BleAdvertisingSet(FALSE, rf_ctrl_adv_off_cb);
		Log_Info("\r\n[RF Ctrl][ble_sleep] stop advertising\r\n");
		/* BLE休眠タイマ停止 */
		xTimerStop(ble_sleep, 0);
		/*  BLE休眠モード状態をBLE_SLEEP_STATE_WAIT_SLEEPに遷移 */
		BleSleepState = BLE_SLEEP_STATE_WAIT_SLEEP;
		Log_Info("\r\n[RF Ctrl][ble_sleep] sleep mode start\r\n");
	}

	if (BleSleepState == BLE_SLEEP_STATE_WAIT_SLEEP) {
		/* 通知のパラメータを保存(ble_wait_wakeup_timeout_handler()で使用) */
		memcpy(&SaveDiscCompParam, param, sizeof(SaveDiscCompParam));
		if (fBleWakeup == TRUE) {
			/* 休眠モード移行中に復帰要求が発生した場合はすぐに Advertise送信を開始 */
			fBleWakeup = FALSE;
			BleSleepState = BLE_SLEEP_STATE_WAKEUP;
			svr_set_adv_data();
			/* BLE休眠状態解除待ちタイマスタート */
			if (wait_wakeup != NULL) {
				Log_Debug("\r\n[RF Ctrl][ble_sleep] wakeup timer start\r\n");
				xTimerReset(wait_wakeup, 0);
			}
		} else {
			BleSleepState = BLE_SLEEP_STATE_SLEEP;
			/* 休眠モードに入る際の切断通知のため rf_ctrl.c への切断通知は上げない */
			Log_Info("\r\n[RF Ctrl][ble_sleep] BLE connection disconnected\r\n");
		}
	} else {
		/* 通常時は rf_ctrl.c へ通知を転送 */
		rf_ctrl_ble_disconnected_cb(param);
	}

	return;
}

/* BLE休眠タイマタイムアウト処理 */
void ble_sleep_timeout_handler(xTimerHandle pxTimer)
{
	/* BLE_SLEEP_STATE_ACTIVE 以外の場合は状態異常やイベントのすれ違いと見做して何も処理しない */
	if (BleSleepState == BLE_SLEEP_STATE_ACTIVE) {
		/* 休眠モード移行開始 */
		BleSleepState = BLE_SLEEP_STATE_WAIT_SLEEP;
		Log_Info("\r\n[RF Ctrl][ble_sleep] sleep mode start\r\n");

		/* 接続を切断 */
		gatt_conn_disc(FALSE);
	}

	return;
}

/* BLE応答受信待ちタイマタイムアウト処理 */
void ble_wait_resp_timeout_handler(xTimerHandle pxTimer)
{
	tBle_ServerIoStatusEnum		bt_io_state = SERVER_IO_STATUS_OFF;

	Log_Error("\r\n[RF Ctrl][upload_ble] resp timeout\r\n");

	/* データ送信タスクへ応答受信失敗を通知 */
	rf_ctrl_parse_upload_resp(type_id, NULL);

	bt_io_state = status_bt_gatt_server();
	if ((bt_io_state != SERVER_IO_STATUS_DATA_UPLOAD) &&
		(bt_io_state != SERVER_IO_STATUS_DATA_UPLOAD_COMPLETE)) {
		/* データの送信処理が完了しない(indication_cb()が呼ばれていない)ままでタイムアウトした場合、
		/* Gatt Server再起動を繰り返すため処理削除し休眠モードへ移行 */
		/* (次回送信要求orリトライ要求まで送信処理を行わない) */
		if (BleSleepState == BLE_SLEEP_STATE_ACTIVE) {
			/* 休眠モード移行開始 */
			BleSleepState = BLE_SLEEP_STATE_WAIT_SLEEP;
			Log_Info("\r\n[RF Ctrl][upload_ble] sleep mode start\r\n");
			/* 接続を切断 */
			gatt_conn_disc(FALSE);
		}
	} else {
		/* 接続を切断して再接続を待つ */
		gatt_conn_disc(TRUE);
	}

	/* アラート通知送信 */
	SendMessageToStateManager(MSG_ERR_DUL, PARAM_DUL_ERR_RESP_TOUT);

	return;
}

/* BLE休眠状態解除待ちタイマ タイムアウト処理 */
void ble_wait_wakeup_timeout_handler(xTimerHandle pxTimer)
{
	Log_Info("\r\n[RF Ctrl][ble_sleep] wakeup timeout\r\n");

	/* Advertiseを送信して接続を待っているが接続されない状態 */
	/* → rf_ctrl.c へBLE切断を通知して全体の状態認識を一致させる */

	/* 休眠モード解除 */
	BleSleepState = BLE_SLEEP_STATE_ACTIVE;

	/* 保存しておいたパラメータを使って切断を通知 */
	rf_ctrl_ble_disconnected_cb(&SaveDiscCompParam);

	return;
}

/* BLEデータ送信応答受信待ち 待機関数 */
bool rf_ctrl_wait_ble_resp(void)
{
	int cnt = 0;

	/* タイマ生成前なら受信待ち中ではないので TRUE を返す */
	if (wait_resp == NULL) {
		return TRUE;
	}

	/* タイマがActive状態の間は応答受信待ち中なので待機 */
	while (xTimerIsTimerActive(wait_resp) != pdFALSE) {
		/* 応答受信待ちタイマが設定されている場合は一定時間待機 */
		vTaskDelay(BLE_RESP_CHECK_PERIOD_MS * portTICK_PERIOD_MS);
		if (++cnt > BLE_RESP_CHECK_RETRY_MAX) {
			/* 一定回数繰り返してもタイマが開放されなければFALSE（＝送信不可）を返す */
			Log_Error("\r\n[RF Ctrl] BLE connection busy\r\n");
			return FALSE;
		}
	}

	return TRUE;
}

/* Indication送信可否チェック */
bool rf_ctrl_data_transport_char_desc_check(void)
{
	/* サービス未起動時は送信不可を返す */
	if (BleSleepState == BLE_SLEEP_STATE_OFF) {
		Log_Info("\r\n[RF Ctrl] Gatt Server is not in service.\r\n");
		return FALSE;
	}

	/* 休眠モード中の場合、送信できるかチェック＝送信しようとしている と判断して、復帰処理を要求 */
	if (BleSleepState == BLE_SLEEP_STATE_WAIT_SLEEP) {
		/* 休眠モード移行中の場合は移行後即復帰させるためのフラグを設定 */
		fBleWakeup = TRUE;	/* BLE休眠状態解除フラグ をON */
	} else if (BleSleepState == BLE_SLEEP_STATE_SLEEP) {
		/* 二重要求防止のためAdvertising実行中かどうかをチェック */
		if (BT_GAP_BleAdvertisingGet() == FALSE) {
			BleSleepState = BLE_SLEEP_STATE_WAKEUP;
			svr_set_adv_data();
			/* BLE休眠状態解除待ちタイマスタート */
			if (wait_wakeup != NULL) {
				Log_Debug("\r\n[RF Ctrl][ble_sleep] wakeup timer start\r\n");
				xTimerReset(wait_wakeup, 0);
			}
		}

		return FALSE;
	}

	return is_enabele_data_transport_desc();
}

/* BLE経由データ送信完了通知 */
void rf_ctrl_data_upload_ble_cb(tBT_GattResEnum result, char *resp)
{
	Log_Info("\r\n[RF Ctrl][upload_ble] result = 0x%02x\r\n", result);

	/* 応答受信待ちタイマ停止 */
	if ((wait_resp != NULL) && (xTimerIsTimerActive(wait_resp) != pdFALSE)) {
		xTimerStop(wait_resp, 0);
	}

#if CONFIG_AUDREY_REDUCE_MALLOC != 1
	/* 送信データ用バッファ開放 */
	free(upload_data_buf);
	upload_data_buf = NULL;
#endif //#if CONFIG_AUDREY_REDUCE_MALLOC != 1

	if (result == BT_GATT_OK) {
		Log_Info("\r\n[RF Ctrl][upload_ble] resp ===\r\n%s\r\n==============================\r\n", resp);
		rf_ctrl_parse_upload_resp(type_id, resp);
	} else {
		Log_Error("\r\n[RF Ctrl][upload_ble] resp error\r\n");
		rf_ctrl_parse_upload_resp(type_id, NULL);
	}

	/* 応答受信用バッファ開放 */
	free(resp);

	/* BLE休眠タイマスタート */
	if (ble_sleep != NULL) {
		Log_Debug("\r\n[RF Ctrl][upload_ble] sleep timer start\r\n");
		xTimerReset(ble_sleep, 0);
	}

	return;
}

/* BLE経由データ送信要求 */
RF_CTRL_ERR rf_ctrl_data_upload_ble(int id, char *data)
{
	tUint32	len = 0;	/* 送信データ長 */
	Log_Info("\r\n[RF_Ctrl][upload_ble] data = %s\r\n", data);

	/* 送信データ種別を記憶 (応答受信時に利用) */
	type_id = id;

	switch (BleSleepState) {
		case BLE_SLEEP_STATE_SLEEP:
			/* 休眠モードから復旧させる */
			BleSleepState = BLE_SLEEP_STATE_WAKEUP;
			svr_set_adv_data();
			/* BLE休眠状態解除待ちタイマスタート */
			if (wait_wakeup != NULL) {
				Log_Debug("\r\n[RF Ctrl][ble_sleep] wakeup timer start\r\n");
				xTimerReset(wait_wakeup, 0);
			}
			/* データ送信タスクへ応答受信失敗を通知 */
			rf_ctrl_parse_upload_resp(type_id, NULL);
			break;

		case BLE_SLEEP_STATE_ACTIVE:
			/* BLE休眠タイマ停止 */
			if ((ble_sleep != NULL) && (xTimerIsTimerActive(ble_sleep) != pdFALSE)) {
				Log_Debug("\r\n[RF Ctrl][upload_ble] sleep timer start\r\n");
				xTimerStop(ble_sleep, 0);
			}
			/* 送信データ編集 */
			len = strlen(data);
#if CONFIG_AUDREY_REDUCE_MALLOC
			if (len > (MAX_UPLOAD_DATA_BUF_LEN + DATA_LENGTH_SIZE)) {
				Log_Error("\r\n[RF Ctrl] data length %d byte is too longd\r\n", __FUNCTION__, len);
				SendMessageToStateManager(MSG_ERR_DUL_FATAL, PARAM_COMMON_NOMEM);
				return RF_ERR_SYSTEM;
			}
			memset(upload_data_buf, 0, len + DATA_LENGTH_SIZE);
			upload_data_buf[0] = (len & 0xFF000000) >> 24;
			upload_data_buf[1] = (len & 0x00FF0000) >> 16;
			upload_data_buf[2] = (len & 0x0000FF00) >> 8;
			upload_data_buf[3] = (len & 0x000000FF);
			memcpy(&upload_data_buf[DATA_LENGTH_SIZE], data, len);
#else //
			upload_data_buf = malloc(len + DATA_LENGTH_SIZE);
			if (upload_data_buf == NULL) {
				Log_Error("\r\n[RF Ctrl] %s : malloc failed\r\n", __FUNCTION__);
				SendMessageToStateManager(MSG_ERR_DUL_FATAL, PARAM_COMMON_NOMEM);
				return RF_ERR_SYSTEM;
			}
			memset(upload_data_buf, 0, len + DATA_LENGTH_SIZE);
			*upload_data_buf     = (len & 0xFF000000) >> 24;
			*(upload_data_buf+1) = (len & 0x00FF0000) >> 16;
			*(upload_data_buf+2) = (len & 0x0000FF00) >> 8;
			*(upload_data_buf+3) = (len & 0x000000FF);
			strcpy((upload_data_buf + DATA_LENGTH_SIZE), data);
#endif //#if CONFIG_AUDREY_REDUCE_MALLOC
			/* 送信データサイズを記憶 */
			upload_data_len = len + DATA_LENGTH_SIZE;
			/* 送信データにEOFを追加 */
			upload_data_buf[upload_data_len] = '\0';

			data_up_req_start();

			/* Indication 送信要求 */
			bt_send_data_transport_char(upload_data_buf, upload_data_len);
			/* 応答受信待ちタイマスタート */
			if (wait_resp != NULL) {
				Log_Debug("\r\n[RF Ctrl][upload_ble] wait resp timer start\r\n");
				xTimerReset(wait_resp, 0);
			}
			break;

		default:
			Log_Debug("\r\n[RF Ctrl][upload_ble] BleSleepState = %d\r\n", BleSleepState);
			/* データ送信タスクへ応答受信失敗を通知 */
			rf_ctrl_parse_upload_resp(type_id, NULL);
			break;
	}

	return RF_ERR_SUCCESS;
}

/* BT経由データアップロード有効化通知 */
void rf_ctrl_data_upload_ble_enable_cb(void)
{
	rf_ctrl_ble_data_upload_enable_cb();
}

/* Configurationデータ受信通知 */
void rf_ctrl_conf_data_recv_cb(char *data)
{
	RF_CTRL_ERR result;

	Log_Info("\r\n[RF Ctrl][ble] receive configuration data\r\n");

	result = rf_ctrl_parse_config_info(data);
	Log_Info("\r\n[RF Ctrl][ble] parse data result: %d\r\n", result);
	
#if CONFIG_AUDREY_REDUCE_MALLOC != 1
	/* 受信用バッファ解放 */
	if (data)
		free(data);
#endif //#if CONFIG_AUDREY_REDUCE_MALLOC != 1
}

/* Configurationデータ送信 */
RF_CTRL_ERR rf_ctrl_conf_data_send(char *data)
{
	tUint32 len = 0;

	Log_Info("\r\n[RF Ctrl][ble] send configuration data\r\n");
	/* 送信データの編集 */
	len = strlen(data);
#if CONFIG_AUDREY_REDUCE_MALLOC
		if (len > (MAX_CONF_DATA_BUF_LEN + CONF_DATA_LEN_SIZE)) {
		Log_Error("\r\n[RF Ctrl] data length %d byte is too longd\r\n", __FUNCTION__, len);
		SendMessageToStateManager(MSG_ERR_DUL_FATAL, PARAM_COMMON_NOMEM);
		return RF_ERR_SYSTEM;
	}
	conf_data_len = len + CONF_DATA_LEN_SIZE;
	memset(conf_data_buf, 0, conf_data_len);
	conf_data_buf[0] = (len & 0xFF000000) >> 24;
	conf_data_buf[1] = (len & 0x00FF0000) >> 16;
	conf_data_buf[2] = (len & 0x0000FF00) >> 8;
	conf_data_buf[3] = (len & 0x000000FF);
	memcpy(&conf_data_buf[CONF_DATA_LEN_SIZE], data, len);
#else //#if CONFIG_AUDREY_REDUCE_MALLOC
	conf_data_buf = malloc(len + CONF_DATA_LEN_SIZE);
	if (!conf_data_buf) {
		Log_Error("\r\n [ERROR] %s:%d memory alloc error\r\n", __FUNCTION__, __LINE__);
		SendMessageToStateManager(MSG_ERR_DUL_FATAL, PARAM_COMMON_NOMEM);
		return RF_ERR_SYSTEM;
	}
	memset(conf_data_buf, 0, len + CONF_DATA_LEN_SIZE);
	*conf_data_buf     = (len & 0xFF000000) >> 24;
	*(conf_data_buf+1) = (len & 0x00FF0000) >> 16;
	*(conf_data_buf+2) = (len & 0x0000FF00) >> 8;
	*(conf_data_buf+3) = (len & 0x000000FF);
	strcpy((conf_data_buf + CONF_DATA_LEN_SIZE), data);
	conf_data_len = len + CONF_DATA_LEN_SIZE;
	conf_data_buf[conf_data_len] = '\0';
#endif //#if CONFIG_AUDREY_REDUCE_MALLOC

	if (is_enabele_configuration_desc()) {
		Log_Info("\r\n[RF Ctrl][ble] send configuration data %d byte(s)\r\n", conf_data_len);
		/* Indication 送信要求 */
		bt_send_configuration_char(conf_data_buf, conf_data_len);
	}
	else {
		Log_Error("\r\n [RF Ctrl] %s:%d sending configuration data is not ready\r\n", __FUNCTION__, __LINE__);
	}

	return RF_ERR_SUCCESS;
}

/* Configurationデータ送信完了通知 */
void rf_ctrl_conf_data_send_comp_cb(void)
{
	Log_Info("\r\n[RF Ctrl][ble] send configuration data complete\r\n");
	rf_ctrl_config_resp_comp();
#if CONFIG_AUDREY_REDUCE_MALLOC != 1
	/* 送信用バッファ解放 */
	if(conf_data_buf) {
		free(conf_data_buf);
		conf_data_buf = NULL;
	}
#endif //#if CONFIG_AUDREY_REDUCE_MALLOC != 1
}

RF_CTRL_ERR rf_ctrl_ble_start_server(void)
{
	/* Gatt Server起動 */
	if (start_bt_gatt_server(NULL, NULL) != 0) {
		return RF_ERR_SYSTEM;
	}
	Log_Info("\r\n[RF Ctrl][ble] gatt server started\r\n");

	/* BLE休眠モード状態 設定 */
	BleSleepState = BLE_SLEEP_STATE_ACTIVE;

#ifndef DISABLE_BLE_SLEEP
	/* 休眠モード用タイマが未生成の場合は生成する */
	if (ble_sleep == NULL) {
		ble_sleep = xTimerCreate("BLE_SLEEP_DELAY",(BLE_SLEEP_DELAY / portTICK_RATE_MS), pdFALSE, (void *)0, ble_sleep_timeout_handler);
	}
#endif /* #ifndef DISABLE_BLE_SLEEP */

	/* 応答受信待ちタイマが未生成の場合は生成する */
	if (wait_resp == NULL) {
		wait_resp = xTimerCreate("BLE_WAIT_RESP_TOUT",(BLE_WAIT_RESP_TOUT / portTICK_RATE_MS), pdFALSE, (void *)0, ble_wait_resp_timeout_handler);
	}

	/* 休眠状態解除待ちタイマが未生成の場合は生成する */
	if (wait_wakeup == NULL) {
		wait_wakeup = xTimerCreate("BLE_SLEEP_WAKEUP_LIMIT",(BLE_SLEEP_WAKEUP_LIMIT / portTICK_RATE_MS), pdFALSE, (void *)0, ble_wait_wakeup_timeout_handler);
	}

	return RF_ERR_SUCCESS;
}

RF_CTRL_ERR rf_ctrl_ble_stop_server(void)
{
	/* 各種タイマを削除 */
	if (ble_sleep != NULL) {
		xTimerDelete(ble_sleep, 0);
		ble_sleep = NULL;
	}
	if (wait_resp != NULL) {
		xTimerDelete(wait_resp, 0);
		wait_resp = NULL;
	}
	if (wait_wakeup != NULL) {
		xTimerDelete(wait_wakeup, 0);
		wait_wakeup = NULL;
	}

	/* BLE休眠モード状態 設定 */
	BleSleepState = BLE_SLEEP_STATE_OFF;

	/* Gatt Server停止 */
	stop_bt_gatt_server();
	Log_Info("\r\n[RF Ctrl][ble] gatt server stopped\r\n");

	return RF_ERR_SUCCESS;
}

#if CONFIG_AUDREY_ALWAYS_BT_ON
RF_CTRL_ERR rf_ctrl_ble_resume_server(void)
{
	Log_Info("\r\n[RF Ctrl][ble] %d: gatt server resumed\r\n", __LINE__);

	/* Advertising開始 */
	svr_set_adv_data();

	/* BLE休眠モード状態 設定 */
	BleSleepState = BLE_SLEEP_STATE_ACTIVE;

#ifndef DISABLE_BLE_SLEEP
	/* 休眠モード用タイマが未生成の場合は生成する */
	if (ble_sleep == NULL) {
		ble_sleep = xTimerCreate("BLE_SLEEP_DELAY",(BLE_SLEEP_DELAY / portTICK_RATE_MS), pdFALSE, (void *)0, ble_sleep_timeout_handler);
	}
#endif /* #ifndef DISABLE_BLE_SLEEP */

	/* 応答受信待ちタイマが未生成の場合は生成する */
	if (wait_resp == NULL) {
		wait_resp = xTimerCreate("BLE_WAIT_RESP_TOUT",(BLE_WAIT_RESP_TOUT / portTICK_RATE_MS), pdFALSE, (void *)0, ble_wait_resp_timeout_handler);
	}

	/* 休眠状態解除待ちタイマが未生成の場合は生成する */
	if (wait_wakeup == NULL) {
		wait_wakeup = xTimerCreate("BLE_SLEEP_WAKEUP_LIMIT",(BLE_SLEEP_WAKEUP_LIMIT / portTICK_RATE_MS), pdFALSE, (void *)0, ble_wait_wakeup_timeout_handler);
	}

	return RF_ERR_SUCCESS;
}

RF_CTRL_ERR rf_ctrl_ble_pause_server(void)
{
	/* BLE接続状態の場合を考慮し、切断を要求しておく */
	gatt_conn_disc(FALSE);

	/* 各種タイマを停止 */
	if ((ble_sleep != NULL) && (xTimerIsTimerActive(ble_sleep) != pdFALSE)) {
		xTimerStop(ble_sleep, 0);
	}
	if ((wait_resp != NULL) && (xTimerIsTimerActive(wait_resp) != pdFALSE)) {
		xTimerStop(wait_resp, 0);
	}
	if ((wait_wakeup != NULL) && (xTimerIsTimerActive(wait_wakeup) != pdFALSE)) {
		xTimerStop(wait_wakeup, 0);
	}

	/* BLE休眠モード状態 設定 */
	BleSleepState = BLE_SLEEP_STATE_OFF;

	Log_Info("\r\n[RF Ctrl][ble] %d: gatt server paused\r\n", __LINE__);

	return RF_ERR_SUCCESS;
}

void rf_ctrl_adv_disable(void)
{
	disable_advertising();
	return;
}

void rf_ctrl_adv_enable(void)
{
	enable_advertising();
	return;
}
#endif //#if CONFIG_AUDREY_ALWAYS_BT_ON

/**
 * Wifi APモード用 Advertise送信関連
 */
typedef enum {
	ADV_STATE_OFF,
	ADV_STATE_INITIALIZING,
	ADV_STATE_ADVERTISING,
	ADV_STATE_DEINITIALIZING,
	ADV_STATE_ERR,
} ADV_STATE;
static ADV_STATE adv_state = ADV_STATE_OFF;
#define ADV_WAIT_CNT_MAX	100		/* Advertise ON/OFF処理待ちのT.O.設定(単位：100ms) */

void rf_ctrl_adv_bt_done_cb(tBT_ResultEnum result, void *param)
{
	if ( result == BT_RES_OK) {
		Log_Info("\r\n[RF Ctrl][adv_on_AP] BT deinitialized\r\n");
		adv_state = ADV_STATE_OFF;
	}
	else {
		Log_Error("\r\n[RF Ctrl][adv_on_AP] Error, error code (tBT_ResultEnum): %d\r\n",result);
		SendMessageToStateManager(MSG_ERR_BT, PARAM_COMMON_NETDOWN);
		adv_state = ADV_STATE_ERR;
	}

	return;
}

static void rf_ctrl_adv_advertising_set_cb(tBT_ResultEnum result, void *param)
{
	if ( result == BT_RES_OK) {
		if (BT_GAP_BleAdvertisingGet() == TRUE) {
			Log_Info("\r\n[RF Ctrl][adv_on_AP] Advertising started\r\n");
			adv_state = ADV_STATE_ADVERTISING;
		} else {
			Log_Info("\r\n[RF Ctrl][adv_on_AP] Advertising stopped\r\n");
			Log_Info("[RF Ctrl][adv_on_AP] BT deinitializing\r\n");
			BT_Done(rf_ctrl_adv_bt_done_cb, 1);
		}
	}
	else {
		Log_Error("\r\n[RF Ctrl][adv_on_AP] Error, error code (tBT_ResultEnum): %d\r\n",result);
		SendMessageToStateManager(MSG_ERR_BT, PARAM_COMMON_NETDOWN);
		adv_state = ADV_STATE_ERR;
	}
}

extern struct netif xnetif[NET_IF_NUM];
void rf_ctrl_adv_bt_init_cb(tBT_ResultEnum result, void *param)
{
	tBT_EirDataStru adv_data;
	tBT_ManufacturerDataStru *manu_data;

	if (result == BT_RES_OK) {
		Log_Info("\r\n[RF Ctrl][adv_on_AP] BT initialized\r\n");
		memset(&adv_data,0,sizeof(tBT_EirDataStru));
		manu_data = &(adv_data.manufacturer_data);
		manu_data->data_len = 6;
		manu_data->company_id[0] = 0x81;
		manu_data->company_id[1] = 0x03;
		manu_data->data = audrey_mac;
		strcpy(adv_data.dev_name, GATT_SERVER_ADV_NAME_STR);
		adv_data.mask = BT_GAP_BLE_ADVDATA_MBIT_NAME | BT_GAP_BLE_ADVDATA_MBIT_MENUDATA;
		tBT_GapLEAdvParamStru param = {
			320, /* tUint16 int_min */
			400, /* tUint16 int_max */
			{0}, /* tble_AddressStru peer_addr */
			BT_GAP_BLE_ADV_TYPE_NONCONN, /* tUint8 type */
			BT_GAP_BLE_OWN_ADDRESS_TYPE_RANDOM, /* MUST BE RANDOM */
			BT_GAP_BLE_ADV_CHNMAP_CHALL, /* tUint8 chn_map */
			BT_GAP_BLE_ADV_FILTER_ALL, /* tUint8 filter */
		};

		BT_GAP_BleAdvertisingDataUpdate(&adv_data);
		BT_GAP_BleScanResponseDataUpdate(&adv_data);
		BT_GAP_BleAdvertisingParamsSet(&param);
		BT_GAP_BleAdvertisingSet(FALSE, NULL);
		BT_GAP_BleAdvertisingSet(TRUE, rf_ctrl_adv_advertising_set_cb);
		Log_Info("\r\n[RF Ctrl][adv_on_AP] start advertising ... Device name: %s\n\r", adv_data.dev_name);
	}
	else {
		Log_Error("\r\n[RF Ctrl][adv_on_AP] Error, error code (tBT_ResultEnum): %d\r\n",result);
		SendMessageToStateManager(MSG_ERR_BT, PARAM_COMMON_NETDOWN);
		adv_state = ADV_STATE_ERR;
	}

	return;
}

bool rf_ctrl_start_adv_on_AP(void)
{
	int cnt = 0;

	Log_Info("\r\n[RF Ctrl][adv_on_AP] Initializing BT ...\r\n");

	adv_state = ADV_STATE_INITIALIZING;
	/* Wifi APモードで呼ばれることが前提のため、Wifi OFF/ON処理は不要 */
	BT_Init(rf_ctrl_adv_bt_init_cb);

	/* 処理完了待ち合わせ */
	while (1) {
		vTaskDelay(100 * portTICK_PERIOD_MS);
		if (cnt++ >= ADV_WAIT_CNT_MAX) {
			Log_Error("\r\n[RF Ctrl][adv_on_AP] Initializing BT timeout\r\n");
			SendMessageToStateManager(MSG_ERR_BT, PARAM_COMMON_NETDOWN);
			return FALSE;
		}
		if (adv_state == ADV_STATE_ERR) {
			Log_Error("\r\n[RF Ctrl][adv_on_AP] Initializing BT failed\r\n");
			/* アラート通知用のメッセージは送信済みなのでここでは何もしない */
			adv_state = ADV_STATE_OFF;
			return FALSE;
		}
		if (adv_state == ADV_STATE_ADVERTISING) {
			/* 処理が完了したのでループを抜ける */
			break;
		}
	}

	return TRUE;
}

bool rf_ctrl_stop_adv_on_AP(void)
{
	int cnt = 0;

	Log_Info("\r\n[RF Ctrl][adv_on_AP] stop advertising ...\r\n");

	adv_state = ADV_STATE_DEINITIALIZING;
	BT_GAP_BleAdvertisingSet(FALSE, rf_ctrl_adv_advertising_set_cb);

	/* 処理完了待ち合わせ */
	while (1) {
		vTaskDelay(100 * portTICK_PERIOD_MS);
		if (cnt++ >= ADV_WAIT_CNT_MAX) {
			Log_Error("\r\n[RF Ctrl][adv_on_AP] Deinitializing BT timeout\r\n");
			SendMessageToStateManager(MSG_ERR_BT, PARAM_COMMON_NETDOWN);
			return FALSE;
		}
		if (adv_state == ADV_STATE_ERR) {
			Log_Error("\r\n[RF Ctrl][adv_on_AP] Deinitializing BT failed\r\n");
			/* アラート通知用のメッセージは送信済みなのでここでは何もしない */
			adv_state = ADV_STATE_OFF;
			return FALSE;
		}
		if (adv_state == ADV_STATE_OFF) {
			/* 処理が完了したのでループを抜ける */
			break;
		}
	}

	return TRUE;
}
