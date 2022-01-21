#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
//#include "esp_event_loop.h"
#include "esp_event.h"

#include "esp_wifi.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "cJSON.h"
#include <string.h>
#include "esp_http_server.h"
#include "esp_sleep.h"

#define EXAMPLE_ESP_WIFI_SSID      "Controlador Nebulizadora"
#define EXAMPLE_ESP_WIFI_PASS      "98765432"
#define DELAY_WIFI_DISC				120000	//wait 2 minutes to stop wifi after all stations disconnected
#define DELAY_WIFI_SERVER_STOP		300000	//wait 5 minutes to stop wifi after server started and no connections

wifi_sta_list_t wifi_sta_list;

extern httpd_handle_t server;
extern xSemaphoreHandle startServerSemaphore;

char *TAG = "CONNECTION";

TimerHandle_t xTimerWifi, xTimerWifiStart;

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
	esp_err_t err;
	wifi_event_ap_staconnected_t* event_conn = (wifi_event_ap_staconnected_t*) event_data;
	wifi_event_ap_stadisconnected_t* event_disconn = (wifi_event_ap_stadisconnected_t*) event_data;

	switch (event_id) {
		case WIFI_EVENT_AP_START:
			xSemaphoreGive(startServerSemaphore);
			xTimerStart(xTimerWifiStart,0);
			break;
		case WIFI_EVENT_AP_STACONNECTED:
			//wifi_event_ap_staconnected_t* event_conn = (wifi_event_ap_staconnected_t*) event_data;
			ESP_LOGI(TAG, "station "MACSTR" join, AID=%d", MAC2STR(event_conn->mac), event_conn->aid);


//			esp_err_t err = esp_wifi_ap_get_sta_list(&wifi_sta_list);
//
//			if (err == ESP_OK)
//			{
//				printf("Number of entries %d\n", wifi_sta_list.num);
//
//				for (int i = 0; i < wifi_sta_list.num; i++)
//				{
//					printf("MAC: " MACSTR "\n", MAC2STR(wifi_sta_list.sta[i].mac));
//					printf("RSSI %d\n", wifi_sta_list.sta[i].rssi);
//				}
//			}

			if (xTimerIsTimerActive(xTimerWifiStart))
				xTimerStop(xTimerWifiStart,0);

			if (xTimerIsTimerActive(xTimerWifi))
				xTimerStop(xTimerWifi,0);

			break;
		case WIFI_EVENT_AP_STADISCONNECTED:
			//wifi_event_ap_stadisconnected_t* event_disconn = (wifi_event_ap_stadisconnected_t*) event_data;
			ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d", MAC2STR(event_disconn->mac), event_disconn->aid);

			err = esp_wifi_ap_get_sta_list(&wifi_sta_list);

			if (err == ESP_OK)
			{
				printf("Connected stations: %d\n", wifi_sta_list.num);
				if (wifi_sta_list.num <= 0)
				{
					//if none connected, start time to deinit wifi
					xTimerStart(xTimerWifi,0);
				}
			}
			break;
		default:
			break;
	}
	//esp_light_sleep_start();
}


