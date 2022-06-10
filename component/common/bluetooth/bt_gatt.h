/*
 *++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * Copyright (c) 1999-2017 BRT Corporation
 *
 * All rights reserved.
 *
 *------------------------------------------------------------------------------
 */
/**
 * @file bt_gatt.h
 * Brief description.
 *
 * Detailed description.
 *
 * @author Weigh
 *
 */
#ifndef BT_GATT_H
#define BT_GATT_H

#include "bt_common.h"

typedef void * tBT_GattConnIF;
typedef void * tBT_GattSvcIF;
typedef void * tBT_GattIncIF;
typedef void * tBT_GattChrIF;
typedef void * tBT_GattDesIF;

/* Error Code in Error Response */
typedef enum {
	/* Internal */
	BT_GATT_OK                                  = 0x00, /* */
	BT_GATT_RES_INVALID_HANDLE,                     /* 0x01 The attribute handle given was not valid on
											 *      this server.
											 */
	BT_GATT_RES_READ_NOT_PERMITTED,         /* 0x02 The attribute cannot be read. */
	BT_GATT_RES_WRITE_NOT_PERMITTED,        /* 0x03 The attribute cannot be written. */
	BT_GATT_RES_INVALID_PDU,                /* 0x04 The attribute PDU was invalid. */
	BT_GATT_RES_INSUFFICIENT_AUTHENTICATION,/* 0x05 The attribute requires authentication before
											 *      it can be read or written.
											 */
	BT_GATT_RES_REQUEST_NOT_SUPPORTED,      /* 0x06 Attribute server does not support the request
											 *      received from the client.
											 */
	BT_GATT_RES_INVALID_OFFSET,             /* 0x07 Offset specified was past the end of the
											 *      attribute
											 */
	BT_GATT_RES_INSUFFICIENT_AUTHORIZATION, /* 0x08 The attribute requires authorization before
											 *      it can be read or written.
											 */
	BT_GATT_RES_PREPARE_QUEUE_FULL,         /* 0x09 Too many prepare writes have been queued,
											 *      for PrepareWrite */
	BT_GATT_RES_ATTRIBUTE_NOT_FOUND,        /* 0x0A No attribute found within the given attribute
											 *      handle range.
											 */
	BT_GATT_RES_ATTRIBUTE_NOT_LONG,         /* 0x0B The attribute cannot be read or written using
											 *      the Read Blob Request, errara 4887 I suggest
											 *      remove it */
	BT_GATT_RES_INSUFFICIENT_ENCRYPTION_KEYSIZE,/* 0x0C The Encryption Key Size used for encrypting
											 *      this link is insufficient.
											 */
	BT_GATT_RES_INVALID_ATTRIBUTE_VALUE_LENGTH, /* 0x0D The attribute value length is invalid for
											 *      the operation.
											 */
	BT_GATT_RES_UNLIKELY_ERROR,             /* 0x0E The attribute request that was requested has
											 *      encountered an error that was unlikely, and
											 *      therefore could not be completed as requested.
											 */
	BT_GATT_RES_INSUFFICIENT_ENCRYPTION,    /* 0x0F The attribute requires encryption before it
											 *      can be read or written.
											 */
	BT_GATT_RES_UNSUPPORTED_GROUPTYPE,      /* 0x10 ReadByGroupType, The attribute type is not
											 *      a supported grouping attribute as defined by a
											 *      higher layer specification.
											 */
	BT_GATT_RES_INSUFFICIENT_RESOURCES,     /* 0x11 Insufficient Resources to complete the request.
											 *      For Write, too long.
											 */
	/* 0x12-0x7F are reserved */
	BT_GATT_RES_INVALID_VALUE				= 0x80,

	/* 0x80-0xFF are application error */
	/* CSS v2 Common Profile and service error code descriptions */
	BT_GATT_RES_OUT_OF_RANGE                = 0xFF,
	BT_GATT_RES_HAS_EXSIT                   = 0xFE,
	BT_GATT_RES_PROCEDURE_ALREADY_IN_PROGRESS = 0xFD,
	BT_GATT_RES_CCC_IMPROPERLY_CONFIGURED   = 0xFC,

	/* Internal */
	BT_GATT_RES_INVALID_IF                  = 0xE8,
	BT_GATT_RES_SMP_FAILED                  = 0xE9,
	BT_GATT_RES_TL_IDLE_TIMEOUT             = 0xEA,
	BT_GATT_RES_CANCEL                      = 0xEB,
	BT_GATT_RES_DONE                        = 0xEC,
	BT_GATT_RES_TL_CONNECT_FAILED           = 0xED,
	BT_GATT_RES_RTXTIMEOUT                  = 0xEE,
	BT_GATT_RES_IGNORE                      = 0xEF,
} tBT_GattResEnum;

