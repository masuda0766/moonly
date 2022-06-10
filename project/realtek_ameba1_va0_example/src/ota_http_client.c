/*
 * ota_http_client.c
 * copy from ota_8195a.c
 */

#include "platform_opts.h"

#include <sys.h>
#include <device_lock.h>
#include "lwip/netdb.h"

#define USE_DNS_GETHOSTBYNAME
#ifdef  USE_DNS_GETHOSTBYNAME
#include "lwip/dns.h"
#endif

#if CONFIG_SSL_HTTPS_CLIENT
//#include <device_lock.h>
#include <flash_api.h>

#if CONFIG_USE_MBEDTLS

#include "mbedtls/config.h"
#include "mbedtls/platform.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "mbedtls/error.h"
#include "mbedtls/debug.h"

#endif // CONFIG_USE_MBEDTLS


#define TCP_READ_TIMEOVER

#endif // CONFIG_SSL_HTTPS_CLIENT

#include "wifi_conf.h"

#include "ota_http_client.h"
#include "ssl_my_func.h"
#include "wdt.h"

#define OTA_SCALE_BIN_MAX_SIZE (64*1024)

//#define CHANGE_IMG_0008
#define SIGNATURE_ERASE_BEFOR_WRITE
#define SIGNATURE_SET_BEFOR_WRITE
//#define SWAP_UPDATE_BOOT_PART2_FIRST
#define SIGNATURE_WRITE_DIRECT
#define SIGNATURE_WRITE_DIRECT_CMD_OTA_IMAGE

#define	 OTA_UPDATE_STOP_AND_CONTINUE (-8888)
#define	 OTA_UPDATE_CONNECT_AGEIN (-9999)

#if CONFIG_OTA_HTTP_UPDATE

sys_thread_t TaskOTA = NULL;
#define STACK_SIZE		1024
#define TASK_PRIORITY	tskIDLE_PRIORITY + 1
#if SWAP_UPDATE
static uint32_t OldImg2Addr;
#endif // CONFIG_OTA_HTTP_UPDATE

static flash_t flash_ota;
static int ota_update_task_run = 0;
int ota_update_task_stop = 0;
static int ota_update_scale_done = 0;
static int ota_update_ameba_done = 0;
static int ble_ota_update_task_stop = 0;

#if CONFIG_OTA_HTTP_UPDATE

#define G_VERSION_SIZE 34
SECTION(".sdram.data")
char g_version[G_VERSION_SIZE];
SECTION(".sdram.data")
char am_version[G_VERSION_SIZE / 2];
SECTION(".sdram.data")
char scale_version[G_VERSION_SIZE / 2];
SECTION(".sdram.data")

#define G_RESPONSE_DATA_SIZE 1024
SECTION(".sdram.data")
char g_response_data[G_RESPONSE_DATA_SIZE];

char server_resource[1024];

int set_new_version = 0;

#include "state_manager.h"

#include "version.h"
#include "scale.h"

#define OTA_HTTP_FLASH_WRITE_ENABLE // flash write

static int connect_agein_count = 0;

#if 1


int debug_print_lebel = 7;

#if 1

//[TODO] Log_Debug
#define DEBUG_PRINT_ENABLE
#ifdef DEBUG_PRINT_ENABLE
#define D(fmt, ...)  { if (debug_print_lebel >= 5) Log_Info("\r\n[FWU][DEBUG]%s::[%d] " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__); }
#define DD(fmt, ...)  { if (debug_print_lebel >= 5) Log_Info(fmt, ##__VA_ARGS__); }
#define D_BUF(data,len) {\
	for(int i=0; i<len; i++) {\
		DD("%02X ",*((unsigned char *)data+i));\
	}\
}

#else
#define D(fmt, ...)
#define DD(fmt, ...)
#define D_BUF(...)
#endif

#define INFO_PRINT_ENABLE
#ifdef INFO_PRINT_ENABLE
#define DI(fmt, ...)  if (debug_print_lebel >= 3) Log_Info("\r\n[FWU][INFO]%s::[%d] " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define DDI(fmt, ...)  if (debug_print_lebel >= 3) Log_Info(fmt, ##__VA_ARGS__)
#define DI_BUF(data,len) {\
	for(int i=0; i<len; i++) {\
		Log_Info("%02X ",*((unsigned char *)data+i));\
	}\
}

#else
#define DI(fmt, ...)
#define DDI(fmt, ...)
#define DI_BUF(...)
#endif

#define TRACE_PRINT_ENABLE
#ifdef TRACE_PRINT_ENABLE
#define DT(fmt, ...)  if (debug_print_lebel >= 6) Log_Info("\r\n[FWU][TRACE]%s::[%d] " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define DDT(fmt, ...)  if (debug_print_lebel >= 6) Log_Info(fmt, ##__VA_ARGS__)
#else
#define DT(fmt, ...)
#define DDT(fmt, ...)
#endif

#define ERROR_PRINT_ENABLE
#ifdef ERROR_PRINT_ENABLE
#define DE(fmt, ...)  Log_Error("\r\n[FWU][ERROR]%s::[%d] " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define DDE(fmt, ...)  Log_Error(fmt, ##__VA_ARGS__)
#else
#define DE(fmt, ...)
#define DDE(fmt, ...)
#endif // ERROR_PRINT_ENABLE



#else

#define DEBUG_PRINT_ENABLE
#ifdef DEBUG_PRINT_ENABLE
#define D(fmt, ...)  Log_Info("\r\n[FWU][DEBUG]%s::[%d] " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define DD(fmt, ...)  Log_Info(fmt, ##__VA_ARGS__)
#define D_BUF(data,len) {\
	for(int i=0; i<len; i++) {\
		DD("0x%02X ",*((unsigned char *)data+i));\
	}\
}
#else
#define D(fmt, ...)
#define DD(fmt, ...)
#define D_BUF(fmt, ...)
#endif

#define INFO_PRINT_ENABLE
#ifdef INFO_PRINT_ENABLE
#define DI(fmt, ...)  Log_Info("\r\n[FWU][INFO]%s::[%d] " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define DDI(fmt, ...)  Log_Info(fmt, ##__VA_ARGS__)
#else
#define DI(fmt, ...)
#define DDI(fmt, ...)
#endif

#define ERROR_PRINT_ENABLE
#ifdef ERROR_PRINT_ENABLE
#define DE(fmt, ...)  Log_Error("\r\n[FWU][ERROR]%s::[%d] " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define DDE(fmt, ...)  Log_Error(fmt, ##__VA_ARGS__)
#else
#define DE(fmt, ...)
#define DDE(fmt, ...)
#endif // ERROR_PRINT_ENABLE


#endif


#else
//#define DEBUG_PRINT_ENABLE
#ifdef DEBUG_PRINT_ENABLE
#define D(fmt, ...)  Log_Info("\r\n[FWU][DEBUG]%s::[%d] " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define DD(fmt, ...)  Log_Info(fmt, ##__VA_ARGS__)
#else
#define D(fmt, ...)
#define DD(fmt, ...)
#endif

#define INFO_PRINT_ENABLE
#ifdef INFO_PRINT_ENABLE
#define DI(fmt, ...)  Log_Info("\r\n[FWU][INFO]%s::[%d] " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define DDI(fmt, ...)  Log_Info(fmt, ##__VA_ARGS__)
#else
#define DI(fmt, ...)
#define DDI(fmt, ...)
#endif

#define ERROR_PRINT_ENABLE
#ifdef ERROR_PRINT_ENABLE
#define DE(fmt, ...)  Log_Error("\r\n[FWU][ERROR]%s::[%d] " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define DDE(fmt, ...)  Log_Error(fmt, ##__VA_ARGS__)
#else
#define DE(fmt, ...)
#define DDE(fmt, ...)
#endif // ERROR_PRINT_ENABLE

#endif

//#define DEBUG_ONLY_ENABLE /* TEST ONLY */
// defined DEBUG_ONLY_ENABLE
// then ATAU command  TEST/DISPV available.
// ATAU=TEST  Acquire from server in POST mode and display version.
//            [CAUTION] dummy POST data upload
// ATAU=DISPV display version

//#define WRITE_FLASH9000_AFTER_FILE_READ

#define BUF_USE_SDRAM
#ifdef  BUF_USE_SDRAM
// sd_ram
#define BUF_SIZE 1024
unsigned char buf[BUF_SIZE];
unsigned char alloc[8];
unsigned char request[1024];
#else
// malloc size
#define BUF_SIZE 512
#endif


#if CONFIG_SSL_HTTPS_CLIENT

#define DEBUG_LEVEL 1

#ifdef MBEDTLS_ERROR_C
char error_buf[100];
#endif

#endif // CONFIG_SSL_HTTPS_CLIENT


void update_ota_http(char *cmd, char *ip, uint32_t port, char *resource);

struct http_task_param {
	char cmd[8];
	uint32_t port;
	char host[64];
	char resource[1024];
	char *post;
	char *out;
};

#define TASK_PARAM_USE_SH_HOST
#ifdef  TASK_PARAM_USE_SH_HOST
//SECTION(".sdram.data")
struct http_task_param sh_host;
#endif // TASK_PARAM_USE_SH_HOST

//SECTION(".sdram.data")
unsigned char buf_sdram[OTA_SCALE_BIN_MAX_SIZE]; // scale FW work area

#endif // #if CONFIG_OTA_HTTP_UPDATE

#if CONFIG_CUSTOM_SIGNATURE
/* ---------------------------------------------------
  *  Customized Signature
  * ---------------------------------------------------*/
// This signature can be used to verify the correctness of the image
// It will be located in fixed location in application image
#include "section_config.h"
SECTION(".custom.validate.rodata")
const unsigned char cus_sig_demo[32] = "Customer Signature-modelxxx";
#endif

#if CONFIG_SSL_HTTPS_CLIENT
static void* update_malloc(unsigned int size){
	//return pvPortMalloc(size);
	return update_my_malloc(size);
}

static void update_free(void *buf){
	//vPortFree(buf);
	update_my_free(buf);
}
#else
void* update_malloc(unsigned int size){
	return pvPortMalloc(size);
}

void update_free(void *buf){
	vPortFree(buf);
}
#endif

#if 0
void ota_platform_reset(void){
	// Set processor clock to default before system reset
	HAL_WRITE32(SYSTEM_CTRL_BASE, 0x14, 0x00000021);
	osDelay(100);

	// Cortex-M3 SCB->AIRCR
	HAL_WRITE32(0xE000ED00, 0x0C, (0x5FA << 16) |                             // VECTKEY
	                              (HAL_READ32(0xE000ED00, 0x0C) & (7 << 8)) | // PRIGROUP
	                              (1 << 2));                                  // SYSRESETREQ
	while(1) osDelay(1000);
}
#endif

#if WRITE_OTA_ADDR
int write_ota_addr_to_system_data(flash_t *flash, uint32_t ota_addr)
{
	uint32_t data, i = 0;
	//Get upgraded image 2 addr from offset
	device_mutex_lock(RT_DEV_LOCK_FLASH);
	flash_read_word(flash, OFFSET_DATA, &data);
	DI("\n\r[%s] data 0x%x ota_addr 0x%x", __FUNCTION__, data, ota_addr);
	if(~0x0 == data){
		DI("set OTA address(0x%x) to 0x%x", ota_addr, OFFSET_DATA);
		flash_write_word(flash, OFFSET_DATA, ota_addr);
	}
	else{
		DI("copy system_data start");
		//erase backup sector
		flash_erase_sector(flash, BACKUP_SECTOR);
		//backup system data to backup sector
		for(i = 0; i < 0x1000; i+= 4){
			flash_read_word(flash, OFFSET_DATA + i, &data);
			if(0 == i) {
				DI("set OFFSET = 0x%x", ota_addr);
				data = ota_addr;
			}
			flash_write_word(flash, BACKUP_SECTOR + i,data);
		}
		//erase system data
		flash_erase_sector(flash, OFFSET_DATA);
		//write data back to system data
		for(i = 0; i < 0x1000; i+= 4){
			flash_read_word(flash, BACKUP_SECTOR + i, &data);
			flash_write_word(flash, OFFSET_DATA + i,data);
		}
		//erase backup sector
		flash_erase_sector(flash, BACKUP_SECTOR);
		DI("copy system_data end");
	}
	device_mutex_unlock(RT_DEV_LOCK_FLASH);
	return 0;
}
#endif

#if CONFIG_OTA_UPDATE

