#ifndef STATE_MANAGER_H
#define STATE_MANAGER_H


#include "portmacro.h"


/* メッセージIDの型 */
#define	MSG_ID_t	u16
/* メッセージ付加パラメータの型 */
#define	MSG_Param_t	u16
/* エラーコード */
#define	ERROR_CODE	(0xffff)
/* メッセージ用タイムアウト時間 */
#define	QUEUE_TIME_OUT			(0xffffffffUL)
/* メッセージ処理後のディレイ時間 */
#define	THREAD_DELAY_TIME		(20)
/* LED 3分タイマ */
#define	TIMER_M_3_LED		(3 * 60 * 1000)


/* 状態定義 */
typedef enum StateManager_state {
	STATE_IDLE			= 0,		/* 初期状態 */
	STATE_WIFI_AP,					/* WiFi APモード状態 */
	STATE_WIFI_WPS,					/* WiFi WPS中状態 */
	STATE_WIFI_CONNECT_WAIT,		/* WiFi接続待ち状態 */
	STATE_WIFI_CONNECT_COMPLETE,	/* WiFi接続完了状態 */
	STATE_WIFI_FW_UPDATE,			/* WiFi接続FW update中状態 */
	STATE_BT_CONNECT_WAIT,			/* BT接続待ち状態 */
	STATE_BT_CONNECT_COMPLETE,		/* BT接続完了状態 */
	STATE_BT_FW_UPDATE,				/* BT接続FW update中状態 */
	STATE_BT_SCAN,					/* Scan実行中状態 */
	STATE_BT_CONF,					/* BT接続初期設定状態 */
#if CONFIG_AUDREY_FWUP_ALONE
	STATE_FWUP_SA_AP,				/* WiFi ファーム更新(stand-alone) AP状態 */
#endif
	STATE_MAX
} STATE_TYPE;