typedef struct {
	tBT_GattResEnum result;                             /* Result value */
	tBT_GattConnIF conn;                              /* Connection handle */
	tInt8 rssi;
} tBT_GattReadRssiInd;

typedef enum {
	BT_GATT_EV_CONNECT_COMPLETE             = 0x01,   /* 0x01 tBT_GattConnCmplStru */
	BT_GATT_EV_DISCONNECT_COMPLETE,                   /* 0x02 tBT_GattDisconnCmplStru */
	BT_GATT_EV_MTU_NOTIFICATION,                      /* 0x03 tBT_GattMtuNotifStru */	
	BT_GATT_EV_READ_RSSI_CFM,						  /* 0x04 tBT_GattReadRssiCfm */
	BT_GATT_EV_CONN_INTERVAL_UADATE,				  /* 0x05 tBT_GattUpdateStru */

	BT_GATTS_EV_SERVICE_START               = 0x10,   /* 0x10 tBT_GattSvcStartStru */
	BT_GATTS_EV_SERVICE_STOP,                         /* 0x11 tBT_GattSvcStopStru */
	BT_GATTS_EV_INCLUDE_START,                        /* 0x12 tBT_GattIncStartStru */
	BT_GATTS_EV_CHARACTERISTIC_START,                 /* 0x13 tBT_GattCharStartStru */
	BT_GATTS_EV_DESCRIPTOR_START,                     /* 0x14 tBT_GattDescStartStru */
	BT_GATTS_EV_READ,                                 /* 0x15 tBT_GattsReadIndStru */
	BT_GATTS_EV_WRITE,                                /* 0x16 tBT_GattsWriteIndStru, Write Command */
	BT_GATTS_EV_WRITE_REQ,                            /* 0x17 tBT_GattsWriteIndStru, Write Request */
	BT_GATTS_EV_PREPARE_WRITE,                        /* 0x18 tBT_GattsWriteIndStru, Prepare Write */
	BT_GATTS_EV_EXCUETE_WRITE,                        /* 0x19 tBT_GattsExcWriteIndStru */
	BT_GATTS_EV_HANDLE_VALUE_CFM,                     /* 0x1A tBT_GattsIndValueCfmStru */

	BT_GATTC_EV_SERVICE_DISCOVERED          = 0x20,   /* tBT_GattcSvcDiscoveredStru */
	BT_GATTC_EV_SERVICE_DISCOVERY_CMPL,               /* 0x21 tBT_GattTaskCmplStru */
	BT_GATTC_EV_INCLUDE_SERVICE_FOUND,                /* 0x22 tBT_GattcSvcDiscoveredStru */
	BT_GATTC_EV_INCLUDE_SERVICE_FIND_CMPL,            /* 0x23 tBT_GattTaskCmplStru */
	BT_GATTC_EV_CHARACTERISTIC_DISCOVERED,            /* 0x24 tBT_GattcChrDiscoveredStru */
	BT_GATTC_EV_CHARACTERISTIC_DISCOVERY_CMPL,        /* 0x25 tBT_GattTaskCmplStru */
	BT_GATTC_EV_DESCRIPTOR_DISCOVERED,                /* 0x26 tBT_GattcDesDiscoveredStru */
	BT_GATTC_EV_DESCRIPTOR_DISCOVERY_CMPL,            /* 0x27 tBT_GattTaskCmplStru */
	BT_GATTC_EV_READ_RSP,                             /* 0x28 tBT_GattcReadRspStru */
	BT_GATTC_EV_READ_CMPL,                            /* 0x29 tBT_GattTaskCmplStru */
	BT_GATTC_EV_WRITE_CMPL,                           /* 0x2A tBT_GattTaskCmplStru */
	BT_GATTC_EV_HANDLE_VALUE_INDICATION,              /* 0x2B tBT_GattcHdlValIndStru */
	BT_GATTC_EV_HANDLE_VALUE_NOTIFICATION,            /* 0x2C tBT_GattcHdlValIndStru */
} tBT_GattEventEnum;

