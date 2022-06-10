/*
 * Copyright (C) 2017 SHARP Corporation. All rights reserved.
 */
#include <platform_opts.h>
#include "FreeRTOS.h"
#include "task.h"
#include <platform_stdlib.h>
#include "semphr.h"
#include "device.h"
#include "serial_api.h"
#include "timers.h"
#include "gpio_api.h"
#include "gpio_irq_api.h"
#include "scale.h"
#include "version.h"
#include "weight.h"
#include "state_manager.h"
#include "osdep_service.h"
#include "serial_ex_api.h"
#include "pinmap.h"
#include <sntp/sntp.h>
#include "rtc_api.h"
#include "data_upload.h"

#define KEY_ENTER				0xd		// '\r'
#define SCALE_UART_BUF_LEN		64

#define SCALE_THRESHOLD_UP		2500	// 体重変化上限値
#define SCALE_THRESHOLD_DOWN	2500	// 体重変化下限値
#define SCALE_THRESHOLD_URINE	   4	// 尿量変化閾値

#define SCALE_UART_VER_LEN		16
#define SCALE_UART_GRV_LEN		5
#define SCALE_UART_ERR_LEN		2
#define SCALE_UART_ATRC_LEN		13
#define SCALE_INIT_NUM			5		// 初期測定回数
#define SCALE_RETRY_NUM			20		// 重量取得不可監視回数
#define SCALE_TO_WEIGHT_NUM		60		// 重量測定未実施監視回数
#define SCALE_OVERLOAD_NUM		10		// 過積載監視回数
#define SCALE_PSV_CNT			15		// 省電力遷移前重量取得回数
#if CONFIG_AUDREY_DBG_UPLOAD
#define SCALE_PSV_CNT_DBG		WEIGHT_DBG_MAX + SCALE_PSV_CNT
										// 省電力遷移前重量取得回数(デバッグモード用)
#endif

typedef enum {
	SCALE_GRV_IDLE = 0,					// 重量加速度確認状態IDLE
	SCALE_GRV_READ,						// 重量加速度取得待ち
	SCALE_GRV_WRITE,					// 重量加速度設定待ち
	SCALE_GRV_MAX,
} SCALE_GRV;

SCALE_INFO scale_info;
SCALE_MODE scale_mode;
SCALE_GRV scale_grv_st = 0;
u8 scale_rcv_buf[SCALE_UART_BUF_LEN] = "\0";
u8 scale_ver[SCALE_UART_VER_LEN+1];
u8 scale_grv[SCALE_UART_GRV_LEN+1];
MSG_Param_t scale_error_retry = 0;
MSG_Param_t scale_error_rcv = 0;
int scale_to_weight_cnt = 0;
int scale_overloar_cnt = 0;
u8 scale_rcv_cnt = 0;
bool scale_psv = FALSE;
bool scale_init_done = FALSE;
bool is_urine_unstable = FALSE;
int scale_init_cnt = 0;
int scale_retry_cnt = 0;
int scale_psv_cnt = 0;

gpio_t scale_gpio;
gpio_irq_t scale_int;
gpio_t scale_gpio_reset;
gpio_t scale_gpio_fup;

TimerHandle_t scale_timer_cmd;

serial_t scale_uart_sobj;
xSemaphoreHandle scale_weight_sema = NULL;

//char ret;	// For Debug

/* Scale通信 ファーム更新開始通知 */
void scale_ota_start(void)
{
	xTimerStop(scale_timer_cmd, 0);
	scale_mode = SCALE_MODE_OTA;
}

/* Scale通信 ファーム更新停止通知 */
void scale_ota_stop(void)
{
	scale_reset();
}

/* Scale通信 データ送信 */
void scale_uart_send_string(u8 *str)
{
	unsigned int i=0;
	while (str[i] != '\0') {
		serial_putc(&scale_uart_sobj, str[i]);
		i++;
	}
}

/* Scale通信 タイマ値変更 */
void scale_timer_change(int time)
{
#if CONFIG_AUDREY_SCALE_1SEC
	xTimerChangePeriod(scale_timer_cmd, ( 1000 / portTICK_RATE_MS ), 0);
#else
	xTimerChangePeriod(scale_timer_cmd, ( time / portTICK_RATE_MS ), 0);
#endif
}