void StopWifi(void *para)
{

  while (true)
  {

	printf("StopWifi task started\n");

	//block task static void inSw_debouncer_callback(void* arg)


	esp_err_t err = ESP_OK;

	if (server != NULL)
	{
		printf("\nhttpd_stop\n");
		err = httpd_stop(server);
	}

	vTaskDelay(100 / portTICK_PERIOD_MS);
	if (err == ESP_OK)
	{
		printf("\nesp_wifi_stop\n");
		err = esp_wifi_stop();
		vTaskDelay(100 / portTICK_PERIOD_MS);
		if (err == ESP_OK)
		{
			printf("\nesp_wifi_deinit\n");
			err = esp_wifi_deinit();
			vTaskDelay(100 / portTICK_PERIOD_MS);
			if (err == ESP_OK)
			{
				printf("\nesp_event_loop_delete_default\n");
				err = esp_event_loop_delete_default();
				vTaskDelay(100 / portTICK_PERIOD_MS);
				if (err != ESP_OK)
					printf("\nesp_event_loop_delete_default error: %s\n", esp_err_to_name(err));
			}
			else
				printf("\nesp_wifi_deinit error: %s\n", esp_err_to_name(err));
		}
		else
			printf("\nesp_wifi_stop error: %s\n", esp_err_to_name(err));
	}
	else
		printf("\nhttpd_stop error: %s\n", esp_err_to_name(err));


	if (xTimerDelete(xTimerWifi,0) == pdFAIL)
		printf("\nxTimerDelete error\n");


	//esp_light_sleep_start();

	vTaskDelete(NULL);
   }
}


static void stop_wifi_timer_callback(void* arg)
{
	xTaskCreate(StopWifi, "StopWifi", 1024*2, NULL, 5, NULL);
}


esp_err_t wifiInit()
{

	xTimerWifi = xTimerCreate("TimerWifi", pdMS_TO_TICKS(DELAY_WIFI_DISC), pdFALSE, ( void * ) 1, stop_wifi_timer_callback);
	xTimerWifiStart = xTimerCreate("TimerWifiStart", pdMS_TO_TICKS(DELAY_WIFI_SERVER_STOP), pdFALSE, ( void * ) 1, stop_wifi_timer_callback);

	esp_err_t ret;
	if (xTimerWifi == NULL)
		return ret = ESP_FAIL;

	ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
	{
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	//if error to init flash cancel wifi init
	if (ret != ESP_OK)
	{
		ESP_LOGI(TAG, "NVS Flash init error:%s", esp_err_to_name(ret));
		return ret;
	}

	// ret = esp_netif_init();
	// if (ret != ESP_OK)
	// {
	// 	ESP_LOGE(TAG, "ESP-NETIF initialization failed with %d in tcpip_adapter compatibility mode", ret);
    // }
	tcpip_adapter_init();

	ret = esp_event_loop_create_default();
	if (ret != ESP_OK)
	{
		ESP_LOGI(TAG, "\nFailed to create task loop error:%s\n", esp_err_to_name(ret));
		return ret;
	}

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ret = esp_wifi_init(&cfg);
	if (ret != ESP_OK)
	{
		ESP_LOGI(TAG, "Failed to init wifi error:%s", esp_err_to_name(ret));
		return ret;
	}

	ret = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL);
	if (ret != ESP_OK)
	{
		ESP_LOGI(TAG, "Register event handler error:%s", esp_err_to_name(ret));
		return ret;
	}

	wifi_config_t wifi_config = {
		.ap = {
			.ssid = EXAMPLE_ESP_WIFI_SSID,
			.ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID),
			.password = EXAMPLE_ESP_WIFI_PASS,
			.max_connection = ESP_WIFI_MAX_CONN_NUM,
			.authmode = WIFI_AUTH_WPA_WPA2_PSK
		},
	};
	if (strlen(EXAMPLE_ESP_WIFI_PASS) == 0)
	{
		wifi_config.ap.authmode = WIFI_AUTH_OPEN;
	}

	ret = esp_wifi_set_mode(WIFI_MODE_AP);
	if (ret != ESP_OK)
	{
		ESP_LOGI(TAG, "Wifi set mode error:%s", esp_err_to_name(ret));
		return ret;
	}

	ret = esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config);
	if (ret != ESP_OK)
	{
		ESP_LOGI(TAG, "wifi set config error:%s", esp_err_to_name(ret));
		return ret;
	}

	ret = esp_wifi_start();
	if (ret != ESP_OK)
	{
		ESP_LOGI(TAG, "Wifi start error:%s", esp_err_to_name(ret));
		return ret;
	}
	return ret = ESP_OK;

	ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s", EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
}
