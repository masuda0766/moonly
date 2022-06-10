/*
 * Copyright (C) 2017 SHARP Corporation. All rights reserved.
 */
#include <platform_opts.h>
#include <platform/platform_stdlib.h>
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include <sntp/sntp.h>
#include <time.h>
#include <math.h>
#include "scale.h"
#include "weight.h"
#include "badge.h"
#include "data_upload.h"
#include "state_manager.h"

#define NUM_WEIGHT				5		// 重量測定回数 (定期送信用)
#define NUM_BODY_IN				5		// 体重測定回数 (入室時)
#define NUM_BODY_OUT			15		// 体重測定回数 (退室時)
#define NUM_URINE				15		// 尿量測定回数
#define NUM_MEDIAN				NUM_URINE		// 中央値測定用バッファサイズ
#define NUM_STABLE				10		// 入室中の重量安定数
#define TIME_ENTRY				10*60	// 入室タイムアウト時間
#define NUM_STATE1TO2			10		// state1<->state2繰り返し最大数

#define THRESH_REMOVE_VAL		500		// 上部ユニット取り外し 閾値
#define THRESH_REF_VAL			250		// 体重変化 閾値
#define THRESH_ENTRY			500		// 入室中体重変化 閾値
#define THRESH_SD				50		// 標準偏差 閾値
#define THRESH_URINE_ENTRY		250		// 入室前の尿量計正常重量 閾値
#define THRESH_CABI				1800	// 上部ユニット重量
#define WEIGHT_DEF				0x7fffffff	// 重量バッファ初期値

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

typedef struct {
	int body[NUM_WEIGHT];
	int urine[NUM_WEIGHT];
} WEIGHT_BUF;

#if CONFIG_AUDREY_CAB_JUDGE
WEIGHT_STATE mainState = STATE_NOPET_NOCAB;
WEIGHT_STATE prevState = STATE_NOPET_NOCAB;
WEIGHT_STATE prev2State = STATE_NOPET_NOCAB;
#else
WEIGHT_STATE mainState = STATE_NOPET_STABLE;
WEIGHT_STATE prevState = STATE_NOPET_STABLE;
WEIGHT_STATE prev2State = STATE_NOPET_STABLE;
#endif

WEIGHT_ENTRY weight_entry;				// 入退室後重量値情報
WEIGHT_INFO weight_info;				// 定期送信用重量値情報
WEIGHT_BUF weight_buf={0};				// 重量測定用バッファ (定期送信用)
int weight_buf_idx=0;					// 重量用測定用インデックス(入室時)

#if CONFIG_AUDREY_DBG_UPLOAD
WEIGHT_DBG weight_dbg[WEIGHT_DBG_MAX];	// デバッグデータ(重量)保存領域
u32 badge_dbg_time_in;					// 入室時beacon取得開始時間
u32 badge_dbg_time_out;					// 退室時beacon取得開始時間
int weight_dbg_cnt = 0;					// デバッグデータ(重量)保存回数カウンタ
#endif

int body_buf_in[NUM_BODY_IN];			// 体重計測用バッファ(入室時)
int body_buf_idx_in=0;					// 体重計測用インデックス(入室時)
int body_buf_out[NUM_BODY_OUT];			// 体重計測用バッファ(退室時)
int body_buf_idx_out=0;					// 体重計測用インデックス(退室時)

bool is_sd_large_in = FALSE;			// 現在の標準偏差(入室時)
bool is_sd_large_out = FALSE;			// 現在の標準偏差(退室時)
bool is_unstabele_entry = FALSE;		// 入室不安定前の状態(入室安定状態から遷移した場合はTRUE)

int weight_curr_body = 0;				// 現在の体重計値
int weight_prev_body = 0;				// 前回の体重計値
int weight_base_body = 0;				// 退室中の体重計値
int weight_entry_body = 0;				// 退室中の体重計値
int weight_next_body = 0;				// 測定完了までに体重上昇した場合の基準体重バックアップ
int weight_base_backup = 0;				// 入室中再入室時の基準体重バックアップ

typedef struct {
	int body;
	int urine;
	u32 time;
} WEIGHT_STABLE;

WEIGHT_STABLE weight_stable[NUM_STABLE];
int weight_stable_cnt;

int weight_start_body;					// 入室開始時の重量バックアップ
int urine_start_pet;					// 入室開始時の尿量バックアップ
int time_start_entry;					// 入室開始時の日時バックアップ

