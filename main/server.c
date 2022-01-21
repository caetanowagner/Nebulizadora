#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_http_server.h"
#include "cJSON.h"
#include "driver/gpio.h"
#include "esp_spiffs.h"
#include "server.h"

//nvs includes
#include "esp_err.h"
#include "nvs.h"
#include "nvs_flash.h"
//#include "freertos/FreeRTOS.h"
//#include "freertos/task.h"
//#include "esp_system.h"

#define TAG "SERVER"
#define LED 2
#define HTML_PWD "987654"

httpd_handle_t server = NULL;

extern NVSParams histData;


/*	init histData	*/
void initHistData(void)
{
	histData.TotalOperHours = 0;
	histData.PartialOperHours = 0;
	histData.NumStartsPerHour = 0;
	histData.DelayTime = 30;
	histData.MaxStartsPerHour = 60;
}

/*	Set NVS historian data*/
void setStorageData()
{

	nvs_stats_t nvs_stats;
	nvs_handle handle;
	esp_err_t result;
	char dataKey[15] = "histData";
	size_t dataSize = sizeof(NVSParams);

	//esp_err_t nvs_get_stats(const char *part_name, nvs_stats_t *nvs_stats)
	result = nvs_get_stats("storage", &nvs_stats);
	if (result == ESP_OK)
		printf("Count: UsedEntries = (%d), FreeEntries = (%d), AllEntries = (%d)\n",
	          nvs_stats.used_entries, nvs_stats.free_entries, nvs_stats.total_entries);
	else
		printf("nvs_get_stats error: %s\n", esp_err_to_name(result));

	result = nvs_flash_init_partition("storage");
	if (result == ESP_OK)
	{
		result = nvs_open_from_partition("storage", "histData", NVS_READWRITE, &handle);
		if (result == ESP_OK)
		{
			result = nvs_set_blob(handle, dataKey, (void *)&histData, dataSize);
			if (result == ESP_OK)
			{
				result = nvs_commit(handle);
				if (result == ESP_OK)
					printf("nvs_commit Done: %lld\n", esp_timer_get_time()/1000);
				else
					printf("nvs_commit error: %s\n", esp_err_to_name(result));
			}
			else
				printf("nvs_set_blob error: %s\n", esp_err_to_name(result));
			nvs_close(handle);
		}
		else
			printf("nvs_open_from_partition error: %s\n", esp_err_to_name(result));

	}
	else
		printf("nvs_flash_init_partition error: %s\n", esp_err_to_name(result));

}

/*	Get NVS historian data*/
void getStorageData()
{

	NVSParams temp_histData;

	ESP_ERROR_CHECK(nvs_flash_init_partition("storage"));

	nvs_handle handle;
	ESP_ERROR_CHECK(nvs_open_from_partition("storage", "histData", NVS_READWRITE, &handle));

	char dataKey[15] = "histData";
	//NVSParams data;
	size_t dataSize = sizeof(NVSParams);

	esp_err_t result;

	result = nvs_get_blob(handle, dataKey, (void *)&temp_histData, &dataSize);
	//printf("actual cat size=%d returned from NVS =%d\n", sizeof(Cat), catSize);

	switch (result)
	{
		case ESP_ERR_NOT_FOUND:
			//Save default hystData values
		case ESP_ERR_NVS_NOT_FOUND:
			ESP_LOGE(TAG, "Value not set yet");
			setStorageData();
			break;
		case ESP_OK:
		  ESP_LOGI(TAG,
			"DelayTime: %d, MaxStartsPerHour: %d, NumStartsPerHour: %d, PartialOperHours: %d, TotalOperHours: %d",
			temp_histData.DelayTime, temp_histData.MaxStartsPerHour, temp_histData.NumStartsPerHour, temp_histData.PartialOperHours, histData.TotalOperHours);

		  if (temp_histData.DelayTime > 0) histData.DelayTime = temp_histData.DelayTime;
		  if (temp_histData.MaxStartsPerHour > 0) histData.MaxStartsPerHour = temp_histData.MaxStartsPerHour;
		  if (temp_histData.NumStartsPerHour > 0) histData.NumStartsPerHour = temp_histData.NumStartsPerHour;
		  if (temp_histData.PartialOperHours > 0) histData.PartialOperHours = temp_histData.PartialOperHours;
		  if (temp_histData.TotalOperHours > 0) histData.TotalOperHours = temp_histData.TotalOperHours;


		  break;
		default:
		  ESP_LOGE(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(result));
		  break;
	}
	nvs_close(handle);
}