int update_ota_connect_server(int server_fd, update_cfg_local_t *cfg){
	struct sockaddr_in server_addr;

	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if(server_fd < 0){
		Log_Error("\n\r[%s] Create socket failed", __FUNCTION__);
		return -1;
	}

	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = cfg->ip_addr;
	server_addr.sin_port = cfg->port;

	if(connect(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1){
		Log_Error("\n\r[%s] Socket connect failed", __FUNCTION__);
		return -1;
	}

	return server_fd;
}
#endif // CONFIG_OTA_UPDATE

uint32_t update_ota_prepare_addr(void){

	uint32_t Img2Len = 0;
	uint32_t IMAGE_x = 0, ImgxLen = 0, ImgxAddr = 0;
	uint32_t NewImg2Addr = 0;
#if WRITE_OTA_ADDR
#if CONFIG_AUDREY
	uint32_t ota_addr = 0x100000;
#else
	uint32_t ota_addr = 0x80000;
#endif /* CONFIG_AUDREY */
#endif

	DBG_INFO_MSG_OFF(_DBG_SPI_FLASH_);
	// The upgraded image2 pointer must 4K aligned and should not overlap with Default Image2
	device_mutex_lock(RT_DEV_LOCK_FLASH);
	flash_read_word(&flash_ota, IMAGE_2, &Img2Len);
	IMAGE_x = IMAGE_2 + Img2Len + 0x10;
	flash_read_word(&flash_ota, IMAGE_x, &ImgxLen);
	flash_read_word(&flash_ota, IMAGE_x+4, &ImgxAddr);
	device_mutex_unlock(RT_DEV_LOCK_FLASH);

	if(0x30000000 == ImgxAddr){
		DI("\n\r[%s] IMAGE_3 0x%x Img3Len 0x%x", __FUNCTION__, IMAGE_x, ImgxLen);
	}
	else{
		DI("\n\r[%s] There is no IMAGE_3", __FUNCTION__);
		IMAGE_x = IMAGE_2;
		ImgxLen = Img2Len;
	}
#if WRITE_OTA_ADDR
	if((ota_addr > IMAGE_x) && ((ota_addr < (IMAGE_x+ImgxLen))) || (ota_addr < IMAGE_x) ||
		((ota_addr & 0xfff) != 0) || (ota_addr == ~0x0)){
		Log_Error("\n\r[%s] illegal ota addr 0x%x", __FUNCTION__, ota_addr);
		SendMessageToStateManager(MSG_ERR_FWUPDATE, PARAM_FW_UPDATE_IMG_BIN_LENGTH_OVER);
		return -1;
	}
	else {
		write_ota_addr_to_system_data(&flash_ota, ota_addr);
	}
#endif

	//Get upgraded image 2 addr from offset
	device_mutex_lock(RT_DEV_LOCK_FLASH);
	flash_read_word(&flash_ota, OFFSET_DATA, &NewImg2Addr);
	device_mutex_unlock(RT_DEV_LOCK_FLASH);
	if((NewImg2Addr > IMAGE_x) && ((NewImg2Addr < (IMAGE_x+ImgxLen))) || (NewImg2Addr < IMAGE_x) ||
		((NewImg2Addr & 0xfff) != 0) || (NewImg2Addr == ~0x0)){
		Log_Error("\n\r[%s] Invalid OTA Address 0x%x", __FUNCTION__, NewImg2Addr);
		SendMessageToStateManager(MSG_ERR_FWUPDATE, PARAM_FW_UPDATE_INVALID_OTA_ADDRESS_ERROR);
		return -1;
	}

	return NewImg2Addr;
}

#if SWAP_UPDATE
uint32_t update_ota_swap_addr(uint32_t img_len, uint32_t NewImg2Addr){
	uint32_t SigImage0,SigImage1;
	uint32_t Part1Addr=0xFFFFFFFF, Part2Addr=0xFFFFFFFF;
	device_mutex_lock(RT_DEV_LOCK_FLASH);
	flash_read_word(&flash_ota, 0x18, &Part1Addr);
	Part1Addr = (Part1Addr&0xFFFF)*1024;	//PART1 : 0x0000B000
	Part2Addr = NewImg2Addr;				//PART2 : 0x00080000

	// read Part1 signature
	flash_read_word(&flash_ota, Part1Addr+8, &SigImage0);
	flash_read_word(&flash_ota, Part1Addr+12, &SigImage1);
	DI(" Part1(0x%x) Sig %x", Part1Addr, SigImage0);
	device_mutex_unlock(RT_DEV_LOCK_FLASH);
	if(SigImage0==0x35393138 && SigImage1==0x31313738) {//Part1 is the new one with signature "81958711"
		OldImg2Addr = Part1Addr;	//Change Part1 to older version
#ifdef SWAP_UPDATE_BOOT_PART2_FIRST
		// read Part2 signature
		device_mutex_lock(RT_DEV_LOCK_FLASH);
		flash_read_word(&flash_ota, Part2Addr+8, &SigImage0);
		flash_read_word(&flash_ota, Part2Addr+12, &SigImage1);
		device_mutex_unlock(RT_DEV_LOCK_FLASH);
		DI(" Part2(0x%x) Sig %x", Part2Addr, SigImage0);
		if(SigImage0==0x35393138 && SigImage1==0x31313738){//Part2 is the new one with signature "81958711"
			DI(" SWAP_UPDATE_BOOT_PART2_FIRST ");
			OldImg2Addr = Part2Addr;	//Change Part2 to older version
			NewImg2Addr = Part1Addr;
			if( img_len > (Part2Addr-Part1Addr) ){	// firmware size too large
				Log_Error("\n\r[%s] Part1 size < OTA size", __FUNCTION__);
				SendMessageToStateManager(MSG_ERR_FWUPDATE, PARAM_FW_UPDATE_IMG_BIN_LENGTH_OVER);
				return -1;
			}
		}
#endif
	}
	else {
		// read Part2 signature
		device_mutex_lock(RT_DEV_LOCK_FLASH);
		flash_read_word(&flash_ota, Part2Addr+8, &SigImage0);
		flash_read_word(&flash_ota, Part2Addr+12, &SigImage1);
		device_mutex_unlock(RT_DEV_LOCK_FLASH);
		DI(" Part2(0x%x) Sig %x", Part2Addr, SigImage0);
		if(SigImage0==0x30303030 && SigImage1==0x30303030){// ATSC signature "00000000"
			DI("Part2Addr signature 00000000");
			OldImg2Addr = Part1Addr;	//Store the new version to Part2
		}
		else if(SigImage0==0x35393138 && SigImage1==0x31313738){//Part2 is the new one with signature "81958711"
			OldImg2Addr = Part2Addr;	//Change Part2 to older version
			NewImg2Addr = Part1Addr;
			if( img_len > (Part2Addr-Part1Addr) ){	// firmware size too large
				Log_Error("\n\r[%s] Part1 size < OTA size", __FUNCTION__);
				SendMessageToStateManager(MSG_ERR_FWUPDATE, PARAM_FW_UPDATE_IMG_BIN_LENGTH_OVER);
				return -1;
			}
		}
		else {
			NewImg2Addr = Part2Addr;
		}
	}

	DI(" New %x, Old %x\r\n", NewImg2Addr, OldImg2Addr);
	return NewImg2Addr;
}
#endif

int update_ota_erase_upg_region(uint32_t img_len, uint32_t NewImg2Len, uint32_t NewImg2Addr){
	uint32_t NewImg2BlkSize = 0;

	if(NewImg2Len == 0){
		NewImg2Len = img_len;
		DI(" NewImg2Len %d  ", NewImg2Len);
		if((int)NewImg2Len > 0){
			NewImg2BlkSize = ((NewImg2Len - 1)/4096) + 1;
			DI(" NewImg2BlkSize %d  0x%8x", NewImg2BlkSize, NewImg2BlkSize);
			device_mutex_lock(RT_DEV_LOCK_FLASH);
			for(int i = 0; i < NewImg2BlkSize; i++) {
				flash_erase_sector(&flash_ota, NewImg2Addr + i * 4096);
			}
			device_mutex_unlock(RT_DEV_LOCK_FLASH);
		}else{
			Log_Error("\n\r[%s] Size INVALID", __FUNCTION__);
			SendMessageToStateManager(MSG_ERR_FWUPDATE, PARAM_FW_UPDATE_IMG_FILE_LENGTH_ERROR);
			return -1;
		}
	}

	DI(" NewImg2Addr 0x%x", NewImg2Addr);
	return NewImg2Len;
}

int update_ota_checksum(_file_checksum *file_checksum, uint32_t flash_checksum, uint32_t NewImg2Addr){

#if CONFIG_CUSTOM_SIGNATURE
	char custom_sig[32] = "Customer Signature-modelxxx";
	uint32_t read_custom_sig[8];

	device_mutex_lock(RT_DEV_LOCK_FLASH);
	for(int i = 0; i < 8; i ++){
		flash_read_word(&flash_ota, NewImg2Addr + 0x28 + i *4, read_custom_sig + i);
	}
	device_mutex_unlock(RT_DEV_LOCK_FLASH);
	DI("\n\r[%s] read_custom_sig %s", __FUNCTION__ , (char*)read_custom_sig);
#endif

	DI("\n\rflash (0x%x) checksum 0x%8x attached checksum 0x%8x", NewImg2Addr, flash_checksum, file_checksum->u);

	// compare checksum with received checksum
	if( (file_checksum->u == flash_checksum)
#if CONFIG_CUSTOM_SIGNATURE
		&& !strcmp((char*)read_custom_sig,custom_sig)
#endif
	){
		//Set signature in New Image 2 addr + 8 and + 12
		uint32_t sig_readback0,sig_readback1;
		device_mutex_lock(RT_DEV_LOCK_FLASH);
		flash_write_word(&flash_ota,NewImg2Addr + 8, 0x35393138);
		flash_write_word(&flash_ota,NewImg2Addr + 12, 0x31313738);
		flash_read_word(&flash_ota, NewImg2Addr + 8, &sig_readback0);
		flash_read_word(&flash_ota, NewImg2Addr + 12, &sig_readback1);
		device_mutex_unlock(RT_DEV_LOCK_FLASH);
		DI(" new(0x%x)signature readback %x,%x", NewImg2Addr , sig_readback0, sig_readback1);
		if (sig_readback0 != 0x35393138) {
			DE(" signature write error [0x%x]", sig_readback0);
			uint32_t sig_readback0,sig_readback1;
			device_mutex_lock(RT_DEV_LOCK_FLASH);
			flash_write_word(&flash_ota,NewImg2Addr + 8, 0x35393138);
			flash_write_word(&flash_ota,NewImg2Addr + 12, 0x31313738);
			flash_read_word(&flash_ota, NewImg2Addr + 8, &sig_readback0);
			flash_read_word(&flash_ota, NewImg2Addr + 12, &sig_readback1);
			device_mutex_unlock(RT_DEV_LOCK_FLASH);
			DI(" new(0x%x)signature readback %x,%x", NewImg2Addr , sig_readback0, sig_readback1);
		}
		if (sig_readback0 != 0x35393138) {
			DE(" signature write error");
			return -1;
		}
#if SWAP_UPDATE
		if(OldImg2Addr != ~0x0 && sig_readback0 != 0x35393138){
			device_mutex_lock(RT_DEV_LOCK_FLASH);
			flash_write_word(&flash_ota,OldImg2Addr + 8, 0x35393130);
			flash_write_word(&flash_ota,OldImg2Addr + 12, 0x31313738);
			flash_read_word(&flash_ota, OldImg2Addr + 8, &sig_readback0);
			flash_read_word(&flash_ota, OldImg2Addr + 12, &sig_readback1);
			device_mutex_unlock(RT_DEV_LOCK_FLASH);
			DI(" old(0x%x) signature %x,%x", OldImg2Addr, sig_readback0, sig_readback1);
		}
#endif

		DI("\n\r[%s] Update OTA success!", __FUNCTION__);
		return 0;
	}
	return -1;
}

#if CONFIG_OTA_UPDATE
// tcp/ip direct read
static void update_ota_local_task(void *param)
{
	int server_fd;
	unsigned char *buf, *alloc;
	_file_checksum *file_checksum;
	int read_bytes = 0, size = 0, i = 0;
	update_cfg_local_t *cfg = (update_cfg_local_t *)param;
	uint32_t address, flash_checksum=0;
	uint32_t NewImg2Len = 0, NewImg2Addr = 0, file_info[3];
	int ret = -1 ;

	DI("\n\r[%s] Update task start", __FUNCTION__);
	alloc = update_malloc(BUF_SIZE+4);
	if(!alloc){
		Log_Error("\n\r[%s] Alloc buffer failed", __FUNCTION__);
		goto update_ota_exit;
	}
	buf = &alloc[4];
	file_checksum = (void*)alloc;

	// Connect server
	server_fd = update_ota_connect_server(server_fd, cfg);
	if(server_fd == -1){
		goto update_ota_exit;
	}

	NewImg2Addr = update_ota_prepare_addr();
	if(NewImg2Addr == -1){
		goto update_ota_exit;
	}

	//Clear file_info
	memset(file_info, 0, sizeof(file_info));

	if(file_info[0] == 0){
		DI("\n\r[%s] Read info first", __FUNCTION__);
		read_bytes = read(server_fd, file_info, sizeof(file_info));
		// !X!X!X!X!X!X!X!X!X!X!X!X!X!X!X!X!X!X!X!X
		// !W checksum !W padding 0 !W file size !W
		// !X!X!X!X!X!X!X!X!X!X!X!X!X!X!X!X!X!X!X!X
		DI("\n\r[%s] info %d bytes", __FUNCTION__, read_bytes);
		DI("\n\r[%s] tx chechsum 0x%x, file size 0x%x", __FUNCTION__, file_info[0],file_info[2]);
		if(file_info[2] == 0){
			Log_Error("\n\r[%s] No checksum and file size", __FUNCTION__);
			goto update_ota_exit;
		}
	}

#if SWAP_UPDATE
	NewImg2Addr = update_ota_swap_addr(file_info[2], NewImg2Addr);
	if(NewImg2Addr == -1){
		goto update_ota_exit;
	}
#endif

	NewImg2Len = update_ota_erase_upg_region(file_info[2], NewImg2Len, NewImg2Addr);
	if(NewImg2Len == -1){
		goto update_ota_exit;
	}

	// reset
	file_checksum->u = 0;
	// Write New Image 2 sector
	if(NewImg2Addr != ~0x0){
		address = NewImg2Addr;
		DI("\n\rStart to read data %d bytes\r\n", NewImg2Len);
		while(1){
			memset(buf, 0, BUF_SIZE);
			read_bytes = read(server_fd, buf, BUF_SIZE);
			if(read_bytes == 0)
				break; // Read end
			if(read_bytes < 0){
				Log_Error("\n\r[%s] Read socket failed", __FUNCTION__);
				goto update_ota_exit;
			}

			if(read_bytes<4) {
				Log_Error("\n\r[%s] Recv small packet", __FUNCTION__);
			}
			DI(".");

			if((size+read_bytes)>NewImg2Len){
				DI("\n\r[%s] Redundant bytes received", __FUNCTION__);
				read_bytes = NewImg2Len-size;
			}

			device_mutex_lock(RT_DEV_LOCK_FLASH);
			if(flash_stream_write(&flash_ota, address + size, read_bytes, buf) < 0){
				Log_Error("\n\r[%s] Write stream failed", __FUNCTION__);
				device_mutex_unlock(RT_DEV_LOCK_FLASH);
				goto update_ota_exit;
			}
			device_mutex_unlock(RT_DEV_LOCK_FLASH);
			size += read_bytes;

			file_checksum->c[0] = alloc[4+read_bytes-4];      // checksum attached at file end
			file_checksum->c[1] = alloc[4+read_bytes-3];
			file_checksum->c[2] = alloc[4+read_bytes-2];
			file_checksum->c[3] = alloc[4+read_bytes-1];

			if(size == NewImg2Len)
				break;
		}
		DI("\n\rRead data finished\r\n");

		// read flash data back and calculate checksum
		for(i = 0; i < size-4; i += BUF_SIZE){
			int k;
			int rlen = (size-4-i) > BUF_SIZE ? BUF_SIZE : (size-4-i);
			flash_stream_read(&flash_ota, NewImg2Addr+i, rlen, buf);
			for(k = 0; k < rlen; k++)
				flash_checksum+=buf[k];
		}

		ret = update_ota_checksum(file_checksum, flash_checksum, NewImg2Addr);
		if(ret == -1){
			Log_Error("\r\nThe checksume is wrong!\r\n");
			goto update_ota_exit;
		}
	}
update_ota_exit:
	if(alloc)
		update_free(alloc);
	if(server_fd >= 0)
		close(server_fd);
	if(param)
		update_free(param);
	TaskOTA = NULL;
	DI("\n\r[%s] Update task exit", __FUNCTION__);
	if(!ret){
		Log_Error("\n\r[%s] Ready to reboot", __FUNCTION__);
		ota_platform_reset();
	}
	vTaskDelete(NULL);
	return;
}

int update_ota_local(char *ip, int port){
	update_cfg_local_t *pUpdateCfg;

	if(TaskOTA){
		Log_Error("\n\r[%s] Update task has created.", __FUNCTION__);
		return 0;
	}
	pUpdateCfg = update_malloc(sizeof(update_cfg_local_t));
	if(pUpdateCfg == NULL){
		Log_Error("\n\r[%s] Alloc update cfg failed", __FUNCTION__);
		return -1;
	}
	pUpdateCfg->ip_addr = inet_addr(ip);
	pUpdateCfg->port = ntohs(port);

	if(xTaskCreate(update_ota_local_task, "OTA_server", STACK_SIZE, pUpdateCfg, TASK_PRIORITY, &TaskOTA) != pdPASS){
	  	update_free(pUpdateCfg);
		Log_Error("\n\r[%s] Create update task failed", __FUNCTION__);
	}
	return 0;
}

void cmd_update(int argc, char **argv){
	int port;
	if(argc != 3){
		Log_Error("\n\r[%s] Usage: update IP PORT", __FUNCTION__);
		return;
	}
	port = atoi(argv[2]);

	update_ota_local(argv[1], port);
}

#endif // #if CONFIG_OTA_UPDATE

static void check_ota_image(void)
{
	flash_t	flash;
	uint32_t Part1Addr = 0xFFFFFFFF,Part2Addr = 0xFFFFFFFF;
	uint8_t *pbuf = NULL;

	device_mutex_lock(RT_DEV_LOCK_FLASH);
	flash_read_word(&flash, 0x18, &Part1Addr);
	Part1Addr = (Part1Addr&0xFFFF)*1024;	// first partition
	flash_read_word(&flash, OFFSET_DATA, &Part2Addr);
	device_mutex_unlock(RT_DEV_LOCK_FLASH);

	DI(" Part1Addr=0x%x Part2Addr=0x%x", Part1Addr, Part2Addr);
	if(Part2Addr == ~0x0) {
		return;
	}

	pbuf = update_malloc(FLASH_SECTOR_SIZE);
	if(!pbuf) {
		Log_Error("\r\n[%s] malloc error", __FUNCTION__);
		SendMessageToStateManager(MSG_ERR_WIFI_UPDATE, PARAM_FW_UPDATE_MALLOC_ERROR);
		return;
	}

	device_mutex_lock(RT_DEV_LOCK_FLASH);
	flash_stream_read(&flash, Part2Addr, FLASH_SECTOR_SIZE, pbuf);
	device_mutex_unlock(RT_DEV_LOCK_FLASH);
	DI(" flash read Part2Addr 0x%x +8[%.8s]", Part2Addr, pbuf+8);
#if SWAP_UPDATE
	device_mutex_lock(RT_DEV_LOCK_FLASH);
	flash_stream_read(&flash, Part1Addr, FLASH_SECTOR_SIZE, pbuf);
	device_mutex_unlock(RT_DEV_LOCK_FLASH);
	DI(" [SWAP_UPDATE] flash read Part1Addr 0x%x +8[%.8s]", Part1Addr, pbuf+8);
#endif
	update_free(pbuf);
}

void cmd_ota_image(bool cmd)
{
	flash_t	flash;
	uint32_t Part1Addr = 0xFFFFFFFF,Part2Addr = 0xFFFFFFFF;
	uint8_t *pbuf = NULL;

	DI(" cmd=%d", cmd);
	device_mutex_lock(RT_DEV_LOCK_FLASH);
	flash_read_word(&flash, 0x18, &Part1Addr);
	Part1Addr = (Part1Addr&0xFFFF)*1024;	// first partition
	flash_read_word(&flash, OFFSET_DATA, &Part2Addr);
	device_mutex_unlock(RT_DEV_LOCK_FLASH);

	DI(" Part1Addr=0x%x Part2Addr=0x%x", Part1Addr, Part2Addr);
	if(Part2Addr == ~0x0) {
		return;
	}

	pbuf = update_malloc(FLASH_SECTOR_SIZE);
	if(!pbuf) {
		Log_Error("\r\n[%s] malloc error", __FUNCTION__);
		SendMessageToStateManager(MSG_ERR_WIFI_UPDATE, PARAM_FW_UPDATE_MALLOC_ERROR);
		return;
	}

	device_mutex_lock(RT_DEV_LOCK_FLASH);
	flash_stream_read(&flash, Part2Addr, FLASH_SECTOR_SIZE, pbuf);
	if (cmd == 1) {
		memcpy((char*)pbuf+8, "81958711", 8);
	}
	else {
		memcpy((char*)pbuf+8, "01958711", 8);
	}

	flash_erase_sector(&flash, Part2Addr);
	flash_stream_write(&flash, Part2Addr, FLASH_SECTOR_SIZE, pbuf);
	device_mutex_unlock(RT_DEV_LOCK_FLASH);
	DI(" flash write Part2Addr 0x%x +8[%.8s]", Part2Addr, pbuf+8);
#if SWAP_UPDATE
	device_mutex_lock(RT_DEV_LOCK_FLASH);
	flash_stream_read(&flash, Part1Addr, FLASH_SECTOR_SIZE, pbuf);
	if (cmd == 1) {
		memcpy((char*)pbuf+8, "01958711", 8);
	}
	else {
		memcpy((char*)pbuf+8, "81958711", 8);
	}

	flash_erase_sector(&flash, Part1Addr);
	flash_stream_write(&flash, Part1Addr, FLASH_SECTOR_SIZE, pbuf);
	device_mutex_unlock(RT_DEV_LOCK_FLASH);
	DI(" [SWAP_UPDATE] flash write Part1Addr 0x%x +8[%.8s]", Part1Addr, pbuf+8);
#endif
	update_free(pbuf);
}


void cmd_ota_image_set(bool cmd)
{
	flash_t	flash;
	uint32_t Part1Addr = 0xFFFFFFFF,Part2Addr = 0xFFFFFFFF;
	uint8_t *pbuf = NULL;

	DI(" cmd=%d", cmd);
	device_mutex_lock(RT_DEV_LOCK_FLASH);
	flash_read_word(&flash, 0x18, &Part1Addr);
	Part1Addr = (Part1Addr&0xFFFF)*1024;	// first partition
	flash_read_word(&flash, OFFSET_DATA, &Part2Addr);
	device_mutex_unlock(RT_DEV_LOCK_FLASH);

	DI(" Part1Addr=0x%x Part2Addr=0x%x", Part1Addr, Part2Addr);
	if(Part2Addr == ~0x0) {
		return;
	}

	pbuf = update_malloc(FLASH_SECTOR_SIZE);
	if(!pbuf) {
		Log_Error("\r\n[%s] malloc error", __FUNCTION__);
		SendMessageToStateManager(MSG_ERR_WIFI_UPDATE, PARAM_FW_UPDATE_MALLOC_ERROR);
		return;
	}

	if (cmd == 1) {
		device_mutex_lock(RT_DEV_LOCK_FLASH);
		flash_stream_read(&flash, Part2Addr, FLASH_SECTOR_SIZE, pbuf);

		memcpy((char*)pbuf+8, "81958711", 8);
		flash_erase_sector(&flash, Part2Addr);
		flash_stream_write(&flash, Part2Addr, FLASH_SECTOR_SIZE, pbuf);
		device_mutex_unlock(RT_DEV_LOCK_FLASH);
		DI(" flash write Part2Addr 0x%x +8[%.8s]", Part2Addr, pbuf+8);
	}

#if SWAP_UPDATE
	if (cmd == 0) {
		device_mutex_lock(RT_DEV_LOCK_FLASH);
		flash_stream_read(&flash, Part1Addr, FLASH_SECTOR_SIZE, pbuf);
		memcpy((char*)pbuf+8, "81958711", 8);
		flash_erase_sector(&flash, Part1Addr);
		flash_stream_write(&flash, Part1Addr, FLASH_SECTOR_SIZE, pbuf);
		device_mutex_unlock(RT_DEV_LOCK_FLASH);
		DI(" [SWAP_UPDATE] flash write Part1Addr 0x%x +8[%.8s]", Part1Addr, pbuf+8);
	}

#endif
	update_free(pbuf);
}


void cmd_ota_image_unset(bool cmd)
{
	flash_t	flash;
	uint32_t Part1Addr = 0xFFFFFFFF,Part2Addr = 0xFFFFFFFF;
	uint8_t *pbuf = NULL;

	DI(" cmd=%d", cmd);
	device_mutex_lock(RT_DEV_LOCK_FLASH);
	flash_read_word(&flash, 0x18, &Part1Addr);
	Part1Addr = (Part1Addr&0xFFFF)*1024;	// first partition
	flash_read_word(&flash, OFFSET_DATA, &Part2Addr);
	device_mutex_unlock(RT_DEV_LOCK_FLASH);

	DI(" Part1Addr=0x%x Part2Addr=0x%x", Part1Addr, Part2Addr);
	if(Part2Addr == ~0x0) {
		return;
	}

	pbuf = update_malloc(FLASH_SECTOR_SIZE);
	if(!pbuf) {
		Log_Error("\r\n[%s] malloc error", __FUNCTION__);
		SendMessageToStateManager(MSG_ERR_WIFI_UPDATE, PARAM_FW_UPDATE_MALLOC_ERROR);
		return;
	}

	if (cmd == 0) {
		device_mutex_lock(RT_DEV_LOCK_FLASH);
		flash_stream_read(&flash, Part2Addr, FLASH_SECTOR_SIZE, pbuf);

		memcpy((char*)pbuf+8, "01958711", 8);
		flash_erase_sector(&flash, Part2Addr);
		flash_stream_write(&flash, Part2Addr, FLASH_SECTOR_SIZE, pbuf);
		device_mutex_unlock(RT_DEV_LOCK_FLASH);
		DI(" flash write Part2Addr 0x%x +8[%.8s]", Part2Addr, pbuf+8);
	}

#if SWAP_UPDATE
	if (cmd == 1) {
		device_mutex_lock(RT_DEV_LOCK_FLASH);
		flash_stream_read(&flash, Part1Addr, FLASH_SECTOR_SIZE, pbuf);
		memcpy((char*)pbuf+8, "01958711", 8);
		flash_erase_sector(&flash, Part1Addr);
		flash_stream_write(&flash, Part1Addr, FLASH_SECTOR_SIZE, pbuf);
		device_mutex_unlock(RT_DEV_LOCK_FLASH);
		DI(" [SWAP_UPDATE] flash write Part1Addr 0x%x +8[%.8s]", Part1Addr, pbuf+8);
	}

#endif
	update_free(pbuf);
}

#if CONFIG_OTA_HTTP_UPDATE

#ifdef USE_DNS_GETHOSTBYNAME
sys_sem_t dns_wait_sem;
ip_addr_t dns_output_ipaddr;
static void dns_found_cb(const char *name, ip_addr_t *ipaddr, void *callback_arg)
{
	DD(" found name=%s ipaddr=0x%x\r\n", name, ipaddr->addr);
	memcpy(&dns_output_ipaddr, ipaddr, sizeof(ip_addr_t));
	sys_sem_signal(&dns_wait_sem);
}
#endif

/******************************************************************************************************************
** Function Name  : update_ota_http_connect_server
** Description    : connect to the OTA server
** Input          : server_fd: the socket used
**					host: host address of the OTA server
**					port: port of the OTA server
** Return         : connect ok:	socket value
**					Failed:		-1
*******************************************************************************************************************/
int update_ota_http_connect_server(int *server_fd, char *host, int port)
{
	struct sockaddr_in server_addr;
    struct hostent *server;

	*server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if(*server_fd < 0){
		Log_Error("\n\r[%s] Create socket failed", __FUNCTION__);
		SendMessageToStateManager(MSG_ERR_FWUPDATE,PARAM_FW_UPDATE_CREATE_SOCKET_ERROR);
		return -1;
	}
	D(" server_fd = %d", *server_fd);
#ifdef USE_DNS_GETHOSTBYNAME
	{ // reference to sntp_request().  sntp_request used dns_gethostbyname().
		ip_addr_t server_address;
		err_t err;
		err = sys_sem_new(&dns_wait_sem, 0);
		if (err != ERR_OK) {
			D("[ERROR] sys_sem_new dns_wait_sem ");
			SendMessageToStateManager(MSG_ERR_FWUPDATE,PARAM_FW_UPDATE_SEM_NEW_ERROR);
			return -1;
		}
		err = dns_gethostbyname(host, &server_address, dns_found_cb, NULL);
		if (err == ERR_INPROGRESS) {
			/* DNS request sent, wait for sntp_dns_found being called */
			D(" Waiting for server address to be resolved.\n");

			D(" wait until dns resolved...");
			sys_sem_wait(&dns_wait_sem);

			memcpy(&server_address , &dns_output_ipaddr, sizeof(ip_addr_t));
			err = ERR_OK;
		} else {
			sys_sem_signal(&dns_wait_sem);
		}
		sys_sem_free(&dns_wait_sem);

		if (err != ERR_OK) {
			/* address conversion failed, try another server */
			Log_Error("\r\n[update_ota_http_connect_server] Invalid server address.(%d)\n",err);
			return -1;
		}

		memset(&server_addr,0,sizeof(server_addr));
		server_addr.sin_family = AF_INET;
		server_addr.sin_port = htons(port);
		memcpy(&server_addr.sin_addr.s_addr, &server_address.addr, sizeof(u32_t));

		D(" dns_gethostbyname name=%s  s_addr:0x%x\n\r", host, server_addr.sin_addr.s_addr);
	}
#else
    server = gethostbyname(host);
    if(server == NULL){
        Log_Error("\n\r[ERROR] Get host ip failed\n\r");
		return -1;
    }

    memset(&server_addr,0,sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    memcpy(&server_addr.sin_addr.s_addr,server->h_addr,server->h_length);

    D(" gethostbyname name=%s  s_addr:0x%x\n\r", host, server_addr.sin_addr.s_addr);
    D(" h_name:%s p_addr:%x  h_len:%d  s_addr:0x%x\n\r",
    			server->h_name, server->h_addr, server->h_length, server_addr.sin_addr.s_addr);

#endif


    if (connect(*server_fd,(struct sockaddr *)&server_addr, sizeof(server_addr)) < 0){
    	close(*server_fd);
		Log_Error("\n\r[%s] Socket connect failed\n\r", __FUNCTION__);
		return -1;
	}

	return *server_fd;
}


/******************************************************************************************************************
** Function Name  : parse_http_response
** Description    : Parse the http response to get some useful parameters
** Input          : response	: The http response got from server
**					response_len: The length of http response
**					result		: The struct that store the usful infor about the http response
** Return         : Parse OK:	1 -> Only got the status code
**								3 -> Got the status code and content_length, but didn't get the full header
**								4 -> Got all the information needed
**					Failed:		-1
*******************************************************************************************************************/
static int parse_http_response(uint8_t *response, uint32_t response_len, http_response_result_t *result) {
    uint32_t i, p, q, m;
    uint32_t header_end = 0;

    D("response-----------\r\n%s\r\n------------------\r\n", response);

    //Get status code
	if(0 == result->parse_status){//didn't get the http response
		uint8_t status[4] = {0};
		i = p = q = m = 0;
		for (; i < response_len; ++i) {
			if (' ' == response[i]) {
				++m;
				if (1 == m) {//after HTTP/1.1
					p = i;
				}
				else if (2 == m) {//after status code
					q = i;
					break;
				}
			}
		}
		if (!p || !q || q-p != 4) {//Didn't get the status code
			return -1;
		}
		memcpy(status, response+p+1, 3);//get the status code
		result->status_code = atoi((char const *)status);
		if(result->status_code == 200) {
			result->parse_status = 1;
		}
		else{
			Log_Error("\n\r[%s] The http response status code is %d", __FUNCTION__, result->status_code);
			SendMessageToStateManager(MSG_ERR_FWUPDATE, PARAM_FW_UPDATE_HTTP_RESPONSE_CODE_ERROR);
			return -1;
		}
	}

	//if didn't receive the full http header
	if(3 == result->parse_status){//didn't get the http response
		p = q = 0;
		for (i = 0; i < response_len; ++i) {
			if (response[i] == '\r' && response[i+1] == '\n' &&
				response[i+2] == '\r' && response[i+3] == '\n') {//the end of header
				header_end = i+4;
				result->parse_status = 4;
				result->header_len = header_end;
				result->body = response + header_end;
				break;
			}
		}
		if (3 == result->parse_status) {//Still didn't receive the full header
			result->header_bak = update_malloc(HEADER_BAK_LEN + 1);
			memset(result->header_bak, 0, strlen(result->header_bak));
			memcpy(result->header_bak, response + response_len - HEADER_BAK_LEN, HEADER_BAK_LEN);
		}
	}

    //Get Content-Length
	if(1 == result->parse_status){//didn't get the content length
		uint32_t content_length = 0;
		const uint8_t *content_length_buf1 = "CONTENT-LENGTH";
		const uint8_t *content_length_buf2 = "Content-Length";
		const uint32_t content_length_buf_len = strlen(content_length_buf1);
		p = q = 0;

		for (i = 0; i < response_len; ++i) {
			if (response[i] == '\r' && response[i+1] == '\n') {
				q = i;//the end of the line
				if (!memcmp(response+p, content_length_buf1, content_length_buf_len) ||
						!memcmp(response+p, content_length_buf2, content_length_buf_len)) {//get the content length
					int j1 = p+content_length_buf_len, j2 = q-1;
					while ( j1 < q && (*(response+j1) == ':' || *(response+j1) == ' ') ) ++j1;
					while ( j2 > j1 && *(response+j2) == ' ') --j2;
					uint8_t len_buf[12] = {0};
					memcpy(len_buf, response+j1, j2-j1+1);
					result->body_len = atoi((char const *)len_buf);
					result->parse_status = 2;
				}
				p = i+2;
			}
			if (response[i] == '\r' && response[i+1] == '\n' &&
					response[i+2] == '\r' && response[i+3] == '\n') {//Get the end of header
				header_end = i+4;//p is the start of the body
				if(result->parse_status == 2){//get the full header and the content length
					result->parse_status = 4;
					result->header_len = header_end;
					result->body = response + header_end;
				}
				else {//there are no content length in header
					Log_Error("\n\r[%s] No Content-Length in header", __FUNCTION__);
					SendMessageToStateManager(MSG_ERR_FWUPDATE,PARAM_FW_UPDATE_HTTP_CONTENT_LENGTH_ERROR);
					return -1;
				}
				break;
			}
		}

		if (1 == result->parse_status) {//didn't get the content length and the full header
			result->header_bak = update_malloc(HEADER_BAK_LEN + 1);
			D(" check ");
			memset(result->header_bak, 0, strlen(result->header_bak)); //[TODO]
			memcpy(result->header_bak, response + response_len - HEADER_BAK_LEN, HEADER_BAK_LEN);
		}
		else if (2 == result->parse_status) {//didn't get the full header but get the content length
			result->parse_status = 3;
			result->header_bak = update_malloc(HEADER_BAK_LEN + 1);
			D(" check ");
			memset(result->header_bak, 0, strlen(result->header_bak)); //[TODO]
			memcpy(result->header_bak, response + response_len - HEADER_BAK_LEN, HEADER_BAK_LEN);
		}
	}

	return result->parse_status;
}

//#include "cmsis_os.h"
#include <cJSON.h>

// input  : iot_json
// output : version
static void handle_json_get_version(char *iot_json, char *version)
{
	cJSON_Hooks memoryHook;

	memoryHook.malloc_fn = pvPortMalloc;
	memoryHook.free_fn = vPortFree;
	cJSON_InitHooks(&memoryHook);


	cJSON *IOTJSObject,  *dataJSObject, *fwJSObject;
	int sensor_data, red, green, blue;

	if((IOTJSObject = cJSON_Parse(iot_json)) != NULL) {

		dataJSObject = cJSON_GetObjectItem(IOTJSObject, "data");

		if(dataJSObject){
			fwJSObject = cJSON_GetObjectItem(dataJSObject, "fw_version");

			if(fwJSObject) {
				D(" fw_version [%s]\n\r", fwJSObject->valuestring);
				strcpy(version, fwJSObject->valuestring);
			} else {
				fwJSObject = cJSON_GetObjectItem(dataJSObject, "ver");
				if(fwJSObject) {
					D(" ver [%s]\n\r", fwJSObject->valuestring);
					strcpy(version, fwJSObject->valuestring);
				}
			}

		}

		if (IOTJSObject) {
			cJSON_Delete(IOTJSObject);
		}
		if (fwJSObject) {
			cJSON_Delete(fwJSObject);
		}

	} else {
		Log_Error("\n\r[%s] cJSON_Parse error  [%s]", __FUNCTION__, iot_json);
		SendMessageToStateManager(MSG_ERR_FWUPDATE, PARAM_FW_UPDATE_JSON_ERROR);

	}
}

#ifdef DEBUG_ONLY_ENABLE
// -------------------------- TEST ONLY -----------------------------
// input  : iot_json
// output : data
static void handle_json_get_data(char *iot_json, char *data)
{
	D("iot_json[%s]", iot_json);
	strcpy(data, iot_json);


	cJSON_Hooks memoryHook;

	memoryHook.malloc_fn = pvPortMalloc;
	memoryHook.free_fn = vPortFree;
	cJSON_InitHooks(&memoryHook);

	cJSON *IOTJSObject, *dataJSObject = NULL, *fwJSObject = NULL;
	int sensor_data, red, green, blue;

	if((IOTJSObject = cJSON_Parse(iot_json)) != NULL) {

		dataJSObject = cJSON_GetObjectItem(IOTJSObject, "data");
		D(" dataJSObject type=%d [%s] cJSON_GetArraySize=%d", dataJSObject->type, dataJSObject->string, cJSON_GetArraySize(dataJSObject));

		if(dataJSObject){
			D(" dataJSObject type=%d [%s]", dataJSObject->type, dataJSObject->string);
		}
		if(dataJSObject){
			fwJSObject = cJSON_GetObjectItem(dataJSObject, "fw_version");

			if(fwJSObject) {
				DI(" fw_version [%s]\n\r", fwJSObject->valuestring);
			} else {
				fwJSObject = cJSON_GetObjectItem(dataJSObject, "ver");
				if(fwJSObject) {
					DI(" ver [%s]\n\r", fwJSObject->valuestring);
				}
			}

		}

		if (IOTJSObject) {
			cJSON_Delete(IOTJSObject);
		}
		if (dataJSObject) {
			cJSON_Delete(dataJSObject);
		}
		if (fwJSObject) {
			cJSON_Delete(fwJSObject);
		}

	} else {
		Log_Error("\n\r[%s] cJSON_Parse error  [%s]", __FUNCTION__, iot_json);

	}

}
#endif // DEBUG_PRINT_ENABLE

#ifdef CONFIG_CRC
static uint32_t crc_table[256];


#define PROD_FW_MAJOR_VER	1
#define PROD_FW_MID_VER		2

#define STAGE_FW_MAJOR_VER	0
#define STAGE_FW_MID_VER	12


boolean check_crc_fw_version()
{

	boolean				ret = FALSE;
	char				*major = NULL;
	char				*middle = NULL;
	char 				version[G_VERSION_SIZE / 2] = {0};

	// am_version(書き込みを行うとしているFW version) をmajor, middle versionに分割
	strncpy(version, am_version, (G_VERSION_SIZE / 2));

	major = strtok(version, ".");
	if (major != NULL) {
		middle = strtok( NULL, ".");
	}
	if ((major == NULL) || (middle == NULL)) {
		// バージョンの文字列が存在していなかった。
		Log_Error("version error:There is no version string.");
		return ret;
	}

	// version比較
	if (
		((atoi((const char*)major) == PROD_FW_MAJOR_VER)  && (atoi((const char*)middle) >= PROD_FW_MID_VER )) || 
		((atoi((const char*)major) == STAGE_FW_MAJOR_VER) && (atoi((const char*)middle) >= STAGE_FW_MID_VER))
		) {
		DI("crc version:Fw check uses with crc.");
		DI("major version:%d middle version:%d", atoi((const char*)major), atoi((const char*)middle));
		ret = TRUE;
	} else {
		// 書き込みを行おうとしているFW バージョンが古く、CRCが付いていないのでchecksumで確認を行う。
		DI("old version:There is no crc in FW.");
	}

	return ret;

}

#endif /* CONFIG_CRC */

int ota_http_client_main(char *cmd, char *host, int port, char *resource, char *post_data)
{
#ifndef  BUF_USE_SDRAM
	unsigned char *buf, *alloc, *request;
#endif
	int ret; //, len;
	int read_bytes, i = 0;
	int retry_count = 0;
#if CONFIG_SSL_HTTPS_CLIENT
#if	CONFIG_USE_MBEDTLS
    //CONFIG_USE_MBEDTLS
	mbedtls_net_context server_fd; // CONFIG_USE_MBEDTLS
	mbedtls_ssl_context ssl;
	mbedtls_ssl_config conf;
#endif
#else
	int server_fd = 0;
#endif
	_file_checksum *file_checksum;
#ifdef CONFIG_CRC
	_file_checksum 		file_crc;
	uint32_t 			flash_crc = 0;
#endif /* CONFIG_CRC */
	uint32_t address, flash_checksum = 0;
	uint32_t NewImg2Len = 0, NewImg2Addr = 0;
	http_response_result_t rsp_result = {0};

#ifdef CHANGE_IMG_0008
	unsigned char data_0008_back;
#endif

	wdt_task_refresh(WDT_TASK_FWUP);

	DI("##### START Wi-Fi update ##### wait 6S");
	vTaskDelay(6*1000);
	DI("wifi_is_up=%d,%d wifi_is_connected_to_ap=%d", wifi_is_up(0), wifi_is_up(1), wifi_is_connected_to_ap());

	if(wifi_is_connected_to_ap()!=0){
		ota_update_task_stop = 2;
		Log_Error("\n\r[%s] Wi-Fi not ready(%d)", __FUNCTION__, wifi_is_connected_to_ap());
		SendMessageToStateManager(MSG_ERR_FWUPDATE, PARAM_FW_UPDATE_WIFI_NOT_READY);
		ret = OTA_UPDATE_STOP_AND_CONTINUE;
		goto update_ota_exit1;
	}

#if CONFIG_SSL_HTTPS_CLIENT
	// org mbedtls_platform_set_calloc_free(my_calloc, my_free);
	ssl_my_alloc_init();
	mbedtls_platform_set_calloc_free(ssl_my_calloc, ssl_my_free);

	update_my_alloc_init();


#if defined(MBEDTLS_DEBUG_C)
	mbedtls_debug_set_threshold(DEBUG_LEVEL);
#endif
#if CONFIG_USE_MBEDTLS
	D(" start CONFIG_USE_MBEDTLS");
	D(" MBEDTLS_SSL_MAX_CONTENT_LEN=%d", MBEDTLS_SSL_MAX_CONTENT_LEN); // config_rsa.h
#endif
#endif //  CONFIG_SSL_HTTPS_CLIENT
	DI("rtw_getFreeHeapSize = %d", rtw_getFreeHeapSize());


	if (ota_update_task_stop != 0) {
		DI(" stop(%d)", ota_update_task_stop);
		ret = OTA_UPDATE_STOP_AND_CONTINUE;
		goto update_ota_exit1;
	}
	ota_update_task_run = 1;

#ifndef  BUF_USE_SDRAM
	alloc = update_malloc(BUF_SIZE + 4);
	if(!alloc){
		Log_Error("\n\r[%s] Alloc buffer failed", __FUNCTION__);
		goto update_ota_exit;
	}
	buf = &alloc[4];
#endif
	file_checksum = (void*)(&alloc[0]);

#if CONFIG_SSL_HTTPS_CLIENT
	/*
	 * 1. Start the connection
	 */
	DI("\n\r  . MBEDTLS Connecting to tcp/ %s %d...", host, port);

	mbedtls_net_init(&server_fd);

	// int mbedtls_net_connect( mbedtls_net_context *ctx, const char *host, const char *port, int proto )
	char char_port[8];
	sprintf(char_port,"%d", port);
	retry_count = 0;
	while((ret = mbedtls_net_connect(&server_fd, host, char_port, MBEDTLS_NET_PROTO_TCP)) != 0) {
		DI("mbedtls_net_connect failed(%d)\n", ret);
		if (++retry_count <= CONNECT_RETRY_MAX && !ota_update_task_stop) {
			DI(" connect retry(%d)", retry_count);
			mbedtls_net_init(&server_fd);
			continue;
		}
		DE("mbedtls_net_connect failed\n\r  ! mbedtls_net_connect returned %d\n", ret);
		if (connect_agein_count++ < 3 && !ota_update_task_stop) {
			ret = OTA_UPDATE_CONNECT_AGEIN; // OTA_UPDATE_STOP_AND_CONTINUE;
		} else {
			ret = OTA_UPDATE_STOP_AND_CONTINUE;
		}
		goto update_ota_exit1;
	}
	connect_agein_count = 0;
#else
	// ------------------ HTTP CONNECT ----------------------------------------
	DI(" connect host=%s port=%d  server_fd=0x%p %d ...", host, port, &server_fd, server_fd);
	int connect_count = CONNECT_RETRY_MAX;
	server_fd = -1;
	while ( --connect_count >= 0 && server_fd < 0) {
		server_fd = update_ota_http_connect_server(&server_fd, host, port);
		if (server_fd >= 0) {
			break;
		}
		DI(" retry connect server %s %d", host, port);
	}
	D(" server_fd (%d)", server_fd);
	if(server_fd == -1){
		DI("server connect error");
		ret = -1;
	}
#endif
	DI(" ok\n");

#if CONFIG_SSL_HTTPS_CLIENT
	/*
	 * 2. Setup stuff
	 */
	DI("  . MBEDTLS Setting up the SSL/TLS structure...");

	mbedtls_ssl_init(&ssl);
	mbedtls_ssl_config_init(&conf);

#ifdef SSL_CLIENT_EXT
#if CONFIG_AUDREY_FWUP_ALONE
	if(strncmp(cmd,"SAA",3) != 0 && strncmp(cmd,"SAS",3) != 0) {
#endif
		D("SSL_CLIENT_EXT ssl_client_ext_init");
		if((ret = ssl_client_ext_init()) != 0) {
			printf("\r\n ssl_client_ext_init failed\n\r  ! ssl_client_ext_init returned %d\n", ret);
			goto update_ota_exit1;
		}
#if CONFIG_AUDREY_FWUP_ALONE
	}
#endif
#endif

	/*
	 * common\network\ssl\mbedtls-2.4.0\include\mbedtls\compat-1.3.h
		#define net_close mbedtls_net_free
		#define net_connect mbedtls_net_connect
		#define net_recv mbedtls_net_recv
		#define net_recv_timeout mbedtls_net_recv_timeout
		#define net_send mbedtls_net_send
		#define net_set_block mbedtls_net_set_block
		#define net_set_nonblock mbedtls_net_set_nonblock
	*/

	//mbedtls_ssl_set_bio(&ssl, &server_fd, mbedtls_net_send, mbedtls_net_recv, NULL);
	// 	ssl_set_bio(ssl, my_ssl_read, server_fd, my_ssl_send, server_fd);
	mbedtls_ssl_set_bio(&ssl, &server_fd, my_ssl_send, my_ssl_read, NULL);

	if((ret = mbedtls_ssl_config_defaults(&conf,
		MBEDTLS_SSL_IS_CLIENT,
		MBEDTLS_SSL_TRANSPORT_STREAM,
		MBEDTLS_SSL_PRESET_DEFAULT)) != 0) {

		printf("\r\n mbedtls_ssl_config_defaults failed\n  ! mbedtls_ssl_config_defaults returned %d\n\n", ret);
		goto update_ota_exit;
	}

#if CONFIG_AUDREY_FWUP_ALONE
	if(strncmp(cmd,"SAA",3) == 0 || strncmp(cmd,"SAS",3) == 0) {
		mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_NONE);
		DI("MBEDTLS_SSL_VERIFY_NONE");
	} else {
#endif
		//SSL_VERIFY_REQUIRED
		mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_REQUIRED);
		DI("MBEDTLS_SSL_VERIFY_REQUIRED");
#if CONFIG_AUDREY_FWUP_ALONE
	}
#endif
	mbedtls_ssl_conf_rng(&conf, ssl_my_random, NULL);
	mbedtls_ssl_conf_dbg(&conf, ssl_my_debug, NULL);



#ifdef SSL_CLIENT_EXT
#if CONFIG_AUDREY_FWUP_ALONE
	if(strncmp(cmd,"SAA",3) != 0 && strncmp(cmd,"SAS",3) != 0) {
#endif
		D("SSL_CLIENT_EXT ssl_client_ext_setup");
		if((ret = ssl_client_ext_setup(&conf)) != 0) {
			printf("\r\n ssl_client_ext_setup failed\n\r  ! ssl_client_ext_setup returned %d\n", ret);
			goto update_ota_exit;
		}
#if CONFIG_AUDREY_FWUP_ALONE
	}
#endif
#endif

	if((ret = mbedtls_ssl_setup(&ssl, &conf)) != 0) {
		printf("\r\n mbedtls_ssl_setup failed\n  ! mbedtls_ssl_setup returned %d\n\n", ret);
		goto update_ota_exit;
	}

	DI(" ok\n");

	/*
	 * 3. Handshake
	 */
	DI("\n\r  . MBEDTLS Performing the SSL/TLS handshake...");

	retry_count = 0;
	while((ret = mbedtls_ssl_handshake(&ssl)) != 0) {
		if((ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE
			&& ret != MBEDTLS_ERR_NET_RECV_FAILED) || retry_count >= CONNECT_RETRY_MAX) {

			printf(" failed\n\r  ! mbedtls_ssl_handshake returned -0x%x\n", -ret);
			ret = OTA_UPDATE_STOP_AND_CONTINUE;
			goto update_ota_exit;
		}

		retry_count++;
	}

	DI(" ok\n");
	DI("\n\r  . MBEDTLS Use ciphersuite %s\n", mbedtls_ssl_get_ciphersuite(&ssl));
#endif // CONFIG_SSL_HTTPS_CLIENT

#ifndef WRITE_FLASH9000_AFTER_FILE_READ
	if (strncmp(cmd,"AM",2)==0 || strncmp(cmd,"SAA",3)==0) {
		NewImg2Addr = update_ota_prepare_addr(); //[TODO]　ここで0x9000を変更するとファイル読み込みに失敗したら誤動作する可能性がある
		if(NewImg2Addr == -1){
			ret = -1;
			goto update_ota_exit;
		}

		{ // signature check!!
			unsigned int OldAddr;
			uint32_t sig_readback0 = 0,sig_readback1 = 0;
			uint32_t sig_readback20 = 0,sig_readback21 = 0;

			DI("[CHECK] NewImg2Addr=0x%x", NewImg2Addr);
			device_mutex_lock(RT_DEV_LOCK_FLASH);
			flash_read_word(&flash_ota, NewImg2Addr + 8, &sig_readback0);
			flash_read_word(&flash_ota, NewImg2Addr + 12, &sig_readback1);
			device_mutex_unlock(RT_DEV_LOCK_FLASH);
			DI("[CHECK] NewImg2Addr=0x%x signature read  %x,%x", NewImg2Addr, sig_readback0, sig_readback1);

			if (NewImg2Addr == 0xb000) {
				OldAddr = 0x100000;
			}else {
				OldAddr = 0xb000;
			}

			device_mutex_lock(RT_DEV_LOCK_FLASH);
			flash_read_word(&flash_ota, OldAddr + 8, &sig_readback20);
			flash_read_word(&flash_ota, OldAddr + 12, &sig_readback21);
			device_mutex_unlock(RT_DEV_LOCK_FLASH);
			DI("[CHECK] OldAddr=0x%x signature read  %x,%x", OldAddr, sig_readback20, sig_readback21);

			if (sig_readback0 == 0x35393138 && sig_readback20 == 0x35393138) {
				DE(" #### Part1 and Part2 are the same signature 0x35393138 #####");
				DE(" #### Part2 Img is started when next rebooting! ####");
			}
		}
	} else {
		NewImg2Addr = buf_sdram; // dummy
	}
	DI("NewImg2Addr=0x%x ota_update_task_run=%d", NewImg2Addr, ota_update_task_run);
#endif // DELAY_WRITE_FLASH9000

	// clear checksum
	file_checksum->u = 0;

	// Write New Image 2 sector
#ifndef DELAY_WRITE_FLASH9000
	if (NewImg2Addr != ~0x0 && ota_update_task_run)
#else
	if(ota_update_task_run)
#endif
	{
		uint32_t idx = 0;
		int data_len = 0;
		int request_len;
		DI("\n\r");
		DI("resource[%s]\r\n", resource);
#ifdef DEBUG_ONLY_ENABLE
		if (strncmp(cmd,"TEST",4)==0) {
			// POST
			request_len = sprintf(request, "POST %s HTTP/1.1\r\n%s\r\n%s%s\r\nContent-Type: application/json\r\nContent-Length: %d\r\n\r\n%s\r\n\r\n",
								resource, OTA_HTTP_REQ_HOST, OTA_HTTP_REQ_X_API_KEY, AUDREY_API_KEY, strlen(post_data), post_data);

		} else {
#endif
		// GET
#if CONFIG_AUDREY_FWUP_ALONE
			if(strncmp(cmd,"SAA",3) == 0 || strncmp(cmd,"SAS",3) == 0) {
				request_len = sprintf(request, "GET %s HTTP/1.1\r\n%s%s\r\nContent-Type: text/plain\r\n\r\n",
						resource, "Host: ", host);
			} else {
#endif
				request_len = sprintf(request, "GET %s HTTP/1.1\r\n%s\r\n%s%s\r\nContent-Type: application/json\r\n\r\n",
						resource, OTA_HTTP_REQ_HOST, OTA_HTTP_REQ_X_API_KEY, AUDREY_API_KEY);
#if CONFIG_AUDREY_FWUP_ALONE
			}
#endif
#ifdef DEBUG_ONLY_ENABLE
		}
#endif
		DI(" Write to server: request [%s]\n\r", request);
#if CONFIG_SSL_HTTPS_CLIENT

		//CONFIG_USE_MBEDTLS
		DI("MBEDTLS write");
		while((ret = mbedtls_ssl_write(&ssl, request, strlen(request))) <= 0) {
			if(ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
				DI(" failed\n\r  ! mbedtls_ssl_write returned %d\n", ret);
				ret = OTA_UPDATE_STOP_AND_CONTINUE;
				goto update_ota_exit;
			}
		}
		if(ret < 0){
			Log_Error("\n\r[%s] Send HTTPS request failed ret=%d", __FUNCTION__, ret);
			SendMessageToStateManager(MSG_ERR_FWUPDATE, PARAM_FW_UPDATE_SSL_WRITE_REQUEST_ERROR);
			ret = -1;
			ret = OTA_UPDATE_STOP_AND_CONTINUE;
			goto update_ota_exit;
		}

		D(" %d bytes written\n\n%s", ret, (char *) buf);


#else
		// http
		if (wait_write_select(server_fd)) {
							DE(" write time out");
							ret = -1;
							goto update_ota_exit;
						}
		ret = write(server_fd, request, strlen(request));
		if(ret < 0){
			Log_Error("\n\r[%s] Send HTTP request failed ret=%d", __FUNCTION__, ret);
			ret = -1;
			goto update_ota_exit;
		}
#endif

		D("next read");
		wdt_task_refresh(WDT_TASK_FWUP);
		while (3 >= rsp_result.parse_status && ota_update_task_run){//still read header
			D(" rsp_result.parse_status = %d", rsp_result.parse_status);
			if(0 == rsp_result.parse_status){//didn't get the http response
				memset(buf, 0, BUF_SIZE);
#if CONFIG_SSL_HTTPS_CLIENT
				D("ssl read len=%d", BUF_SIZE);

				// CONFIG_USE_MBEDTLS
				ret = mbedtls_ssl_read(&ssl,  buf, BUF_SIZE);

				if(ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE)
					continue;

				if(ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
					DI("MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY");
					break;
				}

				if(ret < 0) {
					DI(" failed\n  ! mbedtls_ssl_read returned %d\n", ret);
					//break;
					ret = OTA_UPDATE_STOP_AND_CONTINUE;
					goto update_ota_exit;
				}

				if(ret == 0) { //[TODO] continue ?

					DI("\n\nEOF\n\n");
					break;
				}

				read_bytes = ret;
				D(" %d bytes read\n\n%s", read_bytes, (char *) buf);
#else
				D("http read len=%d", BUF_SIZE);
				if (wait_read_select(server_fd)) {
					DE(" read time out");
					ret = -1;
					goto update_ota_exit;
				}
				read_bytes = read(server_fd, buf, BUF_SIZE);
#endif
				D(" read:%d", read_bytes);
				if(read_bytes == 0)  {
					continue;
				}
				if(read_bytes < 0){
					Log_Error("\n\r[%s] Read socket failed", __FUNCTION__);
					SendMessageToStateManager(MSG_ERR_FWUPDATE, PARAM_FW_UPDATE_READ_ERROR);
					//ret = -1;
					ret = OTA_UPDATE_STOP_AND_CONTINUE;
					goto update_ota_exit;
				}

				idx = read_bytes;
				memset(&rsp_result, 0, sizeof(rsp_result));
				if(parse_http_response(buf, idx, &rsp_result) == -1){
					DI(" parse_http_response -1  rsp_result.status_code=%d", rsp_result.status_code);
					ret = 0; // -1;
					if (rsp_result.status_code == 301) { // Redirecting
						DI("Redirecting rsp_result.parse_status=%d", rsp_result.parse_status);
						char *href;
						char *http;
						request_len = 0;
						href = strstr(buf,"href=");
						if (href) {
							http = strstr(href,"http://");
							if (http){
								char *top;
								top = strstr(http+7, "/");
								if (top) {
									char *amp;
									amp = strstr(top+1, "&amp;");

									if (amp) { // [TODO]  &amp;
										char *last;
										*amp = 0;
										last = strstr(amp+5,">");
										if (last) {
											*(last-1) = 0;
											DI("new url=%s&%s", top+1, amp+5);
#if CONFIG_AUDREY_FWUP_ALONE
											if(strncmp(cmd,"SAA",3) == 0 || strncmp(cmd,"SAS",3) == 0) {
												request_len = sprintf(request, "GET /%s&%s HTTP/1.1\r\n%s%s\r\nContent-Type: text/plain\r\n\r\n",
													top+1, amp+5, "Host: ", host);
											} else {
#endif
												request_len = sprintf(request, "GET /%s&%s HTTP/1.1\r\n%s\r\n%s%s\r\nContent-Type: application/json\r\n\r\n",
													top+1, amp+5, OTA_HTTP_REQ_HOST, OTA_HTTP_REQ_X_API_KEY, AUDREY_API_KEY);
#if CONFIG_AUDREY_FWUP_ALONE
											}
#endif
										}
									} else {
										char *last;
										last = strstr(top,">");

										if (last) {
											*(last-1) = 0;
											DI("new url=%s", top+1);
#if CONFIG_AUDREY_FWUP_ALONE
											if(strncmp(cmd,"SAA",3) == 0 || strncmp(cmd,"SAS",3) == 0) {
												request_len = sprintf(request, "GET /%s HTTP/1.1\r\n%s%s\r\nContent-Type: text/plain\r\n\r\n",
													top+1, "Host: ", host);
											} else {
#endif
												request_len = sprintf(request, "GET /%s HTTP/1.1\r\n%s\r\n%s%s\r\nContent-Type: application/json\r\n\r\n",
													top+1, OTA_HTTP_REQ_HOST, OTA_HTTP_REQ_X_API_KEY, AUDREY_API_KEY);
#if CONFIG_AUDREY_FWUP_ALONE
											}
#endif
										}
									}

								}
							}

						}

						if (request_len > 0) {
							DI(" new request[%s]",request);
#if CONFIG_SSL_HTTPS_CLIENT
							// write CONFIG_USE_MBEDTLS
							while((ret = mbedtls_ssl_write(&ssl, request, strlen(request))) <= 0) {
								if(ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
									printf(" failed\n\r  ! mbedtls_ssl_write returned %d\n", ret);
									ret = OTA_UPDATE_STOP_AND_CONTINUE;
									goto update_ota_exit;
								}
							}
							if(ret < 0){
								Log_Error("\n\r[%s] Send HTTPS request failed ret=%d", __FUNCTION__, ret);
								SendMessageToStateManager(MSG_ERR_FWUPDATE, PARAM_FW_UPDATE_READ_ERROR);
								//ret = -1;
								ret = OTA_UPDATE_STOP_AND_CONTINUE;
								goto update_ota_exit;
							}
							if(ret < 0){
								Log_Error("\n\r[%s] Send HTTPS request failed ret=%d", __FUNCTION__, ret);
								//ret = -1;
								ret = OTA_UPDATE_STOP_AND_CONTINUE;
								goto update_ota_exit;
							}
							//len = ret;
							//printf(" %d bytes written\n\n%s", len, (char *) buf);

#else
							if (wait_write_select(server_fd)) {
												DE(" read time out");
												ret = -1;
												goto update_ota_exit;
											}
							ret = write(server_fd, request, strlen(request));
							if(ret < 0){
								Log_Error("\n\r[%s:%d] Send HTTP request failed ret=%d", __FUNCTION__, __LINE__,  ret);
								ret = -1;
								goto update_ota_exit;
							}
#endif
							rsp_result.parse_status = 0;
							continue;
						}
					} // (rsp_result.status_code == 301)
					Log_Error("\r\n[%s] parse_http_response error. rsp_result.status_code=%d ", __FUNCTION__,  rsp_result.status_code);
					ret = OTA_UPDATE_STOP_AND_CONTINUE;
					goto update_ota_exit;
				}
			}
			else if((1 == rsp_result.parse_status) || (3 == rsp_result.parse_status)){//just get the status code
				memset(buf, 0, BUF_SIZE);
				memcpy(buf, rsp_result.header_bak, HEADER_BAK_LEN);
				update_free(rsp_result.header_bak);
				rsp_result.header_bak = NULL;
#if CONFIG_SSL_HTTPS_CLIENT
				// read CONFIG_USE_MBEDTLS
				ret = mbedtls_ssl_read(&ssl, buf+ HEADER_BAK_LEN, (BUF_SIZE - HEADER_BAK_LEN));

				if(ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE)
					continue;

				if(ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
					DI(" MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY");
					break;
				}

				if(ret < 0) {
					DI(" failed\n  ! mbedtls_ssl_read returned %d\n", ret);
					SendMessageToStateManager(MSG_ERR_FWUPDATE, PARAM_FW_UPDATE_READ_ERROR);
					ret = OTA_UPDATE_STOP_AND_CONTINUE;
					goto update_ota_exit;
				}

				if(ret == 0) {
					DI("\n\nEOF\n\n");
					break;
				}

				read_bytes = ret;
				D(" %d bytes read\n\n%s", read_bytes, (char *) buf);

#else
				if (wait_read_select(server_fd)) {
					DE(" read time out");
					ret = -1;
					goto update_ota_exit;
				}
				read_bytes = read(server_fd, buf+ HEADER_BAK_LEN, (BUF_SIZE - HEADER_BAK_LEN));
#endif
				D("read:%dbytes", read_bytes);
				if(read_bytes == 0) {
					continue;
				}
				if(read_bytes < 0){
					Log_Error("\n\r[%s] Read socket failed ret=%d", __FUNCTION__, read_bytes);
					SendMessageToStateManager(MSG_ERR_FWUPDATE, PARAM_FW_UPDATE_READ_ERROR);
					//ret = -1;
					ret = OTA_UPDATE_STOP_AND_CONTINUE;
					goto update_ota_exit;
				}

				idx = read_bytes + HEADER_BAK_LEN;

				if(parse_http_response(buf, read_bytes + HEADER_BAK_LEN, &rsp_result) == -1){
					Log_Error("\n\r[%s] parse_http_response -1 ", __FUNCTION__);
					ret = -1;
					ret = OTA_UPDATE_STOP_AND_CONTINUE;
					goto update_ota_exit;
				}
			}
			else if(3 == rsp_result.parse_status){
				Log_Error("\n\r[%s] Get the content_length failed", __FUNCTION__);
				SendMessageToStateManager(MSG_ERR_FWUPDATE, PARAM_FW_UPDATE_GET_CONTENT_LEBGTH_ERROR);
				//ret = -1;
				ret = OTA_UPDATE_STOP_AND_CONTINUE;
				goto update_ota_exit;
			}
		}

		if (0 == rsp_result.body_len){
			Log_Error("\n\r[%s] New firmware size = 0 ERROR!", __FUNCTION__);
			SendMessageToStateManager(MSG_ERR_FWUPDATE, PARAM_FW_UPDATE_IMG_SIZE_ZERO);
			//ret = -1;
			ret = OTA_UPDATE_STOP_AND_CONTINUE;
			goto update_ota_exit;
		}
		else {
			D(" Download http body begin, total size : %d\n\r",  rsp_result.body_len);
		}

		ret = 0;

		// ---------------------------------------------------------------

		if(!ota_update_task_run) {
			DI(" stop");
			ret = OTA_UPDATE_STOP_AND_CONTINUE;
			goto update_ota_exit;
		}
#ifdef WRITE_FLASH9000_AFTER_FILE_READ
		if (strncmp(cmd,"AM",2)==0 || strncmp(cmd,"SAA",3) == 0) {
			NewImg2Addr = update_ota_prepare_addr();
			if(NewImg2Addr == -1){
				ret = -1;
				goto update_ota_exit;
			}



			{ // signature check!!
				unsigned int OldAddr;
				uint32_t sig_readback0 = 0,sig_readback1 = 0;
				uint32_t sig_readback20 = 0,sig_readback21 = 0;

				DI("[CHECK] NewImg2Addr=0x%x", NewImg2Addr);
				device_mutex_lock(RT_DEV_LOCK_FLASH);
				flash_read_word(&flash_ota, NewImg2Addr + 8, &sig_readback0);
				flash_read_word(&flash_ota, NewImg2Addr + 12, &sig_readback1);
				device_mutex_unlock(RT_DEV_LOCK_FLASH);
				DI("[CHECK] NewImg2Addr=0x%x signature read  %x,%x", NewImg2Addr, sig_readback0, sig_readback1);

				if (NewImg2Addr == 0xb000) {
					OldAddr = 0x100000;
				}else {
					OldAddr = 0xb000;
				}

				device_mutex_lock(RT_DEV_LOCK_FLASH);
				flash_read_word(&flash_ota, OldAddr + 8, &sig_readback20);
				flash_read_word(&flash_ota, OldAddr + 12, &sig_readback21);
				device_mutex_unlock(RT_DEV_LOCK_FLASH);
				DI("[CHECK] OldAddr=0x%x signature read  %x,%x", OldAddr, sig_readback20, sig_readback21);

				if (sig_readback0 == 0x35393138 && sig_readback20 == 0x35393138) {
					DE(" #### Part1 and Part2 are the same signature 0x35393138 #####");
				}
			}
		} else {
			NewImg2Addr = (uint32_t)buf_sdram; // dummy
		}
		DI("NewImg2Addr=0x%x ota_update_task_run=%d", NewImg2Addr, ota_update_task_run);
#endif

		if (strncmp(cmd,"AM",2) == 0 || strncmp(cmd,"SAA",3) == 0) {

			check_ota_image();

#if SWAP_UPDATE
			NewImg2Addr = update_ota_swap_addr(rsp_result.body_len, NewImg2Addr);
			if(NewImg2Addr == -1){
				ret = -1;
				goto update_ota_exit;
			}

#endif
			NewImg2Len = update_ota_erase_upg_region(rsp_result.body_len, NewImg2Len, NewImg2Addr);
			if(NewImg2Len == -1){
				ret = -1;
				goto update_ota_exit;
			}

			check_ota_image();

#ifdef SIGNATURE_ERASE_BEFOR_WRITE
			{
				//erase signature in New Image 2 addr + 8 and + 12
				uint32_t sig_readback0 = 0,sig_readback1 = 0;
				DI("clear signature +++++++++++++++++ ");
				DI(" NewImg2Addr=0x%x", NewImg2Addr);
				device_mutex_lock(RT_DEV_LOCK_FLASH);
				flash_write_word(&flash_ota,NewImg2Addr + 8, 0xffffffff);
				flash_write_word(&flash_ota,NewImg2Addr + 12, 0xffffffff);
				flash_read_word(&flash_ota, NewImg2Addr + 8, &sig_readback0);
				flash_read_word(&flash_ota, NewImg2Addr + 12, &sig_readback1);
				device_mutex_unlock(RT_DEV_LOCK_FLASH);
				printf("\n\r[%s] NewImg2Addr=0x%x signature readback  %x,%x", __FUNCTION__ , NewImg2Addr, sig_readback0, sig_readback1);
			}
#endif

			check_ota_image();

#ifdef SIGNATURE_SET_BEFOR_WRITE
			{
			//Set signature in New Image 2 addr + 8 and + 12
			uint32_t sig_readback0 = 0,sig_readback1 = 0;

			DI(" NewImg2Addr=0x%x", NewImg2Addr);
			device_mutex_lock(RT_DEV_LOCK_FLASH);
			flash_write_word(&flash_ota,NewImg2Addr + 8, 0xffffffff);
			flash_write_word(&flash_ota,NewImg2Addr + 12, 0xffffffff);
			flash_read_word(&flash_ota, NewImg2Addr + 8, &sig_readback0);
			flash_read_word(&flash_ota, NewImg2Addr + 12, &sig_readback1);
			device_mutex_unlock(RT_DEV_LOCK_FLASH);
			DI(" NewImg2Addr=0x%x signature readback  %x,%x", NewImg2Addr, sig_readback0, sig_readback1);
#if SWAP_UPDATE
			if(OldImg2Addr != ~0x0){
				DI(" OldImg2Addr=0x%x", OldImg2Addr);
				device_mutex_lock(RT_DEV_LOCK_FLASH);
				flash_write_word(&flash_ota,OldImg2Addr + 8, 0x35393138);
				flash_write_word(&flash_ota,OldImg2Addr + 12, 0x31313738);
				flash_read_word(&flash_ota, OldImg2Addr + 8, &sig_readback0);
				flash_read_word(&flash_ota, OldImg2Addr + 12, &sig_readback1);
				device_mutex_unlock(RT_DEV_LOCK_FLASH);
				DI(" old=0x%x signature readback %x,%x", OldImg2Addr, sig_readback0, sig_readback1);
			}
#endif
			}
#endif
			check_ota_image();

			address = NewImg2Addr;

			D(" address=0x%x len=%d \n\r", address, NewImg2Len);

#ifdef CHANGE_IMG_0008
	data_0008_back = 0;
#endif
			//Write the body of http response into flash
			data_len = idx - rsp_result.header_len;
			if(data_len > 0){
				DI(" first read img data_len=%d", data_len);

#ifdef CHANGE_IMG_0008
				if (data_len > 9) {
					DI("signature = %.8s", &buf[rsp_result.header_len + 8]);
					data_0008_back = buf[rsp_result.header_len + 8];
					if (data_0008_back == 0x38) {
						buf[rsp_result.header_len + 8] = 0x30;
						DI("CHANGE_IMG_0008 :  0x38-->0x30");
					}
				}
#endif
				device_mutex_lock(RT_DEV_LOCK_FLASH);
				if(flash_stream_write(&flash_ota, address, data_len, (buf+rsp_result.header_len)) < 0){
					Log_Error("\n\r[%s] Write sector failed", __FUNCTION__);
					device_mutex_unlock(RT_DEV_LOCK_FLASH);
					SendMessageToStateManager(MSG_ERR_FWUPDATE, PARAM_FW_UPDATE_FRASH_WRITE_ERROR);
					ret = -1;
					goto update_ota_exit;
				}
				device_mutex_unlock(RT_DEV_LOCK_FLASH);
			}

			Log_Always("\r\n FW image download flash_address=0x%x size=%d \n\r", address, NewImg2Len);
			wdt_task_refresh(WDT_TASK_FWUP);
			idx = 0;
			idx += data_len;
			while (idx < NewImg2Len && ota_update_task_run){
				if (DEBUG_LEVEL <= 1) {
					Log_Always(".");
				} else {
					DI("read idx=%d", idx);
				}
				data_len = NewImg2Len - idx;
				if(data_len > BUF_SIZE) {
					data_len = BUF_SIZE;
				}

				memset(buf, 0, BUF_SIZE);
#if CONFIG_SSL_HTTPS_CLIENT

				// read CONFIG_USE_MBEDTLS
				ret = mbedtls_ssl_read(&ssl,  buf, data_len);

				if(ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE)
					continue;

				if(ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY)
					break;

				if(ret < 0) {
					printf(" failed\n  ! mbedtls_ssl_read returned %d\n", ret);
					break;
				}

				if(ret == 0) { //[TODO] continue ?

					printf("\n\nEOF\n\n");
					break;
				}

				read_bytes = ret;
				//printf(" %d bytes read\n\n%s", read_bytes, (char *) buf);

#else
				if (wait_read_select(server_fd)) {
					DE(" read time out");
					ret = -1;
					goto update_ota_exit;
				}
				read_bytes = read(server_fd, buf, data_len);
#endif
				if(read_bytes == 0) {
					//continue;
					DI(" file end");
					break; // Read end
				}
				if(read_bytes < 0){
					Log_Error("\n\r[%s] Read socket failed", __FUNCTION__);
					SendMessageToStateManager(MSG_ERR_FWUPDATE, PARAM_FW_UPDATE_READ_ERROR);
					//ret = -1;
					ret = OTA_UPDATE_STOP_AND_CONTINUE;
					goto update_ota_exit;
				}

				if(read_bytes<4) {
					Log_Error("\n\r[%s] Recv small packet", __FUNCTION__);
					SendMessageToStateManager(MSG_ERR_FWUPDATE, PARAM_FW_UPDATE_RECV_SMALL_PACKET);
				}

#ifdef CHANGE_IMG_0008
				if (idx <= 8 && (idx+read_bytes) > 9) {
					DI("idx = %d signature = %.8s", idx, &buf[8 - idx]);
					data_0008_back = buf[8 - idx];
					if (data_0008_back == 0x38) {
						buf[8 - idx] = 0x30;
						DI("CHANGE_IMG_0008 :  0x38-->0x%x", buf[8 - idx]);
					}
				}
#endif
				device_mutex_lock(RT_DEV_LOCK_FLASH);
				if(flash_stream_write(&flash_ota, address + idx, read_bytes, buf) < 0){
					Log_Error("\n\r[%s] Write sector failed", __FUNCTION__);
					device_mutex_unlock(RT_DEV_LOCK_FLASH);
					SendMessageToStateManager(MSG_ERR_FWUPDATE, PARAM_FW_UPDATE_FRASH_WRITE_ERROR);
					ret = -1;
					goto update_ota_exit;
				}
				device_mutex_unlock(RT_DEV_LOCK_FLASH);

				file_checksum->c[0] = buf[read_bytes-4];      // checksum attached at file end
				file_checksum->c[1] = buf[read_bytes-3];
				file_checksum->c[2] = buf[read_bytes-2];
				file_checksum->c[3] = buf[read_bytes-1];

#ifdef CONFIG_CRC
				file_crc.u = 0;
				file_crc.c[0] = buf[read_bytes-8];			// crc attached at file end
				file_crc.c[1] = buf[read_bytes-7];
				file_crc.c[2] = buf[read_bytes-6];
				file_crc.c[3] = buf[read_bytes-5];
#endif /* CONFIG_CRC */

				idx += read_bytes;

				wdt_task_refresh(WDT_TASK_FWUP);
			}

			ret = 0;
			DI(" Download new firmware %d bytes completed\n\r", idx);

			// read flash data back and calculate checksum
			for(i = 0; i < idx-4; i += BUF_SIZE){
				int k;
				int rlen = (idx-4-i)>BUF_SIZE?BUF_SIZE:(idx-4-i);
				device_mutex_lock(RT_DEV_LOCK_FLASH);
				flash_stream_read(&flash_ota, NewImg2Addr+i, rlen, buf);
				device_mutex_unlock(RT_DEV_LOCK_FLASH);
				for(k = 0; k < rlen; k++) {
					flash_checksum+=buf[k];
				}
			}

#ifdef CONFIG_CRC
			// read flash data back and calculate crc
			for (uint32_t i = 0; i < 256; i++) {
				uint32_t c = i;
				for (int j = 0; j < 8; j++) {
					c = (c & 1) ? (0xEDB88320 ^ (c >> 1)) : (c >> 1);
				}
				crc_table[i] = c;
			}
			flash_crc = 0xFFFFFFFF;
			for(i = 0; i < idx-8; i += BUF_SIZE){
				int k;
				int rlen = (idx-8-i)>BUF_SIZE?BUF_SIZE:(idx-8-i);
				device_mutex_lock(RT_DEV_LOCK_FLASH);
				flash_stream_read(&flash_ota, NewImg2Addr+i, rlen, buf);
				device_mutex_unlock(RT_DEV_LOCK_FLASH);
				for(k = 0; k < rlen; k++) {
					flash_crc = crc_table[(flash_crc ^ (uint32_t)buf[k]) & 0xFF] ^ (flash_crc >> 8);
				}
			}
			flash_crc ^= 0xFFFFFFFF;
#endif /* CONFIG_CRC */

#ifdef CHANGE_IMG_0008
			if (data_0008_back == 0x38) {
				file_checksum->u -= (0x38 - 0x30);
				DI(" change file_checksum");
			}
			DI(" file_checksum=%d flash_checksum=%d", file_checksum->u, flash_checksum);
#endif

			D("NewImg2Addr=0x%x len=%d", NewImg2Addr, idx-4);
			Log_Always("\r\nameba update flash_checksum 0x%x  file_checksum->u 0x%x \r\n", flash_checksum, file_checksum->u);
#ifdef CONFIG_CRC
			Log_Always("\r\nameba update flash_crc 0x%x  file_crc.u 0x%x \r\n", flash_crc, file_crc.u);
#endif /* CONFIG_CRC */

			check_ota_image();

			if ((strncmp(cmd,"AMU",3) == 0 || strncmp(cmd,"SAA",3) == 0) && ota_update_task_run) {

				vTaskDelay(1000);

#ifdef OTA_HTTP_FLASH_WRITE_ENABLE
#ifdef SIGNATURE_WRITE_DIRECT
#ifdef SIGNATURE_WRITE_DIRECT_CMD_OTA_IMAGE
				// cmd_ota_imageを使って書き込む
				// OK!

				if(idx != NewImg2Len) {
					Log_Error("\n\r[%s] Length is wrong! idx:%d, len:%d\n\r", __FUNCTION__, idx, NewImg2Len);
					SendMessageToStateManager(MSG_ERR_FWUPDATE, PARAM_FW_UPDATE_LENGTH_ERROR);
					//ret = -1;
					ret = OTA_UPDATE_STOP_AND_CONTINUE;
					goto update_ota_exit;
#ifdef CONFIG_CRC
				} else if ((check_crc_fw_version() == TRUE) && (file_crc.u != flash_crc)) {
					Log_Error("\n\r[%s] The crc is wrong!\n\r", __FUNCTION__);
					SendMessageToStateManager(MSG_ERR_FWUPDATE, PARAM_FW_UPDATE_CRC_ERROR);
					//ret = -1;
					ret = OTA_UPDATE_STOP_AND_CONTINUE;
					goto update_ota_exit;
				} else if ((check_crc_fw_version() == FALSE) && (file_checksum->u != flash_checksum)) {
					Log_Error("\n\r[%s] The checksume is wrong!\n\r", __FUNCTION__);
					SendMessageToStateManager(MSG_ERR_FWUPDATE, PARAM_FW_UPDATE_CHECKSUM_ERROR);
					//ret = -1;
					ret = OTA_UPDATE_STOP_AND_CONTINUE;
					goto update_ota_exit;
#else /* checksum */
				} else if(file_checksum->u != flash_checksum) {
					Log_Error("\n\r[%s] The checksume is wrong!\n\r", __FUNCTION__);
					SendMessageToStateManager(MSG_ERR_FWUPDATE, PARAM_FW_UPDATE_CHECKSUM_ERROR);
					//ret = -1;
					ret = OTA_UPDATE_STOP_AND_CONTINUE;
					goto update_ota_exit;
#endif /* CONFIG_CRC */
				} else {
					uint32_t sig_readback0 = 0,sig_readback1 = 0;
					if (NewImg2Addr == 0xb000) {
						cmd_ota_image_set(0);
					}else {
						cmd_ota_image_set(1);
					}
					DI(" NewImg2Addr=0x%x", NewImg2Addr);
					device_mutex_lock(RT_DEV_LOCK_FLASH);
					flash_read_word(&flash_ota, NewImg2Addr + 8, &sig_readback0);
					flash_read_word(&flash_ota, NewImg2Addr + 12, &sig_readback1);
					device_mutex_unlock(RT_DEV_LOCK_FLASH);
					DI(" NewImg2Addr=0x%x signature readback  %x,%x", NewImg2Addr, sig_readback0, sig_readback1);
					if (sig_readback0 != 0x35393138) {
						DE(" signature write error");
						return -1;
					}
					if (NewImg2Addr == 0xb000) {
						cmd_ota_image_unset(0);
					}else {
						cmd_ota_image_unset(1);
					}
					if(OldImg2Addr != ~0x0){
						device_mutex_lock(RT_DEV_LOCK_FLASH);
						flash_read_word(&flash_ota, OldImg2Addr + 8, &sig_readback0);
						flash_read_word(&flash_ota, OldImg2Addr + 12, &sig_readback1);
						device_mutex_unlock(RT_DEV_LOCK_FLASH);
						DI(" OldImg2Addr=0x%x signature readback  %x,%x", OldImg2Addr, sig_readback0, sig_readback1);
					}
					ret = 0;
					ota_update_ameba_done = 1;
					Log_Always("\r\n FW update new firmware %d bytes completed\n\r", idx);
				}

#else //SIGNATURE_WRITE_DIRECT_CMD_OTA_IMAGE
				// update_ota_checksum()がうまくいかないので　直接ここで書き換える
				// これもうまく書けない
				//
				check_ota_image();
				if(file_checksum->u == flash_checksum && idx == NewImg2Len)
				{
					//Set signature in New Image 2 addr + 8 and + 12
					uint32_t sig_readback0 = 0,sig_readback1 = 0;

					DI(" NewImg2Addr=0x%x", NewImg2Addr);
					device_mutex_lock(RT_DEV_LOCK_FLASH);
					flash_write_word(&flash_ota,NewImg2Addr + 8, 0x35393138);
					flash_write_word(&flash_ota,NewImg2Addr + 12, 0x31313738);
					flash_read_word(&flash_ota, NewImg2Addr + 8, &sig_readback0);
					flash_read_word(&flash_ota, NewImg2Addr + 12, &sig_readback1);
					device_mutex_unlock(RT_DEV_LOCK_FLASH);
					DI(" NewImg2Addr=0x%x signature readback  %x,%x", NewImg2Addr, sig_readback0, sig_readback1);
					if (sig_readback0 != 0x35393138) {
						DE(" signature write error");
						return -1;
					}
#if SWAP_UPDATE
					if(OldImg2Addr != ~0x0){
						DI(" OldImg2Addr=0x%x", OldImg2Addr);
						device_mutex_lock(RT_DEV_LOCK_FLASH);
						flash_write_word(&flash_ota,OldImg2Addr + 8, 0x35393130);
						flash_write_word(&flash_ota,OldImg2Addr + 12, 0x31313738);
						flash_read_word(&flash_ota, OldImg2Addr + 8, &sig_readback0);
						flash_read_word(&flash_ota, OldImg2Addr + 12, &sig_readback1);
						device_mutex_unlock(RT_DEV_LOCK_FLASH);
						DI(" old=0x%x signature readback %x,%x", OldImg2Addr, sig_readback0, sig_readback1);
					}
#endif // SWAP_UPDATE
				}
#endif // SIGNATURE_WRITE_DIRECT_CMD_OTA_IMAGE
#else // SIGNATURE_WRITE_DIRECT
				// これもうまく書けない
				//　+8からの 81958711 が書けない 書いても変化しない
				//
				ret = update_ota_checksum(file_checksum, flash_checksum, NewImg2Addr);

				if(ret == -1){
					Log_Error("\n\r[%s] The checksume is wrong!\n\r", __FUNCTION__);
					ret = -1;
					goto update_ota_exit;
				}
#endif // SIGNATURE_WRITE_DIRECT
				check_ota_image();
#else // OTA_HTTP_FLASH_WRITE_ENABLE
				D("update  test ..... SKIP flash signature write  \n\r");
#endif
			}

		// ----------------------------------------AM-------------------------------------
		}  else if (strncmp(cmd,"SC",2)==0 || strncmp(cmd,"SAS",3)==0) {

			address = (uint32_t)buf_sdram;
			NewImg2Len = rsp_result.body_len;

			//Write the body of http response into flash
			data_len = idx - rsp_result.header_len;
			if(data_len > 0){
				file_checksum->c[0] = buf[data_len-4];	  // checksum attached at file end
				file_checksum->c[1] = buf[data_len-3];
				file_checksum->c[2] = buf[data_len-2];
				file_checksum->c[3] = buf[data_len-1];

				memcpy(address, (buf+rsp_result.header_len), data_len);
			}

			idx = 0;
			idx += data_len;
			while (idx < NewImg2Len && ota_update_task_run){
				if (DEBUG_LEVEL <= 1) {
					Log_Always(".");
				} else {
					DI("read idx=%d", idx);
				}

				data_len = NewImg2Len - idx;
				if(data_len > BUF_SIZE) {
					data_len = BUF_SIZE;
				}

				memset(buf, 0, BUF_SIZE);
#if CONFIG_SSL_HTTPS_CLIENT

				// read CONFIG_USE_MBEDTLS
				ret = mbedtls_ssl_read(&ssl,  buf, data_len);

				if(ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE)
					continue;

				if(ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY)
					break;

				if(ret < 0) {
					printf(" failed\n  ! mbedtls_ssl_read returned %d\n", ret);
					ret = OTA_UPDATE_STOP_AND_CONTINUE;
					goto update_ota_exit;
				}

				if(ret == 0) { //[TODO] continue ?

					printf("\n\nEOF\n\n");
					break;
				}

				read_bytes = ret;
				//printf(" %d bytes read\n\n%s", read_bytes, (char *) buf);

#else
				if (wait_read_select(server_fd)) {
					DE(" read time out");
					ret = -1;
					goto update_ota_exit;
				}
				read_bytes = read(server_fd, buf, data_len);
#endif
				//Log_Always("\r\n idx=%d data_len=%d read_bytes=%d", idx, data_len, read_bytes);
				if(read_bytes == 0) {
					//continue;
					DI(" file end");
					break; // Read end
				}
				if(read_bytes < 0){
					Log_Error("\n\r[%s] Read socket failed", __FUNCTION__);
					SendMessageToStateManager(MSG_ERR_FWUPDATE, PARAM_FW_UPDATE_READ_ERROR);
					//ret = -1;
					ret = OTA_UPDATE_STOP_AND_CONTINUE;
					goto update_ota_exit;
				}

				if(read_bytes<4) {
					Log_Error("\n\r[%s] Recv small packet", __FUNCTION__);
					SendMessageToStateManager(MSG_ERR_FWUPDATE, PARAM_FW_UPDATE_RECV_SMALL_PACKET);
				}

				memcpy(address + idx, buf, read_bytes);

				file_checksum->c[0] = buf[read_bytes-4];      // checksum attached at file end
				file_checksum->c[1] = buf[read_bytes-3];
				file_checksum->c[2] = buf[read_bytes-2];
				file_checksum->c[3] = buf[read_bytes-1];

				idx += read_bytes;
			}
			ret = 0;
			D(" Download new scale firmware %d bytes completed\n\r", idx);

			// read flash data back and calculate checksum
			for(i = 0; i < idx-4; i++){
					flash_checksum+=buf_sdram[i];
			}

			// checking check sum
			DI("\r\nscale update checksum file=0x%x sdram=0x%x \n\r", file_checksum->u, flash_checksum);
			if ((strncmp(cmd,"SCU",3) == 0 || strncmp(cmd,"SAS",3) == 0) &&  flash_checksum == file_checksum->u) {
				int sret;
				// scale fw update
	#ifdef OTA_HTTP_FLASH_WRITE_ENABLE
				 extern int scale_up_start(u8 *, u16);
				 DI("\n\r[%s] scale update start\n\r", __FUNCTION__);
				 sret = scale_up_start(buf_sdram, idx-4);
				 DI("\n\r[%s] scale update end ret=%d\n\r", __FUNCTION__, sret);
				 ret = sret;
				 ota_update_scale_done = 1;
	#else
				 Log_Always("\n\r[%s] scale update SKIP ...... \n\r", __FUNCTION__);
	#endif

			}




		// ---------------------------------------------- SC -------------------------------
		} else if (strncmp(cmd,"VC",2) == 0 || strncmp(cmd,"VU",2) == 0) {

			data_len = idx - rsp_result.header_len;
			if(data_len > 0){
				// body
				D(" body [%s] \n\r", (buf+rsp_result.header_len));
				memset(g_version,0,G_VERSION_SIZE);
				handle_json_get_version((buf+rsp_result.header_len), g_version);

				ret = 0;

				for (i=0; i<8 && g_version[i]!='_' && g_version[i] != 0 && i < 15; i++) {
							am_version[i] = g_version[i];
				}
				if (i >= 16) {
					Log_Error("\n\r[%s] version string over ", __FUNCTION__);
					SendMessageToStateManager(MSG_ERR_FWUPDATE, PARAM_FW_UPDATE_VERSION_ERROR);
					ret = -1;
				}
				if (g_version[i] == '_') {
					strncpy(scale_version, &g_version[i+1],15);
				} else {
					ret = -1;
				}

				am_version[i] = 0;

				DI("\r\n my     wireless_version=%s scale_version=%s", AUDREY_VERSION, scale_ver);
				DI("\r\n server wireless_version=%s scale_version=%s ### NEW ###", am_version, scale_version);

			}

#ifdef DEBUG_ONLY_ENABLE
		// -----------------------------------------------VU ----------------------------------
		} else if (strncmp(cmd,"TEST",4) == 0) {

			data_len = idx - rsp_result.header_len;
			if(data_len > 0){
				// body
				D(" body [%s] \n\r", (buf+rsp_result.header_len));
				memset(g_response_data,0,G_RESPONSE_DATA_SIZE);
				handle_json_get_data((buf+rsp_result.header_len), g_response_data);
				// json  data: { ... }
				D(" data [%s] \n\r", g_response_data);

				memset(g_version,0,G_VERSION_SIZE);
				handle_json_get_version(g_response_data, g_version);

				ret = 0;

				for (i=0; i<8 && g_version[i]!='_' && g_version[i] != 0 && i < 15; i++) {
							am_version[i] = g_version[i];
				}
				if (i >= 16) {
					Log_Error("\n\r[%s] version string over ", __FUNCTION__);
					ret = -1;
				}
				if (g_version[i] == '_') {
					strncpy(scale_version, &g_version[i+1],15);
				} else {
					ret = -1;
				}

				am_version[i] = 0;

				DI("\r\n my fw_version=%s scale_version=%s", AUDREY_VERSION, scale_ver);
				DI("\r\n server fw_version=%s scale_version=%s", am_version, scale_version);


				ret = (strlen(g_response_data));

			} else {
				ret = 0;
			}
#endif

		// -----------------------------------------------TEST ----------------------------------
		}

	}

update_ota_exit:
	DI(" exit  ");
#ifndef  BUF_USE_SDRAM
	if(alloc)
		update_free(alloc);
	if(request)
		update_free(request);
#endif

#if CONFIG_SSL_HTTPS_CLIENT
	// use https

	//CONFIG_USE_MBEDTLS
	mbedtls_ssl_close_notify(&ssl);

#ifdef MBEDTLS_ERROR_C
	if(ret != 0) {
		char error_buf[100];
		mbedtls_strerror(ret, error_buf, 100);
		printf("\n\rLast error was: %d - %s\n", ret, error_buf);
	}
#endif

	mbedtls_net_free(&server_fd);
	mbedtls_ssl_free(&ssl);
	mbedtls_ssl_config_free(&conf);

#ifdef SSL_CLIENT_EXT
#if CONFIG_AUDREY_FWUP_ALONE
	if(strncmp(cmd,"SAA",3) != 0 && strncmp(cmd,"SAS",3) != 0) {
#endif
		ssl_client_ext_free();
#if CONFIG_AUDREY_FWUP_ALONE
	}
#endif
#endif

#else //CONFIG_SSL_HTTPS_CLIENT
	// use http
	if(server_fd >= 0) {
		close(server_fd);
	}
#endif //CONFIG_SSL_HTTPS_CLIENT


update_ota_exit1:
	DI("rtw_getFreeHeapSize = %d", rtw_getFreeHeapSize());
	if (ota_update_task_stop > 0) {
		DI("ota_update_task_stop=%d ret=%d", ota_update_task_stop, ret);
	}
	wdt_task_refresh(WDT_TASK_FWUP);
	D(" end ret(%d)\r\n", ret);
	return ret;
}

