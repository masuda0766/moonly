/**
 ******************************************************************************
 *This file contains general configurations for ameba platform
 ******************************************************************************
*/

#ifndef __PLATFORM_OPTS_H__
#define __PLATFORM_OPTS_H__

/*For MP mode setting*/
#define SUPPORT_MP_MODE		0

/* 商用と開発用で切替 START */
#define CONFIG_AUDREY_BEACON_DEV	1		// 開発時バッジ名(商用:0、開発用:1)
#define AUDREY_SERVER				1		// SSLサーバー情報(商用:2、開発用:1)
/* 商用と検証用で切替 END */

#define CONFIG_AUDREY_DEV_CMD		0		// 開発用コマンド
#define AUDREY_LOG_INFO				0		// 基本情報用ログ

#define CONFIG_AUDREY				1		// 基本的に有効にしておく
#define CONFIG_AUDREY_WDT			1		// デバッグでbreak設定する際はwatchdogを無効化にする必要あり
#define CONFIG_AUDREY_SCL_OFF		1		// Scale通信の有効/無効を切り替える場合に使用
#define CONFIG_AUDREY_BADGE			1		// 固体識別判定(BTとwifiのON/OFF)を無効化する時は0にする
#define CONFIG_AUDREY_LOG_UPLOAD	1		// ログ情報アップロード
#define CONFIG_AUDREY_FWUP_ALONE	1		// stand-aloneファーム更新
#define CONFIG_AUDREY_INDV			1		// 個体識別処理
#define CONFIG_AUDREY_LED_KEY		1		// キーによるLED制御(サーバー接続までは点灯)
#define CONFIG_AUDREY_DBG_UPLOAD	1		// データアップロード デバッグモード
#define CONFIG_AUDREY_BACKUP_ENTRY	1		// 入退室データ不揮発保存対応
#define CONFIG_AUDREY_BACKUP_BADGE	1		// バッジ情報不揮発保存対応
#define CONFIG_AUDREY_CONF_BACKUP	1		// 無線設定値バックアップ対応
#define CONFIG_AUDREY_UPLOAD_RETRY	1		// データアップロードリトライ対応(初回定期送信リトライはこの定義と無関係に実行)

#define CONFIG_AUDREY_DBG_FORCE		0		// データアップロード 強制デバッグモード
#define CONFIG_AUDREY_DBG_BLE		0		// BLE接続デバッグモード
#define CONFIG_AUDREY_SKIP_VERCHK	0		// 初回定期送信時のバージョンチェックをスキップ
#define CONFIG_AUDREY_SCALE_1SEC	0		// 常に１秒間隔で重量取得

#define CONFIG_AUDREY_INDV_LOG		0		// 個体識別処理 確認ログ
#define CONFIG_AUDREY_INDV_EMU		0		// 個体識別処理 エミュレーター
#define CONFIG_AUDREY_INDV_EMU_LOG	0		// 個体識別処理 エミュレーター

#define CONFIG_AUDREY_CONFIG_WIFI	0		// wifi経由初期設定
#define CONFIG_AUDREY_SELF_RESET	0		// 定期リセット
#define CONFIG_AUDREY_LED_WEIGHT	0		// 体重計値によるLED制御
#define CONFIG_AUDREY_LED_BLK3		0		// LED点滅3分で消灯制御
#define CONFIG_AUDREY_CAB_JUDGE		0		// 上部ユニット装着判定

#define CONFIG_AUDREY_EXT_INFO		1		// 拡張情報の出力切り替え
#define CONFIG_AUDREY_SYSDBG		1		// システム関連の各種デバッグ機能有効無効切り替え

#define CONFIG_AUDREY_ALWAYS_BT_ON	0		// 常時BT電源ON
#define CONFIG_AUDREY_REDUCE_MALLOC	1		// mallocの利用個所削減

#define AUDREY_ATCMD_ATAU			0		// ATAU ATコマンド有効無効切り替え
#define AUDREY_ATCMD_ATAG			0		// ATAG ATコマンド有効無効切り替え