/* メッセージID */
typedef enum MessageID {
	MSG_NONE = 0x00,				/* ダミー */
	MSG_RESET,						/* リセット要求 */
	MSG_LINKKEY_PUSH,				/* Key５秒押下通知 */
	MSG_WEBUI_WPS_START,			/* WebUI WPS実行通知 */
	MSG_WEBUI_SET_SSID,				/* WebUI SSID入力通知 */
	MSG_WEBUI_SET_BLE_MODE,			/* WebUI モード切り替え通知 */
	MSG_WIFI_LINK_COMP,				/* WiFi接続完了通知 */
	MSG_WIFI_LINK_OFF,				/* WiFi切断通知 */
	MSG_BT_DATA_GET_COMP,			/* BT接続情報取得完了通知 */
	MSG_BT_LINK_COMP,				/* BT接続完了通知 */
	MSG_BT_LINK_OFF,				/* BT切断通知 */
	MSG_UPLOAD_DATA,				/* データアップロード要求 */
	MSG_FW_UPDATE_START,			/* FW update開始要求 */
	MSG_FW_UPDATE_COMP,				/* FW update完了通知 */
	MSG_FW_UPDATE_STOP,				/* FW update中断（再起動しない）通知 */
	MSG_FW_UPDATE_AGEIN,			/* FW update再開要求 */
	MSG_RSSI_START,					/* RSSI検出開始通知 */
	MSG_RSSI_END,					/* RSSI検出終了通知 */
	MSG_DIAG_COMMAND,				/* Diagコマンド要求 */
	MSG_WIFI_INIT_COMP,				/* WiFi初期化完了通知 */
	MSG_WIFI_RECCONECT_FAIL,		/* Wifi再接続失敗通知 */
	MSG_WIFI_DHCP_FAIL,				/* WifiDHCP失敗通知 */
	MSG_LINKKEY_SHORT,				/* Key短押下通知 */
	MSG_UPLOAD_OK,					/* 定期送信成功通知 */
	MSG_BT_DATA_UPLOAD_ENABLE,		/* BT経由データアップロード有効化通知 */
#if CONFIG_AUDREY_FWUP_ALONE
	MSG_FW_UPDATE_SA_START,			/* FW update(stand-alone)開始要求 */
	MSG_FW_UPDATE_SA_OK,			/* FW update(stand-alone)完了 */
	MSG_FW_UPDATE_SA_FAIL,			/* FW update(stand-alone)失敗 */
	MSG_FW_UPDATE_SA_VER,			/* FW update(stand-alone)Verチェック */
#endif

	/* その他 */
	MSG_OTHER = 0x40,				/* その他系 */
	MSG_TIMER_M_1,					/* １分間タイマ満了通知 */
	MSG_TIMER_M_3_LED,				/* LED 3分タイマ満了通知 */
	MSG_TIMER_M_60,					/* 定期送信タイマ満了通知 */

	/* メッセージ記録用 */
	MSG_RECORD = 0x80,				/* メッセージ記録用 */
	MSG_SCALE_ENTRY,				/* 入退室による重量測定完了 */

	/* エラー系(WiFi接続エラーコード) */
	MSG_ERR_RSP = 0xa0,				/* WiFi接続レスポンスエラー(下３桁はレスポンスコード) */

	/* エラー系(記録&アラート通知) */
	MSG_ERR = 0xc0,					/* エラー通知(記録のみ) */
	MSG_ERR_WIFI,					/* WiFi系エラー通知 */
	MSG_ERR_WIFI_LINK,				/* WiFi接続エラー通知 */
	MSG_ERR_BT,						/* BT系エラー通知 */
	MSG_ERR_BT_LINK,				/* BT接続エラー通知 */
	MSG_ERR_DUL,					/* データアップロード関連エラー */
	MSG_ERR_SCAN,					/* SCAN系エラー通知 */
	MSG_ERR_SCALE,					/* Scale通信関連エラー */
	MSG_ERR_FWUPDATE,				/* FW updateエラー通知(再起動不要) */
	MSG_ERR_WEBSERVER,				/* webserverエラー通知 */
	MSG_ERR_SCALE_W,				/* Scale通信関連エラー(Wireless側検出) */

	/* エラー系(記録&再起動) */
	MSG_ERR_REBOOT = 0xe0,			/* エラー通知(記録&再起動) */
	MSG_ERR_WIFI_UPDATE,			/* WiFi接続FW updateエラー通知 */
	MSG_ERR_BT_UPDATE,				/* BT接続FW updateエラー通知 */
	MSG_ERR_SCAN_FATAL,				/* SCAN系致命的エラー通知 */
	MSG_ERR_SCALE_FATAL,			/* Scale通信致命的エラー通知 */
	MSG_ERR_DUL_FATAL,				/* データアップロード致命的エラー通知 */
	MSG_ERR_WIFI_LINK_FATAL,		/* WiFi接続致命的エラー通知 */
	MSG_ERR_BT_LINK_FATAL,			/* BT接続致命的エラー通知 */

	MSG_RESTORE_CONF_RESET_0 = 0xf7,	/* 無線設定リストア(security_type=0) */
	MSG_RESTORE_CONF_RESET = 0xf8,	/* 無線設定リストア */
	MSG_ERASE_CONF_RESET = 0xf9,	/* 無線設定初期化 */
	MSG_STACK_OVER_RESET = 0xfa,	/* スタックオーバーフロー */
	MSG_MALLOC_FAIL_RESET = 0xfb,	/* ヒープ確保失敗 */
	MSG_OTA_RESET = 0xfc,			/* ファーム更新完了 (記録のためMSG_FW_UPDATE_COMPではなくこちらを使用)*/
	MSG_TASK_RESET = 0xfd,			/* 一定時間タスク処理なし */
	MSG_SELF_RESET = 0xfe,			/* 定期再起動 */
	MSG_ERR_MAX = 0xff,				/* エラー通知の定義はここまで */

	/* 試験用 */
	MSG_CHANGE_STATE	= 0xff00,	/* 強制状態遷移要求 */
} MSG_TYPE;

