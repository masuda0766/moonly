/*
 * Copyright (C) 2017 SHARP Corporation. All rights reserved.
 */
#ifndef WEIGHT_H_
#define WEIGHT_H_

typedef struct {
	u32		time;					// 日時
	int		body;					// 体重
	int		urine;					// 尿量
}WEIGHT_INFO;

typedef struct {
	u32		time;					// 日時
	int		body;					// 体重
	int		urine;					// 尿量
	int		stay;					// 滞在時間
	u8		bd_addr[6];				// BDアドレス
	int		flag;					// 尿量計値が低い状態
//	int		delta;					// 入室時と退室時の差分(最終的に不要か？)
//	int		to_urine;				// 入室から尿開始までの時間(最終的に不要か？)
} WEIGHT_ENTRY;

extern WEIGHT_ENTRY weight_entry;	// 入退室後重量値情報
extern WEIGHT_INFO weight_info;		// 定期送信用重量値情報

int weight_update(void);			// 重量測定
int weight_is_entry(void);			// 入室中判定(0:退室中、0以外:入室中)
int weight_is_cab(void);			// 上部ユニット有無判定(0:無、0以外:有)
void weight_init(void);				// 重量測定初期化

#if CONFIG_AUDREY_DBG_UPLOAD
#define WEIGHT_DBG_MAX			60		// デバッグデータ(重量)保存数
//#define WEIGHT_SCL_LEN			6		// Scale通信情報

typedef struct {
	u32		time;						// 日時
	int		body;						// 体重
	int		urine;						// 尿量
//	u8		scl[WEIGHT_SCL_LEN];		// Scale通信情報
}WEIGHT_DBG;

extern WEIGHT_DBG weight_dbg[WEIGHT_DBG_MAX];
										// デバッグデータ(重量)保存領域
extern u32 badge_dbg_time_in;			// 入室時beacon取得開始時間
extern u32 badge_dbg_time_out;			// 退室時beacon取得開始時間
#endif

#endif /* WEIGHT_H_ */
