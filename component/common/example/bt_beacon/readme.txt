BT BEACON EXAMPLE

Description:
Sending Beacon adv packets

Configuration:
[platform_opts.h]
	#define CONFIG_BT 1
	#define CONFIG_EXAMPLE_BT_BEACON 1

Apple iBeacon:
[example_bt_beacon.c]
	#define BEACON_TYPE BEACON_TYPE_IBEACON

Radius Networks AltBeacon:
[example_bt_beacon.c]
	#define BEACON_TYPE BEACON_TYPE_ALTBEACON


Execution:
When define CONFIG_BT 1 and define CONFIG_EXAMPLE_BT_BEACON 1
In example_entry(), it will run example_bt_beacon() to create bt_beacon_task thread.
Ameba will start broadcasting beacons.

Note:
For more details, please refer to "Beacon" in UM0124 Realtek Ameba-1 bluetooth user manual.pdf
