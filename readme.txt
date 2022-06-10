Example Description

This patch describes how to implement bluetooth in SDK v4.0c by using GCC release project

Introdution:
This patch includes BT libraries and BT examples. bt_beacon is an example that Ameba can broadcast 
Apple iBeacon or Radius Networks AltBeacon. bt_config is an example which provides a simple way for Ameba 
to associate to AP. bt_gatt_server shows how to customize GATT service. bt_gatt_client provides a way to 
access remote GATT server. bt_spp shows how to establish Serial Port Profile connection.

Useage:
1.
	(a) Replace files:
			component\common\api\at_cmd\atcmd_mp.c
			component\common\api\at_cmd\atcmd_mp.h
			component\common\api\at_cmd\log_service.c
			component\common\api\wifi\wifi_conf.c
			component\common\api\wifi\wifi_util.c
			component\common\drivers\wlan\realtek\include\autoconf.h
			component\common\drivers\wlan\realtek\include\wifi_constants.h
			component\common\example\example_entry.c
			component\common\example\uart_atcmd\example_uart_atcmd.h
			component\common\mbed\targets\hal\rtl8195a\pwmout_api.c
			component\os\os_dep\include\device_lock.h
			component\soc\realtek\8195a\fwlib\rtl8195a\rtl8195a_pwm.h
			component\soc\realtek\8195a\fwlib\rtl8195a\src\rtl8195a_pwm.c
			component\soc\realtek\8195a\fwlib\hal_pwm.h
			component\soc\realtek\8195a\fwlib\src\hal_pwm.c
			component\soc\realtek\8195a\misc\bsp\lib\common\GCC\lib_platform.a
			component\soc\realtek\8195a\misc\bsp\lib\common\GCC\lib_wlan.a
			component\soc\realtek\8195a\misc\bsp\lib\common\GCC\lib_wlan_mp.a
			project\realtek_ameba1_va0_example\GCC-RELEASE\application.mk
			project\realtek_ameba1_va0_example\GCC-RELEASE\eclipse\application\.cproject
			project\realtek_ameba1_va0_example\GCC-RELEASE\eclipse\application\.project
			project\realtek_ameba1_va0_example\GCC-RELEASE\rlx8195A-symbol-v02-img2.ld
			project\realtek_ameba1_va0_example\inc\platform_opts.h
			
	(b) Add files:
			component\common\api\at_cmd\atcmd_bt.c
			component\common\api\at_cmd\atcmd_bt.h
			component\common\bluetooth\
			component\common\example\bt_beacon\
			component\common\example\bt_config\
			component\common\example\bt_gatt_client\
			component\common\example\bt_gatt_server\
			component\common\example\bt_spp\
			component\common\mbed\targets\hal\rtl8195a\analogout_api.c			
			component\soc\realtek\8195a\misc\bsp\lib\common\GCC\lib_bt.a
			component\soc\realtek\8195a\misc\bsp\lib\common\GCC\lib_btsdk.a
			doc\BluetoothAPI.chm
			doc\UM0124 Realtek Ameba-1 bluetooth user manual.pdf
			tools\bluetooth\

2. This patch is backward compatible with SDK v4.0a_gcc_22259.
	To use sdk-ameba-v4.0a_gcc_22259, please replace project\realtek_ameba1_va0_example\GCC-RELEASE\application.mk with application_4.0a.mk.
	To use sdk-ameba-v4.0c_gcc, please replace project\realtek_ameba1_va0_example\GCC-RELEASE\application.mk with application_4.0c.mk.

3. Set CONFIG_BT flag in project\realtek_ameba1_va0_example\inc\platform_opts.h to enable Bluetooth.

4. The example configuration please refer to doc\UM0124 Realtek Ameba-1 bluetooth user manual.pdf.

5. The bt_config apps are in tools\bluetooth\BT Config\.

Release Note
	(IAR)
	4.0a_v01
		Add BT libraries.
		Add BT examples: bt_beacon, bt_config, bt_gatt_server, bt_spp.
	4.0a_v02
		Fix Android BT Config scan fail issue.
		Add BT Scan result parsing.
	4.0a_v03
		Support Debug mode.
		Enhance GATT Server and SPP stability.
	4.0a_v04
		Add BT example: GATT Client.
		Refine BT Scan result parsing.
	4.0a_v05
		Fix GATT connect error on some Android phone.
	(GCC)
	4.0a_gcc_v01
		Update BT firmware.
		Check BT is on before doing BT AT command.
                Force BLE and classic BT (BR) to use different BT address.
		Add device name in BR EIR data in bt_config and bt_spp.
		Fix GATT Client service discovery error.
		  (When Service End Handle is 0xFFFF,it can't reach service discovery complete event)
		Fix pairing fail issue when BLE using Random address
	4.0a_gcc_v02
		Fix Write Long Characteristic Values and Read Long Characteristic Values in example_bt_gatt_server
	4.0a_gcc_v03
		Add Eclipse project file
		Fix Wlan KRACK vulnerability
		Avoid using UART2 in example_uart_atcmd
	4.0a_gcc_v04
		Update BT library.
		Support BLE continuous scan.
		Support BLE scan without filtering.
		Refine BT Scan result: print result during scan.
		Add ATBI: get rssi of connected device.
		BT UART TX uses DMA mode to improve throughput.
		Default get BT MAC address from BT efuse
		Add .wlan section name in lib_wlan (Be able to move lib_wlan to SDRAM if needed).
	4.0a_gcc_v05
		Fix issue AT cmd no response when repeatedly wifi on/off
	4.0c_gcc_v01
		A patch for SDK sdk-ameba-v4.0c_gcc. It is backward compatible with SDK v4.0a_gcc_22259.
	4.0c_gcc_v02
		Update BT library
		Fix BT address used in GATT Client connect (force using BLE random address).
		Add update connection interval event.
		Support setting BLE scan interval and window.
		Support pairing data management: get / delete.
		Update lib_wlan (the same as  4.0a_patch_gcc_wifi_logo_with_bt_coexist_(v01).zip )
			NOTE: The lib_wlan.a of wifi logo is larger, it will cause BT example build fail (SRAM not enough) by default.
		