typedef enum {
	BT_GATT_ATTR_TYPE_SVC_PRIMARY,
	BT_GATT_ATTR_TYPE_SVC_SECONDARY,
	BT_GATT_ATTR_TYPE_INCLUDE,
	BT_GATT_ATTR_TYPE_CHARACTERISTIC,
	BT_GATT_ATTR_TYPE_CHR_DESCRIPTOR,
} tBT_GattAttrTypeEnum;

/* Characteristic Properties. */
typedef enum {
	BT_GATT_PROP_BROADCAST                  = (1 << 0),  /* 0x01 */
	BT_GATT_PROP_READ                       = (1 << 1),  /* 0x02 */
	BT_GATT_PROP_WRITE_CMD                  = (1 << 2),  /* 0x04 */
	BT_GATT_PROP_WRITE_REQ                  = (1 << 3),  /* 0x08 */
	BT_GATT_PROP_NOTIFY                     = (1 << 4),  /* 0x10 */
	BT_GATT_PROP_INDICATE                   = (1 << 5),  /* 0x20 */
	BT_GATT_PROP_SIGNWRITE                  = (1 << 6),  /* 0x40 */
	BT_GATT_PROP_EXTENDED                   = (1 << 7),  /* 0x80 */
} tBT_GattChrPropsBits;

/* Attribute permissions. */
typedef enum {
	BT_GATT_PERM_WRITE                      = BT_GATT_PROP_WRITE_REQ,
	BT_GATT_PERM_READ                       = BT_GATT_PROP_READ,

	BT_GATT_PERM_WRITE_AUTHEN_REQUEIRED     = (1 << 8),  /* 0x0100 Writable with authentication */
	BT_GATT_PERM_WRITE_ENCRYPT_REQUEIRED    = (1 << 9),  /* 0x0200 Writable with encryption */
	BT_GATT_PERM_WRITE_MITM_REQUEIRED       = (1 << 10), /* 0x0400 Writable with MITM protection */

	BT_GATT_PERM_READ_AUTHEN_REQUEIRED      = (1 << 12), /* 0x1000 Readable with authentication */
	BT_GATT_PERM_READ_ENCRYPT_REQUEIRED     = (1 << 13), /* 0x2000 Readable with encryption */
	BT_GATT_PERM_READ_MITM_REQUEIRED        = (1 << 14), /* 0x4000 Readable with MITM protection */
} tBT_GattAttPermBits;

/* Characteristic Extended Properties bit field */
typedef enum {
	BT_GATT_EXTPROP_RELIABLE_WRITE          = (1 << 0),  /* 0x01 If set, permits reliable writes of
	                                                      *      the Characteristic Value */
	BT_GATT_EXTPROP_WRITABLE_AUX            = (1 << 1),  /* 0x02 If set, permits writes to the
	                                                      *      characteristic descriptor*/
} tBT_GattExtPropBits;

/* Flags in Execute Write Request */
typedef enum {
	BT_GATT_EWRF_CANCEL_ALL                 = 0x00,      /* Cancel all prepared writes.
	                                                      * When the flags parameter is set to 0x00 all pending prepare write values shall 
	                                                      * be discarded for this client. The queue shall then be cleared, and an Execute 
	                                                      * Write Responseshall be sent.
	                                                      */
	BT_GATT_EWRF_IMMEDIATELY_WRITE_ALL      = 0x01       /* Immediately write all pending prepared values.
	                                                      * When the flags parameter is set to 0x01, values that were queued by the 
                                                          * previous prepare write requests shall be written in the order they were received 
                                                          * in the corresponding Prepare Write Request. The queue shall then be cleared, 
                                                          * and an Execute Write Response shall be sent.
                                                          */
} tBT_ExcWriteFlagEnum;

