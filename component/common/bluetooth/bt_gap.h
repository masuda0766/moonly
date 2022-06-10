/*
 *++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * Copyright (c) 1999-2017 BRT Corporation
 *
 * All rights reserved.
 *
 *------------------------------------------------------------------------------
 */
/**
 * @file bt_gap.h
 * Brief description.
 *
 * @author Weigh
 *
 */

#ifndef BT_GAP_H
#define BT_GAP_H

#include "bt_common.h"

typedef enum {
	BT_GAP_EV_INQUIRY_RESULT,               /* tBT_GapInquiryRetStru */
	BT_GAP_EV_INQUIRY_COMPLETE,             /* tBT_GapInquiryCmpStru */
	BT_GAP_REMOTE_DEVICE_NAME_UPDATED,		/* tBT_GapGetRemoteNameCmpStru */
	BT_GAP_EV_USER_CONFIRM_REQUEST,         /* tBT_GapUserConfirmReqStru. Call BT_UserConfirmReply to accept/refuse */
	BT_GAP_EV_USER_PASSKEY_REQUEST,         /* tBT_GapUserPasskeyReqStru. Call BT_PasskeyEntryReply to accept/refuse */
	BT_GAP_EV_USER_PASSKEY_NOTIFICATION,    /* tBT_GapUserPasskeyNotifStru. Display the passkey to end user */
	BT_GAP_EV_SIMPLE_PAIRING_COMPLETE,      /* tBT_GapSimplePairingCmplStru. Kill the window to display passkey */
	BT_GAP_EV_PIN_CODE_REQ,					/* tBT_GapPincodeReqStru. Call BT_GAP_BrPincodeReply to accept/refuse */
	BT_GAP_EV_REMOTE_DEVICE_PAIRED_INFO,	/* tBT_AddressStru */
} tBT_GapEvEnum;

typedef enum {
	BT_GAP_IOCAP_DISPLAY_ONLY               = 0x00,
	BT_GAP_IOCAP_DISPLAY_YESNO              = 0x01,
	BT_GAP_IOCAP_KEYBOARD_ONLY              = 0x02,
	BT_GAP_IOCAP_NO_INPUT_NO_OUTPUT         = 0x03,
	BT_GAP_IOCAP_KEYBOARD_DISPLAY           = 0x04
} tBT_GapIOCapEnum;

typedef enum {
	BT_GAP_SECU_REQ_BOND                    = (1 << 3), /* 0x08 When this value is not set,
														 * Each connection requires a password pairing. */
} tBT_GapSecuPropBits;

typedef enum {
	BT_GAP_BLE_ADV_TYPE_UNDIRECT = 0,       /* (default) Connectable and scannable undirected
	                                         * advertising (ADV_IND)
	                                         */
	BT_GAP_BLE_ADV_TYPE_DIRECT,             /* Connectable high duty cycle directed advertising
	                                         * (ADV_DIRECT_IND, high duty cycle)
	                                         */
	BT_GAP_BLE_ADV_TYPE_SCANABLE,           /* Scannable undirected advertising (ADV_SCAN_IND) */
	BT_GAP_BLE_ADV_TYPE_NONCONN,            /* Non connectable undirected advertising (ADV_NONCONN_IND) */
	BT_GAP_BLE_ADV_TYPE_LOW_DUTY,           /* Connectable low duty cycle directed advertising
	                                         * (ADV_DIRECT_IND, low duty cycle)
	                                         */
} tBT_GapLEAdvTypeEnum;

typedef enum {
	BT_GAP_BLE_OWN_ADDRESS_TYPE_PUBLIC      = 0x00,
	BT_GAP_BLE_OWN_ADDRESS_TYPE_RANDOM      = 0x01
} tBT_GapLEOwnAddressTypeEnum;

typedef enum {
	BT_GAP_BLE_ADV_CHNMAP_CH37              = (1 << 0), /* 0x01 */
	BT_GAP_BLE_ADV_CHNMAP_CH38              = (1 << 1), /* 0x02 */
	BT_GAP_BLE_ADV_CHNMAP_CH39              = (1 << 2), /* 0x04 */
	BT_GAP_BLE_ADV_CHNMAP_CHALL             = (BT_GAP_BLE_ADV_CHNMAP_CH37 | BT_GAP_BLE_ADV_CHNMAP_CH38 |
											   BT_GAP_BLE_ADV_CHNMAP_CH39),
} tBT_GapLEAdvChnMapBits;

