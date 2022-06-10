#ifndef __BT_GATT_SERVER_H_
#define __BT_GATT_SERVER_H_

#define WRITE_CB_WAIT_TIME_SECOND 180

#if 1
#define GATT_SERVER_ADV_NAME_STR AUDREY_MODEL_SCALE
#else
#define GATT_SERVER_ADV_NAME_STR "Audrey_GATT_Svr"
#endif

typedef enum {
	SERVER_POWER_STATUS_OFF  = 0,
	SERVER_POWER_STATUS_ON_PEND,
	SERVER_POWER_STATUS_ON_ONLY_PEND,
	SERVER_POWER_STATUS_ON,
	SERVER_POWER_STATUS_OFF_PEND
} tBle_ServerPowerStatusEnum;


typedef enum {
	SERVER_IO_STATUS_OFF  = 0,
	SERVER_IO_STATUS_BUSY,
	SERVER_IO_STATUS_READY,
	SERVER_IO_STATUS_NOT_READY,
	SERVER_IO_STATUS_WRITE,
	SERVER_IO_STATUS_WRITE_COMPLETE,
	SERVER_IO_STATUS_READ,
	SERVER_IO_STATUS_READ_COMPLETE,
	SERVER_IO_STATUS_DATA_UPLOAD,
	SERVER_IO_STATUS_DATA_UPLOAD_COMPLETE,
	SERVER_IO_STATUS_DATA_UPLOAD_RESP_WAIT,
	SERVER_IO_STATUS_DATA_UPLOAD_RESP_RECV,
	SERVER_IO_STATUS_DATA_UPLOAD_RESP_COMP,
	SERVER_IO_STATUS_FW_REQ,
	SERVER_IO_STATUS_FW_REQ_COMPLETE,
	SERVER_IO_STATUS_DISC_BEFORE_STOP_SERVER,
} tBle_ServerIoStatusEnum;


/**
 * Type of the general callback function used to handle the execution result of an API request.
 *
 * @param[in]  result  The result.
 */
typedef void (tBT_Gatt_cb)(tBle_ServerIoStatusEnum result);


/**
 * BT GATT サーバー開始
 * BT ON にしてから
 * サーバーを開始する
 * ADV送信を開始する
 * 接続又は切断されたら以下のコールバックを呼ぶ
 * @param connect_cb ... (tBT_Gatt_cb)(tBle_ServerIoStatusEnum result);
 *     BTでのペアリングされて、通信可能状態になった時にコールされる
 *
 * @param disconnect_cb ... (tBT_Gatt_cb)(tBle_ServerIoStatusEnum result);
 *　　　　　BTでのペアリングが解除された時にコールされる
 *     (gattサーバー動作は継続しますので、再度ペアリング可能です)
 */
int start_bt_gatt_server(tBT_Gatt_cb *connect_cb, tBT_Gatt_cb *disconnect_cb);


/**
 * BT GATT サーバー停止
 * サーバーを停止する
 * BT　OFFにする
 */
void stop_bt_gatt_server(void);


/**
 * GATT接続切断要求
 *   切断完了のCallback(disconn_complete_cb)が呼ばれるまで待機してからreturn
 *
 *   @param bool reconnect  TRUE - 切断後再接続可(ADV開始)  FALSE - 切断後再接続不可
 */
void gatt_conn_disc(bool reconnect);

/**
 * BT　をONにしてから、gattサーバーを立ち上げてペアリングされるまで待つ
 * 非同期処理
 * 接続時に　MSG_BT_LINK_COMPメッセージを送信
 * 切断時に MSG_BT_LINK_OFFメッセージを送信
 */
int gatt_open(void);

/**
 * gattサーバーを停止してBTをOFFにする
 */
void gatt_close(void);

/**
 * BLE で　テキストデータを受信する
 *
 * @param int max_len 受信最大長さ
 * @param char *buf
 *
 * @retval 1以上 lenと同じ値　成功
 * @retval 0 データなし
 * @retval -1 エラー
 */
int gatt_read(char *buf, int max_len);

void gatt_set_read_left(int left);


/**
 * BLE で　テキストデータを送信する
 *
 * @param char *data 送信データ
 * @param int len 送信長さ
 *
 * @retval 1以上 lenと同じ値　成功
 * @retval -1 エラー
 */
int gatt_write(char *data, int len);


/**
 * BLE で　テキストデータを送信する
 *  [TODO] 今の実装では、notify_taskから送信する
 * @param char *data 送信データ
 * @param int len 送信長さ
 *
 * @retval 1以上 lenと同じ値　成功
 * @retval -1 エラー
 */
int gatt_write_notify(char *data, int len);


/**
 * writeしてからreadした結果を返す
 * @param wdata ... char *wdata,
 * @param wlen ... write data length
 * @param rdata ... read data buffer
 * @param max_len ... read data max length
 * @retval int ... read length
 */
int gatt_write_and_read(char *wdata, int wlen, char *rdata, int max_len);



/**
 * ATコマンド
 * ATAG=0 停止
 * ATAG=1 開始
 */
void cmd_bt_gatt_server(int argc, char **argv);


/**
 * データ送信要求
 */
void bt_send_data_transport_char(char *data, tUint32 len);

/**
 * データ送信可否チェック
 */
bool is_enabele_data_transport_desc(void);

/**
 * Configurationデータ送信要求
 */
void bt_send_configuration_char(char *data, tUint32 len);

/**
 * Configurationデータ送信可否チェック
 */
bool is_enabele_configuration_desc(void);

/**
 * Advertise送信開始
 */
void svr_set_adv_data(void);


/**
 * BT GATT サーバー状態取得
 */
int status_bt_gatt_server(void);

#if CONFIG_AUDREY_ALWAYS_BT_ON
/**
 * Advertising無効化
 */
void disable_advertising(void);

/**
 * Advertising有効化
 */
void enable_advertising(void);
#endif //#if CONFIG_AUDREY_ALWAYS_BT_ON

#endif // __BT_GATT_SERVER_H_