// test only
int do_get_ota_version(char *out_scale_version, char *out_am_version)
{
	int i;
	int ret = 0;

	ret = ota_http_client_main("VU", OTA_HTTP_HOST, OTA_HTTP_PORT, OTA_HTTP_VER_RESOURCE, NULL);
	if (ret) {
		goto exit;
	}

	for (i=0; i<8 && g_version[i]!='_' && g_version[i] != 0 && i < 15; i++) {
				am_version[i] = g_version[i];
	}
	if (i >= 16) {
		Log_Error("\n\r[%s] version string over ", __FUNCTION__);
		ret = -1;
	}
	if (g_version[i] == '_') {
		strncpy(scale_version, &g_version[i+1],15);
	} else {
		ret = -1;
	}

	am_version[i] = 0;
	if (strlen(am_version)<4) {
		ret = -1;
	}
	strcpy(out_am_version,am_version);

	if (strlen(scale_version)<4) {
				ret = -1;
	}
	if (ret) {
		Log_Error("\r\n version get error");
		SendMessageToStateManager(MSG_ERR_FWUPDATE, PARAM_FW_UPDATE_VERSION_ERROR);
		goto exit;
	}
	strcpy(out_scale_version, scale_version);

	DI("\r\nserver am_version=%s scale_version=%s", am_version, scale_version);
exit:
	return ret;
}