/* Scale通信 コマンド送信後タイムアウト */
void scale_timeout_cmd_handler(xTimerHandle pxTimer)
{
	u8 buf[SCALE_UART_ATRC_LEN+1];
	int body_up, body_low;
	MSG_Param_t err;

	// ATRV、ATRW、ATRC、ATRGの応答が一定回数連続でない場合はScaleをリセット
	if(scale_retry_cnt >= SCALE_RETRY_NUM) {
		Log_Error("[Scale] No response, mode=%d\r\n", scale_mode);
		scale_init_done = TRUE;
		scale_reset();
		switch(scale_mode)
		{
			case SCALE_MODE_PSV:
				if(scale_grv_st != SCALE_GRV_IDLE) {
					err = PARAM_SCL_ERR_NO_RSP_GRAVITY;
				} else {
					err = PARAM_SCL_ERR_NO_RSP_WEIGHT;
				}
				break;
			case SCALE_MODE_BASE:
			case SCALE_MODE_CHK:
				err = PARAM_SCL_ERR_NO_RSP_WEIGHT;
				break;
			case SCALE_MODE_VER:
				err = PARAM_SCL_ERR_NO_RSP_VER;
				break;
			case SCALE_MODE_ATRC:
				err = PARAM_SCL_ERR_NO_RSP_THR;
				break;
		}
		if(err != scale_error_retry) {
			// エラー通知(エラー番号が前回と異なる場合のみ)
			scale_error_retry = err;
			SendMessageToStateManager(MSG_ERR_SCALE_W, err);
		}
		return;
	}

	switch (scale_mode)
	{
		// 省電力モード
		case SCALE_MODE_PSV:
			// 重量加速度確認・更新は、省電力モード中に行う
			if(scale_grv_st == SCALE_GRV_READ) {
				// 現在値取得要求
				scale_uart_send_string("ATRG\r");
				// 60秒間隔→1秒間隔に変更
				scale_timer_change(1000);
				scale_retry_cnt++;
			} else if(scale_grv_st == SCALE_GRV_WRITE){
				// 重量加速度更新要求 ATRGxxxxx\r
				scale_uart_send_string("ATRG");
				scale_uart_send_string(scale_grv);
				scale_uart_send_string("\r");
				scale_retry_cnt++;
			} else {
				scale_uart_send_string("ATRW\r");
			}
			break;

		// 基準体重測定待ち
		// 体重計チェック中
		case SCALE_MODE_BASE:
		case SCALE_MODE_CHK:
			scale_uart_send_string("ATRW\r");
			scale_retry_cnt++;
			break;

		// バージョン情報取得待ち
		case SCALE_MODE_VER:
			scale_uart_send_string("ATRV\r");
			scale_retry_cnt++;
			break;

		// 上限下限値送信待ち
		case SCALE_MODE_ATRC:
			// Scale Ver 1.10以前 ： 上限が0g以下の時は10g、下限が負の時は0gに設定
			// Scale Ver 1.11以降 ： 上限、下限とも負でもそのまま設定
			// 上限値設定
			body_up = weight_info.body * 10;
			if (body_up < 1000000 - SCALE_THRESHOLD_UP) {
				body_up += SCALE_THRESHOLD_UP;
			} else {
				body_up = 999999;
			}
			if(body_up <= 0) {
				if(strcmp(scale_ver, "1.11") < 0) {
					body_up = 100;
				}
			}
			// 下限値設定
			body_low = weight_info.body * 10;
			body_low -= SCALE_THRESHOLD_DOWN;
			if (body_low < 0) {
				if(strcmp(scale_ver, "1.11") < 0) {
					body_low = 0;
				}
			}
			// 上限下限値設定要求 ATRCxxxxxxxxxxxx\r
			Log_Scale("[Scale] Upper %d.%dg, Lower %d.%dg\r\n", body_up/10, body_up%10, body_low/10, body_low%10);
			sprintf(&buf[0], "%06d%06d\r", body_up, body_low);
			buf[SCALE_UART_ATRC_LEN] = 0;
			scale_uart_send_string("ATRC");
			scale_uart_send_string(buf);
			scale_retry_cnt++;
			break;

		default:
			break;

	}
}

