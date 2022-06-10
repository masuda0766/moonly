/*
 * Copyright (C) 2017 SHARP Corporation. All rights reserved.
 */
#ifndef LOG_RECORD_H_
#define LOG_RECORD_H_

#define LOG_VER_MAX		10						// バージョン情報保存数
#define LOG_ERR_MAX		100						// エラー情報保存数
#define LOG_VER_LEN		20						// バージョン情報長

typedef struct hard_fault_data{
	u32 flag;
	u32 StackR0;
	u32 StackR1;
	u32 StackR2;
	u32 StackR3;
	u32 StackR12;
	u32 StackLr;
	u32 StackPc;
	u32 StackPsr;
	u32 Bfar;
	u32 Cfsr;
	u32 Hfsr;
	u32 Dfsr;
	u32 Afsr;
	u32 time;
} HARD_FAULT_DATA;

#define LOG_FAULT_MAX		(FLASH_SECTOR_SIZE - sizeof(HARD_FAULT_DATA) - 4) / sizeof(HARD_FAULT_DATA)

typedef struct {
	u8 id;										// ID
	u8 param;									// パラメータ
	u32 time;									// タイムスタンプ
} LOG_INFO;

typedef struct {
	u8 info[LOG_VER_LEN];						// バージョン情報
	u32 time;									// タイムスタンプ
} VER_INFO;

typedef struct {
	u8 pc[21];										// LR + PC
	u32 time;									// タイムスタンプ
} FAULT_INFO;


void log_record_erase_fault(void);
void log_record_erase_msg(void);
void log_record_show_fault(void);
void log_record_show_msg(void);
void log_record_msg(u8, u8, u8);
void log_record_init(void);
#if CONFIG_AUDREY_LOG_UPLOAD
void log_record_err_get(int, LOG_INFO*);
void log_record_err_del(void);
void log_record_fault_get(int, FAULT_INFO*);
void log_record_erase_fault(void);
void log_record_ver_get(int, VER_INFO*);
#endif
#if CONFIG_AUDREY_CONF_BACKUP
void log_record_backup_wlan_conf(void);
#endif

#endif /* LOG_RECORD_H_ */
