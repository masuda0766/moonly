#ifndef __DATA_UPLOAD_H__
#define __DATA_UPLOAD_H__

/*
 * Copyright (C) 2017 SHARP Corporation. All rights reserved.
 */

#include "platform_opts.h"
#include "weight.h"
#include "badge.h"
#include "timers.h"
#include "bt_util.h"
#if CONFIG_AUDREY_LOG_UPLOAD
#include "log_record.h"
#endif

/* 無線通信状態 */
enum rf_state {
	RF_STATE_NONE = 0,	/* 無線接続無し */
	RF_STATE_WLAN,		/* WLAN接続中 */
	RF_STATE_BLE		/* BLE接続中 */
};

/* データ送信 result code */
enum upload_result {
	UPLOAD_RESULT_SUCCESS = 0,	/* 成功 */
	UPLOAD_RESULT_FAIL = 1,		/* 失敗 */
};

/* 入退室後データ送信 エラー理由 */
enum upload_err {
	UPLOAD_ERR_NO_ERROR = 0,	/* エラーなし */
};

/* 個体識別用バッジ */
#define BD_NUM_MAX				10		/* 扱うバッジの最大数 (10件) */

/* タイプID */
#define TYPE_ID_ENTERING		1
#define TYPE_ID_PERIODIC		2
#define TYPE_ID_ALERT			3
#if CONFIG_AUDREY_DBG_UPLOAD
#define TYPE_ID_DBG_WEIGHT		4
#define TYPE_ID_DBG_BEACON		5
#define TYPE_ID_DBG_RSSI		6
#endif // CONFIG_AUDREY_DBG_UPLOAD
#if CONFIG_AUDREY_DBG_UPLOAD
#define TYPE_ID_ENTERING_DBG	7
#endif // CONFIG_AUDREY_DBG_UPLOAD
#if CONFIG_AUDREY_LOG_UPLOAD
#define TYPE_ID_LOG_ERROR		8
#define TYPE_ID_LOG_HF			9						/* サーバーにはid:8としてアップ */
#endif // CONFIG_AUDREY_LOG_UPLOAD

/* 接続種別 */
#define CONNECTION_TYPE_WLAN	1
#define CONNECTION_TYPE_BLE		2

/* 定期送信 送信データ 固有情報 */
#define VERINF_LENGTH_MAX		16								/* Wireless側, Scale側ともにMAX16byteの可変長文字列 */
#define COMBINED_VERINF_LENGTH	((VERINF_LENGTH_MAX * 2) + 1)	/* セパレータ'_' を含めて最大33byte */

/* 低電池残量バッジデータ構造体 */
struct bd_data {
	int			num;									/* データ件数 */
	u8			bd[BADGE_PET_MAX][BD_ADDR_LEN];			/* バッジのBDアドレス  "xx:xx:xx:xx:xx:xx" */
};

/* 送信データ構造体 */
#define ALERT_MSG_LENGTH		4		/* アラート通知メッセージ文字列長 */
typedef struct data_set {
	int				id;									/* データ種別  1 - 入退室後, 2 - 定期送信, 3 - アラート通知 */
	int				body;								/* 体重計のデータ */
	int				urine;								/* 尿量計のデータ */
	int				flag;								/* 尿量計値が低い状態 */
	int				tem;								/* 温度 */
	u8				mac[6];								/* MACアドレス */
	int				stay;								/* 滞在時間 */
	u32				time;								/* 時刻 */
	u8				bd[BD_ADDR_LEN];					/* 個体識別バッジのBDアドレス */
	char			ver[COMBINED_VERINF_LENGTH + 1];	/* FW Version */
	struct bd_data	low;								/* 低電池残量バッジ情報 */
	int				conn;								/* 接続種別ID 1 - WLAN, 2 - BLE */
	int				init;								/* 初回定期送信情報 */
	char			msg[ALERT_MSG_LENGTH + 1];			/* アラート通知メッセージ */
} upload_data;

#if CONFIG_AUDREY_DBG_UPLOAD
typedef struct dbg_w_dat {
	int				id;									/* データ種別  4 - デバッグデータ(重量) */
	u8				mac[6];								/* MACアドレス */
	WEIGHT_DBG		w_dbg[WEIGHT_DBG_MAX];				/* 重量情報 */
} dbg_weight_data;

typedef struct dbg_b_dat {
	int				id;									/* データ種別  5 - デバッグデータ(beacon) */
	u8				mac[6];								/* MACアドレス */
	time_t			time_in;							/* 入室時刻 */
	time_t			time_out;							/* 退室時刻 */
	int				badge_cnt;							/* バッジの登録数 */
	BADGE_DATA		bdg_dat[BADGE_PET_MAX];				/* バッジデータ */
} dbg_beacon_data;