typedef enum {
	BT_GAP_BLE_ADV_FILTER_ALL,              /* 0x00 [DEFAULT] Process scan and connection requests
											 * from all devices (i.e., the White List is not in use).
											 */
	BT_GAP_BLE_ADV_FILTER_SCAN_WHITE_LIST,  /* 0x01 Process connection requests from all devices and
											 * only scan requests from devices that are in the White
											 * List.
											 */
	BT_GAP_BLE_ADV_FILTER_CONN_WHITE_LIST,  /* 0x02 Process scan requests from all devices and only
											 * connection requests from devices that are in the White
											 * List.
											 */
	BT_GAP_BLE_ADV_FILTER_ALL_WHITE_LIST,   /* 0x03 Process scan and connection requests only from
											 * devices in the White List.
											 */
} tBT_GapLEAdvFilterEnum;


typedef enum { /* Advertising or Scan Response Data node bit field. */
	BT_GAP_BLE_ADVDATA_MBIT_NAME            = (1 << 0), /* 0x01 Device Name. */
	BT_GAP_BLE_ADVDATA_MBIT_MENUDATA        = (1 << 1), /* 0x02 Menufacturer Data. */
	BT_GAP_BLE_ADVDATA_MBIT_UUID            = (1 << 2), /* 0x04 16 bits Service UUID. */
	BT_GAP_BLE_ADVDATA_MBIT_TX_POWER        = (1 << 3), /* 0x08 TX Power Level, -127 to +127dBm */
	BT_GAP_BLE_ADVDATA_MBIT_APPEARANCE      = (1 << 4), /* 0x10 Appearance */
	BT_GAP_BLE_ADVDATA_MBIT_SERVICE_DATA    = (1 << 5), /* 0x20 Service Data */
} tBT_GapLEAdvDataBits;

typedef enum { /* Scan mode define. */
	BT_GAP_BR_NO_SCAN_ENABLE                = 0x00, /* Default */
	BT_GAP_BR_INQUIRY_SCAN_ENABLE_PAGE_SCAN_DISABLE = 0x01,
	BT_GAP_BR_INQUIRY_SCAN_DISABLE_PAGE_SCAN_ENABLE = 0x02,
	BT_GAP_BR_INQUIRY_SCAN_ENABLE_PAGE_SCAN_ENABLE = 0x03,
	BT_GAP_BT_SCAN_ENABLE_ERROR             = 0xFF
} tBT_GapBrScanModeEnum;

typedef enum { /* Device information source */
	BT_GAP_REMOTE_INFO_SRC_INQUIRY          = 0x01, /* Inquiry */
	BT_GAP_REMOTE_INFO_SRC_ADV_REPORT       = 0x03, /* Advertising report */
	BT_GAP_REMOTE_INFO_SRC_SCAN_RESPONSE    = 0x04, /* Scan response */
} tBT_GapDeviceInfoSrc;

typedef struct {
	tBT_AddressStru addr;                   /* Address structure */
	tBT_GapDeviceInfoSrc src;               /* The Device Information source */
	tUint8 dev_cap;                         /* BT_DEVICE_CAP_BLE or BT_DEVICE_CAP_BREDR */
	tUint8 page_scan_rmode;                 /* Page Scan Repetition Mode, shall be ignored if addr.atype is HCI_OWN_ADDRESS_TYPE_RANDOM_DEVICE_ADDRESS */
	tUint16 clock_offset;                   /* Clock offset, shall be ignored if addr.atype is HCI_OWN_ADDRESS_TYPE_RANDOM_DEVICE_ADDRESS */
	tUint32 class_of_device;                /* Class of Device, shall be ignored if addr.atype is HCI_OWN_ADDRESS_TYPE_RANDOM_DEVICE_ADDRESS */
	tUint16 eir_size;                       /* Size of the Extended Inquiry Response Data or LE Advertising Report Data */
	tUint8 rssi;                            /* RSSI, Range -127 <= rssi <= 20; 127 means RSSI is not available */
	tUint8 eir[1];                          /* Extended Inquiry Response Data or LE Advertising Report Data. The content may be different
                                             * from the one got from the device discovery procedure. The Host Stack may modify it, e.g. add
                                             * the complete name got from name discovery procedure, add a service list got from service
                                             * discovery procedure. The Host Stack will construct an EIR packet for a device not supporting EIR.
                                             */
} tBT_GapInquiryRetStru;