/* Scale通信 INT割り込み */
void scale_int_irq_handler(uint32_t id, gpio_irq_event event)
{
	Log_Notify("\r\n[Scale] INT LOW\r\n");
	if(scale_mode == SCALE_MODE_PSV) {
		scale_mode = SCALE_MODE_CHK;
		// 1秒タイマ
		scale_timer_change(1000);
	}
}

/* Scale通信 バージョン情報受信 */
void scale_uart_rcv_ver(u8 *buf)
{
	scale_retry_cnt = 0;
	if(scale_mode == SCALE_MODE_VER) {
		//受信したVersion情報を保存
		memcpy(scale_ver, buf, SCALE_UART_VER_LEN);
		scale_ver[SCALE_UART_VER_LEN] = 0;
		Log_Notify("Wireless ver: %s\r\n", AUDREY_VERSION);
		Log_Notify("   Scale ver: %s\r\n", scale_ver);
		scale_mode = SCALE_MODE_BASE;
#if CONFIG_AUDREY_FWUP_ALONE
		SendMessageToStateManager(MSG_FW_UPDATE_SA_VER, 0);
#endif
	}
}

/* Scale通信 重量加速度受信 */
void scale_uart_rcv_gravity(u8 *buf)
{
	scale_retry_cnt = 0;
	if(scale_grv_st == SCALE_GRV_WRITE) {
		// 更新完了
		scale_grv_st = SCALE_GRV_IDLE;
		if(scale_mode == SCALE_MODE_PSV) {
			scale_init_cnt = 0;
			scale_mode = SCALE_MODE_BASE;
		}
	} else if(scale_grv_st == SCALE_GRV_READ) {
		// 現在値と更新値比較
		if(!memcmp(buf, scale_grv, SCALE_UART_GRV_LEN)) {
			// 現在値＝更新値なら更新不要
			Log_Scale("[Scale] Keep Gravity value %5s\r\n", scale_grv);
			if(scale_mode == SCALE_MODE_PSV) {
				// 1秒間隔→60秒間隔に変更
				scale_timer_change(60000);
			}
			scale_grv_st = SCALE_GRV_IDLE;
		} else {
			// 現在値≠更新値なら更新
			Log_Scale("[Scale] Change Gravity value %5s\r\n", scale_grv);
			scale_grv[5] = 0;
			scale_grv_st = SCALE_GRV_WRITE;
		}
	}
}

/* Scale通信 重量加速度確認 */
void scale_chk_gravity(u8 *buf)
{
	int i;

	for(i = 0; i < SCALE_UART_GRV_LEN; i++) {
		if(*(buf + i) < '0' || *(buf + i) > '9') {
			return;
		}
	}
	scale_timer_change(1000);
	scale_grv_st = SCALE_GRV_READ;
	memcpy(scale_grv, buf, SCALE_UART_GRV_LEN);
	Log_Scale("[Scale] Get Gravity\r\n");
}

extern void alert_notification_req(unsigned int);

/* Scale通信 ERROR受信 */
void scale_uart_rcv_error(unsigned char *buf)
{
	MSG_Param_t err;

	if((scale_rcv_cnt == SCALE_UART_ERR_LEN +5) && *buf >= '0' && *buf <= '9' && *(buf + 1) >= '0' && *(buf + 1) <= '9' ) {
		err = atoi(buf);
		if(err == PARAM_SCL_ERR_OVERLOAD_BODY || err == PARAM_SCL_ERR_OVERLOAD_URINE) {
			// 過積載が一定時間続けばアラート通知
			if(scale_error_rcv == err) {
				if(scale_overloar_cnt < SCALE_OVERLOAD_NUM) {
					scale_overloar_cnt++;
					if(scale_overloar_cnt >= SCALE_OVERLOAD_NUM) {
						SendMessageToStateManager(MSG_ERR_SCALE, err);
					}
				}
			} else {
				scale_overloar_cnt = 1;
			}
			scale_error_rcv = err;
			scale_retry_cnt = 0;
		} else if((err != 01) && scale_error_rcv != err) {
			// エラー通知(エラー番号が前回と異なる場合のみ)
			scale_error_rcv = err;
			SendMessageToStateManager(MSG_ERR_SCALE, scale_error_rcv);
			Log_Error("[Scale] Alert notification error code: %d\r\n", scale_error_rcv);
		}
	}
}

