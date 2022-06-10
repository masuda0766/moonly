/*
 * Copyright (C) 2017 SHARP Corporation. All rights reserved.
 */

/**
 * 無線通信制御部 (Wifi接続固有処理)
 */
#include <platform_opts.h>
#include <lwip_netconf.h>
#include <platform/platform_stdlib.h>
#include <cJSON.h>
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <http_client.h>
#include "FreeRTOS.h"
#include "task.h"
#include "rf_ctrl.h"
#include "rf_ctrl_wifi.h"
#include "ssl_my_func.h"
#include "state_manager.h"

#if CONFIG_USE_MBEDTLS
#include "mbedtls/config.h"
#include "mbedtls/platform.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "mbedtls/error.h"
#include "mbedtls/debug.h"
#endif // CONFIG_USE_MBEDTLS

#define SOCKET_CHECK_PERIOD_MS			100
#define SOCKET_CHECK_RETRY_MAX			100
#define CONNECT_RETRY_MAX				10

#define HTTP_REQ_BUF_LENGTH				1024
#define HTTP_RESP_BUF_LENGTH			2048
#define HTTP_RESP_STATE_OK				200
#define HTTP_RESP_STATE_BADREQ			400

//#define USE_LOCAL_SERVER
#ifdef USE_LOCAL_SERVER
#define DATA_UPLOAD_PORT		80
#define DATA_UPLOAD_HOST		"192.168.0.60"
#else
#ifdef RF_CTRL_WIFI_USE_SSL
#define DATA_UPLOAD_PORT		"443"
#define DATA_UPLOAD_HOST		AUDREY_SERVER_NAME
#else
#define DATA_UPLOAD_PORT		8080
#define DATA_UPLOAD_HOST		"211.130.207.80"
#endif // RF_CTRL_WIFI_USE_SSL
#endif // USE_LOCAL_SERVER
#define DATA_UPLOAD_RESOURCE	AUDREY_SERVER_TYPE"/monitor_data"
#define DATA_UPLOAD_API_KEY		AUDREY_API_KEY

static volatile int		type_id = 0;

#ifdef RF_CTRL_WIFI_USE_SSL
static char		tmp_buf[HTTP_RESP_BUF_LENGTH];
static char		resp_buf[HTTP_RESP_BUF_LENGTH];
static mbedtls_net_context	server_fd = {-1};
static mbedtls_ssl_context	ssl;
static mbedtls_ssl_config	conf;

void rf_ctrl_ssl_wait_resp_init(void);
RF_CTRL_ERR rf_ctrl_ssl_close_connection(void);
#else
#define SOCKET_DESCRIPTOR_INVALID		(-1)
static volatile int		server_socket = SOCKET_DESCRIPTOR_INVALID;

void rf_ctrl_wait_resp_wifi_init(void);
#endif // RF_CTRL_WIFI_USE_SSL


void rf_ctrl_set_server_error_log(int id)
{
	int err_id;

	switch(id) {
		case TYPE_ID_ENTERING:
#if CONFIG_AUDREY_DBG_UPLOAD
		case TYPE_ID_ENTERING_DBG:
#endif
#if CONFIG_AUDREY_UPLOAD_RETRY
			if (is_upload_retry() != 0)
				err_id = PARAM_DUL_ERR_SERVER_ENTRY_R;
			else
#endif
				err_id = PARAM_DUL_ERR_SERVER_ENTRY;
			break;

		case TYPE_ID_PERIODIC:
#if CONFIG_AUDREY_UPLOAD_RETRY
			if (is_upload_retry() != 0)
				err_id = PARAM_DUL_ERR_SERVER_PERIOD_R;
			else
#endif
				err_id = PARAM_DUL_ERR_SERVER_PERIOD;
			break;

		default:
			err_id = PARAM_DUL_ERR_SERVER;
			break;
	}
	SendMessageToStateManager(MSG_ERR_DUL, err_id);
}