#if CONFIG_AUDREY_INDV_EMU
u32 time_curr;						// 現在日時
#else
#define time_curr weight_info.time		// 現在日時(定期送信用情報と共用)
#endif
u32 time_entry;							// 入室日時
u32 time_exit;							// 退室日時
u32 time_urine;							// 尿開始日時
int time_flag;							// 日時取得完了フラグ保持領域

int urine_buf[NUM_URINE];				// 尿量計測用バッファ
int urine_buf_idx = 0;	 	 			// 尿量計測用インデックス
int urine_pet;							// 入室時尿量
int urine_nopet;						// 退室時時尿量
int urine_chk_cnt = 0;
int body_zero_cnt = 0;

int state1to2_cnt = 0;					// state1<->state2繰り返し回数カウンタ


#if CONFIG_AUDREY_INDV
extern boolean badge_rssi_get_flg;
#endif

float standardDeviation(int *list, int list_size) {

	float ave=0;
	float var=0;
	int i,j;

	// calc average
	for(i=0;i<list_size;i++) {
		ave += list[i];
	}
	ave = ave / list_size;

	// calc variance
	for(j=0;j<list_size;j++) {
		var += (list[j] - ave) * (list[j] - ave);
	}
	var = var / list_size;

	// return standard deviation
	return sqrt(var);
}


bool checkSdLarge(int *list, int list_size) {

	float sd=0;

	// calc standard deviation
	sd = standardDeviation(list, list_size);

	if ( sd >= THRESH_SD ) {
		return TRUE;

	} else {
		return FALSE;
	}
}

int cmp( const void *p, const void *q) {
	return *(int*)p - *(int*)q;
}

int median(int *list, int list_size) {

	int zero_cnt;
	int idx;
	int median;
	int tempHistory[NUM_MEDIAN]={0};

	for(zero_cnt=0,idx=0; idx < list_size; idx++) {
		if(*(list + idx) == WEIGHT_DEF) {
			zero_cnt++;
		} else {
			tempHistory[idx - zero_cnt] = *(list + idx);
		}
	}
	if(zero_cnt >= list_size) return 0;
	list_size -= zero_cnt;
	qsort(tempHistory, list_size, sizeof(int), cmp);

	if(list_size % 2) {
		idx = (list_size - 1) / 2;
		median = tempHistory[idx];
	} else {
		idx = list_size / 2;
		median = (tempHistory[idx - 1] + tempHistory[idx]) / 2;
	}
	return median;
}

void updateRefWeight( void ) {

	// backup reference weight
	weight_prev_body = weight_curr_body;

	// update reference weight (mean of weight history)
	weight_curr_body = median(body_buf_in, NUM_BODY_IN);
}


