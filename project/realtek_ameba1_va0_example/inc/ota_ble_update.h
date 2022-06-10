



#ifndef OTA_BLE_UPDATE_H
#define OTA_BLE_UPDATE_H

#include <FreeRTOS.h>
#include <task.h>
#include <platform_stdlib.h>
#include <flash_api.h>
#include <lwip/sockets.h>

#ifndef OTA_8195A_H
/************************Related setting****************************/
//#define HTTP_OTA_UPDATE			//if define, using http protocol, if not, will use socket
#define CONFIG_CUSTOM_SIGNATURE		0	//if verify the custom signature(define in ota_8195a.c cus_sig)
#define WRITE_OTA_ADDR				1
#if CONFIG_AUDREY
#define SWAP_UPDATE					1
#else
#define SWAP_UPDATE					0
#endif /* CONFIG_AUDREY */

#define BUF_SIZE		1024
#define HEADER_BAK_LEN	32

#define OFFSET_DATA		FLASH_SYSTEM_DATA_ADDR
#define IMAGE_2			0x0000B000
#if WRITE_OTA_ADDR
#define BACKUP_SECTOR	(FLASH_SYSTEM_DATA_ADDR - 0x1000)
#endif
/*******************************************************************/

#define OTA_BLE_WIRELESS_RESOURCE	"type=wireless&ver="
#define OTA_BLE_SCALE_RESOURCE		"type=scale&ver="

#if 0
/****************Define the structures used*************************/
typedef struct {
	uint32_t	status_code;
	uint32_t	header_len;
	uint8_t		*body;
	uint32_t	body_len;
	uint8_t		*header_bak;
	uint32_t	parse_status;
} http_response_result_t;

typedef union {
	uint32_t u;
	unsigned char c[4];
} _file_checksum;
/*******************************************************************/
#endif

/****************General functions used by ota update***************/
//void *update_malloc(unsigned int size);
//void update_free(void *buf);
//void ota_platform_reset(void);
//#if WRITE_OTA_ADDR
//int write_ota_addr_to_system_data(flash_t *flash, uint32_t ota_addr);
//#endif
//int update_ota_connect_server(int server_socket, update_cfg_local_t *cfg);
//uint32_t update_ota_prepare_addr(void);
//#if SWAP_UPDATE
//uint32_t update_ota_swap_addr(uint32_t img_len, uint32_t NewImg2Addr);
//#endif
//int update_ota_erase_upg_region(uint32_t img_len, uint32_t NewImg2Len, uint32_t NewImg2Addr);
//int update_ota_checksum(_file_checksum *file_checksum, uint32_t flash_checksum, uint32_t NewImg2Addr);
/*******************************************************************/


/*******************Functions called by AT CMD**********************/
//void cmd_update(int argc, char **argv);
void cmd_ota_image(bool cmd);

#endif // OTA_8195A_H


/**
 * BLE経由 FW update を開始する
 * scale側  wireless側　バージョンが違うもののみupdate実行する
 * バージョンは先に ota_set_new_version() で設定する
 */
void start_ota_ble_update(void);

/**
 * BLE経由 FW update を中止する
 */
void stop_ota_ble_update(void);


/**
 * ATコマンド処理 ATAG
 * @param in argv[0] update
 * @param in argv[1] OTA
 * @param in argv[2] ALL/AM/AMU/SC/SCU/STOP
 * @param in argv[3] IP or server_name
 * @param in argv[4] port  ... 80/443
 * @param in argv[5] resource
 */
void cmd_ota_ble_update(int argc, char **argv);


#endif // OTA_BLE_UPDATE_H



