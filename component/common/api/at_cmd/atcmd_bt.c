#include <platform_opts.h>
#include "log_service.h"
#include "atcmd_bt.h"
#include "bt_gap.h"
#include "bt_util.h"

tBT_GattConnIF  bt_peer_conn_if;
tBT_SppConnIF	spp_conn_if;
tBT_GattConnIF  gatt_initiator_conn;
tBT_SppConnIF	spp_client_conn;
tBT_AddressStru bt_paring_dev;

struct list_head br_req_list;
static unsigned int btScanNum = 0;

typedef struct {
	struct list_head list;
	tBT_RemoteNameReqStru req;
} tBT_BrNameReq;

char is_bt_ready() {
	tBT_StatusEnum status = BT_GetState();
	if (status != BT_ST_READY) {
		AT_PRINTK("Error, BT is not ready\n\r");
		return 0;
	}
	else
		return 1;
}

void print_bt_scan_result(tBT_GapInquiryRetStru* result)
{
	char dev_type[10];
	char addr_type[10];
	char info_src[10];
	
	sprintf(dev_type,"%s",(result->dev_cap==BT_DEVICE_TYPE_BREDR)? "BR/EDR":
						  (result->dev_cap==BT_DEVICE_TYPE_LE)? "LE":
						  (result->dev_cap==BT_DEVICE_TYPE_BOTH_BREDR_LE)? "BOTH":"UNKNOWN");
	sprintf(addr_type,"%s",(result->addr.type==BT_ADDR_TYPE_PUBLIC)? "public":
						   (result->addr.type==BT_ADDR_TYPE_RANDOM)? "random":"unknown");
	sprintf(info_src,"%s",(result->src==BT_GAP_REMOTE_INFO_SRC_INQUIRY)? "INQUIRY":
						  (result->src==BT_GAP_REMOTE_INFO_SRC_ADV_REPORT)? "ADV":
						  (result->src==BT_GAP_REMOTE_INFO_SRC_SCAN_RESPONSE)? "SCAN_RSP":"unknown");
						  
	printf("\tDeviceType\tAddrType\t%-17s\tPacketType\trssi\n\r","BT_Addr");
	printf("%d\t%-10s\t%-8s\t"BD_ADDR_FMT"\t%-8s\t%d\n\r",
			++btScanNum,dev_type,addr_type,BD_ADDR_ARG(result->addr.bd),info_src,(tInt8)result->rssi);


	tUint16 total_len;
	tUint16 pos = 0;
	tUint16 offset = 0;
	BT_AdvStru ad;
	
	if (result->eir_size == 0){
		printf("\n\r");
		return;
	}
	
	total_len = result->eir_size;
	pos = 0;
	offset = 0;
	while (pos < total_len) {
		offset = get_ad_data(result->eir+pos, total_len-pos,&ad);
		if (offset == 0) {
			printf ("Error when parsing EIR data\n\r");
			break;
		}
		pos += offset;
		tUint8 ad_type_str[32];
		get_ad_type_str(ad.ad_type, ad_type_str);
		printf("[%02x][%s] ",ad.ad_type,ad_type_str);
		switch(ad.ad_type) {
			case BT_AD_TYPE_FLAGS: {
				int len = ad.ad_len-1;
				for(int i=0; i<len; i++) {
					printf("%02X ",ad.data[i]);
				}
				break;
			}
			case BT_AD_TYPE_INCOMPLETE_SERVICE_UUID_16:
			case BT_AD_TYPE_COMPLETE_SERVICE_UUID_16: {
				int len = ad.ad_len-1;
				u16 *ptr = (u16*)ad.data;
				for(int i=0; i<len; i+=2) {
					printf("%04X,",*ptr);
					ptr++;
				}
				break;
			}
			case BT_AD_TYPE_INCOMPLETE_SERVICE_UUID_32:
			case BT_AD_TYPE_COMPLETE_SERVICE_UUID_32: {
				int len = ad.ad_len-1;
				u32 *ptr = (u32*)ad.data;
				for(int i=0; i<len; i+=4) {
					printf("%08X,",*ptr);
					ptr++;
				}
				break;
			}
			case BT_AD_TYPE_INCOMPLETE_SERVICE_UUID_128:
			case BT_AD_TYPE_COMPLETE_SERVICE_UUID_128: {
				int len = ad.ad_len-1;
				for(int i=0; i*16+15<len; i++) {
					for(int j=15; j>=0; j--) {
						printf("%02X",ad.data[i*16+j]);
					}
					if (i%16==15)
						printf(",");
				}
				break;
			}
			case BT_AD_TYPE_SHORT_LOCAL_NAME:
			case BT_AD_TYPE_COMPLETE_LOCAL_NAME:
				printf("%s",ad.data);
				break;
			case BT_AD_TYPE_TX_POWER_LEVEL:
				printf("%d",(char)ad.data[0]);
				break;
			case BT_AD_TYPE_SERVICE_DATA: {
				int len = ad.ad_len-1;
				printf("UUID=%04X, data=",*(u16*)ad.data);
				for(int i=2; i<len; i++) {
					printf("%02X ",ad.data[i]);
				}
				break;
			}
			case BT_AD_TYPE_APPEARANCE:
				printf("%d",*(u16*)ad.data);
				break;
			case BT_AD_TYPE_MANUFACTURER_DATA: {
				int len = ad.ad_len-1;
				printf("CompanyID=%04X, data=",*(u16*)ad.data);
				for(int i=2; i<len; i++) {
					printf("%02X ",ad.data[i]);
				}
				break;
			}	
		}
		printf("\n\r");
		if (ad.data != NULL)
			free(ad.data);
	}
	printf("\n\r");
}