void recordWeight( void ) {
	u8 *addr;

	int i, diff, min_diff, pnt;

	// 入室中に入室開始時の重量基準値が切り替わった場合
	if(weight_stable_cnt != 0) {
		// 退室後の体重計値に最も近い基準値を採用
		for(i = 0, min_diff = WEIGHT_DEF, diff = WEIGHT_DEF, pnt = 0; i < weight_stable_cnt; i++) {
			diff = abs(weight_curr_body - weight_stable[i].body);
			if(diff < min_diff) {
				min_diff = diff;
				pnt = i;
			}
		}
		// 現在の体重－最新の基準体重より近い値があればそちらを使用
		if(abs(weight_curr_body - weight_base_body) > min_diff) {
			weight_base_body = weight_stable[pnt].body;
			urine_pet = weight_stable[pnt].urine;
			time_entry = weight_stable[pnt].time;
		} else {
			// 最新の基準体重を採用する場合、再入室バックアップがあればそちらを使用
			if(weight_base_backup != 0) {
				weight_base_body = weight_base_backup;
			}
		}
	}
	// 確定した体重を記録
	weight_entry.time = time_entry;
	weight_entry.urine = ( urine_nopet > urine_pet ? urine_nopet - urine_pet : 0 );
	weight_entry.body = weight_entry_body - weight_base_body - weight_entry.urine;
//	if( weight_curr_body > weight_base_body ) {
//		weight_entry.delta = weight_curr_body - weight_base_body;
//	} else {
//		weight_entry.delta = 0;
//	}
	// 入室前尿量値が一定値以下の場合はトレー異常状態を設定
	weight_entry.flag = ((urine_pet > THRESH_URINE_ENTRY) ? 0 : 1);
	// 滞在時間、尿開始時間取得(入室時と退室時の測定回数の差分を考慮)
	if((time_exit - time_entry) > (NUM_BODY_OUT - NUM_BODY_IN)) {
		weight_entry.stay = time_exit - time_entry - (NUM_BODY_OUT - NUM_BODY_IN);
	} else {
		weight_entry.stay = 1;
	}
//	weight_entry.to_urine = 0;
	// 個体識別判定
#if CONFIG_AUDREY_INDV_EMU == 1
		badge_judge(weight_entry.body, weight_entry.bd_addr);
#endif /* CONFIG_AUDREY_INDV_EMU */
#if CONFIG_AUDREY_BADGE
	// バッジ未登録または日時未取得時は固体識別処理をskip(重量測定結果も未送信)
	if(badge_config_cnt != 0 && time_flag == SNTP_STAT_TIME_SET) {
		badge_judge(weight_entry.body, weight_entry.bd_addr);
	} else {
		Log_Info("[Weight]Skip judge for badge\r\n");
#endif
#if CONFIG_AUDREY_INDV_EMU == 0
		memset(weight_entry.bd_addr, 0x00, BD_ADDR_LEN);
#endif /* CONFIG_AUDREY_INDV_EMU */
#if CONFIG_AUDREY_BADGE
	}
#endif
	// For Debug
	struct tm *tm_save;
	tm_save = get_local_tm(&weight_entry.time);
	Log_Info("[Entry Weight]\r\nBody: %dg, Urine: %dg, Stay: %dsec\r\n"
	, weight_entry.body, weight_entry.urine, weight_entry.stay);
	Log_Info("BD Addr: %02x:%02x:%02x:%02x:%02x:%02x, Time: %s\r\n"
	, weight_entry.bd_addr[0], weight_entry.bd_addr[1], weight_entry.bd_addr[2]
	, weight_entry.bd_addr[3], weight_entry.bd_addr[4], weight_entry.bd_addr[5], asctime(tm_save));
	SendMessageToStateManager(MSG_SCALE_ENTRY, 0);
	// データ送信要求(システムの日時情報が無い場合は送信しない)
	if(time_flag == SNTP_STAT_TIME_SET) {
#if CONFIG_AUDREY_INDV_EMU == 0
		data_upload_req();
#endif /* CONFIG_AUDREY_INDV_EMU */
#if CONFIG_AUDREY_DBG_UPLOAD
		if(badge_config_cnt != 0 && is_dbg_mode()) {
			// デバッグデータ(beacon)送信要求
			Log_Info("[Weight] Upload Debug data(beacon)\r\n");
			dbg_beacon_data_upload_req();
			dbg_rssi_data_upload_req();
		}
#endif
	} else {
		Log_Error("[Weight] No Time info\r\n");
	}
}


// State0: ペット不在、安定
WEIGHT_STATE weightState_NoPet_Stable(void) {

#if CONFIG_AUDREY_INDV_EMU == 0
	// 初期状態では重量測定しない
	if(scale_is_init() != 0) {
		return STATE_NOPET_STABLE;
	}
#endif /* CONFIG_AUDREY_INDV_EMU */
	// 基準値上昇したら、直前の基準値をペットなしの基準値として記録して State1 へ
	if( weight_curr_body >= weight_prev_body + THRESH_REF_VAL ) {
		// 日時取得済情報更新(固体識別のためのBT ONより先に更新必要)
		time_flag = sntp_get_state();
#if CONFIG_AUDREY_INDV_EMU == 1
		time_flag = SNTP_STAT_TIME_SET;
#endif /* CONFIG_AUDREY_INDV_EMU */
		if(time_flag == SNTP_STAT_TIME_SET) {
			urine_pet = median(urine_buf, NUM_URINE);
#if CONFIG_AUDREY_INDV_LOG
			Log_Info("urine_pet = %d; \r\n", urine_pet); 
#endif	/* CONFIG_AUDREY_INDV_LOG */
			time_entry = time_curr;
			weight_stable_cnt = 0;
			is_unstabele_entry = FALSE;
			state1to2_cnt = 0;
			weight_base_backup = 0;
			return STATE_PET_UNSTABLE;
		}
	// 基準値上昇したら、State5 へ
	} else if( weight_curr_body <= weight_prev_body - THRESH_REF_VAL ) {
		time_entry = time_curr;
		return STATE_DOWN_UNSTABLE;
	}
	// 安定している時のみ基準体重を更新
	if(is_sd_large_in == FALSE) {
		weight_base_body = weight_curr_body;
	}
	return STATE_NOPET_STABLE;
}

