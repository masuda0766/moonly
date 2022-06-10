/*
 * Copyright (C) 2017 SHARP Corporation. All rights reserved.
 */
#include <platform_opts.h>
#include <platform/platform_stdlib.h>
#include <lwip_netconf.h>
#include <sntp/sntp.h>
#include "FreeRTOS.h"
#include "semphr.h"
#include "osdep_api.h"
#include "osdep_service.h"
#include "task.h"
#include "timer_api.h"
#include "basic_types.h"
#include "data_upload.h"
#include "rf_ctrl.h"
#include "temperature.h"
#include "version.h"
#include "ota_http_client.h"
#include "scale.h"
#include "rtc_api.h"
#include "state_manager.h"
#include "wdt.h"
#if CONFIG_AUDREY_BACKUP_ENTRY || CONFIG_AUDREY_BACKUP_BADGE
#include "flash_api.h"
#include "device_lock.h"
#endif /* CONFIG_AUDREY_BACKUP_ENTRY || CONFIG_AUDREY_BACKUP_BADGE */
#if CONFIG_AUDREY_CONF_BACKUP
#include "log_record.h"
#endif

/* 開発用オプション (default : OFF) */
//#define TIMER_DELAY_FOR_DEV		/* 定期送信間隔を開発用にする場合に定義 */
//#define FW_UPDATE_DISABLE			/* FW更新が動いてほしくない場合に、ota_http_client.c へのバージョン番号通知を止める */

/* プロトタイプ宣言 */
#define DATA_UP_REQ						300000						/* リクエスト～レスポンス待ち：5分間 (単位：ms) */
bool is_data_up_req = FALSE;										/* リクエスト～レスポンス待ちフラグ */
TimerHandle_t			data_up_req_timer;							/* リクエスト～レスポンス待ちタイマハンドラ */

bool dataup_ssl_fail_flg = FALSE;									/* SSL失敗フラグ(フェールセーフ監視用) */
static int dataup_ssl_fail_cnt = 0;									/* SSL連続失敗数カウンタ */
#define SSL_FAIL_COUNT_MAX			5							/* SSL連続失敗最大数 */

static void periodic_timer_start(uint32_t delay);					/* 定期送信タイマスタート      */
void periodic_timeout_handler(xTimerHandle pxTimer);				/* 定期送信タイマハンドラ      */

/* 共通定義 */
#if CONFIG_LWIP_LAYER
extern struct netif xnetif[NET_IF_NUM];
#endif
static volatile enum rf_state	rf_type = RF_STATE_NONE;			/* 無線接続種別 */
static volatile bool			fPendingDataUploadReq = FALSE;		/* 保留データ送信要求フラグ */
static volatile bool			fDataUploadCheck = FALSE;			/* データ送信処理チェック中フラグ（TRUE中は無線切断処理不可） */

		/* 定期送信関連 */
#define PERIODIC_UPLOAD_DELAY_HOUR		4							/* 定期送信間隔：4時間 (単位：hour)*/
#ifdef TIMER_DELAY_FOR_DEV
#define PERIODIC_UPLOAD_DELAY			300000						/* 定期送信間隔（開発用）：5分間 (単位：ms) */
#else																/* 定期送信間隔：4時間 (単位：ms) */
#define PERIODIC_UPLOAD_DELAY			(PERIODIC_UPLOAD_DELAY_HOUR * 3600000)
#endif
#define PERIODIC_UPLOAD_DELAY_SHORT		30000						/* 定期送信間隔（即時送信用）：30秒間 (単位：ms) */
#define PENDING_DATA_UPLOAD_DELAY		5000						/* 保留データ送信待ち時間：5秒間 */
#define PERIODIC_UPLOAD_DELAY_RETRY		120000						/* 初回定期送信リトライ間隔：2分間 (単位：ms) */
#if CONFIG_AUDREY_UPLOAD_RETRY
#define PERIODIC_UPLOAD_RETRY_MAX		5							/* 初回定期送信回数 5回 */
#define DATA_UPLOAD_FAIL				120000						/* 定期/入退室送信失敗時再送間隔：2分間 (単位：ms) */
#define DATA_UPLOAD_RETRY_MAX			2							/* 定期/入退室送信失敗時再送回数 2回 */
#else
#define PERIODIC_UPLOAD_RETRY_MAX		4							/* 初回定期送信回数 4回 */
#endif
static int						dataup_fail_cnt = 0;
static int						periodic_retry_cnt = PERIODIC_UPLOAD_RETRY_MAX;
																	/* 定期送信リトライカウンタ */
static int						periodic_upload_hour = 0;			/* 定期送信時刻：時 */
static int						periodic_upload_minute = 0;			/* 定期送信時刻：分 */
static int						periodic_upload_sec = 0;			/* 定期送信時刻：秒 */
static volatile bool			fPeriodicTimer = FALSE;				/* 定期送信タイマ起動済フラグ (TRUE:タイマ起動済) */
TimerHandle_t					periodic_timer;						/* 定期送信タイマハンドラ */
static volatile bool			fPeriodicUploadRequest = FALSE;		/* 定期送信要求フラグ */
static volatile bool			fPeriodicUploadByTimer = FALSE;		/* タイマ契機定期送信識別フラグ */
static volatile bool			fPeriodicShortTimer = FALSE;				/* 短時間タイマ満了待ちフラグ */
static volatile uint32_t		last_upload_by_timer = 0;			/* 最終定期送信（タイマによる送信のみ）実施時刻 */
TimerHandle_t					pending_data_upload_delay_timer;	/* 保留データ送信待ちタイマハンドラ */
static bool		pending_data_upload_delay_timer_created = FALSE;	/* 保留データ送信待ちタイマ生成フラグ */

/* 入退室後データ送信用定義  */
#define ENTERING_DATA_BUFF_LENGTH	((10 * 3 * 7) + 1)				/* 入退室後送信データ用バッファサイズ (1日10回×3頭×7日) */
static volatile bool	fEntryUploadReq = FALSE;					/* 入退室後データ送信要求受信フラグ */
																	/* リングバッファ */
static upload_data		entering_data_buf[ENTERING_DATA_BUFF_LENGTH + 1];
static unsigned int		buf_idx_read  = 0;							/* read index  */
static unsigned int		buf_idx_write = 0;							/* write index */

#if CONFIG_AUDREY_BACKUP_ENTRY
#define ENTRY_MAGIC_NUM		"EntryBack-Rev001"						/* マジックナンバー */
#define ENTRY_BACKUP_MAX	(ENTERING_DATA_BUFF_LENGTH - 1)			/* 入退室バックアップ数 */
#define DELAY_ENTRY_BACKUP	120000									/* 入退室バックアップ遅延時間：120秒間 */

typedef struct {
	int				time;											/* 日時 */
	unsigned int	stay_tem;										/* 下位:滞在時間、上位:温度 */
	unsigned int	weight;											/* 下位:体重、上位:尿量 */
} ENTRY_BUF;

typedef struct {
	int				cnt;											/* 入退室情報Backup数 */
	ENTRY_BUF		buf[ENTRY_BACKUP_MAX];							/* 入退室情報Backupデータ */
	u8				bd[ENTRY_BACKUP_MAX][BD_ADDR_LEN];				/* 個体識別バッジのBDアドレス */
	u8				magic[sizeof(ENTRY_MAGIC_NUM)];					/* マジックナンバー格納領域 */
} ENTRY_BACKUP;

flash_t entry_flash;
static ENTRY_BACKUP		entry_backup;

static volatile bool	fEntryWaitConnect = FALSE;					/* 入退室後無線接続待ちフラグ */
TimerHandle_t			delay_entry_backup_timer;					/* 入退室バックアップ遅延タイマハンドラ */
static bool				delay_entry_backup_timer_created = FALSE;	/* 入退室バックアップ遅延タイマ生成フラグ */
#endif /* CONFIG_AUDREY_BACKUP_ENTRY */

#if CONFIG_AUDREY_BACKUP_BADGE
#define BADGE_MAGIC_NUM		"BadgeBack-Rev001"						/*マジックナンバー */

typedef struct {
	BADGE_CONF		buf[BADGE_PET_MAX];								/* バッジ情報 */
	int				cnt_bd;											/* バッジBackup数 */
	u8				magic[sizeof(BADGE_MAGIC_NUM)];					/* マジックナンバー格納領域 */
} BADGE_BACKUP;

flash_t badge_flash;
static BADGE_BACKUP		badge_backup;
#endif /* CONFIG_AUDREY_BACKUP_BADGE */

/* アラート通知用定義  */
#define ALERT_NTF_BUFF_LENGTH		10								/* アラート通知用バッファサイズ */
static volatile bool	fAlertNtfReq = FALSE;						/* アラート通知要求受信フラグ */
static upload_data	alert_ntf_buf[ALERT_NTF_BUFF_LENGTH+1];			/* リングバッファ */
static unsigned int	alert_ntf_buf_idx_read  = 0;					/* read index  */
static unsigned int	alert_ntf_buf_idx_write = 0;					/* write index */
static upload_data	saved_alert;									/* 通知情報保持用バッファ */

#if CONFIG_AUDREY_DBG_UPLOAD
static int				fDbgMode = 0;								/* デバッグモードフラグ */

/* デバッグデータ(重量)送信処理用定義 */
#define DBG_WEIGHT_BUF_LEN			15								/* = (TIME_ENTRY / WEIGHT_DBG_MAX)[端数切り上げ] + 1 */
static volatile bool	fDbgWeightUploadReq = FALSE;				/* デバッグデータ(重量)送信要求受信フラグ */

SECTION(".sdram.data")
dbg_weight_data	dbg_w_data_buf[DBG_WEIGHT_BUF_LEN+1];				/* リングバッファ */

static unsigned int		dbg_w_buf_idx_read  = 0;					/* read index  */
static unsigned int		dbg_w_buf_idx_write = 0;					/* write index */

static WEIGHT_DBG		weight_dbg_save[WEIGHT_DBG_MAX];

/* デバッグデータ(beacon)送信処理用定義 */
#define DBG_BEACON_BUF_LEN			20								/* 20回分のデータを保持 */
static volatile bool	fDbgBeaconUploadReq = FALSE;				/* デバッグデータ(beacon)送信要求受信フラグ */

SECTION(".sdram.data")
dbg_beacon_data	dbg_b_data_buf[DBG_BEACON_BUF_LEN+1];				/* リングバッファ */

static unsigned int		dbg_b_buf_idx_read  = 0;					/* read index  */
static unsigned int		dbg_b_buf_idx_write = 0;					/* write index */

static dbg_beacon_data	badge_data_save;

/* デバッグデータ(rssi)送信処理用定義 */
#define DBG_RSSI_BUF_LEN				18							/* 18回分のデータを保持 */
static volatile bool	fDbgRssiUploadReq = FALSE;					/* デバッグデータ(rssi)送信要求受信フラグ */

SECTION(".sdram.data")
dbg_rssi_data	dbg_r_data_buf[DBG_RSSI_BUF_LEN+1];					/* リングバッファ */