static void send_br_name_request()
{
	tBT_BrNameReq* entry;
	int i=1;
	while(!list_empty(&br_req_list)) {
		entry = list_first_entry(&br_req_list, tBT_BrNameReq, list);
		list_del_init(&entry->list);
		printf("BT_GAP_GetRemoteName: "BD_ADDR_FMT"\n\r",BD_ADDR_ARG(entry->req.bd));
		BT_GAP_GetRemoteName(&entry->req, 1);
		free(entry);
	}
}

void at_gap_ev_cb(tBT_GapEvEnum ev, void *param)
{
	switch (ev) {
		case BT_GAP_EV_INQUIRY_RESULT: {
			tBT_GapInquiryRetStru *in = param;
			
			print_bt_scan_result(in);
			
			if (in->dev_cap & BT_DEVICE_TYPE_BREDR && in->src == BT_GAP_REMOTE_INFO_SRC_INQUIRY) {
				tBT_BrNameReq *req_list = (tBT_BrNameReq *)malloc(sizeof(tBT_BrNameReq));
				memcpy(req_list->req.bd,in->addr.bd,6);
				req_list->req.page_scan_repetition_mode = in->page_scan_rmode;
				req_list->req.clock_offset = in->clock_offset;
				list_add_tail(&req_list->list, &br_req_list);
			}
			break;
		}
		case BT_GAP_EV_INQUIRY_COMPLETE: {
			tBT_GapInquiryCmpStru *in = param;
			if (in->status == BT_RES_OK)
				printf("[ATBS] GAP inquiry complete\n\r");
			else if (in->status == BT_RES_USER_TERMINATED)
				printf("[ATBS] GAP inquiry stopped by ATBS=0\n\r");
			else
				printf("[ATBS] [BT_GAP_EV_INQUIRY_COMPLETE] Error, error code (tBT_ResultEnum): 0x%x\n\r",in->status);
			
			send_br_name_request();
			break;
		}
		case BT_GAP_REMOTE_DEVICE_NAME_UPDATED: {
			tBT_GapGetRemoteNameCmpStru *in = param;
			AT_PRINTK("GAP inquiry device name update: "BD_ADDR_FMT"\t%s",BD_ADDR_ARG(in->bd),in->remote_name);			
			break;
		}
		case BT_GAP_EV_USER_CONFIRM_REQUEST: { /* tBT_GapUserConfirmReqStru. Call BT_UserConfirmReply to accept/refuse */
			tBT_GapUserConfirmReqStru *in = param;
			printf("\n\r[GAP] GAP user confirm request, pairing remote addr = "BD_ADDR_FMT"\n\r",BD_ADDR_ARG(in->addr.bd));
			memcpy(&bt_paring_dev, &in->addr, sizeof(tBT_AddressStru));
			printf("[GAP] Use ATBR=1/0 to accept/reject \n\r");
			break;
		}
		case BT_GAP_EV_USER_PASSKEY_REQUEST: { /* tBT_GapUserPasskeyReqStru. Call BT_PasskeyEntryReply to accept/refuse */
			tBT_GapUserPasskeyReqStru *in = param;
			printf("\n\r[GAP] GAP user passkey entry request, pairing remote addr = "BD_ADDR_FMT"\n\r",BD_ADDR_ARG(in->addr.bd));
			memcpy(&bt_paring_dev, &in->addr, sizeof(tBT_AddressStru));
			printf("[GAP] Use ATBK=PASSKEY to enter corresponding passkey\n\r");
			break;
		}
		case BT_GAP_EV_USER_PASSKEY_NOTIFICATION: { /* tBT_GapUserPasskeyNotifStru. Display the passkey to end user */
			tBT_GapUserPasskeyNotifStru *in = param;
			printf("\n\r[GAP] GAP user passkey notification, pairing remote addr = "BD_ADDR_FMT"\n\r",BD_ADDR_ARG(in->addr.bd));
			printf("[GAP] Passkey = %d\n\r",in->passkey);
			break;
		}
		case BT_GAP_EV_SIMPLE_PAIRING_COMPLETE: { /* tBT_GapSimplePairingCmplStru. Kill the window to display passkey */
			tBT_GapSimplePairingCmplStru *in = param;
			printf("\n\r[GAP] GAP simple paring complete, pairing remote addr = "BD_ADDR_FMT"\n\r",BD_ADDR_ARG(in->addr.bd));
			break;
		}
		case BT_GAP_EV_PIN_CODE_REQ: { /* tBT_GapPincodeReqStru. Call BT_GAP_BrPincodeReply to accept/refuse */
			tBT_GapPincodeReqStru *in = param;
			printf("\n\r[GAP] GAP pin code request, pairing remote addr = "BD_ADDR_FMT"\n\r",BD_ADDR_ARG(in->bd));
			printf("[GAP] BT_GAP_BrPincodeReply(1234)\n\r");
			BT_GAP_BrPincodeReply(in, "1234", 1);
			break;
		}
		case BT_GAP_EV_REMOTE_DEVICE_PAIRED_INFO: {
			tBT_PairedInfo *in = param;
			printf("[ATBG] [BT_GAP_EV_REMOTE_DEVICE_PAIRED_INFO] Number of paired device: %d\r\n", in->con_num);
			for(int i=0; i<in->con_num; i++) {
				char dev_type[10];
				char addr_type[10];
				sprintf(dev_type,"%s",(in->remote_devstru[i].cap==BT_ADDR_CAP_LE)? "LE":
									  (in->remote_devstru[i].cap==BT_ADDR_CAP_BREDR)? "BR/EDR":"UNKNOWN");
				sprintf(addr_type,"%s",(in->remote_devstru[i].type==BT_ADDR_TYPE_PUBLIC)? "public":
									   (in->remote_devstru[i].type==BT_ADDR_TYPE_RANDOM)? "random":"unknown");;
				printf("[BT_GAP_EV_REMOTE_DEVICE_PAIRED_INFO][%d] DeviceType = %s, AddrType = %s, BT_Addr = "BD_ADDR_FMT"\n\r",
					i, dev_type,addr_type, BD_ADDR_ARG(in->remote_devstru[i].bd));
			}
			break;
		}
		default:
			printf("[Warning] [ATCMD_BT] Unhandled GAP event: %x\n\r",ev);
	}
}

