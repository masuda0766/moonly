/*
 * Copyright (C) 2017 SHARP Corporation. All rights reserved.
 */
#include <platform_opts.h>
#include <platform_stdlib.h>
#include "FreeRTOS.h"
#if CONFIG_AUDREY_DBG_UPLOAD
#include "scale.h"
#include "data_upload.h"
#endif
#include "weight.h"
#include "badge.h"
#include "ble_scan.h"
#include "rf_ctrl.h"
#include "mbedtls/debug.h"

// log info
#ifdef CONFIG_AUDREY_INDV_LOG
#define INDV_LOG(fmt, ...)			Log_Info(fmt, ##__VA_ARGS__)
#else
#define INDV_LOG(fmt, ...)
#endif	/* CONFIG_AUDREY_INDV_LOG */

#ifdef CONFIG_AUDREY_INDV_EMU_LOG
#define EMU_LOG(fmt, ...)			Log_Info(fmt, ##__VA_ARGS__)
#else
#define EMU_LOG(fmt, ...)
#endif	/* CONFIG_AUDREY_INDV_EMU_LOG */


#define BADGE_THR_BODY_DIFF			  200		// 入室判定体重差分閾値
#define BADGE_THR_ENTRY_DIS			    5		// 入室分散判定閾値
#define BADGE_THR_ENTRY_RSSI		(-65)		// 入室RSSI判定閾値
#define BADGE_THR_EXIT_DIS			    5		// 退室分散判定閾値
#define BADGE_THR_EXIT_DIFF			   10		// 退室減衰値判定閾値

#if CONFIG_AUDREY_INDV
#define RSSI_HIGHER_DIFF				5		// 入室中RSSI値の差
#define RSSI_NEAR_DIFF					7		// 入室中RSSI値の差
#define RSSI_HIGHER_INI				(-128)		// 入室中RSSI値の初期値
#define RSSI_INVALID_THRESH			 (-75)		// 入室中RSSI値の閾値（本値以下なら個体識別無効）
#define RSSI_OUT_ARRAY_IDX				4		// 入室 → 退出時RSSI値格納配列Index値
#define RSSI_OUT_DIFF				   10		// 退出中RSSI値の差
#define BODY_DIFF_RATE_HIGH				2		// 入室判定体重差分 割合 高(%)
#define BODY_DIFF_RATE_MID				5		// 入室判定体重差分 割合 中(%)
#define BODY_DIFF_RATE_LOW			   10		// 入室判定体重差分 割合 低(%)
#define BODY_DIFF_RATE_NONE			   10		// 入室判定体重差分 割合 なし(%)
#define IND_UNIDENTIFIED			 0xFF		// 個体識別判定不能
#endif	/* CONFIG_AUDREY_INDV */

BADGE_DATA badge_data[BADGE_PET_MAX];
int badge_config_cnt = 0;
int badge_beacon_type;
u8 badge_low_info[BADGE_PET_MAX][BD_ADDR_LEN];
int badge_low_num;

#if CONFIG_AUDREY_DBG_UPLOAD
BADGE_DBG_DATA badge_dbg[BADGE_DBG_NUM_MAX];
int badge_dbg_cnt = 0;
#endif

#if CONFIG_AUDREY_INDV
// 退室後、再度乗った時に RSSI値を再度取得しない為のフラグ
// フラグセットは、入室確定後。つまり、STATE_PET_UNSTABLE → STATE_PET_STABLE
// フラグクリアは、退室後。STATE_PET_STABLE → STATE_NOPET_UNSTABLE
boolean badge_rssi_get_flg = FALSE;
#endif	/* CONFIG_AUDREY_INDV */

#if CONFIG_AUDREY_INDV_EMU_LOG
char badge_time[40] = {0};
char badge_time_tmp[40] = {0};
boolean badge_time_flag = FALSE;
extern boolean badge_judge_through;
#endif /* CONFIG_AUDREY_INDV_EMU_LOG */

#if CONFIG_AUDREY_INDV

/* 配列に格納された値を[0]から高い値にソートを行う */
static void badge_sort_higher(char *rssi_higher_array, int num)
{

	int			i, j;
	char		tmp = 0;

	// iの値と以降のjの値で大きい値をiに格納
	for (i = 0; i < (num - 1); i++) {
		for (j = i + 1; j < num; j++) {
			if (rssi_higher_array[i] < rssi_higher_array[j]) {
				tmp = rssi_higher_array[i];
				rssi_higher_array[i] = rssi_higher_array[j];
				rssi_higher_array[j] = tmp;
			}
		}
	}

#if CONFIG_AUDREY_INDV_LOG
	// ログに出力
	for (i = 0; i < num; i++) {
		Log_Info("rssi_higher_array[%d]:%d\r\n", i, rssi_higher_array[i]);
	}
#endif	/* CONFIG_AUDREY_INDV_LOG */

	return;
}

static char badge_avg(char *rssi_higher_array, int num)
{

	int			i;
	int			avg = 0;
	int			cnt = 0;

	// 平均値算出
	for (i = 0; i < num; i++) {
		// 値に、初期値が入っていたら、処理飛ばす
		if (rssi_higher_array[i] == RSSI_HIGHER_INI) {
			// do nothing
		} else {
			avg += rssi_higher_array[i];
			cnt++;
		}
	}

	// 不在の猫は、初期値をセット
	if (cnt == 0) {
		avg = RSSI_HIGHER_INI;
	} else {
		avg = avg / cnt;
	}

	return (char)avg;
}

static char badge_get_out_diff(char *rssi_out_array)
{

	int			i;
	int			diff = 0;
	int			diff_tmp = 0;
	int			high_idx = 0;


	// 配列、0～3(RSSI_OUT_ARRAY_IDX(4) - 1)の中で、最も値の高いものを洗い出す。
	high_idx = (RSSI_OUT_ARRAY_IDX - 1);
	if (rssi_out_array[high_idx] == 0) {
		return 0;
	}
	for (i = (RSSI_OUT_ARRAY_IDX - 2); i >= 0; i--) {
//#if CONFIG_AUDREY_INDV_LOG
//		Log_Info("[Badge] high_idx:%d, high RSSI i:%d \r\n", high_idx, i);
//		Log_Info("[Badge] rssi_out_array[high_idx]:%d, rssi_out_array[i]:%d \r\n", rssi_out_array[high_idx], rssi_out_array[i]);
//#endif /* CONFIG_AUDREY_INDV_LOG */
		if (rssi_out_array[i] == 0) {
			break;
		} else if (rssi_out_array[high_idx] < rssi_out_array[i]) {
			high_idx = i;
		}
	}

#if CONFIG_AUDREY_INDV_LOG
	Log_Info("[Badge] high RSSI idx:%d, value:%d \r\n", high_idx, rssi_out_array[high_idx]);
#endif /* CONFIG_AUDREY_INDV_LOG */
	// 入室中、最も高い値が、閾値以下であれば、トイレに入っていないと判断
	if (rssi_out_array[high_idx] <= RSSI_INVALID_THRESH) {
		return 0;
	}

	// rssi_out_array[RSSI_OUT_ARRAY_IDX(4)]が、体重差が発生した時のRSSI値
	// そこで、rssi_out_array[RSSI_OUT_ARRAY_IDX-1(3)]と、rssi_out_array[RSSI_OUT_ARRAY_IDX(4)]～[RSSI_OUT_ARRAY_IDX+2(6)]の差分の最大値を取得
	for (i = RSSI_OUT_ARRAY_IDX; i < BADGE_RSSI_OUT_NUM; i++) {
		// 値に、初期値(0)が入っていたら、処理抜ける。
		if (rssi_out_array[i] == 0) {
			break;
		}

		diff_tmp = abs(abs((int)rssi_out_array[high_idx]) - abs((int)rssi_out_array[i]));
		if (diff <= diff_tmp) {
			diff = diff_tmp;
		}
	}

#if CONFIG_AUDREY_INDV_LOG
	Log_Info("[Badge] high RSSI diff:%d \r\n", diff);
#endif /* CONFIG_AUDREY_INDV_LOG */

	// char型の範囲補正
	if (diff >= 128) {
		diff = 127;
	}

	return	(char)diff;
}