void http_update_ota_task(void *vparam){
	struct http_task_param  *param;

	int ret = -1;
#ifdef TASK_PARAM_USE_SH_HOST
	param = &sh_host;
#else
	param = (struct http_task_param *)vparam;
#endif
	DI(" task start\n\r");
	D("param %s, %s, %d, %s\n\r", param->cmd, param->host, param->port, param->resource);

	wdt_task_watch(WDT_TASK_FWUP, WDT_TASK_WATCH_START);

	if (strncmp(param->cmd,"ALL",3) == 0) {
		// all
		int i;

#if 1
		// テスト用サーバーのみの暫定対応

		if (!set_new_version) {
			ret = ota_http_client_main("VU", OTA_HTTP_HOST, OTA_HTTP_PORT, OTA_HTTP_VER_RESOURCE,NULL);
			if (ret) {
				goto exit;
			}

			for (i=0; i<8 && g_version[i]!='_' && g_version[i] != 0 && i < 15; i++) {
						am_version[i] = g_version[i];
			}
			if (i >= 16) {
				Log_Error("\n\r[%s] version string over ", __FUNCTION__);
				SendMessageToStateManager(MSG_ERR_FWUPDATE, PARAM_FW_UPDATE_VERSION_ERROR);
				ret = -1;
			}
			if (g_version[i] == '_') {
				strncpy(scale_version, &g_version[i+1],15);
			} else {
				Log_Error("\n\r[%s] version string separater error ", __FUNCTION__);
				SendMessageToStateManager(MSG_ERR_FWUPDATE, PARAM_FW_UPDATE_VERSION_ERROR);
				ret = -1;
			}
			am_version[i] = 0;
		} else
		{
			DI("new version scale=%s am=%s", scale_version, am_version);
			ret = 0;
		}

#endif

		DI("\r\nserver scale_version=%s am_version=%s ",scale_version,  am_version);
#if 1
		if ((strlen(scale_ver) > 0 && strcmp(scale_version, scale_ver)) != 0 || strcmp("ERROR", scale_ver) == 0) {
			DI("\n\r[%s] Scale verstion is different. new[%s] : old[%s] ", __FUNCTION__, scale_version, scale_ver);

#if 1
			// ##################################################################################
			// 暫定　安全策　古いscale基板には焼かない
			//     焼く場合には ATコマンドにて  ATAU=VU  ATAU=SCU とする
			// ##################################################################################
			if (strcmp(scale_ver,"1.02")==0 || strcmp(scale_ver,"1.03")==0) {
				Log_Always("\r\n ########## SKIP scale update ##############################");
				Log_Always("\r\n scale version is %s ", scale_ver);
				Log_Always("\r\n force update is manual  AT command.   ATAU=VU and  ATAU=SCU");
			} else
#endif
			{
				// scale update
				if (strcmp(scale_version,"ERROR") == 0) {
					DI(" scale_version is ERROR,  clear scale_version");
					strcpy(scale_version,"");
				}
				sprintf(server_resource,"%s%s"OTA_HTTP_FILE_TYPE,OTA_HTTP_SCALE_RESOURCE,scale_version);
				ret = ota_http_client_main("SCU", OTA_HTTP_HOST, OTA_HTTP_PORT, server_resource,NULL);
				if (ret) {
					Log_Error("\r\n[%s] scale download error(%d)", __FUNCTION__, ret);
					SendMessageToStateManager(MSG_ERR_FWUPDATE, PARAM_FW_UPDATE_SCALE_DOWNLOAD_ERROR);
					goto exit;
				}
				DI("\r\nwait 6S"); //[TODO] wait scale update
				vTaskDelay(6*1000);
			}
		} else {
			DI("\n\r[%s] Scale FW is latest version %s  ", __FUNCTION__,  scale_ver);
			// none
		}

		if (strcmp(am_version,AUDREY_VERSION) != 0) {
			DI("\n\r[%s] Audrey verstion is different. new[%s] : old[%s] ", __FUNCTION__, am_version, AUDREY_VERSION);
			// ameba update
			sprintf(server_resource,"%s%s"OTA_HTTP_FILE_TYPE,OTA_HTTP_WIRELESS_RESOURCE,am_version);
			ret = ota_http_client_main("AMU", OTA_HTTP_HOST, OTA_HTTP_PORT, server_resource,NULL);
			if (ret) {
				Log_Error("\r\n[%s] wireless download error(%d)", __FUNCTION__, ret);
				SendMessageToStateManager(MSG_ERR_FWUPDATE, PARAM_FW_UPDATE_WIRELESS_DOWNLOAD_ERROR);
				goto exit;
			}
		} else {
			Log_Always("\n\r[%s] Audrey FW is latest version %s  ", __FUNCTION__,  AUDREY_VERSION);
			// none
		}
#else
		// ---------------- test-------------------------------------------
			Log_Error("\r\n test  ......  force update\r\n");
			sprintf(server_resource,"%s/%s"OTA_HTTP_FILE_TYPE,OTA_HTTP_SCALE_RESOURCE,scale_version);
			ret = ota_http_client_main("SCU", OTA_HTTP_HOST, OTA_HTTP_PORT, server_resource,NULL);
			if (ret) {
				Log_Error("\r\n[%s] scale download error(%d)", __FUNCTION__, ret);
			}
			Log_Error("\r\nwait 6S"); //[TODO] wait scale update
			vTaskDelay(6*1000);

			sprintf(server_resource,"%s/%s"OTA_HTTP_FILE_TYPE,OTA_HTTP_WIRELESS_RESOURCE,am_version);
			ret = ota_http_client_main("AMU", OTA_HTTP_HOST, OTA_HTTP_PORT, server_resource,NULL);
			if (ret) {
				Log_Error("\r\n[%s] ameba download error(%d)", __FUNCTION__, ret);
			}
#endif

	} else {
		ret = ota_http_client_main(param->cmd, param->host, param->port, param->resource, NULL);
	}

exit:
	DI(" task end ret=%d\r\n",  ret);

#if CONFIG_AUDREY_FWUP_ALONE
	if (strcmp(param->cmd,"SAA") == 0 || strcmp(param->cmd,"SAS") == 0) {
		if(ret != 0){
			SendMessageToStateManager(MSG_FW_UPDATE_SA_FAIL, (strcmp(param->cmd,"SAS") == 0 ? 0 : 1));
			DI(" update fail\r\n");
		} else {
			SendMessageToStateManager(MSG_FW_UPDATE_SA_OK, (strcmp(param->cmd,"SAS") == 0 ? 0 : 1));
			DI(" update OK!\r\n");
		}
	} else
#endif
	if (strcmp(param->cmd,"ALLM") == 0) {
		if (ota_update_task_stop == 0){
				// reset
				if(ret == 0){
					//send ok message
					DI(" update OK!\r\n");
					SendMessageToStateManager(MSG_OTA_RESET, 0);

				} else if (ret == OTA_UPDATE_STOP_AND_CONTINUE) {
					DI(" update stop \r\n");
					SendMessageToStateManager(MSG_FW_UPDATE_STOP, 0);
				} else if (ret == OTA_UPDATE_CONNECT_AGEIN) {
					DI(" update stop \r\n");
					SendMessageToStateManager(MSG_FW_UPDATE_AGEIN, 0);
				} else {
					//send error
					Log_Error("\r\n[%s] WLAN FW update error ret(%d)", __FUNCTION__, ret);
					SendMessageToStateManager(MSG_ERR_WIFI_UPDATE, 0);
				}

		} else {
			// STOP
			if (ota_update_scale_done > 0 || ota_update_ameba_done > 0) {
				// reset
				DI("already updated!");
				if(!ret){
					//send ok message
					SendMessageToStateManager(MSG_OTA_RESET, 0);

				} else {
					//send error
					Log_Error("\r\n[%s] WLAN FW update error ret(%d)", __FUNCTION__, ret);
					SendMessageToStateManager(MSG_ERR_WIFI_UPDATE, 0);
				}
			} else {
				// continue MSG_ERR_FWUPDATE
				DI(" stop\r\n");
				SendMessageToStateManager(MSG_FW_UPDATE_STOP, 0);
			}
		}
	}

#ifndef TASK_PARAM_USE_SH_HOST
	if(vparam) {
		update_free(vparam);
	}
#endif

#if defined(INCLUDE_uxTaskGetStackHighWaterMark) && (INCLUDE_uxTaskGetStackHighWaterMark == 1)
		printf("\n\rMin available stack size of %s = %d * %d bytes\n\r", __FUNCTION__, uxTaskGetStackHighWaterMark(NULL), sizeof(portBASE_TYPE));
#endif

	wdt_task_watch(WDT_TASK_FWUP, WDT_TASK_WATCH_NONE);

	TaskOTA = NULL;
	ota_update_task_run = 0;

	vTaskDelete(NULL);

}


