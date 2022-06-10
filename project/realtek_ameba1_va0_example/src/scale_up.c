/*
 * Copyright (C) 2017 SHARP Corporation. All rights reserved.
 */
#include <platform_opts.h>
#include "FreeRTOS.h"
#include "task.h"
#include <platform/platform_stdlib.h>
#include "semphr.h"
#include "device.h"
#include "gpio_api.h"
#include "serial_api.h"
#include "flash_api.h"
#include "device_lock.h"
#include "timers.h"
#include "scale.h"
#include "state_manager.h"
#include "osdep_service.h"
#include "serial_ex_api.h"
#include "pinmap.h"

#define SCALE_UP_CMD_ERASE				0x00
#define SCALE_UP_CMD_FALSH				0x01
#define SCALE_UP_CMD_CRC				0x02
#define SCALE_UP_CMD_INFO				0x03
#define SCALE_UP_CMD_RESET				0x04
#define SCALE_UP_CMD_EXIT				0x05

#define SCALE_UP_RSP_OK					0x4f
#define SCALE_UP_RSP_FAIL				0x46

#define SCALE_UP_PARAM_ERASE_MASS		0x0a
#define SCALE_UP_PARAM_ERASE_PAGE		0x08
#define SCALE_UP_PARAM_FLASH_VERIFY		0x00
#define SCALE_UP_PARAM_FLASH_PROG		0x01
#define SCALE_UP_PARAM_FLASH_READ		0x02
#define SCALE_UP_PARAM_FLASH_BLANK		0x03
#define SCALE_UP_PARAM_RESET_AP			0x00
#define SCALE_UP_PARAM_RESET_IAP		0x01

#define SCALE_UP_TIME					180

#define SCALE_UP_PAYLOAD				52
#define SCALE_UP_BUFLEN					64

#define SCALE_UP_SIZE_MAX				60 * 1024

typedef enum {
    SCALE_UP_MODE_INIT = 0,
    SCALE_UP_MODE_ERASE,
    SCALE_UP_MODE_PROG,
    SCALE_UP_MODE_VERIFY,
    SCALE_UP_MODE_RESET
} SCALE_UP_MODE;

typedef struct {
	u8 cmd;
	u8 param;
	u16 crc;
	u32 start;
	u32 end;
	u8 payload[SCALE_UP_PAYLOAD];
}SCALE_UP_FMT;

union {
	u8 buf[SCALE_UP_BUFLEN];
	SCALE_UP_FMT fmt;
} send_buf;

SCALE_UP_MODE scale_up_mode;

u16 flash_cnt;
u16 flash_size;
u8 *flash_pnt;

int scale_up_retry;
TimerHandle_t scale_up_timer;

/* CRC計算 */
u16 calc_crc(u8 *buf, u8 n)
{
	u16 crc;
	u8 i;

	crc = 0;
	while (n-- > 0) {
	   crc = crc ^ (u16)*buf++ << 8;

	   for (i = 0; i < 8; i++)
	       if (crc & 0x8000)
		   crc = crc << 1 ^ 0x1021;
	       else
		   crc = crc << 1;
	}
	return (crc & 0xffff);
}

int Erase_Scale_Update_flg(){
	flash_t flash;

	device_mutex_lock(RT_DEV_LOCK_FLASH);
	flash_erase_sector(&flash, SCALE_IS_UPDATE_FLG);
	device_mutex_unlock(RT_DEV_LOCK_FLASH);
	return 0;
}

/* Scaleファーム更新 Erase */
void scale_up_send_erase(void)
{
	u8 i;

	send_buf.fmt.cmd = SCALE_UP_CMD_ERASE;
	send_buf.fmt.param = SCALE_UP_PARAM_ERASE_PAGE;
	send_buf.fmt.crc = 0;
	send_buf.fmt.start = 0;
	send_buf.fmt.end = flash_size - 1;
	memset(send_buf.fmt.payload, 0, SCALE_UP_PAYLOAD);

	//CRC計算(計算前のCRC領域は0にしておく)
	send_buf.fmt.crc = calc_crc(send_buf.buf, SCALE_UP_BUFLEN);

	//UART送信
	for(i=0; i < SCALE_UP_BUFLEN; i++) {
		serial_putc(&scale_uart_sobj, send_buf.buf[i]);
	}
	scale_rcv_cnt = 0;
	scale_up_mode = SCALE_UP_MODE_ERASE;
	Log_Scale("[Scale] update Erase start\r\n");
}