/* HTTP通信 応答待ち状態チェック */
bool rf_ctrl_wait_http_resp(void)
{
	int		cnt = 0;
#ifdef RF_CTRL_WIFI_USE_SSL
	while (server_fd.fd >= 0) {
#else
	while (server_socket != SOCKET_DESCRIPTOR_INVALID) {
#endif
		/* socketのディスクリプタの値が初期値でなければ一定時間待機 */
		vTaskDelay(SOCKET_CHECK_PERIOD_MS * portTICK_PERIOD_MS);
		if (++cnt > SOCKET_CHECK_RETRY_MAX) {
			/* 一定回数繰り返してもsocketが空かなければ送信を断念 */
			Log_Error("\r\n[RF Ctrl] Socket used other process\r\n");
			return FALSE;
		}
	}

	/* server_socket が未設定  = HTTP Request未送信 or Response受信済
	 * -> 次のHTTP Request送信 や Wifi OFF を実施可能 */
	return TRUE;
}

/* HTTP通信 応答待ち強制停止 */
bool rf_ctrl_force_stop_waiting_resp(void)
{
#ifdef RF_CTRL_WIFI_USE_SSL
	if (server_fd.fd >= 0) {
		rf_ctrl_ssl_close_connection();
	}
#else
	if (server_socket == SOCKET_DESCRIPTOR_INVALID) {
		return TRUE;
	}

	if (close(server_socket) < 0) {
		Log_Error("\r\n[RF Ctrl] Socket close failed\r\n");
		SendMessageToStateManager(MSG_ERR_DUL, PARAM_DUL_ERR_COMM);
		return FALSE;
	}
#endif
	return TRUE;
}

#ifndef RF_CTRL_WIFI_USE_SSL
/* 測定データ送信 (Wifi) */
RF_CTRL_ERR rf_ctrl_data_upload_wifi(int id, char *data)
{
	char				buf[1024] = {0};
	int					read_size = 0;
	struct sockaddr_in	server_addr;
	struct hostent		*server;
	int					ret = 0;
	int					cnt = 0;

	/* HTTP通信 応答待ち状態チェック */
	if (rf_ctrl_wait_http_resp() != TRUE) {
		/* socketが空かなければ送信を断念 */
		return RF_ERR_NETWORK;
	}

	/* socket生成＆接続処理 */
	do {
		server_socket = socket(AF_INET, SOCK_STREAM, 0);
		if(server_socket < 0){
			Log_Error("\r\n[RF Ctrl] Create socket failed\r\n");
			SendMessageToStateManager(MSG_ERR_DUL, PARAM_DUL_ERR_COMM);
			return RF_ERR_SYSTEM;
		}

		server = gethostbyname(DATA_UPLOAD_HOST);
		if(server == NULL){
			Log_Error("\r\n[RF Ctrl] Get host ip failed\r\n");
			SendMessageToStateManager(MSG_ERR_DUL, PARAM_DUL_ERR_COMM);
			return RF_ERR_SYSTEM;
		}

		memset(&server_addr,0,sizeof(server_addr));
		server_addr.sin_family = AF_INET;
		server_addr.sin_port = htons(DATA_UPLOAD_PORT);
		memcpy(&server_addr.sin_addr.s_addr,server->h_addr,server->h_length);

		ret = SOCKET_DESCRIPTOR_INVALID;
		ret = connect(server_socket,(struct sockaddr *)&server_addr, sizeof(server_addr));
		if (ret >= 0) {
			Log_Info("\r\n[RF Ctrl] Connection to server is completed\r\n");
			break;
		} else {
			Log_Error("\r\n[RF Ctrl] Connect failed\r\n");
			close(server_socket);
			server_socket = SOCKET_DESCRIPTOR_INVALID;
		}
		cnt++;
		if ((cnt > CONNECT_RETRY_MAX) || (rf_ctrl_check_connection() != RF_ERR_SUCCESS)) {
			/* 一定回数繰り返しても接続できない、または接続が切れている場合はエラー終了 */
			Log_Error("\r\n[RF Ctrl] Connection to server is failed\r\n");
			rf_ctrl_set_server_error_log(id);
			return RF_ERR_NETWORK;
		}
	} while(ret < 0);

	Log_Info("\r\n[RF_Ctrl] ========== HTTP request ==========\r\n");

	/* POSTリクエスト送信 */
	sprintf(buf, "POST %s HTTP/1.1\r\nHost: %s\r\nContent-Type: application/json\r\nContent-Length: %d\r\nX-api-key: %s\r\n\r\n",
			DATA_UPLOAD_RESOURCE, DATA_UPLOAD_HOST, strlen(data), DATA_UPLOAD_API_KEY);
	send(server_socket, buf, strlen(buf), 0);
#if CONFIG_AUDREY_DBG_UPLOAD
	Log_Debug("%s", buf);
#else
	Log_Info("%s", buf);
#endif
	send(server_socket, data, strlen(data), 0);
	Log_Info("%s\r\n============================================\r\n", data);

	/* 送信したデータのタイプIDを記憶 (受信時に利用) */
	type_id = id;

	/* 応答受信待ち開始 */
	rf_ctrl_wait_resp_wifi_init();

	return RF_ERR_SUCCESS;
}

TaskHandle_t handle_wifi_resp = NULL;
static void rf_ctrl_wait_response_wifi(void *param)
{
	char	buf[HTTP_RESP_BUF_LENGTH] = {0};
	int		read_size = 0;
	int		cnt = 0;
	int		i = 0;
	int		http_resp_state = 0;
	char *	response_body = NULL;

	if (server_socket == SOCKET_DESCRIPTOR_INVALID) {
		/* socketがclose済みの場合は何もせずに終了 */
		Log_Error("\r\n[RF Ctrl] HTTP response receive failed : not connected\r\n");
		goto end;
	}

	/* 応答受信 */
	memset(buf, 0, sizeof(buf));
	Log_Info("\r\n[RF_Ctrl] ========= HTTP  response =========\r\n");
	while (1) {
		vTaskDelay(10);

		read_size = recv(server_socket, &buf[cnt], sizeof(buf)-cnt, 0);
		if (read_size > 0) {
			cnt += read_size;
			if (cnt > HTTP_RESP_BUF_LENGTH) {
				Log_Error("\r\n[RF Ctrl] HTTP response buffer over flow : %d\r\n%s\r\n", cnt, buf);
				SendMessageToStateManager(MSG_ERR_DUL, PARAM_DUL_ERR_COMM);
				goto err_end;
			}
		} else if (read_size < 0) {
			Log_Error("\r\n[RF Ctrl] HTTP response receive failed : %d\r\n", errno);
			SendMessageToStateManager(MSG_ERR_DUL, PARAM_DUL_ERR_RESP_ERR);
			break;
		} else {
			/* read_size == 0 ： 正常に接続断 */
			break;
		}
	}
	Log_Info("%s\r\n============================== len:%4d ===\r\n", buf, cnt);

	if (read_size >= 0) {
		/* "HTTP/" までは固定なので、その先からステータス番号の位置を検索 */
		for (i = 5 ; i < strlen(buf) - 3 ; i++ ) {	/* buf の領域外をアクセスしないようにカウンタ上限値に注意 */
			if ((buf[i] == ' ') && ((buf[i+1] >= '1' ) && (buf[i+1] <= '5'))) {
				http_resp_state = (buf[i+1]-'0')*100 + (buf[i+2]-'0')*10 + (buf[i+3]-'0');
				break;
			}
		}
		switch (http_resp_state) {
		/* 200(OK)と400(Bad Request)は再送しない */
		case HTTP_RESP_STATE_OK:
		case HTTP_RESP_STATE_BADREQ:
			Log_Info("\r\n[RF Ctrl] HTTP response OK\r\n");
			response_body = http_response_body(buf, strlen(buf));
			break;
		default:
			Log_Error("\r\n[RF Ctrl][ssl] HTTP response error : %d\r\n", http_resp_state);
			/* 下３桁はレスポンスコードのエラーコード */
			int err_f;
#if CONFIG_AUDREY_DBG_UPLOAD
			if(type_id == TYPE_ID_ENTERING || type_id == TYPE_ID_ENTERING_DBG) {
#else
			if(type_id == TYPE_ID_ENTERING) {
#endif
				err_f = 0;
			} else if( type_id == TYPE_ID_PERIODIC) {
				err_f = 10;
			} else {
				err_f = 20;
			}
			int err_h = http_resp_state / 100;
			int err_l = http_resp_state % 100;
			SendMessageToStateManager(MSG_ERR_RSP + err_f + err_h, err_l);
			break;
		}
	}
	rf_ctrl_parse_upload_resp(type_id, response_body);
	if (response_body != NULL) {
		http_free(response_body);
	}

err_end:
	close(server_socket);
	server_socket = SOCKET_DESCRIPTOR_INVALID;

end:
	if (handle_wifi_resp != NULL) {
		vTaskDelete(handle_wifi_resp);
	}
	return;
}

void rf_ctrl_wait_resp_wifi_init(void)
{
	if(xTaskCreate(rf_ctrl_wait_response_wifi, ((const char*)"rf_ctrl_wait_response_wifi"), 1024, NULL, tskIDLE_PRIORITY + 2 , &handle_wifi_resp) != pdPASS)
		Log_Error("\n\r%s xTaskCreate(rf_ctrl_wait_response_wifi) failed", __FUNCTION__);
}
#else  // RF_CTRL_WIFI_USE_SSL
#define SSL_CLIENT_EXT
extern int ssl_client_ext_init(void);
extern void ssl_client_ext_free(void);
extern int ssl_client_ext_setup(mbedtls_ssl_config *conf);
extern bool dataup_ssl_fail_flg;
RF_CTRL_ERR rf_ctrl_ssl_create_connection(int id)
{
	int ret, len;
	int retry_count = 0;

	/* 重複呼び出し防止 */
	if (server_fd.fd >= 0) {
		Log_Error(" failed\n\r  ! already used\n\r");
		SendMessageToStateManager(MSG_ERR_DUL, PARAM_COMMON_BUSY);
		return RF_ERR_STATE;
	}

	ssl_my_alloc_init();
	mbedtls_platform_set_calloc_free(ssl_my_calloc, ssl_my_free);
	update_my_alloc_init();
#if defined(MBEDTLS_DEBUG_C)
	mbedtls_debug_set_threshold(DEBUG_LEVEL);
#endif

	/*
	 * 1. Start the connection
	 */
	Log_Info("\n\r  . Connecting to tcp/%s/%s...", DATA_UPLOAD_HOST, DATA_UPLOAD_PORT);

	mbedtls_net_init(&server_fd);

	if((ret = mbedtls_net_connect(&server_fd, DATA_UPLOAD_HOST, DATA_UPLOAD_PORT, MBEDTLS_NET_PROTO_TCP)) != 0) {
		Log_Error(" failed\n\r  ! mbedtls_net_connect returned %d\n", ret);
		if(ret == MBEDTLS_ERR_NET_CONNECT_FAILED) {
			dataup_ssl_fail_flg = TRUE;
		} else {
			dataup_ssl_fail_flg = FALSE;
		}
		mbedtls_net_free(&server_fd);
		rf_ctrl_set_server_error_log(id);
		return RF_ERR_NETWORK;
	}

	Log_Info(" ok\n");

	/*
	 * 2. Setup stuff
	 */
	Log_Info("  . Setting up the SSL/TLS structure...");

	mbedtls_ssl_init(&ssl);
	mbedtls_ssl_config_init(&conf);

#ifdef SSL_CLIENT_EXT
	if((ret = ssl_client_ext_init()) != 0) {
		Log_Error(" failed\n\r  ! ssl_client_ext_init returned %d\n", ret);
		SendMessageToStateManager(MSG_ERR_DUL, PARAM_DUL_ERR_SSL);
		goto exit;
	}
#endif

	mbedtls_ssl_set_bio(&ssl, &server_fd, my_ssl_send, my_ssl_read, NULL);

	if((ret = mbedtls_ssl_config_defaults(&conf,
		MBEDTLS_SSL_IS_CLIENT,
		MBEDTLS_SSL_TRANSPORT_STREAM,
		MBEDTLS_SSL_PRESET_DEFAULT)) != 0) {
		Log_Error(" failed\n  ! mbedtls_ssl_config_defaults returned %d\n\n", ret);
		SendMessageToStateManager(MSG_ERR_DUL, PARAM_DUL_ERR_SSL);
		goto exit;
	}

	mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_REQUIRED);
	mbedtls_ssl_conf_rng(&conf, ssl_my_random, NULL);
	mbedtls_ssl_conf_dbg(&conf, ssl_my_debug, NULL);

#ifdef SSL_CLIENT_EXT
	if((ret = ssl_client_ext_setup(&conf)) != 0) {
		Log_Error(" failed\n\r  ! ssl_client_ext_setup returned %d\n", ret);
		SendMessageToStateManager(MSG_ERR_DUL, PARAM_DUL_ERR_SSL);
		goto exit;
	}
#endif

	if((ret = mbedtls_ssl_setup(&ssl, &conf)) != 0) {
		Log_Error(" failed\n  ! mbedtls_ssl_setup returned %d\n\n", ret);
		rf_ctrl_set_server_error_log(id);
		goto exit;
	}

	Log_Info(" ok\n");

	/*
	 * 3. Handshake
	 */
	Log_Info("\n\r  . Performing the SSL/TLS handshake...");

	retry_count = 0;
	while((ret = mbedtls_ssl_handshake(&ssl)) != 0) {
		if((ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE
			&& ret != MBEDTLS_ERR_NET_RECV_FAILED) || retry_count >= 5) {
			Log_Error(" failed\n\r  ! mbedtls_ssl_handshake returned -0x%x\n", -ret);
			rf_ctrl_set_server_error_log(id);
			goto exit;
		}

		retry_count++;
	}

	Log_Info(" ok\n");
	Log_Info("\n\r  . Use ciphersuite %s\n", mbedtls_ssl_get_ciphersuite(&ssl));

	dataup_ssl_fail_flg = FALSE;
	return RF_ERR_SUCCESS;

exit:
	dataup_ssl_fail_flg = FALSE;
#ifdef MBEDTLS_ERROR_C
	if(ret != 0) {
		char error_buf[100];
		mbedtls_strerror(ret, error_buf, 100);
		Log_Error("Last error was: 0x%04x - %s\n\n", ret, error_buf);
	}
#endif

	mbedtls_net_free(&server_fd);
	mbedtls_ssl_free(&ssl);
	mbedtls_ssl_config_free(&conf);

#ifdef SSL_CLIENT_EXT
	ssl_client_ext_free();
#endif

	return RF_ERR_NETWORK;
}

RF_CTRL_ERR rf_ctrl_ssl_close_connection(void)
{
	mbedtls_ssl_close_notify(&ssl);

	mbedtls_net_free(&server_fd);
	mbedtls_ssl_free(&ssl);
	mbedtls_ssl_config_free(&conf);

#ifdef SSL_CLIENT_EXT
	ssl_client_ext_free();
#endif

	return RF_ERR_SUCCESS;
}

/* 測定データ送信 (Wifi/SSL) */
#define MAX_WRITE_LENGTH			1024
#define MAX_BODY_LENGTH				4096
#define MAX_HEADER_LENGTH			512
char __attribute__((section(".ssldata.ssl_buf"))) ssl_buf[MAX_BODY_LENGTH + MAX_HEADER_LENGTH];

RF_CTRL_ERR rf_ctrl_data_upload_ssl(int id, char *data)
{
	char				*buf = NULL;
	int					read_size = 0;
	struct sockaddr_in	server_addr;
	struct hostent		*server;
	int					ret = 0;
	int					i = 0, j = 0;
	int					len = 0;

	data_up_req_start();
	if (rf_ctrl_ssl_create_connection(id) != RF_ERR_SUCCESS) {
		Log_Error("\r\n[RF Ctrl] create ssl connection failed\r\n");
		data_up_req_end();
		return RF_ERR_NETWORK;
	}
	Log_Info("\r\n[RF Ctrl] create ssl connection completed\r\n");

	if (strlen(data) < MAX_BODY_LENGTH) {
		/*** POSTリクエスト送信 ***/
		sprintf(ssl_buf, "POST %s HTTP/1.1\r\nHost: %s\r\nContent-Type: application/json\r\nContent-Length: %d\r\nX-api-key: %s\r\n\r\n%s",
				DATA_UPLOAD_RESOURCE, DATA_UPLOAD_HOST, strlen(data), DATA_UPLOAD_API_KEY, data);
		Log_Info("\r\n[RF Ctrl] ========== HTTP REQUEST ==========\r\n");
		Log_Info("%.4096s", ssl_buf);	/* %s の精度(＝文字列出力の最大長)は MAX_BODY_LENGTH に合わせること */
		ret = mbedtls_ssl_write(&ssl, ssl_buf, strlen(ssl_buf));
		if (ret <= 0) {
			if(ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
				Log_Error(" failed\n\r  ! mbedtls_ssl_write (header+body) returned %d\n", ret);
				SendMessageToStateManager(MSG_ERR_DUL, PARAM_DUL_ERR_REQ_ERR);
				goto exit;
			}
		}
		Log_Info("\r\n============================================ len:%d\r\n", strlen(ssl_buf));
		goto write_exit;
	}

	/*** POSTリクエスト送信 ***/
	/* ヘッダ部を送信 */
	buf = malloc(MAX_WRITE_LENGTH);
	if (buf == NULL) {
		Log_Error("\r\n[RF Ctrl] %s : malloc failed\r\n", __FUNCTION__);
		SendMessageToStateManager(MSG_ERR_DUL_FATAL, PARAM_COMMON_NOMEM);
		data_up_req_end();
		return RF_ERR_SYSTEM;
	}
	sprintf(buf, "POST %s HTTP/1.1\r\nHost: %s\r\nContent-Type: application/json\r\nContent-Length: %d\r\nX-api-key: %s\r\n\r\n",
			DATA_UPLOAD_RESOURCE, DATA_UPLOAD_HOST, strlen(data), DATA_UPLOAD_API_KEY);
	Log_Info("\r\n[RF Ctrl] ========== HTTP request ==========\r\n");
	Log_Info("%s\r\n", buf);
	ret = mbedtls_ssl_write(&ssl, buf, strlen(buf));
	if (ret <= 0) {
		if(ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
			Log_Error(" failed\n\r  ! mbedtls_ssl_write (header) returned %d\n", ret);
			SendMessageToStateManager(MSG_ERR_DUL, PARAM_DUL_ERR_REQ_ERR);
			goto exit;
		}
	}

	/* ボディ部を送信 (一定サイズで分割送信) */
	ret = 0;
	for (i = 0; i < strlen(data); i += ret) {
		j++;	/* 分割送信回数カウント */
		len = strlen(data) - i;
		if (len > MAX_WRITE_LENGTH) {
			len = MAX_WRITE_LENGTH;
		} else if (len <= 0) {
			/* 通常あり得ない(for分の判定で抜ける)条件だがガードとして入れておく */
			break;
		}
		ret = mbedtls_ssl_write(&ssl, data + i, len);
		if (ret <= 0) {
			if(ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
				Log_Error(" failed\n\r  ! mbedtls_ssl_write (boby:%d) returned %d\n", j, ret);
				SendMessageToStateManager(MSG_ERR_DUL, PARAM_DUL_ERR_REQ_ERR_B);
				goto exit;
			}
		}
		Log_Info("%.1024s", data + i);	/* %s の精度(＝文字列出力の最大長)は MAX_WRITE_LENGTH に合わせること */
	}
	Log_Info("\r\n============================================ len:%d\r\n", strlen(buf) + strlen(data));
	free(buf);

write_exit:
	/* 送信したデータのタイプIDを記憶 (受信時に利用) */
	type_id = id;

	/* 応答受信待ち開始 */
	rf_ctrl_ssl_wait_resp_init();

	return RF_ERR_SUCCESS;

exit:
	free(buf);
	rf_ctrl_ssl_close_connection();

	data_up_req_end();
	return RF_ERR_NETWORK;
}

TaskHandle_t handle_ssl_resp = NULL;
static void rf_ctrl_ssl_wait_response(void *param)
{
	int		cnt = 0;
	int		i = 0;
	int		http_resp_state = 0;
	char	*response_body = NULL;
	int		ret = 0;

	Log_Info("\r\n[RF Ctrl] HTTPS response receive start : handle 0x%0x\r\n", handle_ssl_resp);

	if (server_fd.fd < 0) {
		/* socketがclose済みの場合は何もせずに終了 */
		Log_Error("\r\n[RF Ctrl] HTTPS response receive failed : not connected\r\n");
		goto end;
	}

	/* 応答受信 */
	Log_Info("\r\n[RF_Ctrl] ========= HTTP  response =========\r\n");
	do {
		memset(tmp_buf, 0, HTTP_RESP_BUF_LENGTH);
		ret = mbedtls_ssl_read(&ssl, tmp_buf, HTTP_RESP_BUF_LENGTH);

		/* エラー時 or EOF時 はそのまま抜ける */
		if(ret < 0) {
			Log_Error(" failed\n  ! mbedtls_ssl_read returned %d\n", ret);
			SendMessageToStateManager(MSG_ERR_DUL, PARAM_DUL_ERR_RESP_ERR);
			break;
		}

		if(ret == 0) {
			Log_Info("\n\nEOF\n\n");
			break;
		}

		/* 受信成功時は受信データをバッファに保存 */
		if (cnt + strlen(tmp_buf) > HTTP_RESP_BUF_LENGTH) {
			Log_Error("\r\n[RF Ctrl] HTTPS response buffer over flow : %d\r\n%s\r\n", cnt + ret, tmp_buf);
			rf_ctrl_ssl_close_connection();
			goto end;
		}
		memset(resp_buf, 0, HTTP_RESP_BUF_LENGTH);
		strcat(resp_buf, tmp_buf);
		cnt += strlen(tmp_buf);

		/* 続きのデータがある場合は再度read */
		if(ret == MBEDTLS_ERR_SSL_WANT_READ) {
			continue;
		}

		/* Close Notify が来た場合は受信終了 */
		if(ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
			break;
		}

		/* Comment about mbedtls_ssl_read() in mbedtls/ssh.h
		 *
		 * \note           If this function returns something other than a positive
		 *                 value or MBEDTLS_ERR_SSL_WANT_READ/WRITE or
		 *                 MBEDTLS_ERR_SSL_CLIENT_RECONNECT, then the ssl context
		 *                 becomes unusable, and you should either free it or call
		 *                 \c mbedtls_ssl_session_reset() on it before re-using it for
		 *                 a new connection; the current connection must be closed.
		 *
		 * => ret が 上記以外の正の値であれば受信完了しているのでループを抜ける
		 */
		break;
	} while(1);
	Log_Info("%s\r\n", resp_buf);
	Log_Info("============================== len:%4d ===\r\n", cnt);
	rf_ctrl_ssl_close_connection();

	if(ret >= 0) {
		/* "HTTP/" までは固定なので、その先からステータス番号の位置を検索 */
		for (i = 5 ; i < strlen(resp_buf) - 3 ; i++ ) {	/* buf の領域外をアクセスしないようにカウンタ上限値に注意 */
			if ((resp_buf[i] == ' ') && ((resp_buf[i+1] >= '1' ) && (resp_buf[i+1] <= '5'))) {
				http_resp_state = (resp_buf[i+1]-'0')*100 + (resp_buf[i+2]-'0')*10 + (resp_buf[i+3]-'0');
				break;
			}
		}

		switch (http_resp_state) {
		/* 200(OK)と400(Bad Request)は再送しない */
		case HTTP_RESP_STATE_OK:
		case HTTP_RESP_STATE_BADREQ:
			Log_Info("\r\n[RF Ctrl][ssl] HTTP response OK\r\n");
			response_body = http_response_body(resp_buf, strlen(resp_buf));
			break;
		default:
			Log_Error("\r\n[RF Ctrl][ssl] HTTP response error : %d\r\n", http_resp_state);
			/* 下３桁はレスポンスコードのエラーコード */
			int err_f;
#if CONFIG_AUDREY_DBG_UPLOAD
			if(type_id == TYPE_ID_ENTERING || type_id == TYPE_ID_ENTERING_DBG) {
#else
			if(type_id == TYPE_ID_ENTERING) {
#endif
				err_f = 0;
			} else if( type_id == TYPE_ID_PERIODIC) {
				err_f = 10;
			} else {
				err_f = 20;
			}
			int err_h = http_resp_state / 100;
			int err_l = http_resp_state % 100;
			SendMessageToStateManager(MSG_ERR_RSP + err_f + err_h, err_l);
			break;
		}
	}
	rf_ctrl_parse_upload_resp(type_id, response_body);
	if (response_body != NULL) {
		http_free(response_body);
	}

end:
	if (handle_ssl_resp != NULL) {
		vTaskDelete(handle_ssl_resp);
	}
	data_up_req_end();

	return;
}

void rf_ctrl_ssl_wait_resp_init(void)
{
	if(xTaskCreate(rf_ctrl_ssl_wait_response, ((const char*)"rf_ctrl_ssl_wait_response"), 1024, NULL, tskIDLE_PRIORITY + 2 , &handle_ssl_resp) != pdPASS) {
		Log_Error("\n\r%s xTaskCreate(rf_ctrl_ssl_wait_response) failed", __FUNCTION__);
		SendMessageToStateManager(MSG_ERR_REBOOT, PARAM_COMMON_TASK_CREATE_FAIL);
	}
}
#endif // RF_CTRL_WIFI_USE_SSL
