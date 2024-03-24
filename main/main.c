#include <stdio.h>
#include <stdbool.h>
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_http_server.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_http_server.h"
#include "esp_https_server.h"

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

    if (load_config() == false){
    	while (load_config() == false){
    		ESP_LOGE(TAG, "Failed to load config, retrying after 5 seconds");
    		vTaskDelay(pdMS_TO_TICKS(5000));
    	}
    }
    if (load_web() == false){
    	while (load_web() == false){
    		ESP_LOGE(TAG, "Failed to load web files, retrying after 5 seconds");
    		vTaskDelay(pdMS_TO_TICKS(5000));
    	}
    }
    if (load_ligts_params() == false){
    	while (load_ligts_params() == false){
    		ESP_LOGE(TAG, "Failed to load light parameters, retrying after 5 seconds");
    		vTaskDelay(pdMS_TO_TICKS(5000));
    	}
    }

    wifi_init_softap();

    esp_rom_gpio_pad_select_gpio(LED_PIN);
	gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);

	setup_server();
	setup_wss_server();
	start_mdns_service();

	ESP_LOGI(TAG, "Web server is running ...\n");
}