typedef enum {
	BT_GATT_CCC_VAL_NONE                    = 0x0000,
	BT_GATT_CCC_VAL_NOTIFICATION            = 0x0001,    /* The Characteristic Value shall be notified.This value can only 
	                                                      * be set if the characteristic's property has the notify bit set.
	                                                      */
	BT_GATT_CCC_VAL_INDICATION              = 0x0002     /* The Characteristic Value shall be indicated.This value can only 
	                                                      * be set if the characteristic's property has the indicate bit set.
	                                                      */
} tBT_GattCCCValEnum;

/**
 * GATT Callback Function define.
 *
 * @param[in]  ev        The event.
 * @param[in]  param     The parameters.
 */
typedef void (tBT_GattEvCbkFunc)(tBT_GattEventEnum ev, void *param);

/* The parameter of BT_GATT_EV_CONNECT_COMPLETE */
typedef struct {
	tBT_GattConnIF conn;                    /* Connection interface. */
	tBT_AddressStru addr;
	tBT_ResultEnum res;
} tBT_GattConnCmplStru;

/* The parameter of BT_GATT_EV_DISCONNECT_COMPLETE */
typedef struct {
	tBT_GattConnIF conn;                   /* Connection interface. */
	tBT_ResultEnum res;
} tBT_GattDisconnCmplStru;

/* The parameter of BT_GATT_EV_MTU_NOTIFICATION */
typedef struct {
	tBT_GattConnIF conn;                   /* Connection interface. */
	tUint16 mtu;
} tBT_GattMtuNotifStru;

typedef struct {
	tBT_GattConnIF conn;
	void *context;
	tBT_GattEvCbkFunc *cfm_cbk;            /* If NULL, the default callback function called */
} tBT_GattTaskInfoStru;

typedef struct {
	tBT_GattConnIF cif;
	void *context;
	tBT_GattResEnum res;
	tUint16 att_hdl;                       /* When the request of BT_GATTC_ReadReq/BT_GATTC_WriteReq/BT_GATTC_WriteCmd is effective */
} tBT_GattTaskCmplStru;

/* The parameter of BT_GATT_EV_GET_CONN_INTERVAL_UADATE */
typedef struct {
	tBT_GattConnIF cif; 					/* Connection interface. */
	tBT_AddressStru addr;
	tBT_GattResEnum res;
	tUint16 conn_interval;					/* Connection Interval */
}tBT_GattUpdateStru;

/**
 * Register callback function.
 *
 * @param      ev_cbk  The callback function.
 */
void BT_GATT_CallbackRegister(tBT_GattEvCbkFunc *ev_cbk);

/**
 * Unregister callback function.
 *
 * @param      ev_cbk  The callback function.
 */
void BT_GATT_CallbackUnregister(tBT_GattEvCbkFunc *ev_cbk);

/**
 * Set the default ATT_MTU.
 *
 * Over LE link, the minimum of this default ATT_MTU and the remote device's MTU will be applied during
 * a connection. And the applied MTU will be called back by event BT_GATT_EV_MTU_NOTIFICATION, after
 * the procedure Exchange MTU.
 *
 * Over ACL link, this default ATT_MTU is the local max received MTU. And the remote max received MTU
 * will be called back by event BT_GATT_EV_MTU_NOTIFICATION
 *
 * @param[in]  mtu   The MTU, should be greater than or equal to the 23.
 */
void BT_GATT_DefaultMtuSet(tUint16 mtu);

/**
 * Request to disconnect a GATT connection.
 *
 * @param[in]  addr  Address of the remote device.
 *
 * @return     The connection interface.
 */
tBT_GattConnIF BT_GATT_ConnectReq(tBT_AddressStru *addr);

/**
 * Request to disconnect a GATT connection.
 *
 * @param[in]  conn  The connection interface.
 */
void BT_GATT_DisconnectReq(tBT_GattConnIF conn);