/* メッセージ付加パラメータ */
enum {
	PARAM_NONE			= 0,		/* パラメータ無し */

	/* 汎用エラー */
	PARAM_COMMON_PARAM	= 1,		/* 引数エラー */
	PARAM_COMMON_BUSY,				/* リソース使用中 */
	PARAM_COMMON_FAULT,				/* アドレス不正 */
	PARAM_COMMON_IO,				/* 入出力エラー */
	PARAM_COMMON_NETDOWN,			/* ネットワーク不通 */
	PARAM_COMMON_NOMEM,				/* 空きメモリー領域不十分 */
	PARAM_COMMON_TASK_CREATE_FAIL,	/* タスク生成失敗 */
	PARAM_COMMON_STATE,				/* 状態不正 */
};

enum {
	/* WiFi接続エラー */
	PARAM_WIFI_ERR_DHCP = 21,				/* DHCP失敗 */
	PARAM_WIFI_ERR_DISC,					/* Disconnectエラー */
	PARAM_WIFI_ERR_BEACON,					/* Beacon停止後エラー */
	PARAM_WIFI_ERR_CONNECT = 30,			/* Connectエラー */
};

enum {
	/* Scale関連エラー */
	PARAM_SCL_ERR_ADIC_BODY = 21,			/* Scale Initial Body AD IC fail */
	PARAM_SCL_ERR_ADIC_URINE = 22,			/* Scale Initial Urine AD IC fail */
	PARAM_SCL_ERR_OVERLOAD_BODY = 23,		/* Scale Body overload */
	PARAM_SCL_ERR_OVERLOAD_URINE = 24,		/* Scale Urine overload */
	PARAM_SCL_ERR_ADCHIGH_BODY = 25,		/* Scale Body AD count too high */
	PARAM_SCL_ERR_ADCHIGH_URINE = 26,		/* Scale Urine AD count too high */
	PARAM_SCL_ERR_ADCLOW_BODY = 27,			/* Scale Body AD count too low */
	PARAM_SCL_ERR_ADCLOW_URINE = 28,		/* Scale Urine AD count too low */
};

enum {
	/* Scale関連エラー */
	PARAM_SCL_ERR_WEIGHT_NONE = 1,			/* Scale 重量測定実施不可 */
	PARAM_SCL_ERR_NO_RSP_VER,				/* Scale 応答コマンド未受信(バージョン情報) */
	PARAM_SCL_ERR_NO_RSP_WEIGHT,			/* Scale 応答コマンド未受信(重量値) */
	PARAM_SCL_ERR_NO_RSP_THR,				/* Scale 応答コマンド未受信(閾値設定) */
	PARAM_SCL_ERR_NO_RSP_GRAVITY,			/* Scale 応答コマンド未受信(重量加速度) */
	PARAM_SCL_ERR_UPDATE_FAIL_RES,			/* Scale ファーム更新失敗(レスポンス異常) */
	PARAM_SCL_ERR_UPDATE_FAIL_TO,			/* Scale ファーム更新失敗(レスポンスタイムアウト) */
	PARAM_SCL_ERR_UPDATE_FAIL_TO2,			/* Scale ファーム更新失敗(更新完了タイムアウト) */
};