// State1: ペットあり、不安定
WEIGHT_STATE weightState_Pet_Unstable(void) {

#if CONFIG_AUDREY_INDV_LOG
	Log_Info("[body] curr:%d, prev:%d, base:%d, entry:%d \r\n"
	, weight_curr_body, weight_prev_body, weight_base_body, weight_entry_body); 
#endif	/* CONFIG_AUDREY_INDV_LOG */
	// 入室用標準偏差が小さければ、
	if(is_sd_large_in == FALSE) {
		if( weight_curr_body >= weight_base_body + THRESH_REF_VAL ) {
			// ペットなし基準値と比べて大きければ State2 へ
			weight_entry_body = weight_curr_body;
			// 入室時間保持
			time_entry = time_curr;
#if CONFIG_AUDREY_INDV_EMU == 1
				// 個体識別エミュレートを実行する場合、CONFIG_AUDREY_BADGE を無効にする為、
				// 以下を CONFIG_AUDREY_BADGE の外に出した。
//				badge_rssi_get_flg = TRUE;					// RSSI値変数格納許可
				// 入室時beacon取得開始
//				badge_beacon_start(BADGE_TYPE_ENTRY);
#endif /* CONFIG_AUDREY_INDV_EMU */
#if CONFIG_AUDREY_BADGE
			// バッジ未登録または日時未取得時は固体識別処理をskip(重量測定結果も未送信)
			if(badge_config_cnt != 0 && time_flag == SNTP_STAT_TIME_SET) {
#if CONFIG_AUDREY_INDV
//				badge_rssi_get_flg = TRUE;					// RSSI値変数格納許可
#endif /* CONFIG_AUDREY_INDV */
				// 入室時beacon取得開始
//				badge_beacon_start(BADGE_TYPE_ENTRY);
#if CONFIG_AUDREY_DBG_UPLOAD
				// 入室時beacon取得開始時間保持
				badge_dbg_time_in = scale_info.time;
#endif
			}
#endif
			// beacon 再スキャンは体重安定後に開始
			if(is_unstabele_entry) {
				is_unstabele_entry = FALSE;
				if(badge_config_cnt != 0 && time_flag == SNTP_STAT_TIME_SET) {
					badge_beacon_start(BADGE_TYPE_ENTRY);
				}
			}
			return STATE_PET_STABLE;
		} else {
			// さほど大きくなければ State1前の状態へ
			if(is_unstabele_entry) {
				if(prev2State == STATE_PET_STABLE) {
					// state2->state1->state2の場合
					state1to2_cnt++;
					if(state1to2_cnt > 1) {
						// 切替が２回以上続く場合は基準値バックアップ情報は破棄
						weight_stable_cnt--;
						weight_base_body = weight_stable[weight_stable_cnt].body;
						urine_pet = weight_stable[weight_stable_cnt].urine;
						time_entry = weight_stable[weight_stable_cnt].time;
					}
					// state1<->state2の切替が一定回数続いたら現在の体重を入室中体重にする
					if(state1to2_cnt >= NUM_STATE1TO2) {
						state1to2_cnt = 0;
						weight_entry_body = weight_curr_body;
					}
				} else {
					state1to2_cnt = 0;
				}
				is_unstabele_entry = FALSE;
				return STATE_PET_STABLE;
			} else {
				urine_chk_cnt = 0;
				return STATE_NOPET_STABLE;
			}
		}
	}
	return STATE_PET_UNSTABLE;

}

