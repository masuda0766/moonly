/*
 * ota_http_client.h
 * copy from ota_8195a.h
 */
#ifndef OTA_HTTP_CLIENT_H
#define OTA_HTTP_CLIENT_H

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


/****************Define the structures used*************************/
typedef struct{
	uint32_t	ip_addr;
	uint16_t	port;
}update_cfg_local_t;

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


/****************General functions used by ota update***************/
//void *update_malloc(unsigned int size);
//void update_free(void *buf);
void ota_platform_reset(void);
#if WRITE_OTA_ADDR
int write_ota_addr_to_system_data(flash_t *flash, uint32_t ota_addr);
#endif
int update_ota_connect_server(int server_socket, update_cfg_local_t *cfg);
uint32_t update_ota_prepare_addr(void);
#if SWAP_UPDATE
uint32_t update_ota_swap_addr(uint32_t img_len, uint32_t NewImg2Addr);
#endif
int update_ota_erase_upg_region(uint32_t img_len, uint32_t NewImg2Len, uint32_t NewImg2Addr);
int update_ota_checksum(_file_checksum *file_checksum, uint32_t flash_checksum, uint32_t NewImg2Addr);
/*******************************************************************/


/*******************Functions called by AT CMD**********************/
//void cmd_update(int argc, char **argv);
void cmd_ota_image(bool cmd);

#endif // OTA_8195A_H


#if CONFIG_OTA_HTTP_UPDATE


#if CONFIG_SSL_HTTPS_CLIENT
#define OTA_HTTP_PORT	443
#define SSL_CLIENT_EXT
#else
//#define OTA_HTTP_PORT	80
#define OTA_HTTP_PORT	8080
#endif





	// old server
//#define OTA_HTTP_WIRELESS_RESOURCE	"dev/ota/img/wireless/"
//#define OTA_HTTP_SCALE_RESOURCE		"dev/ota/img/scale/"
//#define OTA_HTTP_FILE_TYPE			".bin"

#if 0
	// test http server
#define OTA_HTTP_HOST	"192.168.0.60"
#define OTA_HTTP_REQ_HOST "Host: 192.168.0.60"

#define OTA_HTTP_WIRELESS_RESOURCE	"/dev/ota/getfirmware?type=wireless&ver="
#define OTA_HTTP_SCALE_RESOURCE		"/dev/ota/getfirmware?type=scale&ver="
#define OTA_HTTP_FILE_TYPE			""
#define OTA_HTTP_UPLOAD_RESOURCE	"/dev/api/bio_data/upload"
#define OTA_HTTP_VER_RESOURCE		"/dev/api/bio_data/upload"

#define JSON_DATA_ID		"data"
#define JSON_VERSION_ID		"fw_version"
#define JSON_PET_DATA_ID	"pet_info"

#define OTA_HTTP_REQ_X_API_KEY "X-api-key: "

#else

#define OTA_HTTP_HOST	AUDREY_SERVER_NAME
#define OTA_HTTP_REQ_HOST "Host: "AUDREY_SERVER_NAME
#define OTA_HTTP_WIRELESS_RESOURCE	AUDREY_SERVER_TYPE"/getfirmware?type=wireless&ver="
#define OTA_HTTP_SCALE_RESOURCE		AUDREY_SERVER_TYPE"/getfirmware?type=scale&ver="
#define OTA_HTTP_FILE_TYPE			""
#define OTA_HTTP_UPLOAD_RESOURCE	AUDREY_SERVER_TYPE"/monitor_data"
#define OTA_HTTP_VER_RESOURCE		AUDREY_SERVER_TYPE"/monitor_data"

#define JSON_DATA_ID		"data"
#define JSON_VERSION_ID		"ver"
#define JSON_PET_DATA_ID	"pet"

#define OTA_HTTP_REQ_X_API_KEY "X-api-key: "

#endif


#define CONNECT_RETRY_MAX (3)


/*******************Functions called by AT CMD**********************/

void cmd_http_update(int argc, char **argv);

void start_http_update(void);

void stop_http_update(void);

//[TODO] test only
int do_get_ota_version(char *out_scale_version, char *out_am_version);
/*******************************************************************/

int ota_set_new_version(char *in_scale_version, char *in_am_version);

void ota_clear_new_version(void);

#endif // #if CONFIG_OTA_HTTP_UPDATE

#endif