/* Scale通信 重量データ受信 */
void scale_uart_rcv_weight(unsigned char *rcv_buf)
{
	int i;
	int dig;
	int body;
	int urine;
	int urine_prev;
	bool body_neg = FALSE;
	bool urine_neg = FALSE;

	// 端末起動直後は指定回数重量値を必ず取得する
	if(scale_mode == SCALE_MODE_BASE && scale_init_cnt < SCALE_INIT_NUM) {
		scale_init_cnt++;
	}
	scale_retry_cnt = 0;
	scale_overloar_cnt = 0;
	// stable/unstable情報取得
	scale_info.stable.psv = (*(rcv_buf) == 'Y' ? 'Y' : 'N');
	scale_info.stable.body = (*(rcv_buf+13) == 'S' ? 'S' : 'U');
	scale_info.stable.urine = (*(rcv_buf+14) == 'S' ? 'S' : 'U');
	// Y or N 取得
	scale_psv = (*rcv_buf++ == 'Y' ? TRUE : FALSE);
	// 体重計値取得
	for(i=0, dig = 100000, body = 0, body_neg = FALSE; i < 6; i++, rcv_buf++) {
		if(*rcv_buf == '-') {
			*rcv_buf = '0';
			body_neg = TRUE;
		}
		body += (*rcv_buf & 0x0f) * dig;
		dig = dig / 10;
	}
	// 尿量計値取得
	for(i=0, dig = 100000, urine = 0, urine_neg = FALSE; i < 6; i++, rcv_buf++) {
		if(*rcv_buf == '-') {
			*rcv_buf = '0';
			urine_neg = TRUE;
		}
		urine += (*rcv_buf & 0x0f) * dig;
		dig = dig / 10;
	}
/*  自動でのゼロ補正は行わないようにする（負の値はゼロとして扱う）
	// 負の値があればゼロ補正要求
	if(body_neg && urine_neg) {
		Log_Scale("[Scale] Send 0 for body&urine\r\n");
		scale_uart_send_string("ATRZ3\r");
	} else if(!body_neg && urine_neg) {
		Log_Scale("[Scale] Send 0 for urine\r\n");
		scale_uart_send_string("ATRZ2\r");
	} else if(body_neg && !urine_neg) {
		Log_Scale("[Scale] Send 0 for body\r\n");
		scale_uart_send_string("ATRZ1\r");
	}
*/

	urine_prev = scale_info.urine;
	// 最新情報バックアップ（負の値は負の値のまま扱う）
	scale_info.body = (body_neg ? 0 - body/10 : body/10);
	scale_info.urine = (urine_neg ? 0 - urine/10 : urine/10);
	scale_info.time = rtc_read();
	scale_to_weight_cnt++;
	// 省電力状態で尿量が変化した場合は1秒間隔で重量取得
	if(scale_mode == SCALE_MODE_PSV
	 && (scale_info.urine - urine_prev > SCALE_THRESHOLD_URINE || urine_prev - scale_info.urine > SCALE_THRESHOLD_URINE)) {
		is_urine_unstable = TRUE;
	}
	if(scale_to_weight_cnt > SCALE_TO_WEIGHT_NUM) {
		// 一定回数重量測定が実施できてない状態であれば異常状態としてシステム再起動
		Log_Error("[Scale] Can't update weight data\r\n");
		SendMessageToStateManager(MSG_ERR_SCALE_FATAL, PARAM_SCL_ERR_WEIGHT_NONE);
	} else {
		// 重量測定へ
		rtw_up_sema_from_isr(&scale_weight_sema);
	}
}

