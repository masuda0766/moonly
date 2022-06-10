#ifndef __BT_MISC_H__
#define __BT_MISC_H__

#include "PinNames.h"

#define BT_CHIP_EN					PE_5
#define BT_WLAN_ACT					PC_3
#define BT_SET						PC_1
#define BT_ACT						PC_0
#define BT_WAKE_HOST				PE_2

void bt_reset_bt_chip(bool enable);

#endif /* __BT_MISC_H__ */