//==================---=========================================================
// 最初の有効猫（除外猫以外）を見つける
//==================---=========================================================
static int badge_find_valid_first_cat(void)
{

	int					i = 0;
	int					valid_idx = IND_UNIDENTIFIED;

	for (i = 0; i < badge_config_cnt; i++) {
		if (badge_data[i].exclution_flg == 0) {
			valid_idx = i;
			break;
		}
	}

	return valid_idx;
}

//==================---=========================================================
// 次の有効猫（除外猫以外）を見つける
//==================---=========================================================
static int badge_find_valid_next_cat(int cat_idx)
{

	int					i = 0;
	int					valid_idx = IND_UNIDENTIFIED;

	INDV_LOG("cat id:%d\r\n", cat_idx);
	cat_idx++;
	for (i = cat_idx; i < badge_config_cnt; i++) {
		if (badge_data[i].exclution_flg == 0) {
			valid_idx = i;
			break;
		}
	}
	INDV_LOG("next cat id:%d\r\n", valid_idx);

	return valid_idx;
}

//==================---=========================================================
// 有効猫（除外猫以外）の判断
//==================---=========================================================
static boolean badge_judge_valid_cat(int cat_idx)
{

	boolean				ret = FALSE;

	if (badge_data[cat_idx].exclution_flg == 1) {
		ret = TRUE;
	} else {
		INDV_LOG("exclusive cat %d", cat_idx);
	}

	return ret;
}

//==================---=========================================================
// 各バッチの
// badge_data[*].buf[BADGE_TYPE_ENTRY].rssi_higher_array[] （上位３点）
// の中で一番値の高いものと２番目の差が5dBm以上の差があれば、４ポイント付ける。
//==================---=========================================================
static boolean badge_point_entering_rssi(void)
{

	int					i;
	int					highest_idx = 0;
	int					highest_value = RSSI_HIGHER_INI;
	char				avg = RSSI_HIGHER_INI;
	boolean				near_flg = FALSE;

	INDV_LOG("\r\n");
	INDV_LOG("3 Point Entering RSSI\r\n");
	INDV_LOG("===========================\r\n");

	// 有効な（除外以外）最初の１匹目を見つける。
	highest_idx = badge_find_valid_first_cat();
	// もっとも値の高いものを見つける。
	// 同じ値の場合は、無視する。
	for(i = badge_find_valid_first_cat(); i < badge_config_cnt; i= badge_find_valid_next_cat(i)) {
		avg = badge_data[i].buf[BADGE_TYPE_ENTRY].rssi_higher_avg;
		if (badge_data[highest_idx].buf[BADGE_TYPE_ENTRY].rssi_higher_avg < avg) {
			highest_idx = i;
		}
	}

	// もし最も価の高いものが、初期値（RSSI_HIGHER_INI）なら、該当なしで処理を抜ける
	// バッジの電源が切れているか、バッジの電波の届かない状態で、バッジを付けていないものが乗った。
	if (badge_data[highest_idx].buf[BADGE_TYPE_ENTRY].rssi_higher_avg == RSSI_HIGHER_INI) {
		Log_Info("[Badge] Not Found\r\n");
		return near_flg;
	}

	// 最も高い値を元に、他の値を比較し（自身は無視）、
	// 5dBm未満があれば処理を抜け、無ければ、自分に4ポイントを付ける。
	highest_value = abs((int)badge_data[highest_idx].buf[BADGE_TYPE_ENTRY].rssi_higher_avg);
	for (i = badge_find_valid_first_cat(); i < badge_config_cnt; i= badge_find_valid_next_cat(i)) {
		// 比較対象が自身なら処理スキップ
		if (highest_idx == i) {
			// do nothing
		} else {
			// 5dBm以上の場合
			if ((abs((int)badge_data[i].buf[BADGE_TYPE_ENTRY].rssi_higher_avg) - highest_value) >= RSSI_HIGHER_DIFF) {
				// do nothing
			// 5dBm未満の場合
			} else {
				near_flg = TRUE;
				break;
			}
		}
	}
	// 5dBm未満が存在した
	if (near_flg == TRUE) {
		// do nothing
	// 5dBm未満が存在しなかった
	} else {
		badge_data[highest_idx].point += 4;
#if CONFIG_AUDREY_INDV_EMU_LOG
		badge_data[highest_idx].rssi_in_point += 4;
#endif /* CONFIG_AUDREY_INDV_EMU_LOG */
	}

	INDV_LOG("\r\n");
	INDV_LOG("[Badge] Judge for "BD_ADDR_FMT"\r\n", BD_ADDR_ARG(badge_data[highest_idx].conf.bd_addr));
	INDV_LOG("[Badge] RSSI: %d point\r\n", badge_data[highest_idx].point);

	return near_flg;
}

//==================---=========================================================
// 各バッチの
// badge_data[*].buf[BADGE_TYPE_ENTRY].rssi_higher_array[] （上位７点）
// の中で一番値の高いものと２番目の差が5dBm以上の差があれば、４ポイント付ける。
// 5dBm未満の場合は、２ポイント付ける
//==================---=========================================================
static void badge_point_entering_long_rssi(void)
{

	int					i;
	int					highest_idx = 0;
	int					highest_value = RSSI_HIGHER_INI;
	char				avg = RSSI_HIGHER_INI;
	boolean				near_flg = FALSE;

	INDV_LOG("\r\n");
	INDV_LOG("7 Point Entering RSSI\r\n");
	INDV_LOG("===========================\r\n");
	// 有効な（除外以外）最初の１匹目を見つける。
	highest_idx = badge_find_valid_first_cat();
	// もっとも値の高いものを見つける。
	// 同じ値の場合は、無視する。
	for(i = badge_find_valid_first_cat(); i < badge_config_cnt; i= badge_find_valid_next_cat(i)) {
		avg = badge_data[i].buf[BADGE_TYPE_ENTRY].rssi_higher_long_avg;
		if (badge_data[highest_idx].buf[BADGE_TYPE_ENTRY].rssi_higher_long_avg < avg) {
			highest_idx = i;
		}
	}

	// もし最も価の高いものが、初期値（RSSI_HIGHER_INI）なら、該当なしで処理を抜ける
	// バッジの電源が切れているか、バッジの電波の届かない状態で、バッジを付けていないものが乗った。
	if (badge_data[highest_idx].buf[BADGE_TYPE_ENTRY].rssi_higher_long_avg == RSSI_HIGHER_INI) {
		Log_Info("[Badge] Not Found\r\n");
		return;
	}

	// 最も高い値を元に、他の値を比較し（自身は無視）、
	// 5dBm以上でポイントは0。
	// 5dBm未満であれば、ポイントに2を付け、全て終わったら自分にも2を付ける。
	// もし、無ければ、自分に4を付ける。
	highest_value = abs((int)badge_data[highest_idx].buf[BADGE_TYPE_ENTRY].rssi_higher_long_avg);
	for (i = badge_find_valid_first_cat(); i < badge_config_cnt; i= badge_find_valid_next_cat(i)) {
		// 比較対象が自身なら処理スキップ
		if (highest_idx == i) {
			// do nothing
		} else {
			// 5dBm以上の場合
			if ((abs((int)badge_data[i].buf[BADGE_TYPE_ENTRY].rssi_higher_long_avg) - highest_value) >= RSSI_HIGHER_DIFF) {
				// do nothing
			// 5dBm未満の場合
			} else {
				near_flg = TRUE;
				badge_data[i].point += 2;
#if CONFIG_AUDREY_INDV_EMU_LOG
				badge_data[i].rssi_in_point += 2;
#endif /* CONFIG_AUDREY_INDV_EMU_LOG */
				INDV_LOG("[Badge] Judge for "BD_ADDR_FMT"\r\n", BD_ADDR_ARG(badge_data[i].conf.bd_addr));
				INDV_LOG("[Badge] RSSI: %d point\r\n", badge_data[i].point);
			}
		}
	}
	// 5dBm未満が存在した
	if (near_flg == TRUE) {
		badge_data[highest_idx].point += 2;
#if CONFIG_AUDREY_INDV_EMU_LOG
		badge_data[highest_idx].rssi_in_point += 2;
#endif /* CONFIG_AUDREY_INDV_EMU_LOG */
	// 5dBm未満が存在しなかった
	} else {
		badge_data[highest_idx].point += 4;
#if CONFIG_AUDREY_INDV_EMU_LOG
		badge_data[highest_idx].rssi_in_point += 4;
#endif /* CONFIG_AUDREY_INDV_EMU_LOG */
	}

	INDV_LOG("\r\n");
	INDV_LOG("[Badge] Judge for "BD_ADDR_FMT"\r\n", BD_ADDR_ARG(badge_data[highest_idx].conf.bd_addr));
	INDV_LOG("[Badge] RSSI: %d point\r\n", badge_data[highest_idx].point);

	return;
}