/**
 * Request the RSSI value with a GATT connection. And the result will be return with the Callback function
 * (Which was registered registered by BT_GATT_CallbackRegister function).
 *
 * @param[in]  conn  The connection interface.
 */
void BT_GATT_RssiReq(tBT_GattConnIF conn);

/**
 * Get a remote device address.
 *
 * @param[in]      conn      The connection
 * @param[out]     addr      The address
 *
 * @return         TRUE      Success
 *                 FALSE     Fail
 */
tBool BT_GATT_RmtAddrGet(tBT_GattConnIF conn, tBT_AddressStru *addr);

/**
 * Get the MTU of a connection.
 *
 * @param[in]  conn  The connection
 *
 * @return     MTU.
 */
tUint16 BT_GATT_MtuGet(tBT_GattConnIF conn);

/* The parameter of BT_GATTS_EV_SERVICE_START */
typedef struct {
	tBT_GattSvcIF svc;                     /* Service interface. */
	tBT_GattResEnum res;
	tUint16 attr_hdl;
	tUint16 end_hdl;
} tBT_GattSvcStartStru;

/* The parameter of BT_GATTS_EV_SERVICE_STOP */
typedef struct {
	tBT_GattSvcIF svc;                     /* Service interface. */
	tBT_GattResEnum res;
} tBT_GattSvcStopStru;

typedef struct {
	tBT_GattIncIF inc;                     /* include interface. */
	tBT_GattResEnum res;
	tUint16 attr_hdl;
} tBT_GattIncStartStru;

/* The parameter of BT_GATTS_EV_CHARACTERISTIC_START */
typedef struct {
	tBT_GattChrIF chr;                     /* Characteristic interface. */
	tBT_GattSvcIF svc;
	tBT_GattResEnum res;
	tUint16 attr_hdl;
	tUint16 val_hdl;
} tBT_GattCharStartStru;

/* The parameter of BT_GATTS_EV_DESCRIPTOR_START */
typedef struct {
	tBT_GattDesIF des;                     /* Descriptor interface. */
	tBT_GattChrIF chr;
	tBT_GattResEnum res;
	tUint16 attr_hdl;
} tBT_GattDescStartStru;

/* The parameter of BT_GATTS_EV_READ, */
typedef struct {
	tBT_GattConnIF conn;                   /* Connection interface. */
	tBT_GattAttrTypeEnum attr_type;        /* Attribute type. */
	void *att_if;                          /* Characteristic or Descriptor interface. */
	tUint16 att_hdl;                       /* Attribute handle. */
	tUint16 off;
} tBT_GattsReadIndStru;

/* The parameter of BT_GATTS_EV_WRITE, BT_GATTS_EV_WRITE_REQ, BT_GATTS_EV_PREPARE_WRITE */
typedef struct {
	tBT_GattConnIF conn;                   /* Connection interface. */
	tBT_GattAttrTypeEnum attr_type;        /* Attribute type. */
	void *att_if;                          /* Characteristic or Descriptor interface. */
	tUint16 att_hdl;                       /* Attribute handle. */
	tUint16 off;
	tUint16 val_len;
	tUint8 *val;
} tBT_GattsWriteIndStru;

/* The parameter of BT_GATTS_EV_EXCUETE_WRITE */
typedef struct {
	tBT_GattConnIF conn;                   /* Connection interface. */
	tBT_GattAttrTypeEnum attr_type;        /* Attribute type. */
	void *att_if;                          /* Characteristic or Descriptor interface. */
	tUint16 att_hdl;                       /* Characteristic handle. */
	tUint8 flag;                           /* tBT_ExcWriteFlagEnum */
} tBT_GattsExcWriteIndStru;

/* The parameter of BT_GATTS_EV_HANDLE_VALUE_INDICATION_CFM */
typedef struct {
	tBT_GattConnIF conn;                   /* Connection interface. */
	tBT_GattChrIF chr;
	void *context;                         /* should be the last member */
} tBT_GattsIndValueCfmStru;

/**
 * Create a Service.
 *
 * @param[in]  uuid          The Service UUID.
 * @param[in]  ev_cbk        The Event callback function.
 * @param[in]  is_secondary  Indicates if Secondary Service.
 *
 * @return     Service interface.
 */
