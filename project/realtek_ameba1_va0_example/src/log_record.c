/*
 * Copyright (C) 2017 SHARP Corporation. All rights reserved.
 */
#include <platform_opts.h>
#include <platform_stdlib.h>
#include "rtl8195a.h"
#include "FreeRTOS.h"
#include "task.h"
#include <sntp/sntp.h>
#include "rtc_api.h"
#include "flash_api.h"
#include "device_lock.h"
#include "version.h"
#include "state_manager.h"
#include "log_record.h"
#include "data_upload.h"
#if CONFIG_AUDREY_CONF_BACKUP
#include "fast_connect.h"
#endif

#if (CONFIG_AUDREY_EXT_INFO == 1)
	#include "sh_system.h"

	#ifndef MIN /* Takahara */
	#define MIN(x,y)  ((x) < (y) ? (x) : (y))
	#endif /* ifndef MIN */
#endif

#define LOG_RECORD_PERIOD	900000				// 監視間隔 15分秒
#define LOG_INIT_PERIOD		20000				// 起動直後監視時間 20秒

typedef enum {
	FLASH_ADDR_BOTH = 0,						// 両面
	FLASH_ADDR_1,								// １面
	FLASH_ADDR_2,								// ２面(Backup領域)
} 	FLASH_ADDR;

#define LOG_MAGIC_NUM		"LogRecord-Rev001"	// マジックナンバー
#define LOG_VER_SIZE	LOG_VER_MAX * sizeof(VER_INFO)
												// バージョン情報保存領域サイズ
#define LOG_ERR_SIZE	LOG_ERR_MAX * sizeof(LOG_INFO)
												// エラー情報保存領域サイズ
#define LOG_MSG_MAX		(FLASH_SECTOR_SIZE - (sizeof(LOG_MAGIC_NUM) + 16 + LOG_ERR_SIZE + LOG_VER_SIZE)) / sizeof(LOG_INFO)
												// メッセージ情報保存数

typedef struct {
	u8 magic[sizeof(LOG_MAGIC_NUM)];			// マジックナンバー格納領域
	u16 p_ver;									// バージョン情報ポインタ
	u16 p_err;									// エラー情報ポインタ
	u16 p_msg;									// メッセージポインタ
	VER_INFO ver[LOG_VER_MAX];					// バージョン情報
	LOG_INFO err[LOG_ERR_MAX];					// エラー情報
	LOG_INFO msg[LOG_MSG_MAX];					// メッセージ情報
} LOG_MSG;

LOG_MSG log_msg;								// ログ保存情報

bool log_record_exe;							// ログ保存要求
flash_t flash_log;
u8 log_err_prev_id = 0;
u8 log_err_prev_param = 0;
u32 log_err_prev_time = 0;

#define LOG_FAULT_MAGIC		"Log-Fault-Rev000"	// マジックナンバー

typedef struct {
	HARD_FAULT_DATA info[LOG_FAULT_MAX];		// Hard Fault Error 情報
	u16 p;										// 格納ポインタ
	u8 magic[sizeof(LOG_FAULT_MAGIC)];			// マジックナンバー格納領域
} LOG_FAULT_FLASH;

typedef struct {
	HARD_FAULT_DATA data;						// Hard Fault Error 情報
	u8 magic[sizeof(LOG_FAULT_MAGIC)];			// マジックナンバー格納領域
} LOG_FAULT_VOLATILE;

LOG_FAULT_VOLATILE __attribute__((section(".noinit.reboot_info"))) reboot_info;
												// Hard Fault Error 一時保存領域(Reboot後も残る)
LOG_FAULT_FLASH log_fault;						// Hard Fault Error 保存領域