// State2: ペットあり、安定
WEIGHT_STATE weightState_Pet_Stable(void) {

#if CONFIG_AUDREY_INDV_LOG
	Log_Info("[body] curr:%d, prev:%d, base:%d, entry:%d \r\n"
	, weight_curr_body, weight_prev_body, weight_base_body, weight_entry_body); 
#endif	/* CONFIG_AUDREY_INDV_LOG */

#if CONFIG_AUDREY_INDV
	// 退室直後にRSSI値の変化を測定
	if( scale_info.body <= (weight_entry_body - ((weight_entry_body * 10) / 100)) ) {
		badge_rssi_get_flg = FALSE;					// RSSI値変数格納禁止
#if CONFIG_AUDREY_INDV_LOG
		Log_Info("badge_rssi_get_flg = FALSE; \r\n"); 
#endif	/* CONFIG_AUDREY_INDV_LOG */
	}
#endif	/* CONFIG_AUDREY_INDV */

	if( weight_curr_body >= weight_entry_body + THRESH_ENTRY ) {
		// 基準値上昇したら、State1 へ
		if(weight_stable_cnt < NUM_STABLE) {
			// 入室開始時の値をバックアップ
			weight_stable[weight_stable_cnt].body = weight_base_body;
			weight_stable[weight_stable_cnt].urine = urine_pet;
			weight_stable[weight_stable_cnt].time = time_entry;
			weight_stable_cnt++;
		}
		weight_base_body = weight_prev_body;
		// 最新の基準値を更新する際に入室中体重値をバックアップしておく
		weight_base_backup = weight_entry_body;
		urine_chk_cnt = 0;
		urine_pet = median(urine_buf, NUM_URINE);
#if CONFIG_AUDREY_INDV_LOG
		Log_Info("urine_pet = %d; \r\n", urine_pet); 
#endif	/* CONFIG_AUDREY_INDV_LOG */
		time_entry = time_curr;
		is_unstabele_entry = TRUE;
		return STATE_PET_UNSTABLE;
	} else if( weight_curr_body <= weight_entry_body - THRESH_ENTRY ) {
		// 基準値下降したら、State3 へ
		return STATE_NOPET_UNSTABLE;
	}
	// 入室時基準体重が上がっていれば更新
	if( is_sd_large_in == FALSE && weight_curr_body > weight_entry_body) {
		weight_entry_body = weight_curr_body;
	}
	return STATE_PET_STABLE;

}

// State3: ペット不在、不安定
WEIGHT_STATE weightState_NoPet_UnStable(void) {

	// 入室用標準偏差が小さければ、
	if(is_sd_large_in == FALSE) {
		if( weight_curr_body > (weight_base_body + ((weight_entry_body - weight_base_body) / 4)) ) {
			// 入室中の1/4以上の重量であれば State2 へ
			// 体重値が以前より増えていれば更新
			if(weight_curr_body > weight_entry_body) {
				weight_entry_body = weight_curr_body;
			}
			urine_chk_cnt = 1;
#if CONFIG_AUDREY_INDV
			badge_rssi_get_flg = TRUE;					// RSSI値変数格納許可（再度乗った時用）
			Log_Info("badge_rssi_get_flg = TRUE; \r\n"); 
#endif	/* CONFIG_AUDREY_INDV */
			return STATE_PET_STABLE;
		}
	}
	// 退室用標準偏差が小さければ、
	if(is_sd_large_out == FALSE) {
		if( weight_curr_body <= (weight_base_body + ((weight_entry_body - weight_base_body) / 4)) ) {
			// 入室中の1/4以下の重量であれば State4 へ
			// 退室時間保持
			time_exit = time_curr;
			urine_chk_cnt = 1;
#if CONFIG_AUDREY_INDV_EMU == 1
				// 退室時beacon取得開始
//				badge_beacon_start(BADGE_TYPE_EXIT);
#endif /* CONFIG_AUDREY_INDV_EMU */
#if CONFIG_AUDREY_BADGE
			// バッジ未登録または日時未取得時は固体識別処理をskip(重量測定結果も未送信)
			if(badge_config_cnt != 0 && time_flag == SNTP_STAT_TIME_SET) {
				// 退室時beacon取得開始
//				badge_beacon_start(BADGE_TYPE_EXIT);
#if CONFIG_AUDREY_DBG_UPLOAD
				// 退室時beacon取得開始時間保持
				badge_dbg_time_out = scale_info.time;
#endif
			}
#endif
			// 測定完了までに体重上昇した場合の基準体重バックアップ
			weight_next_body = weight_curr_body;
			return STATE_NOPET_URINE;
		}
	}
	return STATE_NOPET_UNSTABLE;

}