void fATBA (void *arg) {
	tUint8 br_bd[6];
	tUint8 ble_bd[6];
	
	if (!is_bt_ready())
		return;
	
	AT_PRINTK("[ATBA]: _AT_BT_DEVICE_ADDR_\n\r");
	BT_GAP_LocalBDGet(br_bd,ble_bd);
	AT_PRINTK("BR_BD_ADDR = "BD_ADDR_FMT,BD_ADDR_ARG(br_bd));
	AT_PRINTK("BLE_BD_ADDR = "BD_ADDR_FMT,BD_ADDR_ARG(ble_bd));
}

static void at_discoverable_on_cb(tBT_ResultEnum result)
{
	if ( result == BT_RES_OK) {
		printf("BR/EDR is discoverable\n\r");
	}
	else {
		printf("\n\r[discoverable_on_cb] Error, error code (tBT_ResultEnum): 0x%x\n\r",result);
	}
	
}

static void at_discoverable_off_cb(tBT_ResultEnum result)
{
	if ( result == BT_RES_OK) {
		printf("BR/EDR is undiscoverable\n\r");
	}
	else {
		printf("\n\r[at_discoverable_off_cb] Error, error code (tBT_ResultEnum): 0x%x\n\r",result);
	}
}

static void at_adv_on_cb(tBT_ResultEnum result)
{
	if ( result == BT_RES_OK) {
		printf("LE ADV is on\n\r");
	}
	else {
		printf("\n\r[at_adv_on_cb] Error, error code (tBT_ResultEnum): 0x%x\n\r",result);
	}
	
}

static void at_adv_off_cb(tBT_ResultEnum result)
{
	if ( result == BT_RES_OK) {
		printf("LE ADV is off\n\r");
	}
	else {
		printf("\n\r[at_adv_off_cb] Error, error code (tBT_ResultEnum): 0x%x\n\r",result);
	}
}

void fATBB (void *arg) {
	int argc = 0;
	char *argv[MAX_ARGC] = {0};
	u8 param;
	
	if (!is_bt_ready())
		return;
	
	argc = parse_param(arg, argv);
	
	if (argc != 3)
		goto exit;
	
	param = atoi((const char *)(argv[2]));
	AT_PRINTK("[ATBB]: _AT_BT_BROADCAST_[%s,%d]",argv[1],param);

	if(strcmp(argv[1], "BR") == 0) {
		if (param == 0) {
			if (BT_GAP_BrScanEnableGet() != BT_GAP_BR_NO_SCAN_ENABLE)
				BT_GAP_BrScanEnableSet(BT_GAP_BR_NO_SCAN_ENABLE, at_discoverable_off_cb);
			else
				AT_PRINTK("[ATBB] BR/EDR is already undiscoverable");
		}
		else if (param == 1) {
			if (BT_GAP_BrScanEnableGet() == BT_GAP_BR_NO_SCAN_ENABLE)
				BT_GAP_BrScanEnableSet(BT_GAP_BR_INQUIRY_SCAN_ENABLE_PAGE_SCAN_ENABLE, at_discoverable_on_cb);
			else
				AT_PRINTK("[ATBB] BR/EDR is already discoverable");	
		}
		else
			goto exit;
	}
	else if(strcmp(argv[1], "LE") == 0) {
		if (param == 0) {
			if (BT_GAP_BleAdvertisingGet() == 1)
				BT_GAP_BleAdvertisingSet(FALSE, at_adv_off_cb);
			else
				AT_PRINTK("[ATBB] LE ADV is already off");
		}
		else if (param == 1) {
			if (BT_GAP_BleAdvertisingGet() == 0)
				BT_GAP_BleAdvertisingSet(TRUE, at_adv_on_cb);
			else
				AT_PRINTK("[ATBB] LE ADV is already on");	
		}
		else
			goto exit;
	}
	else
		goto exit;
	return;
exit:
	AT_PRINTK("[ATBB] Usage: ATBB=BR/LE,1/0\n\r");
}