enum {
	/* データアップロード関連エラー */
	PARAM_DUL_ERR = 21,						/* データアップロード関連エラー */
	PARAM_DUL_ERR_SERVER,					/* データアップロード サーバー接続エラー */
	PARAM_DUL_ERR_RSP,						/* データアップロード 応答エラー */
	PARAM_DUL_ERR_COMM,						/* データアップロード 通信エラー
											   (異常データ受信, 関数エラーreturn 等) */
	PARAM_DUL_ERR_SSL,						/* データアップロード SSL設定エラー */
	PARAM_DUL_ERR_RESP_TOUT,				/* データアップロード 応答受信待ちタイムアウト */
	PARAM_DUL_ERR_SSL_FAIL,					/* データアップロード SSL接続連続失敗 */
	PARAM_DUL_ERR_SERVER_RETRY,				/* データアップロード サーバー接続エラー(リトライ) */
	PARAM_DUL_ERR_SERVER_ENTRY,				/* データアップロード サーバー接続エラー(入退室) */
	PARAM_DUL_ERR_SERVER_ENTRY_R,			/* データアップロード サーバー接続エラー(入退室リトライ) */
	PARAM_DUL_ERR_SERVER_PERIOD,			/* データアップロード サーバー接続エラー(定期送信) */
	PARAM_DUL_ERR_SERVER_PERIOD_R,			/* データアップロード サーバー接続エラー(定期送信リトライ) */
	PARAM_DUL_ERR_RESP_ERR,					/* データアップロード レスポンスエラー */
	PARAM_DUL_ERR_REQ_ERR,					/* データアップロード リクエストエラー */
	PARAM_DUL_ERR_REQ_ERR_B,				/* データアップロード リクエストエラー(Body) */
	PARAM_DUL_ERR_JSON = 50,				/* データアップロード JSON編集エラー */
};

enum {
	/* BT SCAN系エラー */
	PARAM_SCAN_STATUS_ERROR	= 21,			/* BT_SCAN ステータスエラー */
	PARAM_SCAN_CB_ERROR,					/* BT_SCAN コールバックエラー */
	PARAM_SCAN_NOT_READY,					/* BT_SCAN NOT READY */
	PARAM_SCAN_FATAL_ERROR,					/* BT_SCAN FATAL エラー */
	PARAM_SCAN_INITIALIZING_FATAL_ERROR,	/* BT_SCAN ON FATALエラー */
	PARAM_SCAN_DEINITIALIZING_FATAL_ERROR,	/* BT_SCAN OFF FATALエラー */
};

enum {
	/* BT接続系エラー */
	PARAM_BT_LINK_ERR = 21,					/* BT接続系エラー */
	PARAM_BT_LINK_INDICATE_INCOMPLETE,		/* BT接続 indication送信エラー */
	PARAM_BT_LINK_DISC_INCOMPLETE,			/* BT接続 切断処理エラー */
	PARAM_BT_LINK_POWER_OFF_INCOMPLETE,		/* BT接続 GattServer停止エラー */
};