// State4: ペット不在、尿量計測待ち
WEIGHT_STATE weightState_NoPet_Urine(void) {

	int weight_backup;

	// 基準値下降したら、上部ユニット取り外しとみなして重量測定確定させる
	if( weight_curr_body <= weight_prev_body - THRESH_REMOVE_VAL ) {
		// １つ前の体重計値を現在体重として扱う(入退室データ確定後に現在体重を戻す)
		weight_backup = weight_curr_body;
		weight_curr_body = weight_prev_body;
		// 中央値を尿量として取得し State0 へ
		urine_nopet = median(urine_buf, NUM_URINE);
#if CONFIG_AUDREY_INDV_LOG
		Log_Info("urine_nopet = %d; \r\n", urine_nopet); 
#endif	/* CONFIG_AUDREY_INDV_LOG */
		// 入退室情報記録
		recordWeight();
		weight_curr_body = weight_backup;
		urine_chk_cnt = 0;
		return STATE_NOPET_STABLE;
	}
	// 基準値上昇したら、State1 へ(バッジ登録有り時は個体識別優先)
	if( badge_config_cnt != 0 && weight_curr_body >= weight_prev_body + THRESH_REF_VAL ) {
		// １つ前の体重計値を現在体重として扱う(入退室データ確定後に現在体重を戻す)
		weight_backup = weight_curr_body;
		weight_curr_body = weight_prev_body;
		// 中央値を退室時尿量計値として取得
		urine_nopet = median(urine_buf, NUM_URINE);
#if CONFIG_AUDREY_INDV_LOG
		Log_Info("urine_nopet = %d; \r\n", urine_nopet); 
#endif	/* CONFIG_AUDREY_INDV_LOG */
		// 入退室情報記録して新規入室からはじめる
		recordWeight();
		weight_curr_body = weight_backup;
		urine_chk_cnt = 0;
		weight_base_body = weight_next_body;
		urine_pet = median(urine_buf, NUM_URINE);
#if CONFIG_AUDREY_INDV_LOG
		Log_Info("urine_pet = %d; \r\n", urine_pet); 
#endif	/* CONFIG_AUDREY_INDV_LOG */
		time_entry = time_curr;
		weight_stable_cnt = 0;
		is_unstabele_entry = FALSE;
		state1to2_cnt = 0;
		weight_base_backup = 0;
		return STATE_PET_UNSTABLE;
	}
	// 体重値が不安定になれば、State3 へ(バッジ登録無し時は重量安定を優先)
	if(badge_config_cnt == 0 && is_sd_large_out == TRUE) {
		return STATE_NOPET_UNSTABLE;
	}
	// 指定回数経過したら、
	if( urine_chk_cnt >= NUM_URINE ) {
		// 中央値を尿量として取得し State0 へ
		urine_nopet = median(urine_buf, NUM_URINE);
#if CONFIG_AUDREY_INDV_LOG
		Log_Info("urine_nopet = %d; \r\n", urine_nopet); 
#endif	/* CONFIG_AUDREY_INDV_LOG */
		// 入退室情報記録
		recordWeight();
		urine_chk_cnt = 0;
		return STATE_NOPET_STABLE;
	} else {
		urine_chk_cnt++;
	}
	return STATE_NOPET_URINE;

}

// State5: 重量低下、不安定
WEIGHT_STATE weightState_Down_Unstable(void) {
	// 退室用標準偏差が小さければ、state1 へ
	if(is_sd_large_out == FALSE) {
		weight_base_body = weight_curr_body;
		return STATE_NOPET_STABLE;
	}
	return STATE_DOWN_UNSTABLE;
}

#if CONFIG_AUDREY_CAB_JUDGE
// State5: ペット不在、上部ユニット未挿入
WEIGHT_STATE weightState_NoPet_Nocab(void) {

	// 基準値上昇したら、State0 へ
	if(weight_curr_body > THRESH_CABI) {
		if(is_sd_large_in == FALSE) {
			weight_base_body = weight_curr_body;
			urine_chk_cnt = 0;
			return STATE_NOPET_STABLE;
		}
	}
	return STATE_NOPET_NOCAB;
}
#endif