static esp_err_t on_url_hit(httpd_req_t *req)
{
     esp_vfs_spiffs_conf_t config = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true};
    esp_vfs_spiffs_register(&config);

    ESP_LOGI(TAG, "url %s was hit", req->uri);
    char path[600];

    if(strcmp("/style.css", req->uri)==0)
    {
    	sprintf(path, "/spiffs/style.css");
    	ESP_LOGI(TAG, "setting mime type to css");
		httpd_resp_set_type(req, "text/css");
    }
    else if(strcmp("/scripts.js", req->uri)==0)
    {
    	sprintf(path, "/spiffs/scripts.js");
    }
    else
    {
    	sprintf(path, "/spiffs/index.html");
    }

	FILE *file = fopen(path, "r");
    if (file == NULL)
    {
        httpd_resp_send_404(req);
        esp_vfs_spiffs_unregister(NULL);
        return ESP_OK;
    }

    char lineRead[256];
    while (fgets(lineRead, sizeof(lineRead), file))
    {
        httpd_resp_sendstr_chunk(req, lineRead);
    }
    httpd_resp_sendstr_chunk(req, NULL);

    esp_vfs_spiffs_unregister(NULL);
    return ESP_OK;

}


static esp_err_t get_params(httpd_req_t *req)
{
    ESP_LOGI(TAG, "url %s was hit", req->uri);
    char buf[100];
    memset(&buf, 0, sizeof(buf));
    httpd_req_recv(req, buf, req->content_len);
    ESP_LOGI(TAG, "Buffer size: %d", req->content_len);

    char message[150];
    sprintf(message,
    		"{\"TimeDelay\":%d, \"MaxStartsPerHour\":%d, \"NumStartsPerHour\":%d, \"PartialOperHours\":%d, \"TotalOperHours\":%d}",
    		histData.DelayTime, histData.MaxStartsPerHour, histData.NumStartsPerHour, (histData.PartialOperHours/10), (histData.TotalOperHours/10));
    ESP_LOGI(TAG, "%s", message);
    httpd_resp_send(req, message, strlen(message));
    //cJSON_Delete(payload);
    return ESP_OK;
}


static esp_err_t set_params(httpd_req_t *req)
{
    ESP_LOGI(TAG, "url %s was hit", req->uri);
    char buf[100];
    memset(&buf, 0, sizeof(buf));
    httpd_req_recv(req, buf, req->content_len);
    ESP_LOGI(TAG, "Buffer size: %d", req->content_len);
    cJSON *payload = cJSON_Parse(buf);

    int TimeDelay = cJSON_GetObjectItem(payload, "TimeDelay")->valueint;
    int MotorStartLimit = cJSON_GetObjectItem(payload, "MotorStartLimit")->valueint;
    char *strPwd = cJSON_GetObjectItem(payload, "Password")->valuestring;

    ESP_LOGI(TAG, "TimeDelay: %d, MotorStartLimit: %d, Password: %s", TimeDelay, MotorStartLimit, strPwd);

    uint16_t status = 999;
    char message[100];

    if ((TimeDelay < 0) | (TimeDelay > 1000))
    {
    	ESP_LOGI(TAG, "TimeDelay out of range");
    	status = 4;
    }
    else if ((MotorStartLimit < 2) | (MotorStartLimit > 60))
    {
		ESP_LOGI(TAG, "MotorStartLimit out of range");
		status = 3;
	}
    else if (strcmp(strPwd, HTML_PWD) != 0)
	{
		ESP_LOGI(TAG, "Password incorrect");
		status = 2;

	}
    else if (strcmp(strPwd, HTML_PWD) == 0)
    {
    	ESP_LOGI(TAG, "Flash new parameters");
    	histData.DelayTime = TimeDelay;
    	histData.MaxStartsPerHour = MotorStartLimit;
    	setStorageData();
    	status = 1;
    }
    else
    {
    	ESP_LOGI(TAG, "Unknown error");
    }

    sprintf(message, "{\"flash\":%d}",status);
    ESP_LOGI(TAG, "%s", message);
    httpd_resp_send(req, message, strlen(message));
    cJSON_Delete(payload);
    return ESP_OK;
}