void update_ota_http(char *cmd, char *ip, uint32_t port, char *resource)
{
	struct http_task_param *param;
	D(" start ");
	if(TaskOTA){
		Log_Error("\n\r[%s] Update task has already created.\r\n", __FUNCTION__);
		//send error
		SendMessageToStateManager(MSG_ERR_FWUPDATE, PARAM_FW_UPDATE_ALREADY_RUNNING);
		return;
	}
#ifdef TASK_PARAM_USE_SH_HOST
	strcpy(sh_host.cmd, cmd);
	strcpy(sh_host.host, ip);
	strcpy(sh_host.resource,resource);
	sh_host.port = port;
	D(" sh_host %s %s,%d,%s,%.10s\n\r", sh_host.cmd, sh_host.host, sh_host.port, sh_host.resource, sh_host.post);
	if(xTaskCreate(http_update_ota_task, (char const *)"http_update_ota_task", 2048, NULL, tskIDLE_PRIORITY + 3, &TaskOTA) != pdPASS){
		Log_Error("\n\r[%s] Create http_update_ota_task task failed", __FUNCTION__);
		//send error
		SendMessageToStateManager(MSG_ERR_WIFI_UPDATE, 0);
	}
#else
	param = update_malloc(sizeof(param));
	if(!param){
		Log_Error("\n\r[%s] Alloc buffer failed", __FUNCTION__);
		//send error
		SendMessageToStateManager(MSG_ERR_WIFI, MSG_ERR_WIFI_UPDATE);
		goto update_ota_http_exit;
	}
	strcpy(param->host, ip);
	strcpy(param->resource,resource);
	param->port = port;
	if(xTaskCreate(http_update_ota_task, (char const *)"http_update_ota_task", 1024, param, tskIDLE_PRIORITY + 1, &TaskOTA) != pdPASS){
		Log_Error("\n\r[%s] Create update_ota_http task failed", __FUNCTION__);
		//send error
		SendMessageToStateManager(MSG_ERR_WIFI_UPDATE, PARAM_FW_UPDATE_WIRELESS_TASK_CREAT_ERROR);
	}
#endif

update_ota_http_exit:
	D(" end \r\n");
}

#ifdef DEBUG_ONLY_ENABLE
// --------------------------------- TEST TEST TEST ----------------------------
static int test_http_upload_task_run = 0;
static void test_http_upload_task(void *vparam){
	struct http_task_param  *param;

	int ret = -1;
#ifdef TASK_PARAM_USE_SH_HOST
	param = &sh_host;
#else
	param = (struct http_task_param *)vparam;
#endif

	test_http_upload_task_run = 2;

	DI(" task start\n\r");
	D("param %s, %s, %d, %s\n\r", param->cmd, param->host, param->port, param->resource);

	ret = ota_http_client_main("TEST", OTA_HTTP_HOST, OTA_HTTP_PORT, OTA_HTTP_VER_RESOURCE,param->post);
	if (ret > 0) {
		D(" out=[%s]", g_response_data);
		strcpy(param->out, g_response_data);
	}


exit:
	DI(" task end ret=%d\r\n", ret);

#ifndef TASK_PARAM_USE_SH_HOST
	if(vparam) {
		update_free(vparam);
	}
#endif

	test_http_upload_task_run = 0;
	vTaskDelete(NULL);

}


static void test_http_upload(char *cmd, char *ip, uint32_t port, char *resource, char *post, char *out)
{
	struct http_task_param *param;
	D(" start ");
#ifdef TASK_PARAM_USE_SH_HOST
	strcpy(sh_host.cmd, cmd);
	strcpy(sh_host.host, ip);
	strcpy(sh_host.resource,resource);
	sh_host.port = port;
	sh_host.post =post;
	sh_host.out = out;
	D(" sh_host %s %s,%d,%s,%.10s\n\r", sh_host.cmd, sh_host.host, sh_host.port, sh_host.resource, sh_host.post);
	if(xTaskCreate(test_http_upload_task, (char const *)"test_up_task", 1024, NULL, tskIDLE_PRIORITY + 1, NULL) != pdPASS){
		Log_Error("\n\r[%s] Create test_http_upload_task task failed", __FUNCTION__);
		SendMessageToStateManager(MSG_ERR_WIFI_UPDATE, PARAM_FW_UPDATE_WIRELESS_TASK_CREAT_ERROR);
	}
#else
	param = update_malloc(sizeof(param));
	if(!param){
		Log_Error("\n\r[%s] Alloc buffer failed", __FUNCTION__);
		goto update_ota_http_exit;
	}
	strcpy(param->cmd, cmd);
	strcpy(param->host, ip);
	strcpy(param->resource,resource);
	param->port = port;
	param->post =post;
	param->out = out;

	if(xTaskCreate(test_http_upload_task, (char const *)"test_http_upload_task", 1024, param, tskIDLE_PRIORITY + 1, NULL) != pdPASS){
		Log_Error("\n\r[%s] Create test_http_upload_task task failed", __FUNCTION__);
		SendMessageToStateManager(MSG_ERR_WIFI_UPDATE, PARAM_FW_UPDATE_WIRELESS_TASK_CREAT_ERROR);
	}
#endif

	D(" end \r\n");
}

// Synchronization processing
static void test_do_http_upload(char *post, char *out_data)
{
	int ret;
	DI(" start");
	D("post[%s]", post);
	test_http_upload_task_run = 1;
	test_http_upload("TEST", OTA_HTTP_HOST, OTA_HTTP_PORT, OTA_HTTP_UPLOAD_RESOURCE, post, out_data);


	// wait?@to task ending
	while(	test_http_upload_task_run != 0) {
		vTaskDelay(500);
	}

	DI(" end\r\n");

}

static char *test_data = "{\
  \"id\": 1,\
  \"body\": 10,\
  \"urine\": 25,\
  \"tem\": 18,\
  \"mac\": \"80d21d92ee2b\",\
  \"time\": 123456,\
  \"ver\": \"01.01.02_01.03.04\",\
  \"low\": [\
  \"bd\": \"223344556677\",\
  \"bd\": \"223344556688\",\
  ], \
  \"conn\": 1\
}";

static char out_data[1024];
#endif // DEBUG_ONLY_ENABLE

static char http_update_resource[256];

/**
 * FW update を開始する
 * scale側  wireless側　バージョンが違うもののみupdate実行する
 * バージョンは先に ota_set_new_version() で設定する
 */
void start_http_update(void)
{
	D(" start");
	ota_update_task_stop = 0;
	update_ota_http("ALLM", OTA_HTTP_HOST, OTA_HTTP_PORT, "");
	D(" end\r\n");
}

/**
 * FW update を中止する
 */
void stop_http_update(void)
{
	int i;
	DI(" start (%x)", TaskOTA);
	ota_update_task_run = 0;
	ota_update_task_stop = 1;
	vTaskDelay(10);
	for (i = 0;  i < 250 &&  TaskOTA != NULL; i++) {
		vTaskDelay(500);
		DDI(">");
	}
	if (i >= 250) {
		Log_Error("\r\n[%s] task stop error ", __FUNCTION__);
		vTaskDelete(TaskOTA);
		vTaskDelay(500);
		TaskOTA = NULL;
		SendMessageToStateManager(MSG_ERR_FWUPDATE, PARAM_FW_UPDATE_TASK_STOP_TIME_OVER);
	}
	ota_update_task_stop = 0;
	DI(" end\r\n");
}

#if AUDREY_ATCMD_ATAU
/**
 * ATコマンド処理 ATAU
 * @param in argv[0] update
 * @param in argv[1] ALL/AM/AMU/SC/SCU/STOP
 * @param in argv[2] IP or server_name
 * @param in argv[3] port  ... 80/443
 * @param in argv[4] resource
 */
