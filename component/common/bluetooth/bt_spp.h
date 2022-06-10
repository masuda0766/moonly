/*
 *++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * Copyright (c) 1999-2017 BRT Corporation
 *
 * All rights reserved.
 *
 *------------------------------------------------------------------------------
 */
/**
 * @file bt_spp.h
 * Brief description.
 *
 * Detailed description.
 *
 * @author Asher
 *
 */

#ifndef _BT_SPP_
#define _BT_SPP_

#include "bt_common.h"

typedef void * tBT_SppConnIF;
typedef void * tBT_SppSvcIF;

/* Event Code for SPP */
typedef enum {
	BT_SPP_EV_SERVICE_START = 0x01,	                  /* tBT_SppSvcIF */
	BT_SPP_EV_SERVICE_STOP,                           /* tBT_SppSvcIF */
	BT_SPP_EV_CONNECT_IND,                            /* tBT_SppConnInd */
	BT_SPP_EV_CONNECT_COMPLETE,                       /* tBT_SppConnInd */
	BT_SPP_EV_DISCONNECT_COMPLETE,                    /* tBT_SppConnInd */
	BT_SPP_EV_DATA_IND                                /* tBT_SppDataInd */
} tBT_SppEventEnum;

/* Error Code in Error Response and Event Indicate */
typedef enum {
	BT_SPP_RET_SUCCESS = 0x00,
	BT_SPP_RET_L2CAP_CONNECT_FAILED,
	BT_SPP_RET_L2CAP_DISCONNECT_COMPLETE,
	BT_SPP_RET_UA_IND_DISC,
	BT_SPP_RET_DM_IND,
	BT_SPP_RET_CONNECTION_DISCONNECT_REQ,
	BT_SPP_RET_DLCI_DISCONNECT_REQ,
	BT_SPP_RET_DLCI_UA_IND_DISC,
	BT_SPP_RET_DLCI_DM_IND,
	BT_SPP_RET_DLCI_DISC_IND,
	BT_SPP_RET_DLCI_UNREG_SERVER,
	BT_SPP_RET_RTX_TIMEOUT,
	BT_SPP_RET_CONNECT_RSP_FAILED,                    /* Upper do */
	BT_SPP_RET_CONNECT_REQ_DLCIX_DUPLICATE,           /* BD and Server_channel duplicate connect_req. */
	BT_SPP_RET_CONNECT_REQ_INVALID_SERVERCHANNEL,
	BT_SPP_RET_DATA_IND_ERROR,
	BT_SPP_RET_SECURITY_FAILED,
	BT_SPP_RET_NSC_CFM,
	BT_SPP_RET_DONE,
	BT_SPP_RET_SDP_ERROR,
} tBT_SppResult;

typedef struct
{
	tUint32 data_1;
	tUint16 data_2;
	tUint16 data_3;
	tUint8 data_4[8];
} tSpp_SvcUUIDStru;

typedef enum {
	BT_SPP_ROLE_SERVER,
	BT_SPP_ROLE_CLIENT,
} tSpp_ConnRole;

typedef struct {
	tBool custom_svc;                                 /* 1: mean a custom UUID service; 0: mean SPP service */
	tSpp_SvcUUIDStru uuid;                            /* Custom UUID */
	tUint16 mfs;                                      /* Maximum Transfer Unit(It will use larger memory with the larger mfs size) */
	tUint8 init_credit;                               /* Credit number, Range: 1~7 */
} tBT_SPPSvcPro;

typedef struct {
	tBT_SppResult result;                             /* Result value */
	tBT_SppConnIF conn;                               /* Connection handle */
	tUint16 cur_mfs;								  /* Max bytes of packet */
	tUint8 bd[6];                                     /* Remote device address */
} tBT_SppConnInd;

typedef struct {
	tBT_SppConnIF conn;                               /* Connection handle */
	tUint8 *data;
	tUint16 length;
} tBT_SppDataInd;

typedef struct {
	tUint8 bd[6];                                     /* Remote device address */
	tUint16 mfs;                                      /* Maximum Transfer Unit */
	tUint8 init_credit;                               /* Credit number, Range: 1~7 */
} tBT_SppConnReq;

/**
 * SPP Callback Function define.
 *
 * @param[in]  ev         The event code.
 * @param[in]  param      The parameters.
 */
typedef void (tBT_SppEvCbkFunc)(tBT_SppEventEnum ev, void *param);

/**
 * Create a SPP Service.
 *
 * @param[in]  svc_pro    The Service properties.
 * @param[in]  cbk        The Event callback function.
 *
 * @return     Service interface.
 */
tBT_SppSvcIF BT_SPP_ServiceCreate(tBT_SPPSvcPro *svc_pro, tBT_SppEvCbkFunc *cbk);

/**
 * Start a Service.
 *
 * @param[in]  svc        The service interface.
 *
 * @return     Result
 */
tBool BT_SPP_ServiceStart(tBT_SppSvcIF svc);

/**
 * Stop a Service.
 *
 * @param[in]  svc        The service interface.
 *
 * @return     Result: True or False
 */
tBool BT_SPP_ServiceStop(tBT_SppSvcIF svc);

/**
 * Delete a Service.
 *
 * This service SHOULD BE stopped or not started, otherwise the function will fail.
 *
 * @param[in]  svc        The Service interface.
 *
 * @return     Result: True or False
 */
tBool BT_SPP_ServiceDelete(tBT_SppSvcIF svc);

/**
 * Response a connection request.
 *
 * @param[in]  conn       The Connection handle.
 * @param[in]  result     The Response code (BT_SPP_RET_SUCCESS or BT_SPP_RET_CONNECT_RSP_FAILED).
 *
 * @return     Result: True or False
 */
tBool BT_SPP_ConnectRsp(tBT_SppConnIF conn, tBT_SppResult result);

/**
 * Launch a connection request.
 *
 * @param[in]  req        The Connection handle.
 * @param[in]  cbk        The Event callback function.
 *
 * @return     Result: The Connection handle.
 */
tBT_SppConnIF BT_SPP_Connect(tBT_SppConnReq *req, tBT_SppEvCbkFunc *cbk);

/**
 * Disconnect a connection.
 *
 * @param[in]  conn       The Connection handle.
 *
 * @return     Result: True or False
 */
tBool BT_SPP_Disconnect(tBT_SppConnIF conn);

/**
 * Send data with the connection.
 *
 * @param[in]  conn       The Connection handle.
 * @param[in]  data       The Data buffer.
 * @param[in]  length     The Data length.
 *
 * @return     Result: True or False
 */
tBool BT_SPP_SendData(tBT_SppConnIF conn, tUint8 *data, tUint16 length);

#endif /* _BT_SPP_ */

