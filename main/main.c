#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_http_server.h"
#include "esp_spiffs.h"
#include "esp_system.h"
#include "esp_netif.h"
#include "cjson.h"

#include "file_handler.h"
#include "wifi_web_handler.h"
#include "ws_handler.h"

static const char *TAG = "main";

//Nastavení pinu pro LED a počáteční stav LED
#define LED_PIN 2
int led_state = 0;


void app_main()
{
    init_spiffs();
    load_config();
    load_web();

    wifi_init_softap();

    esp_rom_gpio_pad_select_gpio(LED_PIN);
	gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);

	setup_server();
	start_mdns_service();

	ESP_LOGI(TAG, "ESP32 ESP-IDF WebSocket Web Server is running ... ...\n");
}
