/*
 * Copyright (C) 2017 SHARP Corporation. All rights reserved.
 */
#include "FreeRTOS.h"
#include "task.h"
#include <platform_stdlib.h>
#include <platform_opts.h>
#include "wdt_api.h"

#define WDT_MONITOR_PERIOD				10000		// 監視間隔 10秒

#include "wdt.h"
#include "state_manager.h"
#if CONFIG_AUDREY_SELF_RESET
#include "sys_api.h"
#define WDT_RESET_TIME					30*60*60/10	// リセット時間 30時間/監視間隔
#endif
#define WDT_TASK_WATCH_TIME				5*60/10	// リセット時間 5分/監視間隔

typedef struct {
	int st;										// 監視フラグ
	int cnt;										// 監視時間(÷10秒)
}WDT_T;

WDT_T wdt_task_t[WDT_TASK_MAX];

void wdt_task_watch(int task, int type)
{
	wdt_task_t[task].st = type;
	wdt_task_t[task].cnt = 0;
}

void wdt_task_refresh(int task)
{
	wdt_task_t[task].cnt = 0;
}

#if CONFIG_AUDREY_WDT
static void wdt_thread(void *param)
{
#if CONFIG_AUDREY_SELF_RESET
	UINT wdt_cnt = 0;
#endif
	int i;

	watchdog_init(WDT_MONITOR_PERIOD * portTICK_PERIOD_MS * 2);
	watchdog_start();
	watchdog_refresh();
	Log_Info("\r\nWatchdog Timer is initialized\r\n");

	// タスク死活監視情報初期化
	memset(wdt_task_t, 0, sizeof(wdt_task_t));

	while(1) {
		vTaskDelay(WDT_MONITOR_PERIOD * portTICK_PERIOD_MS);
		watchdog_refresh();
		// 一定時間監視中タスクが未処理(リフレッシュなし)の場合は再起動
		for(i = 0; i < WDT_TASK_MAX; i++) {
			if(wdt_task_t[i].st == WDT_TASK_WATCH_START) {
				wdt_task_t[i].cnt++;
				if(wdt_task_t[i].cnt > WDT_TASK_WATCH_TIME) {
					Log_Info("\r\n[[[Reset for Task ID : %d]]]\r\n", i);
					SendMessageToStateManager(MSG_TASK_RESET, i);
					vTaskDelay(1000 * portTICK_PERIOD_MS);
					sys_reset();
				}
			}
		}
#if CONFIG_AUDREY_SELF_RESET
		wdt_cnt++;
		// 一定時間経過で再起動
		if(wdt_cnt >= WDT_RESET_TIME) {
			Log_Info("\r\n[[[Self Reset]]]\r\n");
			SendMessageToStateManager(MSG_SELF_RESET, PARAM_NONE);
			vTaskDelay(1000 * portTICK_PERIOD_MS);
			sys_reset();
		}
#endif
	}
	watchdog_stop();
	vTaskDelete(NULL);
}
#endif /* CONFIG_AUDREY_WDT */

void wdt_init(void)
{
#if CONFIG_AUDREY_WDT
	if(xTaskCreate(wdt_thread, ((const char*)"wdt_thread"), 1024, NULL, tskIDLE_PRIORITY + 10, NULL) != pdPASS){
		Log_Error("\n\r%s xTaskCreate(wdt_thread) failed", __FUNCTION__);
	}
#endif /* CONFIG_AUDREY_WDT */
}