/* Hard Fault Error 発生時にコールされる */
void HalHardFaultHandler_user_define(u32 HardDefaultArg)
{
    DBG_8195A_HAL("<<< Hard Fault Error!!!! >>>\n");

	reboot_info.data.StackR0 = HAL_READ32(HardDefaultArg, 0x0);
	reboot_info.data.StackR1 = HAL_READ32(HardDefaultArg, 0x4);
	reboot_info.data.StackR2 = HAL_READ32(HardDefaultArg, 0x8);
	reboot_info.data.StackR3 = HAL_READ32(HardDefaultArg, 0xC);
	reboot_info.data.StackR12 = HAL_READ32(HardDefaultArg, 0x10);
	reboot_info.data.StackLr = HAL_READ32(HardDefaultArg, 0x14);
	reboot_info.data.StackPc = HAL_READ32(HardDefaultArg, 0x18);
	reboot_info.data.StackPsr = HAL_READ32(HardDefaultArg, 0x1C);
	reboot_info.data.Bfar = HAL_READ32(0xE000ED38, 0x0);
	reboot_info.data.Cfsr = HAL_READ32(0xE000ED28, 0x0);
	reboot_info.data.Hfsr = HAL_READ32(0xE000ED2C, 0x0);
	reboot_info.data.Dfsr = HAL_READ32(0xE000ED30, 0x0);
	reboot_info.data.Afsr = HAL_READ32(0xE000ED3C, 0x0);
	reboot_info.data.time = rtc_read();
	reboot_info.data.flag = 0x12345678;

	DBG_8195A_HAL("R0 = 0x%x\n", reboot_info.data.StackR0);
	DBG_8195A_HAL("R1 = 0x%x\n", reboot_info.data.StackR1);
	DBG_8195A_HAL("R2 = 0x%x\n", reboot_info.data.StackR2);
	DBG_8195A_HAL("R3 = 0x%x\n", reboot_info.data.StackR3);
	DBG_8195A_HAL("R12 = 0x%x\n", reboot_info.data.StackR12);
	DBG_8195A_HAL("LR = 0x%x\n", reboot_info.data.StackLr);
	DBG_8195A_HAL("PC = 0x%x\n", reboot_info.data.StackPc);
	DBG_8195A_HAL("PSR = 0x%x\n", reboot_info.data.StackPsr);
	DBG_8195A_HAL("BFAR = 0x%x\n", reboot_info.data.Bfar);
	DBG_8195A_HAL("CFSR = 0x%x\n", reboot_info.data.Cfsr);
	DBG_8195A_HAL("HFSR = 0x%x\n", reboot_info.data.Hfsr);
	DBG_8195A_HAL("DFSR = 0x%x\n", reboot_info.data.Dfsr);
	DBG_8195A_HAL("AFSR = 0x%x\n", reboot_info.data.Afsr);
	DBG_8195A_HAL("PriMask 0x%x\n", __get_PRIMASK());
	DBG_8195A_HAL("BasePri 0x%x\n", __get_BASEPRI());
	DBG_8195A_HAL("SVC priority: 0x%02x\n", HAL_READ8(0xE000ED1F, 0));
	DBG_8195A_HAL("PendSVC priority: 0x%02x\n", HAL_READ8(0xE000ED22, 0));
	DBG_8195A_HAL("Systick priority: 0x%02x\n", HAL_READ8(0xE000ED23, 0));

	memcpy(reboot_info.magic, LOG_FAULT_MAGIC, sizeof(LOG_FAULT_MAGIC));

#if (CONFIG_AUDREY_EXT_INFO == 1)
   	//Output Current Task Info
    DBG_8195A_HAL("-- Task --\n");

	extern void*		 ___crntTask;
   	extern const char	*___crntTaskName;
   	extern void			*___crntTaskTopOfStack;
   	extern void			*___crntTaskStackArea;
   	extern uint32_t sh_sys_freertos_get_crntTask_StackSize_fromISR();
   	uint32_t psp=0;
   	uint32_t stacksize = sh_sys_freertos_get_crntTask_StackSize_fromISR();
   	__asm volatile
   	(
   		"mrs %0, psp	\n"
   		:"=r"(psp)
   	);
	if(___crntTaskName!=NULL)
	{
		DBG_8195A_HAL("TaskName   = %s (TCB:%08x)\n", ___crntTaskName, ___crntTask);
		DBG_8195A_HAL("StackArea  = 0x%08x\n", ___crntTaskStackArea);
		DBG_8195A_HAL("StackSize  = 0x%08x\n", stacksize);
		DBG_8195A_HAL("TopOfStack = 0x%08x\n", ___crntTaskTopOfStack);
	}
	DBG_8195A_HAL("SP         = 0x%x\n", psp);
	DBG_8195A_HAL("Stack trace :\n");
	if(___crntTaskName!=NULL)
	{
			for(uint32_t p=MIN(((uint32_t)___crntTaskTopOfStack)&0xFFFFFFE0,psp&0xFFFFFFE0);
			p<(uint32_t)___crntTaskStackArea+stacksize;p+=32)
		{
			DBG_8195A_HAL("%08x | %08x %08x %08x %08x : %08x %08x %08x %08x\n",
					p,
					*(uint32_t*)(p+0x00),	*(uint32_t*)(p+0x04),	*(uint32_t*)(p+0x08),	*(uint32_t*)(p+0x0C),
					*(uint32_t*)(p+0x10),	*(uint32_t*)(p+0x14),	*(uint32_t*)(p+0x18),	*(uint32_t*)(p+0x1C)
					);
		}
	}
	else
	{
		for(uint32_t p=psp&0xFFFFFFE0; p<psp+1024; p+=32)
		{
			DBG_8195A_HAL("%08x | %08x %08x %08x %08x : %08x %08x %08x %08x\n",
					*(uint32_t*)(p+0x00),	*(uint32_t*)(p+0x04),	*(uint32_t*)(p+0x08),	*(uint32_t*)(p+0x0C),
					*(uint32_t*)(p+0x10),	*(uint32_t*)(p+0x14),	*(uint32_t*)(p+0x18),	*(uint32_t*)(p+0x1C)
					);
		}
	}
	
    DBG_8195A_HAL("----------\n");
#endif

    for(;;);
}