void fATBC (void *arg) {
	int argc = 0;
	char *argv[MAX_ARGC] = {0};
	tUint16 mtu;

	if (!is_bt_ready())
		return;
	
	argc = parse_param(arg, argv);

	if ( argc < 3 || argc > 4)
		goto exit;
	
	AT_PRINTK("[ATBC]: _AT_BT_GATT_CONNECT_[%s,%s,%s]",argv[1],argv[2],argv[3]);
	if (strlen(argv[2]) != 2*BD_ADDR_LEN)
		goto exit;	
	tBT_AddressStru addr;
	if(strcmp(argv[1], "P") == 0)
		addr.type = BT_ADDR_TYPE_PUBLIC;
	else if(strcmp(argv[1],"R") == 0)
		addr.type = BT_ADDR_TYPE_RANDOM;
	else
		goto exit;	
	hex_str_to_bd_addr(strlen(argv[2]), argv[2], addr.bd);
	mtu = (argc==4)? atoi((const char *)(argv[3])): 256;
	BT_GATT_DefaultMtuSet(mtu);
	gatt_initiator_conn = BT_GATT_ConnectReq(&addr);
	return;
exit:
	AT_PRINTK("[ATBC] Usage: ATBC=P/R,BLE_BD_ADDR[,mtu]");
	AT_PRINTK("[ATBC] addr_type: P=public, R=random; default mtu: 256\n\r");
}

void fATBD (void *arg) {
	int argc = 0;
	char *argv[MAX_ARGC] = {0};
	
	if (!is_bt_ready())
		return;
	
	argc = parse_param(arg, argv);
	
	AT_PRINTK("[ATBD]: _AT_BT_DISCONNECT_[%s]",argv[1]);
	
	if (argc != 2) {
		AT_PRINTK("[ATBD] Usage: ATBD=GATT/SPP\n\r");
		return;
	}
	
	if (strcmp(argv[1], "GATT") == 0) {
		if (bt_peer_conn_if != NULL)
			BT_GATT_DisconnectReq(bt_peer_conn_if);
		else
			AT_PRINTK("[ATBD]: bt_peer_conn_if is null\n\r");
	}
	else if (strcmp(argv[1], "SPP") == 0) {
		if (spp_conn_if != NULL)
			BT_SPP_Disconnect(spp_conn_if);
		else
			AT_PRINTK("[ATBD]: spp_conn_if is null\n\r");
	}
	else {
		AT_PRINTK("[ATBD] Usage: ATBD=GATT/SPP\n\r");
		return;
	}
}

void fATBG (void *arg) {

	if (!is_bt_ready())
		return;
	
	AT_PRINTK("[ATBG]: _AT_BT_GET_PAIRED_INFO_");
	BT_GAP_GetPairedInfo();
	return;
}

void fATBH (void *arg) {
	int argc = 0;
	char *argv[MAX_ARGC] = {0};
	tBT_AddressStru addr;
	
	if (!is_bt_ready())
		return;
	
	argc = parse_param(arg, argv);
	
	AT_PRINTK("[ATBH]: _AT_BT_DELETE_PAIRED_INFO_[%s,%s,%s]",argv[1],argv[2],argv[3]);
	
	if(strcmp(argv[1],"ALL") == 0) {
		BT_GAP_DelPairedInfo(NULL);
		return;
	}

	if (argc != 4)
		goto exit;

  	if(strcmp(argv[1], "BR") == 0){
		addr.cap = BT_ADDR_CAP_BREDR;
	}
	else if(strcmp(argv[1], "LE") == 0){
		addr.cap = BT_ADDR_CAP_LE;
	}
	else
		goto exit;
	
	if(strcmp(argv[2], "P") == 0)
		addr.type = BT_ADDR_TYPE_PUBLIC;
	else if(strcmp(argv[2],"R") == 0)
		addr.type = BT_ADDR_TYPE_RANDOM;
	else
		goto exit;
	
	if (strlen(argv[3]) != 2*BD_ADDR_LEN)
		goto exit;
	
	hex_str_to_bd_addr(strlen(argv[3]), argv[3], addr.bd);
	BT_GAP_DelPairedInfo(&addr);
	return;
exit:	
	AT_PRINTK("[ATBH] Usage: ATBH=BR|LE,P/R,BLE_BD_ADDR or ATBH=ALL");
	AT_PRINTK("[ATBH] addr_type: P=public, R=random\n\r");
}