static unsigned int		dbg_r_buf_idx_read  = 0;					/* read index  */
static unsigned int		dbg_r_buf_idx_write = 0;					/* write index */

//static dbg_rssi_data	rssi_data_save;

/* デバッグデータ(重量)送信要求 */
void dbg_weight_data_upload_req(void)
{
	Log_Info("\r\n[data_upload][dbg][weight] debug data (weight) upload requested\r\n");
	memcpy(weight_dbg_save, weight_dbg, sizeof(weight_dbg_save)); /* データを手元にコピーして保持 */
	fDbgWeightUploadReq = TRUE;
	return;
}

/* デバッグデータ(beacon)送信要求 */
void dbg_beacon_data_upload_req(void)
{
	Log_Info("\r\n[data_upload][dbg][beacon] debug data (beacon) upload requested\r\n");
	badge_data_save.time_in = badge_dbg_time_in;
	badge_data_save.time_out = badge_dbg_time_out;
	badge_data_save.badge_cnt = badge_config_cnt;
	memcpy(badge_data_save.bdg_dat, &badge_data, sizeof(badge_data_save.bdg_dat));
	fDbgBeaconUploadReq = TRUE;
	return;
}

/* デバッグデータ(rssi)送信要求 */
void dbg_rssi_data_upload_req(void)
{
	Log_Info("\r\n[data_upload][dbg][rssi] debug data (rssi) upload requested\r\n");
	fDbgRssiUploadReq = TRUE;
	return;
}

/* デバッグデータ(重量)送信処理 実行関数 */
static void debug_weight_data_upload_proc(void)
{
#if CONFIG_AUDREY_REDUCE_MALLOC
	/* writeポインタがreadポインタの直後にある場合、readポインタを進めてバッファを開ける */
	if ((dbg_w_buf_idx_write == dbg_w_buf_idx_read - 1) ||
		((dbg_w_buf_idx_write == DBG_WEIGHT_BUF_LEN) && (dbg_w_buf_idx_read == 0))) {
		if (dbg_w_buf_idx_read == DBG_WEIGHT_BUF_LEN) {
			dbg_w_buf_idx_read = 0;
		} else {
			dbg_w_buf_idx_read++;
		}
		Log_Info("\r\n[data_upload][dbg_w ] dbg_w_data_buf[] full. oldest data deleted. \r\n");
	}
	/* 送信データ編集 */
	dbg_w_data_buf[dbg_w_buf_idx_write].id = TYPE_ID_DBG_WEIGHT;
	memcpy(&dbg_w_data_buf[dbg_w_buf_idx_write].mac, &audrey_mac, sizeof(audrey_mac));
	memcpy(&dbg_w_data_buf[dbg_w_buf_idx_write].w_dbg, &weight_dbg_save, sizeof(weight_dbg_save));
	/* writeポインタ更新 */
	if (dbg_w_buf_idx_write >= DBG_WEIGHT_BUF_LEN) {
		dbg_w_buf_idx_write = 0;
	} else {
		dbg_w_buf_idx_write++;
	}
#else //#if CONFIG_AUDREY_REDUCE_MALLOC
	dbg_weight_data	*updata;

	/* 送信データ編集用メモリ確保 */
	updata = malloc(sizeof(dbg_weight_data));
	if (updata == NULL) {
		Log_Error("\r\n[data_upload][dbg_w ] malloc() failed\r\n");
		SendMessageToStateManager(MSG_ERR_DUL_FATAL, PARAM_COMMON_NOMEM);
		return;
	}

	/* 送信データ編集 */
	updata->id = TYPE_ID_DBG_WEIGHT;
	memcpy(updata->mac, audrey_mac, sizeof(updata->mac));
	memcpy(updata->w_dbg, &weight_dbg_save, sizeof(updata->w_dbg));

	/* writeポインタがreadポインタの直後にある場合、readポインタを進めてバッファを開ける */
	if ((dbg_w_buf_idx_write == dbg_w_buf_idx_read - 1) ||
		((dbg_w_buf_idx_write == DBG_WEIGHT_BUF_LEN) && (dbg_w_buf_idx_read == 0))) {
		if (dbg_w_buf_idx_read == DBG_WEIGHT_BUF_LEN) {
			dbg_w_buf_idx_read = 0;
		} else {
			dbg_w_buf_idx_read++;
		}
		Log_Info("\r\n[data_upload][dbg_w ] dbg_w_data_buf[] full. oldest data deleted. \r\n");
	}
	/* バッファに保存 */
	memcpy(&dbg_w_data_buf[dbg_w_buf_idx_write], updata, sizeof(dbg_weight_data));
	/* writeポインタ更新 */
	if (dbg_w_buf_idx_write >= DBG_WEIGHT_BUF_LEN) {
		dbg_w_buf_idx_write = 0;
	} else {
		dbg_w_buf_idx_write++;
	}
	free(updata);
#endif //#if CONFIG_AUDREY_REDUCE_MALLOC

	/* データ送信可否チェック  */
	if (rf_ctrl_check_connection() != RF_ERR_SUCCESS) {
		/* 送信不可時は何もせずに終了 */
		Log_Info("\r\n[data_upload][dbg_w ] Debug data (weight) put into buffer. But upload skipped.\r\n");
		goto exit;
	} else {
		/* データ送信要求 */
		rf_ctrl_dbg_w_data_upload(&dbg_w_data_buf[dbg_w_buf_idx_read]);
	}

exit:
	return;
}


/* デバッグデータ(beacon)送信処理 実行関数 */
static void debug_beacon_data_upload_proc(void)
{
	/* 送信データ編集 */
	badge_data_save.id = TYPE_ID_DBG_BEACON;
	memcpy(badge_data_save.mac, audrey_mac, sizeof(badge_data_save.mac));

	/* writeポインタがreadポインタの直後にある場合、readポインタを進めてバッファを開ける */
	if ((dbg_b_buf_idx_write == dbg_b_buf_idx_read - 1) ||
		((dbg_b_buf_idx_write == DBG_BEACON_BUF_LEN) && (dbg_b_buf_idx_read == 0))) {
		if (dbg_b_buf_idx_read == DBG_BEACON_BUF_LEN) {
			dbg_b_buf_idx_read = 0;
		} else {
			dbg_b_buf_idx_read++;
		}
		Log_Info("\r\n[data_upload][dbg_b ] dbg_b_data_buf[] full. oldest data deleted. \r\n");
	}
	/* バッファに保存 */
	memcpy(&dbg_b_data_buf[dbg_b_buf_idx_write], &badge_data_save, sizeof(dbg_beacon_data));
	if (dbg_b_buf_idx_write >= DBG_BEACON_BUF_LEN) {
		dbg_b_buf_idx_write = 0;
	} else {
		dbg_b_buf_idx_write++;
	}

	/* データ送信可否チェック  */
	if (rf_ctrl_check_connection() != RF_ERR_SUCCESS) {
		/* 送信不可時は何もせずに終了 */
		Log_Info("\r\n[data_upload][dbg_b ] Debug data (beacon) put into buffer. But upload skipped.\r\n");
		goto exit;
	} else {
		/* データ送信要求 */
		rf_ctrl_dbg_b_data_upload(&dbg_b_data_buf[dbg_b_buf_idx_read]);
	}

exit:
	return;
}

/* デバッグデータ(rssi)送信処理 実行関数 */
static void debug_rssi_data_upload_proc(void)
{
	int i;

#if CONFIG_AUDREY_REDUCE_MALLOC
	/* 送信データ編集 */
	for(i = 0; i < badge_dbg_cnt; i++) {
		if(badge_dbg[i].cnt != 0) {
			/* writeポインタがreadポインタの直後にある場合、readポインタを進めてバッファを開ける */
			if ((dbg_r_buf_idx_write == dbg_r_buf_idx_read - 1) ||
				((dbg_r_buf_idx_write == DBG_RSSI_BUF_LEN) && (dbg_r_buf_idx_read == 0))) {
				if (dbg_r_buf_idx_read == DBG_RSSI_BUF_LEN) {
					dbg_r_buf_idx_read = 0;
				} else {
					dbg_r_buf_idx_read++;
				}
				Log_Info("\r\n[data_upload][dbg_r ] dbg_r_data_buf[] full. oldest data deleted. \r\n");
			}
			Log_Info("\r\n[data_upload][dbg][rssi] badge_dbg_cnt=%d, len=%d, rp=%d, wp=%d\r\n"
			, badge_dbg_cnt, badge_dbg[i].cnt, dbg_r_buf_idx_read, dbg_r_buf_idx_write);
			dbg_r_data_buf[dbg_r_buf_idx_write].id = TYPE_ID_DBG_RSSI;
			memcpy(&dbg_r_data_buf[dbg_r_buf_idx_write].mac, &audrey_mac, sizeof(audrey_mac));
			memcpy(&dbg_r_data_buf[dbg_r_buf_idx_write].bd, badge_dbg[i].bd_addr, BD_ADDR_LEN);
			memcpy(&dbg_r_data_buf[dbg_r_buf_idx_write].bdg_dat, &badge_dbg[i].buf[0], sizeof(BADGE_DBG_BUF)*badge_dbg[i].cnt);
			dbg_r_data_buf[dbg_r_buf_idx_write].cnt = badge_dbg[i].cnt;
			if (dbg_r_buf_idx_write >= DBG_RSSI_BUF_LEN) {
				dbg_r_buf_idx_write = 0;
			} else {
				dbg_r_buf_idx_write++;
			}
		}
	}
#else //#if CONFIG_AUDREY_REDUCE_MALLOC
	dbg_rssi_data	*updata;

	Log_Info("\r\n[data_upload][dbg][rssi] debug data (rssi) proc\r\n");
	/* 送信データ編集用メモリ確保 */
	updata = malloc(sizeof(dbg_rssi_data));
	if (updata == NULL) {
		Log_Error("\r\n[data_upload][info__] malloc() failed\r\n");
		SendMessageToStateManager(MSG_ERR_DUL_FATAL, PARAM_COMMON_NOMEM);
		return;
	}

	/* 送信データ編集 */
	updata->id = TYPE_ID_DBG_RSSI;
	memcpy(updata->mac, audrey_mac, sizeof(updata->mac));
	for(i = 0; i < badge_dbg_cnt; i++) {
		if(badge_dbg[i].cnt != 0) {
			memcpy(updata->bd, badge_dbg[i].bd_addr, sizeof(updata->bd));
			memcpy(updata->bdg_dat, &badge_dbg[i].buf[0], sizeof(BADGE_DBG_BUF)*badge_dbg[i].cnt);
			updata->cnt = badge_dbg[i].cnt;
			/* writeポインタがreadポインタの直後にある場合、readポインタを進めてバッファを開ける */
			if ((dbg_r_buf_idx_write == dbg_r_buf_idx_read - 1) ||
				((dbg_r_buf_idx_write == DBG_RSSI_BUF_LEN) && (dbg_r_buf_idx_read == 0))) {
				if (dbg_r_buf_idx_read == DBG_RSSI_BUF_LEN) {
					dbg_r_buf_idx_read = 0;
				} else {
					dbg_r_buf_idx_read++;
				}
				Log_Info("\r\n[data_upload][dbg_r ] dbg_r_data_buf[] full. oldest data deleted. \r\n");
			}
			Log_Info("\r\n[data_upload][dbg][rssi] badge_dbg_cnt=%d, "BD_ADDR_FMT", len=%d, rp=%d, wp=%d\r\n"
			, badge_dbg_cnt, BD_ADDR_ARG(updata->bd), badge_dbg[i].cnt, dbg_r_buf_idx_read, dbg_r_buf_idx_write);
			memcpy(&dbg_r_data_buf[dbg_r_buf_idx_write], updata, sizeof(dbg_rssi_data));
			if (dbg_r_buf_idx_write >= DBG_RSSI_BUF_LEN) {
				dbg_r_buf_idx_write = 0;
			} else {
				dbg_r_buf_idx_write++;
			}
		}
	}
	free(updata);
#endif //#if CONFIG_AUDREY_REDUCE_MALLOC

	/* データ送信可否チェック  */
	if(dbg_r_buf_idx_read == dbg_r_buf_idx_write) {
		/* 受信RSSIが無い場合は何もせずに終了 */
		Log_Info("\r\n[data_upload][dbg_r ] Debug data (rssi) Empty\r\n");
		goto exit;
	} else if (rf_ctrl_check_connection() != RF_ERR_SUCCESS) {
		/* 送信不可時は何もせずに終了 */
		Log_Info("\r\n[data_upload][dbg_r ] Debug data (rssi) put into buffer. But upload skipped.\r\n");
		goto exit;
	} else {
		/* データ送信要求 */
		rf_ctrl_dbg_r_data_upload(&dbg_r_data_buf[dbg_r_buf_idx_read]);
	}

exit:
	return;
}

