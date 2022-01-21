/*
 * gpio.c
 *
 *  Created on: Aug 5, 2020
 *  Author: Wagner Caetano
 *  Set inputs and outputs configuration, interrupt and timers operations
 */

#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_event_loop.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "server.h"
#include "connect.h"

/*	gpio includes	*/
#include "freertos/queue.h"
#include "driver/gpio.h"

#include "freertos/timers.h"

/*	timer includes	*/
#include "esp_types.h"
#include "driver/periph_ctrl.h"
#include "driver/timer.h"
#include "gpio_ex.h"

#include <unistd.h>
#include "esp_timer.h"
#include "esp_sleep.h"
#include "sdkconfig.h"

#include "server.h"
#include "main.h"

/*	GPIO Definitions	*/
#define OUTPUT_R1		16
#define OUTPUT_R2		17
#define OUTPUT_LED		14
#define test_output		23
#define GPIO_OUTPUT_PIN_SEL  ((1ULL<<OUTPUT_R1) | (1ULL<<OUTPUT_R2) | (1ULL<<OUTPUT_LED)  | (1ULL<<test_output))
//#define IN1				25	//Start
//#define IN2				27	//Motor Running
//#define SW_IN			12
#define IN1				18		//Start
#define IN2				19		//Motor Running
#define SW_IN			21		//Enable wifi
#define GPIO_INPUT_PIN_SEL  ((1ULL<<IN1) | (1ULL<<IN2) | (1ULL<<SW_IN))
#define ESP_INTR_FLAG_DEFAULT 0
#define TAG				"gpio"
#define DEBOUNCETIME	20	//miliseconds RTOs tick (100Hz - 10ms) at sdkconfig	*/
#define PERIODTIMER		10000000		//6 minutes

/* RTOS Timer Handler  */
TimerHandle_t xTimerIn1, xTimerIn2, xTimerSwIn, xTimerR1OutDelay;

/*	High Resolution Timer Handler	*/
esp_timer_handle_t periodicTimer;

/*	Starts per hour log	*/
uint16_t numStartsHour [200][200] = {};

//extern NVSParams histData;
//extern xSemaphoreHandle startServerSemaphore;

xSemaphoreHandle periodicTimerMutex;

/*	Functions Prototype	*/
//static void IRAM_ATTR gpio_isr_handler(void* arg);
static void in1_debouncer_callback(void* arg);
static void in2_debouncer_callback(void* arg);
static void inSw_debouncer_callback(void* arg);
static void periodic_timer_callback(void* arg);
static void IRAM_ATTR gpio_isr_handler(void* arg);


/*	High Resolution Timer Configuration Struct	*/
const esp_timer_create_args_t periodicTimerArgs =
{
	.callback = &periodic_timer_callback,
	/* name is optional, but may help identify the timer when debugging */
	.name = "periodic"
};


static void periodic_timer_callback(void* arg)
{
	printf("Periodic callback - Save data to NVS, %lld\n",esp_timer_get_time()/1000);
	histData.TotalOperHours++;
	histData.PartialOperHours++;

	setStorageData();
}


/*	RTOS gpio debounce	*/
static void in1_debouncer_callback(void* arg)
{
	if (gpio_get_level(IN1))
	{
		printf("IN1, %lld\n",esp_timer_get_time()/1000);
		printf("Set Output Relay 1\n");

		esp_err_t err = gpio_set_level(OUTPUT_R1, 1);
		if (err ==ESP_OK)
		{
			if( xTimerIsTimerActive(xTimerR1OutDelay) == pdFALSE )
				histData.NumStartsPerHour++;
			else
				xTimerStop(xTimerR1OutDelay,5);

			if (xTimerGetPeriod(xTimerR1OutDelay) != pdMS_TO_TICKS(histData.DelayTime*1000))
			{
				if (histData.DelayTime <= 0)
					xTimerChangePeriod(xTimerR1OutDelay,1,5);
				else
					xTimerChangePeriod(xTimerR1OutDelay,pdMS_TO_TICKS(histData.DelayTime*1000),5);
			}
			else
				xTimerStart(xTimerR1OutDelay,0);
		}
	}
	gpio_isr_handler_add(IN1, gpio_isr_handler, (void*) IN1);

}


static void in2_debouncer_callback(void* arg)
{
	esp_err_t err;
	if (gpio_get_level(IN2))
	{
		if (xSemaphoreTake(periodicTimerMutex, 1))
		{
			err = esp_timer_start_periodic(periodicTimer, PERIODTIMER);
			printf("esp_timer_start_periodic: %s\n",esp_err_to_name(err));
		}
		printf("IN2 = 1, %lld\n",esp_timer_get_time()/1000);
	}
	else
	{
		err = (esp_timer_stop(periodicTimer));
		printf("esp_timer_stop_periodic: %s\n",esp_err_to_name(err));
		xSemaphoreGive(periodicTimerMutex);
		printf("IN2 = 0, %lld\n",esp_timer_get_time()/1000);
	}
	gpio_isr_handler_add(IN2, gpio_isr_handler, (void*) IN2);
}


