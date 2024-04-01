#include <stdio.h>
#include <stdbool.h>
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_http_server.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_http_server.h"
#include "esp_https_server.h"
#include "nvs_flash.h"

#include "file_handler.h"
#include "wifi_web_handler.h"
#include "ws_handler.h"

static const char *TAG = "main";

#define LED_PIN 4
#define BUTTON_PIN 23

static QueueHandle_t button_event_queue;

//---------------------------------------------------------
// Při stisknutí tlačítka se do fronty přidá
// funkce na nastavení výchozícho konfigu
//---------------------------------------------------------
static void IRAM_ATTR reset_config_button_handler(void* arg)
{
    uint32_t event = 1;

    xQueueSendFromISR(button_event_queue, &event, NULL);
}

//---------------------------------------------------------
// Po přidání nové položky do fronty "button_event_queue"
// se provede funkce na nastavení výchozícho konfigu
//---------------------------------------------------------
void reset_config_button_handler_task(void* arg)
{
    uint32_t event;

    while (1) {
        if (xQueueReceive(button_event_queue, &event, portMAX_DELAY)) {
            ESP_LOGI(TAG, "Default config button was pressed");
            if (load_default_config()) {
                esp_restart();
            }
        }
    }
}

void app_main()
{
	// Vytvoření nové fronty pro FreeRTOS
    button_event_queue = xQueueCreate(10, sizeof(uint8_t));

    init_spiffs();

    // Inicializace NVS (Non-Volatile Storage) - potřebné pro konfiguraci WiFi
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

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
	setup_server();
	setup_secure_server();
	start_mdns_service();

	gpio_set_direction(BUTTON_PIN, GPIO_MODE_INPUT);
	gpio_set_pull_mode(BUTTON_PIN, GPIO_PULLUP_ONLY);
	gpio_set_intr_type(BUTTON_PIN, GPIO_INTR_POSEDGE);
	gpio_install_isr_service(0);
	gpio_isr_handler_add(BUTTON_PIN, reset_config_button_handler, NULL);

    //Zapnutí LED, pokud vše proběhne v pořádku
    esp_rom_gpio_pad_select_gpio(LED_PIN);
	gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
	gpio_set_level(LED_PIN, 1);

    xTaskCreate(reset_config_button_handler_task, "Default_button_handler", 2048, NULL, 5, NULL);

	ESP_LOGI(TAG, "Web server is running ...\n");
}