#if AUDREY_SERVER == 2
#define AUDREY_SERVER_NAME		"pet.sharp.co.jp"
#define AUDREY_SERVER_TYPE		"/up/api"
//#define AUDREY_API_KEY			"YZoRGVVZoW9FMnIlMlUEM5mTZENs9K6Q1gF6n6D9"
#define AUDREY_API_KEY_1		"YZoRGVVZoW"
#define AUDREY_API_KEY_2		"9FMnIlMlUE"
#define AUDREY_API_KEY_3		"M5mTZENs9K"
#define AUDREY_API_KEY_4		"6Q1gF6n6D9"
#elif AUDREY_SERVER == 1
#define AUDREY_SERVER_NAME		"st-pet.sharp.co.jp"
#define AUDREY_SERVER_TYPE		"/up/st/api"
//#define AUDREY_API_KEY			"ucyPKhqIK73dz0Hs0y1Vd6tNI0i2mudZRShwYWVb"
#define AUDREY_API_KEY_1		"ucyPKhqIK7"
#define AUDREY_API_KEY_2		"3dz0Hs0y1V"
#define AUDREY_API_KEY_3		"d6tNI0i2mu"
#define AUDREY_API_KEY_4		"dZRShwYWVb"
#else
#define AUDREY_SERVER_NAME		"nyarp.tk"
#define AUDREY_SERVER_TYPE		"/dev/api"
//#define AUDREY_API_KEY			"rhyfHIQuCR46vguHWSShR6Ag0OCMYCrA8Zpo4njH"
#define AUDREY_API_KEY_1		"rhyfHIQuCR"
#define AUDREY_API_KEY_2		"46vguHWSSh"
#define AUDREY_API_KEY_3		"R6Ag0OCMYC"
#define AUDREY_API_KEY_4		"rA8Zpo4njH"
#endif
extern unsigned char AUDREY_API_KEY[];
#define AUDREY_MODEL_SCALE		"HN-PCS01"
#define AUDREY_MODEL_BADGE		"HN-PM001"

#define AUDREY_LOG_ALWAYS		1		// 常に出力するログ
#define AUDREY_LOG_ERROR		1		// エラーログ
#define AUDREY_LOG_NOTIFY		1		// 通知ログ
#define AUDREY_LOG_DEBUG		0		// デバッグ用ログ
#define AUDREY_LOG_SCALE		1		// Scale通信用ログ

#if AUDREY_LOG_ALWAYS
#define Log_Always(...)		printf(__VA_ARGS__)
#else
#define Log_Always(...)
#endif
#if AUDREY_LOG_ERROR
#define Log_Error(...)		printf(__VA_ARGS__)
#else
#define Log_Error(...)
#endif
#if AUDREY_LOG_NOTIFY
#define Log_Notify(...)		printf(__VA_ARGS__)
#else
#define Log_Notify(...)
#endif
#if AUDREY_LOG_INFO
#define Log_Info(...)		printf(__VA_ARGS__)
#else
#define Log_Info(...)
#endif
#if AUDREY_LOG_DEBUG
#define Log_Debug(...)		printf(__VA_ARGS__)
#else
#define Log_Debug(...)
#endif
#if AUDREY_LOG_SCALE
#define Log_Scale(...)		printf(__VA_ARGS__)
#else
#define Log_Scale(...)
#endif

/**
 * For AT cmd Log service configurations
 */
#define SUPPORT_LOG_SERVICE	1
#if SUPPORT_LOG_SERVICE
#define LOG_SERVICE_BUFLEN     100 //can't larger than UART_LOG_CMD_BUFLEN(127)
#define CONFIG_LOG_HISTORY	0
#if CONFIG_LOG_HISTORY
#define LOG_HISTORY_LEN    5
#endif
#define SUPPORT_INTERACTIVE_MODE	0//on/off wifi_interactive_mode
#define CONFIG_LOG_SERVICE_LOCK		0
#if SUPPORT_MP_MODE
#define CONFIG_ATCMD_MP				1 //support MP AT command
#endif
#define USE_MODE                    1 //for test
#endif

/**
 * For interactive mode configurations, depends on log service
 */
#if SUPPORT_INTERACTIVE_MODE
#define CONFIG_INTERACTIVE_MODE		1
#define CONFIG_INTERACTIVE_EXT		0
#else
#define CONFIG_INTERACTIVE_MODE		0
#define CONFIG_INTERACTIVE_EXT		0
#endif

