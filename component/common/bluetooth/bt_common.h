/*
 *++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * Copyright (c) 1999-2017 BRT Corporation
 *
 * All rights reserved.
 *
 *------------------------------------------------------------------------------
 */
/**
 * @file bt_common.h
 * The Bluelet stack common APIs.
 *
 * Detailed description.
 *
 * @author Weigh
 *
 */

#ifndef BT_COMMON_H
#define BT_COMMON_H

#define tUint32     unsigned long
#define tInt32      signed long
#define tUint16     unsigned short
#define tInt16      signed short
#define tUint8      unsigned char
#define tInt8       signed char
#define tUChar      unsigned char
#define tChar       char
#define tWChar      unsigned short
#define tBool       unsigned char   /* None 0 - TRUE, 0 - FALSE */
#define tUStr       tUChar *        /* null terminated string, usually UTF-8 */
#define tStr        tChar *         /* null terminated ANSI    string  */
#define tWStr       tWChar *        /* null terminated UNICODE string  */

#define BT_SDK_VERSION_SIZE		(32)
typedef tUint8 (*tVersion)[BT_SDK_VERSION_SIZE];

typedef enum {
	BT_ADDR_CAP_LE                                             = 1,
	BT_ADDR_CAP_BREDR                                          = 2
} tBT_AddrCapEnum;

typedef enum {
	BT_ADDR_TYPE_PUBLIC,
	BT_ADDR_TYPE_RANDOM
} tBT_AddrTypeEnum;

typedef struct {
	tUint8 cap;                    /* Bluetooth Address capability, is defined by tBT_AddrCapEnum */
	tUint8 type;                   /* Bluetooth Address type, is defined by tBT_AddrTypeEnum */
	tUint8 bd[6];                  /* Bluetooth Address */
} tBT_AddressStru;

typedef struct {
	tBT_AddressStru *remote_devstru;
	tUint16 con_num;			   /* Number of successful pairs */
}tBT_PairedInfo;

typedef struct {
	enum {
		BT_UUID_LEN_16                                         = 2,
		BT_UUID_LEN_32                                         = 4,
		BT_UUID_LEN_128                                        = 16
	} len;
	union {
		tUint8 uuid128[BT_UUID_LEN_128];
		tUint32 uuid32;
		tUint16 uuid16;
	} u;
} tBT_UuidStru;

typedef struct {
	tUint16 len;
	tUint8 data[1];
} tBT_ValueStru;