static void at_gatt_ev_cb(tBT_GattEventEnum ev, void *param)
{
	switch (ev) {
		case BT_GATT_EV_READ_RSSI_CFM: {
				tBT_GattReadRssiInd* rssiStru = (tBT_GattReadRssiInd *)param;
				AT_PRINTK("[ATBI] RSSI = %d\n\r",rssiStru->rssi);
			}
			break;
		default:
			break;
	}
}

void fATBI (void *arg) {	
	int argc = 0;
	char *argv[MAX_ARGC] = {0};
	
	if (!is_bt_ready())
		return;
	
	argc = parse_param(arg, argv);
	
	AT_PRINTK("[ATBI]: _AT_BT_GET_RSSI_[%s]",argv[1]);
	
	if (argc != 2) {
		AT_PRINTK("[ATBI] Usage: ATBI=GATT/SPP\n\r");
		return;
	}
	
	if (strcmp(argv[1], "GATT") == 0) {
		if (bt_peer_conn_if != NULL) {
			BT_GATT_CallbackUnregister(at_gatt_ev_cb);
			BT_GATT_CallbackRegister(at_gatt_ev_cb);
			BT_GATT_RssiReq(bt_peer_conn_if);
		}
		else
			AT_PRINTK("[ATBI]: bt_peer_conn_if is null\n\r");
	}
	else if (strcmp(argv[1], "SPP") == 0) {
			AT_PRINTK("[ATBI]: Todo...\n\r");
	}
	else {
		AT_PRINTK("[ATBI] Usage: ATBI=GATT/SPP\n\r");
		return;
	}
}

void fATBK (void *arg) {
	if (!is_bt_ready())
		return;
	
	if(!arg){
		AT_PRINTK("Reject paring request\n\r");
		BT_GAP_PasskeyEntryReply(&bt_paring_dev, 0, FALSE);
		
		AT_PRINTK("[ATBK] Usage: ATBK=passkey\n\r");
	}
	else {
		AT_PRINTK("[ATBK]: _AT_BT_PASSKEY_ [%s]\n\r", (char*)arg);
		tUint32 passkey = atoi(arg);
		BT_GAP_PasskeyEntryReply(&bt_paring_dev, passkey, TRUE);
	}
}

void fATBM (void *arg) {
	if (!is_bt_ready())
		return;
	
	if(!arg){
		AT_PRINTK("[ATBM] Usage: ATBM=mtu\n\r");
	}
	else {
		AT_PRINTK("[ATBM]: _AT_BT_DEFAULT_MTU_ [%s]\n\r", (char*)arg);
		tUint16 mtu = atoi((const char *)(arg));
		if (mtu < 23) {
			mtu = 23;
		}
		BT_GATT_DefaultMtuSet(mtu);
		AT_PRINTK("GATT default mtu = %d\r\n",mtu);
	}
}

void br_device_name_cb(tBT_ResultEnum result, tUint8 *local_name)
{
	if (result == BT_RES_OK) {
		printf("[ATBN] BR/EDR Device Name: %s\n\r", local_name);
	}
	else {
		printf("[ATBN] [BT_GAP_BrDeviceNameGet] Error, error code (tBT_ResultEnum): 0x%x\n\r", result);
	}
}

void fATBN (void *arg) {
	if (!is_bt_ready())
		return;
	
	AT_PRINTK("[ATBN]: _AT_BT_BR_EDR_DEVICE_NAME_\n\r");
	BT_GAP_BrDeviceNameGet(br_device_name_cb);
}

static void bt_on_cb (tBT_ResultEnum result)
{	
	if (result == BT_RES_OK) {
		BT_GAP_CallbackReg(at_gap_ev_cb);
		AT_PRINTK("\n\rBT initialized\n\r");
	}
	else {
		AT_PRINTK("\n\rERROR: Init BT Failed! (%x)\n\r",result);
	}
}

static void bt_off_cb (tBT_ResultEnum result)
{
	if (result == BT_RES_OK) {
		AT_PRINTK("\n\rBT deinitialized\n\r");
	}
	else {
		AT_PRINTK("\n\rERROR: Deinit BT Failed! (%x)\n\r",result);
	}
}

void fATBP (void *arg) {
	unsigned int param = atoi((const char *)(arg));
	AT_PRINTK("[ATBP]: _AT_BT_POWER_[%s]\n\r", param?"ON":"OFF");
	if (param == 1) {
		BT_Init(bt_on_cb);
	}
	else if (param == 0) {
		BT_Done(bt_off_cb, 1);		
	}
	else
		AT_PRINTK("[ATBP] Usage: ATBP=0/1\n\r");
}