typedef struct {
	tBT_ResultEnum status;
} tBT_GapInquiryCmpStru;

typedef struct {
	tBT_ResultEnum status;
	tUint8 bd[6];
	tUint8 remote_name[248];
} tBT_GapGetRemoteNameCmpStru;

/* The parameter of BT_GAP_EV_USER_CONFIRM_REQUEST */
typedef struct {
	tBT_AddressStru addr;
	tUint32 passkey;
} tBT_GapUserConfirmReqStru;

/* The parameter of BT_GAP_EV_USER_PASSKEY_EREQUEST */
typedef struct {
	tBT_AddressStru addr;
} tBT_GapUserPasskeyReqStru;

/* The parameter of BT_GAP_EV_USER_PASSKEY_NOTIFICATION */
typedef struct {
	tBT_AddressStru addr;
	tUint32 passkey;
} tBT_GapUserPasskeyNotifStru;

/* The parameter of BT_GAP_EV_SIMPLE_PAIRING_COMPLETE */
typedef struct {
	tBT_AddressStru addr;
} tBT_GapSimplePairingCmplStru;

typedef struct {
	/* The Advertising_Interval_Min and Advertising_Interval_Max range 0x0020 to 0x4000, 
		but shall not be set to less than 0x00A0 (100 ms) if the Advertising_Type
		is set to 0x02(ADV_SCAN_IND) or 0x03(ADV_NONCONN_IND). */
	tUint16 int_min;                        /* Advertising Interval Min */
	tUint16 int_max;                        /* Advertising Interval Max */
	tBT_AddressStru peer_addr;              /* Peer Address */
	tUint8 type;                            /* Advertising Type, mandatory to set BT_ADDR_TYPE_RANDOM, tBT_GapLEAdvTypeEnum */
	tUint8 own_address_type;                /* Own Address Type, mandatory to set BT_GAP_BLE_OWN_ADDRESS_TYPE_RANDOM, 
	                                           tBT_GapLEOwnAddressTypeEnum */
	tUint8 chn_map;                         /* Advertising Channel Map, tBT_GapLEAdvChnMapBits */
	tUint8 filter;                          /* Advertising Filter Policy, tBT_GapLEAdvFilterEnum */
} tBT_GapLEAdvParamStru;

typedef struct {
	tUint8 data_len;                        /* Include Data field only */
	tUint16 uuid16;                         /* Service UUID 16bits */
	tUint8 *data;                           /* Data */
} tBT_ServiceDataStru;

typedef struct {
	tUint8 data_len;                        /* Include Manufacturer Data field only */
	tUint8 company_id[2];                   /* Company Identifier Code. */
	tUint8 *data;                           /* Manufacturer Data. */
} tBT_ManufacturerDataStru;

typedef struct {
	tChar dev_name[31];                     /* Device Name, The length of the fluid, it declared by MAX length. */
	tBT_ManufacturerDataStru manufacturer_data; /* Menufacturer Data. */
	tBT_ServiceDataStru service_data;
	tBT_UuidStru *uuid;
	tUint8 uuid_num;
	tUint8 tx_power_level;                  /* TX Power Level, -127 to +127dBm */
	tUint16 appearence;                     /* Appearance */
	tBT_GapLEAdvDataBits mask;              /* Bit Mask.
								             * If the bit is set to one, then the node associated with that
								             * bit will be enabled.
								             */
} tBT_EirDataStru;

typedef enum {
	BT_DEVICE_TYPE_BREDR					= 0x01, /* Only initiate BR/EDR device discovery - Ignored by LE-only Controller */
	BT_DEVICE_TYPE_LE						= 0x02, /* Only initiate LE device discovery - Ignored by BR/EDR-only Controller */
	BT_DEVICE_TYPE_BOTH_BREDR_LE			= BT_DEVICE_TYPE_BREDR | BT_DEVICE_TYPE_LE, /* Initiated both BR/EDR and LE device discovery. */
} tBT_DeviceTypeEnum;