/* デバッグデータ(重量)送信結果 */
void data_upload_debug_weight_result(upload_resp *data)
{
	if (data == NULL) {
		Log_Error("\r\n[data_upload][debug_w_result] Receive response error\r\n");
		/* 応答受信失敗時は readポインタを進めずに終了 → 次のタイミングで再送 */
		return;
	}

	Log_Info("\r\n[data_upload][debug_w_result] result = %d \r\n", data->result);
	if (data->result == UPLOAD_RESULT_FAIL) {
		/* 送信結果＝失敗時は readポインタを進めずに終了 → 次のタイミングで再送 */
		return;
	}

	/* 送信成功の応答を受けたら、バッファのreadポインタを進める */
	if (data->result == UPLOAD_RESULT_SUCCESS) {
		if (dbg_w_buf_idx_read >= DBG_WEIGHT_BUF_LEN) {
			dbg_w_buf_idx_read = 0;
		} else {
			dbg_w_buf_idx_read++;
		}
		fPendingDataUploadReq = TRUE;	/* 保留データ送信要求フラグ ON */
	}

	return;
}

/* デバッグデータ(beacon)送信結果 */
void data_upload_debug_beacon_result(upload_resp *data)
{
	if (data == NULL) {
		Log_Error("\r\n[data_upload][debug_b_result] Receive response error\r\n");
		/* 応答受信失敗時は readポインタを進めずに終了 → 次のタイミングで再送 */
		return;
	}

	Log_Info("\r\n[data_upload][debug_b_result] result = %d \r\n", data->result);
	if (data->result == UPLOAD_RESULT_FAIL) {
		/* 送信結果＝失敗時は readポインタを進めずに終了 → 次のタイミングで再送 */
		return;
	}

	/* 送信成功の応答を受けたら、バッファのreadポインタを進める */
	if (data->result == UPLOAD_RESULT_SUCCESS) {
		if (dbg_b_buf_idx_read >= DBG_BEACON_BUF_LEN) {
			dbg_b_buf_idx_read = 0;
		} else {
			dbg_b_buf_idx_read++;
		}
		fPendingDataUploadReq = TRUE;	/* 保留データ送信要求フラグ ON */
	}

	return;
}

/* デバッグデータ(rssi)送信結果 */
void data_upload_debug_rssi_result(upload_resp *data)
{
	if (data == NULL) {
		Log_Error("\r\n[data_upload][debug_r_result] Receive response error\r\n");
		/* 応答受信失敗時は readポインタを進めずに終了 → 次のタイミングで再送 */
		return;
	}

	Log_Info("\r\n[data_upload][debug_r_result] result = %d \r\n", data->result);
	if (data->result == UPLOAD_RESULT_FAIL) {
		/* 送信結果＝失敗時は readポインタを進めずに終了 → 次のタイミングで再送 */
		return;
	}

	/* 送信成功の応答を受けたら、バッファのreadポインタを進める */
	if (data->result == UPLOAD_RESULT_SUCCESS) {
		if (dbg_r_buf_idx_read >= DBG_RSSI_BUF_LEN) {
			dbg_r_buf_idx_read = 0;
		} else {
			dbg_r_buf_idx_read++;
		}
		fPendingDataUploadReq = TRUE;	/* 保留データ送信要求フラグ ON */
	}

	return;
}

int is_dbg_mode(void)
{
	return fDbgMode;
}

#endif // CONFIG_AUDREY_DBG_UPLOAD

#if CONFIG_AUDREY_LOG_UPLOAD
/* エラーログ送信処理用定義 */
static volatile bool	fLogErrorUploadReq = FALSE;					/* エラーログ送信要求受信フラグ */
static volatile bool	fLogHfUploadReq = FALSE;					/* HardFaultError送信要求受信フラグ */

SECTION(".sdram.data")
log_error_data	log_error_buf;

/* エラーログ送信要求 */
void dbg_errlog_upload_req(void)
{
	fLogErrorUploadReq = TRUE;
	return;
}

/* HardFaultError送信要求 */
void dbg_hflog_upload_req(void)
{
	fLogHfUploadReq = TRUE;
	return;
}

/* エラーログ送信処理 実行関数 */
static void log_error_upload_proc(void)
{
	VER_INFO ver;
	LOG_INFO data;
	int i;

	/* 送信データ編集 */
	log_error_buf.id = TYPE_ID_LOG_ERROR;
	memcpy(log_error_buf.mac, audrey_mac, sizeof(log_error_buf.mac));

	for(i = 0; i < LOG_VER_MAX; i++) {
		log_record_ver_get(i, &ver);
		if(ver.time == 0) {
			break;
		}
		memcpy(log_error_buf.ver[i].info, ver.info, LOG_VER_LEN);
		log_error_buf.ver[i].time = ver.time;
	}
	log_error_buf.v_cnt = i;

	for(i = 0; i < LOG_ERR_MAX; i++) {
		log_record_err_get(i, &data);
		if(data.id == 0) {
			break;
		}
		log_error_buf.err[i].id = data.id;
		log_error_buf.err[i].param = data.param;
		log_error_buf.err[i].time = data.time;
	}
	log_error_buf.e_cnt = i;
	log_error_buf.f_cnt = 0;

	/* データ送信可否チェック  */
	if(log_error_buf.v_cnt == 0 && log_error_buf.e_cnt == 0) {
		/* エラーログが無い場合は何もせずに終了 */
		Log_Info("\r\n[data_upload][log_e ] Empty\r\n");
		dbg_hflog_upload_req();
		goto exit;
	} else if (rf_ctrl_check_connection() != RF_ERR_SUCCESS) {
		/* 送信不可時は何もせずに終了 */
		Log_Info("\r\n[data_upload][log_e ] put into buffer. But upload skipped.\r\n");
		goto exit;
	} else {
		/* データ送信要求 */
		Log_Info("\r\n[data_upload][log_e ] Update\r\n");
		rf_ctrl_log_e_upload(&log_error_buf);
	}

exit:
	return;
}

/* HardFaultError送信処理 実行関数 */
static void log_hf_upload_proc(void)
{
	LOG_INFO data;
	FAULT_INFO fault;
	int i;

	/* 送信データ編集 */
	log_error_buf.id = TYPE_ID_LOG_HF;
	memcpy(log_error_buf.mac, audrey_mac, sizeof(log_error_buf.mac));

	for(i = 0; i < LOG_FAULT_MAX; i++) {
		log_record_fault_get(i, &fault);
		if(fault.time == 0) {
			break;
		}
		strcpy(log_error_buf.fault[i].pc, fault.pc);
		log_error_buf.fault[i].time = fault.time;
	}
	log_error_buf.f_cnt = i;
	log_error_buf.e_cnt = 0;
	log_error_buf.v_cnt = 0;

	/* データ送信可否チェック  */
	if(log_error_buf.f_cnt == 0) {
		/* HardFaultErrorが無い場合は何もせずに終了 */
		Log_Info("\r\n[data_upload][log_h ] Empty\r\n");
		goto exit;
	} else if (rf_ctrl_check_connection() != RF_ERR_SUCCESS) {
		/* 送信不可時は何もせずに終了 */
		Log_Info("\r\n[data_upload][log_h ] put into buffer. But upload skipped.\r\n");
		goto exit;
	} else {
		/* データ送信要求 */
		Log_Info("\r\n[data_upload][log_h ] Update\r\n");
		rf_ctrl_log_e_upload(&log_error_buf);
	}

exit:
	return;
}

/* エラーログ送信結果 */
void data_upload_log_error_result(upload_resp *data)
{
	fPendingDataUploadReq = TRUE;	/* 保留データ送信要求フラグ ON */

	if (data == NULL) {
		Log_Error("\r\n[data_upload][log_e_result] Receive response error\r\n");
		return;
	}

	Log_Info("\r\n[data_upload][log_e_result] result = %d \r\n", data->result);
	if (data->result == UPLOAD_RESULT_FAIL) {
		return;
	}

	/* 送信成功の応答を受けたら、ログ消去 */
	if (data->result == UPLOAD_RESULT_SUCCESS) {
		log_record_err_del();
		Log_Info("\r\n[data_upload][log_e_result] Delete Error Log\r\n");
		dbg_hflog_upload_req();
	}
	return;
}

void data_upload_log_hf_result(upload_resp *data)
{
	fPendingDataUploadReq = TRUE;	/* 保留データ送信要求フラグ ON */

	if (data == NULL) {
		Log_Error("\r\n[data_upload][log_h_result] Receive response error\r\n");
		return;
	}

	Log_Info("\r\n[data_upload][log_h_result] result = %d \r\n", data->result);
	if (data->result == UPLOAD_RESULT_FAIL) {
		return;
	}

	/* 送信成功の応答を受けたら、ログ消去 */
	if (data->result == UPLOAD_RESULT_SUCCESS) {
		log_record_erase_fault();
		Log_Info("\r\n[data_upload][log_h_result] Delete Fault Log\r\n");
	}
	return;
}
#endif // CONFIG_AUDREY_LOG_UPLOAD

/* 入退室後データ送信要求 */
void data_upload_req(void)
{
	Log_Info("\r\n[data_upload][entry_] requested\r\n");
	fEntryUploadReq = TRUE;
	return;
}