//==================---=========================================================
// 入室中RSSIの MAX3点 or 7点の平均が ７dBm（TBD）以内 の猫が居るかチェックする
// True :猫居る（体重チェックを行う）
// False:猫居なかった（体重チェックを行わない）
//==================---=========================================================
static boolean badge_check_RSSI_near(void)
{
	int					i;
	int					highest_idx = 0;
	int					highest_value = RSSI_HIGHER_INI;
	char				avg = RSSI_HIGHER_INI;

	// ３点
	//--------------------------------------------------------------------------
	INDV_LOG("3 Point Body\r\n");
	// 有効な（除外以外）最初の１匹目を見つける。
	highest_idx = badge_find_valid_first_cat();
	// もっとも値の高いものを見つける。
	// 同じ値の場合は、無視する。
	for(i = badge_find_valid_first_cat(); i < badge_config_cnt; i= badge_find_valid_next_cat(i)) {
		avg = badge_data[i].buf[BADGE_TYPE_ENTRY].rssi_higher_avg;
		if (badge_data[highest_idx].buf[BADGE_TYPE_ENTRY].rssi_higher_avg < avg) {
			highest_idx = i;
		}
	}

	// もし最も価の高いものが、初期値（RSSI_HIGHER_INI）なら、該当なしで処理を抜ける
	// バッジの電源が切れているか、バッジの電波の届かない状態で、バッジを付けていないものが乗った。
	// 上位３点の平均が初期値なら、上位７点の平均も初期値なので、猫居ないで処理抜ける
	if (badge_data[highest_idx].buf[BADGE_TYPE_ENTRY].rssi_higher_avg == RSSI_HIGHER_INI) {
		Log_Info("[Badge] badge_check_RSSI_near() MAX3 Not Found\r\n");
		return FALSE;
	}

	// 最も高い値を元に、他の値を比較し（自身は無視）、
	// 7dBm以下がいれば処理を抜ける。
	highest_value = abs((int)badge_data[highest_idx].buf[BADGE_TYPE_ENTRY].rssi_higher_avg);
	for (i = badge_find_valid_first_cat(); i < badge_config_cnt; i= badge_find_valid_next_cat(i)) {
		// 比較対象が自身なら処理スキップ
		if (highest_idx == i) {
			// do nothing
		} else {
			// 7dBm 以内の場合
			if ((abs((int)badge_data[i].buf[BADGE_TYPE_ENTRY].rssi_higher_avg) - highest_value) <= RSSI_NEAR_DIFF) {
				Log_Info("[Badge] badge_check_RSSI_near() MAX3 Fount near cat \r\n");
				return TRUE;
			}
		}
	}

	// ７点
	//--------------------------------------------------------------------------
	INDV_LOG("7 Point Body\r\n");
	highest_idx = badge_find_valid_first_cat();
	// もっとも値の高いものを見つける。
	// 同じ値の場合は、無視する。
	for(i = badge_find_valid_first_cat(); i < badge_config_cnt; i= badge_find_valid_next_cat(i)) {
		avg = badge_data[i].buf[BADGE_TYPE_ENTRY].rssi_higher_long_avg;
		if (badge_data[highest_idx].buf[BADGE_TYPE_ENTRY].rssi_higher_long_avg < avg) {
			highest_idx = i;
		}
	}

	// もし最も価の高いものが、初期値（RSSI_HIGHER_INI）なら、該当なしで処理を抜ける
	// バッジの電源が切れているか、バッジの電波の届かない状態で、バッジを付けていないものが乗った。
	if (badge_data[highest_idx].buf[BADGE_TYPE_ENTRY].rssi_higher_long_avg == RSSI_HIGHER_INI) {
		Log_Info("[Badge] badge_check_RSSI_near() MAX7 Not Found\r\n");
		return FALSE;
	}

	// 最も高い値を元に、他の値を比較し（自身は無視）、
	// 7dBm以下がいれば処理を抜ける。
	highest_value = abs((int)badge_data[highest_idx].buf[BADGE_TYPE_ENTRY].rssi_higher_long_avg);
	for (i = badge_find_valid_first_cat(); i < badge_config_cnt; i= badge_find_valid_next_cat(i)) {
		// 比較対象が自身なら処理スキップ
		if (highest_idx == i) {
			// do nothing
		} else {
			// 7dBm 以内の場合
			if ((abs((int)badge_data[i].buf[BADGE_TYPE_ENTRY].rssi_higher_long_avg) - highest_value) <= RSSI_NEAR_DIFF) {
				Log_Info("[Badge] badge_check_RSSI_near() MAX7 Fount near cat \r\n");
				return TRUE;
			}
		}
	}

	return FALSE;

}

//==================---=========================================================
// 各バッチにおいて、
// 登録されている体重と、現在の体重の差に応じてポイントを付ける
// 猫の体重 平均:3.6 ～ 4.5 kg
// 4kg:200g:5%
// 4kg:400g:10%
//==================---=========================================================
static void badge_point_body(int body)
{

	int					i;
	int					body_diff = 0;
	int					cat_count = 0;
	int					cat_idx = 0;

	INDV_LOG("\r\n");
	INDV_LOG("Point Body\r\n");
	INDV_LOG("===========================\r\n");

	// 体重チェック
	// 複数猫のサーバー登録体重の中で、乗った猫の体重 ±10% 以内の猫が１匹の場合、
	// ４ポイントを付けて終了する。
	// １匹でなければ、また、登録体重が 0g の猫が１匹でもいれば、体重アルゴリズムを通す
	//--------------------------------------------------------------------------
	// 入室中RSSIの MAX3点 or 7点の平均が ７dBm（TBD）以内 の猫がいれば、体重チェックを行う
	if (badge_check_RSSI_near() == TRUE) {

		// 各猫のサーバー登録体重の±10%以内に、トイレに乗った猫の体重 の猫が何匹いるかチェックする
		for (i = badge_find_valid_first_cat(); i < badge_config_cnt; i= badge_find_valid_next_cat(i)) {

			// サーバー体重が 0g の猫が居るか？もしいれば、処理を抜ける
			if (badge_data[i].conf.body == 0) {
				cat_count = 0;							// 体重アルゴリズムを通す為、cat_count は初期化する
				Log_Info("[Badge] badge_data[%d].conf.body: %d \r\n", i, badge_data[i].conf.body);
				break;
			}

			// 各猫のサーバー登録体重の±10%以内に、トイレに乗った猫の体重 の猫が何匹いるかカウントする
			body_diff = abs(badge_data[i].conf.body - body);
			if (body_diff <= ((badge_data[i].conf.body * BODY_DIFF_RATE_LOW) / 100)) {
				cat_count++;
				cat_idx = i;
			}
			INDV_LOG("[Badge] Judge for "BD_ADDR_FMT"\r\n", BD_ADDR_ARG(badge_data[i].conf.bd_addr));
			INDV_LOG("[Badge] badge_data[i].conf.body: %d, body: %d \r\n", badge_data[i].conf.body, body);
		}

		// １匹だけなら、４ポイントを付けて、体重アルゴリズムを実施しない
		// それ以外（０匹 or ２匹以上）なら、体重アルゴリズムを通す
		if (cat_count == 1) {
			badge_data[cat_idx].point += 4;
#if CONFIG_AUDREY_INDV_EMU_LOG
			badge_data[cat_idx].weight_point = 4;
#endif /* CONFIG_AUDREY_INDV_EMU_LOG */

			INDV_LOG("[Badge] Judge for "BD_ADDR_FMT"\r\n", BD_ADDR_ARG(badge_data[cat_idx].conf.bd_addr));
			INDV_LOG("[Badge] badge_data[cat_idx].conf.body: %d, body: %d \r\n", badge_data[cat_idx].conf.body, body);
			INDV_LOG("[Badge] body: %d point\r\n", badge_data[cat_idx].point);
			return;
		}
	}

	// 体重アルゴリズム
	// サーバー登録体重と乗った猫の体重が ±5% 以内なら ＋２ポイント
	// ±10% 以内なら ＋１ポイント付ける
	//--------------------------------------------------------------------------
	for (i = badge_find_valid_first_cat(); i < badge_config_cnt; i= badge_find_valid_next_cat(i)) {
		INDV_LOG("\r\n");
		INDV_LOG("[Badge] body thresh MID: %d\r\n", ((badge_data[i].conf.body * 5) / 100));
		INDV_LOG("[Badge] body thresh MID: %d\r\n", ((badge_data[i].conf.body * 10) / 100));
		// 体重判定
		if (badge_data[i].conf.body == 0) {
			// do nothing
		} else {
			body_diff = abs(badge_data[i].conf.body - body);
			if (body_diff <= ((badge_data[i].conf.body * BODY_DIFF_RATE_MID) / 100)) {
				badge_data[i].point += 2;
				Log_Info("[Badge] body point +2 \r\n");
#if CONFIG_AUDREY_INDV_EMU_LOG
				badge_data[i].weight_point = 2;
#endif /* CONFIG_AUDREY_INDV_EMU_LOG */
			} else if (body_diff > ((badge_data[i].conf.body * BODY_DIFF_RATE_MID) / 100) 
						&& body_diff <= ((badge_data[i].conf.body * BODY_DIFF_RATE_LOW) / 100)) {
				badge_data[i].point += 1;
				Log_Info("[Badge] body point +1 \r\n");
#if CONFIG_AUDREY_INDV_EMU_LOG
				badge_data[i].weight_point = 1;
#endif /* CONFIG_AUDREY_INDV_EMU_LOG */
			}
		}

		INDV_LOG("[Badge] Judge for "BD_ADDR_FMT"\r\n", BD_ADDR_ARG(badge_data[i].conf.bd_addr));
		INDV_LOG("[Badge] badge_data[i].conf.body: %d, body: %d \r\n", badge_data[i].conf.body, body);
		INDV_LOG("[Badge] body: %d point\r\n", badge_data[i].point);

	}

	return;
}