void cmd_http_update(int argc, char **argv)
{
	int port;
	char str_prot[6];

	 if (strcmp(argv[1],"SETV") == 0) {

		D(" version set api");

		ota_set_new_version(argv[2], argv[3]);
		Log_Always("\r\n  scale_version=%s am_version=%s\r\n",
				scale_version, am_version);
		return;

	} else if (strcmp(argv[1],"SETVSZ") == 0) {

		D(" version set api");

		strcpy(scale_ver,"");
		Log_Always("\r\n  scale_version=%s am_version=%s\r\n",
				scale_version, am_version);
		Log_Always("\r\n scale_ver=%s", scale_ver);

		ota_set_new_version(argv[2], argv[3]);
		return;

	} else if (strcmp(argv[1],"CLRV") == 0) {

		D(" version clear api");

		ota_clear_new_version();
		Log_Always("\r\n  scale_version=%s am_version=%s\r\n",
				scale_version, am_version);
		return;

	} else if (strcmp(argv[1],"DISPV") == 0) {

		Log_Always("\r\n  my  scale_version=%s am_version=%s", scale_ver, AUDREY_VERSION);
		Log_Always("\r\n  new scale_version=%s am_version=%s\r\n",
				scale_version, am_version);
		return;
	} else if (strcmp(argv[1],"STOP") == 0) {
		Log_Always("\r\n stop ota update task");
		stop_http_update();
		return;
	}
	else if (strcmp(argv[1],"DBLEV")==0) {
		debug_print_lebel = atoi(argv[2]);
		printf("\r\n debug_print_lebel=%d", debug_print_lebel);
		return;
	}
	else if (strcmp(argv[1],"D3") == 0) {
		debug_print_lebel = 3;
		printf("\r\n debug_print_lebel=%d", debug_print_lebel);
		return;
	}
	else if (strcmp(argv[1],"FDUMP") == 0) {
		flash_t *flash;
		uint32_t addr;
		_file_checksum data0,data4,data8,data12;
		int i = 0;
		DI(" dump addr:%s  length:%s ", argv[3], argv[4]);
		DDI("\r\n------------------------------start\r\n");
		addr = atoi(argv[3]);

		device_mutex_lock(RT_DEV_LOCK_FLASH);
		for(i = 0; i<atoi(argv[4]); i+=16) {
			flash_read_word(flash, addr+i, &data0.u);
			flash_read_word(flash, addr+i+4, &data4.u);
			flash_read_word(flash, addr+i+8, &data8.u);
			flash_read_word(flash, addr+i+12, &data12.u);
			if ( data0.u !=  ~0x0 || data4.u !=  ~0x0 || data8.u !=  ~0x0 || data12.u !=  ~0x0) {
				DDI("\n%02X:%04X %02X %02X %02X %02X", ((addr+i) & 0xf0000) / 0x10000,  (addr+i) & 0xffff,
						data0.c[0], data0.c[1], data0.c[2], data0.c[3]);
				DDI(" %02X %02X %02X %02X",  data4.c[0], data4.c[1], data4.c[2], data4.c[3]);
				DDI("-%02X %02X %02X %02X",  data8.c[0], data8.c[1], data8.c[2], data8.c[3]);
				DDI(" %02X %02X %02X %02X",  data12.c[0], data12.c[1], data12.c[2], data12.c[3]);
			}
		}
		device_mutex_unlock(RT_DEV_LOCK_FLASH);
		DDI("\n------------------------------end\r\n");
		DDI("\r\n");
		return;
	}
#ifdef DEBUG_ONLY_ENABLE
	else if (strcmp(argv[1],"TEST") == 0) {

		D(" test upload");

		test_do_http_upload(test_data, out_data);

		Log_Always("\r\n upload responce=[%s]\r\n", out_data);
		return;

	} else if (strcmp(argv[1],"TESTA") == 0) {

		D(" test upload [%s]", argv[2]);

		test_do_http_upload(argv[2], out_data);

		Log_Always("\r\n upload responce=[%s]\r\n", out_data);
		return;
	}  else if (strcmp(argv[1],"DNS") == 0) {

		D(" dns server addr [%s] = 0x%x", argv[2], 	dns_getserver(atoi(argv[2])));

		return;
	}
#endif // DEBUG_ONLY_ENABLE

	sprintf(str_prot,"%d", OTA_HTTP_PORT);
	if(argc == 1) {
		argv[1] = "ALL";
		argv[2] = OTA_HTTP_HOST;
		argv[3] = str_prot;
		argc = 4;
	} else if(argc == 2) {
		argv[2] = OTA_HTTP_HOST;
		argv[3] = str_prot;
		argc = 4;
	} else if(argc == 3) {
		argv[3] = str_prot;
		argc = 4;
	}

	port = atoi(argv[3]);

	D(" CMD= %s start\r\n", argv[1]);

	if (strncmp(argv[1],"ALL",3) == 0) {
			D(" get version & scale update & ameba update");
			if (argc == 4) {
				sprintf(http_update_resource,"%s/%s"OTA_HTTP_FILE_TYPE,OTA_HTTP_WIRELESS_RESOURCE,am_version);
				argv[4] = http_update_resource;
			}

			update_ota_http(argv[1], argv[2], port, argv[4]);
	} else if (strncmp(argv[1],"AM",2) == 0) {
		D("ameba update");
		if (argc == 4) {
			sprintf(http_update_resource,"%s%s"OTA_HTTP_FILE_TYPE,OTA_HTTP_WIRELESS_RESOURCE,am_version);
			argv[4] = http_update_resource;
		}

		update_ota_http(argv[1], argv[2], port, argv[4]);
	} else if (strncmp(argv[1],"SC",2) == 0) {
		D("scale update");
		//if (argc == 4) {
	//		argv[4] = OTA_HTTP_SCALE_RESOURCE;
		//}
		if (argc == 4) {
					sprintf(http_update_resource,"%s%s"OTA_HTTP_FILE_TYPE,OTA_HTTP_SCALE_RESOURCE,scale_version);
					argv[4] = http_update_resource;
		}
		update_ota_http(argv[1], argv[2], port, argv[4]);
	} else if (strcmp(argv[1],"VC") == 0 || strcmp(argv[1],"VU") == 0) {
		D(" version check");
		if (argc == 4) {
			argv[4] = OTA_HTTP_VER_RESOURCE;
		}
		update_ota_http(argv[1], argv[2], port, argv[4]);
	} else if (strcmp(argv[1],"DOVC") == 0) {
		char out_scale_version[17];
		char out_am_version[17];
		int ret;

		out_scale_version[0] = 0;
		out_am_version[0] = 0;

		D(" do version check api");

		ret = do_get_ota_version(out_scale_version, out_am_version);
		Log_Always("\r\n ret=%d out_scale_version=%s out_am_version=%s\r\n",
				ret, out_scale_version, out_am_version);

	} else {
		Log_Always("\r\n ATAU USAGE \n\rATAU=ALL/AM/AMU/SC/SCU/STOP ");
		Log_Always("\r\n ATAU ... scale&ameba FW update");
		Log_Always("\r\n ATAU=SCU ... scale force update");
		Log_Always("\r\n ATAU=AMU ... ameba force update");
		Log_Always("\r\n ATAU=AM ... ameba down load only");
		Log_Always("\r\n ATAU=SC ... scale down load only");
		Log_Always("\r\n ATAU=STOP ... stop ota update task");
		Log_Always("\r\n ATAU={ALL/SC/SCU/AM/AMU},ip_adress,port,pass");
		Log_Always("\r\n ATAU=SCU,"OTA_HTTP_HOST",8080,"OTA_HTTP_SCALE_RESOURCE"1.07"OTA_HTTP_FILE_TYPE" ... scale foce update");
		Log_Always("\r\n ATAU=AMU,"OTA_HTTP_HOST",8080,"OTA_HTTP_SCALE_RESOURCE"00.02.00"OTA_HTTP_FILE_TYPE" ... ameba foce update");
		Log_Always("\r\n");
	}

	D(" CMD= %s end\r\n", argv[1]);
}
#endif // AUDREY_ATCMD_ATAU

#if CONFIG_AUDREY_FWUP_ALONE
extern char standalone_ip[];
extern char standalone_scale[];
extern char standalone_wireless[];

#if CONFIG_AUDREY_DEV_CMD
void ota_start_stand_alone(char type, char *ip)
{
	DI("Start stand-alone OTA");
	if(type == 4) {
		strcpy(ip, "st-pet.sharp.co.jp");
		update_ota_http("SAA", ip, OTA_HTTP_PORT, "/app/st/view/file_manage/get_file/fw_file/wireless/wireless.bin");
		SendMessageToStateManager(MSG_FW_UPDATE_SA_START, type);
		return;
	}
	if(type == 3) {
		strcpy(ip, "st-web.funband.sharp.co.jp");
		update_ota_http("SAA", ip, OTA_HTTP_PORT, "/support/wireless.bin");
		SendMessageToStateManager(MSG_FW_UPDATE_SA_START, type);
		return;
	}
	if(type == 1) {
		update_ota_http("SAS", ip, OTA_HTTP_PORT, "/fw/scale/scale.bin");
	} else {
		update_ota_http("SAA", ip, OTA_HTTP_PORT, "/fw/wireless/wireless.bin");
	}
	SendMessageToStateManager(MSG_FW_UPDATE_SA_START, type);
}
#endif

void ota_start_stand_alone_ap(char type, char *ip, char *ver_scale, char *ver_wireless)
{
	char path[32];

	Log_Info("[stand-alone update] type:%d, ip:%s, scale:%s, wireless:%s\r\n", type, ip, ver_scale, ver_wireless);
	if((type & STANDALONE_TYPE_SCALE) == 1) {
		strcpy(standalone_scale, ver_scale);
		strcpy(path, "/fw/scale/");
		strcat(path, ver_scale);
		strcat(path, ".bin");
		update_ota_http("SAS", ip, OTA_HTTP_PORT, path);
		if(type == STANDALONE_TYPE_BOTH) {
			strcpy(standalone_wireless, ver_wireless);
			strcpy(standalone_ip, ip);
		}
	} else {
		strcpy(standalone_wireless, ver_wireless);
		strcpy(path, "/fw/wireless/");
		strcat(path, ver_wireless);
		strcat(path, ".bin");
		update_ota_http("SAA", ip, OTA_HTTP_PORT, path);
	}
	SendMessageToStateManager(MSG_FW_UPDATE_SA_START, type);
}
#endif

/**
 * 最新バージョン設定
 * バージョンチェックして違う場合には　FW update要求 MSG_FW_UPDATE_START を送信する
 * @param in in_scale_version scale側最新バージョン
 * @param in in_am_version wireless側最新バージョン
 */
int ota_set_new_version(char *in_scale_version, char *in_am_version)
{
	int run_update = 0;

	strncpy(scale_version, in_scale_version, 16);
	strncpy(am_version, in_am_version, 16);
	set_new_version = 1;
	DI("\r\nnew version  scale=%s wireless=%s ",scale_version,  am_version);

	if (strlen(in_scale_version) > 0) {
		if ((strlen(scale_ver) > 0 && strcmp(scale_version, scale_ver) != 0) || strcmp("ERROR", scale_ver) == 0) {
			run_update = 1;
			DI("scale update old[%s] --> new[%s]", scale_ver, scale_version);
	#if 1
			// ##################################################################################
			// 暫定　安全策　古いscale基板には焼かない
			//     焼く場合には ATコマンドにて  ATAU=VU  ATAU=SCU とする
			// ##################################################################################
			if (strcmp(scale_ver,"1.02")==0 || strcmp(scale_ver,"1.03")==0) {
				run_update = 0;
				Log_Always("\r\n ########## SKIP scale update ##############################");
				Log_Always("\r\n scale version is %s ", scale_ver);
				Log_Always("\r\n force update is manual  AT command.   ATAU=VU and  ATAU=SCU");
			}
	#endif
		}
	} else {
		Log_Error("sclae new version is error[%s]", in_scale_version);
		SendMessageToStateManager(MSG_ERR_FWUPDATE, PARAM_FW_UPDATE_VERSION_ERROR);
	}

	if (strlen(in_am_version) > 0) {
		if (strcmp(am_version,AUDREY_VERSION) != 0) {
			run_update = 1;
			DI("wireless update old[%s] --> new[%s]", AUDREY_VERSION, am_version);
		}
	} else {
		Log_Error("wireless new version is error[%s]", in_am_version);
		SendMessageToStateManager(MSG_ERR_FWUPDATE, PARAM_FW_UPDATE_VERSION_ERROR);
	}
	if (run_update) {
		//send message
		DI("send message MSG_FW_UPDATE_START\r\n");
		SendMessageToStateManager(MSG_FW_UPDATE_START, 0);
	}
	return run_update;
}

/**
 * バージョンをクリアする
 */
void ota_clear_new_version(void)
{
	strcpy(scale_version, "");
	strcpy(am_version, "");
	set_new_version = 0;
}

/* ******************************************************************
 * ******************************************************************
 *
 *                 BLE SERVER
 *
 * ******************************************************************
 * ******************************************************************
 */

#include "bt_common.h"
#include "bt_gatt_server.h"
#include "ota_ble_update.h"

extern int ble_debug_lebel;
extern int ble_ota_update_task_run;
extern int test_download_length;

#define      OTA_BLE_RETRY_CONNECT_MAX 100
static int	  retry_connect_count = 0;
//OFFSET
unsigned int scale_buf_read_offset = 0;
unsigned int ble_read_scale_checksum = 0;
unsigned int ameba_buf_read_offset = 0;
int           ameba_resume_NewImg2Len = 0;
unsigned int ameba_resume_NewImg2Addr = 0;
unsigned int ble_read_ameba_checksum = 0;
unsigned char ble_scale_request[128];
unsigned char ble_ameba_request[128];


extern int write_cb_rsp_time; //[TODO]

static int check_sum_index = 0;
static unsigned int check_sum_table[2][500];
static unsigned int check_sum_read[2][500];

int ble_debug_lebel = 7;
int ble_ota_update_task_run = 0;
int test_download_length = 20000;

//struct ble_update_task_param {
//	char cmd[8];
//	char resource[128];
//};



int update_ota_ble_connect_server(void)
{
	int ret = 0;

	DI(" start");
	ret = gatt_open();

	DI("end");
	return ret;
}

void update_ota_ble_disconnect_server(void)
{

	DI(" start");
	gatt_close();
	D(" wait 2S");
	vTaskDelay(2000);
	DI("end");
	return;
}