typedef enum {
	/* These values are the LMP/HCI Error Codes defined in Bluetooth Core Spec */
	BT_RES_OK                                                 = 0x00,
	BT_RES_UNKNOWN_BT_COMMAND                                 = 0x01,
	BT_RES_UNKNOWN_CONNECTION_IDENTIFIER                      = 0x02,
	BT_RES_HARDWARE_FAILURE                                   = 0x03,
	BT_RES_PAGE_TIMEOUT                                       = 0x04,
	BT_RES_AUTHENTICATION_FAILURE                             = 0x05,
	BT_RES_PIN_OR_KEY_MISSING                                 = 0x06,
	BT_RES_MEMORY_CAPACITY_EXCEEDED                           = 0x07,
	BT_RES_CONNECTION_TIMEOUT                                 = 0x08,
	BT_RES_CONNECTION_LIMIT_EXCEEDED                          = 0x09,
	BT_RES_SYNCHRONOUS_CONNECTION_LIMIT_TO_A_DEVICE_EXCEEDED  = 0x0a,
	BT_RES_ACL_CONNECTION_ALREADY_EXISTS                      = 0x0b,
	BT_RES_COMMAND_DISALLOWED                                 = 0x0c,
	BT_RES_CONNECTION_REJECTED_LIMITED_RESOURCES              = 0x0d,
	BT_RES_CONNECTION_REJECTED_SECURITY_REASONS               = 0x0e,
	BT_RES_CONNECTION_REJECTED_UNACCEPTABLE_BD_ADDR           = 0x0f,
	BT_RES_CONNECTION_ACCEPT_TIMEOUT_EXCEEDED                 = 0x10,
	BT_RES_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE             = 0x11,
	BT_RES_INVALID_BT_COMMAND_PARAMETERS                      = 0x12,
	BT_RES_REMOTE_USER_TERMINATED_CONNECTION                  = 0x13,
	BT_RES_REMOTE_TERMINATED_CONNECTION_LOW_RESOURCES         = 0x14,
	BT_RES_REMOTE_TERMINATED_CONNECTION_POWER_OFF             = 0x15,
	BT_RES_CONNECTION_TERMINATED_BY_LOCAL_HOST                = 0x16,
	BT_RES_REPEATED_ATTEMPTS                                  = 0x17,
	BT_RES_PAIRING_NOT_ALLOWED                                = 0x18,
	BT_RES_UNKNOWN_LMP_PDU                                    = 0x19,
	BT_RES_UNSUPPORTED_REMOTE_OR_LMP_FEATURE                  = 0x1a,
	BT_RES_SCO_OFFSET_REJECTED                                = 0x1b,
	BT_RES_SCO_INTERVAL_REJECTED                              = 0x1c,
	BT_RES_SCO_AIR_MODE_REJECTED                              = 0x1d,
	BT_RES_INVALID_LMP_PARAMETERS                             = 0x1e,
	BT_RES_UNSPECIFIED_ERROR                                  = 0x1f,
	BT_RES_UNSUPPORTED_LMP_PARAMETER_VALUE                    = 0x20,
	BT_RES_ROLE_CHANGE_NOT_ALLOWED                            = 0x21,
	BT_RES_LMP_OR_LL_RESPONSE_TIMEOUT                         = 0x22,
	BT_RES_LMP_ERROR_TRANSACTION_COLLISION                    = 0x23,
	BT_RES_LMP_PDU_NOT_ALLOWED                                = 0x24,
	BT_RES_ENCRYPTION_MODE_NOT_ACCEPTABLE                     = 0x25,
	BT_RES_LINK_KEY_CANNOT_BE_CHANGED                         = 0x26,
	BT_RES_REQUESTED_QOS_NOT_SUPPORTED                        = 0x27,
	BT_RES_INSTANT_PASSED                                     = 0x28,
	BT_RES_PAIRING_WITH_UNIT_KEY_NOT_SUPPORTED                = 0x29,
	BT_RES_DIFFERENT_TRANSACTION_COLLISION                    = 0x2a,
	BT_RES_QOS_UNACCEPTABLE_PARAMETER                         = 0x2c,
	BT_RES_QOS_REJECTED                                       = 0x2d,
	BT_RES_CHANNEL_ASSESSMENT_NOT_SUPPORTED                   = 0x2e,
	BT_RES_INSUFFICIENT_SECURITY                              = 0x2f,
	BT_RES_PARAMETER_OUT_OF_MANDATORY_RANGE                   = 0x30,
	BT_RES_ROLE_SWITCH_PENDING                                = 0x32,
	BT_RES_RESERVED_SLOT_VIOLATION                            = 0x34,
	BT_RES_ROLE_SWITCH_FAILED                                 = 0x35,
	BT_RES_EXTENDED_INQUIRY_RESPONSE_TOO_LARGE                = 0x36,
	BT_RES_SIMPLE_PAIRING_NOT_SUPPORTED_BY_HOST               = 0x37,
	BT_RES_HOST_BUSY_PAIRING                                  = 0x38,
	BT_RES_CONNECTION_REJECTED_NO_SUITABLE_CHANNEL_FOUND      = 0x39,
	BT_RES_CONTROLLER_BUSY                                    = 0x3a,
	BT_RES_UNACCEPTABLE_CONNECTION_INTERVAL                   = 0x3b,
	BT_RES_DIRECTED_ADVERTISING_TIMEOUT                       = 0x3c,
	BT_RES_CONNECTION_TERMINATED_DUE_TO_MIC_FAILURE           = 0x3d,
	BT_RES_CONNECTION_FAILED_TO_BE_ESTABLISHED                = 0x3e,
	BT_RES_MAC_CONNECTION_FAILED                              = 0x3f,
	/* Other unknown LMP/HCI Error Codes returned by the Controller 0x40 - 0xFF */

	/* Private error codes 0x0100 - 0xFFFF */
	BT_RES_HOST_NOT_READY                                     = 0x100,
	BT_RES_NOT_SUPPORTED                                      = 0x101,
	BT_RES_LOCAL_HOST_TIMEOUT                                 = 0x102,
	BT_RES_USER_TERMINATED                                    = 0x103,
	BT_RES_CONTROLLER_UNAVAILABLE                             = 0x104,
	BT_RES_INVALID_UI_PARAMETERS                              = 0x105,
} tBT_ResultEnum;