typedef struct {
	tUint8 length;                          /* Maximum amount of time duration for each Inquiry operation. Both the BR/EDR and LE Range: 0x01 - 0x30 (*1.28s).
	                                         * The name discovery procedure is not controlled by this duration on BR/EDR mode.
	                                         */
	tUint8 num_rsp;                         /* Maximum number of responses from each inquiry before this inquiry is halted. 0 means unlimited number of responses.
	                                         * This paramter supported by BR/EDR only.
	                                         */
	tUint8 enable_ble_duplicate_filtering;  /* The Filter_Duplicates parameter controls whether the Link Layer should filter out duplicate advertising reports (Filtering_Enabled) 
	                                         * to the Host, or if the Link Layer should generate advertising reports for each packet received (Filtering_Disabled).
	                                         */
	tUint8 enable_ble_scan_req;             /* The LE_Scan_Type parameter controls the type of scan to perform. 
	                                         * 0: Passive Scanning. No SCAN_REQ packets shall be sent.
	                                         * 1: Active scanning. SCAN_REQ packets may be sent. (Default)
	                                         */
	tUint16 le_scan_interval;               /* This is defined as the time interval from when the Controller started its last LE scan until it begins the subsequent LE scan. 
	                                         * Range: 0x0004 to 0x4000, the default value is used if it over range.
	                                         * Default: 0x0048 (45 ms)
	                                         * Time = N * 0.625 msec
	                                         * Time Range: 2.5 msec to 10.24 seconds.
	                                         */
	tUint16 le_scan_window;                 /* The duration of the LE scan. LE_Scan_Window shall be less than or equal to LE_Scan_Interval.
	                                         * Range: 0x0004 to 0x4000, the default value is used if it over range.
	                                         * Default: 0x0030 (30 ms)
	                                         * Time = N * 0.625 msec
	                                         * Time Range: 2.5 msec to 10240 msec
	                                         */
	tBT_DeviceTypeEnum dev_type;            /* Witch device type be inquiried */
} tBT_InquiryReqStru;

typedef struct {
	tUint8 bd[6];                           /* Remote device address for BR/EDR */
	tUint8 page_scan_repetition_mode;       /* This paramter returned by Inquiry operation */
	tUint16 clock_offset;                   /* This paramter returned by Inquiry operation */
} tBT_RemoteNameReqStru;

/* The parameter of BT_GAP_EV_PIN_CODE_REQ */
typedef struct {
	tUint8 bd[6];                           /* Initiate the requested remote device address */
} tBT_GapPincodeReqStru;

/**
 * GAP callback function define.
 *
 * @param[in]   ev      The event.
 * @param[in]   param   The parameter.
 */
typedef void (tBT_GAP_EvCbk)(tBT_GapEvEnum ev, void *param);

/**
 * Register the GAP callback function.
 *
 * @param[in]  ev_cbk  The callback function.
 */
void BT_GAP_CallbackReg(tBT_GAP_EvCbk *ev_cbk);

/**
 * Unregister the GAP callback function.
 *
 * @param[in]  ev_cbk  The callback function.
 */
void BT_GAP_CallbackUnreg(tBT_GAP_EvCbk *ev_cbk);

/**
 * Get the local Bluetooth address.
 *
 * @param[out] bd    Bluetooth Address
 */
tBool BT_GAP_LocalBDGet(tUint8 br_bd[6], tUint8 ble_bd[6]);

/**
 * Set the Pairing Features.
 *
 * @param[in]  io_cap     The IO Capability.
 * @param[in]  secu_prop  The requested Security Properties (bit-mask tBT_GapSecuPropBits)
 */
void BT_GAP_PairingFeatureSet(tBT_GapIOCapEnum io_cap, tUint8 secu_prop);

/**
 * Get the Pairing Features.
 *
 * @param[out] io_cap     The IO Capability.
 * @param[out] secu_prop  The requested Security Properties (bit-mask tBT_GapSecuPropBits)
 *
 * @return     TRUE       Successful
 *             FALSE      Failed
 */
tBool BT_GAP_PairingFeatureGet(tBT_GapIOCapEnum *io_cap, tUint8 *secu_prop);

/**
 * Reply the Passkey Entry Request.
 *
 * @param[in]  addr     The address
 * @param[in]  passkey  The passkey
 * @param[in]  accept   Whether accept or not.
 */
void BT_GAP_PasskeyEntryReply(tBT_AddressStru *addr, tUint32 passkey, tBool accept);

/**
 * Reply the User Confirm Request.
 *
 * @param[in]  addr     The address.
 * @param[in]  accept   Whether accept or not.
 */
void BT_GAP_UserConfirmReply(tBT_AddressStru *addr, tBool accept);

/**
 * Inquiry and cancel inquiry device.
 *
 * @param[in]  req      The Inquiry parameters.
 * @param[in]  enable   true: Inquiry; false: cancel inquiry.
 */
