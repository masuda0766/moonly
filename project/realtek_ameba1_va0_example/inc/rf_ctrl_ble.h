#ifndef __RF_CTRL_BLE_H__
#define __RF_CTRL_BLE_H__

/*
 * Copyright (C) 2017 SHARP Corporation. All rights reserved.
 */
#include "rf_ctrl.h"
#include "bt_gatt.h"

/** 外部公開API **/
void rf_ctrl_ble_conn_complete_cb(tBT_GattConnCmplStru *param);
void rf_ctrl_ble_disconn_complete_cb(tBT_GattDisconnCmplStru *param);
RF_CTRL_ERR rf_ctrl_data_upload_ble(int id, char *data);
void rf_ctrl_data_upload_ble_cb(tBT_GattResEnum result, char *resp);
bool rf_ctrl_data_transport_char_desc_check(void);
bool rf_ctrl_wait_ble_resp(void);
RF_CTRL_ERR rf_ctrl_ble_start_server(void);
RF_CTRL_ERR rf_ctrl_ble_stop_server(void);
#if CONFIG_AUDREY_ALWAYS_BT_ON
RF_CTRL_ERR rf_ctrl_ble_resume_server(void);
RF_CTRL_ERR rf_ctrl_ble_pause_server(void);
void rf_ctrl_adv_disable(void);
void rf_ctrl_adv_enable(void);
#endif //#if CONFIG_AUDREY_ALWAYS_BT_ON
bool rf_ctrl_start_adv_on_AP(void);
bool rf_ctrl_stop_adv_on_AP(void);
void rf_ctrl_data_upload_ble_enable_cb(void);
void rf_ctrl_conf_data_recv_cb(char *data);
RF_CTRL_ERR rf_ctrl_conf_data_send(char *data);
void rf_ctrl_conf_data_send_comp_cb(void);

#endif //#ifndef __RF_CTRL_BLEI_H_