typedef struct dbg_r_dat {
	int				id;									/* データ種別  6 - デバッグデータ(rssi) */
	int				cnt;								/* 送信データ数 */
	u8				mac[6];								/* MACアドレス */
	u8				bd[BD_ADDR_LEN];					/* BDアドレス */
	BADGE_DBG_BUF	bdg_dat[BADGE_DBG_RSSI_MAX];		/* RSSIデータ */
} dbg_rssi_data;
#endif

#if CONFIG_AUDREY_LOG_UPLOAD
typedef struct log_e_dat {
	int				id;									/* データ種別  8 - エラーログ */
	int				e_cnt;								/* 送信データ数 */
	int				v_cnt;								/* 送信データ数 */
	int				f_cnt;								/* 送信データ数 */
	u8				mac[6];								/* MACアドレス */
	LOG_INFO		err[LOG_ERR_MAX];					/* エラーコード */
	VER_INFO		ver[LOG_VER_MAX];					/* エラーコード */
	FAULT_INFO		fault[LOG_FAULT_MAX];					/* エラーコード */
} log_error_data;
#endif

/* 定期送信応答データペット情報構造体 */
typedef struct pet_data {
	u8		bd[BD_ADDR_LEN];	/* 個体識別バッジのBDアドレス */
	int		body;				/* 体重 */
} PET_DATA;

typedef struct upload_response {
	int				id;									/* データ種別  1 - 入退室後, 2 - 定期送信, 3 - アラート通知 */
	int				result;								/* 処理結果 */
	char			ver[COMBINED_VERINF_LENGTH + 1];	/* FW Version */
	int				petnum;								/* ペット情報件数 */
	PET_DATA		petdata[BADGE_PET_MAX];				/* ペット情報 */
#if CONFIG_AUDREY_DBG_UPLOAD
	int				dbg;								/* デバッグモードフラグ */
#endif
} upload_resp;

/* main関数向けAPI */
void data_upload_init(void);

/* Scale側向けAPI */
void data_upload_req(void);								/* 入退室後データ送信要求 */
#if CONFIG_AUDREY_DBG_UPLOAD
void dbg_weight_data_upload_req(void);					/* デバッグデータ(重量)送信要求 */
void dbg_beacon_data_upload_req(void);					/* デバッグデータ(beacon)送信要求 */
void dbg_rssi_data_upload_req(void);					/* デバッグデータ(rssi)送信要求 */
#endif
#if CONFIG_AUDREY_LOG_UPLOAD
void log_error_upload_req(void);						/* エラーログ送信要求 */
#endif

/* 状態管理部向けAPI */
void data_upload_req(void);									/* 入退室後データ送信要求 */
void data_upload_rf_state_changed(enum rf_state state);		/* 無線接続状態遷移通知 (保留データ送信要求) */
void alert_notification_req(unsigned int id);				/* アラート通知送信要求 */
void pending_data_upload_req(void);							/* 保留データ送信要求 */

/* 無線通信制御部向けAPI */
void data_upload_entering_result(upload_resp *data);		/* 入退室後データ送信結果 */
void data_upload_periodic_result(upload_resp *data);		/* 定期送信データ送信結果 */
void data_upload_alert_result(upload_resp *data);			/* アラート通知送信結果 */
#if CONFIG_AUDREY_DBG_UPLOAD
void data_upload_debug_weight_result(upload_resp *data);	/* デバッグデータ(重量)送信結果 */
void data_upload_debug_beacon_result(upload_resp *data);	/* デバッグデータ(beacon)送信結果 */
void data_upload_debug_rssi_result(upload_resp *data);		/* デバッグデータ(rssi)送信結果 */
int is_dbg_mode(void);										/* デバッグモード チェック */
#endif
#if CONFIG_AUDREY_LOG_UPLOAD
void data_upload_log_error_result(upload_resp *data);		/* エラーログ送信結果 */
void data_upload_log_hf_result(upload_resp *data);			/* HardFaultError送信結果 */
#endif
bool is_data_upload_in_progress(void);						/* データ送信処理実施状況 チェック */

void data_up_req_start(void);								/* リクエスト～レスポンス待ち状態設定 */
void data_up_req_end(void);									/* リクエスト～レスポンス待ち状態解除 */

#if CONFIG_AUDREY_UPLOAD_RETRY
int is_upload_retry(void);									/* データアップロード リトライ中チェック */
#endif

#endif //#ifndef __DATA_UPLOAD_H__