/**
 * Type of the general callback function used to handle the execution result of an API request.
 *
 * @param[in]  result  The result.
 * @param[in]  param   The parameter.
 */
typedef void (tBT_CfmFunc)(tBT_ResultEnum result, void *param);

typedef enum {
	/* These values are the BT state */
	BT_ST_IDLE,                     /* Power off */
	BT_ST_INIT,                     /* Initializing */
	BT_ST_READY,                    /* Power on */
	BT_ST_REMOVING,                 /* Is being destroyed */
	BT_ST_DONE,                     /* Reserved */
} tBT_StatusEnum;

/**
 * Initialize the Bluelet Stack and Bluetooth Controller.
 *
 * Before calling this function, the transport which between Bluelet Stack and Bluetooth Controller
 * MUST be ready.
 * The BT_Done() function will CANCEL the initialization process.
 * The result will be notified by cfm_cbk function, when Bluelet Stack and Bluetooth Controller are
 * initialized completely.
 *
 * @param[in]  cfm_cbk  The confirm callback function.
 */
void BT_Init(tBT_CfmFunc *cfm_cbk);

/**
 * Release Bluelet Stack resource, all connections and Controller related resource.
 *
 * Generally, this function should be called before removes or powers off the controller.
 * When the Bluetooth Controller is been powered off or removed accidently, could call this function
 * to notify the Stack.
 * The result will be notified by the cfm_cbk function.
 *
 * @param[in]  cfm_cbk       The confirm callback function.
 * @param[in]  is_power_off  If the Bluetooth Controller has been removed or powered off accidently.
 */
void BT_Done(tBT_CfmFunc *cfm_cbk, tBool is_power_off);

/**
 * Get the BT state.
 *
 * @return     The Bt state.
 */
tBT_StatusEnum BT_GetState(void);

/**
 * Get the Bluelet Stack version.
 *
 * This function can be called everytime.
 * @param[out]  major  The Bluelet Stack major version.
 * @param[out]  minor  The Bluelet Stack minor version.
 */
void BT_GetVersion(tVersion major, tVersion minor);

/*
 * External functions shall be implemented by the APP.
 */

/**
 * Get public address allocated for this device.
 *
 * @param      loc_bd  The local Bluetooth
 * 
 * @return     True or False, If return true the parameter bd will be used, if return false the default bd will be used
 */
extern tBool BT_EXT_LocBDGet(tUint8 loc_bd[6]);

/**
 * Reset BT chip.
 *
 * @return     Success or Fail.
 */
extern tBool BT_EXT_BTChipEnable(tBool enable);

/**
 * Init Transport.
 *
 * @return     Success or Fail.
 */
extern tBool BT_EXT_TLInit(void);

/**
 * Change Transport param.
 *
 * @return     Success or Fail.
 */
extern tBool BT_EXT_TLParam(tUint32 baudrate);

/**
 * Read bytes stream from Transport.
 *
 * @param[out] buf   The buffer
 * @param[in]  len   The length
 *
 * @return     Actual length put in buffer.
 */
extern tUint32 BT_EXT_TLDataRead(tUint8 *buf, tUint32 len);

/**
 * Write bytes stream to Transport.
 *
 * @param[in]  buf   The buffer
 * @param[in]  len   The length
 *
 * @return     Actual length wrote to transport layer.
 */
extern tUint32 BT_EXT_TLDataWrite(tUint8 *buf, tUint32 len);

/**
 * Done Transport.
 *
 * @return     Success or Fail.
 */
extern tBool BT_EXT_TLDone(void);

/**
 * Write Data to persistent memory.
 *
 * @param[in]  data  The data
 * @param[in]  len   The length
 */
extern tUint16 BT_EXT_PersistentWrite(tUint8 *data, tUint16 len);

/**
 * Read Data from persistent memory.
 *
 * @param[out] data  The data
 * @param[in]  len   The length
 */
extern tUint16 BT_EXT_PersistentRead(tUint8 *data, tUint16 len);

/**
 * Write debug log to the platform dependent output device.
 *
 * @param[in]  data  The log data.
 * @param[in]  len   The length
 */
extern void BT_EXT_DbgPrint(tUint32 len, tChar *data);

#endif /* BT_COMMON_H */

