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
#include <lwip/sockets.h>
#include <lwip/sys.h>
#include <lwip/api.h>
#include <lwip/netdb.h>
#include "mdns.h"
#include "cjson.h"

static const char *TAG = "Jez DMP"; // TAG pro debug

//Nastavení pinu pro LED a počáteční stav LED
#define LED_PIN 2
int led_state = 0;

//Umístění souborů
#define INDEX_PATH "/spiffs/index.html"
#define LOGO_PATH "/spiffs/logo.svg"
#define ADMIN_PATH "/spiffs/admin.html"
#define DEFAULTCONFIG_PATH "/spiffs/default_config.txt"
#define CONFIG_PATH "/spiffs/config.txt"

//Deklarace proměnných, které slouží pro uložení html kódu webové stránky
char index_html[8192];
char response_data[8192];
char logo_svg[6650];
char admin_html[13000];

struct user_config {
	char *ssid;
	char *wifi_pass;
	uint8_t maxcon;
	char *mdns;
	char *admin_pass;
}user_config;

httpd_handle_t server = NULL;
struct async_resp_arg {
    httpd_handle_t hd;
    int fd;
};

cJSON json_in;

void load_config(){
	//Nastavení konfigurace pro SPIFFS (SPI Flash File System)
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 8,
        .format_if_mount_failed = true};

    ESP_ERROR_CHECK(esp_vfs_spiffs_register(&conf));	//Registrace SPIFFS pomocí nastavené konfigurace


    struct stat st;
    if (stat(CONFIG_PATH, &st))
    {
        ESP_LOGE(TAG, "config file not found");
        return;
    }
    char *loaded_json_config;
    loaded_json_config = (char *)malloc(st.st_size);
    FILE *fp = fopen(CONFIG_PATH, "r");

    if (fread(loaded_json_config, st.st_size, 1, fp) == 0)
    {
        ESP_LOGE(TAG, "fread failed");
    }
    fclose(fp);
    ESP_LOGE(TAG, "Loaded config: %s", loaded_json_config);

	cJSON *root = cJSON_Parse(loaded_json_config);
	user_config.ssid = cJSON_GetObjectItem(root,"ssid")->valuestring;
	user_config.wifi_pass = cJSON_GetObjectItem(root,"wifi_pass")->valuestring;
	user_config.maxcon = cJSON_GetObjectItem(root,"maxcon")->valueint;
	user_config.mdns = cJSON_GetObjectItem(root,"mdns")->valuestring;
	user_config.admin_pass = cJSON_GetObjectItem(root,"admin_pass")->valuestring;
    free(loaded_json_config);
}

