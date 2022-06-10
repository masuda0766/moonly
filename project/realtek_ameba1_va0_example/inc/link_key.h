#ifndef __LINK_KEY_H__
#define __LINK_KEY_H__

/*
 * Copyright (C) 2017 SHARP Corporation. All rights reserved.
 */
#include "FreeRTOS.h"
#include "semphr.h"
#include "osdep_api.h"

#define LINK_KEY		PD_5

typedef enum long_press_type{
	LONG_PRESS_BOOT   = 0x00,	/* システム起動時からの長押し */
	LONG_PRESS_NORMAL = 0x01,	/* システム起動後の長押し */
} LONG_PRESS_TYPE;

void link_key_init(void);
void link_key_enable(void);
void link_key_disable(void);

#endif //#ifndef __LINK_KEY_H__