void BT_GAP_Inquiry(tBT_InquiryReqStru *req, tBool enable);

/**
 * Remote device name request. Should not be called until BT_GAP_Inquiry operation complete
 * (BT_GAP_EV_INQUIRY_COMPLETE happens).
 *
 * @param[in]  req      The request parameters.
 * @param[in]  enable   true: Enable; false: Cancel.
 */
void BT_GAP_GetRemoteName(tBT_RemoteNameReqStru *req, tBool enable);

/**
 * Write inquiry scan activity parameters.
 *
 * @param[in]  interval   INQUIRY SCAN INTERVAL:
                          Range: 0x0012 to 0x1000; only even values are valid
                          Default: 0x1000
                          Mandatory Range: 0x0012 to 0x1000
                          Time = N * 0.625 msec
                          Time Range: 11.25 to 2560 msec
                          Time Default: 2.56 sec
 * @param[in]  window     INQUIRY SCAN WINDOW: 
                          Range: 0x0011 to 0x1000
                          Default: 0x0012
                          Mandatory Range: 0x0011 to Inquiry Scan Interval
                          Time = N * 0.625 msec
                          Time Range: 10.625 msec to 2560 msec
                          Time Default: 11.25 msec.
 * @param[in]  cfm_cbk     The result callback function.
 */
void BT_GAP_BrInquiryScanActivitySet(tUint16 interval, tUint16 window, tBT_CfmFunc *cfm_cbk);

/**
 * Write page scan activity parameters.
 *
 * @param[in]  interval   INQUIRY SCAN INTERVAL:
                          Range: 0x0012 to 0x1000; only even values are valid
                          Default: 0x0800
                          Mandatory Range: 0x0012 to 0x1000
                          Time = N * 0.625 msec
                          Time Range: 11.25 to 2560 msec
                          Time Default: 1.28 sec
 * @param[in]  window     INQUIRY SCAN WINDOW: 
                          Range: 0x0011 to 0x1000
                          Default: 0x0012
                          Mandatory Range: 0x0011 to Page Scan Interval
                          Time = N * 0.625 msec
                          Time Range: 10.625 msec to 2560 msec
                          Time Default: 11.25 msec.
 * @param[in]  cfm_cbk     The result callback function.
 */
void BT_GAP_BrPageScanActivitySet(tUint16 interval, tUint16 window, tBT_CfmFunc *cfm_cbk);

/**
 * Write simple pairing mode (Default disable).
 *
 * @param[in]  enable      1: enable; 0: diasble.
 * @param[in]  cfm_cbk     The result callback function.
 */
void BT_GAP_BrSSPSet(tBool enable, tBT_CfmFunc *cfm_cbk);

/**
 * Get simple pairing mode.
 *
 * @return     Current ssp enable state.
 */
tBool BT_GAP_BrSSPGet(void);

/**
 * Write scan enable.
 *
 * @param[in]  enable      1: enable; 0: diasble.
 * @param[in]  cfm_cbk     The result callback function.
 */
void BT_GAP_BrScanEnableSet(tBT_GapBrScanModeEnum scan_enable, tBT_CfmFunc *cfm_cbk);

/**
 * Read scan enable.
 *
 * @return     Current scan enable mode.
 */
tBT_GapBrScanModeEnum BT_GAP_BrScanEnableGet(void);

/**
 * Write local device name.
 *
 * @param[in]  name        Device name string.
 * @param[in]  cfm_cbk     The result callback function.
 */
void BT_GAP_BrDeviceNameSet(tUint8 *name, tBT_CfmFunc *cfm_cbk);

/**
 * Read local device name.
 *
 * @param[in]  cfm_cbk     The result callback function(The result paramter type is "String").
 */
void BT_GAP_BrDeviceNameGet(tBT_CfmFunc *cfm_cbk);

/**
 * Write class of device.
 *
 * @param[in]  cod         Class of device.
 * @param[in]  cfm_cbk     The result callback function.
 */
void BT_GAP_BrClassOfDevice(tUint32 cod, tBT_CfmFunc *cfm_cbk);

/**
 * PIN code request reply or negative reply.
 *
 * @param[in]  req         PIN code request parameters.
 * @param[in]  pincode     PIN code string.
 * @param[in]  accept      1: Reply; 2: Negative reply.
 */
void BT_GAP_BrPincodeReply(tBT_GapPincodeReqStru *req, tUint8 *pincode, tBool accept);