//==================---=========================================================
// 各バッチの
// badge_data[*].buf[BADGE_TYPE_ENTRY].rssi_out_diff
// の値で、RSSI_OUT_DIFFdBmより差が付いていたものに、1ポイント付ける
//==================---=========================================================
static void badge_point_leaving_rssi(void) {

	int					i;


	INDV_LOG("\r\n");
	INDV_LOG("Point Leaving RSSI\r\n");
	INDV_LOG("===========================\r\n");


	for (i = badge_find_valid_first_cat(); i < badge_config_cnt; i= badge_find_valid_next_cat(i)) {
		if (badge_data[i].buf[BADGE_TYPE_ENTRY].rssi_out_diff > RSSI_OUT_DIFF) {
			badge_data[i].point += 1;
#if CONFIG_AUDREY_INDV_EMU_LOG
			badge_data[i].rssi_out_point = 1;
#endif /* CONFIG_AUDREY_INDV_EMU_LOG */
		}

		INDV_LOG("\r\n");
		INDV_LOG("[Badge] Judge for "BD_ADDR_FMT"\r\n", BD_ADDR_ARG(badge_data[i].conf.bd_addr));
		INDV_LOG("[Badge] rssi_out_diff: %d \r\n", badge_data[i].buf[BADGE_TYPE_ENTRY].rssi_out_diff);
		INDV_LOG("[Badge] RSSI OUT: %d point\r\n", badge_data[i].point);

	}

	return;
}

//==================---=========================================================
// ポイントが4ポイント以上の猫を見つけ、同点が複数居たらなら、識別不可とする。
//==================---=========================================================
static int badge_judge_cat()
{

	int					i;
	int					cat_idx;				// 識別不可値セット


	INDV_LOG("\r\n");
	INDV_LOG("Total Point\r\n");
	INDV_LOG("===========================\r\n");
	for (i = 0; i < badge_config_cnt; i++) {
		INDV_LOG("\r\n");
		INDV_LOG("[Badge] Judge for "BD_ADDR_FMT"\r\n", BD_ADDR_ARG(badge_data[i].conf.bd_addr));
		INDV_LOG("[Badge] Total: %d point\r\n", badge_data[i].point);
#if CONFIG_AUDREY_INDV_EMU_LOG
		Log_Info("[Badge] RSSI_IN, WEIGHT, RSSI_OUT point:%d,%d,%d;\r\n", badge_data[i].rssi_in_point, badge_data[i].weight_point, badge_data[i].rssi_out_point);
		badge_judge_through = TRUE;
#endif /* CONFIG_AUDREY_INDV_EMU_LOG */
		INDV_LOG("===========================\r\n");
	}

	cat_idx = badge_find_valid_first_cat();
	// もっともポイントの高い猫を見つける。
	for(i = badge_find_valid_first_cat(); i < badge_config_cnt; i= badge_find_valid_next_cat(i)) {
		if (badge_data[cat_idx].point < badge_data[i].point) {
			cat_idx = i;
		}
	}

	// 4ポイント未満であれば、識別不可とする。
	if (badge_data[cat_idx].point < 4) {
		Log_Info("[Badge] NO JUDGE. short of point... \r\n");
		return IND_UNIDENTIFIED;
	}

	// 同点が複数居たら識別不可とする。
	for(i = badge_find_valid_first_cat(); i < badge_config_cnt; i= badge_find_valid_next_cat(i)) {
		// 比較対象が最も高いものなら処理スキップ
		if (cat_idx == i) {
			// do nothing
		} else {
			if (badge_data[cat_idx].point == badge_data[i].point) {
				Log_Info("[Badge] NO JUDGE. same point... \r\n");
				return IND_UNIDENTIFIED;
			}
		}
	}

	// 判定された猫のRSSI値が -75dBm 以下なら無効
	if (badge_data[cat_idx].buf[BADGE_TYPE_ENTRY].rssi_higher_avg <= RSSI_INVALID_THRESH) {
		Log_Info("[Badge] NO JUDGE. RSSI under Threshold \r\n");
		return IND_UNIDENTIFIED;
	}

	return cat_idx;
}

//==================---=========================================================
// 除外猫フラグをクリアする
//==================---=========================================================
static void badge_ini_exclution_flg()
{

	int					i = 0;

	for(i = 0; i < BADGE_PET_MAX; i++) {
		badge_data[i].exclution_flg = 0;
	}

	return;
}

//==================---=========================================================
// ポイントをクリアする
//==================---=========================================================
static void badge_ini_point()
{

	int					i = 0;

	for(i = 0; i < BADGE_PET_MAX; i++) {
		badge_data[i].point = 0;
#if CONFIG_AUDREY_INDV_EMU_LOG
		badge_data[i].rssi_in_point = 0;
		badge_data[i].weight_point = 0;
		badge_data[i].rssi_out_point = 0;
#endif /* CONFIG_AUDREY_INDV_EMU_LOG */

	}

	return;
}

