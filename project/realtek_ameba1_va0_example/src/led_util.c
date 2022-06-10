#include <platform_opts.h>
#include "FreeRTOS.h"
#include <platform/platform_stdlib.h>
#include <rtl8195a.h>
#include "task.h"
#include "queue.h"
#include "timers.h"
#include <gpio_api.h>
#include "led_util.h"
#include "weight.h"

#define GPIO_LED_PIN PB_3

static gpio_t gGpioLed;
static int gCurType = LED_TYPE_OFF;
static int gCurLedVal = 0;
static xTimerHandle gTimerIdBlink;

#if CONFIG_AUDREY_LED_KEY
bool led_off_connect = FALSE;
bool led_on_key = FALSE;
#endif

// **************************************************
//
//  Private Function
//
// **************************************************
static void set_led(int en){
    int val = 0;
    if( (en & 1) == 0 ){
        val = 1;
    }
    else{
        val = 0;
    }
    gCurLedVal = en;
    gpio_write(&gGpioLed, val);
}

static void led_blink_timer_handler(xTimerHandle pxTimer){
    int val = 0;
    if( gCurType == LED_TYPE_CUST_1){
        if( gCurLedVal == 0 ){
            // on 250ms
            val = 1;
            xTimerChangePeriod(gTimerIdBlink, ( 250 / portTICK_RATE_MS ), 0);
        }
        else if( gCurLedVal == 1 ){
            // off 4750ms
            val = 0;
            xTimerChangePeriod(gTimerIdBlink, ( 4750 / portTICK_RATE_MS ), 0);
        }
    }
    else if( gCurType == LED_TYPE_CUST_2){
        if( gCurLedVal >= 3){
            val = 0;
        } else {
            val = gCurLedVal + 1;
        }
        if( val == 0){
            // off 2250ms
            xTimerChangePeriod(gTimerIdBlink, ( 2250 / portTICK_RATE_MS ), 0);
        } else {
            // on or off  250ms
            xTimerChangePeriod(gTimerIdBlink, ( 250 / portTICK_RATE_MS ), 0);
        }
    }
    else if( gCurType == LED_TYPE_CUST_3){
        if( gCurLedVal >= 7){
            val = 0;
        } else {
            val = gCurLedVal + 1;
        }
        if( val == 0){
            // off 2250ms
            xTimerChangePeriod(gTimerIdBlink, ( 2250 / portTICK_RATE_MS ), 0);
        } else {
            // on or off  250ms
            xTimerChangePeriod(gTimerIdBlink, ( 250 / portTICK_RATE_MS ), 0);
        }
    }
    else if( gCurType == LED_TYPE_CUST_4){
        if( gCurLedVal == 0 ){
            // on 4875ms
            val = 1;
            xTimerChangePeriod(gTimerIdBlink, ( 4750 / portTICK_RATE_MS ), 0);
        }
        else if( gCurLedVal == 1 ){
            // off 125ms
            val = 0;
            xTimerChangePeriod(gTimerIdBlink, ( 250 / portTICK_RATE_MS ), 0);
        }
    }
    else{
        // Default Blink
        if( gCurLedVal== 0 ){
            val = 1;
        }
        else{
            val = 0;
        }
    }
    set_led(val);
}

// **************************************************
//
//  Public Function
//
// **************************************************
void led_init(){
    gpio_init(&gGpioLed, GPIO_LED_PIN);
    gpio_write(&gGpioLed, 1);
    gpio_dir(&gGpioLed, PIN_OUTPUT);
    gpio_mode(&gGpioLed, PullNone);
    gTimerIdBlink = xTimerCreate((const char*)"LED_BLINK", ( 1000 / portTICK_RATE_MS ), 1, NULL, led_blink_timer_handler);

    gCurType = LED_TYPE_OFF;
    set_led(0);
}

void led_control(int type){
    if( type == gCurType ){
        return;
    }

   // stop
   xTimerStop(gTimerIdBlink, 0);

   // start
   gCurType = type;
#if CONFIG_AUDREY_LED_WEIGHT && CONFIG_AUDREY_CAB_JUDGE
   if(weight_is_cab()) return;
#endif
#if CONFIG_AUDREY_LED_KEY
    if(led_off_connect && !led_on_key) return;
#endif
   switch( type ){
       case LED_TYPE_OFF:
           set_led(0);
           break;
       case LED_TYPE_ON:
           set_led(1);
           break;
       case LED_TYPE_BLINK_LOW:
           set_led(1);
           xTimerChangePeriod(gTimerIdBlink, ( 1000 / portTICK_RATE_MS ), 0);
           xTimerStart( gTimerIdBlink, 0);
           break;
       case LED_TYPE_BLINK_MID:
           set_led(1);
           xTimerChangePeriod(gTimerIdBlink, ( 125 / portTICK_RATE_MS ), 0);
           xTimerStart( gTimerIdBlink, 0);
           break;
       case LED_TYPE_CUST_1:
           // Off:4750ms
           // On:250ms
           set_led(1);
           xTimerChangePeriod(gTimerIdBlink, ( 250 / portTICK_RATE_MS ), 0);
           xTimerStart( gTimerIdBlink, 0);
           break;
       case LED_TYPE_CUST_2:
           // Off:2250ms
           // On:250ms
           // Off:250ms
           // On:250ms
           set_led(0);
           xTimerChangePeriod(gTimerIdBlink, ( 250 / portTICK_RATE_MS ), 0);
           xTimerStart( gTimerIdBlink, 0);
           break;
       case LED_TYPE_CUST_3:
           // Off:2250ms
           // On:250ms
           // Off:250ms
           // On:250ms
           // Off:250ms
           // On:250ms
           // Off:250ms
           // On:250ms
           set_led(0);
           xTimerChangePeriod(gTimerIdBlink, ( 250 / portTICK_RATE_MS ), 0);
           xTimerStart( gTimerIdBlink, 0);
           break;
       case LED_TYPE_CUST_4:
           // Off:250ms
           // On:4750ms
           set_led(0);
           xTimerChangePeriod(gTimerIdBlink, ( 250 / portTICK_RATE_MS ), 0);
           xTimerStart( gTimerIdBlink, 0);
           break;
   }
}

#if (CONFIG_AUDREY_LED_WEIGHT && CONFIG_AUDREY_CAB_JUDGE) || CONFIG_AUDREY_LED_KEY
void led_revert(void){
    int type = gCurType;
    gCurType = LED_TYPE_OFF;
    led_control(type);
}

void led_off_force(void){
    xTimerStop(gTimerIdBlink, 0);
    set_led(0);
}
#endif