static esp_err_t reset_params(httpd_req_t *req)
{
    ESP_LOGI(TAG, "url %s was hit", req->uri);
    char buf[100];
    memset(&buf, 0, sizeof(buf));
    httpd_req_recv(req, buf, req->content_len);
    ESP_LOGI(TAG, "Buffer size: %d", req->content_len);
    ESP_LOGI(TAG, "Buffer: %s", buf);

    cJSON *payload = cJSON_Parse(buf);
    char *strPwd = cJSON_GetObjectItem(payload, "Password")->valuestring;
    ESP_LOGI(TAG, "Password: %s", strPwd);

    uint16_t status = 999;
    char message[100];

    if (strcmp(strPwd, HTML_PWD) != 0)
	{
		ESP_LOGI(TAG, "Password incorrect");
		status = 2;
	}
    else if (strcmp(strPwd, HTML_PWD) == 0)
    {
    	ESP_LOGI(TAG, "Flash new parameters");
    	histData.PartialOperHours = 0;
		setStorageData();
		status = 1;
    }
    else
    {
    	ESP_LOGI(TAG, "Unknown error");
    }
    sprintf(message, "{\"flash\":%d}",status);
    ESP_LOGI(TAG, "%s", message);
    httpd_resp_send(req, message, strlen(message));
    cJSON_Delete(payload);
    return ESP_OK;
}


static esp_err_t favicon(httpd_req_t *req)
{
    ESP_LOGI(TAG, "url %s was hit", req->uri);
    char buf[100];
    memset(&buf, 0, sizeof(buf));
    httpd_req_recv(req, buf, req->content_len);
    ESP_LOGI(TAG, "Buffer size: %d", req->content_len);
    ESP_LOGI(TAG, "Buffer: %s", buf);

    httpd_resp_set_status(req, "204 NO CONTENT");
    httpd_resp_send(req, NULL, 0);

    //httpd_resp_send(req, message, strlen(message));
    return ESP_OK;
}


void RegisterEndPoints(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;

    ESP_LOGI(TAG, "starting server");
    if (httpd_start(&server, &config) != ESP_OK)
    {
        ESP_LOGE(TAG, "COULD NOT START SERVER");
    }

    httpd_uri_t get_params_end_point_config = {
        .uri = "/api/get_params",
        .method = HTTP_POST,
        .handler = get_params};
    httpd_register_uri_handler(server, &get_params_end_point_config);

    //httpd_register_err_handler(server, error, handler_fn)

    httpd_uri_t set_params_end_point_config = {
        .uri = "/api/set_params",
        .method = HTTP_POST,
        .handler = set_params};
    httpd_register_uri_handler(server, &set_params_end_point_config);

    httpd_uri_t reset_params_end_point_config = {
        .uri = "/api/reset_params",
        .method = HTTP_POST,
        .handler = reset_params};
    httpd_register_uri_handler(server, &reset_params_end_point_config);

    httpd_uri_t favicon_end_point_config = {
        .uri = "/favicon.ico",
        .method = HTTP_GET,
        .handler = favicon};
    httpd_register_uri_handler(server, &favicon_end_point_config);

    httpd_uri_t first_end_point_config = {
        .uri = "/*",
        .method = HTTP_GET,
        .handler = on_url_hit};
    httpd_register_uri_handler(server, &first_end_point_config);
}