void fATBp (void *arg) {
	if (!is_bt_ready())
		return;
	
	AT_PRINTK("[ATBp]: _AT_BT_GATT_PEER_ADDR_\n\r");
	tBT_AddressStru bt_peer_addr;
	if (BT_GATT_RmtAddrGet(bt_peer_conn_if,&bt_peer_addr) == TRUE) {
		AT_PRINTK ("BT peer_addr_type = %s, peer_addr = "BD_ADDR_FMT"\n\r", (bt_peer_addr.type == BT_ADDR_TYPE_PUBLIC)? "public":"random",BD_ADDR_ARG(bt_peer_addr.bd));
	}
	else {
		AT_PRINTK ("\n\rERROR: No bt peer device\n\r");		
	}
}

void fATBS (void *arg) {
	int argc = 0;
	char *argv[MAX_ARGC] = {0};

	if (!is_bt_ready())
		return;
	
	argc = parse_param(arg, argv);

	int argv_count = 1;
	tBT_InquiryReqStru req;
	req.enable_ble_duplicate_filtering = 1;
	req.enable_ble_scan_req = 1;
	req.length = 0x0A; // Range: 0x01 - 0x30, Time = N * 1.28 sec, Time Range: 1.28 - 61.44 Sec
	req.num_rsp = 0;
	req.le_scan_interval = 0x0048; // Range: 0x0004 to 0x4000, Time = N * 0.625 msec, Time Range: 2.5 msec to 10.24 Sec
	req.le_scan_window = 0x0030; // // Range: 0x0004 to 0x4000, Time = N * 0.625 msec, Time Range: 2.5 msec to 10.24 Sec
	tBool enable = 1;
	
	if (argc < 2) {
		goto error_exit;
	}
	
	while(argv_count<argc){
		//first operation
		if(argv_count == 1) {
			if(strcmp(argv[argv_count], "BR") == 0){
				req.dev_type = BT_DEVICE_TYPE_BREDR;
			}
			else if(strcmp(argv[argv_count], "LE") == 0){
				req.dev_type = BT_DEVICE_TYPE_LE;
			}
			else if(strcmp(argv[argv_count], "BOTH") == 0){
				req.dev_type = BT_DEVICE_TYPE_BOTH_BREDR_LE;
			}
			else if (strcmp(argv[argv_count], "0") == 0){
				enable = 0;
			}
			else
				goto error_exit;
			
			argv_count++;
		}
		else {
			if (argc < argv_count+2)
				goto error_exit;
			
			if (strcmp(argv[argv_count], "-f") == 0) {
				req.enable_ble_duplicate_filtering = (atoi(argv[argv_count+1]))? 1:0;
			}
			else if (strcmp(argv[argv_count], "-r") == 0) {
				req.enable_ble_scan_req = (atoi(argv[argv_count+1]))? 1:0;
			}
			else if (strcmp(argv[argv_count], "-i") == 0) {
				req.le_scan_interval = atoi(argv[argv_count+1]);
			}
			else if (strcmp(argv[argv_count], "-w") == 0) {
				req.le_scan_window = atoi(argv[argv_count+1]);
			}
			else if (strcmp(argv[argv_count], "-t") == 0) {
				req.length = atoi(argv[argv_count+1]);
			}
			else if (strcmp(argv[argv_count], "-n") == 0) {
				req.num_rsp = atoi(argv[argv_count+1]);
			}
			argv_count+=2;
		}
	}
	
	if (req.length > 0x30)
		req.length = 0x30;
	if ((req.le_scan_interval < 0x0004) || (req.le_scan_interval > 0x4000))
		req.le_scan_interval = 0x0048;
	if ((req.le_scan_window < 0x0004) || (req.le_scan_interval > 0x4000))
		req.le_scan_window = 0x0030;
	if (req.le_scan_window > req.le_scan_interval)
		req.le_scan_window = req.le_scan_interval;
	
	if (enable) {
		AT_PRINTK("[ATBS]: _AT_BT_SCAN_ [%s, Inquiry_length: 0x%02X (%.2f secs)]",(req.dev_type==BT_DEVICE_TYPE_BREDR? "BR/EDR":
												req.dev_type==BT_DEVICE_TYPE_LE? "LE":
												req.dev_type==BT_DEVICE_TYPE_BOTH_BREDR_LE? "BOTH":""),
												req.length, (float)req.length*1.28);
		if (req.dev_type & BT_DEVICE_TYPE_LE)
			AT_PRINTK("        [LE] ble_duplicate_filtering=%d, ble_scan_req=%d, le_scan_interval=0x%04X (%.3f msecs), le_scan_window=0x%04X (%.3f msecs)",
				req.enable_ble_duplicate_filtering, req.enable_ble_scan_req, req.le_scan_interval, (float)req.le_scan_interval*0.625, req.le_scan_window, (float)req.le_scan_window*0.625);
		if (req.dev_type & BT_DEVICE_TYPE_BREDR)
			AT_PRINTK("        [BR] Num_Responses=%d", req.num_rsp);
		
		INIT_LIST_HEAD(&br_req_list);
		btScanNum = 0;
		AT_PRINTK("BT Scanning...");
	}
	else
		AT_PRINTK("[ATBS]: _AT_BT_SCAN_ [CANCEL]\n\r");
	
	BT_GAP_Inquiry(&req, enable);
	return;
	
error_exit:
	AT_PRINTK("[ATBS] Usage: ATBS=BR|LE|BOTH|0,[options]");
	AT_PRINTK("     -f    #        [LE] enable ble duplicate filtering (default 1)");
	AT_PRINTK("     -r    #        [LE] enable ble scan request (default 1)");
	AT_PRINTK("     -i    #        [LE] le scan interval [0x0004-0x4000] (default 0x0048 <45 msec>)");
	AT_PRINTK("     -w    #        [LE] le scan window [0x0004-0x4000] (default 0x0030 <30 msec>)");
	AT_PRINTK("     -t    #        scan time: [BR][0x01-0x30] (default 0x0A <12.8 sec>)");
	AT_PRINTK("           #                   [LE][0x00-0x30], 0 means BLE continuous scan");
	AT_PRINTK("     -n    #        [BR] Maximum number of responses, 0 means unlimited (default 0)");
	AT_PRINTK("\n\r   Example:");
	AT_PRINTK("     ATBS=LE,-f,0,-r,1,-t,0");
	AT_PRINTK("     ATBS=BR,-n,10,-t,5");
	AT_PRINTK("     ATBS=BOTH,-f,0,-r,0,-n,10,-t,5");		
	
	return;
}