/**
 * For FreeRTOS tickless configurations
 */
#define FREERTOS_PMU_TICKLESS_PLL_RESERVED   0   // In sleep mode, 0: close PLL clock, 1: reserve PLL clock
#define FREERTOS_PMU_TICKLESS_SUSPEND_SDRAM  1   // In sleep mode, 1: suspend SDRAM, 0: no act

/******************************************************************************/

/**
* For common flash usage
*/
#if CONFIG_AUDREY
#define FAST_RECONNECT_DATA			0x001FF000		// 無線接続情報
#define FLASH_USER_CONFIG_BT_BASE	0x001FE000		// BT接続情報
#define SCALE_IS_UPDATE_FLG			0x001FD000		// Scaleファーム更新中フラグ
#define LOG_RECORD_2				0x001FB000		// ログ保存領域(Backup)
#define LOG_RECORD_1				0x001FA000		// ログ保存領域
#define LOG_FAULT_2					0x001F9000		// HardFaultError保存領域(Backup)
#define LOG_FAULT_1					0x001F8000		// HardFaultError保存領域
#if CONFIG_AUDREY_BACKUP_ENTRY
#define ENTRY_FALSH					0x001F7000		// 入退室データ保存領域
#endif
#if CONFIG_AUDREY_BACKUP_BADGE
#define BADGE_FALSH					0x001F6000		// バッジ情報保存領域
#endif
#if CONFIG_AUDREY_CONF_BACKUP
#define FAST_RECONNECT_BACKUP		0x001F5000		// 無線接続情報Backup
#endif
#else
#define AP_SETTING_SECTOR		0x000FE000
#define UART_SETTING_SECTOR		0x000FC000

#if defined(__GNUC__)
#define FAST_RECONNECT_DATA 	(0x86800 - 0x1000)
#else
#define FAST_RECONNECT_DATA 	(0x80000 - 0x1000)
#endif /* defined(__GNUC__) */
#endif

#define FLASH_SECTOR_SIZE       0x1000

/**
 * For Wlan configurations
 */
#define CONFIG_WLAN		1
#if CONFIG_WLAN
#define CONFIG_LWIP_LAYER	1
#define CONFIG_INIT_NET		1 //init lwip layer when start up
#define CONFIG_WIFI_IND_USE_THREAD	0	// wifi indicate worker thread
#if (SUPPORT_MP_MODE == 0)
#define CONFIG_ENABLE_AP_POLLING_CLIENT_ALIVE 1 // on or off AP POLLING CLIENT
#endif

//on/off relative commands in log service
#define CONFIG_SSL_CLIENT	0
#define CONFIG_SSL_HTTPS_CLIENT	1 // https
#define CONFIG_OTA_HTTP_UPDATE	1 // http fw update
#define CONFIG_WEBSERVER	1
#define CONFIG_OTA_UPDATE	0
#define CONFIG_BSD_TCP		0//NOTE : Enable CONFIG_BSD_TCP will increase about 11KB code size
#define CONFIG_AIRKISS			0//on or off tencent airkiss
#define CONFIG_UART_SOCKET	0
#define CONFIG_JD_SMART		0//on or off for jdsmart
#define CONFIG_JOYLINK			0//on or off for jdsmart2.0
#define CONFIG_QQ_LINK		0//on or off for qqlink
#define CONFIG_AIRKISS_CLOUD	0//on or off for weixin hardware cloud
#define CONFIG_UART_YMODEM	0//support uart ymodem upgrade or not
#define CONFIG_GOOGLE_NEST	0//on or off the at command control for google nest
#define CONFIG_TRANSPORT	0//on or off the at command for transport socket
#define CONFIG_ALINK		0//on or off for alibaba alink
#define CONFIG_BT			1//on or off for bluetooth
#define CONFIG_BT_GATT_SERVER	1 // gatt_server
#define CONFIG_CRC			1

