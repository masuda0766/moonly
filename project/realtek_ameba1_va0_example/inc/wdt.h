/*
 * Copyright (C) 2017 SHARP Corporation. All rights reserved.
 */
#ifndef WDT_H_
#define WDT_H_

typedef enum {
	WDT_TASK_DATAUP = 0,							// データアップロード
	WDT_TASK_FWUP,									// ファーム更新
	WDT_TASK_MAX,
} WDT_TASK_ID;

typedef enum {
	WDT_TASK_WATCH_NONE = 0,						// タスク死活監視対象外
	WDT_TASK_WATCH_START,							// タスク死活監視開始
} WDT_TASK_TYPE;

void wdt_task_watch(int task, int type);			// タスク死活監視要求(task:WDT_TASK_ID、type:WDT_TASK_TYPE)
void wdt_task_refresh(int task);					// タスク死活監視タイマリフレッシュ

#endif /* WDT_H_ */