/* アラート通知送信要求 */
void alert_notification_req(unsigned int id)
{
	Log_Info("\r\n[data_upload][alert_] requested : %u\r\n", id);
	saved_alert.id = TYPE_ID_ALERT;
	memcpy(saved_alert.mac, audrey_mac, sizeof(saved_alert.mac));
	saved_alert.time = rtc_read();
	sprintf(saved_alert.msg, "%04d", id);
	fAlertNtfReq= TRUE;
	return;
}

void pending_data_upload_delay_timeout_handler(xTimerHandle pxTimer)
{
	/* 時刻合わせ済みであれば送信処理実施 */
	if (SNTP_STAT_TIME_SET == sntp_get_state()) {
		fPendingDataUploadReq = TRUE;	/* 保留データ送信要求フラグ ON */
	}
}

/* 保留データ送信要求 */
void pending_data_upload_req(void)
{
	Log_Notify("\r\n[data_upload][pending_] requested\r\n");
	/* "PENDING_DATA_UPLOAD_DELAY"で指定した時間待ってから保留データを送信 */
	if (!pending_data_upload_delay_timer_created) {
		pending_data_upload_delay_timer = xTimerCreate("PENDING_DATA_UPLOAD_DELAY",
														(PENDING_DATA_UPLOAD_DELAY / portTICK_RATE_MS),
														pdFALSE,
														(void *)0,
														pending_data_upload_delay_timeout_handler);
		xTimerStart(pending_data_upload_delay_timer, 0);
		pending_data_upload_delay_timer_created = TRUE;
	}
	else {
		xTimerReset(pending_data_upload_delay_timer, 0);
	}
	return;
}

#if CONFIG_AUDREY_BACKUP_ENTRY
void data_upload_entering_backup(void)
{
	int idx;

	if(entry_backup.cnt >= 0 && entry_backup.cnt <= ENTRY_BACKUP_MAX) {
		/* バックアップ件数フルの場合は最古データ削除 */
		if(entry_backup.cnt == ENTRY_BACKUP_MAX) {
			memmove(&entry_backup.buf[0], &entry_backup.buf[1], (ENTRY_BACKUP_MAX - 1)*sizeof(ENTRY_BUF));
			memmove(&entry_backup.bd[0], &entry_backup.bd[1], (ENTRY_BACKUP_MAX - 1)*sizeof(BD_ADDR_LEN));
			entry_backup.cnt--;
			Log_Info("\r\n[data_upload][backup_entry] Remove old data\r\n");
		}
		if (buf_idx_write == 0) {
			idx = ENTERING_DATA_BUFF_LENGTH;
		} else {
			idx = buf_idx_write - 1;
		}
		/* 同一日時のデータがバックアップされている場合はスキップ */
		for(int i = 0; i < entry_backup.cnt; i++) {
			if(entry_backup.buf[i].time == entering_data_buf[idx].time) {
				Log_Info("\r\n[data_upload][backup_entry] Find same backup data\r\n");
				return;
			}
		}
		entry_backup.buf[entry_backup.cnt].time = entering_data_buf[idx].time;
		/* 温度と滞在時間を纏めてバックアップ */
		entry_backup.buf[entry_backup.cnt].stay_tem
		 = (entering_data_buf[idx].tem < 0 ? 0 : entering_data_buf[idx].tem << 16 & 0xffff0000)
		 | (entering_data_buf[idx].stay < 0 ? 0 : entering_data_buf[idx].stay & 0x0000ffff);
		/* 体重と尿量を纏めてバックアップ */
		entry_backup.buf[entry_backup.cnt].weight
		 = (entering_data_buf[idx].urine < 0 ? 0 : entering_data_buf[idx].urine << 16 & 0x7fff0000)
		 | (entering_data_buf[idx].body < 0 ? 0 : entering_data_buf[idx].body & 0x0000ffff)
		 | (entering_data_buf[idx].flag == 0 ? 0 : 0x80000000);
		memcpy(&entry_backup.bd[entry_backup.cnt], &entering_data_buf[idx].bd, BD_ADDR_LEN);
		entry_backup.cnt++;
		memcpy(entry_backup.magic, ENTRY_MAGIC_NUM, sizeof(ENTRY_MAGIC_NUM));
		device_mutex_lock(RT_DEV_LOCK_FLASH);
		flash_erase_sector(&entry_flash, ENTRY_FALSH);
		flash_stream_write(&entry_flash, ENTRY_FALSH, sizeof(entry_backup), (uint8_t *)&entry_backup);
		device_mutex_unlock(RT_DEV_LOCK_FLASH);
		Log_Notify("\r\n[data_upload][backup_entry] Save (total:%d)\r\n", entry_backup.cnt);
	}
}

void delay_entry_backup_timeout_handler(xTimerHandle pxTimer)
{
	Log_Info("\r\n[data_upload][backup_entry] Timeout delay timer\r\n");
	xTimerStop(delay_entry_backup_timer, 0);
	if(fEntryWaitConnect) {
		fEntryWaitConnect = FALSE;
		data_upload_entering_backup();
	}
}

void delay_entry_backup_stop(void)
{
	fEntryWaitConnect = FALSE;
	xTimerStop(delay_entry_backup_timer, 0);
}

void delay_entry_backup(void)
{
	Log_Info("\r\n[data_upload][backup_entry] Start delay timer\r\n");
	fEntryWaitConnect = TRUE;
	/* "DELAY_ENTRY_BACKUP"で指定した時間待ってから入退室データをバックアップ */
	if (!delay_entry_backup_timer_created) {
		delay_entry_backup_timer = xTimerCreate("DELAY_ENTRY_BACKUP",
												(DELAY_ENTRY_BACKUP / portTICK_RATE_MS),
												pdFALSE,
												(void *)0,
												delay_entry_backup_timeout_handler);
		xTimerStart(delay_entry_backup_timer, 0);
		delay_entry_backup_timer_created = TRUE;
	}
	else {
		xTimerReset(delay_entry_backup_timer, 0);
	}
}
#endif /* CONFIG_AUDREY_BACKUP_ENTRY */

void check_ssl_clear(void)
{
	if(dataup_ssl_fail_cnt != 0) {
		Log_Info("[data_upload][ssl_fail] clear count\r\n");
	}
	dataup_ssl_fail_cnt = 0;
}

void check_ssl_fail(void)
{
	if(dataup_ssl_fail_flg) {
		/* SSL失敗(NET_CONNECT_FAILED)が一定回数連続で続いた場合はログ保存のみ行う */
		dataup_ssl_fail_flg = FALSE;
#if CONFIG_AUDREY_UPLOAD_RETRY
		if(periodic_retry_cnt != 0 || dataup_fail_cnt != 0) return;
#endif
		dataup_ssl_fail_cnt++;
		Log_Notify("[data_upload][ssl_fail] round %d\r\n", dataup_ssl_fail_cnt);
		if(dataup_ssl_fail_cnt > SSL_FAIL_COUNT_MAX) {
			dataup_ssl_fail_cnt = 0;
			SendMessageToStateManager(MSG_ERR_DUL, PARAM_DUL_ERR_SSL_FAIL);
//			SendMessageToStateManager(MSG_ERR_DUL_FATAL, PARAM_DUL_ERR_SSL_FAIL);
		}
		return;
	}
	check_ssl_clear();
}

#if CONFIG_AUDREY_UPLOAD_RETRY
int is_upload_retry(void)
{
	return(periodic_retry_cnt | dataup_fail_cnt);
}

void check_periodic_retry(void)
{
	if(periodic_retry_cnt != 0) {
		Log_Info("[data_upload][periodic_] retry %d time left\r\n", periodic_retry_cnt);
		periodic_timer_start(PERIODIC_UPLOAD_DELAY_RETRY);
	} else if(dataup_fail_cnt != 0) {
		Log_Info("[data_upload][fail] retry %d time left\r\n", dataup_fail_cnt);
		periodic_timer_start(DATA_UPLOAD_FAIL);
	}
}
#endif

