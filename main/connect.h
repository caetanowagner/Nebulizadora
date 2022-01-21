#ifndef _CONNECT_H_
#define _CONNECT_H_

/** Argument structure for WIFI_EVENT_AP_STACONNECTED event */
//typedef struct {
//    uint8_t mac[6];           /**< MAC address of the station connected to ESP32 soft-AP */
//    uint8_t aid;              /**< the aid that ESP32 soft-AP gives to the station connected to  */
//} wifi_ap_stations_t;
//
//wifi_ap_stations_t wifi_ap_stations[10];

esp_err_t wifiInit();


#endif