tBT_GattSvcIF BT_GATTS_ServiceCreate(const tBT_UuidStru *uuid, tBT_GattEvCbkFunc *ev_cbk, tBool is_secondary);

/**
 * Create a Include, and add it to a Service.
 *
 * @param[in]  svc      The Service that the Include will be added to.
 * @param[in]  svc_inc  The included Service, this Service should be started.
 *
 * @return     Include interface.
 */
tBT_GattIncIF BT_GATTS_IncludeAdd(tBT_GattSvcIF svc, tBT_GattSvcIF svc_inc);

/**
 * Create a Characteristic, and add it to a Service.
 *
 * @param[in]  svc        The Service that the Characteristic will be added to.
 * @param[in]  uuid       The Characteristic UUID.
 * @param[in]  props      The Characteristic Properties bits field. (tBT_GattChrPropsBits)
 * @param[in]  val_perms  The Characteristic Value attribute permissions.
 *
 * @return     Descriptor interface.
 */
tBT_GattChrIF BT_GATTS_CharacteristicAdd(tBT_GattSvcIF svc, const tBT_UuidStru *uuid, tUint8 props,
										 tUint16 val_perms);

/**
 * Create a Descriptor, and add it to a Characteristic.
 *
 * @param[in]  chr        The Characteristic that Descriptor will be added to
 * @param[in]  desc_uuid  The Descriptor UUID
 * @param[in]  perms      The permissions (tBT_GattAttPermBits)
 *
 * @return     Descriptor Interface.
 */
tBT_GattDesIF BT_GATTS_DescriptorAdd(tBT_GattChrIF chr, const tBT_UuidStru *desc_uuid, tUint16 perms);

/**
 * Create a Characteristic User Description Descriptor, and add it to a Service.
 *
 * @param[in]  chr    The Characteristic that Descriptor will be added to
 * @param[in]  perms  The permissions (tBT_GattAttPermBits)
 *
 * @return     Descriptor Interface.
 */
tBT_GattDesIF BT_GATTS_DesUserDescriptionAdd(tBT_GattChrIF chr, tUint16 perms);

/**
 * Create a Client Characteristic Configuration Descriptor, and add it to a Service.
 *
 * @param[in]  chr    The Characteristic that Descriptor will be added to
 * @param[in]  props  The Characteristic Properties bits field. (Just BT_GATT_PROP_NOTIFY or
 *                    BT_GATT_PROP_INDICATE)
 * @param[in]  perms  The permissions (tBT_GattAttPermBits)
 * @param[in]  val    The default value.
 *
 * @return     Descriptor Interface.
 */
tBT_GattDesIF BT_GATTS_DesCCCAdd(tBT_GattChrIF chr, tUint8 props, tUint16 perms, tBT_GattCCCValEnum val);

/**
 * Create a Server Characteristic Configuration Descriptor, and add it to a Service.
 *
 * @param[in]  chr    The Characteristic that Descriptor will be added to
 * @param[in]  perms  The permissions (tBT_GattAttPermBits)
 *
 * @return     Descriptor Interface.
 */
tBT_GattDesIF BT_GATTS_DesSCCAdd(tBT_GattChrIF chr, tUint16 perms);

/**
 * Create a Characteristic Presentation Format Descriptor, and add it to a Service.
 *
 * @param[in]  chr          The Characteristic that Descriptor will be added to
 * @param[in]  format       The format
 * @param[in]  exponent     The exponent
 * @param[in]  unit         The unit
 * @param[in]  namespace_   The namespace
 * @param[in]  description  The description
 *
 * @return     Descriptor Interface.
 */
tBT_GattDesIF BT_GATTS_DesPresFormatAdd(tBT_GattChrIF chr, tUint8 format, tUint8 exponent, tUint16 unit,
										tUint8 namespace_, tUint16 description);

/**
 * Create a Characteristic Aggregate Format Descriptor, and add it to a Service.
 *
 * @param[in]  chr      The Characteristic that Descriptor will be added to
 * @param[in]  val      The value
 * @param[in]  val_len  The value length
 *
 * @return     Descriptor Interface.
 */