// Funkce pro nastavení a spuštění Wi-Fi Access Point (AP)
static void wifi_init_softap() {
	esp_netif_init();
	esp_event_loop_create_default();
    esp_netif_create_default_wifi_ap();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    wifi_config_t wifi_config = {
        .ap = {
    			.channel = 2,
				.ssid_hidden = false,
                .max_connection = user_config.maxcon,
                .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };
    int i;
    for (i = 0; i < (strlen(user_config.ssid)+1); i++){
    	wifi_config.ap.ssid[i] = *((user_config.ssid)+i);
    }
    for (i = 0; i < (strlen(user_config.wifi_pass)+1); i++){
    	wifi_config.ap.password[i] = *((user_config.wifi_pass)+i);
    }

    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    esp_wifi_start();
}

//Funkce pro službu mdns
void start_mdns_service()
{
    esp_err_t err = mdns_init();
    if (err) {
        printf("MDNS Init failed: %d\n", err);
        return;
    }
    mdns_hostname_set(user_config.mdns);
}

//Funkce pro načtení souboru index.html a uložení do proměnné index_html
static void initi_web_page_buffer(void)
{
    memset((void *)index_html, 0, sizeof(index_html));	//Vyplnění paměťové oblasti nulami
    memset((void *)logo_svg, 0, sizeof(logo_svg));
    memset((void *)admin_html, 0, sizeof(admin_html));

    //Získání informací o souboru "index.html"
    struct stat st;
    if (stat(INDEX_PATH, &st))
    {
        ESP_LOGE(TAG, "index.html not found");
        return;
    }
    FILE *fp = fopen(INDEX_PATH, "r");	//Otevření souboru "index.html" pro čtení
    //Přečtení dat ze souboru a uložení do index_html (pokud čtení nevyjde, je zahlášena chyba)
    if (fread(index_html, st.st_size, 1, fp) == 0)
    {
        ESP_LOGE(TAG, "fread failed");
    }
    fclose(fp);	//Uzavření souboru po provedení operace čtení

    // Načtení loga z paměti
    if (stat(LOGO_PATH, &st))
    {
        ESP_LOGE(TAG, "logo.svg not found");
        return;
    }
    fp = fopen(LOGO_PATH, "r");
    if (fread(logo_svg, st.st_size, 1, fp) == 0)
    {
        ESP_LOGE(TAG, "fread failed");
    }
    fclose(fp);

    // Načtení admin.html z paměti
    if (stat(ADMIN_PATH, &st))
    {
        ESP_LOGE(TAG, "admin.html not found");
        return;
    }
    fp = fopen(ADMIN_PATH, "r");
    if (fread(admin_html, st.st_size, 1, fp) == 0)
    {
        ESP_LOGE(TAG, "fread failed");
    }
    fclose(fp);
}

//Funkce pro obsluhu HTTP GET požadavků
esp_err_t get_req_handler(httpd_req_t *req)
{
    int response;
    if(led_state)
    {
        sprintf(response_data, index_html, 1);
    }
    else
    {
        sprintf(response_data, index_html, 0);
    }
    response = httpd_resp_send(req, response_data, HTTPD_RESP_USE_STRLEN);
    return response;
}

//Funkce pro odesílání zpráv přes WebSocket
static void ws_async_send(void *arg)
{
    httpd_ws_frame_t ws_pkt;	//Inicializace websocket rámce
    struct async_resp_arg *resp_arg = arg;

    char *resp_str = cJSON_Print(&json_in);
    ESP_LOGI(TAG, "Json out: %s", resp_str);

    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = (uint8_t *)resp_str;	//Nastavení atributu přenášené zprávy na data
    //ws_pkt.payload = (uint8_t *)data;			//Nastavení atributu přenášené zprávy na data
    ws_pkt.len = strlen(resp_str);					//Nastavení atributu délky podle délky bufferu
    ws_pkt.type = HTTPD_WS_TYPE_TEXT; 			//Nastavení atributu type rámce na text

    size_t fds = CONFIG_LWIP_MAX_LISTENING_TCP; //Získání maximálního možného počtu naslouchajících TCP spojení

    int client_fds[fds];

    esp_err_t ret = httpd_get_client_list(server, &fds, client_fds);	//Získání maximálního možného počtu naslouchajících TCP spojení
    if (ret != ESP_OK) {
        return;
    }

    for (int i = 0; i < fds; i++) {
        int client_info = httpd_ws_get_fd_info(server, client_fds[i]); //Záskání inforamce o klientovi na základě file descriptoru
        if (client_info == HTTPD_WS_CLIENT_WEBSOCKET) {
            httpd_ws_send_frame_async(resp_arg->hd, client_fds[i], &ws_pkt); //Odeslání rámce přes WS
        }
    }
    free(resp_arg);
}

//Funkce pro vložení odesílací funkce do pracovní fronty
static esp_err_t trigger_async_send(httpd_handle_t handle, httpd_req_t *req)
{
    struct async_resp_arg *resp_arg = malloc(sizeof(struct async_resp_arg));
    resp_arg->hd = req->handle;
    resp_arg->fd = httpd_req_to_sockfd(req);
    return httpd_queue_work(handle, ws_async_send, resp_arg);
}

//Funkce pro zpracování přijaté zprávy
static esp_err_t handle_ws_req(httpd_req_t *req)
{

    if (req->method == HTTP_GET) //Kontrola metody
    {
        ESP_LOGI(TAG, "Handshake done, the new connection was opened");
        return ESP_OK;
    }

    httpd_ws_frame_t ws_pkt;	//Inicializace websocket rámce
    uint8_t *buf = NULL;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t)); //Nastavení WS rámce na nuly

    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0); //Získání pouze délky WS rámce z req (max_len = 0 - zjistí pouze délku rámce)
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "httpd_ws_recv_frame failed to get frame len with %d", ret);
        return ret;
    }

    if (ws_pkt.len)
    {
        buf = calloc(1, ws_pkt.len + 1);
        if (buf == NULL)
        {
            ESP_LOGE(TAG, "Failed to calloc memory for buf");
            return ESP_ERR_NO_MEM;
        }

        ws_pkt.payload = buf;

        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "httpd_ws_recv_frame failed with %d", ret);
            free(buf);
            return ret;
        }
        ESP_LOGI(TAG, "Got packet with message: %s", ws_pkt.payload);
    }

    ESP_LOGI(TAG, "frame len is %d", ws_pkt.len);

    if (ws_pkt.type == HTTPD_WS_TYPE_TEXT){
    	cJSON *root = cJSON_Parse((char *)ws_pkt.payload);
    	json_in = *cJSON_Parse((char *)ws_pkt.payload);

    	char *tempjson = cJSON_GetObjectItem(&json_in,"type")->valuestring;
    	ESP_LOGI(TAG, "Json in: %s", tempjson);


    	char *typew = cJSON_GetObjectItem(root,"type")->valuestring;

    	if (strcmp(typew, "svalue") == 0){
    		ESP_LOGI(TAG, "Type is %s", typew);
    		int idw = cJSON_GetObjectItem(root,"id")->valueint;
    		ESP_LOGI(TAG, "ID is %d", idw);
    		int valuew = cJSON_GetObjectItem(root,"value")->valueint;
    		ESP_LOGI(TAG, "Value is %d", valuew);
    		free(buf);
    		return trigger_async_send(req->handle, req); //Odeslání dat na web
    	}
    	else if (strcmp(typew, "button") == 0){

			ESP_LOGI(TAG, "Type is %s", typew);
			int idw = cJSON_GetObjectItem(root,"id")->valueint;
			ESP_LOGI(TAG, "ID is %d", idw);
			if (idw==1){
			    //Přenastavení LED
			    led_state = !led_state;
			    gpio_set_level(LED_PIN, led_state);

				free(buf);
				return trigger_async_send(req->handle, req); //Odeslání dat na web
			}
    	}
		else if (strcmp(typew, "login") == 0){

			ESP_LOGI(TAG, "Type is %s", typew);
			char *pass = cJSON_GetObjectItem(root,"value")->valuestring;
			int loginval = 0;
			if (strcmp(pass, user_config.admin_pass) == 0){
				loginval = 1;
			}
			cJSON *loginjson;
			loginjson=cJSON_CreateObject();
			cJSON_AddItemToObject(loginjson, "type", cJSON_CreateString("login"));
			cJSON_AddItemToObject(loginjson, "value", cJSON_CreateNumber(loginval));
			free(buf);
			char *loginjsonout = cJSON_Print(loginjson);
			ESP_LOGI(TAG, "Json out: %s", loginjsonout);

			httpd_ws_frame_t ws_pkt;
			memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
			ws_pkt.payload = (uint8_t *)loginjsonout;	//Nastavení atributu přenášené zprávy na data
			//ws_pkt.payload = (uint8_t *)data;			//Nastavení atributu přenášené zprávy na data
			ws_pkt.len = strlen(loginjsonout);					//Nastavení atributu délky podle délky bufferu
			ws_pkt.type = HTTPD_WS_TYPE_TEXT;
			httpd_ws_send_frame_async(req->handle, httpd_req_to_sockfd(req), &ws_pkt);
		}
    /*
    if (ws_pkt.type == HTTPD_WS_TYPE_TEXT &&
        strcmp((char *)ws_pkt.payload, "button1") == 0) //Podmínka, zda je přijatá zpráva "button1" (bylo stisknuto tlačítko 1 na webu)
    {
        free(buf);
        return trigger_async_send(req->handle, req); //Odeslání dat na web
    }
    */
    }
    return ESP_OK;
}

esp_err_t logo_get_handler(httpd_req_t *req)
{
    //Nastavení HTTP hlavičky s MIME typem pro SVG soubor
    httpd_resp_set_type(req, "image/svg+xml");

    //Odeslání obsahu logo.svg jako odpovědi na GET požadavek
    if (httpd_resp_send(req, logo_svg, HTTPD_RESP_USE_STRLEN) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to send logo.svg");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t get_admin_handler(httpd_req_t *req)
{
    if (httpd_resp_send(req, admin_html, HTTPD_RESP_USE_STRLEN) != ESP_OK)
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
        .handler = get_req_handler,
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
		.handler = logo_get_handler,
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

void app_main()
{
    //Inicializace NVS (Non-Volatile Storage) - potřebné pro konfiguraci WiFi
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    load_config();

    wifi_init_softap();

    esp_rom_gpio_pad_select_gpio(LED_PIN);
	gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);

	initi_web_page_buffer();

	setup_server();
	start_mdns_service();

	ESP_LOGI(TAG, "ESP32 ESP-IDF WebSocket Web Server is running ... ...\n");
}