enum {
	/* FW update系エラー */
	PARAM_FW_UPDATE_IMG_BIN_LENGTH_OVER	= 21,			/* FW update imgファイル長オーバー　エラー */
	PARAM_FW_UPDATE_INVALID_OTA_ADDRESS_ERROR,					/* FW update 書き込みフラッシューメモリーアドレス　エラー */
	PARAM_FW_UPDATE_IMG_FILE_LENGTH_ERROR,					/* FW update ファイル長　エラー */
	PARAM_FW_UPDATE_MALLOC_ERROR,					/* FW update malloc エラー */
	PARAM_FW_UPDATE_CREATE_SOCKET_ERROR,	/* FW update ソケット生成エラー */
	PARAM_FW_UPDATE_SEM_NEW_ERROR,	/* FW update セマフォ生成エラー */
	PARAM_FW_UPDATE_HTTP_RESPONSE_CODE_ERROR, /* FW update HTTP レスポンスコード　エラー */
	PARAM_FW_UPDATE_HTTP_CONTENT_LENGTH_ERROR,  /* FW update HTTP コンテンツレングス　エラー */
	PARAM_FW_UPDATE_JSON_ERROR, /* FW update JSONエラー*/
	PARAM_FW_UPDATE_WIFI_NOT_READY,  /*  FW update WIFI not ready */
	PARAM_FW_UPDATE_SSL_WRITE_REQUEST_ERROR, /* SSL リクエストエラー */
	PARAM_FW_UPDATE_READ_ERROR, /* FW update read エラー */
	PARAM_FW_UPDATE_GET_CONTENT_LEBGTH_ERROR, /* HTTP RSP コンテントレングス取得　エラー */
	PARAM_FW_UPDATE_IMG_SIZE_ZERO, /* イメージサイズ　ゼロ */
	PARAM_FW_UPDATE_FRASH_WRITE_ERROR, /* FLASH書き込みエラー */
	PARAM_FW_UPDATE_RECV_SMALL_PACKET, /* Recv small packet */
	PARAM_FW_UPDATE_CHECKSUM_ERROR, /* checksum エラー */
	PARAM_FW_UPDATE_VERSION_ERROR, /* version エラー */
	PARAM_FW_UPDATE_SCALE_DOWNLOAD_ERROR, /* scale download エラー */
	PARAM_FW_UPDATE_WIRELESS_DOWNLOAD_ERROR, /* wireless download エラー */
	PARAM_FW_UPDATE_ALREADY_RUNNING, /* ２重起動 エラー */
	PARAM_FW_UPDATE_TASK_CREAT_ERROR, /* タスク生成 エラー */
	PARAM_FW_UPDATE_TASK_STOP_TIME_OVER, /* タスク停止 エラー */
	PARAM_FW_UPDATE_BLE_WIFI_CONNECTED, /* WiFi使用中 エラー */
	PARAM_FW_UPDATE_BLE_CONNECT_ERROR, /* BLE接続 エラー */
	PARAM_FW_UPDATE_BLE_FILE_READ_ERROR, /* BLE read エラー */
	PARAM_FW_UPDATE_BLE_RESUME_ADDRESS_ERROR, /* BLE レジューム　アドレス不一致 エラー */
	PARAM_FW_UPDATE_BLE_READ_LENGTH_OVER, /* BLE 読み込みバイト数オーバー エラー */
	PARAM_FW_UPDATE_BLE_ALREADY_RUNNING, /* ２重起動 エラー */
	PARAM_FW_UPDATE_BLE_TASK_CREAT_ERROR, /* タスク生成 エラー */
	PARAM_FW_UPDATE_BLE_TASK_STOP_TIME_OVER, /* タスク停止 エラー */
	PARAM_FW_UPDATE_LENGTH_ERROR, /* length エラー */
	PARAM_FW_UPDATE_CRC_ERROR, /* crc エラー */
};

enum {
	/* webserver関連エラー */
	PARAM_WEB_ERR = 21,						/* webserver関連エラー */
	PARAM_WEB_ERR_SCN,						/* スキャン失敗 */
	PARAM_WEB_ERR_SCNTO,					/* スキャンタイムアウト */
};


/* 戻り値定義 */
typedef enum ReturnCode {
	RTN_FAIL			= 0,		/* 失敗 */
	RTN_SUCCESS						/* 成功 */
} RTN_TYPE;

/* 状態遷移用メッセージ構造体 */
typedef struct MainMessage {
	MSG_ID_t	MessageID;
	MSG_Param_t	Param;
} xMainMessage;

#if CONFIG_AUDREY_FWUP_ALONE
#define		STANDALONE_TYPE_SCALE		0x01
#define		STANDALONE_TYPE_WIRELESS	0x02
#define		STANDALONE_TYPE_BOTH		(STANDALONE_TYPE_SCALE + STANDALONE_TYPE_WIRELESS)
#endif

static void StateManager_Idle(void);
static void StateManager(enum MessageID MsgID, MSG_Param_t Param);
static void AnalysisMessage(enum MessageID MsgID, MSG_Param_t Param);
static void ChangeState(enum StateManager_state state);
static enum StateManager_state CheckStateBkUp(void);
static void StateManager_reset(void);
static void TimerStart_LED(void);
static void TimerStop_LED(void);
static void TimerComp_LED(void);
void SendMessageToStateManager(MSG_ID_t MsgID, MSG_Param_t Param);
static xMainMessage ReceiveMessage(void);
static void StateManager_thread(void *param);
void StateManagerInit(void);

int is_state_ble_conf(void);		/* BLE初期設定中チェック  1:BLE初期設定中 */

#endif /* STATE_MANAGER_H */