/* Scaleファーム更新 Program */
void scale_up_send_flash(u8 param)
{
	u8 i;

	send_buf.fmt.cmd = SCALE_UP_CMD_FALSH;
	send_buf.fmt.param = param;
	send_buf.fmt.crc = 0;
	send_buf.fmt.start = flash_cnt * SCALE_UP_PAYLOAD;
	send_buf.fmt.end = (flash_cnt + 1) * SCALE_UP_PAYLOAD - 1;

	//最終コマンドのpayloadが52byteに満たない時は0xFFパディング
	if ((flash_cnt + 1) * SCALE_UP_PAYLOAD > flash_size) {
		memcpy(send_buf.fmt.payload, flash_pnt+(flash_cnt * SCALE_UP_PAYLOAD), flash_size % SCALE_UP_PAYLOAD);
		memset(&send_buf.fmt.payload[flash_size % SCALE_UP_PAYLOAD], 0xFF, SCALE_UP_PAYLOAD - (flash_size % SCALE_UP_PAYLOAD));
	} else {
		memcpy(send_buf.fmt.payload, flash_pnt+(flash_cnt * SCALE_UP_PAYLOAD), SCALE_UP_PAYLOAD);
	}

	//CRC計算(計算前のCRC領域は0にしておく)
	send_buf.fmt.crc = calc_crc(send_buf.buf, SCALE_UP_BUFLEN);

	//UART送信
	for(i=0; i < SCALE_UP_BUFLEN; i++) {
		serial_putc(&scale_uart_sobj, send_buf.buf[i]);
	}

	scale_up_mode = ((param == SCALE_UP_PARAM_FLASH_PROG) ? SCALE_UP_MODE_PROG : SCALE_UP_MODE_VERIFY);
	if(flash_cnt == 0) {
		if(scale_up_mode == SCALE_UP_MODE_PROG) {
			Log_Scale("[Scale] update Program start\r\n");
		} else {
			Log_Scale("[Scale] update Verify start\r\n");
		}
	}
	flash_cnt++;
	scale_rcv_cnt = 0;
}


/* Scaleファーム更新 タイムアウト */
void scale_timeout_up_handler(xTimerHandle pxTimer)
{

	flash_t flash;
	u8 data;

	switch (scale_up_mode)
	{
		case SCALE_UP_MODE_INIT:
			// ファーム書き換え端子をLowに落とした状態でScaleをリセット
			gpio_write(&scale_gpio_fup, 0);
			vTaskDelay(100 / portTICK_RATE_MS);
			gpio_write(&scale_gpio_reset, 0);
			vTaskDelay(100 / portTICK_RATE_MS);
			gpio_write(&scale_gpio_reset, 1);
			vTaskDelay(100 / portTICK_RATE_MS);
			gpio_write(&scale_gpio_fup, 1);
			vTaskDelay(1000 / portTICK_RATE_MS);

			if(!flash_size) {
				// ファーム書き換え中タイマ設定
				xTimerChangePeriod(scale_up_timer, ( 1000 / portTICK_RATE_MS ), 0);
				scale_up_mode = SCALE_UP_MODE_RESET;
				break;
			}

			// ファーム書き換え中フラグ設定
			device_mutex_lock(RT_DEV_LOCK_FLASH);
			flash_stream_read(&flash, SCALE_IS_UPDATE_FLG,  1, &data);
			if(data >= 1 && data <= 9) {
				data += 1;
			} else if(data != 10){
				data = 1;
			}
			flash_erase_sector(&flash, SCALE_IS_UPDATE_FLG);
			flash_stream_write(&flash, SCALE_IS_UPDATE_FLG, 1, &data);
			device_mutex_unlock(RT_DEV_LOCK_FLASH);

			// ファーム書き換え中タイマ設定
			xTimerChangePeriod(scale_up_timer, ( 60000 / portTICK_RATE_MS ), 0);

			// Erase
			scale_up_send_erase();
			break;


		case SCALE_UP_MODE_RESET:
			// 書き換え完了から1秒経過でScaleをリセット
			xTimerStop(scale_up_timer, 0);
			scale_up_mode = SCALE_UP_MODE_INIT;
			scale_reset();
			break;

		case SCALE_UP_MODE_ERASE:
		case SCALE_UP_MODE_PROG:
		case SCALE_UP_MODE_VERIFY:
			// Erase～Verifyの一定時間経過で中断
			scale_up_retry = 0;
			SendMessageToStateManager(MSG_ERR_SCALE_FATAL, PARAM_SCL_ERR_UPDATE_FAIL_TO);
			scale_reset();
			break;

		default:
			break;
	}
}

