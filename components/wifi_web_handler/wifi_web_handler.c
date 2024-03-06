#include <stdio.h>
#include <string.h>
#include <lwip/sockets.h>
#include "mdns.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_http_server.h"

#include "ws_handler.h"
#include "file_handler.h"
#include "wifi_web_handler.h"

static const char *TAG = "Wifi_web_handler";

httpd_handle_t server;

// Funkce pro nastavení a spuštění Wi-Fi Access Point (AP)
void wifi_init_softap(void) {
    //Inicializace NVS (Non-Volatile Storage) - potřebné pro konfiguraci WiFi
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

	esp_netif_init();
	esp_event_loop_create_default();
    esp_netif_create_default_wifi_ap();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    wifi_config_t wifi_config = {
        .ap = {
    			.channel = 2,
				.ssid_hidden = false,
                .max_connection = user_config->maxcon,
                .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };
    int i;
    for (i = 0; i < (strlen(user_config->ssid)+1); i++){
    	wifi_config.ap.ssid[i] = *((user_config->ssid)+i);
    }
    for (i = 0; i < (strlen(user_config->wifi_pass)+1); i++){
    	wifi_config.ap.password[i] = *((user_config->wifi_pass)+i);
    }

    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    esp_wifi_start();
}

//Funkce pro službu mdns
void start_mdns_service(void)
{
    esp_err_t err = mdns_init();
    if (err) {
        ESP_LOGE(TAG, "MDNS Init failed: %d\n", err);
        return;
    }
    mdns_hostname_set(user_config->mdns);
    ESP_LOGI(TAG, "mDNS hostname set to %s",user_config->mdns);
}

esp_err_t get_index_handler(httpd_req_t *req)
{
    if (httpd_resp_send(req, loaded_files->index_html, HTTPD_RESP_USE_STRLEN) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to send index.html");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t get_logo_handler(httpd_req_t *req)
{
    //Nastavení HTTP hlavičky s MIME typem pro SVG soubor
    httpd_resp_set_type(req, "image/svg+xml");

    //Odeslání obsahu logo.svg jako odpovědi na GET požadavek
    if (httpd_resp_send(req, loaded_files->logo_svg, HTTPD_RESP_USE_STRLEN) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to send logo.svg");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t get_admin_handler(httpd_req_t *req)
{
    if (httpd_resp_send(req, loaded_files->admin_html, HTTPD_RESP_USE_STRLEN) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to send admin.html");
        return ESP_FAIL;
    }

    return ESP_OK;
}

//Funkce pro vytvoření a konfiguraci HTTP serveru
httpd_handle_t setup_server(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    httpd_uri_t uri_get = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = get_index_handler,
        .user_ctx = NULL};

    httpd_uri_t uri_admin_get = {
		.uri = "/admin",
		.method = HTTP_GET,
		.handler = get_admin_handler,
		.user_ctx = NULL};

    httpd_uri_t ws = {
        .uri = "/ws",
        .method = HTTP_GET,
        .handler = handle_ws_req,
        .user_ctx = NULL,
        .is_websocket = true};

    httpd_uri_t uri_logo_get = {
		.uri = "/logo",
		.method = HTTP_GET,
		.handler = get_logo_handler,
		.user_ctx = NULL};

    if (httpd_start(&server, &config) == ESP_OK)
    {
        httpd_register_uri_handler(server, &uri_get);
        httpd_register_uri_handler(server, &ws);
        httpd_register_uri_handler(server, &uri_logo_get);
        httpd_register_uri_handler(server, &uri_admin_get);
    }

    return server;
}