int ble_ota_http_client_main(char *cmd, char *resource)
{
#ifndef  BUF_USE_SDRAM
	unsigned char *buf, *alloc, *request;
#endif
	int ret; //, len;
	int read_bytes, i = 0;
	int retry_count = 0;

	_file_checksum *file_checksum;
#ifdef CONFIG_CRC
	_file_checksum 		file_crc;
	uint32_t 			flash_crc = 0;
#endif /* CONFIG_CRC */
	uint32_t address, flash_checksum = 0;
	uint32_t NewImg2Len = 0, NewImg2Addr = 0;
	http_response_result_t rsp_result = {0};

#ifdef CHANGE_IMG_0008
	unsigned char data_0008_back;
#endif

	DI("rtw_getFreeHeapSize = %d", rtw_getFreeHeapSize());
	ble_ota_update_task_run = 1;

#ifndef  BUF_USE_SDRAM
	alloc = update_malloc(BUF_SIZE + 4);
	if(!alloc){
		Log_Error("\n\r[%s] Alloc buffer failed", __FUNCTION__);
		SendMessageToStateManager(MSG_ERR_FWUPDATE, PARAM_FW_UPDATE_MALLOC_ERROR);
		goto update_ota_exit1;
	}
	buf = &alloc[4];
#endif
	file_checksum = (void*)(&alloc[0]);

resume_start:
	ret = update_ota_ble_connect_server(); //ble power on & ble connected
	if (ret != 0) {
		Log_Error("[ERROR]%s:%d ble connect error", __FUNCTION__, ret);
		ret = OTA_UPDATE_STOP_AND_CONTINUE;
		SendMessageToStateManager(MSG_ERR_FWUPDATE, PARAM_FW_UPDATE_BLE_CONNECT_ERROR);
		goto update_ota_exit;
	}

#ifndef WRITE_FLASH9000_AFTER_FILE_READ
	if (strncmp(cmd,"AM",2)==0) {
		NewImg2Addr = update_ota_prepare_addr(); //[TODO]　ここで0x9000を変更するとファイル読み込みに失敗したら誤動作する可能性がある
		if(NewImg2Addr == -1){
			ret = -1;
			goto update_ota_exit;
		}

		{ // signature check!!
			unsigned int OldAddr;
			uint32_t sig_readback0 = 0,sig_readback1 = 0;
			uint32_t sig_readback20 = 0,sig_readback21 = 0;

			DI("[CHECK] NewImg2Addr=0x%x", NewImg2Addr);
			device_mutex_lock(RT_DEV_LOCK_FLASH);
			flash_read_word(&flash_ota, NewImg2Addr + 8, &sig_readback0);
			flash_read_word(&flash_ota, NewImg2Addr + 12, &sig_readback1);
			device_mutex_unlock(RT_DEV_LOCK_FLASH);
			DI("[CHECK] NewImg2Addr=0x%x signature read  %x,%x", NewImg2Addr, sig_readback0, sig_readback1);

			if (NewImg2Addr == 0xb000) {
				OldAddr = 0x100000;
			}else {
				OldAddr = 0xb000;
			}

			device_mutex_lock(RT_DEV_LOCK_FLASH);
			flash_read_word(&flash_ota, OldAddr + 8, &sig_readback20);
			flash_read_word(&flash_ota, OldAddr + 12, &sig_readback21);
			device_mutex_unlock(RT_DEV_LOCK_FLASH);
			DI("[CHECK] OldAddr=0x%x signature read  %x,%x", OldAddr, sig_readback20, sig_readback21);

			if (sig_readback0 == 0x35393138 && sig_readback20 == 0x35393138) {
				DE(" #### Part1 and Part2 are the same signature 0x35393138 #####");
			}
		}
	} else {
		NewImg2Addr = buf_sdram; // dummy
	}
	DI("NewImg2Addr=0x%x ble_ota_update_task_run=%d", NewImg2Addr, ble_ota_update_task_run);
#endif // DELAY_WRITE_FLASH9000

	// clear checksum
	file_checksum->u = 0;

	// Write New Image 2 sector
#ifndef DELAY_WRITE_FLASH9000
	if (NewImg2Addr != ~0x0 && ble_ota_update_task_run)
#else
	if(ble_ota_update_task_run)
#endif
	{
		uint32_t idx = 0;
		int data_len = 0;
		int request_len;

		DI("cmd=%s\n\r", cmd);
		DI("resource[%s]\r\n", resource);

		request_len = sprintf(request+8, "%s", resource);

		//DI(" Write to server: request string[%s]\n\r", request+8);
		DI(" cmd=%s scale_buf_read_offset=%d ameba_buf_read_offset=%d", cmd, scale_buf_read_offset, ameba_buf_read_offset);

		// offset
		request[0] = 0;
		request[1] = 0;
		request[2] = 0;
		request[3] = 0;

		if (strncmp(cmd,"SC",2)==0) {

			unsigned  int csum = 0;
			for(i = 0; i < scale_buf_read_offset; i++){
				csum+=buf_sdram[i];
			}
			D(" sd_ram csum=0x%x ble_read_scale_checksum=0x%x", csum, ble_read_scale_checksum);
			DI(" request[%s] : [%s]", request+8  , ble_scale_request+8);

			if (scale_buf_read_offset > 0 &&
					csum == ble_read_scale_checksum &&
					strcmp(ble_scale_request+8, request+8)==0)
			{
				// scale_buf_read_offset
				DI(" ##### RESUME SCALE #####");
				DI(" ##### scale_buf_read_offset=%d  0x%X #####", scale_buf_read_offset, scale_buf_read_offset);
				request[0] =  (scale_buf_read_offset >> 24 ) & 0xff;
				request[1] =  (scale_buf_read_offset >> 16 ) & 0xff;
				request[2] =  (scale_buf_read_offset >> 8 ) & 0xff;
				request[3] =  (scale_buf_read_offset) & 0xff;
			} else {
				scale_buf_read_offset = 0;
				retry_connect_count = 0;
				DI(" ##### NEW SCALE IMAGE DOUNLOAD ##### [%s]", request+8);
				vTaskDelay(3*1000);
			}
			memcpy(ble_scale_request,request,sizeof(ble_scale_request));
		}
		else if (strncmp(cmd,"AM",2)==0) {

			DI(" request[%s] : [%s]", request+8  , ble_ameba_request+8);

			if (ameba_buf_read_offset > 0 &&
				strncmp(ble_ameba_request+8, request+8, strlen(request+8))==0)
			{
				// ameba_buf_read_offset
				DI(" ##### RESUME AMEBA #####");
				DI(" ##### ameba_buf_read_offset=%d 0x%x #####", ameba_buf_read_offset, ameba_buf_read_offset);
				request[0] =  (ameba_buf_read_offset >> 24 ) & 0xff;
				request[1] =  (ameba_buf_read_offset >> 16 ) & 0xff;
				request[2] =  (ameba_buf_read_offset >> 8 ) & 0xff;
				request[3] =  (ameba_buf_read_offset) & 0xff;

			} else {
				ameba_buf_read_offset = 0;
				retry_connect_count = 0;
				DI(" ##### NEW AMEBA IMAGE DOUNLOAD ##### [%s]", request+8);
				DI(" wait 3S ...");
				vTaskDelay(3*1000);
#ifdef DEBUG_ONLY_ADD_WAIT_TIME
				DI(" and wait long time ...");
				vTaskDelay(DEBUG_ONLY_ADD_WAIT_TIME*1000);
#endif

			}

			memcpy(ble_ameba_request,request,sizeof(ble_ameba_request));
		}

		// request string length
		request[4] = (request_len >> 24 ) & 0xff;
		request[5] = (request_len >> 16 ) & 0xff;
		request[6] = (request_len >> 8 ) & 0xff;
		request[7] = (request_len) & 0xff;
		D("request header = ["); D_BUF(request,8); D("]\r\n");
		D("request string=[%s]\r\n", request+8);

		write_cb_rsp_time = 0;



		DI("write gatt len=%d\r\n", request_len+8);
		D("write data["); D_BUF(request, request_len+8); D("]\r\n");

		ret = gatt_write(request, request_len + 8);
		if (ret < 0) {
			Log_Error("\r\n[ERROR]%s gatt_write() error", __FUNCTION__);
			ret = OTA_UPDATE_STOP_AND_CONTINUE;
			goto update_ota_exit;
		}

		rsp_result.body_len = test_download_length;
		idx = 0;
		rsp_result.header_len = idx;
		D("[**TEST**] rsp_result.body_len = %d", rsp_result.body_len);
		write_cb_rsp_time = 500;
		gatt_set_read_left(2048);

		ret = 0;

		// ---------------------------------------------------------------

		if(!ble_ota_update_task_run) {
			DI(" stop");
			goto update_ota_exit;
		}
#ifdef WRITE_FLASH9000_AFTER_FILE_READ

		if (strncmp(cmd,"AM",2)==0) {
			DI("### WRITE_FLASH9000_AFTER_FILE_READ ###");
			NewImg2Addr = update_ota_prepare_addr();
			if(NewImg2Addr == -1){
				ret = -1;
				goto update_ota_exit;
			}



			{ // signature check!!
				unsigned int OldAddr;
				uint32_t sig_readback0 = 0,sig_readback1 = 0;
				uint32_t sig_readback20 = 0,sig_readback21 = 0;

				DI("[CHECK] NewImg2Addr=0x%x", NewImg2Addr);
				device_mutex_lock(RT_DEV_LOCK_FLASH);
				flash_read_word(&flash_ota, NewImg2Addr + 8, &sig_readback0);
				flash_read_word(&flash_ota, NewImg2Addr + 12, &sig_readback1);
				device_mutex_unlock(RT_DEV_LOCK_FLASH);
				DI("[CHECK] NewImg2Addr=0x%x signature read  %x,%x", NewImg2Addr, sig_readback0, sig_readback1);

				if (NewImg2Addr == 0xb000) {
					OldAddr = 0x100000;
				}else {
					OldAddr = 0xb000;
				}

				device_mutex_lock(RT_DEV_LOCK_FLASH);
				flash_read_word(&flash_ota, OldAddr + 8, &sig_readback20);
				flash_read_word(&flash_ota, OldAddr + 12, &sig_readback21);
				device_mutex_unlock(RT_DEV_LOCK_FLASH);
				DI("[CHECK] OldAddr=0x%x signature read  %x,%x", OldAddr, sig_readback20, sig_readback21);

				if (sig_readback0 == 0x35393138 && sig_readback20 == 0x35393138) {
					DE(" #### Part1 and Part2 are the same signature 0x35393138 #####");
				}
			}
		} else {

				NewImg2Addr = (uint32_t)buf_sdram;

		}
		DI("NewImg2Addr=0x%x ble_ota_update_task_run=%d", NewImg2Addr, ble_ota_update_task_run);
#endif

		if (strncmp(cmd,"AM",2) == 0) {

			DI("start FW update");
			memset(buf, 0, BUF_SIZE);

			DI(" 1st read");
			//write_cb_rsp_time = 0;
			read_bytes = gatt_read(buf, 4);
			DI("read["); DI_BUF(buf, read_bytes); DI("]\r\n");

			if(read_bytes == 0) {
				//continue;
				DI(" file end");
				Log_Error("\n\r[%s] 1st read file end", __FUNCTION__);
				SendMessageToStateManager(MSG_ERR_FWUPDATE, PARAM_FW_UPDATE_BLE_FILE_READ_ERROR);
				ret = -1;
				goto update_ota_exit;
			}
			if(read_bytes < 0){
				Log_Error("\n\r[%s] Read socket failed (%d)", __FUNCTION__, read_bytes);
				SendMessageToStateManager(MSG_ERR_FWUPDATE, PARAM_FW_UPDATE_BLE_FILE_READ_ERROR);
				ret = -1;
				goto update_ota_exit;
			}

			if(read_bytes < 4) {
				Log_Error("\n\r[%s] Recv small packet", __FUNCTION__);
				SendMessageToStateManager(MSG_ERR_FWUPDATE, PARAM_FW_UPDATE_BLE_FILE_READ_ERROR);
				ret = -1;
				goto update_ota_exit;
			}

			idx = read_bytes;
			rsp_result.header_len = 4;

			// 最初の4バイトにファイル長がある

			//DI("read 1st record len=%d", read_bytes);
			//DI("buf["); DI_BUF(buf, 20); DD("]\r\n");
			//idx = 0;
			rsp_result.body_len =
					(buf[0] & 0xff) << 24 |
					(buf[1] & 0xff) << 16 |
					(buf[2] & 0xff) << 8 |
					(buf[3] & 0xff);
			DI("set rsp_result.body_len=%d ", rsp_result.body_len);
			// 1MB - bootloader,calibration領域分が更新可能サイズ最大長
			if (rsp_result.body_len < 4 || rsp_result.body_len > (0x00100000 - 0x0000b000)) {
				DI("[ERROR] rsp_result.body_len error (%d) " , rsp_result.body_len);
				SendMessageToStateManager(MSG_ERR_FWUPDATE, PARAM_FW_UPDATE_BLE_FILE_READ_ERROR);
				ret = -1;
				goto update_ota_exit;
			}


			write_cb_rsp_time = 500;


			check_ota_image();

#if SWAP_UPDATE
			DI(" ### SWAP_UPDATE ###");
			NewImg2Addr = update_ota_swap_addr(rsp_result.body_len, NewImg2Addr);
			if(NewImg2Addr == -1){
				Log_Error("\r\n[ERROR]%s flash address error", __FUNCTION__);
				SendMessageToStateManager(MSG_ERR_FWUPDATE, PARAM_FW_UPDATE_INVALID_OTA_ADDRESS_ERROR);
				ret = -1;
				goto update_ota_exit;
			}

#endif // SWAP_UPDATE

			gatt_set_read_left(NewImg2Len - idx);

			if (ameba_buf_read_offset == 0) {
				DI("erase start");
				NewImg2Len = update_ota_erase_upg_region(rsp_result.body_len, NewImg2Len, NewImg2Addr);
				if(NewImg2Len == -1){
					Log_Error("\r\n[ERROR]%s flash length error", __FUNCTION__);
					SendMessageToStateManager(MSG_ERR_FWUPDATE, PARAM_FW_UPDATE_IMG_FILE_LENGTH_ERROR);
					ret = -1;
					goto update_ota_exit;
				}
				ameba_resume_NewImg2Len= NewImg2Len;
				DI("erase end");
			} else {

				if (NewImg2Addr != ameba_resume_NewImg2Addr) {
					Log_Error("\r\n[ERROR]%s resume flash address error", __FUNCTION__);
					SendMessageToStateManager(MSG_ERR_FWUPDATE, PARAM_FW_UPDATE_BLE_RESUME_ADDRESS_ERROR);
					ret = -1;
					goto update_ota_exit;
				}
				NewImg2Len = ameba_resume_NewImg2Len;
				DI(" #RESUME#  not erase");
			}


			check_ota_image();

#ifdef SIGNATURE_ERASE_BEFOR_WRITE
			DI(" ### SIGNATURE_ERASE_BEFOR_WRITE ### ");
			if (ameba_buf_read_offset == 0) {
				//erase signature in New Image 2 addr + 8 and + 12
				uint32_t sig_readback0 = 0,sig_readback1 = 0;
				DI("clear signature +++++++++++++++++ ");
				DI(" NewImg2Addr=0x%x", NewImg2Addr);
				device_mutex_lock(RT_DEV_LOCK_FLASH);
				flash_write_word(&flash_ota,NewImg2Addr + 8, 0xffffffff);
				flash_write_word(&flash_ota,NewImg2Addr + 12, 0xffffffff);
				flash_read_word(&flash_ota, NewImg2Addr + 8, &sig_readback0);
				flash_read_word(&flash_ota, NewImg2Addr + 12, &sig_readback1);
				device_mutex_unlock(RT_DEV_LOCK_FLASH);
				printf("\n\r[%s] NewImg2Addr=0x%x signature readback  %x,%x", __FUNCTION__ , NewImg2Addr, sig_readback0, sig_readback1);
			} else {
				DI(" #RESUME# not erase signature");
			}
#endif // SIGNATURE_ERASE_BEFOR_WRITE

			check_ota_image();

#ifdef SIGNATURE_SET_BEFOR_WRITE
			DI(" ### SIGNATURE_SET_BEFOR_WRITE ### ");
			{
			//Set signature in New Image 2 addr + 8 and + 12
			uint32_t sig_readback0 = 0,sig_readback1 = 0;

			DI(" NewImg2Addr=0x%x", NewImg2Addr);
			device_mutex_lock(RT_DEV_LOCK_FLASH);
			flash_write_word(&flash_ota,NewImg2Addr + 8, 0xffffffff);
			flash_write_word(&flash_ota,NewImg2Addr + 12, 0xffffffff);
			flash_read_word(&flash_ota, NewImg2Addr + 8, &sig_readback0);
			flash_read_word(&flash_ota, NewImg2Addr + 12, &sig_readback1);
			device_mutex_unlock(RT_DEV_LOCK_FLASH);
			DI(" NewImg2Addr=0x%x signature readback  %x,%x", NewImg2Addr, sig_readback0, sig_readback1);
#if SWAP_UPDATE
			DI(" ### SWAP_UPDATE ###");
			if(OldImg2Addr != ~0x0){
				DI(" OldImg2Addr=0x%x", OldImg2Addr);
				device_mutex_lock(RT_DEV_LOCK_FLASH);
				flash_write_word(&flash_ota,OldImg2Addr + 8, 0x35393138);
				flash_write_word(&flash_ota,OldImg2Addr + 12, 0x31313738);
				flash_read_word(&flash_ota, OldImg2Addr + 8, &sig_readback0);
				flash_read_word(&flash_ota, OldImg2Addr + 12, &sig_readback1);
				device_mutex_unlock(RT_DEV_LOCK_FLASH);
				DI(" old=0x%x signature readback %x,%x", OldImg2Addr, sig_readback0, sig_readback1);
			}
#endif // SWAP_UPDATE
			}
#endif // SIGNATURE_SET_BEFOR_WRITE
			check_ota_image();

			address = NewImg2Addr;
			ameba_resume_NewImg2Addr = NewImg2Addr;

			DI(" address=0x%x len=%d \n\r", address, NewImg2Len);

			if (ameba_buf_read_offset == 0) {
#ifdef CHANGE_IMG_0008
				DI(" ### CHANGE_IMG_0008 ###");
				data_0008_back = 0;
#endif
			//Write the body of http response into flash
			data_len = idx - rsp_result.header_len;
			if(data_len > 0){
				DI(" first read img data_len=%d", data_len);

#ifdef CHANGE_IMG_0008
				DI(" ### CHANGE_IMG_0008 ###");
				if (data_len > 9) {
					DI("signature = %.8s", &buf[rsp_result.header_len + 8]);
					data_0008_back = buf[rsp_result.header_len + 8];
					if (data_0008_back == 0x38) {
						buf[rsp_result.header_len + 8] = 0x30;
						DI("CHANGE_IMG_0008 :  0x38-->0x30");
					}
				}
#endif
				DI("flash_stream_write %d byte", data_len);
				device_mutex_lock(RT_DEV_LOCK_FLASH);
				if(flash_stream_write(&flash_ota, address, data_len, (buf+rsp_result.header_len)) < 0){
					Log_Error("\n\r[%s] Write sector failed", __FUNCTION__);
					device_mutex_unlock(RT_DEV_LOCK_FLASH);
					SendMessageToStateManager(MSG_ERR_FWUPDATE, PARAM_FW_UPDATE_FRASH_WRITE_ERROR);
					ret = -1;
					goto update_ota_exit;
				}
				device_mutex_unlock(RT_DEV_LOCK_FLASH);
				DI("write end");
			}

			} else {
				data_len = ameba_buf_read_offset;
			}

			Log_Always("\r\n ### FW image download ###\r\nflash_address=0x%x length=%d\n\r", address, NewImg2Len);
			idx = 0;
			idx += data_len;
			DI(" idx = %d", idx);
			write_cb_rsp_time = 0;
			while (idx < NewImg2Len && ble_ota_update_task_run){
				if (ble_debug_lebel <= 1) {
					Log_Always(".");
				} else {
					DI("read idx=%d", idx);
				}
				data_len = NewImg2Len - idx;
				if(data_len > BUF_SIZE) {
					data_len = BUF_SIZE;
				}

				memset(buf, 0, BUF_SIZE);

				read_bytes = gatt_read(buf, data_len);

				if(read_bytes == 0) {
					//continue;
					DI(" file end");
					ret = -2;
					break; // Read end
				}
				if(read_bytes == -1) {
									//continue;
					DI(" ble disconnect error");
					ret = -2;
					break; // Read end
				}
				if(read_bytes == -2) {
									//continue;
					DI(" ble disconnect stop");
					ret = -2;
					break; // Read end
				}
				if(read_bytes < 0){
					Log_Error("\n\r[%s] Read packet failed", __FUNCTION__);
					SendMessageToStateManager(MSG_ERR_FWUPDATE, PARAM_FW_UPDATE_READ_ERROR);
					ret = -1;
					goto update_ota_exit;
				}

				if(read_bytes<4) {
					Log_Error("\n\r[%s] Recv small packet", __FUNCTION__);
					SendMessageToStateManager(MSG_ERR_FWUPDATE, PARAM_FW_UPDATE_RECV_SMALL_PACKET);
				}

#ifdef CHANGE_IMG_0008
				if (idx <= 8 && (idx+read_bytes) > 9) {
					DI(" ### CHANGE_IMG_0008 ###");
					DI("idx = %d signature = %.8s", idx, &buf[8 - idx]);
					data_0008_back = buf[8 - idx];
					if (data_0008_back == 0x38) {
						buf[8 - idx] = 0x30;
						DI("CHANGE_IMG_0008 :  0x38-->0x%x", buf[8 - idx]);
					}
				}
#endif
				DI("flash_stream_write %d byte", read_bytes);
				device_mutex_lock(RT_DEV_LOCK_FLASH);
				if(flash_stream_write(&flash_ota, address + idx, read_bytes, buf) < 0){
					Log_Error("\n\r[%s] Write sector failed", __FUNCTION__);
					device_mutex_unlock(RT_DEV_LOCK_FLASH);
					SendMessageToStateManager(MSG_ERR_FWUPDATE, PARAM_FW_UPDATE_FRASH_WRITE_ERROR);
					ret = -1;
					goto update_ota_exit;
				}
				device_mutex_unlock(RT_DEV_LOCK_FLASH);
				DI("write end");

				file_checksum->c[0] = buf[read_bytes-4];      // checksum attached at file end
				file_checksum->c[1] = buf[read_bytes-3];
				file_checksum->c[2] = buf[read_bytes-2];
				file_checksum->c[3] = buf[read_bytes-1];

#ifdef CONFIG_CRC
				file_crc.u = 0;
				file_crc.c[0] = buf[read_bytes-8];			// crc attached at file end
				file_crc.c[1] = buf[read_bytes-7];
				file_crc.c[2] = buf[read_bytes-6];
				file_crc.c[3] = buf[read_bytes-5];
#endif /* CONFIG_CRC */

				idx += read_bytes;

				gatt_set_read_left(NewImg2Len - idx);

				wdt_task_refresh(WDT_TASK_FWUP);
			}

			//ret = 0;
			DI("ret=%d Download new firmware %d bytes completed\n\r", ret, idx);
			/* Downloadしたデータが存在しない場合、DFUをエラー終了する */
			if (idx == 0) {
				Log_Error("\n\r[ERROR][%s] Download failed", __FUNCTION__);
				goto update_ota_exit;
			}
			ameba_buf_read_offset = idx;

			// read flash data back and calculate checksum
			flash_checksum = 0;
			for(i = 0; i < idx-4; i += BUF_SIZE){
				int k;
				int rlen = (idx-4-i)>BUF_SIZE?BUF_SIZE:(idx-4-i);
				device_mutex_lock(RT_DEV_LOCK_FLASH);
				flash_stream_read(&flash_ota, NewImg2Addr+i, rlen, buf);
				device_mutex_unlock(RT_DEV_LOCK_FLASH);
				for(k = 0; k < rlen; k++) {
					flash_checksum+=buf[k];
				}
			}

#ifdef CONFIG_CRC
			// read flash data back and calculate crc
			for (uint32_t i = 0; i < 256; i++) {
				uint32_t c = i;
				for (int j = 0; j < 8; j++) {
					c = (c & 1) ? (0xEDB88320 ^ (c >> 1)) : (c >> 1);
				}
				crc_table[i] = c;
			}
			flash_crc = 0xFFFFFFFF;
			for(i = 0; i < idx-8; i += BUF_SIZE){
				int k;
				int rlen = (idx-8-i)>BUF_SIZE?BUF_SIZE:(idx-8-i);
				device_mutex_lock(RT_DEV_LOCK_FLASH);
				flash_stream_read(&flash_ota, NewImg2Addr+i, rlen, buf);
				device_mutex_unlock(RT_DEV_LOCK_FLASH);
				for(k = 0; k < rlen; k++) {
					flash_crc = crc_table[(flash_crc ^ (uint32_t)buf[k]) & 0xFF] ^ (flash_crc >> 8);
				}
			}
			flash_crc ^= 0xFFFFFFFF;
#endif /* CONFIG_CRC */

#ifdef CHANGE_IMG_0008
			DI(" ### CHANGE_IMG_0008 ###");
			if (data_0008_back == 0x38) {
				file_checksum->u -= (0x38 - 0x30);
				DI(" change file_checksum");
			}
			DI(" file_checksum=%d flash_checksum=%d", file_checksum->u, flash_checksum);
#endif

			D("NewImg2Addr=0x%x len=%d", NewImg2Addr, idx-4);
			Log_Always("\r\nameba update flash_checksum 0x%x  file_checksum->u 0x%x \r\n", flash_checksum, file_checksum->u);
#ifdef CONFIG_CRC
			Log_Always("\r\nameba update flash_crc 0x%x  file_crc.u 0x%x \r\n", flash_crc, file_crc.u);
#endif /* CONFIG_CRC */

			check_ota_image();

			if (strncmp(cmd,"AMU",3) == 0 && ble_ota_update_task_run) {

				vTaskDelay(1000);

#ifdef OTA_HTTP_FLASH_WRITE_ENABLE
#ifdef SIGNATURE_WRITE_DIRECT
#ifdef SIGNATURE_WRITE_DIRECT_CMD_OTA_IMAGE
				// cmd_ota_imageを使って書き込む
				// OK!

				DI("### SIGNATURE_WRITE_DIRECT_CMD_OTA_IMAGE ###");
#ifdef CONFIG_CRC
				if ((check_crc_fw_version() == TRUE) && (file_crc.u != flash_crc)) {
					Log_Error("\n\r[%s] The crc is wrong!\n\r", __FUNCTION__);
					SendMessageToStateManager(MSG_ERR_FWUPDATE, PARAM_FW_UPDATE_CRC_ERROR);
					ret = OTA_UPDATE_STOP_AND_CONTINUE;
					ameba_buf_read_offset = 0;
					goto update_ota_exit;
				} else if ((check_crc_fw_version() == FALSE) && (file_checksum->u != flash_checksum)) {
					Log_Error("\n\r[%s] The checksume is wrong!\n\r", __FUNCTION__);
					SendMessageToStateManager(MSG_ERR_FWUPDATE, PARAM_FW_UPDATE_CHECKSUM_ERROR);
					ret = OTA_UPDATE_STOP_AND_CONTINUE;
					ameba_buf_read_offset = 0;
					goto update_ota_exit;
				} else {
#else
				if (file_checksum->u == flash_checksum) {
#endif /* CONFIG_CRC */
					uint32_t sig_readback0 = 0,sig_readback1 = 0;
					if (NewImg2Addr == 0xb000) {
						cmd_ota_image_set(0);
					}else {
						cmd_ota_image_set(1);
					}
					DI(" NewImg2Addr=0x%x", NewImg2Addr);
					device_mutex_lock(RT_DEV_LOCK_FLASH);
					flash_read_word(&flash_ota, NewImg2Addr + 8, &sig_readback0);
					flash_read_word(&flash_ota, NewImg2Addr + 12, &sig_readback1);
					device_mutex_unlock(RT_DEV_LOCK_FLASH);
					DI(" NewImg2Addr=0x%x signature readback  %x,%x", NewImg2Addr, sig_readback0, sig_readback1);
					if (sig_readback0 != 0x35393138) {
						DE(" signature write error");
						return -1;
					}
					if (NewImg2Addr == 0xb000) {
						cmd_ota_image_unset(0);
					}else {
						cmd_ota_image_unset(1);
					}
					if(OldImg2Addr != ~0x0){
						device_mutex_lock(RT_DEV_LOCK_FLASH);
						flash_read_word(&flash_ota, OldImg2Addr + 8, &sig_readback0);
						flash_read_word(&flash_ota, OldImg2Addr + 12, &sig_readback1);
						device_mutex_unlock(RT_DEV_LOCK_FLASH);
						DI(" OldImg2Addr=0x%x signature readback  %x,%x", OldImg2Addr, sig_readback0, sig_readback1);
					}
					ret = 0;

					ameba_buf_read_offset = 0;
					ota_update_ameba_done = 1;

					Log_Always("\r\n FW update new firmware %d bytes completed\n\r", idx);
#ifdef CONFIG_CRC
				}
#else
				} else {
					Log_Error("\n\r[%s] The checksume is wrong!\n\r", __FUNCTION__);
					//ret = -1;
					//goto update_ota_exit;
				}
#endif /* CONFIG_CRC */

#else //SIGNATURE_WRITE_DIRECT_CMD_OTA_IMAGE
				// update_ota_checksum()がうまくいかないので　直接ここで書き換える
				// これもうまく書けない
				//
				check_ota_image();
				if(file_checksum->u == flash_checksum)
				{
					//Set signature in New Image 2 addr + 8 and + 12
					uint32_t sig_readback0 = 0,sig_readback1 = 0;

					DI(" NewImg2Addr=0x%x", NewImg2Addr);
					device_mutex_lock(RT_DEV_LOCK_FLASH);
					flash_write_word(&flash_ota,NewImg2Addr + 8, 0x35393138);
					flash_write_word(&flash_ota,NewImg2Addr + 12, 0x31313738);
					flash_read_word(&flash_ota, NewImg2Addr + 8, &sig_readback0);
					flash_read_word(&flash_ota, NewImg2Addr + 12, &sig_readback1);
					device_mutex_unlock(RT_DEV_LOCK_FLASH);
					DI(" NewImg2Addr=0x%x signature readback  %x,%x", NewImg2Addr, sig_readback0, sig_readback1);
					if (sig_readback0 != 0x35393138) {
						DE(" signature write error");
						return -1;
					}
#if SWAP_UPDATE
					if(OldImg2Addr != ~0x0){
						DI(" OldImg2Addr=0x%x", OldImg2Addr);
						device_mutex_lock(RT_DEV_LOCK_FLASH);
						flash_write_word(&flash_ota,OldImg2Addr + 8, 0x35393130);
						flash_write_word(&flash_ota,OldImg2Addr + 12, 0x31313738);
						flash_read_word(&flash_ota, OldImg2Addr + 8, &sig_readback0);
						flash_read_word(&flash_ota, OldImg2Addr + 12, &sig_readback1);
						device_mutex_unlock(RT_DEV_LOCK_FLASH);
						DI(" old=0x%x signature readback %x,%x", OldImg2Addr, sig_readback0, sig_readback1);
					}
#endif // SWAP_UPDATE
				}
#endif // SIGNATURE_WRITE_DIRECT_CMD_OTA_IMAGE
#else // SIGNATURE_WRITE_DIRECT
				// これもうまく書けない
				//　+8からの 81958711 が書けない 書いても変化しない
				//
				int csum_ret = update_ota_checksum(file_checksum, flash_checksum, NewImg2Addr);

				if(csum_ret == -1){
					Log_Error("\n\r[%s] The checksume is wrong!\n\r", __FUNCTION__);
					SendMessageToStateManager(MSG_ERR_FWUPDATE, PARAM_FW_UPDATE_CHECKSUM_ERROR);
					//ret = -1;
					//goto update_ota_exit;
				}
#endif // SIGNATURE_WRITE_DIRECT
				check_ota_image();
#else // OTA_HTTP_FLASH_WRITE_ENABLE
				D("update  test ..... ### SKIP flash signature write ###  \n\r");
#endif
			} // AMU

			D("retry_connect_count=%d ret=%d ameba_buf_read_offset=%d ble_ota_update_task_run=%s",
					retry_connect_count, ret, ameba_buf_read_offset,
					ble_ota_update_task_run?"true":"false");

			if (retry_connect_count < OTA_BLE_RETRY_CONNECT_MAX
					&& (ret == -2 || ret == -1)
					&& ameba_buf_read_offset > 0
					&& ble_ota_update_task_run) {
				DI("resume ameba_buf_read_offset=%d", ameba_buf_read_offset);
				retry_connect_count ++;

				update_ota_ble_disconnect_server();
				DI("resume start");
				goto resume_start;
			} else {
				retry_connect_count = 0;
			}

		// ----------------------------------------AM-------------------------------------
		}  else if (strncmp(cmd,"SC",2)==0) {

			static unsigned int file_size_sum = 0;
			memset(buf, 0, BUF_SIZE);

			// read file size
			read_bytes = gatt_read(buf, 4);
			if (ble_debug_lebel > 1) {
				DDI("read<%d>", read_bytes);
			}

			//Log_Always("\r\n idx=%d data_len=%d read_bytes=%d", idx, data_len, read_bytes);
			if(read_bytes == 0) {
				//continue;
				DI(" file end");
				Log_Error("\n\r[%s] Read packet failed", __FUNCTION__);
				SendMessageToStateManager(MSG_ERR_FWUPDATE, PARAM_FW_UPDATE_READ_ERROR);
				ret = -1;
				goto update_ota_exit;
			}

			if(read_bytes < 0){
				Log_Error("\n\r[%s] Read packet failed", __FUNCTION__);
				SendMessageToStateManager(MSG_ERR_FWUPDATE, PARAM_FW_UPDATE_READ_ERROR);
				ret = -1;
				goto update_ota_exit;
			}

			if(read_bytes<4) {
				Log_Error("\n\r[%s] Recv small packet", __FUNCTION__);
				SendMessageToStateManager(MSG_ERR_FWUPDATE, PARAM_FW_UPDATE_RECV_SMALL_PACKET);
			}

			DI("file size  len=%d", read_bytes);
			DI("buf["); DI_BUF(buf, 4); DD("]\r\n");
			idx = 0;
			NewImg2Len =
					(buf[0] & 0xff) << 24 |
					(buf[1] & 0xff) << 16 |
					(buf[2] & 0xff) << 8 |
					(buf[3] & 0xff);
			DI("NewImg2Len=%d ", NewImg2Len);

			if (NewImg2Len < 4 || NewImg2Len > 64000) {
				Log_Error("\n\r[%s] file length failed (%d)", __FUNCTION__, NewImg2Len);
				SendMessageToStateManager(MSG_ERR_FWUPDATE, PARAM_FW_UPDATE_IMG_FILE_LENGTH_ERROR);
				ret = -1;
				goto update_ota_exit;
			}

			gatt_set_read_left(NewImg2Len);

			int sum = 0;
			for(i = 0; i < 4; i++){
				sum += buf[i];
			}
			DI(" read_bytes=%d idx=%d file size sum=0x%x", read_bytes, idx, sum);
			file_size_sum = sum;

			address = (uint32_t)buf_sdram;

			/*
			NewImg2Len = rsp_result.body_len;

			//Write the body of http response into flash
			data_len = idx - rsp_result.header_len;
			if(data_len > 0){
				file_checksum->c[0] = buf[data_len-4];	  // checksum attached at file end
				file_checksum->c[1] = buf[data_len-3];
				file_checksum->c[2] = buf[data_len-2];
				file_checksum->c[3] = buf[data_len-1];

				memcpy(address, (buf+rsp_result.header_len), data_len);
				DI("data_len=%d", data_len)
			}
			*/

			idx = 0;

			if (scale_buf_read_offset > 0) {
				DI(" scale_buf_read_offset=%d (0x%x) request=[%s]",
						scale_buf_read_offset, scale_buf_read_offset, request+8);
				idx = scale_buf_read_offset;
				NewImg2Len += idx;
			}
			//idx += data_len;
			//NewImg2Len = 13000;

			check_sum_index = 0;
			DI("start idx=%d NewImg2Len=%d(0x%x) buf_sdram=0x%x address=0x%x",
					idx, NewImg2Len, NewImg2Len, buf_sdram, address);
			DI("start idx=%d NewImg2Len=%d(0x%x) buf_sdram=0x%x address=0x%x",
								idx, NewImg2Len, NewImg2Len, buf_sdram, address);
			while (idx < NewImg2Len && ble_ota_update_task_run){

				data_len = NewImg2Len - idx;

				if (ble_debug_lebel <= 1) {
					Log_Always(".");
				} else {
					DI("idx=%d (%d)", idx, data_len);
				}

				if(data_len > BUF_SIZE) {
					data_len = BUF_SIZE;
				}

				memset(buf, 0, data_len);

				read_bytes = gatt_read(buf, data_len);
				if (ble_debug_lebel > 1) {
					DDI("read<%d>", read_bytes);
				}

				//Log_Always("\r\n idx=%d data_len=%d read_bytes=%d", idx, data_len, read_bytes);
				if(read_bytes == 0) {
					//continue;
					DI(" file end");
					ret = -2;
					break; // Read end
				}
				if(read_bytes == -2){
					Log_Error("\n\r[%s] ble disconnected", __FUNCTION__);
					//SendMessageToStateManager(MSG_ERR_FWUPDATE, PARAM_FW_UPDATE_READ_ERROR); //[TODO]
					ret = -2;
					break;
				}
				if(read_bytes < 0){
					Log_Error("\n\r[%s] Read socket failed", __FUNCTION__);
					//SendMessageToStateManager(MSG_ERR_FWUPDATE, PARAM_FW_UPDATE_READ_ERROR); //[TODO]
					ret = -1;
					goto update_ota_exit;
				}

				if(read_bytes<4) {
					Log_Error("\n\r[ERROR][%s] Recv small packet", __FUNCTION__);
					SendMessageToStateManager(MSG_ERR_FWUPDATE, PARAM_FW_UPDATE_RECV_SMALL_PACKET);
				}

				if(read_bytes > data_len) {
					Log_Error("\n\r[ERROR][%s] ############# read_bytes(%d)  data_len(%d) #############", __FUNCTION__,
							read_bytes, data_len);
					SendMessageToStateManager(MSG_ERR_FWUPDATE, PARAM_FW_UPDATE_BLE_READ_LENGTH_OVER);
				}



				memcpy(address + idx, buf, read_bytes);

				file_checksum->c[0] = buf[read_bytes-4];      // checksum attached at file end
				file_checksum->c[1] = buf[read_bytes-3];
				file_checksum->c[2] = buf[read_bytes-2];
				file_checksum->c[3] = buf[read_bytes-1];

				idx += read_bytes;

				gatt_set_read_left(NewImg2Len - idx);

				int sum = 0;
				for(i = 0; i < read_bytes; i++){
					sum += buf[i];
				}
				check_sum_table[0][check_sum_index] = sum;
				check_sum_read[0][check_sum_index] = read_bytes;
				check_sum_index++;
				DI(" read_bytes=%d idx=%d adr=0x%x sum=0x%x", read_bytes, idx, address + idx, sum);
			}
			ret = 0;
			DI(" Download new scale firmware %d bytes completed\n\r", idx);
			/* Downloadしたデータが存在しない場合、DFUをエラー終了する */
			if (idx == 0) {
				Log_Error("\n\r[ERROR][%s] Download failed", __FUNCTION__);
				goto update_ota_exit;
			}
#if 1
			{
				int sum = 0;
				int k = 0;

				for(int i = 0; i < idx-4; i += BUF_SIZE){
					sum = 0;
					for ( int j = 0; j < BUF_SIZE; j++) {
						if ( i + j >= (idx - 4)) {
							break;
						}
						sum += buf_sdram[i+j];
					}
					check_sum_table[1][k] = sum;
					k++;
				}

				DI(" check");
				int sumlen = 0;
				for (i = 0; i<check_sum_index; i++ ) {
					DDI("\r\n %02d %04d 0x%04x : 0x%04x %s", i, check_sum_read[0][i],  check_sum_table[0][i], check_sum_table[1][i],
							check_sum_table[0][i] == check_sum_table[1][i] ? "ok": "error");
					sumlen += check_sum_read[0][i];
				}
				DI(" end  sumlen=%d", sumlen);
			}
#endif

			// read flash data back and calculate checksum
			flash_checksum = 0;
			for(i = 0; i < idx-4; i++){
					flash_checksum+=buf_sdram[i];
			}

			scale_buf_read_offset = (idx -4) & 0xff00;
			memcpy(ble_scale_request, request, 128);
			ble_read_scale_checksum = 0;
			for(i = 0; i < scale_buf_read_offset; i++){
				ble_read_scale_checksum+=buf_sdram[i];
			}

			DI(" sd_ram length=%d (scale_buf_read_offset=%d)", idx-4,  scale_buf_read_offset);
			// checking check sum
			DI("\r\nscale update checksum file=0x%x sdram=0x%x \n\r", file_checksum->u, flash_checksum);
			if (flash_checksum != file_checksum->u) {
				DI("check sum error");
			} else if (strncmp(cmd,"SCU",3) == 0) {
				int sret;
				// scale fw update
				scale_buf_read_offset = 0;
	#ifdef OTA_HTTP_FLASH_WRITE_ENABLE
				DI(" ### OTA_HTTP_FLASH_WRITE_ENABLE ###");
				 extern int scale_up_start(u8 *, u16);
				 DI("\n\r[%s] scale update start\n\r", __FUNCTION__);
				 sret = scale_up_start(buf_sdram, idx-4);
				 DI("\n\r[%s] scale update end ret=%d\n\r", __FUNCTION__, sret);
				 ret = sret;
				 ota_update_scale_done = 1;
	#else
				 Log_Always("\n\r[%s] scale update SKIP ...... \n\r", __FUNCTION__);
	#endif

			}

			if (retry_connect_count < OTA_BLE_RETRY_CONNECT_MAX
					&& ret == -2
					&& scale_buf_read_offset > 0
					&& ble_ota_update_task_run) {
				DI("sesume ameba_buf_read_offset=%d", scale_buf_read_offset);
				retry_connect_count ++;

				update_ota_ble_disconnect_server();

				ret = OTA_UPDATE_STOP_AND_CONTINUE;

				goto resume_start;
			} else {
				retry_connect_count = 0;
			}

		// ---------------------------------------------- SC -------------------------------
		}

	}

update_ota_exit:
	DI(" exit");
#ifndef  BUF_USE_SDRAM
	if(alloc)
		update_free(alloc);
	if(request)
		update_free(request);
#endif

	update_ota_ble_disconnect_server();

update_ota_exit1:
	DI("rtw_getFreeHeapSize = %d", rtw_getFreeHeapSize());
	D(" end ret(%d)\r\n", ret);
	return ret;
}