/**
 * Write authentication requested.
 *
 * @param[in]  enable      Enable or disable authentication requested.
 * @param[in]  cfm_cbk     The result callback function.
 */
void BT_GAP_BrAuthEnable(tUint8 enable, tBT_CfmFunc *cfm_cbk);

/**
 * Write the extended inquiry response to be sent during the extended inquiry response procedure.
 *
 * @param[in]  eir_data    The EIR data.
 * @param[in]  cfm_cbk     The result callback function.
 */
void BT_GAP_BrWriteEIRData(tBT_EirDataStru *eir_data, tBT_CfmFunc *cfm_cbk);

/**
 * Update the LE controller Advertising Data.
 *
 * The Advertising Data will be applied in next Advertising Enabling.
 *
 * @param[in]  adv_data  The advance data
 */
void BT_GAP_BleAdvertisingDataUpdate(tBT_EirDataStru *adv_data);

/**
 * Update the LE controller Scan Response Data.
 *
 * The Scan Response Data will be applied in next Advertising Enabling.
 *
 * @param[in]  scan_rsp_data  The scan rsp data
 */
void BT_GAP_BleScanResponseDataUpdate(tBT_EirDataStru *scan_rsp_data);

/**
 * Set the LE controller Advertising parameters.
 *
 * The parameters will be applied in next Advertising Enabling.
 *
 * @param[in]  param    The parameters.
 */
void BT_GAP_BleAdvertisingParamsSet(tBT_GapLEAdvParamStru *param);

/**
 * Get the LE controller Advertising parameters.
 *
 * @param[out] param    The parameters
 *
 * @return     TRUE     Successful
 *             FALSE    Failed
 */
tBool BT_GAP_BleAdvertisingParamsGet(tBT_GapLEAdvParamStru *param);

/**
 * Set the LE controller Advertising enable or not, cannot repeat open!
 *
 * @param[in]  enable_adv  Whether enable Advertising or not.
 * @param[in]  cfm_cbk     The result callback function.
 */
void BT_GAP_BleAdvertisingSet(tBool enable_adv, tBT_CfmFunc *cfm_cbk);

/**
 * Set the LE controller Advertising enable or not.
 *
 * @return     TRUE     Advertising enabled
 *             FALSE    Advertising disabled
 */
tBool BT_GAP_BleAdvertisingGet(void);

/**
 * Add a device to LE White List.
 * This Function can be used at any time except when:
 *	the advertising filter policy uses the white list and advertising is enabled.
 *	the scanning filter policy uses the white list and scanning is enabled.
 *	the initiator filter policy uses the white list and a create connection command is outstanding.
 *
 * @param[in]  addr     The device address.
 * @param[in]  cfm_cbk  The result callback function.
 */
void BT_GAP_BleAddToWhitelist(tBT_AddressStru *addr, tBT_CfmFunc *cfm_cbk);

/**
 * Remove a device from LE White List.
 * This Function can be used at any time except when:
 *	the advertising filter policy uses the white list and advertising is enabled.
 *	the scanning filter policy uses the white list and scanning is enabled.
 *	the initiator filter policy uses the white list and a create connection command is outstanding.
 *
 * @param[in]  addr     The device address.
 * @param[in]  cfm_cbk  The result callback function.
 */
void BT_GAP_BleRemoveFromWhitelist(tBT_AddressStru *addr, tBT_CfmFunc *cfm_cbk);

/**
 * Clear the LE White List.
 * This Function can be used at any time except when:
 *	the advertising filter policy uses the white list and advertising is enabled.
 *	the scanning filter policy uses the white list and scanning is enabled.
 *	the initiator filter policy uses the white list and a create connection command is outstanding.
 *
 * @param[in]  cfm_cbk  The result callback function.
 */
void BT_GAP_BleClearWhitelist(tBT_CfmFunc *cfm_cbk);

/**
 * Get the pairing device info.
 * This function retrieves information about the device.
 *	After obtaining the device information, the results is trigger the BT_GAP_EV_REMOTE_DEVICE_PAIRED_INFO event. 
 *	You can obtain up to ten information about the matching device.
 */
void BT_GAP_GetPairedInfo(void);

/**
 * Delete paired device info.
 *
 * @param[in] addr		The matching device information address structure type to be deleted.
 *           			If the parameter is NULL, all device information will be removed.
 */
void BT_GAP_DelPairedInfo(tBT_AddressStru *addr);

#endif /* BT_GAP_H */