static void inSw_debouncer_callback(void* arg)
{
	esp_err_t err;
	wifi_mode_t mode;
	err = esp_wifi_get_mode(&mode);
	if (gpio_get_level(SW_IN) && err == ESP_ERR_WIFI_NOT_INIT)
	{
		printf("SW_IN, %lld\n",esp_timer_get_time()/1000);
		xTaskCreate(&OnConnected, "handel comms", 1024 * 5, NULL, 5, NULL);
	}
	else
		printf("esp_wifi_get_mode %s\n", esp_err_to_name(err));

	vTaskDelay(pdMS_TO_TICKS(100));
	gpio_isr_handler_add(SW_IN, gpio_isr_handler, (void*) SW_IN);
}


static void output_R1_callback(void* arg)
{
	esp_err_t err;
	printf("Reset Output Relay 1,  %lld\n",esp_timer_get_time()/1000);
	err = gpio_set_level(OUTPUT_R1, 0);
	if (err != ESP_OK)
		printf("gpio_set_level(OUTPUT_R1) error: %s\n",esp_err_to_name(err));
}

/*	GPIO Interrupt	Handler*/
static void IRAM_ATTR gpio_isr_handler(void* arg)
{
	uint32_t inputPin = (uint32_t) arg;	//Pin number
	gpio_isr_handler_remove(inputPin);
	uint32_t pinLevel = gpio_get_level(inputPin);
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;

	switch (inputPin)
	{
		case IN1:
			if (pinLevel)
			{
				if (xTimerStartFromISR( xTimerIn1, &xHigherPriorityTaskWoken ) != pdPASS )
					gpio_isr_handler_add(IN1, gpio_isr_handler, (void*) IN1);

			}
			else
				gpio_isr_handler_add(IN1, gpio_isr_handler, (void*) IN1);
			break;
		case IN2:
			if (xTimerStartFromISR( xTimerIn2, &xHigherPriorityTaskWoken ) != pdPASS )
				gpio_isr_handler_add(IN2, gpio_isr_handler, (void*) IN2);
			break;

		case SW_IN:
			if (pinLevel)
			{
				xTimerStartFromISR( xTimerSwIn, &xHigherPriorityTaskWoken );
					//gpio_isr_handler_add(SW_IN, gpio_isr_handler, (void*) SW_IN);

			}
			else
				gpio_isr_handler_add(SW_IN, gpio_isr_handler, (void*) SW_IN);
			break;

		default:
			break;
	}
}


void GPIO_Config()
{
	gpio_config_t io_conf;
	io_conf.intr_type = GPIO_INTR_DISABLE;			//disable interrupt
	io_conf.mode = GPIO_MODE_INPUT_OUTPUT;			//set as output mode
	io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;		//bit mask of the pins that you want to set,e.g.GPIO18/19
	io_conf.pull_down_en = 0;						//disable pull-down mode
	io_conf.pull_up_en = 0;							//disable pull-up mode
	gpio_config(&io_conf);							//configure GPIO with the given settings

	io_conf.intr_type = GPIO_INTR_POSEDGE;			//interrupt of rising edge
	io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;		//bit mask of the pins, use GPIO4/5 here
	io_conf.mode = GPIO_MODE_INPUT;					//set as input mode
	io_conf.pull_down_en = 1;						//enable pull-down mode
	gpio_config(&io_conf);

	//change gpio interrupt type for one pin
	gpio_set_intr_type(IN1, GPIO_INTR_POSEDGE);
	gpio_set_intr_type(IN2, GPIO_INTR_ANYEDGE);
	gpio_set_intr_type(SW_IN, GPIO_INTR_POSEDGE);

	ESP_ERROR_CHECK(esp_timer_create(&periodicTimerArgs, &periodicTimer));
	periodicTimerMutex = xSemaphoreCreateMutex();

	gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
	gpio_isr_handler_add(IN1, gpio_isr_handler, (void*) IN1);
	gpio_isr_handler_add(IN2, gpio_isr_handler, (void*) IN2);
	gpio_isr_handler_add(SW_IN, gpio_isr_handler, (void*) SW_IN);

	xTimerIn1 = xTimerCreate("TimerIn1", pdMS_TO_TICKS(DEBOUNCETIME), pdFALSE, ( void * ) 1, in1_debouncer_callback);
	xTimerIn2 = xTimerCreate("TimerIn2", pdMS_TO_TICKS(DEBOUNCETIME), pdFALSE, ( void * ) 2, in2_debouncer_callback);
	xTimerSwIn = xTimerCreate("TimerIn3", pdMS_TO_TICKS(DEBOUNCETIME), pdFALSE, ( void * ) 3, inSw_debouncer_callback);


	if (histData.DelayTime <= 0)
		xTimerR1OutDelay = xTimerCreate("TimerIn3", 1, pdFALSE, ( void * ) 4, output_R1_callback);
	else
		xTimerR1OutDelay = xTimerCreate("TimerIn3", pdMS_TO_TICKS(histData.DelayTime*1000), pdFALSE, ( void * ) 4, output_R1_callback);



}

