#ifndef __SCALE_UART_H__
#define __SCALE_UART_H__

/*
 * Copyright (C) 2017 SHARP Corporation. All rights reserved.
 */
#include "FreeRTOS.h"
#include "semphr.h"
#include "osdep_api.h"
#include "serial_api.h"
#include "timers.h"

#define UART_TX    				PD_3
#define UART_RX    				PD_0
#define UART_RTS				PD_2
#define UART_CTS				PD_1

#define SCALE_GPIO_RST			PD_4
#define SCALE_GPIO_FUP			PD_6
#define SCALE_GPIO_INT			PD_7


typedef struct {
	char psv;							// 省電力情報
	char body;							// 体重
	char urine;							// 尿量
}STABLE_INFO;

typedef struct {
	u32 time;							// 日時
	int body;							// 体重
	int urine;							// 尿量
	STABLE_INFO stable;					// 安定/不安定情報
}SCALE_INFO;

typedef enum {
	SCALE_MODE_VER = 0,					// バージョン情報取得待ち
	SCALE_MODE_BASE,					// 基準体重測定待ち
	SCALE_MODE_ATRC,					// 上限下限値送信待ち
	SCALE_MODE_PSV,						// 省電力モード
	SCALE_MODE_CHK,						// 体重計チェック中
	SCALE_MODE_OTA,						// ファームウェア更新中(Wireless/Scale)
	SCALE_MODE_FUP,						// Scaleファームウェア更新中
	SCALE_MODE_MAX,
} SCALE_MODE;

extern SCALE_INFO scale_info;
extern SCALE_MODE scale_mode;
extern u8 scale_rcv_cnt;

extern serial_t scale_uart_sobj;
extern gpio_t scale_gpio_reset;
extern gpio_t scale_gpio_fup;
extern gpio_irq_t scale_int;

void scale_xmodem_start(void);

//#if CONFIG_AUDREY_DBG_UPLOAD
//void scale_dbg_start(void);
//#endif

void scale_ota_start(void);
void scale_ota_stop(void);
void scale_uart_send_string(u8 *);
void scale_chk_gravity(u8 *);
int scale_up_start(u8 *, u16);
int scale_is_update(void);				// Scaleファーム更新中確認 0:更新中でない、0以外:更新中
int scale_is_psv(void);					// Scale通信省電力中確認 0:省電力中でない、0以外:省電力中
int scale_is_init(void);				// Scale初期測定中確認 0:初期測定完了、0以外:初期測定中
void scale_up_res(u8);
void scale_reset(void);
void scale_init(void);

#endif //#ifndef __SCALE_UART_H__
