#ifndef __BLE_SCAN_H__
#define __BLE_SCAN_H__

typedef enum {
	SCAN_POWER_STATUS_OFF  = 0,
	SCAN_POWER_STATUS_ON_PEND,
	SCAN_POWER_STATUS_ON_ONLY_PEND,
	SCAN_POWER_STATUS_ON,
	SCAN_POWER_STATUS_OFF_PEND
} tBle_ScanPowerStatusEnum;


void ble_scan_init(void);

// LE scan start
// It is only valid for 10 seconds
void start_le_scan(void);

// LE scan stop
void stop_le_scan(void);

// in  sw == 0 : BT power off
// in  sw == 1 : BT power on
void power_le_scan(int sw);


int status_le_scan(void);

#if CONFIG_AUDREY_ALWAYS_BT_ON
void scan_gap_ev_cb(tBT_GapEvEnum ev, void *param);
#endif //#if CONFIG_AUDREY_ALWAYS_BT_ON

/* example
 power_le_scan(1);
 3 seconds until usable ...

 start_le_scan();
 stop_le_scan();
 start_le_scan();
 stop_le_scan();
 ....
 power_le_scan(0);

*/
#endif // __BLE_SCAN_H__