//==================---=========================================================
// 登録体重による候補猫除外判定
// ●候補猫が識別不能(IND_UNIDENTIFIED)なら、FALSEを戻し、処理を抜ける。
// ●登録体重が 0g の猫が1匹でも居たら、FALSEを戻し、処理を抜ける。
// ●候補猫が、以下条件
// 	「候補猫の登録体重が、トイレに入った猫体重より±２０％以上差異がある。」
// 	 且
// 	「トイレに入った猫体重の±５％以内に他猫存在」
// 	する場合は、候補猫の除外フラグを立てて、TRUEを戻し、再度個体識別する。
// ●調べる猫が居なくなったら引数の候補猫のindexに識別不能(IND_UNIDENTIFIED)にして、FALSEを戻し、処理を抜ける。
// 引数：
//		body   ：トイレに乗った猫体重
//		cat_idx：個体識別結果の候補猫index
// 戻り値：
// 		TRUE ：除外猫が居た為、再度個体識別を行う
//		FALSE：除外猫がいない為、処理終了
//==================---=========================================================
static boolean badge_judge_exclude_cat(int body, int *cat_idx)
{

	int					i = 0;
	int					body_diff = 0;
	boolean				ret = FALSE;
	int					count = 0;

	// ●候補猫が識別不能(IND_UNIDENTIFIED)なら、FALSEを戻し、処理を抜ける。
	// 除外する猫が分からない為。
	if (*cat_idx == IND_UNIDENTIFIED) {
		INDV_LOG("[Badge] judge is unidentified\r\n");
		return FALSE;
	}

	// ●登録体重が 0g の猫が1匹でも居たら、FALSEを戻し、処理を抜ける。
	// 本当に乗った猫の体重が 0g の場合、一生個体識別出来ない為。
	for (i = 0; i < badge_config_cnt; i++) {
		if (badge_data[i].conf.body == 0) {
			INDV_LOG("[Badge] Registration weight 0g. badge_data[%d].conf.body: %d \r\n", i, badge_data[i].conf.body);
			return FALSE;
		}
	}

	// ●候補猫が、以下条件
	// 	「候補猫の登録体重が、トイレに入った猫体重より±２０％以上差異がある。」
	// 	 且
	// 	「トイレに入った猫体重の±５％以内に他猫存在」
	// 	する場合は、候補猫の除外フラグを立てて、TRUEを戻し、再度個体識別する。
	// 例えばトイレ重量：4kg(3.2～4.8kgの間に登録体重があるかどうかチェック)
	// トイレに入った猫体重より±２０％以上なので、トイレに入った猫体重を基準にしている。
	body_diff = abs(badge_data[*cat_idx].conf.body - body);
	if (body_diff >= ((body * BODY_DIFF_RATE_NONE) / 100)) {
		INDV_LOG("[Badge] cat weight is over 20%.\r\n");

		for (i = 0; i < badge_config_cnt; i++) {
			// 自分・除外猫は対象外にする？
			// 両方とも、そもそも以下条件には入らない(除外猫にならない)。よって、対象外にする必要はない。
			// トイレに入った猫体重の±５％以内なので、トイレに入った猫体重を基準にしている
			body_diff = abs(badge_data[i].conf.body - body);
			INDV_LOG("[Badge] conf;%d, body_diff:%d, 5:%d\r\n", badge_data[i].conf.body, body_diff, ((body * BODY_DIFF_RATE_MID) / 100));
			if (body_diff <= ((body * BODY_DIFF_RATE_MID) / 100)) {
				badge_data[*cat_idx].exclution_flg = TRUE;
				ret = TRUE;
				INDV_LOG("[Badge] There are other cats weighing in the toilet.\r\n");
			}
		}
	}

	// ●調べる猫が居なくなったら引数の候補猫のindexに識別不能(IND_UNIDENTIFIED)にして、
	// FALSEを戻し、処理を抜ける。（本来ここには来ないはずだが、フェールセーフとして対応する）
	for (i = 0; i < badge_config_cnt; i++) {
		if (badge_data[i].exclution_flg == TRUE) {
			count++;
		}
	}
	if (badge_config_cnt == count) {
		Log_Error("[Badge] There is not Candidate cats. \r\n");
		*cat_idx = IND_UNIDENTIFIED;
		ret = FALSE;
	}


	return ret;

}

#endif	/* CONFIG_AUDREY_INDV */


int badge_dispersion(char *list, char list_size) {

	int ave=0;
	int var=0;
	char i,j;

	for(i=0;i<list_size;i++) {
		ave += list[i];
	}
	ave = ave / list_size;
	for(j=0;j<list_size;j++) {
		var += (list[j] - ave) * (list[j] - ave);
	}
	var = var / list_size;

	return abs(var);
//	return sqrt(var);
}

int badge_cmp( const void *p, const void *q) {
	return *(int*)p - *(int*)q;
}

char badge_median(char *list, char list_size) {

	char idx;
	char median;
#if CONFIG_AUDREY_DBG_UPLOAD
	// 最高値を返却する
	for(idx = 0, median = 0; idx < list_size; idx++, list++) {
		if(median == 0 || median < *list) {
			median = *list;
		}
	}
#else
	char zero_cnt;
	int tempHistory[10]={0};

	for(idx = 0; idx < list_size; idx++) {
		tempHistory[idx] = *list;
		list++;
	}
	qsort(tempHistory, list_size, sizeof(int), badge_cmp);

	for(zero_cnt=0,idx=0; idx < list_size; idx++) {
		if(tempHistory[idx] == 0) zero_cnt++;
	}
	if(zero_cnt >= list_size) return 0;
	list_size -= zero_cnt;

	if(list_size % 2) {
		idx = (list_size - 1) / 2;
		median = tempHistory[zero_cnt + idx];
	} else {
		idx = list_size / 2;
		median = (tempHistory[zero_cnt + idx - 1] + tempHistory[zero_cnt + idx]) / 2;
	}
#endif
	return median;
}

/* 個体識別 初期化 */
void badge_init(void)
{
	badge_config_cnt = 0;
}

/* 個体識別 BDアドレス、体重値設定 */
void badge_config(BADGE_CONF *buf)
{
	int i, j;
	u8 *bd;

	Log_Debug("New Badge: %02x:%02x:%02x:%02x:%02x:%02x\r\n", *buf->bd_addr,*(buf->bd_addr+1),*(buf->bd_addr+2),*(buf->bd_addr+3),*(buf->bd_addr+4),*(buf->bd_addr+5));
	// BDアドレスがオール0の場合は登録しない
	for(i = 0; i < BD_ADDR_LEN; i++) {
		if(*(buf->bd_addr+i) != 0) break;
	}
	if(i >= BD_ADDR_LEN) {
		return;
	}
	if(badge_config_cnt < BADGE_PET_MAX) {
#if CONFIG_AUDREY_INDV_EMU == 0
		for(j = 0; j < badge_config_cnt; j++) {
			Log_Debug("Configured Badge: "BD_ADDR_FMT"\r\n", BD_ADDR_ARG(badge_data[j].conf.bd_addr));
			for(i = 0, bd = (buf->bd_addr + BD_ADDR_LEN - 1); i < BD_ADDR_LEN; i++, bd--) {
				if(badge_data[j].conf.bd_addr[i] != *bd) {
					break;
				}
			}
			// 同一BDアドレスが登録済みの場合は終了
			if(i >= BD_ADDR_LEN) {
				return;
			}
		}
		// 同一BDアドレスが未登録の場合は登録
		for(i = 0, bd = (buf->bd_addr + BD_ADDR_LEN - 1); i < BD_ADDR_LEN; i++, bd--) {
			badge_data[badge_config_cnt].conf.bd_addr[i] = *bd;
		}
#else /* CONFIG_AUDREY_INDV_EMU */
		for(j = 0; j < badge_config_cnt; j++) {
			for(i = 0, bd = (buf->bd_addr); i < BD_ADDR_LEN; i++, bd++) {
				if(badge_data[j].conf.bd_addr[i] != *bd) {
					break;
				}
			}
			// 同一BDアドレスが登録済みの場合は終了
			if(i >= BD_ADDR_LEN) {
				return;
			}
		}
		// 同一BDアドレスが未登録の場合は登録
		for(i = 0, bd = (buf->bd_addr); i < BD_ADDR_LEN; i++, bd++) {
			badge_data[badge_config_cnt].conf.bd_addr[i] = *bd;
		}
#endif
		badge_data[badge_config_cnt].conf.body = buf->body;
		badge_data[badge_config_cnt].buf[BADGE_TYPE_ENTRY].cnt = 0;
		badge_data[badge_config_cnt].buf[BADGE_TYPE_EXIT].cnt = 0;
		badge_data[badge_config_cnt].battery = 0;
		badge_data[badge_config_cnt].score = 0;
		badge_config_cnt++;
	}
}
/* 個体識別 BT ON/OFF */
void badge_bt_set(int sw)
{
//	power_le_scan(sw);
	if (sw) {
		rf_ctrl_start_beacon();
	} else {
		rf_ctrl_stop_beacon();
	}
}

