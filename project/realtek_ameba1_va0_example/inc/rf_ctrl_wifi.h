#ifndef __RF_CTRL_WIFI_H__
#define __RF_CTRL_WIFI_H__

/*
 * Copyright (C) 2017 SHARP Corporation. All rights reserved.
 */
#include "rf_ctrl.h"

/** 外部公開API **/
#ifdef RF_CTRL_WIFI_USE_SSL
RF_CTRL_ERR rf_ctrl_data_upload_ssl(int id, char *data);
#else
RF_CTRL_ERR rf_ctrl_data_upload_wifi(int id, char *data);
#endif
#endif //#ifndef __RF_CTRL_WIFI_H_
