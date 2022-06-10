#include "FreeRTOS.h"
#include "task.h"
#include "diag.h"
#include "main.h"
#include "state_manager.h"

extern void console_init(void);
extern void wdt_init(void);
extern void rtc_init(void);
extern void log_record_init(void);
extern void scale_init(void);
extern void led_init(void);
extern void link_key_init(void);
extern void temperature_init(void);
extern void data_upload_init(void);
extern void rf_ctrl_init(void);

/**
  * @brief  Main program.
  * @param  None
  * @retval None
  */
void main(void)
{
	// Watchdog
	wdt_init();

	// RTC
	rtc_init();

	// Log Record
	log_record_init();

	/* Initialize log uart and at command service */
	console_init();	

	/* 状態管理タスク登録 */
	StateManagerInit();

	// Initialize LED
	led_init();

	// Link Key監視開始
	link_key_init();

	// Initialize Temperature
	temperature_init();

	// Scale通信開始
	scale_init();

	// データ送信処理
	data_upload_init();

	// 無線通信制御部
	rf_ctrl_init();

	/*Enable Schedule, Start Kernel*/
#if defined(CONFIG_KERNEL) && !TASK_SCHEDULER_DISABLED
	#ifdef PLATFORM_FREERTOS
	vTaskStartScheduler();
	#endif
#else
	RtlConsolTaskRom(NULL);
#endif
}