void fATBx (void *arg) {
	char state_str[16] = {0};
	tBT_StatusEnum status = BT_GetState();
	if (status == BT_ST_IDLE)
		strcpy(state_str,"OFF");
	else if (status == BT_ST_INIT)
		strcpy(state_str,"INITIALIZING");
	else if (status == BT_ST_READY)
		strcpy(state_str,"ON");
	else if (status == BT_ST_REMOVING)
		strcpy(state_str,"DEINITIALIZING");
	else if (status == BT_ST_DONE)
		strcpy(state_str,"RESERVED");

	AT_PRINTK("[ATB?]: _AT_BT_STATUS_[%s]\n\r", state_str);
}

void fATBY (void *arg) {
	if (!is_bt_ready())
		return;
	tBool cfm = atoi((const char *)(arg));
	AT_PRINTK("[ATBY]: _AT_BT_REPLY_GAP_USER_CONFIRM_[%s]\n\r", cfm?"ACCEPT":"REJECT");
	BT_GAP_UserConfirmReply(&bt_paring_dev, cfm);
}

#if CONFIG_EXAMPLE_BT_GATT_CLIENT || 1

extern tBT_GattTaskInfoStru gatt_client;

void fATBR (void *arg) {
	int argc = 0;
	char *argv[MAX_ARGC] = {0};	

	if (!is_bt_ready())
		return;
	
	argc = parse_param(arg, argv);
	AT_PRINTK("[ATBR]: _AT_BT_GATT_READ_[%s]",argv[1]);

	if (gatt_client.conn == NULL) {
		AT_PRINTK("[ATBR]: gatt_client.conn is null\n\r");
		return;
	}
	else if (argc!=2) {
		AT_PRINTK("[ATBR] Usage: ATBR=attr_handle\n\r");
		return;
	}
	
	tUint16 attr_hdl = (tUint16) strtol(argv[1],NULL,16);
	gatt_client.context = NULL;
	BT_GATTC_ReadReq(&gatt_client, attr_hdl);
}

void fATBW (void *arg) {
	int argc = 0;
	char *argv[MAX_ARGC] = {0};
	tUint8 *byte_array = NULL;
	tUint8 array_len = 0;

	if (!is_bt_ready())
		return;
	
	argc = parse_param(arg, argv);
	AT_PRINTK("[ATBW]: _AT_BT_GATT_WRITE_[%s,%s,%s]",argv[1],argv[2],argv[3]);
	if (gatt_client.conn == NULL) {
		AT_PRINTK("[ATBW]: gatt_client.conn is null\n\r");
		return;
	}
	else if (argc!=4) {
		AT_PRINTK("[ATBW] Usage: ATBW=attr_handle,req/cmd,data\n\r");
		return;
	}
	
	if (strncmp(argv[3], "0x", strlen("0x")) == 0 || strncmp(argv[3], "0X", strlen("0X")) == 0 ) {
		if (strlen(argv[3]) %2 != 0) {
			AT_PRINTK("[ATBW] Invalid Byte Array\n\r");
			return;
		}
		array_len = (strlen(argv[3])-2)/2;
		byte_array = (tUint8*)malloc(array_len);
		hex_str_to_num_array(strlen(argv[3])-2, (&argv[3][2]), byte_array);
	}
	
	tUint16 attr_hdl = (tUint16) strtol(argv[1],NULL,16);
	gatt_client.context = NULL;
	
	if(strcmp(argv[2], "req") == 0) {
		if (byte_array == NULL)
			BT_GATTC_WriteReq(&gatt_client, attr_hdl,strlen(argv[3]),argv[3]);
		else
			BT_GATTC_WriteReq(&gatt_client, attr_hdl,array_len,byte_array);
	}
	else if(strcmp(argv[2], "cmd") == 0) {
		if (byte_array == NULL)
			BT_GATTC_WriteCmd(&gatt_client, attr_hdl,strlen(argv[3]),argv[3]);
		else
			BT_GATTC_WriteCmd(&gatt_client, attr_hdl,array_len,byte_array);
	}
	else {
		AT_PRINTK("[ATBW] Usage: ATBW=attr_handle,req/cmd,data\n\r");
	}
	
	if(byte_array != NULL)
		free(byte_array);
}