void weightStateMachine(int body, int urine, u32 time) {

#if CONFIG_AUDREY_INDV_EMU
	if(mainState == STATE_NOPET_STABLE || mainState == STATE_DOWN_UNSTABLE) {
		time_curr = 0;
	}else {
		time_curr++;
	}
#endif

	prev2State = prevState;
	prevState = mainState;

	// 直近５回の中央値を定期送信用重量とする(日時は最新日時情報)
	weight_buf.body[weight_buf_idx] = body;
	weight_buf.urine[weight_buf_idx] = urine;
	if(weight_buf_idx >= NUM_WEIGHT-1) {
		weight_buf_idx = 0;
	} else {
		weight_buf_idx++;
	}
	weight_info.body = median(weight_buf.body, NUM_WEIGHT);
	weight_info.urine = median(weight_buf.urine, NUM_WEIGHT);
	weight_info.time = time;

	// 入室判定用体重値更新
	body_buf_in[body_buf_idx_in] = body;
	if( body_buf_idx_in >= NUM_BODY_IN - 1 ) {
		body_buf_idx_in = 0;
	} else {
		body_buf_idx_in++;
	}

	// 退室判定用体重値更新
	body_buf_out[body_buf_idx_out] = body;
	if( body_buf_idx_out >= NUM_BODY_OUT - 1 ) {
		body_buf_idx_out = 0;
	} else {
		body_buf_idx_out++;
	}

	// 尿量値更新
	urine_buf[urine_buf_idx] = urine;
	if(urine_buf_idx >= NUM_URINE-1) {
		urine_buf_idx = 0;
	} else {
		urine_buf_idx++;
	}

	// 最新/前回体重値更新
	updateRefWeight();

	// 入室判定用標準偏差更新
	is_sd_large_in = checkSdLarge(body_buf_in, NUM_BODY_IN);

	// 退室判定用標準偏差更新
	is_sd_large_out = checkSdLarge(body_buf_out, NUM_BODY_OUT);

	// 体重計が上部ユニット重量より少ない場合は状態変更
#if CONFIG_AUDREY_CAB_JUDGE
	if(mainState != STATE_NOPET_NOCAB && weight_curr_body <= THRESH_CABI) {
		mainState = STATE_NOPET_NOCAB;
	} else {
#endif
		switch(mainState) {
			// State0: ペット不在、安定
			case STATE_NOPET_STABLE:
				mainState = weightState_NoPet_Stable();
				break;

			// State1: ペットあり、不安定
			case STATE_PET_UNSTABLE:
				mainState = weightState_Pet_Unstable();
				break;

			// State2: ペットあり、安定
			case STATE_PET_STABLE:
				mainState = weightState_Pet_Stable();
				break;

			// State3: ペット不在、不安定
			case STATE_NOPET_UNSTABLE:
				mainState = weightState_NoPet_UnStable();
				break;

			// State4: ペット不在、尿量取得待ち
			case STATE_NOPET_URINE:
				mainState = weightState_NoPet_Urine();
				break;

			// State5: 重量低下、不安定
			case STATE_DOWN_UNSTABLE:
				mainState = weightState_Down_Unstable();
				break;

#if CONFIG_AUDREY_CAB_JUDGE
			// State5: ペット不在、上部ユニット未挿入
			case STATE_NOPET_NOCAB:
				mainState = weightState_NoPet_Nocab();
				break;
#endif

			case STATE_MAX:
			default:
				break;
		}
		// 入室状態が一定時間継続したら State0 に戻す
		if(mainState != STATE_NOPET_STABLE
#if CONFIG_AUDREY_CAB_JUDGE
		 && mainState != STATE_NOPET_NOCAB
#endif
		  && time_curr - time_entry > TIME_ENTRY) {
			weight_base_body = weight_curr_body;
			urine_chk_cnt = 0;
			mainState = STATE_NOPET_STABLE;
		}
#if CONFIG_AUDREY_CAB_JUDGE
	}
#endif
	// 入室開始でBT ON、退室完了でBT OFF
	Log_Info("[Weight] State = %d\r\n",mainState);

#if CONFIG_AUDREY_INDV_EMU
	// 個体識別emulatorでは、CONFIG_AUDREY_BADGEを無効化する為、
	// 以下処理から、badge_beacon_startだけを外に出した。
	if(prevState != STATE_PET_UNSTABLE && prevState != STATE_PET_STABLE && mainState == STATE_PET_UNSTABLE) {
		Log_Info("[Weight] badge_beacon_start\r\n");
		badge_beacon_start(BADGE_TYPE_ENTRY);
	}
#endif /* CONFIG_AUDREY_INDV_EMU */

#if CONFIG_AUDREY_BADGE
	// バッジ未登録または日時未取得時は固体識別処理をskip(重量測定結果も未送信)
	if(badge_config_cnt != 0 && time_flag == SNTP_STAT_TIME_SET) {
		if((prevState == STATE_NOPET_STABLE || prevState == STATE_DOWN_UNSTABLE
#if CONFIG_AUDREY_CAB_JUDGE
		 || prevState == STATE_NOPET_NOCAB
#endif
		 ) && (mainState != STATE_NOPET_STABLE && mainState != STATE_DOWN_UNSTABLE
#if CONFIG_AUDREY_CAB_JUDGE
		  && mainState != STATE_NOPET_NOCAB
#endif
		 )) {
			Log_Info("[Weight] BT for badge ON\r\n");
			badge_bt_set(1);
			vTaskDelay(20 / portTICK_RATE_MS);
		} else if((prevState != STATE_NOPET_STABLE && prevState != STATE_DOWN_UNSTABLE
#if CONFIG_AUDREY_CAB_JUDGE
		 && prevState != STATE_NOPET_NOCAB
#endif
		 ) && (mainState == STATE_NOPET_STABLE || mainState == STATE_DOWN_UNSTABLE
#if CONFIG_AUDREY_CAB_JUDGE
		 || mainState == STATE_NOPET_NOCAB
#endif
		 )) {
			Log_Info("[Weight] BT for badge OFF\r\n");
			badge_beacon_end(BADGE_TYPE_EXIT);
			vTaskDelay(20 / portTICK_RATE_MS);
			badge_bt_set(0);
		}
		if(prevState != STATE_PET_UNSTABLE && prevState != STATE_PET_STABLE && mainState == STATE_PET_UNSTABLE) {
			badge_beacon_start(BADGE_TYPE_ENTRY);
		}
	}
#endif
#if CONFIG_AUDREY_LED_WEIGHT && CONFIG_AUDREY_CAB_JUDGE
	if(prevState == STATE_NOPET_NOCAB && mainState != STATE_NOPET_NOCAB) {
		// 上部ユニット装着のタイミングでLED OFF
		led_off_force();
	} else if(prevState != STATE_NOPET_NOCAB && mainState == STATE_NOPET_NOCAB) {
		// 上部ユニットが外されたタイミングでLED ON
		led_revert();
	}
#endif
}