void scale_chk_next(int ret)
{
	Log_Debug("scale_mode = %d, scale_init_cnt = %d, ret = %d, scale_psv = %d\r\n", scale_mode, scale_init_cnt, ret, scale_psv);
	// 体重測定中で測定不要になった場合、省電力状態なのに上限下限通知が無効の場合、はATRC送信
	if(scale_mode == SCALE_MODE_PSV) {
		if(!scale_psv) {
			// 省電力状態なのに上限下限通知が無効の場合は上限下限値通知
			scale_timer_change(1000);
			scale_mode = SCALE_MODE_ATRC;
			Log_Notify("[Scale] Send Limit value in Power save\r\n");
		} else if(ret || is_urine_unstable) {
			// 省電力状態なのに変動がある場合は１秒間隔のチェックに切り替える
			scale_timer_change(1000);
			scale_mode = SCALE_MODE_CHK;
			Log_Notify("[Scale] Detect weight change in Power save\r\n");
		}
	} else if((scale_mode == SCALE_MODE_BASE && scale_init_cnt >= SCALE_INIT_NUM)
	 || (scale_mode == SCALE_MODE_CHK && !ret && !is_urine_unstable)) {
		// １秒間隔のチェック中に退室安定した場合は上限下限値通知
		scale_init_done = TRUE;
		if(scale_psv_cnt) {
			scale_psv_cnt--;
		} else {
			scale_mode = SCALE_MODE_ATRC;
		}
	} else if(ret || is_urine_unstable) {
		is_urine_unstable = FALSE;
#if CONFIG_AUDREY_DBG_UPLOAD
		scale_psv_cnt = (is_dbg_mode() ? SCALE_PSV_CNT_DBG : SCALE_PSV_CNT);
#else
		scale_psv_cnt = SCALE_PSV_CNT;
#endif
	}
}

/* Scale通信 上限下限値通知応答 */
void scale_uart_rcv_ok(void)
{
	scale_retry_cnt = 0;
	if(scale_mode == SCALE_MODE_ATRC) {
		// 送信間隔を60秒に変更
		scale_timer_change(60000);
		scale_mode = SCALE_MODE_PSV;
	}
}

/* Scale通信 UART受信割り込み */
void scale_uart_irq(uint32_t id, SerialIrq event)
{
	serial_t    *sobj = (serial_t *)id;
	unsigned char rc=0;
	int i;

	if(event == RxIrq) {
		rc = serial_getc(sobj);
		// ファーム更新中の場合はレスポンスチェック
		if (scale_mode == SCALE_MODE_FUP) {
			if(rc == 'O' || rc == 'F') {
				scale_up_res(rc);
			}
			return;
		}

		if(rc == KEY_ENTER){
			if(scale_rcv_cnt>0){
				// "OK"受信チェック
				if(!memcmp(scale_rcv_buf, "OK", 2)) {
#if CONFIG_AUDREY_INDV_EMU == 0
					Log_Notify("[Scale] Receive: %s[DONE]\r\n", scale_rcv_buf);
#endif /* CONFIG_AUDREY_INDV_EMU */
					if(scale_rcv_cnt==15 || scale_rcv_cnt==17) {
						for(i=3; i<15; i++) {
							if((scale_rcv_buf[i] < '0' || scale_rcv_buf[i] > '9') && scale_rcv_buf[i] != '-') {
								Log_Error("[Scale] OK data is incorrect\r\n");
								break;
							}
						}
						if(i >= 15 && (scale_rcv_buf[2]== 'Y' || scale_rcv_buf[2]== 'N')) {
#if CONFIG_AUDREY_INDV_EMU == 0
							scale_uart_rcv_weight(&scale_rcv_buf[2]);
#elif CONFIG_AUDREY_INDV_EMU == 1
							scale_retry_cnt = 0;
#endif /* CONFIG_AUDREY_INDV_EMU */
						}
					} else if(scale_rcv_cnt==2) {
						scale_uart_rcv_ok();
					}
				// "VER"受信チェック
				} else if(!memcmp(scale_rcv_buf, "VER", 3)) {
					Log_Notify("[Scale] Receive: %s[DONE]\r\n", scale_rcv_buf);
					scale_uart_rcv_ver(&scale_rcv_buf[3]);
				// "GRV"受信チェック
				} else if(!memcmp(scale_rcv_buf, "GRV", 3)) {
					Log_Notify("[Scale] Receive: %s[DONE]\r\n", scale_rcv_buf);
					if(scale_rcv_cnt == 3+SCALE_UART_GRV_LEN) {
						scale_uart_rcv_gravity(&scale_rcv_buf[3]);
					}
				// "DAIG"受信チェック
				} else if(!memcmp(scale_rcv_buf, "DIAG", 4)) {
					Log_Notify("[Scale] Receive: %s[DONE]\r\n", scale_rcv_buf);
				// "ERROR"受信チェック
				} else if(!memcmp(scale_rcv_buf, "ERROR", 5)) {
#if CONFIG_AUDREY_INDV_EMU != 1
					Log_Error("[Scale] Receive: %s[DONE]\r\n", scale_rcv_buf);
#endif
					if(scale_rcv_cnt == 5+SCALE_UART_ERR_LEN) {
						scale_uart_rcv_error(&scale_rcv_buf[5]);
					}
				}
				rtw_memset(scale_rcv_buf,'\0',scale_rcv_cnt);
				scale_rcv_cnt=0;
			}
		} else {
			// 先頭が無効データの場合は破棄
			if(scale_rcv_cnt == 0 && (rc != 'O' && rc != 'V' && rc != 'G' && rc != 'D' && rc != 'E')) {
				return;
			} else if(scale_rcv_cnt < (SCALE_UART_BUF_LEN - 1)){
				scale_rcv_buf[scale_rcv_cnt] = rc;
				scale_rcv_cnt++;
			}
			else if(scale_rcv_cnt == (SCALE_UART_BUF_LEN - 1)){
				scale_rcv_buf[scale_rcv_cnt] = '\0';
			}
		}
	}
}


