#ifndef __LED_UTIL_H__
#define __LED_UTIL_H__

#define LED_TYPE_OFF         0     // 消灯
#define LED_TYPE_ON          1     // 点灯
#define LED_TYPE_BLINK_LOW   2     // 0.5Hz 点灯/消灯繰り返し
#define LED_TYPE_BLINK_MID   3     // 4Hz 点灯/消灯繰り返し
#define LED_TYPE_CUST_1      10    // 250ms点灯 → 4750ms消灯の繰り返し
#define LED_TYPE_CUST_2      20    // 2250消灯 → 250ms 周期点滅×２ の繰り返し
#define LED_TYPE_CUST_3      30    // 2250消灯 → 250ms 周期点滅×４ の繰り返し
#define LED_TYPE_CUST_4      40    // 4750ms点灯 → 250ms消灯の繰り返し

#if CONFIG_AUDREY_LED_KEY
extern bool led_off_connect;
extern bool led_on_key;
#endif

/** 
 * @brief  LEDの初期化
 * @param  None
 * @return  None
 */
void led_init(void);

/** 
 * @brief  LEDの制御
 * @param  type : LEDの動作タイプ
 * @return  None
 */
void led_control(int type);

#if CONFIG_AUDREY_LED_WEIGHT || CONFIG_AUDREY_LED_KEY
/** 
 * @brief  LED制御の復帰
 * @param  None
 * @return  None
 */
void led_revert(void);

/** 
 * @brief  LED制御の強制OFF
 * @param  None
 * @return  None
 */
void led_off_force(void);
#endif

#endif // __LED_UTIL_H__