/* ログ メッセージ書き込み */
void log_record_flash_msg(int addr)
{
	device_mutex_lock(RT_DEV_LOCK_FLASH);

	if(addr != FLASH_ADDR_2) {
		// 1面目を更新
		flash_erase_sector(&flash_log, LOG_RECORD_1);
		flash_stream_write(&flash_log, LOG_RECORD_1, sizeof(log_msg), (uint8_t *)&log_msg);
	}
	if(addr != FLASH_ADDR_1) {
		// 2面目を更新
		flash_erase_sector(&flash_log, LOG_RECORD_2);
		flash_stream_write(&flash_log, LOG_RECORD_2, sizeof(log_msg), (uint8_t *)&log_msg);
	}
	Log_Info("[LOG RECORD] Flash Msg&Err&Ver Log\r\n");

	device_mutex_unlock(RT_DEV_LOCK_FLASH);
}

/* ログ Hard Fault Error消去 */
void log_record_erase_fault(void)
{
	int i;

	device_mutex_lock(RT_DEV_LOCK_FLASH);
	flash_erase_sector(&flash_log, LOG_FAULT_1);
	flash_erase_sector(&flash_log, LOG_FAULT_2);
	device_mutex_unlock(RT_DEV_LOCK_FLASH);
	for(i = 0; i < LOG_FAULT_MAX; i++) {
		log_fault.info[i].flag = 0;
	}
	log_fault.p = 0;
	Log_Notify("[LOG RECORD] Erase Hard Fault Error Info\r\n");
}

/* ログ メッセージ消去 */
void log_record_erase_msg(void)
{
	device_mutex_lock(RT_DEV_LOCK_FLASH);
	flash_erase_sector(&flash_log, LOG_RECORD_1);
	flash_erase_sector(&flash_log, LOG_RECORD_2);
	device_mutex_unlock(RT_DEV_LOCK_FLASH);
	memcpy(log_msg.magic, LOG_MAGIC_NUM, sizeof(LOG_MAGIC_NUM));
	log_msg.p_ver = LOG_VER_MAX;
	log_msg.p_err = 0;
	log_msg.p_msg = 0;
	memset(log_msg.ver, 0x00, sizeof(VER_INFO)*LOG_VER_MAX);
	memset(log_msg.err, 0x00, sizeof(LOG_INFO)*LOG_ERR_MAX);
	memset(log_msg.msg, 0x00, sizeof(LOG_INFO)*LOG_MSG_MAX);
	Log_Notify("[LOG RECORD] Erase Msg&Err&Ver Log\r\n");
}