/* Scale通信省電力中確認 */
int scale_is_psv(void) {

	if(scale_mode == SCALE_MODE_PSV) {
		scale_uart_send_string("ATRW\r");
		return 1;
	} else {
		return 0;
	}
}

/* Scale初期測定中確認 */
int scale_is_init(void) {

	if((scale_init_done == TRUE) || !strcmp(scale_ver, "ERROR")) {
		return 0;
	} else {
		return 1;
	}
}

/* Scale通信 リセット */
void scale_reset(void)
{
	int i;

	scale_init_cnt = 0;
	scale_retry_cnt = 0;
	scale_grv_st = SCALE_GRV_IDLE;

	// Scale Reset
	vTaskDelay(20 / portTICK_RATE_MS);
	gpio_write(&scale_gpio_reset, 0);
	vTaskDelay(100 / portTICK_RATE_MS);
	gpio_write(&scale_gpio_reset, 1);
	vTaskDelay(100 / portTICK_RATE_MS);

	i = scale_is_update();
	if(i) {
		// Scaleファーム更新失敗時はバージョン情報を"ERROR"にする
		Log_Error("\r\n[Scale] update Fail Round %d\r\n", i);
		strcpy(scale_ver, "ERROR");
		scale_mode = SCALE_MODE_BASE;

	} else if(scale_mode == SCALE_MODE_FUP) {
		// Scale更新後はOTA中状態に戻す(最終的にシステム再起動される)
		scale_ota_start();
	} else {
		// 起動直後はバージョン情報取得から
		scale_mode = SCALE_MODE_VER;

		// バージョン情報取得待ち、基準体重測定待ち、体重チェック中の要求間隔タイマ(1秒)
#if CONFIG_AUDREY_INDV_EMU == 0
		if( xTimerIsTimerActive(scale_timer_cmd) != pdFALSE ) {
			xTimerDelete(scale_timer_cmd, 0);
		}
		scale_timer_cmd = xTimerCreate("SCALE_TIMER",(1000 / portTICK_RATE_MS), pdTRUE, (void *)0, scale_timeout_cmd_handler);
		xTimerStart(scale_timer_cmd, 0);
#endif
	}
	Log_Notify("[Scale] reset\r\n");
}

