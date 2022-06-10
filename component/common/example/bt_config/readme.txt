BT CONFIG EXAMPLE

Description:
Enable BT Config app 

Configuration:
[platform_opts.h]
	#define CONFIG_BT 1
	#define CONFIG_EXAMPLE_BT_CONFIG 1

Execution:
When define CONFIG_BT 1 and define CONFIG_EXAMPLE_BT_CONFIG 1,
in example_entry(), it will run example_bt_config() to create bt_config_task thread.
once see "[BT Config] BT initialized" on the console, you can start using BT Config App
to connect to Ameba.

Note:
1. Ameba stops broadcasting ADV packest when (1) BT connection established (2) Ameba is connected to AP.
2. BT Config app and user guide is in tools\bluetooth\BT Config\
3. bt_config_cmdThread_init(): a command thread listening to bt_config commands.
4. For more details, please refer to "BT Config" in UM0124 Realtek Ameba-1 bluetooth user manual.pdf
