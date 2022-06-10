/*
 * Copyright (C) 2017 SHARP Corporation. All rights reserved.
 */
#include <string.h>
#include <platform_stdlib.h>
#include <platform_opts.h>
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "device.h"
#include "sys_api.h"
#include "log_uart_api.h"
#include "scale.h"

#define	SOH						0x01
#define	STX						0x02
#define	EOT						0x04
#define	ACK						0x06
#define	NAK						0x15
#define	CAN						0x18
#define	CTRLZ					0x1a

#define SCALE_XM_RETRY			9

#define SCALE_XM_LEN_BLK		2
#define SCALE_XM_LEN_DATA		128
#define SCALE_XM_LEN_SUM		1
#define SCALE_XM_OFS_BLK		0
#define SCALE_XM_OFS_DATA		SCALE_XM_LEN_BLK
#define SCALE_XM_OFS_SUM		SCALE_XM_LEN_BLK + SCALE_XM_LEN_DATA
#define SCALE_XM_LEN			SCALE_XM_LEN_BLK + SCALE_XM_LEN_DATA + SCALE_XM_LEN_SUM
#define SCALE_XM_LEN_MAX		1024 * 60

typedef enum {
	SCALE_XM_SOH = 0,
	SCALE_XM_DATA,
	SCALE_XM_END,
	SCALE_XM_MAX
} SCALE_XM_MODE;

u8 scale_xm_buf[SCALE_XM_LEN];
u8 scale_xm_img[SCALE_XM_LEN_MAX];
int scale_xm_cnt;
int scale_xm_retry;
int scale_xm_blkno;
int scale_xm_size;
SCALE_XM_MODE scale_xm_mode;
log_uart_t xmodem_obj;
xSemaphoreHandle scale_xmodem_sema;
TimerHandle_t xmodem_timer;

extern void log_uart_enable_printf(void);
extern void log_uart_disable_printf(void);

u8 calc_csum(u8 *buf, int n)
{
	u8 csum;
	int i;

	for(csum = 0, i = 0; i < n; i++) {
		csum += *buf++;
	}
	return csum;
}

/* ログUARTからのScale更新 NAK送信リトライ処理 */
void scale_xmodem_retry(void)
{
	// リトライ数上限チェック
	if (scale_xm_retry < SCALE_XM_RETRY) {
		scale_xm_retry++;
		// SOH受信待ちへ
		scale_xm_mode = SCALE_XM_SOH;
		// NAK送信
		log_uart_putc(&xmodem_obj, NAK);
	} else {
		// リトライ数上限到達時はシステム再起動
		sys_reset();
	}
}

/* ログUARTからのScale更新 タイムアウト処理 */
void xmodem_timeout_handler(xTimerHandle pxTimer)
{
	// NAK送信リトライ
	scale_xmodem_retry();
}

/* ログUARTからのScale更新 データ受信処理(xmodem) */
void xmodem_irq(uint32_t id, LOG_UART_INT_ID event)
{
	u8 rc = 0;

	rc = log_uart_getc(&xmodem_obj);
	switch(scale_xm_mode)
	{
		case SCALE_XM_SOH:
			if(rc == SOH) {
				scale_xm_cnt = 0;
				// データ受信待ちへ
				scale_xm_mode = SCALE_XM_DATA;
			} else if(rc == EOT) {
				// ACK送信
				log_uart_putc(&xmodem_obj, ACK);
				// リトライタイマ停止
				xTimerStop(xmodem_timer, 0);
				// xmodem通信終了
				scale_xm_mode = SCALE_XM_END;
				// Scale更新
				rtw_up_sema_from_isr(&scale_xmodem_sema);
			}
			break;

		case SCALE_XM_DATA:
			scale_xm_buf[scale_xm_cnt++] = rc;
			if(scale_xm_cnt >= SCALE_XM_LEN ) {
				// １ブロックのチェックサムNG
				// または、ブロック番号の補数値が不正
				// または、ブロック番号値がNG、かをチェック
				if ((calc_csum(&scale_xm_buf[SCALE_XM_OFS_DATA], SCALE_XM_LEN_DATA) != scale_xm_buf[SCALE_XM_OFS_SUM])
				|| (scale_xm_buf[SCALE_XM_OFS_BLK] != (0xff - scale_xm_buf[SCALE_XM_OFS_BLK + 1]))
				|| (scale_xm_buf[SCALE_XM_OFS_BLK] != scale_xm_blkno + 1)) {
					// NAK送信リトライ
					scale_xmodem_retry();
				} else {
					// Scaleイメージサイズ最大値を超えていない場合
					if(scale_xm_blkno * SCALE_XM_LEN_DATA <= SCALE_XM_LEN_MAX) {
						// バッファにコピー
						memcpy(&scale_xm_img[scale_xm_blkno * SCALE_XM_LEN_DATA], &scale_xm_buf[SCALE_XM_OFS_DATA], SCALE_XM_LEN_DATA);
						// BLK番号更新
						scale_xm_blkno++;
						scale_xm_retry = 0;
						xTimerReset(xmodem_timer, 0);
						// SOH受信待ちへ
						scale_xm_mode = SCALE_XM_SOH;
						// ACK送信
						log_uart_putc(&xmodem_obj, ACK);
					}
				}
			}
			break;

		case SCALE_XM_END:
		default:
			break;
	}
}