/* 入退室後データ送信処理 実行関数 */
static void entry_data_upload_proc(void)
{
	upload_data		*updata;
	float			temp = 0;
	WEIGHT_ENTRY	entry_data;
	int				i = 0;
	int				j = 0;

#if CONFIG_AUDREY_BACKUP_ENTRY
	/* 既にバックアップ待ちデータがある場合は先にバックアップしておく */
	if(fEntryWaitConnect) {
		Log_Info("\r\n[data_upload][backup_entry] Backup previous data\r\n");
		delay_entry_backup_stop();
		data_upload_entering_backup();
	}
#endif /* CONFIG_AUDREY_BACKUP_ENTRY */

	/* Scale監視側の測定データ(グローバル変数)を読み出し */
	memcpy(&entry_data, &weight_entry, sizeof(entry_data));
	/* 温度情報取得 */
	temp = get_temperature();
	Log_Info("[data_upload][entry_] time = %ld, body = %d, urine = %d, temp = %f\r\n",
			entry_data.time, entry_data.body, entry_data.urine, temp);
	Log_Info("[data_upload][entry_] stay = %d\r\n",
			entry_data.stay);

#if CONFIG_AUDREY_REDUCE_MALLOC
	/* writeポインタがreadポインタの直後にある場合、readポインタを進めてバッファを開ける */
	if ((buf_idx_write == buf_idx_read - 1) ||
		((buf_idx_write == ENTERING_DATA_BUFF_LENGTH) && (buf_idx_read == 0))) {
		if (buf_idx_read == ENTERING_DATA_BUFF_LENGTH) {
			buf_idx_read = 0;
		} else {
			buf_idx_read++;
		}
		Log_Info("\r\n[data_upload][entry_] entering_data_buf[] full. oldest data deleted. \r\n");
	}
	/* 送信データ編集 */
#if CONFIG_AUDREY_DBG_UPLOAD
	entering_data_buf[buf_idx_write].id = (!is_dbg_mode() ? TYPE_ID_ENTERING : TYPE_ID_ENTERING_DBG);
#else
	entering_data_buf[buf_idx_write].id = TYPE_ID_ENTERING;
#endif
	entering_data_buf[buf_idx_write].body = entry_data.body;
	entering_data_buf[buf_idx_write].urine = entry_data.urine;
	entering_data_buf[buf_idx_write].flag = entry_data.flag;
	entering_data_buf[buf_idx_write].tem = (int)(temp + 0.5);	/* 端数切り上げ */
	memcpy(&entering_data_buf[buf_idx_write].mac, &audrey_mac, sizeof(audrey_mac));
	entering_data_buf[buf_idx_write].stay = entry_data.stay;
	entering_data_buf[buf_idx_write].time = entry_data.time;
	memcpy(&entering_data_buf[buf_idx_write].bd, entry_data.bd_addr, BD_ADDR_LEN);
	entering_data_buf[buf_idx_write].low.num = badge_low_num;
	for (i = 0 ; i < badge_low_num ; i++) {
		memcpy(&entering_data_buf[buf_idx_write].low.bd[i], badge_low_info[i], BD_ADDR_LEN);
	}
	/* writeポインタ更新 */
	if (buf_idx_write == ENTERING_DATA_BUFF_LENGTH) {
		buf_idx_write = 0;
	} else {
		buf_idx_write++;
	}
#else //#if CONFIG_AUDREY_REDUCE_MALLOC
	/* 送信データ編集 */
	updata = malloc(sizeof(upload_data));
	if (updata == NULL) {
		Log_Error("\r\n[data_upload][entry_] malloc() failed\r\n");
		SendMessageToStateManager(MSG_ERR_DUL_FATAL, PARAM_COMMON_NOMEM);
		return;
	}
#if CONFIG_AUDREY_DBG_UPLOAD
	updata->id = (!is_dbg_mode() ? TYPE_ID_ENTERING : TYPE_ID_ENTERING_DBG);
#else
	updata->id = TYPE_ID_ENTERING;
#endif
	updata->body = entry_data.body;
	updata->urine = entry_data.urine;
	updata->tem = (int)(temp + 0.5);	/* 端数切り上げ */
	memcpy(updata->mac, audrey_mac, sizeof(updata->mac));
	updata->stay = entry_data.stay;
	updata->time = entry_data.time;
	memcpy(updata->bd, entry_data.bd_addr, sizeof(updata->bd));
	updata->low.num = badge_low_num;
	for (i = 0 ; i < badge_low_num ; i++) {
		memcpy(updata->low.bd[i], badge_low_info[i], BD_ADDR_LEN);
	}

	/* writeポインタがreadポインタの直後にある場合、readポインタを進めてバッファを開ける */
	if ((buf_idx_write == buf_idx_read - 1) ||
		((buf_idx_write == ENTERING_DATA_BUFF_LENGTH) && (buf_idx_read == 0))) {
		if (buf_idx_read == ENTERING_DATA_BUFF_LENGTH) {
			buf_idx_read = 0;
		} else {
			buf_idx_read++;
		}
		Log_Info("\r\n[data_upload][entry_] entering_data_buf[] full. oldest data deleted. \r\n");
	}
	/* バッファに保存 */
	memcpy(&entering_data_buf[buf_idx_write], updata, sizeof(upload_data));
	/* writeポインタ更新 */
	if (buf_idx_write == ENTERING_DATA_BUFF_LENGTH) {
		buf_idx_write = 0;
	} else {
		buf_idx_write++;
	}
	free(updata);
#endif //#if CONFIG_AUDREY_REDUCE_MALLOC

	/* データ送信可否チェック  */
	if (rf_ctrl_check_connection() != RF_ERR_SUCCESS) {
		/* 送信不可時は何もせずに終了 */
		Log_Info("\r\n[data_upload][entry_] Entry data put into buffer. But upload skipped.\r\n");
		/* 一定時間後にバックアップ */
		delay_entry_backup();
		goto exit;
	} else {
		/* データ送信要求 */
#if CONFIG_AUDREY_UPLOAD_RETRY
		if(periodic_retry_cnt == 0 && dataup_fail_cnt == 0) {
			dataup_fail_cnt = DATA_UPLOAD_RETRY_MAX;
		}
#endif
#if CONFIG_AUDREY_BACKUP_ENTRY
		delay_entry_backup_stop();
		if(rf_ctrl_data_upload(&entering_data_buf[buf_idx_read]) != RF_ERR_SUCCESS) {
			data_upload_entering_backup();
#if CONFIG_AUDREY_UPLOAD_RETRY
			check_periodic_retry();
#endif
			check_ssl_fail();
		} else {
			check_ssl_clear();
		}
#else /* CONFIG_AUDREY_BACKUP_ENTRY */
		rf_ctrl_data_upload(&entering_data_buf[buf_idx_read]);
#endif /* CONFIG_AUDREY_BACKUP_ENTRY */
	}

exit:
	return;
}

static void alert_notification_proc(void)
{
	/* writeポインタがreadポインタの直後にある場合、readポインタを進めてバッファを開ける */
	if ((alert_ntf_buf_idx_write == alert_ntf_buf_idx_read - 1) ||
		((alert_ntf_buf_idx_write == ALERT_NTF_BUFF_LENGTH) && (alert_ntf_buf_idx_read == 0))) {
		if (alert_ntf_buf_idx_read == ALERT_NTF_BUFF_LENGTH) {
			alert_ntf_buf_idx_read = 0;
		} else {
			alert_ntf_buf_idx_read++;
		}
		Log_Info("\r\n[data_upload][alert_] alert_ntf_buf[] full. oldest data deleted. \r\n");
	}
	/* バッファに保存 */
	memcpy(&alert_ntf_buf[alert_ntf_buf_idx_write], &saved_alert, sizeof(upload_data));
	/* writeポインタ更新 */
	if (alert_ntf_buf_idx_write >= ALERT_NTF_BUFF_LENGTH) {
		alert_ntf_buf_idx_write = 0;
	} else {
		alert_ntf_buf_idx_write++;
	}

	/* データ送信可否チェック  */
	if (rf_ctrl_check_connection() != RF_ERR_SUCCESS) {
		/* 送信不可時は何もせずに終了 */
		Log_Info("\r\n[data_upload][alert_] Alert notification put into buffer. But upload skipped.\r\n");
		goto exit;
	} else {
		/* データ送信要求 */
		rf_ctrl_data_upload(&alert_ntf_buf[alert_ntf_buf_idx_read]);
	}

exit:
	return;
}

/* 無線接続状態遷移通知 (保留データ送信要求) */
void data_upload_rf_state_changed(enum rf_state state)
{
	uint32_t current_time = 0;
	uint32_t time_diff = 0;

	switch (state) {
		case RF_STATE_WLAN:
			/* 通知された無線通信の状態を記憶 */
			rf_type = state;
			break;
		case RF_STATE_BLE:
			/* 通知された無線通信の状態を記憶 */
			rf_type = state;
			break;
		case RF_STATE_NONE:
			/* 通知された無線通信の状態を記憶 */
			rf_type = state;
			/* no break */
		default:
			/* WLAN, BLE接続時以外は何もせずに終了 */
			return;
	}

	/* 接続確立時、 バッファに保持してあるデータの送信を実施 */
	/* 接続確立時、 バッファに保持してあるデータの送信を実施 */
	if ((rf_type == RF_STATE_WLAN) || (rf_type == RF_STATE_BLE)) {
		/* 接続確立をトリガとした定期送信は、タイマによる定期送信から所定の間隔以上時間経過している場合のみ行う */
		current_time = rtc_read();
		time_diff = current_time - last_upload_by_timer;
		Log_Info("\r\n[data_upload][info__] last_upload=%u, current_time=%u, diff=%u\r\n",
				last_upload_by_timer, current_time, time_diff);
		if ((last_upload_by_timer == 0) || (time_diff > PERIODIC_UPLOAD_DELAY / 1000)) {
			fPeriodicUploadRequest = TRUE;	/* 定期送信要求フラグ ON */
			fPeriodicUploadByTimer = FALSE;	/* 接続確立契機なのでタイマ契機定期送信識別フラグはOFF */
		}
		if (SNTP_STAT_TIME_SET == sntp_get_state()) {	/* 時刻合わせ済みであれば送信処理実施 */
			fPendingDataUploadReq  = TRUE;	/* 保留データ送信要求フラグ ON */
		}
	}

	return;
}

/* アラート通知送信結果 */
void data_upload_alert_result(upload_resp *data)
{
	if (data == NULL) {
		Log_Error("\r\n[data_upload][alert_result] Receive response error\r\n");
		/* 応答受信失敗時はそのまま終了 */
		return;
	}

	Log_Info("\r\n[data_upload][alert_result] result = %d \r\n", data->result);
	if (data->result == UPLOAD_RESULT_FAIL) {
		/* 送信結果＝失敗時は readポインタを進めずに終了 → 次のタイミングで再送 */
		return;
	}

	/* 送信成功の応答を受けたら、バッファのreadポインタを進める */
	if (data->result == UPLOAD_RESULT_SUCCESS) {
		if (alert_ntf_buf_idx_read == ALERT_NTF_BUFF_LENGTH) {
			alert_ntf_buf_idx_read = 0;
		} else {
			alert_ntf_buf_idx_read++;
		}
		fPendingDataUploadReq = TRUE;	/* 保留データ送信要求フラグ ON */
	}

	return;
}

/* 入退室後データ送信結果 */
void data_upload_entering_result(upload_resp *data)
{
	if (data == NULL) {
		Log_Error("\r\n[data_upload][entering_result] Receive response error\r\n");
		/* 応答受信失敗時は readポインタを進めずに終了 → 次のタイミングで再送 */
		goto end_resp_err;
	}

	Log_Info("\r\n[data_upload][entering_result] result = %d \r\n", data->result);
	if (data->result == UPLOAD_RESULT_FAIL) {
		/* 送信結果＝失敗時は readポインタを進めずに終了 → 次のタイミングで再送 */
		goto end_resp_err;
	}

#if CONFIG_AUDREY_BACKUP_ENTRY
	int i;

	/* 送信成功時にバックアップデータがあれば削除 */
	if(entry_backup.cnt > 0 && entry_backup.cnt <= ENTRY_BACKUP_MAX) {
		/* 同一日時のバックアップデータを検索 */
		for(i = 0; i < entry_backup.cnt; i++) {
			if(entering_data_buf[buf_idx_read].time == entry_backup.buf[i].time) break;
		}
		if(i < entry_backup.cnt) {
			/* バックアップデータ一件削除 */
			memmove(&entry_backup.buf[i], &entry_backup.buf[i+1], (entry_backup.cnt - i)*sizeof(ENTRY_BUF));
			memmove(&entry_backup.bd[i], &entry_backup.bd[i+1], (entry_backup.cnt - i)*sizeof(BD_ADDR_LEN));
			entry_backup.cnt--;
			Log_Info("\r\n[data_upload][backup_entry] Remove entry data(total:%d)\r\n", entry_backup.cnt);
			/* バックアップデータが０件になればErase */
			if (entry_backup.cnt == 0) {
				device_mutex_lock(RT_DEV_LOCK_FLASH);
				flash_erase_sector(&entry_flash, ENTRY_FALSH);
				flash_stream_write(&entry_flash, ENTRY_FALSH, sizeof(entry_backup), (uint8_t *)&entry_backup);
				device_mutex_unlock(RT_DEV_LOCK_FLASH);
				Log_Info("\r\n[data_upload][backup_entry] Erase entry data\r\n");
			}
		}
	}
#endif /* CONFIG_AUDREY_BACKUP_ENTRY */
#if CONFIG_AUDREY_UPLOAD_RETRY
	dataup_fail_cnt = 0;
#endif
	/* 送信成功の応答を受けたら、バッファのreadポインタを進める */
	if (data->result == UPLOAD_RESULT_SUCCESS) {
		if (buf_idx_read == ENTERING_DATA_BUFF_LENGTH) {
			buf_idx_read = 0;
		} else {
			buf_idx_read++;
		}
		fPendingDataUploadReq = TRUE;	/* 保留データ送信要求フラグ ON */
	}
	return;

end_resp_err:
#if CONFIG_AUDREY_BACKUP_ENTRY
	delay_entry_backup_stop();
	data_upload_entering_backup();
#endif /* CONFIG_AUDREY_BACKUP_ENTRY */
#if CONFIG_AUDREY_UPLOAD_RETRY
	check_periodic_retry();
#endif
	return;

}