/* For WPS and P2P */
#define CONFIG_ENABLE_WPS		1
#define CONFIG_ENABLE_P2P		0
#if CONFIG_ENABLE_WPS
#define CONFIG_ENABLE_WPS_DISCOVERY	1
#endif
#if CONFIG_ENABLE_P2P
#define CONFIG_ENABLE_WPS_AP		1
#undef CONFIG_WIFI_IND_USE_THREAD
#define CONFIG_WIFI_IND_USE_THREAD	1
#endif
#if (CONFIG_ENABLE_P2P && ((CONFIG_ENABLE_WPS_AP == 0) || (CONFIG_ENABLE_WPS == 0)))
#error "If CONFIG_ENABLE_P2P, need to define CONFIG_ENABLE_WPS_AP 1"
#endif

/* For SSL/TLS */
#define CONFIG_USE_POLARSSL     0 // 1
#define CONFIG_USE_MBEDTLS      1  // 0
#if ((CONFIG_USE_POLARSSL == 0) && (CONFIG_USE_MBEDTLS == 0)) || ((CONFIG_USE_POLARSSL == 1) && (CONFIG_USE_MBEDTLS == 1))
#undef CONFIG_USE_POLARSSL
#define CONFIG_USE_POLARSSL 1
#undef CONFIG_USE_MBEDTLS
#define CONFIG_USE_MBEDTLS 0
#endif

/* For Simple Link */
#define CONFIG_INCLUDE_SIMPLE_CONFIG		0

/*For fast reconnection*/
#define CONFIG_EXAMPLE_WLAN_FAST_CONNECT	0

/*For wowlan service settings*/
#define CONFIG_WOWLAN_SERVICE           			0

#define CONFIG_GAGENT			0
/*Disable CONFIG_EXAMPLE_WLAN_FAST_CONNECT when CONFIG_GAGENT is enabled,because
	reconnect to previous AP is not suitable when re-configuration.
*/
#if CONFIG_GAGENT
#define CONFIG_EXAMPLE_WLAN_FAST_CONNECT 0
#endif


#endif //end of #if CONFIG_WLAN
/*******************************************************************************/

/**
 * For Ethernet configurations
 */
#define CONFIG_ETHERNET 0
#if CONFIG_ETHERNET

#define CONFIG_LWIP_LAYER	1
#define CONFIG_INIT_NET         1 //init lwip layer when start up

//on/off relative commands in log service
#define CONFIG_SSL_CLIENT	0
#define CONFIG_BSD_TCP		0//NOTE : Enable CONFIG_BSD_TCP will increase about 11KB code size

#endif


/**
 * For iNIC configurations
 */
#ifdef CONFIG_INIC //this flag is defined in IAR project
#define CONFIG_INIC_EN 1//enable iNIC mode
#undef CONFIG_ENABLE_WPS
#define CONFIG_ENABLE_WPS 1
#undef CONFIG_INCLUDE_SIMPLE_CONFIG
#define CONFIG_INCLUDE_SIMPLE_CONFIG	1
#undef CONFIG_WOWLAN_SERVICE
#define CONFIG_WOWLAN_SERVICE 1
#undef LOG_SERVICE_BUFLEN
#define LOG_SERVICE_BUFLEN 256
#undef CONFIG_LWIP_LAYER
#define CONFIG_LWIP_LAYER	0
#undef CONFIG_OTA_UPDATE
#define CONFIG_OTA_UPDATE 0
#undef CONFIG_EXAMPLE_WLAN_FAST_CONNECT
#define CONFIG_EXAMPLE_WLAN_FAST_CONNECT 0
#define CONFIG_INIC_SDIO_HCI	1 //for SDIO or USB iNIC
#define CONFIG_INIC_USB_HCI	0
#define CONFIG_INIC_CMD_RSP     1 //need to return msg to host
#endif
/******************End of iNIC configurations*******************/

/* for CoAP example*/
#define CONFIG_EXAMPLE_COAP              0

/* For aj_basic_example */
#define CONFIG_EXAMPLE_AJ_BASIC          0

/*For aj_ameba_led example*/
#define CONFIG_EXAMPLE_AJ_AMEBA_LED      0

/* For WIFI GET BEACON FRAME example */
#define CONFIG_EXAMPLE_GET_BEACON_FRAME  0

/* For WIFI MAC MONITOR example */
#define CONFIG_EXAMPLE_WIFI_MAC_MONITOR  0