/* 個体識別 ビーコン受信 開始 */
void badge_beacon_start(BADGE_TYPE type)
{
	int i;

	badge_beacon_type = type;

	if(type == BADGE_TYPE_ENTRY) {
#if CONFIG_AUDREY_INDV
		badge_rssi_get_flg = TRUE;						// RSSI値変数格納許可
		Log_Info("badge_rssi_get_flg = TRUE; \r\n"); 
#endif	/* CONFIG_AUDREY_INDV */
		for(i = 0; i < BADGE_PET_MAX; i++) {
			memset(badge_data[i].buf, 0, sizeof(BADGE_BUF)*BADGE_TYPE_MAX);
			badge_data[i].battery = 0;
			badge_data[i].score = 0;
#if CONFIG_AUDREY_INDV
			badge_data[i].point = 0;
			badge_data[i].exclution_flg = 0;
			// 以下、badge_data[i].buf の初期化の後にしないといけない。
			memset(badge_data[i].buf[BADGE_TYPE_ENTRY].rssi_higher_array, RSSI_HIGHER_INI, BADGE_RSSI_HIGHER_NUM);
#endif	/* CONFIG_AUDREY_INDV */
#if CONFIG_AUDREY_INDV_EMU_LOG
			badge_data[i].rssi_in_point = 0;
			badge_data[i].weight_point = 0;
			badge_data[i].rssi_out_point = 0;
#endif /* CONFIG_AUDREY_INDV_EMU_LOG */
		}
	}
	if(type == BADGE_TYPE_ENTRY) {
#if CONFIG_AUDREY_DBG_UPLOAD
		if(is_dbg_mode()) {
			if(badge_config_cnt > BADGE_DBG_NUM_MAX) {
				badge_dbg_cnt = BADGE_DBG_NUM_MAX;
			} else {
				badge_dbg_cnt = badge_config_cnt;
			}
			for(i = 0; i < badge_dbg_cnt; i++) {
				for(int j = 0; j < BD_ADDR_LEN; j++) {
					badge_dbg[i].bd_addr[BD_ADDR_LEN - 1 - j] = badge_data[i].conf.bd_addr[j];
				}
				badge_dbg[i].cnt = 0;
			}
		}
#endif
		Log_Info("[Badge] Scan for Entry Start (Badge Num: %d)\r\n", badge_config_cnt);
	} else {
		Log_Info("[Badge] Scan for Exit Start (Badge Num: %d)\r\n", badge_config_cnt);
	}
#if CONFIG_AUDREY_INDV_EMU == 0
	start_le_scan();
#endif /* CONFIG_AUDREY_INDV_EMU */
}

/* 個体識別 ビーコン受信 終了 */
void badge_beacon_end(BADGE_TYPE type)
{
	stop_le_scan();
}

/* 個体識別 ビーコン受信 */
void badge_beacon_rcv(u8 *bd_addr, char rssi, char battery)
{
	int i;
#if CONFIG_AUDREY_INDV
	int					j;
	char				*rssi_out_array = NULL;
	static boolean		prev_get_flg = FALSE;
	char				*out_cnt_p = 0;
#endif	/* CONFIG_AUDREY_INDV */


#if CONFIG_AUDREY_INDV

#if CONFIG_AUDREY_INDV_LOG
	Log_Info("badge_beacon_rcv(rssi;%d, battery:%d) \r\n", rssi, battery); 
#endif	/* CONFIG_AUDREY_INDV_LOG */
	// 各バッチで、入室中（上位4つ）と、退出直後のRSSI値を取得
	//--------------------------------------------------------------------------
	for(i = 0; i < badge_config_cnt; i++) {
		if(!memcmp(bd_addr, &badge_data[i].conf.bd_addr, BD_ADDR_LEN)) {
#if CONFIG_AUDREY_INDV_LOG
			Log_Info("badge_address: "BD_ADDR_FMT"\r\n", BD_ADDR_ARG(badge_data[i].conf.bd_addr));
#endif	/* CONFIG_AUDREY_INDV_LOG */
			// 入室～退室直後までの値を取得
			if (badge_rssi_get_flg == TRUE) {
#if CONFIG_AUDREY_INDV_EMU_LOG
				// 最初に入ったら、時間をコピー
				if (badge_time_flag == TRUE) {
					strcpy(badge_time, badge_time_tmp);
					badge_time_flag = FALSE;
					printf("badge time:%s \r\n", badge_time);
				}
#endif /* CONFIG_AUDREY_INDV_EMU_LOG */
				// 入室中の判定処理用
				//--------------------------------------------------------------
				// RSSI値を配列の最後に格納し、値が高い順番にソートを行う。
				badge_data[i].buf[BADGE_TYPE_ENTRY].rssi_higher_array[BADGE_RSSI_HIGHER_NUM - 1] = rssi;
				badge_sort_higher(badge_data[i].buf[BADGE_TYPE_ENTRY].rssi_higher_array, BADGE_RSSI_HIGHER_NUM);

				// 退室後の判定処理用
				//--------------------------------------------------------------
				// これまでの値を配列の左にずらし、取得した値を配列[RSSI_OUT_ARRAY_IDX - 1]（変化値）に格納する。
				rssi_out_array = badge_data[i].buf[BADGE_TYPE_ENTRY].rssi_out_array;
				for (j = 0; j < (RSSI_OUT_ARRAY_IDX - 1); j++) {
					rssi_out_array[j] = rssi_out_array[j + 1];
				}
				rssi_out_array[RSSI_OUT_ARRAY_IDX - 1] = rssi;

			// 退室直後から 数回の値を取得
			// 状態が変わった時は、既に値が変化している。よって、変化値を使う場合は、
			// 今回の値の前の値からの差を見る。
			} else {
				// 状態が変化したら、*out_cnt_p値の配列に値を格納する
				out_cnt_p = &badge_data[i].buf[BADGE_TYPE_ENTRY].rssi_out_cnt;
				if (prev_get_flg == TRUE) {
					// 入室 → 退出へ状態変わった為、全バッチのカウンタを RSSI_OUT_ARRAY_IDX で初期化
					for(j = 0; j < badge_config_cnt; j++) {
						badge_data[j].buf[BADGE_TYPE_ENTRY].rssi_out_cnt = RSSI_OUT_ARRAY_IDX;
					}
				}
				if ((*out_cnt_p >= RSSI_OUT_ARRAY_IDX) && (*out_cnt_p < BADGE_RSSI_OUT_NUM)) {
#if CONFIG_AUDREY_INDV_LOG
					Log_Info("*out_cnt_p;%d \r\n", *out_cnt_p); 
#endif	/* CONFIG_AUDREY_INDV_LOG */
					rssi_out_array = badge_data[i].buf[BADGE_TYPE_ENTRY].rssi_out_array;
					rssi_out_array[*out_cnt_p] = rssi;

					*out_cnt_p = *out_cnt_p + 1;
					if (*out_cnt_p >= BADGE_RSSI_OUT_NUM) {
						*out_cnt_p = 0;
					}
				}
			}
			prev_get_flg = badge_rssi_get_flg;

#if CONFIG_AUDREY_INDV_LOG
			if (rssi_out_array != NULL) {
				Log_Info("rssi_out_array[0]:%d, [1]:%d, [2]:%d, [3]:%d, [4]:%d \r\n", 
					rssi_out_array[0], rssi_out_array[1], rssi_out_array[2], rssi_out_array[3], rssi_out_array[4]);
				Log_Info("rssi_out_array[5]:%d, [6]:%d, [7]:%d, [8]:%d, [9]:%d \r\n", 
					rssi_out_array[5], rssi_out_array[6], rssi_out_array[7], rssi_out_array[8], rssi_out_array[9]);
				Log_Info("rssi_out_array[10]:%d, [11]:%d, [12]:%d \r\n", 
					rssi_out_array[10], rssi_out_array[11], rssi_out_array[12]);
			}
#endif	/* CONFIG_AUDREY_INDV_LOG */
		}
	}
#endif	/* CONFIG_AUDREY_INDV */

#if CONFIG_AUDREY_DBG_UPLOAD
	if(is_dbg_mode()) {
		// 入室～退室完了までのビーコン情報をバックアップ
		for(i = 0; i < badge_dbg_cnt; i++) {
			if(!memcmp(bd_addr, badge_data[i].conf.bd_addr, BD_ADDR_LEN)) {
				if((badge_dbg[i].cnt < BADGE_DBG_RSSI_MAX - 10)
				 || (badge_beacon_type == BADGE_TYPE_EXIT && badge_dbg[i].cnt < BADGE_DBG_RSSI_MAX)) {
					badge_dbg[i].buf[badge_dbg[i].cnt].time = scale_info.time;
					badge_dbg[i].buf[badge_dbg[i].cnt].rssi = rssi;
					badge_dbg[i].cnt++;
				}
			}
		}
	}
	if(battery >= 0x10) return;
#endif
	Log_Debug("[Badge] Receive beacon: "BD_ADDR_FMT", RSSI: %d, Battery: %d\r\n"
	, BD_ADDR_ARG(bd_addr), rssi, battery);
	for(i = 0; i < badge_config_cnt; i++) {
		if(!memcmp(bd_addr, &badge_data[i].conf.bd_addr, BD_ADDR_LEN)) {
			if(badge_data[i].buf[badge_beacon_type].cnt < BADGE_RCV_MAX) {
				badge_data[i].buf[badge_beacon_type].rssi[badge_data[i].buf[badge_beacon_type].cnt] = rssi;
				badge_data[i].buf[badge_beacon_type].cnt++;
				badge_data[i].battery = battery & 0x01;
				break;
			}
		}
	}
/*
	// For Debug 未登録BDアドレスを判定対象バッジとして追加
	if(i >= badge_config_cnt && badge_config_cnt < BADGE_PET_MAX) {
		memcpy(badge_data[badge_config_cnt].conf.bd_addr, bd_addr, BD_ADDR_LEN);
		badge_data[badge_config_cnt].conf.body = 0;
		badge_data[badge_config_cnt].buf[badge_beacon_type].rssi[0] = rssi;
		badge_data[badge_config_cnt].buf[badge_beacon_type].cnt = 1;
		badge_data[badge_config_cnt].buf[badge_beacon_type ^ 1].cnt = 0;
		badge_data[badge_config_cnt].battery = battery;
		Log_Info("[Badge] Add "BD_ADDR_FMT"\r\n", BD_ADDR_ARG(badge_data[badge_config_cnt].conf.bd_addr));
		badge_config_cnt++;
	}
*/
}

