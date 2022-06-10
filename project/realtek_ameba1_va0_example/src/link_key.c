/*
 * Copyright (C) 2017 SHARP Corporation. All rights reserved.
 */
#include <platform_opts.h>
#include "FreeRTOS.h"
#include "task.h"
#include <platform/platform_stdlib.h>
#include "semphr.h"
#include "device.h"
#include "timer_api.h"
#include "gpio_api.h"
#include "gpio_irq_api.h"
#include "osdep_service.h"
#include "pinmap.h"
#include "basic_types.h"
#include "link_key.h"
#include "state_manager.h"

#define USE_INT							/* key押下検出に割り込みを使用 */

#ifdef USE_INT
gpio_irq_t	link_key_int;
#endif
gpio_t		link_key;

bool	fBoot = TRUE;					/* 起動時Key押下フラグ */
#define CHECK_PRESS_PERIOD	300 		/* Keyが押されたかどうかをチェックする間隔 (ms)			*/
#ifndef USE_INT
volatile bool	fEnable = TRUE;			/* Key監視機能 ON/OFF (TRUE : ON)				*/
#endif
volatile bool	fPressing = FALSE;		/* 長押し監視中フラグ							*/
#define LONG_PRESS_CHECK_PERIOD	50		/*		Keyが押され続けているかをチェックする間隔 (ms)	*/
volatile u16	KeepTime = 0;			/*		Keyが押され続けている時間 (ms)				*/
#define	LONG_PRESS_THRESHOLD	5000	/* 		長押し判定閾値 (ms)					*/

#ifdef USE_INT
void link_key_irq_handler(uint32_t id, gpio_irq_event event)
{
	Log_Notify("\r\n[link_key] INT event %d\r\n", event);

	if (event == IRQ_FALL) {
		Log_Notify("[link_key] Key pushed\r\n");
		fPressing = TRUE;
		KeepTime = 0;
	}
}
#endif

/* Keyイベント通知再開／Key押下されていれば長押し判定も開始する  */
void link_key_enable(void)
{
#ifdef USE_INT
	gpio_irq_enable(&link_key_int);
#else
	fEnable = TRUE;
#endif

	Log_Info("\r\n[link_key] Key Event enabled\r\n");

	if (!gpio_read(&link_key)) {
		Log_Notify("\r\n[link_key] Key pushed\r\n");
		fPressing = TRUE;
		KeepTime = 0;
	} else {
		fPressing = FALSE;
	}
}

/* Keyイベント通知停止／実行中の長押し判定も停止する  */
void link_key_disable(void)
{
	KeepTime = 0;
	fPressing = FALSE;

#ifdef USE_INT
	gpio_irq_disable(&link_key_int);
#else
	fEnable = FALSE;
#endif

	Log_Info("\r\n[link_key] Key Event disabled\r\n");
}

static void link_key_thread(void *param)
{
	rtw_msleep_os(20);

	/* Link Key GPIO設定 */
	gpio_init(&link_key, LINK_KEY);
	gpio_dir(&link_key, PIN_INPUT);
	gpio_mode(&link_key, PullUp);
	Log_Info("\r\n[link_key] GPIO ready\r\n");

#ifdef USE_INT
	/* GPIO割り込み設定(High->Low) */
	gpio_irq_init(&link_key_int, LINK_KEY, link_key_irq_handler, 0);
	gpio_irq_set(&link_key_int, IRQ_FALL, 1);
	gpio_irq_enable(&link_key_int);
	Log_Info("\r\n[link_key] INT ready\r\n");
#endif

	/* 起動時初期状態確認 */
	if (!gpio_read(&link_key)) {
		Log_Notify("\r\n[link_key] Key pushed\r\n");
		fPressing = TRUE;
		KeepTime = 0;
		fBoot = TRUE;
	} else {
		fPressing = FALSE;
		fBoot = FALSE;
	}

	while(1) {
		if (fPressing == TRUE) {
			vTaskDelay(LONG_PRESS_CHECK_PERIOD * portTICK_PERIOD_MS);
			if (!gpio_read(&link_key)) {
				if (KeepTime <= LONG_PRESS_THRESHOLD) {
					KeepTime += LONG_PRESS_CHECK_PERIOD;
					if (KeepTime > LONG_PRESS_THRESHOLD) {
						if (fBoot) {
							Log_Notify("\r\n[link_key] -> Long Press from Boot Time\r\n");
							// 起動からの押し続けはイベント発行しない
//							SendMessageToStateManager(MSG_LINKKEY_PUSH, (MSG_Param_t)LONG_PRESS_BOOT);
						} else {
							Log_Notify("\r\n[link_key] -> Long Press\r\n");
							SendMessageToStateManager(MSG_LINKKEY_PUSH, (MSG_Param_t)LONG_PRESS_NORMAL);
						}
						fPressing = FALSE;
						fBoot = FALSE;
					}
				}
			} else {
				Log_Notify("\r\n[link_key] Key released\r\n");
				Log_Notify("[link_key] -> Short Press\r\n");
				SendMessageToStateManager(MSG_LINKKEY_SHORT, PARAM_NONE);
				KeepTime = 0;
				fPressing = FALSE;

				if (fBoot) {
					fBoot = FALSE;
				}
			}
		} else {
			vTaskDelay(CHECK_PRESS_PERIOD * portTICK_PERIOD_MS);
#ifndef USE_INT
			if (!fEnable) {
				continue;
			}

			if (!gpio_read(&link_key)) {
				if (KeepTime == 0) {
					Log_Notify("\r\n[link_key] Key pushed\r\n");
					fPressing = TRUE;
				}
			} else {
				if (fBoot) {
					fBoot = FALSE;
				}

				if (KeepTime > 0) {
					Log_Notify("\r\n[link_key] Key released\r\n");
					KeepTime = 0;
				}
			}
#endif
		}
	}

#ifdef USE_INT
	gpio_irq_free(&link_key_int);
#endif
	vTaskDelete(NULL);
}

void link_key_init(void)
{
	if(xTaskCreate(link_key_thread, ((const char*)"link_key_thread"), 1024, NULL, tskIDLE_PRIORITY + 1 , NULL) != pdPASS)
		Log_Error("\n\r%s xTaskCreate(link_key_thread) failed", __FUNCTION__);
}