void scale_xmodem_thread(void *param)
{
	int size, sum, i;

	scale_xm_mode = SCALE_XM_SOH;
	scale_xm_cnt = 0;
	scale_xm_retry = 0;
	scale_xm_blkno = 0;

	// OTAからの更新時と同様にScale通信をとめておく
	scale_ota_start();

	// 10秒間隔でNAK送信
	xTimerStop(xmodem_timer, 0);
	xmodem_timer = xTimerCreate("XMODEM_TIMER",(10000 / portTICK_RATE_MS), pdTRUE, (void *)0, xmodem_timeout_handler);
	xTimerStart(xmodem_timer, 0);

	Log_Notify("[Scale] Wait Scale image from log UART\r\n");
	vTaskDelay(100 / portTICK_RATE_MS);
	// ログ出力無効
	log_uart_disable_printf();
	vTaskDelay(100 / portTICK_RATE_MS);
	// ログUART設定
	log_uart_init(&xmodem_obj, 38400, 8, ParityNone, 1);
	log_uart_irq_set(&xmodem_obj, IIR_RX_RDY, 1);
	log_uart_irq_handler(&xmodem_obj, xmodem_irq, (uint32_t)&xmodem_obj);
	// NAK送信
	log_uart_putc(&xmodem_obj, NAK);

	vSemaphoreCreateBinary(scale_xmodem_sema);
	xSemaphoreTake(scale_xmodem_sema, 1/portTICK_RATE_MS);

	while(xSemaphoreTake(scale_xmodem_sema, portMAX_DELAY) != pdTRUE);
	vTaskDelay(100 / portTICK_RATE_MS);
	// ログ出力有効
	log_uart_enable_printf();
	vTaskDelay(100 / portTICK_RATE_MS);
	// イメージサイズ計算
	size = scale_xm_blkno * SCALE_XM_LEN_DATA;
	for(i = 0; i < SCALE_XM_LEN_DATA; i++) {
		if(scale_xm_img[size - 1] == CTRLZ) {
			size--;
		} else {
			break;
		}
	}
	Log_Notify("[Scale] Complete downloading Scale from log UART\r\nImage size=%d\r\n", size);

	for(sum = 0, i = 0; i < size - 4; i++) {
		sum += (int)scale_xm_img[i];
	}
	Log_Notify("sum=%08x\r\n", sum);
	// ファイルchecksum値照合
	if(((unsigned char)sum != scale_xm_img[size - 4])
	|| ((unsigned char)(sum >> 8) != scale_xm_img[size - 3])
	|| ((unsigned char)(sum >> 16) != scale_xm_img[size - 2])
	|| ((unsigned char)(sum >> 24) != scale_xm_img[size - 1])) {
		// checksum値NGの場合はシステム再起動
		Log_Error("[Scale] checksum NG\r\n");
		vTaskDelay(1000 / portTICK_RATE_MS);
		sys_reset();
	}
	// checksum値領域は更新対象外
	size -= 4;
	vTaskDelay(1000 / portTICK_RATE_MS);
	// Scaleファームウェア更新
	scale_up_start(scale_xm_img, size);
	while(1) {
		vTaskDelay(10 / portTICK_RATE_MS);
		if(scale_mode != SCALE_MODE_FUP) {
			// Scaleファームウェア更新後にシステム再起動
			sys_reset();
		}
	}
	vTaskDelete(NULL);
}

void scale_xmodem_start(void)
{
	if(xTaskCreate(scale_xmodem_thread, ((const char*)"scale_xmodem_thread"), 1024, NULL, tskIDLE_PRIORITY + 3 , NULL) != pdPASS)
		Log_Error("\n\r%s xTaskCreate(scale_xmodem_thread) failed", __FUNCTION__);
}