/* For HTTP CLIENT example */
#define CONFIG_EXAMPLE_HTTP_CLIENT  0

/* For MQTT example */
#define CONFIG_EXAMPLE_MQTT				0

/* For WiGadget example */
#define CONFIG_EXAMPLE_WIGADGET			0

/*For google nest example*/
#define CONFIG_EXAMPLE_GOOGLE_NEST		0

/* For mDNS example */
#define CONFIG_EXAMPLE_MDNS				0

/* For multicast example */
#define CONFIG_EXAMPLE_MCAST			0

/* For XML example */
#define CONFIG_EXAMPLE_XML				0

/* For socket select example */
#define CONFIG_EXAMPLE_SOCKET_SELECT	0

/* For socket nonblocking connect example */
#define CONFIG_EXAMPLE_NONBLOCK_CONNECT	0

/* For socket TCP bidirectional transmission example */
#define CONFIG_EXAMPLE_SOCKET_TCP_TRX	0

/* For ssl download example */
#define CONFIG_EXAMPLE_SSL_DOWNLOAD      0

/* For http download example */
#define CONFIG_EXAMPLE_HTTP_DOWNLOAD	0

/* For httpc example */
#define CONFIG_EXAMPLE_HTTPC			0

/* For httpd example */
#define CONFIG_EXAMPLE_HTTPD			0

/* For tcp keepalive example */
#define CONFIG_EXAMPLE_TCP_KEEPALIVE	0

/* For sntp show time example */
#define CONFIG_EXAMPLE_SNTP_SHOWTIME	0

/* For pppoe example */
#define CONFIG_EXAMPLE_PPPOE            0

/* For websocket client example */
#define CONFIG_EXAMPLE_WEBSOCKET		0

/*For Audio example */
#define CONFIG_EXAMPLE_AUDIO			0
#if CONFIG_EXAMPLE_AUDIO
#define FATFS_DISK_SD 	1
#define CONFIG_EXAMPLE_CODEC_SGTL5000         1
#endif

/* For audio mp3 example */
#define CONFIG_EXAMPLE_AUDIO_MP3		0
#if CONFIG_EXAMPLE_AUDIO_MP3
#define FATFS_DISK_SD 	1
#undef CONFIG_WLAN
#define CONFIG_WLAN		0
#undef CONFIG_EXAMPLE_WLAN_FAST_CONNECT
#define CONFIG_EXAMPLE_WLAN_FAST_CONNECT  0
#define CONFIG_EXAMPLE_MP3_STREAM_SGTL5000 1
#endif

/* For UART Module AT command example */
#define CONFIG_EXAMPLE_UART_ATCMD	0
#if CONFIG_EXAMPLE_UART_ATCMD
#undef FREERTOS_PMU_TICKLESS_PLL_RESERVED
#define FREERTOS_PMU_TICKLESS_PLL_RESERVED  1
#undef CONFIG_OTA_UPDATE
#define CONFIG_OTA_UPDATE 1
#undef CONFIG_TRANSPORT
#define CONFIG_TRANSPORT 1
#undef LOG_SERVICE_BUFLEN
#define LOG_SERVICE_BUFLEN 1600
#undef CONFIG_LOG_SERVICE_LOCK
#define CONFIG_LOG_SERVICE_LOCK 1
#undef CONFIG_EXAMPLE_WLAN_FAST_CONNECT
#define CONFIG_EXAMPLE_WLAN_FAST_CONNECT  0
#endif

/* For SPI Module AT command example */
#define CONFIG_EXAMPLE_SPI_ATCMD 0

#if CONFIG_EXAMPLE_SPI_ATCMD
#undef FREERTOS_PMU_TICKLESS_PLL_RESERVED
#define FREERTOS_PMU_TICKLESS_PLL_RESERVED  1
#undef CONFIG_OTA_UPDATE
#define CONFIG_OTA_UPDATE 1
#undef CONFIG_TRANSPORT
#define CONFIG_TRANSPORT 1
#undef LOG_SERVICE_BUFLEN
#define LOG_SERVICE_BUFLEN 1600
#undef CONFIG_LOG_SERVICE_LOCK
#define CONFIG_LOG_SERVICE_LOCK 1
#undef CONFIG_EXAMPLE_WLAN_FAST_CONNECT
#define CONFIG_EXAMPLE_WLAN_FAST_CONNECT  0
#endif