tBT_GattDesIF BT_GATTS_DesAggrFormatAdd(tBT_GattChrIF chr, tUint8 *val, tUint16 val_len);

/**
 * Delete a Service.
 *
 * This service SHOULD BE stopped or not started, otherwise the function will fail.
 *
 * @param[in]  svc   The Service interface.
 */
void BT_GATTS_ServiceDelete(tBT_GattSvcIF svc);

/**
 * Start a Service.
 *
 * When the Service is started, the Attribute Handle of each attribute in this service will be returned
 * by the callback function.
 *
 * @param[in]  svc   The service interface.
 */
void BT_GATTS_ServiceStart(tBT_GattSvcIF svc);

/**
 * Stop a Service.
 *
 * @param[in]  svc   The service interface.
 */
void BT_GATTS_ServiceStop(tBT_GattSvcIF svc);

/**
 * Send a Notification or Indication of a Characteristic Value.
 *
 * When the Client Characteristic Configuration Descriptor in this Characteristic is set as Notification
 * or Indication, this Value will be notified or indicated.
 *
 * @param[in]  chr      The Characteristic interface.
 * @param[in]  val      The value array.
 * @param[in]  val_len  The value length.
 *
 * @return                   Success or Fail.
 */
tBool BT_GATTS_ChrValueSend(tBT_GattConnIF conn, tBT_GattChrIF chr, tUint8 *val, tUint16 val_len);

/**
 * Send a response to a read request.
 *
 * @param[in]  conn     The connection interface.
 * @param[in]  res      Error code.
 * @param[in]  val  	The value array.
 * @param[in]  val_len  The value length.
 */
void BT_GATTS_ReadRsp(tBT_GattConnIF conn, tBT_GattResEnum res, tUint8 *val, tUint16 val_len);

/**
 * Send a response to a Write request.
 *
 * @param[in]  conn     The connection interface.
 * @param[in]  res      Error code.
 */
void BT_GATTS_WriteRsp(tBT_GattConnIF conn, tBT_GattResEnum res);

/**
 * Send a response to a Execute Write request.
 *
 * @param[in]  conn     The connection interface.
 * @param[in]  res      Error code.
 */
void BT_GATTS_ExecuteWriteRsp(tBT_GattConnIF conn, tBT_GattResEnum res);

/* The parameter of the event BT_GATTC_EV_SERVICE_DISCOVERED and BT_GATTC_EV_INCLUDE_SERVICE_FOUND */
typedef struct {
	tBT_GattConnIF conn;                     /* Connection interface. */
	void *context;
	tBT_GattSvcIF svc;
	tUint16 att_hdl;
	tBT_UuidStru uuid;
} tBT_GattcSvcDiscoveredStru;

typedef struct {
	tBT_GattConnIF conn;                     /* Connection interface. */
	void *context;
	tBT_GattChrIF chr;
	tUint16 att_hdl;
	tBT_UuidStru uuid;
	tUint8 props;
} tBT_GattcChrDiscoveredStru;

typedef struct {
	tBT_GattConnIF conn;                     /* Connection interface. */
	void *context;
	tUint16 att_hdl;
	tBT_UuidStru uuid;
	tUint8 props;
} tBT_GattcDesDiscoveredStru;

typedef struct {
	tBT_GattConnIF conn;                     /* Connection interface. */
	void *context;
	tUint8 *val;
	tUint16 val_off;
	tUint16 val_len;
	tUint16 att_hdl;
} tBT_GattcReadRspStru;

typedef struct {
	tBT_GattConnIF conn;                     /* Connection interface. */
	tUint8 *val;
	tUint16 val_len;
	tUint16 att_hdl;
} tBT_GattcHdlValIndStru;

/**
 * Register default callback function for GATT client.
 *
 * @param      conn    The connection interface.
 * @param      ev_cbk  The callback function.
 */
void BT_GATTC_RegisterEvCallback(tBT_GattConnIF conn, tBT_GattEvCbkFunc *ev_cbk);

/**
 * Unregister default callback function for GATT client.
 *
 * @param      conn    The connection interface.
 * @param      ev_cbk  The callback function.
 */