void ota_ble_update_task(void *vparam){
	struct http_task_param  *param;

	int ret = -1;
#ifdef TASK_PARAM_USE_SH_HOST
	param = (struct http_task_param *)&sh_host;
#else
	param = (struct http_task_param *)vparam;
#endif
	DI(" task start\n\r");
	ota_update_scale_done = 0;
	ota_update_ameba_done = 0;
	ble_ota_update_task_stop = 0;
	D("param %s, %s\n\r", param->cmd, param->resource);

	wdt_task_watch(WDT_TASK_FWUP, WDT_TASK_WATCH_START);

	if (strncmp(param->cmd,"ALL",3) == 0) {
		// all
		int i;


		DI("\r\nserver scale_version=%s am_version=%s ",scale_version,  am_version);
#if 1
		if ((strlen(scale_ver) > 0 && strcmp(scale_version, scale_ver)) != 0 || strcmp("ERROR", scale_ver) == 0) {
			DI("\n\r[%s] Scale verstion is different. new[%s] : old[%s] ", __FUNCTION__, scale_version, scale_ver);

#if 1
			// ##################################################################################
			// 暫定　安全策　古いscale基板には焼かない
			//     焼く場合には ATコマンドにて  ATAU=VU  ATAU=SCU とする
			// ##################################################################################
			if (strcmp(scale_ver,"1.02")==0 || strcmp(scale_ver,"1.03")==0) {
				Log_Always("\r\n ########## SKIP scale update ##############################");
				Log_Always("\r\n scale version is %s ", scale_ver);
				Log_Always("\r\n force update is manual  AT command.   ATAU=VU and  ATAU=SCU");
			} else
#endif
			{
				// scale update
				if (strcmp(scale_version,"ERROR") == 0) {
					DI(" scale_version is ERROR,  clear scale_version");
					strcpy(scale_version,"");
				}
				sprintf(server_resource,"%s%s", OTA_BLE_SCALE_RESOURCE, scale_version);
				ret = ble_ota_http_client_main("SCU",  server_resource);
				if (ret) {
					Log_Error("\r\n[%s] scale download error(%d)", __FUNCTION__, ret);
					SendMessageToStateManager(MSG_ERR_FWUPDATE, PARAM_FW_UPDATE_SCALE_DOWNLOAD_ERROR);
					goto exit;
				}
				DI("\r\nwait 6S"); //[TODO] wait scale update
				vTaskDelay(6*1000);
			}
		} else {
			DI("\n\r[%s] Scale FW is latest version %s  ", __FUNCTION__,  scale_ver);
			// none
		}

		if (strcmp(am_version,AUDREY_VERSION) != 0) {
			DI("\n\r[%s] Audrey verstion is different. new[%s] : old[%s] ", __FUNCTION__, am_version, AUDREY_VERSION);
			// ameba update
			sprintf(server_resource,"%s%s", OTA_BLE_WIRELESS_RESOURCE,am_version);
			ret = ble_ota_http_client_main("AMU", server_resource);
			if (ret) {
				Log_Error("\r\n[%s] wireless download error(%d)", __FUNCTION__, ret);
				SendMessageToStateManager(MSG_ERR_FWUPDATE, PARAM_FW_UPDATE_WIRELESS_DOWNLOAD_ERROR);
				goto exit;
			}
		} else {
			Log_Always("\n\r[%s] Audrey FW is latest version %s  ", __FUNCTION__,  AUDREY_VERSION);
			// none
		}
#else
		// ---------------- test-------------------------------------------
			Log_Error("\r\n test  ......  force update\r\n");
			sprintf(server_resource,"%s/%s"OTA_HTTP_FILE_TYPE,OTA_HTTP_SCALE_RESOURCE,scale_version);
			ret = ota_http_client_main("SCU", OTA_HTTP_HOST, OTA_HTTP_PORT, server_resource,NULL);
			if (ret) {
				Log_Error("\r\n[%s] scale download error(%d)", __FUNCTION__, ret);
			}
			Log_Error("\r\nwait 6S"); //[TODO] wait scale update
			vTaskDelay(6*1000);

			sprintf(server_resource,"%s/%s"OTA_HTTP_FILE_TYPE,OTA_HTTP_WIRELESS_RESOURCE,am_version);
			ret = ota_http_client_main("AMU", OTA_HTTP_HOST, OTA_HTTP_PORT, server_resource,NULL);
			if (ret) {
				Log_Error("\r\n[%s] ameba download error(%d)", __FUNCTION__, ret);
			}
#endif

	} else {
		DI(" start");
		ret = ble_ota_http_client_main(param->cmd,  param->resource);
	}

exit:
	DI(" task end ret=%d\r\n",  ret);
	Log_Notify("Update %s !!!\r\n", (ret == 0 ? "success" : "fail"));
	if (strcmp(param->cmd,"ALLM") == 0) {
		if (ble_ota_update_task_stop == 0){
				// reset
				if(ret == 0){
					//send ok message
					SendMessageToStateManager(MSG_OTA_RESET, 0);

				} else if (ret == OTA_UPDATE_STOP_AND_CONTINUE) {
					// continue
					DI(" update stop \r\n");
					SendMessageToStateManager(MSG_FW_UPDATE_STOP, 0);
				} else {
					//send error
					Log_Error("\r\n[%s] BLE FW update error ret(%d)", __FUNCTION__, ret);
					SendMessageToStateManager(MSG_FW_UPDATE_STOP, 0);
				}

		} else {
			// STOP
			if (ota_update_scale_done > 0 || ota_update_ameba_done > 0) {
				// reset
				DI("already updated!");
				if(!ret){
					//send ok message
					SendMessageToStateManager(MSG_OTA_RESET, 0);

				} else {
					//send error
					Log_Error("\r\n[%s] BLE FW update error ret(%d)", __FUNCTION__, ret);
					SendMessageToStateManager(MSG_ERR_WIFI_UPDATE, 0);
				}
			} else {
				// continue MSG_ERR_FWUPDATE
				SendMessageToStateManager(MSG_FW_UPDATE_STOP, 0);
			}
		}
	}
#ifndef TASK_PARAM_USE_SH_HOST
	if(vparam) {
		update_free(vparam);
	}
#endif

#if defined(INCLUDE_uxTaskGetStackHighWaterMark) && (INCLUDE_uxTaskGetStackHighWaterMark == 1)
		printf("\n\rMin available stack size of %s = %d * %d bytes\n\r", __FUNCTION__, uxTaskGetStackHighWaterMark(NULL), sizeof(portBASE_TYPE));
#endif

	wdt_task_watch(WDT_TASK_FWUP, WDT_TASK_WATCH_NONE);
	TaskOTA = NULL;
	ble_ota_update_task_run = 0;

	DI("task end");
	vTaskDelete(NULL);

}


void ota_ble_update_start_task(char *cmd, char *resource)
{
	struct http_task_param *param;
	DI(" start ");
	if(TaskOTA){
		Log_Error("\n\r[%s] Update task has already created.\r\n", __FUNCTION__);
		//send error
		SendMessageToStateManager(MSG_ERR_FWUPDATE, PARAM_FW_UPDATE_BLE_ALREADY_RUNNING);
		return;
	}
#ifdef TASK_PARAM_USE_SH_HOST
	strcpy(sh_host.cmd, cmd);
	strcpy(sh_host.resource,resource);
	D(" sh_host %s %s\n\r", sh_host.cmd, sh_host.resource);
	if(xTaskCreate(ota_ble_update_task, (char const *)"ble_ota", 2048, NULL, tskIDLE_PRIORITY + 1, &TaskOTA) != pdPASS){
		Log_Error("\n\r[%s] Create ota_ble_update_task task failed", __FUNCTION__);
		//send error
		SendMessageToStateManager(MSG_ERR_WIFI_UPDATE, PARAM_FW_UPDATE_BLE_TASK_CREAT_ERROR);
	}
#else
	param = update_malloc(sizeof(param));
	if(!param){
		Log_Error("\n\r[%s] Alloc buffer failed", __FUNCTION__);
		//send error
		SendMessageToStateManager(MSG_ERR_WIFI, MSG_ERR_WIFI_UPDATE);
		goto update_ota_http_exit;
	}
	strcpy(param->host, ip);
	strcpy(param->resource,resource);
	param->port = port;
	if(xTaskCreate(http_update_ota_task, (char const *)"http_update_ota_task", 1024, param, tskIDLE_PRIORITY + 1, &TaskOTA) != pdPASS){
		Log_Error("\n\r[%s] Create update_ota_http task failed", __FUNCTION__);
		//send error
		SendMessageToStateManager(MSG_ERR_WIFI_UPDATE, PARAM_FW_UPDATE_BLE_WIRELESS_TASK_CREAT_ERROR);
	}
#endif

update_ota_http_exit:
	DI(" end \r\n");
}


//static char http_update_resource[128];

/**
 * BLE経由 FW update を開始する
 * scale側  wireless側　バージョンが違うもののみupdate実行する
 * バージョンは先に ota_set_new_version() で設定する
 */
void start_ota_ble_update(void)
{
	DI(" start");
	ble_ota_update_task_stop = 0;
	ota_ble_update_start_task("ALLM", "");
	DI(" end\r\n");
}



/**
 * BLE経由 FW update を中止する
 */
void stop_ota_ble_update(void)
{
	int i;
	DI(" start (%x)", TaskOTA);
	ble_ota_update_task_run = 0;
	ble_ota_update_task_stop = 1;
	vTaskDelay(10);
	for (i = 0;  i < 100 &&  TaskOTA != NULL; i++) {
		vTaskDelay(500);
		DDI(">");
	}
	if (i >= 100) {
		Log_Error("\r\n[%s] task stop error ", __FUNCTION__);
		vTaskDelete(TaskOTA);
		vTaskDelay(500);
		TaskOTA = NULL;
		SendMessageToStateManager(MSG_ERR_FWUPDATE, PARAM_FW_UPDATE_BLE_TASK_STOP_TIME_OVER);
	}
	DI(" end");
}

#if AUDREY_ATCMD_ATAG
/**
 * ATコマンド処理 ATAG
 * @param in argv[0] update
 * @param in argv[1] OTA
 * @param in argv[2] ALL/AM/AMU/SC/SCU/STOP
 * @param in argv[3] IP or server_name
 * @param in argv[4] port  ... 80/443
 * @param in argv[5] resource
 */
void cmd_ota_ble_update(int argc, char **argv)
{


	DI(" start");

	if (strcmp(argv[2],"STOP") == 0) {
		Log_Always("\r\n stop ota update task");
		stop_ota_ble_update();
		return;
	} else if (strcmp(argv[2],"START") == 0) {
		Log_Always("\r\n start ota update task");

		if (argc >= 4) {
			strcpy(scale_version, argv[3]);
		}
		if (argc >= 5) {
			strcpy(am_version, argv[4]);
		}
		DI("new scale_version=%s  am_version=%s", scale_version, am_version);

		start_ota_ble_update();

		return;
	} else if (strcmp(argv[2],"FDUMP") == 0) {
		flash_t *flash;
		uint32_t addr;
		_file_checksum data0,data4,data8,data12;
		int i = 0;
		DI(" dump addr:%s  length:%s ", argv[3], argv[4]);
		DDI("\r\n------------------------------start\r\n");
		addr = atoi(argv[3]);

		device_mutex_lock(RT_DEV_LOCK_FLASH);
		for(i = 0; i<atoi(argv[4]); i+=16) {
			flash_read_word(flash, addr+i, &data0.u);
			flash_read_word(flash, addr+i+4, &data4.u);
			flash_read_word(flash, addr+i+8, &data8.u);
			flash_read_word(flash, addr+i+12, &data12.u);
			if ( data0.u !=  ~0x0 || data4.u !=  ~0x0 || data8.u !=  ~0x0 || data12.u !=  ~0x0) {
				DDI("\n%02X:%04X %02X %02X %02X %02X", ((addr+i) & 0xf0000) / 0x10000,  (addr+i) & 0xffff,
						data0.c[0], data0.c[1], data0.c[2], data0.c[3]);
				DDI(" %02X %02X %02X %02X",  data4.c[0], data4.c[1], data4.c[2], data4.c[3]);
				DDI("-%02X %02X %02X %02X",  data8.c[0], data8.c[1], data8.c[2], data8.c[3]);
				DDI(" %02X %02X %02X %02X",  data12.c[0], data12.c[1], data12.c[2], data12.c[3]);
			}
		}
		device_mutex_unlock(RT_DEV_LOCK_FLASH);
		DDI("\n------------------------------end\r\n");
		DDI("\r\n");
		return;
	}
	//[TODO] test
	else if (strcmp(argv[2],"DLEN") == 0) {
		test_download_length = atoi(argv[3]);
		DI("test_download_length = %d", test_download_length);
		return;
	}
	else if (strcmp(argv[2],"DLEV") == 0) {
		ble_debug_lebel = atoi(argv[3]);
		DI("ble_debug_lebel = %d", ble_debug_lebel);
		return;
	}

	// test_write_req
//	else if (strcmp(argv[2],"TREQ") == 0) {
//		test_write_req = atoi(argv[3]);
//		DI("test_write_req = %d", test_write_req);
//		return;
//	}
	else if (strcmp(argv[2],"DBLEV")==0) {
		debug_print_lebel = atoi(argv[3]);
		printf("\r\n debug_print_lebel=%d", debug_print_lebel);
		return;
	} else if (strcmp(argv[2],"D3") == 0) {
		debug_print_lebel = 3;
		printf("\r\n debug_print_lebel=%d", debug_print_lebel);
		return;
	}
	//scale_buf_read_offset
//	else if (strcmp(argv[2],"SOFF") == 0) {
//		scale_buf_read_offset = atoi(argv[3]);
//		printf("\r\n scale_buf_read_offset=%d", scale_buf_read_offset);
//		return;
//	}
//	else if (strcmp(argv[2],"AOFF") == 0) {
//			ameba_buf_read_offset = atoi(argv[3]);
//			printf("\r\n ameba_buf_read_offset=%d", ameba_buf_read_offset);
//			return;
//		}


	// ATAG=OTA,SC,nharp.tk,443,dev/api/getfirmware?type=scale&ver=0.08
	//sprintf(str_prot,"%d", OTA_HTTP_PORT);
	if(argc == 2) {
		argv[2] = "ALL";
		argc = 3;
	}

	D(" BLE %s CMD= %s start\r\n", argv[1], argv[2]);

	if (strncmp(argv[2],"ALL",3) == 0) {
			D(" get version & scale update & ameba update");
			if (argc == 3) {
				sprintf(http_update_resource,"%s_%s",am_version, scale_version);
				argv[3] = http_update_resource;
			}

			ota_ble_update_start_task(argv[2], argv[3]);
	} else if (strncmp(argv[2],"AMO",3) == 0) {
		D("ameba update resume");
		if (argc == 3) {
			sprintf(http_update_resource,"type=wireless&ver=%s",am_version);
						//"dev/api/getfirmware?type=wireless&ver="
			argv[3] = http_update_resource;
		}

		ota_ble_update_start_task(argv[2], argv[3]);
	}  else if (strncmp(argv[2],"AM",2) == 0) {
		D("ameba update");
		ameba_buf_read_offset = 0;
		if (argc == 3) {
			sprintf(http_update_resource,"type=wireless&ver=%s",am_version);
						//"dev/api/getfirmware?type=wireless&ver="

			argv[3] = http_update_resource;
		}

		ota_ble_update_start_task(argv[2], argv[3]);
	}  else if (strncmp(argv[2],"SCO",3) == 0 || strncmp(argv[2],"SCUO",4) == 0) {
		D("scale update offset");
		if (argc == 3) {
			//sprintf(http_update_resource,"%s%s"OTA_HTTP_FILE_TYPE,OTA_HTTP_SCALE_RESOURCE,scale_version);
			sprintf(http_update_resource,"type=scale&ver=%s",scale_version);
			argv[3] = http_update_resource;
		}
		ota_ble_update_start_task(argv[2], argv[3]);
	} else if (strncmp(argv[2],"SC",2) == 0) {
		D("scale update");
		scale_buf_read_offset = 0;
		if (argc == 3) {
			//sprintf(http_update_resource,"%s%s"OTA_HTTP_FILE_TYPE,OTA_HTTP_SCALE_RESOURCE,scale_version);
			sprintf(http_update_resource,"type=scale&ver=%s",scale_version);
			argv[3] = http_update_resource;
		}
		ota_ble_update_start_task(argv[2], argv[3]);
	}  else {
		Log_Always("\r\n ATAG OTA USAGE \n\rATAG=OTA,ALL/AM/AMU/SC/SCU/STOP ");
		Log_Always("\r\n ATAG ... scale&ameba FW update");
		Log_Always("\r\n ATAG=OTA,SCU ... scale force update");
		Log_Always("\r\n ATAG=OTA,AMU ... ameba force update");
		Log_Always("\r\n ATAG=OTA,AM ... ameba down load only");
		Log_Always("\r\n ATAG=OTA,SC ... scale down load only");
		Log_Always("\r\n ATAG=OTA,STOP ... stop ota update task");
		Log_Always("\r\n ATAG=OTA,{ALL/SC/SCU/AM/AMU},resource");
		Log_Always("\r\n");
	}

	D(" CMD= %s end\r\n", argv[1]);
}
#endif // AUDREY_ATCMD_ATAG

#endif //#if CONFIG_OTA_HTTP_UPDATE

#endif //#if CONFIG_OTA_HTTP_UPDATE ||  CONFIG_OTA_UPDATE