#define CONFIG_EXAMPLE_MEDIA_SS 				0
#define CONFIG_EXAMPLE_MEDIA_MS					0
#define CONFIG_EXAMPLE_MEDIA_AUDIO_FROM_RTP                     0
//Defines for mp3 streaming over wifi, default output through alc5651
#define CONFIG_EXAMPLE_MP3_STREAM_RTP                           0

#if CONFIG_EXAMPLE_MP3_STREAM_RTP
#undef CONFIG_EXAMPLE_MEDIA_AUDIO_FROM_RTP
#define CONFIG_EXAMPLE_MEDIA_AUDIO_FROM_RTP 1
#undef	CONFIG_INCLUDE_SIMPLE_CONFIG
#define CONFIG_INCLUDE_SIMPLE_CONFIG	0
//Set this flag to 1 in case sgtl5000 to be used else alc5651 will be used
#define CONFIG_EXAMPLE_MP3_STREAM_SGTL5000                      0
#endif
// Use media source/sink example
#if (CONFIG_EXAMPLE_MEDIA_SS==1) || (CONFIG_EXAMPLE_MEDIA_MS==1)
#undef CONFIG_INCLUDE_SIMPLE_CONFIG
#define CONFIG_INCLUDE_SIMPLE_CONFIG		0
#define CONFIG_ENABLE_WPS	0
#endif

/* For Mjpeg capture example*/
#define CONFIG_EXAMPLE_MJPEG_CAPTURE		0
#if CONFIG_EXAMPLE_MJPEG_CAPTURE
#define FATFS_DISK_SD 	1
#endif

/* For DCT example*/
#define CONFIG_EXAMPLE_DCT				0

/****************** For EAP method example *******************/
#define CONFIG_EXAMPLE_EAP	0

// on/off specified eap method
#define CONFIG_ENABLE_PEAP	0
#define CONFIG_ENABLE_TLS	0
#define CONFIG_ENABLE_TTLS	0

// optional feature: whether to verify the cert of radius server
#define ENABLE_EAP_SSL_VERIFY_SERVER	0

#if CONFIG_ENABLE_PEAP || CONFIG_ENABLE_TLS || CONFIG_ENABLE_TTLS
#define CONFIG_ENABLE_EAP
#define CONFIG_EXAMPLE_WLAN_FAST_CONNECT 0
#endif

#if CONFIG_ENABLE_TLS
#define ENABLE_EAP_SSL_VERIFY_CLIENT	1
#else
#define ENABLE_EAP_SSL_VERIFY_CLIENT	0
#endif
/******************End of EAP configurations*******************/

/* For usb mass storage example */
#define CONFIG_EXAMPLE_USB_MASS_STORAGE		0

/* For FATFS example*/
#define CONFIG_EXAMPLE_FATFS			0
#if CONFIG_EXAMPLE_FATFS
#define CONFIG_FATFS_EN	1
#if CONFIG_FATFS_EN
// fatfs version
#define FATFS_R_10C
// fatfs disk interface
#define FATFS_DISK_USB	0
#define FATFS_DISK_SD 	1
#endif
#endif

/* For iNIC host example*/
#ifdef CONFIG_INIC_GSPI_HOST //this flag is defined in IAR project
#define	CONFIG_EXAMPLE_INIC_GSPI_HOST	1
#if CONFIG_EXAMPLE_INIC_GSPI_HOST

#define CONFIG_INIC_HOST	1

#undef CONFIG_WLAN
#define CONFIG_WLAN		0
#undef	CONFIG_INCLUDE_SIMPLE_CONFIG
#define CONFIG_INCLUDE_SIMPLE_CONFIG	0
#undef	CONFIG_EXAMPLE_WLAN_FAST_CONNECT
#define CONFIG_EXAMPLE_WLAN_FAST_CONNECT	0
#undef CONFIG_LWIP_LAYER
#define CONFIG_LWIP_LAYER	1
#undef	CONFIG_BSD_TCP
#define CONFIG_BSD_TCP		1