/* 定期送信データ送信結果 */
void data_upload_periodic_result(upload_resp *data)
{
	int			i = 0;
	char		scale_ver[VERINF_LENGTH_MAX + 1] = {0};
	char		wireless_ver[VERINF_LENGTH_MAX + 1] = {0};
	BADGE_CONF	bd_conf;
	int			is_update;

	if (data == NULL) {
		Log_Error("\r\n[data_upload][periodic_result] Receive response error\r\n");
		goto end_resp_err;
	}

	Log_Info("\r\n[data_upload][periodic_result] result = %d \r\n", data->result);
	if (data->result == UPLOAD_RESULT_FAIL) {
		goto end_resp_err;
	} else if ((last_upload_by_timer == 0) || (fPeriodicUploadByTimer == TRUE)) {
			/* タイマ契機の定期送信の場合は時刻を記憶 */
			/* 最終送信時刻が0 （＝起動後初回送信 or 送信失敗後の送信）の場合はフラグに関係なく時刻を記憶 */
			last_upload_by_timer = rtc_read();
	}

	/* この関数が呼ばれている時点で id=2 のはずだが、念のためチェック */
	if (data->id == TYPE_ID_PERIODIC) {
		Log_Info("\r\n[data_upload][periodic_result] ver = %s \r\n", data->ver);
#if CONFIG_AUDREY_INDV_EMU == 0
		badge_init();
#endif /* CONFIG_AUDREY_INDV_EMU */
		for (i = 0 ; i < data->petnum ; i++) {
			Log_Info("\r\n[data_upload][periodic_result] bd = %02x:%02x:%02x:%02x:%02x:%02x  body = %d\r\n",
					data->petdata[i].bd[0], data->petdata[i].bd[1], data->petdata[i].bd[2],
					data->petdata[i].bd[3], data->petdata[i].bd[4], data->petdata[i].bd[5], data->petdata[i].body);
			/* バッジ情報設定 */
			memcpy(bd_conf.bd_addr, data->petdata[i].bd, sizeof(bd_conf.bd_addr));
			bd_conf.body = data->petdata[i].body;
#if CONFIG_AUDREY_INDV_EMU == 0
			badge_config(&bd_conf);
#endif /* CONFIG_AUDREY_INDV_EMU */
		}
#if CONFIG_AUDREY_BACKUP_BADGE
		bool backup_exe = FALSE;
		/* バックアップしているバッジ情報数に変化があれば更新 */
		if(data->petnum == 1 && memcmp(data->petdata[0].bd, "\0\0\0\0\0\0", BD_ADDR_LEN) == 0) {
			data->petnum = 0;
		}
		/* バックアップしているバッジアドレスと比較し、バッジ数が違えば更新 */
		if(((badge_backup.cnt_bd > 0 && badge_backup.cnt_bd <= BADGE_PET_MAX) && data->petnum != badge_backup.cnt_bd)
		 || (!(badge_backup.cnt_bd > 0 && badge_backup.cnt_bd <= BADGE_PET_MAX) && data->petnum != 0)) {
			Log_Info("\r\n[data_upload][backup_badge] Change badge num\r\n");
			for(i = 0; i < data->petnum; i++) {
				memcpy(&badge_backup.buf[i].bd_addr, data->petdata[i].bd, BD_ADDR_LEN);
				badge_backup.buf[i].body = data->petdata[i].body;
			}
			badge_backup.cnt_bd = data->petnum;
			backup_exe = TRUE;
		/* バックアップしているバッジアドレスと比較し、情報に変更があれば更新 (体重は±５％以上の変化があれば更新) */
		} else if((badge_backup.cnt_bd > 0 && badge_backup.cnt_bd <= BADGE_PET_MAX) && data->petnum != 0) {
			for(i = 0; i < data->petnum; i++) {
				if(memcmp(&badge_backup.buf[i].bd_addr, data->petdata[i].bd, BD_ADDR_LEN) != 0
				|| (badge_backup.buf[i].body > (data->petdata[i].body * 105) / 100)
				|| (badge_backup.buf[i].body < (data->petdata[i].body * 95) / 100)
				) {
					Log_Info("\r\n[data_upload][backup_badge] Change badge info\r\n");
					Log_Debug("upper:%d, lower:%d, backup:%d\r\n"
					, (data->petdata[i].body * 105) / 100, (data->petdata[i].body * 95) / 100, badge_backup.buf[i].body);
					memcpy(&badge_backup.buf[i].bd_addr, data->petdata[i].bd, BD_ADDR_LEN);
					badge_backup.buf[i].body = data->petdata[i].body;
					backup_exe = TRUE;
				}
			}
		}
		/* バッジ情報が更新されていればFlash保存 */
		if(backup_exe) {
			memcpy(badge_backup.magic, BADGE_MAGIC_NUM, sizeof(BADGE_MAGIC_NUM));
			device_mutex_lock(RT_DEV_LOCK_FLASH);
			flash_erase_sector(&badge_flash, BADGE_FALSH);
			flash_stream_write(&badge_flash, BADGE_FALSH, sizeof(badge_backup), (uint8_t *)&badge_backup);
			device_mutex_unlock(RT_DEV_LOCK_FLASH);
			Log_Notify("\r\n[data_upload][backup_badge] Save (total:%d)\r\n", badge_backup.cnt_bd);
			for(i = 0; i < badge_backup.cnt_bd; i++) {
				Log_Info("BD:%02x%02x%02x%02x%02x%02x, body:%d\r\n"
				, badge_backup.buf[i].bd_addr[0], badge_backup.buf[i].bd_addr[1], badge_backup.buf[i].bd_addr[2]
				, badge_backup.buf[i].bd_addr[3], badge_backup.buf[i].bd_addr[4], badge_backup.buf[i].bd_addr[5]
				, badge_backup.buf[i].body);
			}
		}
#endif /* CONFIG_AUDREY_BACKUP_BADGE */
		/* バージョン情報通知 (ファーム更新要否チェック) */
		for (i = 0 ; i < VERINF_LENGTH_MAX ; i++) {
			if (data->ver[i] == '_') {
				strncpy(wireless_ver, &(data->ver[0]), i);
				strcpy(scale_ver, &(data->ver[i+1]));
			}
		}
#ifndef FW_UPDATE_DISABLE
#if CONFIG_AUDREY_SKIP_VERCHK
		if(periodic_retry_cnt == 0) {
			is_update = ota_set_new_version(scale_ver, wireless_ver);
		} else {
			is_update = 0;
		}
#else
		is_update = ota_set_new_version(scale_ver, wireless_ver);
#endif
#endif
#if CONFIG_AUDREY_DBG_UPLOAD
#if CONFIG_AUDREY_DBG_BLE
		if(rf_type == CONNECTION_TYPE_WLAN || rf_type == CONNECTION_TYPE_BLE) {
#else
		if(rf_type == CONNECTION_TYPE_WLAN) {
#endif
#if CONFIG_AUDREY_DBG_FORCE
			/* 本define有効時はサーバー対応されるまで定期送信成功後に強制的にデバッグモードにする */
			fDbgMode = 1;
#else /* CONFIG_AUDREY_DBG_FORCE */
			fDbgMode = data->dbg;
#endif /* CONFIG_AUDREY_DBG_FORCE */
		} else {
			fDbgMode = 0;
		}
#endif /* CONFIG_AUDREY_DBG_UPLOAD */
	}

end_normal:
	SendMessageToStateManager(MSG_UPLOAD_OK, PARAM_NONE);
	/* タイマ契機定期送信識別フラグをクリア */
	fPeriodicUploadByTimer = FALSE;
	/* 保留データ送信要求フラグ ON */
	fPendingDataUploadReq = TRUE;
#if CONFIG_AUDREY_LOG_UPLOAD
	if(is_update == 0) {
		dbg_errlog_upload_req();
	}
#endif
	periodic_retry_cnt = 0;
	dataup_fail_cnt = 0;
	return;

end_resp_err:
	/* 応答受信失敗 or resultがfailの時 は再送しない */
//	fPeriodicUploadRequest = TRUE;
	/* 最終送信時刻をクリア (次回無線接続確立時に必ず送信させるため) */
//	last_upload_by_timer = 0;
	/* タイマ契機定期送信識別フラグをクリア */
	fPeriodicUploadByTimer = FALSE;
	/* 起動直後の失敗時は一定回数送信間隔を短くする */
#if CONFIG_AUDREY_UPLOAD_RETRY
	check_periodic_retry();
#else
	if(periodic_retry_cnt != 0) {
		Log_Info("[data_upload][periodic_] retry %d time left\r\n", periodic_retry_cnt);
		periodic_timer_start(PERIODIC_UPLOAD_DELAY_RETRY);
		periodic_retry_cnt--;
	}
#endif
	return;
}

/* 定期送信データ送信処理 実行関数 */
static void periodic_data_upload_proc(void)
{
#if CONFIG_AUDREY_REDUCE_MALLOC
	upload_data periodic_data;
#else //#if CONFIG_AUDREY_REDUCE_MALLOC
	upload_data *updata;
#endif //#if CONFIG_AUDREY_REDUCE_MALLOC
	float temp = 0;
	WEIGHT_INFO	info_data;
	int	separator_pos = 0;
	RF_CTRL_ERR ret = RF_ERR_SUCCESS;

	/* Scale初期測定状態確認 */
	if (scale_is_init() != 0) {
		Log_Info("\r\n[data_upload][info__] scale initializing in progress -> Pending Upload\r\n");
		goto end_resend;
	}

	/* データ読み出し可否チェック */
	if (weight_is_entry()) {
		Log_Info("\r\n[data_upload][info__] weight_is_entry() == TRUE -> Pending Upload\r\n");
		goto end_resend;
	}

	/* 次回送信用のタイマを設定 */
	if (xTimerIsTimerActive(periodic_timer) == pdFALSE) {
		/* タイマが未設定の場合のみタイマをスタート */
		periodic_timer_start(0);
	}

	/* Scale監視側の測定データ(グローバル変数)を読み出し */
	memcpy(&info_data, &weight_info, sizeof(info_data));
	/* 温度情報取得 */
	temp = get_temperature();
	Log_Info("[data_upload][info__] time = %d, body = %d, urine = %d, temp = %f\r\n",
			info_data.time, info_data.body, info_data.urine, temp);

	/* データ送信可否チェック  */
	ret = rf_ctrl_check_connection();
	if (ret != RF_ERR_SUCCESS) {
		Log_Info("\r\n[data_upload][info__] connection check NG -> Pending Upload\r\n");
		if (ret == RF_ERR_RETRY) {
			/* リトライが要求された場合は短時間のタイマを設定しなおす */
			periodic_timer_start(PERIODIC_UPLOAD_DELAY_SHORT);
		}
		goto end_resend;
	} else {
#if CONFIG_AUDREY_REDUCE_MALLOC
		/* 送信データ編集 */
		Log_Info("\r\n[data_upload][periodic_] start\r\n");
		periodic_data.id = TYPE_ID_PERIODIC;
		periodic_data.body = info_data.body;
		periodic_data.urine = info_data.urine;
		periodic_data.tem = (int)(temp + 0.5);	/* 端数切り上げ */
		memcpy(&periodic_data.mac, &audrey_mac, sizeof(audrey_mac));
		periodic_data.time = rtc_read();	/* time は Scale側から取得した値ではなく現在時刻を取得して設定 */
		strcpy(&periodic_data.ver[0], AUDREY_VERSION);
		separator_pos = strlen(&periodic_data.ver);
		periodic_data.ver[separator_pos] = '_';
		strcpy(&periodic_data.ver[separator_pos + 1], scale_ver);
		periodic_data.conn = (rf_type == RF_STATE_WLAN ? CONNECTION_TYPE_WLAN : CONNECTION_TYPE_BLE);
		periodic_data.init = (periodic_retry_cnt == 0 ? 0 : 1);
#if CONFIG_AUDREY_UPLOAD_RETRY
		if(periodic_retry_cnt != 0) {
			periodic_retry_cnt--;
		} else {
			if(dataup_fail_cnt == 0) {
				dataup_fail_cnt = DATA_UPLOAD_RETRY_MAX;
			} else {
				dataup_fail_cnt--;
			}
		}
#endif
		/* データ送信要求 */
		ret = rf_ctrl_data_upload(&periodic_data);
		if(ret != RF_ERR_SUCCESS && periodic_retry_cnt == 0) {
			check_ssl_fail();
		} else {
			check_ssl_clear();
		}
#else //#if CONFIG_AUDREY_REDUCE_MALLOC
		updata = malloc(sizeof(upload_data));
		if (updata == NULL) {
			Log_Error("\r\n[data_upload][info__] malloc() failed\r\n");
			SendMessageToStateManager(MSG_ERR_DUL_FATAL, PARAM_COMMON_NOMEM);
			return;
		}
		/* 送信データ編集 */
		Log_Info("\r\n[data_upload][periodic_] start\r\n");
		updata->id = TYPE_ID_PERIODIC;
		updata->body = info_data.body;
		updata->urine = info_data.urine;
		updata->tem = (int)(temp + 0.5);	/* 端数切り上げ */
		memcpy(updata->mac, audrey_mac, sizeof(updata->mac));
		updata->time = rtc_read();	/* time は Scale側から取得した値ではなく現在時刻を取得して設定 */
		strcpy(updata->ver, AUDREY_VERSION);
		separator_pos = strlen(updata->ver);
		updata->ver[separator_pos] = '_';
		strcpy(&(updata->ver[separator_pos + 1]), scale_ver);
		updata->conn = (rf_type == RF_STATE_WLAN ? CONNECTION_TYPE_WLAN : CONNECTION_TYPE_BLE);
		/* データ送信要求 */
		ret = rf_ctrl_data_upload(updata);
		free(updata);
#endif //#if CONFIG_AUDREY_REDUCE_MALLOC
		if (ret != RF_ERR_SUCCESS) {
			goto end_resend;
		}
	}

end_normal:
	/* 定期送信要求フラグをクリア*/
	fPeriodicUploadRequest = FALSE;
	return;

end_resend:	/* 送信ができなかった場合 */
	/* 定期送信要求フラグをON */
	fPeriodicUploadRequest = TRUE;
	/* タイマ契機定期送信識別フラグをクリア */
	fPeriodicUploadByTimer = FALSE;
	/* 起動直後の失敗時は一定回数送信間隔を短くする */
#if CONFIG_AUDREY_UPLOAD_RETRY
	check_periodic_retry();
#else
	if(periodic_retry_cnt != 0) {
		Log_Info("[data_upload][periodic_] retry %d time left\r\n", periodic_retry_cnt);
		periodic_timer_start(PERIODIC_UPLOAD_DELAY_RETRY);
		periodic_retry_cnt--;
	}
#endif
	return;
}

/* 保留送信データチェック＆送信要求処理 */
static void pending_upload_check(void)
{
	RF_CTRL_ERR ret = RF_ERR_SUCCESS;

	Log_Info("\r\n[data_upload][pending_check] start pending data check\r\n");
	/* データ送信可否チェック  */
	ret = rf_ctrl_check_connection();
	if (ret != RF_ERR_SUCCESS) {
		Log_Info("\r\n[data_upload][pending_check] data upload skipped\r\n");
//		if (ret == RF_ERR_RETRY) {
//			/* リトライが要求された場合は短時間のタイマを設定しなおす */
//			periodic_timer_start(PERIODIC_UPLOAD_DELAY_SHORT);
//		}
#if CONFIG_AUDREY_BACKUP_ENTRY
		if(buf_idx_read != buf_idx_write) {
			/* 一定時間後にバックアップ */
			delay_entry_backup();
		}
#endif /* CONFIG_AUDREY_BACKUP_ENTRY */
		goto exit;
	}
	/* 保留されている入退室後データがあれば送信 (初回定期送信未完了時を除く) */
	if (buf_idx_read != buf_idx_write && periodic_retry_cnt == 0) {
		Log_Info("\r\n[data_upload][pending_check] buf_idx_read %d, buf_idx_write %d\r\n", buf_idx_read, buf_idx_write);
#if CONFIG_AUDREY_UPLOAD_RETRY
		if(dataup_fail_cnt == 0) {
			dataup_fail_cnt = DATA_UPLOAD_RETRY_MAX;
		} else {
			dataup_fail_cnt--;
		}
#endif
#if CONFIG_AUDREY_BACKUP_ENTRY
		delay_entry_backup_stop();
		if(rf_ctrl_data_upload(&entering_data_buf[buf_idx_read]) != RF_ERR_SUCCESS) {
			data_upload_entering_backup();
#if CONFIG_AUDREY_UPLOAD_RETRY
			check_periodic_retry();
#endif
			check_ssl_fail();
		} else {
			check_ssl_clear();
		}
#else /* CONFIG_AUDREY_BACKUP_ENTRY */
		rf_ctrl_data_upload(&entering_data_buf[buf_idx_read]);
#endif /* CONFIG_AUDREY_BACKUP_ENTRY */
		goto exit;
	}
	/* 保留されているアラート通知があれば送信 */
	if (alert_ntf_buf_idx_read != alert_ntf_buf_idx_write) {
		Log_Info("\r\n[data_upload][pending_check] alert_ntf_buf_idx_read %d, alert_ntf_buf_idx_write %d\r\n", alert_ntf_buf_idx_read, alert_ntf_buf_idx_write);
		rf_ctrl_data_upload(&alert_ntf_buf[alert_ntf_buf_idx_read]);
		goto exit;
	}
	/* 保留中のデータが残っていなければ定期送信を実施 */
	if (fPeriodicUploadRequest == TRUE) {
		periodic_data_upload_proc();
		goto exit;
	}
#if CONFIG_AUDREY_DBG_UPLOAD
	/* デバッグデータ(重量)があれば送信 */
	if (dbg_w_buf_idx_read != dbg_w_buf_idx_write) {
		dbg_weight_data updata;
		Log_Info("\r\n[data_upload][pending_check] dbg_w_buf_idx_read %d, dbg_w_buf_idx_write %d\r\n", dbg_w_buf_idx_read, dbg_w_buf_idx_write);
		memcpy(&updata, &dbg_w_data_buf[dbg_w_buf_idx_read], sizeof(updata));
		rf_ctrl_dbg_w_data_upload(&updata);
		goto exit;
	}
	/* デバッグデータ(beacon)があれば送信 */
	if (dbg_b_buf_idx_read != dbg_b_buf_idx_write) {
		dbg_beacon_data updata;
		Log_Info("\r\n[data_upload][pending_check] dbg_b_buf_idx_read %d, dbg_b_buf_idx_write %d\r\n", dbg_b_buf_idx_read, dbg_b_buf_idx_write);
		memcpy(&updata, &dbg_b_data_buf[dbg_b_buf_idx_read], sizeof(updata));
		rf_ctrl_dbg_b_data_upload(&updata);
		goto exit;
	}
	/* デバッグデータ(rssi)があれば送信 */
	if (dbg_r_buf_idx_read != dbg_r_buf_idx_write) {
		dbg_rssi_data updata;
		Log_Info("\r\n[data_upload][pending_check] dbg_r_buf_idx_read %d, dbg_r_buf_idx_write %d\r\n", dbg_r_buf_idx_read, dbg_r_buf_idx_write);
		memcpy(&updata, &dbg_r_data_buf[dbg_r_buf_idx_read], sizeof(updata));
		rf_ctrl_dbg_r_data_upload(&updata);
		goto exit;
	}
#endif // CONFIG_AUDREY_DBG_UPLOAD

	Log_Info("\r\n[data_upload][pending_check] no data remaining\r\n");

exit:
	Log_Info("\r\n[data_upload][pending_check] pending data check end\r\n");
	return;
}

void periodic_timeout_handler(xTimerHandle pxTimer)
{
	fPeriodicShortTimer = FALSE;
	/* 定期送信要求フラグ と 保留データ送信要求フラグ を ON
	 * -> 送信処理は data_upload_thread() で実施 */
	fPeriodicUploadRequest = TRUE;
	fPendingDataUploadReq  = TRUE;

	/* タイマ契機の定期送信時は識別フラグON */
	fPeriodicUploadByTimer = TRUE;

	/* 定期送信タイマ再設定 */
	periodic_timer_start(0);
}

static void periodic_timer_start(uint32_t delay)
{
#ifndef TIMER_DELAY_FOR_DEV
	uint32_t period_sec, now_sec;
#endif
	/* 短時間タイマ満了状態設定 */
	if (delay == PERIODIC_UPLOAD_DELAY_SHORT) {
		fPeriodicShortTimer = TRUE;
	} else {
		fPeriodicShortTimer = FALSE;
	}
	/* delay == 0 のときは自動設定 */
	if (delay == 0) {
		delay = PERIODIC_UPLOAD_DELAY;
#ifndef TIMER_DELAY_FOR_DEV
		/* 次回送信タイミングまでの時間計算 */
		u32 t = rtc_read();
		period_sec  = periodic_upload_hour   * 60 * 60;
		period_sec += periodic_upload_minute * 60;
		period_sec += periodic_upload_sec;
		now_sec = t % (PERIODIC_UPLOAD_DELAY_HOUR * 60 * 60);
		if (period_sec > now_sec) {
			delay = (period_sec - now_sec) * 1000;
		} else {
			delay = PERIODIC_UPLOAD_DELAY - ((now_sec - period_sec) * 1000);
		}
#endif
	}
	Log_Info("\r\n[data_upload][info__] Next upload is %02u:%02u:%02u later.\r\n",
			 (delay / 3600000), ((delay % 3600000)/ 60000), ((delay % 3600000) % 60000) /1000);

	if (xTimerIsTimerActive(periodic_timer) != pdFALSE) {
		xTimerStop(periodic_timer, 0);
	}
	xTimerChangePeriod(periodic_timer, (delay / portTICK_RATE_MS), 0);

	return;
}

void data_up_req_start(void)
{
	is_data_up_req = TRUE;
	xTimerReset(data_up_req_timer, 0);
	Log_Info("[data_upload][request] Start\r\n");
}

void data_up_req_end(void)
{
	is_data_up_req = FALSE;
	xTimerStop(data_up_req_timer, 0);
	Log_Info("[data_upload][request] End\r\n");
}

void data_up_req_timeout_handler(xTimerHandle pxTimer)
{
	Log_Info("[data_upload][request] Time out\r\n");
	data_up_req_end();
}

static void data_upload_thread(void *param)
{
	u32	mac = 0;	/* MACアドレス (定期送信時刻計算用) */
	WEIGHT_ENTRY entry_data;
	uint32_t delay = PERIODIC_UPLOAD_DELAY;
#ifndef TIMER_DELAY4DEV
	struct tm tm_now;
#endif

	rtw_msleep_os(20);

	/* WDTタスクへタスク起動を通知 */
	wdt_task_watch(WDT_TASK_DATAUP, WDT_TASK_WATCH_START);

	/* MACアドレス取得用wifi ON */
	rf_ctrl_wifi_mac();
	/* 定期送信時刻計算 */
	/* 奇数MACアドレスのみの運用となるため２で割った値から算出する（MACアドレス -1 ＝ BDアドレス） */
	mac = (audrey_mac[3] << 16 | audrey_mac[4] << 8 | audrey_mac[5]) / 2;
	periodic_upload_hour   = mac % PERIODIC_UPLOAD_DELAY_HOUR;
	periodic_upload_minute = (((mac / PERIODIC_UPLOAD_DELAY_HOUR) % 60) % 10 * 6) + (((mac / PERIODIC_UPLOAD_DELAY_HOUR) % 60) / 10);
	periodic_upload_sec    = ((((mac / PERIODIC_UPLOAD_DELAY_HOUR) / 60) % 60) % 10 * 6) + ((((mac / PERIODIC_UPLOAD_DELAY_HOUR) / 60) % 60) / 10);
	Log_Notify("[data_upload][init__] addr=%02x:%02x:%02x:%02x:%02x:%02x, hour=%d, min=%d, sec=%d\r\n",
			audrey_mac[0], audrey_mac[1], audrey_mac[2], audrey_mac[3], audrey_mac[4], audrey_mac[5],
			periodic_upload_hour, periodic_upload_minute, periodic_upload_sec);

	/* リクエスト～レスポンスタイマ生成 */
	data_up_req_timer = xTimerCreate("DATA_UP_REQ",(DATA_UP_REQ / portTICK_RATE_MS),pdFALSE,(void *)0,data_up_req_timeout_handler);

	/* 定期送信用タイマ生成 */
	periodic_timer = xTimerCreate("PERIODIC_UPLOAD_DELAY",(delay / portTICK_RATE_MS), pdFALSE, (void *)0, periodic_timeout_handler);

#if CONFIG_AUDREY_BACKUP_ENTRY || CONFIG_AUDREY_BACKUP_BADGE
	device_mutex_lock(RT_DEV_LOCK_FLASH);
#if CONFIG_AUDREY_BACKUP_ENTRY
	flash_stream_read(&entry_flash, ENTRY_FALSH,  sizeof(entry_backup), (uint8_t *)&entry_backup);
	if(memcmp(entry_backup.magic, ENTRY_MAGIC_NUM, sizeof(ENTRY_MAGIC_NUM)) == 0
	 && entry_backup.cnt > 0 && entry_backup.cnt <= ENTRY_BACKUP_MAX) {
	 	/* 入退室情報のリストア(電池残量低下情報を除く) */
		for(int i = 0; i < entry_backup.cnt; i++) {
#if CONFIG_AUDREY_DBG_UPLOAD
			entering_data_buf[i].id = (!is_dbg_mode() ? TYPE_ID_ENTERING : TYPE_ID_ENTERING_DBG);
#else
			entering_data_buf[i].id = TYPE_ID_ENTERING;
#endif
			entering_data_buf[i].time = entry_backup.buf[i].time;
			entering_data_buf[i].stay = entry_backup.buf[i].stay_tem & 0x0000ffff;
			entering_data_buf[i].tem = (entry_backup.buf[i].stay_tem >> 16) & 0x0000ffff;
			entering_data_buf[i].body = entry_backup.buf[i].weight & 0x0000ffff;
			entering_data_buf[i].urine = (entry_backup.buf[i].weight >> 16) & 0x00007fff;
			entering_data_buf[i].flag = ((entry_backup.buf[i].weight & 0x80000000) == 0 ? 0 : 1);
			memcpy(&entering_data_buf[i].bd, &entry_backup.bd[i], BD_ADDR_LEN);
			memcpy(&entering_data_buf[i].mac, audrey_mac, sizeof(audrey_mac));
		}
		buf_idx_write = entry_backup.cnt;
		Log_Notify("\r\n[data_upload][backup_entry] Restore (total:%d)\r\n", entry_backup.cnt);
	} else {
		entry_backup.cnt = 0;
		Log_Info("\r\n[data_upload][backup_entry] Empty entry data\r\n");
	}
#endif /* CONFIG_AUDREY_BACKUP_BADGE */
#if CONFIG_AUDREY_BACKUP_BADGE
	flash_stream_read(&badge_flash, BADGE_FALSH,  sizeof(badge_backup), (uint8_t *)&badge_backup);
	if(memcmp(badge_backup.magic, BADGE_MAGIC_NUM, sizeof(BADGE_MAGIC_NUM)) == 0
	&& badge_backup.cnt_bd > 0 && badge_backup.cnt_bd <= BADGE_PET_MAX) {
	/* バッジ情報のリストア */
		badge_init();
		for(int i = 0; i < badge_backup.cnt_bd; i++) {
			BADGE_CONF badge_buf;
			memcpy(&badge_buf.bd_addr, &badge_backup.buf[i].bd_addr, BD_ADDR_LEN);
			badge_buf.body = badge_backup.buf[i].body;
			badge_config(&badge_buf);
		}
		Log_Notify("\r\n[data_upload][backup_badge] Restore (total:%d)\r\n", badge_backup.cnt_bd);
	} else {
		badge_backup.cnt_bd = 0;
		Log_Info("\r\n[data_upload][backup_badge] Empty badge info\r\n");
	}
#endif /* CONFIG_AUDREY_BACKUP_BADGE */
	device_mutex_unlock(RT_DEV_LOCK_FLASH);
#endif /* CONFIG_AUDREY_BACKUP_ENTRY || CONFIG_AUDREY_BACKUP_BADGE*/

	while (1) {
		vTaskDelay(200 * portTICK_PERIOD_MS);

		/* WDTタスクへタスク生存を通知 */
		wdt_task_refresh(WDT_TASK_DATAUP);

		/* 時刻合わせ状態チェック */
		if ((fPeriodicTimer == FALSE) && (SNTP_STAT_TIME_SET == sntp_get_state())) {
			/* 定期送信タイマスタート */
			if (fPeriodicUploadRequest == TRUE) {
				/* 初回定期送信が保留されている場合、すぐに送信されるようにタイマ設定 */
				periodic_timer_start(PERIODIC_UPLOAD_DELAY_SHORT);
#if CONFIG_AUDREY_CONF_BACKUP
				log_record_backup_wlan_conf();
#endif
			} else {
				/* 初回定期送信が保留されていない場合、規定時刻までの残秒数でタイマ設定 */
				periodic_timer_start(0);
			}
			fPeriodicTimer = TRUE;
		}

		/* データ送信要求チェック */
		if(is_data_up_req == FALSE) {
			fDataUploadCheck = TRUE;	/* データ送信処理チェック中フラグ ON */
			if (fEntryUploadReq == TRUE) {
					Log_Info("\r\n[data_upload][entry_] start\r\n");
					/* 入退室後データ送信処理 */
					entry_data_upload_proc();
					/* 入退室後データ送信要求受信フラグクリア */
					fEntryUploadReq = FALSE;
			} else if (fAlertNtfReq == TRUE) {
					Log_Info("\r\n[data_upload][alert_] start\r\n");
					/* アラート通知送信処理 */
					alert_notification_proc();
					/* アラート通知要求受信フラグクリア */
					fAlertNtfReq = FALSE;
#if CONFIG_AUDREY_DBG_UPLOAD
			} else if (fDbgWeightUploadReq == TRUE) {
					Log_Info("\r\n[data_upload][dbg][weight] start debug data (weight) upload\r\n");
					/* デバッグデータ(重量)送信処理 */
					debug_weight_data_upload_proc();
					/* デバッグデータ(重量)送信要求受信フラグクリア */
					fDbgWeightUploadReq = FALSE;
			} else if (fDbgBeaconUploadReq == TRUE) {
					Log_Info("\r\n[data_upload][dbg][beacon] start debug data (beacon) upload\r\n");
					/* デバッグデータ(beacon)送信処理 */
					debug_beacon_data_upload_proc();
					/* デバッグデータ(beacon)送信要求受信フラグクリア */
					fDbgBeaconUploadReq = FALSE;
			} else if (fDbgRssiUploadReq == TRUE) {
					Log_Info("\r\n[data_upload][dbg][beacon] start debug data (rssi) upload\r\n");
					/* デバッグデータ(rssi)送信処理 */
					debug_rssi_data_upload_proc();
					/* デバッグデータ(rssi)送信要求受信フラグクリア */
					fDbgRssiUploadReq = FALSE;
#endif
#if CONFIG_AUDREY_LOG_UPLOAD
			} else if (fLogErrorUploadReq == TRUE) {
					/* エラーログ送信処理 */
					log_error_upload_proc();
					/* エラーログ送信要求受信フラグクリア */
					fLogErrorUploadReq = FALSE;
			} else if (fLogHfUploadReq == TRUE) {
					/* HardHaultError送信処理 */
					log_hf_upload_proc();
					/* HardHaultError送信要求受信フラグクリア */
					fLogHfUploadReq = FALSE;
#endif
			} else if (fPendingDataUploadReq == TRUE && fPeriodicShortTimer == FALSE) {
					/* 保留送信データチェック＆送信要求処理 */
				pending_upload_check();
				/* 保留データ送信要求フラグクリア */
				fPendingDataUploadReq = FALSE;
			}
			fDataUploadCheck = FALSE;	/* データ送信処理チェック中フラグ OFF */
		}
	}

	/* WDTタスクへタスク停止を通知 */
	wdt_task_watch(WDT_TASK_DATAUP, WDT_TASK_WATCH_NONE);

	vTaskDelete(NULL);
}

/* データ送信処理実施状況 チェック関数
 *   TRUE　:データ送信処理中
 *   FALSE:データ送信処理未実施
 *
 *   Note:
 * 		データ送信の応答待ち中の状態を含まないため、
 * 		FALSEの場合でも応答受信待ち状況の確認は別途行うこと
 * */
bool is_data_upload_in_progress(void)
{
	return fDataUploadCheck;
}

void data_upload_init(void)
{
	if(xTaskCreate(data_upload_thread, ((const char*)"data_upload_thread"), 2048, NULL, tskIDLE_PRIORITY + 4 , NULL) != pdPASS) {
		Log_Error("\n\r%s xTaskCreate(data_upload_thread) failed", __FUNCTION__);
	}
}