/* 重量測定 情報更新 */
int weight_update(void)
{
	weightStateMachine(scale_info.body, scale_info.urine, scale_info.time);
#if CONFIG_AUDREY_DBG_UPLOAD
	// 日時登録時のみデバッグデータを送信
	if(sntp_get_state() == SNTP_STAT_TIME_SET && is_dbg_mode()) {
		weight_dbg[weight_dbg_cnt].time = scale_info.time;
		weight_dbg[weight_dbg_cnt].body = scale_info.body;
		weight_dbg[weight_dbg_cnt].urine = scale_info.urine;
//		weight_dbg[weight_dbg_cnt].scl[0] = scale_info.stable.psv;
//		weight_dbg[weight_dbg_cnt].scl[1] = scale_info.stable.body;
//		weight_dbg[weight_dbg_cnt].scl[2] = scale_info.stable.urine;
//		sprintf(&weight_dbg[weight_dbg_cnt].scl[3], "%01d", mainState);
//		weight_dbg[weight_dbg_cnt].scl[4] = '\0';
		if(weight_dbg_cnt >= WEIGHT_DBG_MAX - 1) {
			// デバッグデータ(重量)送信要求
			Log_Info("[Weight] Upload Debug data(weight)\r\n");
			dbg_weight_data_upload_req();
			weight_dbg_cnt = 0;
		} else {
			weight_dbg_cnt++;
		}
	}
#endif
	Log_Debug("[Weight] mainState: %d, scale_info.body :%d, weight_base_body :%d, is_sd_large_in :%d\r\n", mainState, scale_info.body, weight_base_body, is_sd_large_in);
	if((mainState != STATE_NOPET_STABLE
#if CONFIG_AUDREY_CAB_JUDGE
	 && mainState != STATE_NOPET_NOCAB
#endif
	) /* || (mainState == STATE_NOPET_STABLE&& scale_info.body >= weight_base_body + THRESH_REF_VAL) */
	 || (is_sd_large_in == TRUE)) {
		return 1;
	} else {
		return 0;
	}
}

/* 重量測定 入室中判定 */
int weight_is_entry(void)
{
	if(mainState != STATE_NOPET_STABLE
#if CONFIG_AUDREY_CAB_JUDGE
	 && mainState != STATE_NOPET_NOCAB
#endif
	) {
		return(mainState);
	} else {
		return 0;
	}
}

#if CONFIG_AUDREY_LED_WEIGHT && CONFIG_AUDREY_CAB_JUDGE
/* 重量測定 上部ユニット有無判定 */
int weight_is_cab(void)
{
	if(mainState == STATE_NOPET_NOCAB) {
		return 0;
	} else {
		return 1;
	}
}
#endif

/* 重量測定 初期化 */
void weight_init(void)
{
	int i;

	for(i = 0; i < NUM_WEIGHT; i++) {
		weight_buf.body[i] = WEIGHT_DEF;
	}
	for(i = 0; i < NUM_WEIGHT; i++) {
		weight_buf.urine[i] = WEIGHT_DEF;
	}
	for(i = 0; i < NUM_BODY_IN; i++) {
		body_buf_in[i] = WEIGHT_DEF;
	}
	for(i = 0; i < NUM_BODY_OUT; i++) {
		body_buf_out[i] = WEIGHT_DEF;
	}
	for(i = 0; i < NUM_URINE; i++) {
		urine_buf[i] = WEIGHT_DEF;
	}
}