/* ログ Hard Fault Error表示 */
void log_record_show_fault(void)
{
	struct tm *t;
	int i, cnt;

	Log_Notify("<<Hard Fault Error Log>>\r\n");
	for(i = 0, cnt = 0; i < LOG_FAULT_MAX; i++) {
		if(log_fault.info[i].flag == 0x12345678) {
			t = get_local_tm(&log_fault.info[i].time);
			Log_Notify("%04d-%02d-%02d %02d:%02d:%02d\r\n"
			, t->tm_year+1900, t->tm_mon+1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
			Log_Notify("R0 = 0x%x\r\n", log_fault.info[i].StackR0);
			Log_Notify("R1 = 0x%x\r\n", log_fault.info[i].StackR1);
			Log_Notify("R2 = 0x%x\r\n", log_fault.info[i].StackR2);
			Log_Notify("R3 = 0x%x\r\n", log_fault.info[i].StackR3);
			Log_Notify("R12 = 0x%x\r\n", log_fault.info[i].StackR12);
			Log_Notify("LR = 0x%x\r\n", log_fault.info[i].StackLr);
			Log_Notify("PC = 0x%x\r\n", log_fault.info[i].StackPc);
			Log_Notify("PSR = 0x%x\r\n", log_fault.info[i].StackPsr);
			Log_Notify("BFAR = 0x%x\r\n", log_fault.info[i].Bfar);
			Log_Notify("CFSR = 0x%x\r\n", log_fault.info[i].Cfsr);
			Log_Notify("HFSR = 0x%x\r\n", log_fault.info[i].Hfsr);
			Log_Notify("DFSR = 0x%x\r\n", log_fault.info[i].Dfsr);
			Log_Notify("AFSR = 0x%x\r\n\r\n", log_fault.info[i].Afsr);
			cnt++;
		}
	}
	Log_Notify("Total:%d / %d, Pointer:%d\r\n<<End of Log>>\r\n\r\n", cnt, LOG_FAULT_MAX, log_fault.p);
}

/* ログ メッセージ表示 */
void log_record_show_msg(void)
{
	struct tm *t;
	int i, cnt;

	Log_Notify("[LOG RECORD]\r\n");
	Log_Notify("<<Version Log>>\r\nver,time\r\n");
	for(i = 0, cnt = 0; i < LOG_VER_MAX; i++) {
		if(log_msg.ver[i].info[0] != 0) {
			t = get_local_tm(&log_msg.ver[i].time);
			Log_Notify("%s,%04d-%02d-%02d %02d:%02d:%02d\r\n", log_msg.ver[i].info
			, t->tm_year+1900, t->tm_mon+1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
			cnt++;
		}
	}
	Log_Notify("\r\nTotal:%d / %d, Pointer:%d\r\n\r\n", cnt, LOG_VER_MAX, log_msg.p_ver);
	Log_Notify("<<Error Log>>\r\nid,param,time\r\n");
	for(i = 0, cnt = 0; i < LOG_ERR_MAX; i++) {
		if(log_msg.err[i].id != 0) {
			t = get_local_tm(&log_msg.err[i].time);
			Log_Notify("%d,%d,%04d-%02d-%02d %02d:%02d:%02d\r\n", log_msg.err[i].id, log_msg.err[i].param
			, t->tm_year+1900, t->tm_mon+1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
			cnt++;
		}
	}
	Log_Notify("\r\nTotal:%d / %d, Pointer:%d\r\n\r\n", cnt, LOG_ERR_MAX, log_msg.p_err);
	Log_Notify("<<Message Log>>\r\nid,state,time\r\n");
	for(i = 0, cnt = 0; i < LOG_MSG_MAX; i++) {
		if(log_msg.msg[i].id != 0) {
			t = get_local_tm(&log_msg.msg[i].time);
			Log_Notify("%d,%d,%04d-%02d-%02d %02d:%02d:%02d\r\n", log_msg.msg[i].id, log_msg.msg[i].param
			, t->tm_year+1900, t->tm_mon+1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
			cnt++;
		}
	}
	Log_Notify("\r\nTotal:%d / %d, Pointer:%d\r\n<<End of Log>>\r\n\r\n", cnt, LOG_MSG_MAX, log_msg.p_msg);
}

#if CONFIG_AUDREY_LOG_UPLOAD
/* ログ エラーメッセージ取得 */
void log_record_err_get(int cnt, LOG_INFO* data)
{
	data->id = log_msg.err[cnt].id;
	data->param = log_msg.err[cnt].param;
	data->time = log_msg.err[cnt].time;
}

/* ログ エラーメッセージ＆バージョン消去 */
void log_record_err_del(void)
{
	u8 log_ver_buf[32+1];

	log_msg.p_err = 0;
	log_msg.p_ver = 0;
	memset(log_msg.err, 0x00, sizeof(LOG_INFO)*LOG_ERR_MAX);
	memset(log_msg.ver, 0x00, sizeof(VER_INFO)*LOG_VER_MAX);

	sprintf(log_ver_buf, "%s_%s", AUDREY_VERSION, scale_ver);
	memcpy(log_msg.ver[LOG_VER_MAX - 1].info, log_ver_buf, LOG_VER_LEN-1);
	log_msg.ver[LOG_VER_MAX - 1].time = 0;

	log_record_flash_msg(FLASH_ADDR_BOTH);
	log_record_exe = FALSE;
}
/* ログ バージョン取得 */
void log_record_ver_get(int cnt, VER_INFO* data)
{
	memcpy(data->info, log_msg.ver[cnt].info, LOG_VER_LEN);
	data->time = log_msg.ver[cnt].time;
}

/* ログ Hard Fault Error取得 */
void log_record_fault_get(int cnt, FAULT_INFO* data)
{
	memset(data->pc, 0x00, 21);
	if(log_fault.info[cnt].flag == 0x12345678) {
		sprintf(data->pc, "0x%08x_0x%08x", log_fault.info[cnt].StackLr, log_fault.info[cnt].StackPc);
		data->time = log_fault.info[cnt].time;
	} else {
		data->time = 0;
	}
}
#endif

/* ログ エラーメッセージ記録 */
void log_record_err(u8 id, u8 param, u32 time)
{
	// エラー情報の記録
	log_msg.err[log_msg.p_err].id = id;
	// エラー情報の場合はパラメーターを記録
	log_msg.err[log_msg.p_err].param = param;
	log_msg.err[log_msg.p_err].time = time;
	log_msg.p_err++;
	if(log_msg.p_err >= LOG_ERR_MAX) {
		log_msg.p_err = 0;
	}
}

#if CONFIG_AUDREY_LOG_UPLOAD
int is_alert_msg(u8 id, u8 param) {
	if(id == MSG_ERR_SCALE) {
		return 1;
	} else {
		return 0;
	}
}
#endif

/* ログ メッセージ記録 */
void log_record_msg(u8 id, u8 param, u8 state)
{
	int type;

	// 種別判定
	if(id < MSG_ERR_RSP) {
		// メッセージ情報の記録(エラー情報が記録されるまでFlash保存されない)
		log_msg.msg[log_msg.p_msg].id = id;
		// メッセージの場合はState番号を記録
		log_msg.msg[log_msg.p_msg].param = state;
		log_msg.msg[log_msg.p_msg].time = rtc_read();
		log_msg.p_msg++;
		if(log_msg.p_msg >= LOG_MSG_MAX) {
			log_msg.p_msg = 0;
		}
	} else {
		if(id >= MSG_ERR_REBOOT) {
			// 再起動要求メッセージの場合は即時保存
			log_record_err(id, param, rtc_read());
			log_record_flash_msg(FLASH_ADDR_BOTH);
		} else if(id >= MSG_ERR_RSP) {
				log_record_err(id, param, rtc_read());
				if(log_err_prev_id != id || log_err_prev_param != param) {
#if CONFIG_AUDREY_LOG_UPLOAD
					// アラート通知をコール(特定のエラーコードのみ)
					if(is_alert_msg(id, param)) {
						alert_notification_req((unsigned int)((id - MSG_ERR) * 100) + (unsigned int)param);
					}
#else
					alert_notification_req((unsigned int)((id - MSG_ERR) * 100) + (unsigned int)param);
#endif
				}
				log_err_prev_id = id;
				log_err_prev_param = param;
				// エラーメッセージの場合は定期チェックタイミングで保存
				log_record_exe = TRUE;
		}
	}
}


#if CONFIG_AUDREY_CONF_BACKUP
void log_record_backup_wlan_conf(void)
{
	flash_t flash_conf_1;
	flash_t flash_conf_2;
	struct wlan_fast_reconnect read_data_conf_1 = {0};
	struct wlan_fast_reconnect read_data_conf_2 = {0};

	device_mutex_lock(RT_DEV_LOCK_FLASH);
	flash_stream_read(&flash_conf_1, FAST_RECONNECT_DATA, sizeof(struct wlan_fast_reconnect), (u8 *) &read_data_conf_1);
	flash_stream_read(&flash_conf_2, FAST_RECONNECT_BACKUP, sizeof(struct wlan_fast_reconnect), (u8 *) &read_data_conf_2);
	if(read_data_conf_1.type == FAST_CONNECT_WLAN && read_data_conf_2.type != FAST_CONNECT_WLAN) {
		// 無線設定情報バックアップ
		Log_Notify("[LOG RECORD] Backup wlan config\r\n");
		memcpy(&read_data_conf_2, &read_data_conf_1, sizeof(struct wlan_fast_reconnect));
		flash_erase_sector(&flash_conf_2, FAST_RECONNECT_BACKUP);
		flash_stream_write(&flash_conf_2, FAST_RECONNECT_BACKUP, sizeof(struct wlan_fast_reconnect), (u8 *) &read_data_conf_2);
	} else {
		Log_Info("[LOG RECORD] Skip backup for wlan config\r\n");
	}
	device_mutex_unlock(RT_DEV_LOCK_FLASH);
}
#endif

static void log_record_thread(void *param)
{
	int type;
	log_record_exe = FALSE;
	u8 log_ver_buf[32+1];
	u16 cnt;

	// Reboot前のHard Fault Error 有無チェック
	if(memcmp(reboot_info.magic, LOG_FAULT_MAGIC, sizeof(LOG_FAULT_MAGIC)) == 0) {
		// Hard Fault Error 情報があればFlash保存
		device_mutex_lock(RT_DEV_LOCK_FLASH);
		flash_stream_read(&flash_log, LOG_FAULT_1,  sizeof(log_fault), (uint8_t *)&log_fault);
		if(memcmp(log_fault.magic, LOG_FAULT_MAGIC, sizeof(LOG_FAULT_MAGIC)) != 0) {
			flash_stream_read(&flash_log, LOG_FAULT_2,  sizeof(log_fault), (uint8_t *)&log_fault);
			// MAGICナンバーが不正の場合は2面目を読み込み
			if(memcmp(log_fault.magic, LOG_FAULT_MAGIC, sizeof(LOG_FAULT_MAGIC)) != 0) {
				// Backup領域のMAGICナンバーも不正の場合は初期化
				memset(log_fault.info, 0x00, sizeof(HARD_FAULT_DATA)*LOG_FAULT_MAX);
				memcpy(log_fault.magic, LOG_FAULT_MAGIC, sizeof(LOG_FAULT_MAGIC));
				log_fault.p = 0;
			}
		}
		// 1970/1/1 00:09:00 の場合は+1しておく
		if(reboot_info.data.time == 0) {
			reboot_info.data.time = 1;
		}
		// Hard Fault Error 情報をFlash保存領域に追加
		memcpy(&log_fault.info[log_fault.p], &reboot_info.data, sizeof(HARD_FAULT_DATA));
		log_fault.p++;
		if(log_fault.p >= LOG_FAULT_MAX) {
			log_fault.p = 0;
		}
		// Flash保存
		flash_erase_sector(&flash_log, LOG_FAULT_1);
		flash_stream_write(&flash_log, LOG_FAULT_1, sizeof(log_fault), (uint8_t *)&log_fault);
		flash_erase_sector(&flash_log, LOG_FAULT_2);
		flash_stream_write(&flash_log, LOG_FAULT_2, sizeof(log_fault), (uint8_t *)&log_fault);
		Log_Info("[LOG RECORD] Flash Hard Fault Error Info\r\n");
		device_mutex_unlock(RT_DEV_LOCK_FLASH);
	}
	// Reboot前のHard Fault Error 情報初期化
	memset(&reboot_info, 0x00, sizeof(LOG_FAULT_VOLATILE));

	// エラー/メッセージをFlashから読み込む
	device_mutex_lock(RT_DEV_LOCK_FLASH);
	flash_stream_read(&flash_log, LOG_FAULT_1,  sizeof(log_fault), (uint8_t *)&log_fault);
	flash_stream_read(&flash_log, LOG_RECORD_1,  sizeof(log_msg), (uint8_t *)&log_msg);
	device_mutex_unlock(RT_DEV_LOCK_FLASH);
	if(memcmp(log_msg.magic, LOG_MAGIC_NUM, sizeof(LOG_MAGIC_NUM)) != 0) {
		device_mutex_lock(RT_DEV_LOCK_FLASH);
		flash_stream_read(&flash_log, LOG_RECORD_2,  sizeof(log_msg), (uint8_t *)&log_msg);
		device_mutex_unlock(RT_DEV_LOCK_FLASH);
		// MAGICナンバーが不正の場合は2面目を読み込み
		if(memcmp(log_msg.magic, LOG_MAGIC_NUM, sizeof(LOG_MAGIC_NUM)) != 0) {
			// Backup領域のMAGICナンバーも不正の場合は初期化
			memcpy(log_msg.magic, LOG_MAGIC_NUM, sizeof(LOG_MAGIC_NUM));
			log_msg.p_ver = LOG_VER_MAX;
			log_msg.p_err = 0;
			log_msg.p_msg = 0;
			memset(log_msg.ver, 0x00, sizeof(VER_INFO)*LOG_VER_MAX);
			memset(log_msg.err, 0x00, sizeof(LOG_INFO)*LOG_ERR_MAX);
			memset(log_msg.msg, 0x00, sizeof(LOG_INFO)*LOG_MSG_MAX);
		}
	} else {
		// Backup領域が不正の場合はBackup領域のみ更新
		device_mutex_lock(RT_DEV_LOCK_FLASH);
		flash_stream_read(&flash_log, LOG_RECORD_2,  sizeof(LOG_MAGIC_NUM), (uint8_t *)&log_msg.magic);
		device_mutex_unlock(RT_DEV_LOCK_FLASH);
		if(memcmp(log_msg.magic, LOG_MAGIC_NUM, sizeof(LOG_MAGIC_NUM)) != 0) {
			memcpy(log_msg.magic, LOG_MAGIC_NUM, sizeof(LOG_MAGIC_NUM));
			log_record_flash_msg(FLASH_ADDR_2);
		}
	}
#if CONFIG_AUDREY_CONF_BACKUP
	flash_t flash_conf_1;
	flash_t flash_conf_2;
	struct wlan_fast_reconnect read_data_conf_1 = {0};
	struct wlan_fast_reconnect read_data_conf_2 = {0};
	uint32_t sec;

	// 直前のエラー情報ポインタ指定
	if(log_msg.p_err == 0) {
		cnt = LOG_ERR_MAX - 1;
	} else {
		cnt= log_msg.p_err - 1;
	}
	if(log_msg.err[cnt].id == MSG_MALLOC_FAIL_RESET) {
		// 直前がヒープ確保失敗でリセットした場合
		device_mutex_lock(RT_DEV_LOCK_FLASH);
		flash_stream_read(&flash_conf_2, FAST_RECONNECT_BACKUP, sizeof(struct wlan_fast_reconnect), (u8 *) &read_data_conf_2);
		if(read_data_conf_2.type == FAST_CONNECT_WLAN) {
			// バックアップ情報がWLANの場合
			flash_stream_read(&flash_conf_1, FAST_RECONNECT_DATA, sizeof(struct wlan_fast_reconnect), (u8 *) &read_data_conf_1);
			if(memcmp(&read_data_conf_1, &read_data_conf_2, sizeof(struct wlan_fast_reconnect)) != 0) {
				// 現在の無線設定値がバックアップ情報と異なる
				sec = read_data_conf_1.security_type;
				// 無線設定情報リストア
				Log_Notify("[LOG RECORD] Restore wlan config\r\n");
				memcpy(&read_data_conf_1, &read_data_conf_2, sizeof(struct wlan_fast_reconnect));
				flash_erase_sector(&flash_conf_1, FAST_RECONNECT_DATA);
				flash_stream_write(&flash_conf_1, FAST_RECONNECT_DATA, sizeof(struct wlan_fast_reconnect), (u8 *) &read_data_conf_1);
				device_mutex_unlock(RT_DEV_LOCK_FLASH);
				// エラーログ保存してシステム再起動
				vTaskDelay(1000 * portTICK_PERIOD_MS);
				SendMessageToStateManager((sec == 0 ? MSG_RESTORE_CONF_RESET_0 : MSG_RESTORE_CONF_RESET), PARAM_NONE);
			}
		}
		device_mutex_unlock(RT_DEV_LOCK_FLASH);
	}

#endif
	// 端末起動から一定時間後にバージョン情報をチェック
	vTaskDelay(LOG_INIT_PERIOD * portTICK_PERIOD_MS);
	// 現行バージョンを取得して比較
	memset(log_ver_buf, 0x00, sizeof(log_ver_buf));
	sprintf(log_ver_buf, "%s_%s", AUDREY_VERSION, scale_ver);
	if(log_msg.p_ver == 0) {
		cnt = LOG_VER_MAX - 1;
	} else if(log_msg.p_ver >= LOG_VER_MAX) {
		// バージョン情報が１つも保存されていない場合は先頭に保存
		cnt = LOG_VER_MAX;
		log_msg.p_ver = 0;
	} else {
		cnt= log_msg.p_ver - 1;
	}
	if(cnt == LOG_VER_MAX || strcmp(log_msg.ver[cnt].info, log_ver_buf)) {
		// バージョン情報が前回と異なる場合は保存
		memcpy(log_msg.ver[log_msg.p_ver].info, log_ver_buf, LOG_VER_LEN-1);
		log_msg.ver[log_msg.p_ver].info[LOG_VER_LEN-1] = 0;
		log_msg.ver[log_msg.p_ver].time = rtc_read();
		log_msg.p_ver++;
		if(log_msg.p_ver >= LOG_VER_MAX) {
			log_msg.p_ver = 0;
		}
		Log_Info("[LOG RECORD] Record Version Info\r\n");
		log_record_flash_msg(FLASH_ADDR_BOTH);
		log_record_exe = FALSE;
	}
	while(1) {
		vTaskDelay(LOG_RECORD_PERIOD * portTICK_PERIOD_MS);
		if(log_record_exe == TRUE) {
			// エラーメッセージの保存要求があれば保存
			log_record_flash_msg(FLASH_ADDR_BOTH);
			log_record_exe = FALSE;
		}
	}
	vTaskDelete(NULL);
}

void log_record_init(void)
{
	if(xTaskCreate(log_record_thread, ((const char*)"log_record_thread"), 1024, NULL, tskIDLE_PRIORITY + 4, NULL) != pdPASS){
		Log_Error("\n\r%s xTaskCreate(log_record_thread) failed", __FUNCTION__);
	}
}