static void scale_uart_thread(void *param)
{
	// Scale ファーム更新状態設定用GPIO設定(High)
	gpio_init(&scale_gpio_fup, SCALE_GPIO_FUP);
	gpio_write(&scale_gpio_fup, 1);
	gpio_dir(&scale_gpio_fup, PIN_OUTPUT);
	gpio_mode(&scale_gpio_fup, PullNone);
	vTaskDelay(100 / portTICK_RATE_MS);

	// Scale Reset用GPIO設定(High)
	gpio_init(&scale_gpio_reset, SCALE_GPIO_RST);
	gpio_write(&scale_gpio_reset, 1);
	gpio_dir(&scale_gpio_reset, PIN_OUTPUT);
	gpio_mode(&scale_gpio_reset, PullNone);

#if CONFIG_AUDREY_SCL_OFF
#include "flash_api.h"
#include "device_lock.h"

	flash_t flash;
	u8 data[2];
	struct tm *tm_save;

	device_mutex_lock(RT_DEV_LOCK_FLASH);
	flash_stream_read(&flash, SCALE_IS_UPDATE_FLG,  2, (uint8_t *)data);
	device_mutex_unlock(RT_DEV_LOCK_FLASH);

	if(data[1] == 'D' || data[1] == 'I') {
		Log_Scale("[Scale] UART is disabled\r\n");
		// Scale Reset
		vTaskDelay(20 / portTICK_RATE_MS);
		if(data[1] == 'I') {
			Log_Scale("[Scale] ISP Low\r\n");
			gpio_write(&scale_gpio_fup, 0);
			vTaskDelay(100 / portTICK_RATE_MS);
		}
		gpio_write(&scale_gpio_reset, 0);
		vTaskDelay(100 / portTICK_RATE_MS);
		gpio_write(&scale_gpio_reset, 1);
		vTaskDelete(NULL);
		return;
	}
#endif
	// UART通信設定
	Log_Debug("\r\nSCALE_UART_CON: 115200bps,Data8bit,ParityNone,Stop1bit,FlowNone\r\n");
	serial_init(&scale_uart_sobj, UART_TX, UART_RX);
	serial_baud(&scale_uart_sobj, UART_BAUD_RATE_115200);
	serial_format(&scale_uart_sobj, 8, ParityNone, 1);
	serial_set_flow_control(&scale_uart_sobj, FlowControlNone, UART_RTS, UART_CTS);

	serial_irq_handler(&scale_uart_sobj, scale_uart_irq, (uint32_t)&scale_uart_sobj);
	serial_irq_set(&scale_uart_sobj, RxIrq, 1);
	// GPIO割り込み設定(High → Low 割り込み)
	gpio_init(&scale_gpio, SCALE_GPIO_INT);
	gpio_dir(&scale_gpio, PIN_INPUT);
	gpio_mode(&scale_gpio, PullUp);
	gpio_irq_init(&scale_int, SCALE_GPIO_INT, scale_int_irq_handler, 0);
	gpio_irq_set(&scale_int, IRQ_FALL, 1);
	gpio_irq_enable(&scale_int);

	// 重量測定初期化
	weight_init();

	// Scale側をリセット
	scale_init_done = FALSE;
	scale_reset();

	// 重量取得待ち
	vSemaphoreCreateBinary(scale_weight_sema);
	xSemaphoreTake(scale_weight_sema, 1/portTICK_RATE_MS);

	while(1) {
		while(xSemaphoreTake(scale_weight_sema, portMAX_DELAY) != pdTRUE);
		scale_to_weight_cnt = 0;
		// 重量測定 情報更新
#if AUDREY_LOG_SCALE
		tm_save = get_local_tm(&scale_info.time);
		Log_Scale(" %dg, %dg, %04d-%02d-%02d %02d:%02d:%02d [DONE]\r\n", scale_info.body, scale_info.urine
		, tm_save->tm_year+1900, tm_save->tm_mon+1, tm_save->tm_mday, tm_save->tm_hour, tm_save->tm_min, tm_save->tm_sec);
#endif
		scale_chk_next(weight_update());
	}

	vTaskDelete(NULL);
}

void scale_init(void)
{
	if(xTaskCreate(scale_uart_thread, ((const char*)"scale_uart_thread"), 1024, NULL, tskIDLE_PRIORITY + 3 , NULL) != pdPASS)
		Log_Error("\n\r%s xTaskCreate(scale_uart_thread) failed", __FUNCTION__);
}
