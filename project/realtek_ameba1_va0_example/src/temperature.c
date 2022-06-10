/*
 * Copyright (C) 2017 SHARP Corporation. All rights reserved.
 */

#include <platform_opts.h>
#include "FreeRTOS.h"
#include "task.h"
#include <platform_stdlib.h>
#include "xml.h"
#include "i2c_api.h"
#include "temperature.h"

#define I2C_SDA PD_9
#define I2C_SCL PD_8
#define I2C_CLOCK (400 * 1000)

#define I2C_SLAVE_ADDR 0x48
#define POINTER_TEMPERATURE 0x00
#define POINTER_CONFIG 0x01
#define POINTER_T_LOW  0x02
#define POINTER_T_HIGH 0x03

/* 温度監視間隔 */
#define TEMP_CHECK_DELAY				60000		/* 1分間隔 (単位：ms) */

i2c_t gI2c;
float gTemperature;

// **************************************************
//
//  Private Function
//
// **************************************************
static void send_reg(char pointer, char val){
	char sendBuf[] = {
		pointer,
		val
	};
	i2c_write(&gI2c, I2C_SLAVE_ADDR, sendBuf, sizeof(sendBuf), 1);
}

static void recv_reg(char pointer, char* recvBuf, int len){
	int ret, cnt;
	char sendBuf[] = {
		pointer
	};

	i2c_write(&gI2c, I2C_SLAVE_ADDR, sendBuf, sizeof(sendBuf), 0);
	i2c_read(&gI2c, I2C_SLAVE_ADDR, recvBuf, len, 1);
}

static int get_temp(){
	int ret;
	char config;
	char temperature[2];

	temperature[0] = 0;
	temperature[1] = 0;

	/* Configuration Register を読み出して ONE_SHOT=ON を設定 */
	recv_reg( POINTER_CONFIG, &config, 1 );
	config |= CONFIG_ONE_SHOT_ENABLE;
	send_reg( POINTER_CONFIG, config);
	/* 温度読み出し */
	recv_reg( POINTER_TEMPERATURE, temperature, 2);
	/* Configuration Register に ONE_SHOT=OFF を設定 */
	config &= ~CONFIG_ONE_SHOT_ENABLE;
	send_reg( POINTER_CONFIG, config);
	/* 読み出したレジスタの値を1つの変数に変換 */
	ret = ((int)temperature[0] << TEMP_REG_BYTE1_LSHIFT) |
		  (((unsigned int)temperature[1] & TEMP_REG_BYTE2_MASK) >> TEMP_REG_BYTE2_RSHIFT);
	return ret;
}

static void temperature_thread(void *param)
{
	Log_Info("start temperature_thread\n");
	i2c_init(&gI2c, I2C_SDA ,I2C_SCL);
	i2c_frequency(&gI2c, I2C_CLOCK);

	send_reg( POINTER_CONFIG, CONFIG_DEFAULT );

	while(1) {
		int reg = get_temp();
		float temp = reg * TEMP_UNIT;

		//DiagPrintf("temperature %x %d\n", reg, (int)temp);
		gTemperature = temp;

		vTaskDelay(TEMP_CHECK_DELAY);
	}
	vTaskDelete(NULL);
}

// **************************************************
//
//  Public Function
//
// **************************************************
float get_temperature(){
	return gTemperature;
}

void temperature_init(void)
{
	gTemperature = 0;
	if(xTaskCreate(temperature_thread, ((const char*)"temperature_thread"), 1024, NULL, tskIDLE_PRIORITY + 1, NULL) != pdPASS){
		Log_Error("\n\r%s xTaskCreate(init_thread) failed", __FUNCTION__);
	}
}

