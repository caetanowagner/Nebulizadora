#include <gpio_ex.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
//#include "esp_event_loop.h" //deprecated
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "server.h"
#include "connect.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
//#include "gpio_ex.h"
#include "esp_sleep.h"

/*	timer includes	*/
#include "esp_types.h"
#include "driver/periph_ctrl.h"
#include "driver/timer.h"
//#include "main.h"

#include "driver/periph_ctrl.h"

#define TAG "DATA"
//#define NUMBER CONFIG_TEL_NUMBER

#define IN1				18		//Start
#define IN2				19		//Motor Running
#define SW_IN			21		//Enable wifi

xSemaphoreHandle startServerSemaphore;

void OnConnected(void *para)
{
  while (true)
  {
	esp_err_t err;
	err = wifiInit();
	if (err == ESP_OK)
		RegisterEndPoints();

	vTaskDelete(NULL);
   }
}


void app_main()
{

	/* Uncomment to erase flash log data	*/
	//nvs_flash_erase_partition("storage");

	startServerSemaphore = xSemaphoreCreateBinary();
	initHistData();
	getStorageData();
	GPIO_Config();
//	gpio_wakeup_enable(IN1, GPIO_INTR_HIGH_LEVEL);
//	gpio_wakeup_enable(IN2, GPIO_INTR_HIGH_LEVEL);
//	gpio_wakeup_enable(SW_IN, GPIO_PIN_INTR_HILEVEL);

	//esp_sleep_enable_gpio_wakeup();
	//esp_light_sleep_start();

}