/* Scaleファーム更新 Response受信 */
void scale_up_res(u8 result)
{

	flash_t flash;
	u8 data;

	// 'F'受信またはタイムアウトでリトライ
	if(result == 'F' && scale_up_mode != SCALE_UP_MODE_INIT && scale_up_mode != SCALE_UP_MODE_RESET) {
		scale_up_retry++;
		Log_Error("[Scale] update Response fail round %d \r\n", scale_up_retry);
		// リトライ２回NGだった場合は中断
		if(scale_up_retry >= 3) {
			scale_up_retry = 0;
			SendMessageToStateManager(MSG_ERR_SCALE_FATAL, PARAM_SCL_ERR_UPDATE_FAIL_RES);
			scale_reset();
		}
	} else {
		scale_up_retry = 0;
	}
	switch (scale_up_mode)
	{
		case SCALE_UP_MODE_ERASE:
			if(scale_up_retry) {
				// Erase for Retry
				scale_up_send_erase();
				break;
			}
			flash_cnt = 0;
		case SCALE_UP_MODE_PROG:
			if(scale_up_retry) {
				flash_cnt--;
			}
			if((flash_cnt * SCALE_UP_PAYLOAD) < flash_size) {
				// Program
				scale_up_send_flash(SCALE_UP_PARAM_FLASH_PROG);
				break;
			} else {
				flash_cnt = 0;
			}
		case SCALE_UP_MODE_VERIFY:
			if(scale_up_retry) {
				flash_cnt--;
			}
			if((flash_cnt * SCALE_UP_PAYLOAD) < flash_size) {
				// Verify
				scale_up_send_flash(SCALE_UP_PARAM_FLASH_VERIFY);
				break;
			} else {
				// ファーム書き換え中フラグ解除
				Erase_Scale_Update_flg();
				Log_Notify("[Scale] update Complete\r\n");
				// 1秒後にScale側をリセット
				xTimerChangePeriod(scale_up_timer, ( 1000 / portTICK_RATE_MS ), 0);
				scale_up_mode = SCALE_UP_MODE_RESET;
			}
			break;

		default:
			break;
	}
}

/* Scaleファーム更新中確認 */
int scale_is_update(void) {

	flash_t flash;
	u8 data;

	device_mutex_lock(RT_DEV_LOCK_FLASH);
	flash_stream_read(&flash, SCALE_IS_UPDATE_FLG,  1, &data);
	device_mutex_unlock(RT_DEV_LOCK_FLASH);

	if(data >= 1 && data <= 10) {
		return data;
	} else {
		return 0;
	}
}

/* Scaleファーム更新 開始 */
int scale_up_start(u8 *addr, u16 size) {

	int i;

	if(scale_mode == SCALE_MODE_FUP) {
		return -1;
	}
	if(size > SCALE_UP_SIZE_MAX) {
		return -1;
	}

	Log_Notify("[Scale] update Start\r\n");
	scale_mode = SCALE_MODE_FUP;
	scale_up_mode = SCALE_UP_MODE_INIT;

	// firmware imageのファイルサイズを取得;
	flash_size = size;
	flash_pnt = addr;

	scale_up_retry = 0;

	// 100ms後にErase
	xTimerStop(scale_up_timer, 0);
	scale_up_timer = xTimerCreate("SCALE_UP_TIMER",(100 / portTICK_RATE_MS), pdTRUE, (void *)0, scale_timeout_up_handler);
	xTimerStart(scale_up_timer, 0);

	for(i = 0; i < SCALE_UP_TIME; i++) {
		vTaskDelay(1000 / portTICK_RATE_MS);
		if(scale_mode != SCALE_MODE_FUP) {
			return 0;
		}
	}
	// 一定時間で終了しなければ中断
	SendMessageToStateManager(MSG_ERR_SCALE_FATAL, PARAM_SCL_ERR_UPDATE_FAIL_TO2);
	return -1;
}