void BT_GATTC_UnRegisterEvCallback(tBT_GattConnIF conn, tBT_GattEvCbkFunc *ev_cbk);

/**
 * This function is used by a client to discover a specific primary service on a server when only 
 * the Service UUID is known or discover all the primary services on a server.
 *
 * @param[in]  task_info     The task information.
 * @param[in]  uuid          The UUID information (Discover all the primary services when the uuid is NULL).
 */
void BT_GATTC_PrimarySvcDiscover(tBT_GattTaskInfoStru *task_info, const tBT_UuidStru *uuid);

/**
 * This function is used by a client to find include service declarations within a service definition 
 * on a server.
 *
 * @param[in]  task_info     The task information.
 * @param[in]  svc           The service destination.
 */
void BT_GATTC_IncludeSvcFind(tBT_GattTaskInfoStru *task_info, tBT_GattSvcIF svc);

/**
 * This funtion is used by a client to find all the characteristic declarations within a service 
 * definition on a server.
 *
 * @param[in]  task_info     The task information.
 * @param[in]  uuid          The UUID information.
 */
void BT_GATTC_ChrDiscover(tBT_GattTaskInfoStru *task_info, tBT_GattSvcIF svc, const tBT_UuidStru *uuid);

/**
 * This funtion is used by a client to find all the characteristic descriptor's Attribute Handles and 
 * Attribute Types within a service definition on a server.
 *
 * @param[in]  task_info     The task information.
 * @param[in]  chr           The characteristic interface.
 */
void BT_GATTC_AllDesDiscover(tBT_GattTaskInfoStru *task_info, tBT_GattChrIF chr);

/**
 * The Read Request is used to request the server to read the value of an attribute and return its value 
 * in a Read Response.
 *
 * @param[in]  task_info     The task information.
 * @param[in]  att_hdl       The destination attribute handle.
 */
void BT_GATTC_ReadReq(tBT_GattTaskInfoStru *task_info, tUint16 att_hdl);

/**
 * The Read Request is used to request the server to read the value of an attribute and return its value 
 * in a Read Response.
 *
 * @param[in]  task_info     The task information.
 * @param[in]  svc           The service interface.
 * @param[in]  uuid          The UUID information.
 */
void BT_GATTC_ReadByUUID(tBT_GattTaskInfoStru *task_info, tBT_GattSvcIF svc, const tBT_UuidStru *uuid);

/**
 * The Read Multiple Request is used to request the server to read two or more values of a set of attributes 
 * and return their values in a Read Multiple Response.
 *
 * @param[in]  task_info     The task information.
 * @param[in]  hdl_arr       The destination handle array of attributes.
 * @param[in]  hdl_num       The handle number.
 */
void BT_GATTC_MultReadReq(tBT_GattTaskInfoStru *task_info, tUint16 *hdl_arr, tUint8 hdl_num);

/**
 * The Write Request function is used to request the server to write the value of an attribute and acknowledge 
 * that this has been achieved in a Write Response.
 *
 * @param[in]  task_info     The task information.
 * @param[in]  att_hdl       The destination attribute handle.
 * @param[in]  val_len       The value length.
 * @param[in]  val           The value array.
 */
void BT_GATTC_WriteReq(tBT_GattTaskInfoStru *task_info, tUint16 att_hdl, tUint16 val_len, tUint8 *val);

/**
 * The Write Command function is used to request the server to write the value of an attribute.
 *
 * @param[in]  task_info     The task information.
 * @param[in]  att_hdl       The destination attribute handle.
 * @param[in]  val_len       The value length.
 * @param[in]  val           The value array.
 *
 * @return                   Success or Fail.
 */
tBool BT_GATTC_WriteCmd(tBT_GattTaskInfoStru *task_info, tUint16 att_hdl, tUint16 val_len, tUint8 *val);

/**
 * The Handle Value Confirmation function is sent in response to a received Handle Value
 * Indication and confirms that the client has received an indication of the given attribute.
 *
 * @param[in]  conn          The connection interface.
 */
void BT_GATTC_HdlValCfm(tBT_GattConnIF conn);

#endif /* BT_GATT_H */