#endif

#if CONFIG_EXAMPLE_BT_SPP

extern void ex_spp_cb(tBT_SppEventEnum ev, void *param);
void fATBE (void *arg) {
	int argc = 0;
	char *argv[MAX_ARGC] = {0};
	tUint16 mtu;

	if (!is_bt_ready())
		return;
	
	argc = parse_param(arg, argv);

	if ( (argc != 2) && (argc != 3))
		goto exit;

	if (strlen(argv[1]) != 2*BD_ADDR_LEN)
		goto exit;

	AT_PRINTK("[ATBE]: _AT_BT_SPP_CONNECT_[%s,%s]",argv[1],argv[2]);
	if (argc == 3) {
		mtu = atoi(argv[2]);			
		if (mtu<=23)
			mtu = 23; 
	}
	else
		mtu = 490;
	
	tBT_SppConnReq req;
	req.init_credit = 7;
	req.mfs = mtu;
	hex_str_to_bd_addr(strlen(argv[1]), argv[1], req.bd);	
	spp_client_conn = BT_SPP_Connect(&req, ex_spp_cb);
	return;
exit:
	AT_PRINTK("[ATBE] Usage: ATBE=BR_BD_ADDR,[mtu]\n\r");
	AT_PRINTK("[ATBE] default mtu: 490\n\r");	
}

void fATBF (void *arg) {
	int argc = 0;
	char *argv[MAX_ARGC] = {0};
	tUint8 *byte_array = NULL;
	tUint8 array_len = 0;

	if (!is_bt_ready())
		return;
	
	argc = parse_param(arg, argv);

	if (spp_conn_if == NULL) {
		AT_PRINTK("[ATBF]: spp_conn_if is null\n\r");
		return;
	}
	else if (argc==1) {
		AT_PRINTK("[ATBF] Usage: ATBF=data\n\r");
		return;
	}

	if (strncmp(argv[1], "0x", strlen("0x")) == 0 || strncmp(argv[1], "0X", strlen("0X")) == 0 ) {
		if (strlen(argv[1]) %2 != 0) {
			AT_PRINTK("[ATBF] Invalid Byte Array\n\r");
			return;
		}
		array_len = (strlen(argv[1])-2)/2;
		byte_array = (tUint8*)malloc(array_len);
		hex_str_to_num_array(strlen(argv[1])-2, (&argv[1][2]), byte_array);
	}
	
	AT_PRINTK("[ATBF]: _AT_BT_SPP_SEND_[%s]",argv[1]);

	if (byte_array == NULL)
		BT_SPP_SendData(spp_conn_if, argv[1],strlen(argv[1]));
	else
		BT_SPP_SendData(spp_conn_if, byte_array,array_len);

	if(byte_array != NULL)
		free(byte_array);
}

#endif

void fATBt (void *arg) {
	AT_PRINTK("[ATB#]: _AT_BT_TEST_");
}

log_item_t at_bt_items[ ] = {
	{"ATB?", fATBx,}, // BT Status
	{"ATBP", fATBP,}, // BT power on/ off
#if CONFIG_AUDREY_DEV_CMD
	{"ATBA", fATBA,}, // BT device address
	{"ATBB", fATBB,}, // BT ADV on/off
	{"ATBC", fATBC,}, // Create a GATT connection	
	{"ATBD", fATBD,}, // Disconnect BT Connection
	{"ATBG", fATBG,}, // Get Paired Info
	{"ATBH", fATBH,}, // Delete Paired Info
	{"ATBI", fATBI,}, // Get RSSI value of connected device	
	{"ATBK", fATBK,}, // Reply GAP passkey
	{"ATBM", fATBM,}, // Set default GATT mtu
	{"ATBN", fATBN,}, // Get BT device name (BR/EDR)
	{"ATBp", fATBp,}, // BT peer address
	{"ATBS", fATBS,}, // Scan BT
	{"ATBY", fATBY,}, // Reply GAP user confrim
	{"ATB#", fATBt,},// test command

#if CONFIG_EXAMPLE_BT_GATT_CLIENT
	{"ATBR", fATBR,}, // gatt client read	
	{"ATBW", fATBW,}, // gatt client write		
#endif

#if CONFIG_EXAMPLE_BT_SPP
	{"ATBE", fATBE,}, // Example_bt_spp connect
	{"ATBF", fATBF,}, // SPP Send
#endif
#endif /* CONFIG_AUDREY_DEV_CMD */

};

void at_bt_init(void)
{
	log_service_add_table(at_bt_items, sizeof(at_bt_items)/sizeof(at_bt_items[0]));
}

#if SUPPORT_LOG_SERVICE
log_module_init(at_bt_init);
#endif