#endif
#endif

/*For uart update example*/
#define CONFIG_UART_UPDATE				0
#if CONFIG_UART_UPDATE
#undef CONFIG_EXAMPLE_WLAN_FAST_CONNECT
#define CONFIG_EXAMPLE_WLAN_FAST_CONNECT 0
#endif


/*For arduino wifi shield example */
#define CONFIG_EXAMPLE_ARDUINO_WIFI	0
#if CONFIG_EXAMPLE_ARDUINO_WIFI
#undef CONFIG_WIFI_NORMAL
#endif

/* For uart adapter example */
/* Please also configure LWIP_UART_ADAPTER to 1
in lwip_opt.h for support uart adapter*/
#define CONFIG_EXAMPLE_UART_ADAPTER	0
#if CONFIG_EXAMPLE_UART_ADAPTER
#undef CONFIG_EXAMPLE_WLAN_FAST_CONNECT
#define CONFIG_EXAMPLE_WLAN_FAST_CONNECT	1
#undef CONFIG_EXAMPLE_MDNS
#define CONFIG_EXAMPLE_MDNS	1
#undef FREERTOS_PMU_TICKLESS_PLL_RESERVED
#define FREERTOS_PMU_TICKLESS_PLL_RESERVED  1
#endif
#if CONFIG_JD_SMART
#if (CONFIG_ENABLE_WPS == 1)
#define CONFIG_ENABLE_WPS	0
#endif
#if (CONFIG_INCLUDE_SIMPLE_CONFIG == 1)
#define CONFIG_INCLUDE_SIMPLE_CONFIG	0
#endif
#if (CONFIG_EXAMPLE_WLAN_FAST_CONNECT == 1)
#define CONFIG_EXAMPLE_WLAN_FAST_CONNECT	0
#endif
#endif

/* For wifi scenarios example (Wi-Fi, WPS enrollee, P2P GO) */
// also need to enable WPS and P2P
#define CONFIG_EXAMPLE_WLAN_SCENARIO	0

/* For broadcast example */
#define CONFIG_EXAMPLE_BCAST			0

/* For high-load memory use case memory usage */
#define CONFIG_EXAMPLE_HIGH_LOAD_MEMORY_USE		0

/* For rarp example */
#define CONFIG_EXAMPLE_RARP				0

/* For ssl server example */
#define CONFIG_EXAMPLE_SSL_SERVER		0

/*For timelapse example */
#define CONFIG_EXAMPLE_TIMELAPSE 0

#if CONFIG_EXAMPLE_TIMELAPSE
#undef CONFIG_INCLUDE_SIMPLE_CONFIG
#define CONFIG_INCLUDE_SIMPLE_CONFIG		0
#define CONFIG_ENABLE_WPS	0
#define CONFIG_FATFS_EN	1
#define FATFS_R_10C
#define FATFS_DISK_SD 	1
#endif

/* For ota update http example */
#define CONFIG_EXAMPLE_OTA_HTTP			0

/* For Amazon AWS IoT example */
#define CONFIG_EXAMPLE_AMAZON_AWS_IOT 0

#if CONFIG_BT
/* For bt beacon example */
#define CONFIG_EXAMPLE_BT_BEACON		0

/* For bt config example */
#define CONFIG_EXAMPLE_BT_CONFIG	0

/* For bt BLE GATT Client example */
#define CONFIG_EXAMPLE_BT_GATT_CLIENT	0

/* For bt BLE GATT Server example */
#define CONFIG_EXAMPLE_BT_GATT_SERVER	0

/* For bt SPP (Serial Port Profile) example */
#define CONFIG_EXAMPLE_BT_SPP	0
#endif // CONFIG_BT

#if CONFIG_QQ_LINK
#define FATFS_R_10C
#define FATFS_DISK_USB	0
#define FATFS_DISK_SD 	1
#endif

#if CONFIG_ENABLE_WPS
#define WPS_CONNECT_RETRY_COUNT		4
#define WPS_CONNECT_RETRY_INTERVAL	5000 // in ms
#endif

#define AUTO_RECONNECT_COUNT	8
#define AUTO_RECONNECT_INTERVAL	5 // in sec

#endif
