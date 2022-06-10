BT GATT SERVER EXAMPLE

Description:
Example of how to create and run GATT service on GATT server

Configuration:
[platform_opts.h]
	#define CONFIG_BT 1
	#define CONFIG_EXAMPLE_BT_GATT_SERVER 1

Execution:
When define CONFIG_BT 1 and define CONFIG_EXAMPLE_BT_GATT_SERVER 1,
in example_entry(), it will run example_bt_gatt_server() to create bt_gatt_server_task thread.
Once see "[example_bt_gatt_server] BT initialized" on the console, you can start using GATT apps
such as "LightBlue" on iOS or "nRF Connect" on Android to use GATT service.

Note:
For more details, please refer to "GATT Server" in UM0124 Realtek Ameba-1 bluetooth user manual.pdf
