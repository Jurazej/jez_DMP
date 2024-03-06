#include <stdio.h>
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_http_server.h"

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

	ESP_LOGI(TAG, "Web server is running ...\n");
}
