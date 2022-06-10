/*
 * Copyright (C) 2017 SHARP Corporation. All rights reserved.
 */
#ifndef BADGE_H_
#define BADGE_H_

#include "bt_gap.h"
#include "bt_util.h"

#define BADGE_PET_MAX			   10		// 多頭飼最大数
#define BADGE_RCV_MAX			   10		// beacon受信最大数
#if CONFIG_AUDREY_INDV
#define BADGE_RSSI_HIGHER_NUM	   7+1		// beacon MAX値 格納数（7 + 1(配列操作する為に余計に+1している)）
#define BADGE_RSSI_HIGHER_SHORT_NUM	3		// beacon MAX値 上位3点
#define BADGE_RSSI_HIGHER_LONG_NUM	7		// beacon MAX値 上位7点
#define BADGE_RSSI_OUT_NUM	   	   13		// beacon 退出 格納数 (TBD)
#endif	/* CONFIG_AUDREY_INDV */

typedef enum {
	BADGE_TYPE_ENTRY = 0,					// 入室中beacon取得
	BADGE_TYPE_EXIT,						// 退室中beacon取得
	BADGE_TYPE_MAX,
} BADGE_TYPE;

typedef struct {
	u8		bd_addr[BD_ADDR_LEN];			// BDアドレス
	int		body;							// 体重
}BADGE_CONF;

typedef struct {
	char	rssi[BADGE_RCV_MAX];			// RSSI
	char	cnt;							// beacon受信回数
	int		dis;							// 分散
	char	val;							// 中央値
#if CONFIG_AUDREY_INDV
	char	rssi_higher_array[BADGE_RSSI_HIGHER_NUM];	// 上位値7つ格納
	char	rssi_higher_avg;							// 上位3平均
	char	rssi_higher_long_avg;						// 上位7平均
	char	rssi_out_array[BADGE_RSSI_OUT_NUM];			// 退出値
	char	rssi_out_cnt;								// 退出時のカウント
	char	rssi_out_diff;								// 退出時の差分(絶対値)
#endif	/* CONFIG_AUDREY_INDV */
}BADGE_BUF;

typedef struct {
	BADGE_CONF	conf;						// 設定情報
	BADGE_BUF	buf[BADGE_TYPE_MAX];		// 受信情報、演算結果
	char		battery;					// バッテリー情報
	char		score;						// スコア
#if CONFIG_AUDREY_INDV
	char		point;						// ポイント
	char		exclution_flg;				// 除外猫判定(0:採用、1:除外)
#endif	/* CONFIG_AUDREY_INDV */
#if CONFIG_AUDREY_INDV_EMU_LOG
	char		rssi_in_point;
	char		weight_point;
	char		rssi_out_point;
#endif /* CONFIG_AUDREY_INDV_EMU_LOG */
}BADGE_DATA;

extern int badge_config_cnt;				// バッジ登録数
extern BADGE_DATA badge_data[BADGE_PET_MAX];			// 各バッジ情報(体重、RSSI、固体識別判定結果)
extern u8 badge_low_info[BADGE_PET_MAX][BD_ADDR_LEN];	// 低電池残量バッジBDアドレス
extern int badge_low_num;					// 低電池残量バッジ数

void badge_init(void);						// バッジ情報初期化
void badge_bt_set(int);						// 個体識別判定時 BT ON/OFF
void badge_config(BADGE_CONF *);			// バッジ登録
void badge_beacon_start(BADGE_TYPE);		// beacon受信開始(入室時/退室時の情報を引数指定)
void badge_beacon_end(BADGE_TYPE);			// beacon受信停止(入室時/退室時の情報を引数指定)
void badge_beacon_rcv(u8 *, char, char);	// beacon受信時処理
void badge_judge(int, u8 *);				// 固体識別判定

#if CONFIG_AUDREY_DBG_UPLOAD
#define BADGE_DBG_NUM_MAX		   10		// デバッグデータ用多頭飼最大数
#define BADGE_DBG_RSSI_MAX		   120+10	// デバッグデータ用RSSI受信最大数

typedef struct {
	u32		time;							// 日時
	char	rssi;							// RSSI値
}BADGE_DBG_BUF;

typedef struct {
	u8				bd_addr[BD_ADDR_LEN];	// BDアドレス
	int				cnt;					// RSSI受信数
	BADGE_DBG_BUF	buf[BADGE_DBG_RSSI_MAX];
}BADGE_DBG_DATA;

extern BADGE_DBG_DATA badge_dbg[BADGE_DBG_NUM_MAX];
extern int badge_dbg_cnt;
#endif

#endif /* BADGE_H_ */