/* 個体識別 判定処理 */
void badge_judge(int body, u8 *bd_result)
{
	int i, j;
	int cnt;
	int scr;
#if CONFIG_AUDREY_INDV
	int				cat_idx = IND_UNIDENTIFIED;
	int				count = 0;
#endif /* CONFIG_AUDREY_INDV */

#if CONFIG_AUDREY_INDV

#if CONFIG_AUDREY_INDV_LOG
	char			*rssi_higher_array;
	char			*rssi_out_array;
	Log_Info("\r\n");
	Log_Info("[Badge] Start Judge \r\n");
#endif /* CONFIG_AUDREY_INDV_LOG */

	// 各バッチの値集計
	//--------------------------------------------------------------------------
	for(i = 0; i < badge_config_cnt; i++) {
#if CONFIG_AUDREY_INDV_LOG
		Log_Info("\r\n");
		Log_Info("Value Calculation\r\n");
		Log_Info("===========================\r\n");
		Log_Info("[Badge] Judge for "BD_ADDR_FMT"\r\n", BD_ADDR_ARG(badge_data[i].conf.bd_addr));

		// RSSI 上位値3つ
		rssi_higher_array = badge_data[i].buf[BADGE_TYPE_ENTRY].rssi_higher_array;
		Log_Info("rssi_higher_array[0]:%d, [1]:%d, [2]:%d, [3]:%d, [4]:%d, [5]:%d, [6]:%d, [7]:%d \r\n", 
			rssi_higher_array[0], rssi_higher_array[1], rssi_higher_array[2], rssi_higher_array[3], 
			rssi_higher_array[4], rssi_higher_array[5], rssi_higher_array[6], rssi_higher_array[7]);

		// 退出値
		rssi_out_array = badge_data[i].buf[BADGE_TYPE_ENTRY].rssi_out_array;
		Log_Info("rssi_out_array[0]:%d, [1]:%d, [2]:%d, [3]:%d, [4]:%d \r\n", 
			rssi_out_array[0], rssi_out_array[1], rssi_out_array[2], rssi_out_array[3], rssi_out_array[4]);
		Log_Info("rssi_out_array[5]:%d, [6]:%d, [7]:%d, [8]:%d, [9]:%d \r\n", 
			rssi_out_array[5], rssi_out_array[6], rssi_out_array[7], rssi_out_array[8], rssi_out_array[9]);
		Log_Info("rssi_out_array[10]:%d, [11]:%d, [12]:%d \r\n", 
			rssi_out_array[10], rssi_out_array[11], rssi_out_array[12]);
#endif	/* CONFIG_AUDREY_INDV_LOG */

		// RSSI上位3点の平均値算出
		badge_data[i].buf[BADGE_TYPE_ENTRY].rssi_higher_avg =
			badge_avg(badge_data[i].buf[BADGE_TYPE_ENTRY].rssi_higher_array, BADGE_RSSI_HIGHER_SHORT_NUM);

#if CONFIG_AUDREY_INDV_LOG
		Log_Info("[Badge] rssi_higher_avg:%d \r\n", badge_data[i].buf[BADGE_TYPE_ENTRY].rssi_higher_avg);
#endif /* CONFIG_AUDREY_INDV_LOG */

		// RSSI上位7点の平均値算出
		badge_data[i].buf[BADGE_TYPE_ENTRY].rssi_higher_long_avg =
			badge_avg(badge_data[i].buf[BADGE_TYPE_ENTRY].rssi_higher_array, BADGE_RSSI_HIGHER_LONG_NUM);

#if CONFIG_AUDREY_INDV_LOG
		Log_Info("[Badge] rssi_higher_long_avg:%d \r\n", badge_data[i].buf[BADGE_TYPE_ENTRY].rssi_higher_long_avg);
#endif /* CONFIG_AUDREY_INDV_LOG */

		// 退出時の差分値算出
		badge_data[i].buf[BADGE_TYPE_ENTRY].rssi_out_diff =
			badge_get_out_diff(badge_data[i].buf[BADGE_TYPE_ENTRY].rssi_out_array);

#if CONFIG_AUDREY_INDV_LOG
		Log_Info("[Badge] rssi_out_diff:%d \r\n", badge_data[i].buf[BADGE_TYPE_ENTRY].rssi_out_diff);
#endif /* CONFIG_AUDREY_INDV_LOG */

	}

	// 体重による候補猫除外対応
	// 「候補猫の登録体重が、トイレに入った猫体重より±２０％以上差異がある。」
	//  且
	// 「トイレに入った猫体重の±５％以内に他猫存在」
	//  する場合は、候補猫を外して再度個体識別する。
	//--------------------------------------------------------------------------

	// 除外猫フラグをクリアする
	badge_ini_exclution_flg();
	count = 0;										// 無限ループにならない為のフェールセーフ用

	do {

		// ポイントをクリアする
		badge_ini_point();

		// 有効猫が１匹もいない時の処理
		if (badge_find_valid_first_cat() == IND_UNIDENTIFIED) {
			cat_idx = IND_UNIDENTIFIED;
			INDV_LOG("There is not a valid cat");
			break;
		}

		// ポイント付け
		//--------------------------------------------------------------------------
		// 登録バッチ中の入室中RSSIを比較して、ポイントを付ける
		// 上位３点チェックし、近い猫が居たら、上位７点チェックを行う。
		if (badge_point_entering_rssi() == TRUE) {
			badge_point_entering_long_rssi();
		}

		// 体重
		badge_point_body(body);

		// 退室時の差分値比較
		badge_point_leaving_rssi();

		// ポイント確認
		// ポイントが4ポイント以上の猫を見つけ、同点が複数居たらなら、識別不可とする。
		//--------------------------------------------------------------------------
		cat_idx = badge_judge_cat();

		count++;
		if (count > BADGE_PET_MAX) {
			Log_Error("[Badge] fail safe. There is not Candidate cats. \r\n");
			cat_idx = IND_UNIDENTIFIED;
			break;
		}
	// 登録体重による候補猫除外判定
	// また、調べる猫が居なくなったら識別不能で処理を抜ける。
	} while ((badge_judge_exclude_cat(body, &cat_idx) == TRUE) && (count <= BADGE_PET_MAX));

	if(cat_idx != IND_UNIDENTIFIED) {
		// 判定成功
		for(cnt = 0, bd_result += (BD_ADDR_LEN - 1); cnt < BD_ADDR_LEN; cnt++, bd_result--) {
			*bd_result = badge_data[cat_idx].conf.bd_addr[cnt];
		}
#if CONFIG_AUDREY_INDV_EMU_LOG
		Log_Info("===========================\r\n");
		Log_Info("[Badge][Final Judge] Time;%s, Addr;"BD_ADDR_FMT", Point:%d point\r\n", badge_time, BD_ADDR_ARG(badge_data[cat_idx].conf.bd_addr), badge_data[cat_idx].point);
		Log_Info("[Badge] RSSI_IN, WEIGHT, RSSI_OUT point:%d,%d,%d;\r\n", badge_data[cat_idx].rssi_in_point, badge_data[cat_idx].weight_point, badge_data[cat_idx].rssi_out_point);
		Log_Info("===========================\r\n");
#endif /* CONFIG_AUDREY_INDV_EMU_LOG */

	} else {
		memset(bd_result, 0x00, BD_ADDR_LEN);
#if CONFIG_AUDREY_INDV_EMU_LOG
		Log_Info("===========================\r\n");
		Log_Info("[Badge][Final Judge] Time;%s, Not Found \r\n", badge_time);
		Log_Info("===========================\r\n");
#endif /* CONFIG_AUDREY_INDV_EMU_LOG */
	}




#else	/* CONFIG_AUDREY_INDV */

	for(i = 0; i < badge_config_cnt; i++) {
		// 入室時RSSI中央値取得
		badge_data[i].buf[BADGE_TYPE_ENTRY].val
		 = badge_median(badge_data[i].buf[BADGE_TYPE_ENTRY].rssi, badge_data[i].buf[BADGE_TYPE_ENTRY].cnt);
#ifndef CONFIG_AUDREY_DBG_UPLOAD
		// 入室時分散値取得
		badge_data[i].buf[BADGE_TYPE_ENTRY].dis
		 = badge_dispersion(badge_data[i].buf[BADGE_TYPE_ENTRY].rssi, badge_data[i].buf[BADGE_TYPE_ENTRY].cnt);
#endif
		// 退室時のRSSI中央値取得
		badge_data[i].buf[BADGE_TYPE_EXIT].val
		 = badge_median(badge_data[i].buf[BADGE_TYPE_EXIT].rssi, badge_data[i].buf[BADGE_TYPE_EXIT].cnt);
/*
		// 退室時の分散値は判定で使用しない
		// 退室時分散値取得
		badge_data[i].buf[BADGE_TYPE_EXIT].dis
		 = badge_dispersion(badge_data[i].buf[BADGE_TYPE_EXIT].rssi, badge_data[i].buf[BADGE_TYPE_EXIT].cnt);
*/
#if AUDREY_LOG_INFO
		Log_Info("[Badge] Judge for "BD_ADDR_FMT"\r\n", BD_ADDR_ARG(badge_data[i].conf.bd_addr));
		Log_Info("Weight=%d, ENTRY rcv=%d dis=%d val=%d, EXIT rcv=%d val=%d\r\n"
		, badge_data[i].conf.body
		, badge_data[i].buf[BADGE_TYPE_ENTRY].cnt
		, badge_data[i].buf[BADGE_TYPE_ENTRY].dis
		, badge_data[i].buf[BADGE_TYPE_ENTRY].val
		, badge_data[i].buf[BADGE_TYPE_EXIT].cnt
		, badge_data[i].buf[BADGE_TYPE_EXIT].val);
		Log_Info("ENTRY RSSI");
		for(j = 0; j < badge_data[i].buf[BADGE_TYPE_ENTRY].cnt; j++)
		{
			Log_Info(",%d", badge_data[i].buf[BADGE_TYPE_ENTRY].rssi[j]);
		}
		Log_Info("\r\n");
		Log_Info("EXIT RSSI");
		for(j = 0; j < badge_data[i].buf[BADGE_TYPE_EXIT].cnt; j++)
		{
			Log_Info(",%d", badge_data[i].buf[BADGE_TYPE_EXIT].rssi[j]);
		}
		Log_Info("\r\n");
#endif
		// 体重判定
		if(badge_data[i].conf.body != 0 && abs(badge_data[i].conf.body - body) <= BADGE_THR_BODY_DIFF) {
			badge_data[i].score++;
			Log_Info("Score for Weight +1\r\n");
		}
		// 入室時RSSI判定
		if(badge_data[i].buf[BADGE_TYPE_ENTRY].val < 0
#ifndef CONFIG_AUDREY_DBG_UPLOAD
		 && badge_data[i].buf[BADGE_TYPE_ENTRY].dis <= BADGE_THR_ENTRY_DIS
#endif
		 && badge_data[i].buf[BADGE_TYPE_ENTRY].val >= BADGE_THR_ENTRY_RSSI) {
			badge_data[i].score++;
			Log_Info("Score for Entry +1\r\n");
		}
		// 退室時RSSI判定
		if(
/*
		// 退室時の分散値は判定で使用しない
		 badge_data[i].buf[BADGE_TYPE_EXIT].dis <= BADGE_THR_EXIT_DIS && 
*/
		 badge_data[i].buf[BADGE_TYPE_ENTRY].val != 0
		 && ((badge_data[i].buf[BADGE_TYPE_ENTRY].val - badge_data[i].buf[BADGE_TYPE_EXIT].val >= BADGE_THR_EXIT_DIFF)
		    || badge_data[i].buf[BADGE_TYPE_EXIT].val == 0)) {
			badge_data[i].score++;
			Log_Info("Score for Exit +1\r\n");
		}
	}
	// スコア３のbeaconチェック
	for(i = 0, scr = 0; i < badge_config_cnt; i++) {
		if(badge_data[i].score == 3) {
			if(!scr) {
				scr = i + 1;
			} else {
				// 同スコアが複数ある場合は判定失敗
				scr = 0;
				break;
			}
		}
	}
	// スコア３のbeaconが１つのみある場合はスコア２のチェックは行わない
	if(!scr) {
		// スコア２のbeaconチェック
		for(i = 0, scr = 0; i < badge_config_cnt; i++) {
			if(badge_data[i].score == 2) {
				if(!scr) {
					scr = i + 1;
				} else {
					// 同スコアが複数ある場合は判定失敗
					scr = 0;
					break;
				}
			}
		}
	}
	if(scr != 0) {
		// 判定成功
		for(cnt = 0, bd_result += (BD_ADDR_LEN - 1); cnt < BD_ADDR_LEN; cnt++, bd_result--) {
			*bd_result = badge_data[scr - 1].conf.bd_addr[cnt];
		}
	} else {
		// スコア２以上が無ければ判定失敗
		memset(bd_result, 0x00, BD_ADDR_LEN);
	}

#endif	/* CONFIG_AUDREY_INDV */

	// 低電池残量のバッジをピックアップ
	for(i = 0, badge_low_num = 0; i < badge_config_cnt; i++) {
		if(badge_data[i].battery != 0) {
			for(cnt = 0; cnt < BD_ADDR_LEN; cnt++) {
				badge_low_info[badge_low_num][BD_ADDR_LEN - 1 - cnt] = badge_data[i].conf.bd_addr[cnt];
			}
			badge_low_num++;
		}
	}
}

