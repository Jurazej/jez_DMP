#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <lwip/sockets.h>
#include "mdns.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_https_server.h"

#include "keep_alive.h"
#include "ws_handler.h"
#include "file_handler.h"
#include "wifi_web_handler.h"

static const char *TAG = "Wifi_web_handler";

httpd_handle_t server;
httpd_handle_t wss_server;

struct async_resp_arg {
    httpd_handle_t hd;
    int fd;
};

//---------------------------------------------------------
// Nastavení a spuštění WiFi Access Point (AP)
//---------------------------------------------------------
esp_err_t wifi_init_softap(void) {
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
    strcpy((char*)wifi_config.ap.ssid, user_config->ssid);
    strcpy((char*)wifi_config.ap.password, user_config->wifi_pass);

    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    return esp_wifi_start();
}

//---------------------------------------------------------
// Služba mdns
//---------------------------------------------------------
void start_mdns_service(void) {
    esp_err_t err = mdns_init();
    if (err) {
        ESP_LOGE(TAG, "MDNS Init failed: %d\n", err);
        return;
    }
    mdns_hostname_set(user_config->mdns);
    ESP_LOGI(TAG, "mDNS hostname set to %s",user_config->mdns);
}

//---------------------------------------------------------
// Funkce odpovídající na http requesty
//---------------------------------------------------------
esp_err_t get_index_handler(httpd_req_t *req) {
    if (httpd_resp_send(req, loaded_files->index_html, HTTPD_RESP_USE_STRLEN) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send index.html");
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t get_index_js_handler(httpd_req_t *req) {
    if (httpd_resp_send(req, loaded_files->index_js, HTTPD_RESP_USE_STRLEN) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send index.html");
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t get_logo_handler(httpd_req_t *req) {
    //Nastavení HTTP hlavičky s MIME typem pro SVG soubor
    httpd_resp_set_type(req, "image/svg+xml");

    //Odeslání obsahu logo.svg jako odpovědi na GET požadavek
    if (httpd_resp_send(req, loaded_files->logo_svg, HTTPD_RESP_USE_STRLEN) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send logo.svg");
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t get_logo_hella_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "image/svg+xml");

    if (httpd_resp_send(req, loaded_files->logo_hella_svg, HTTPD_RESP_USE_STRLEN) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send logo_hella.svg");
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t get_info_icon_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "image/svg+xml");

    if (httpd_resp_send(req, loaded_files->info_icon_svg, HTTPD_RESP_USE_STRLEN) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send info_icon.svg");
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t get_admin_handler(httpd_req_t *req) {
    if (httpd_resp_send(req, loaded_files->admin_html, HTTPD_RESP_USE_STRLEN) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send admin.html");
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t get_admin_js_handler(httpd_req_t *req) {
    if (httpd_resp_send(req, loaded_files->admin_js, HTTPD_RESP_USE_STRLEN) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send admin.js");
        return ESP_FAIL;
    }
    return ESP_OK;
}

//---------------------------------------------------------
// Uzavření soketu HTTP serveru
//---------------------------------------------------------
void close_fd_cb(httpd_handle_t hd, int sockfd)
{
   struct linger so_linger;
   so_linger.l_onoff = true;
   so_linger.l_linger = 0;
   lwip_setsockopt(sockfd, SOL_SOCKET, SO_LINGER, &so_linger, sizeof(so_linger));
   close(sockfd);
}

//---------------------------------------------------------
// Vytvoření a konfiguraci HTTP serveru
//---------------------------------------------------------
httpd_handle_t setup_server(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_open_sockets = 10;
    config.lru_purge_enable = true;
    config.close_fn = close_fd_cb;

    httpd_uri_t uri_get = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = get_index_handler,
        .user_ctx = NULL};

    httpd_uri_t uri_index_js_get = {
        .uri = "/index.js",
        .method = HTTP_GET,
        .handler = get_index_js_handler,
        .user_ctx = NULL};

    httpd_uri_t uri_logo_get = {
		.uri = "/logo.svg",
		.method = HTTP_GET,
		.handler = get_logo_handler,
		.user_ctx = NULL};

    httpd_uri_t uri_logo_hella_get = {
		.uri = "/hella_logo.svg",
		.method = HTTP_GET,
		.handler = get_logo_hella_handler,
		.user_ctx = NULL};

    httpd_uri_t uri_info_icon_get = {
		.uri = "/info_icon.svg",
		.method = HTTP_GET,
		.handler = get_info_icon_handler,
		.user_ctx = NULL};

    httpd_uri_t ws = {
		.uri        = "/ws",
		.method     = HTTP_GET,
		.handler    = handle_ws_req,
		.user_ctx   = NULL,
		.is_websocket = true,
		.handle_ws_control_frames = true};

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &uri_get);
        httpd_register_uri_handler(server, &uri_index_js_get);
        httpd_register_uri_handler(server, &uri_logo_get);
        httpd_register_uri_handler(server, &uri_logo_hella_get);
        httpd_register_uri_handler(server, &uri_info_icon_get);
        httpd_register_uri_handler(server, &ws);
    }

    return server;
}

//---------------------------------------------------------
// Odesílání ping odpovědi přes WSS
//---------------------------------------------------------
static void send_ping(void *arg)
{
    struct async_resp_arg *resp_arg = arg;
    httpd_handle_t hd = resp_arg->hd;
    int fd = resp_arg->fd;
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = NULL;
    ws_pkt.len = 0;
    ws_pkt.type = HTTPD_WS_TYPE_PING;

    httpd_ws_send_frame_async(hd, fd, &ws_pkt);
    free(resp_arg);
}

//---------------------------------------------------------
// Zpracování nově připojených klientů pro WSS
//---------------------------------------------------------
esp_err_t wss_open_fd(httpd_handle_t hd, int sockfd)
{
    ESP_LOGI(TAG, "New client connected %d", sockfd);
    wss_keep_alive_t h = httpd_get_global_user_ctx(hd);
    return wss_keep_alive_add_client(h, sockfd);
}

//---------------------------------------------------------
// Zpracování při odpojení klienta pro WSS
//---------------------------------------------------------
void wss_close_fd(httpd_handle_t hd, int sockfd)
{
    ESP_LOGI(TAG, "Client disconnected %d", sockfd);
    wss_keep_alive_t h = httpd_get_global_user_ctx(hd);
    wss_keep_alive_remove_client(h, sockfd);
    close(sockfd);
}

//---------------------------------------------------------
// Detekce neaktivního klienta
//---------------------------------------------------------
_Bool client_not_alive_cb(wss_keep_alive_t h, int fd)
{
    ESP_LOGE(TAG, "Client not alive, closing fd %d", fd);
    httpd_sess_trigger_close(wss_keep_alive_get_user_ctx(h), fd);
    return true;
}

//---------------------------------------------------------
// Kontrola, zda je klient aktivní
//---------------------------------------------------------
_Bool check_client_alive_cb(wss_keep_alive_t h, int fd)
{
    ESP_LOGD(TAG, "Checking if client (fd=%d) is alive", fd);
    struct async_resp_arg *resp_arg = malloc(sizeof(struct async_resp_arg));
    resp_arg->hd = wss_keep_alive_get_user_ctx(h);
    resp_arg->fd = fd;

    if (httpd_queue_work(resp_arg->hd, send_ping, resp_arg) == ESP_OK) {
        return true;
    }
    return false;
}

//---------------------------------------------------------
// Vytvoření a konfiguraci HTTPS serveru
//---------------------------------------------------------
httpd_handle_t setup_secure_server(void) {
    // Připravit službu keep-alive
    wss_keep_alive_config_t keep_alive_config = KEEP_ALIVE_CONFIG_DEFAULT();
    keep_alive_config.not_alive_after_ms = 300000;
    keep_alive_config.client_not_alive_cb = client_not_alive_cb;
    keep_alive_config.check_client_alive_cb = check_client_alive_cb;
    wss_keep_alive_t keep_alive = wss_keep_alive_start(&keep_alive_config);

	httpd_ssl_config_t ssl_config = HTTPD_SSL_CONFIG_DEFAULT();
	ssl_config.httpd.lru_purge_enable = true;
	ssl_config.httpd.global_user_ctx = keep_alive;
	ssl_config.httpd.open_fn = wss_open_fd;
	ssl_config.httpd.close_fn = wss_close_fd;
	ssl_config.httpd.ctrl_port = 32769;
    extern const unsigned char servercert_start[] asm("_binary_servercert_pem_start");
    extern const unsigned char servercert_end[]   asm("_binary_servercert_pem_end");
    ssl_config.servercert = servercert_start;
    ssl_config.servercert_len = servercert_end - servercert_start;

    extern const unsigned char prvtkey_pem_start[] asm("_binary_prvtkey_pem_start");
    extern const unsigned char prvtkey_pem_end[]   asm("_binary_prvtkey_pem_end");
    ssl_config.prvtkey_pem = prvtkey_pem_start;
    ssl_config.prvtkey_len = prvtkey_pem_end - prvtkey_pem_start;

    httpd_uri_t ws = {
		.uri        = "/wss",
		.method     = HTTP_GET,
		.handler    = handle_wss_req,
		.user_ctx   = NULL,
		.is_websocket = true,
		.handle_ws_control_frames = true};

    httpd_uri_t uri_admin_get = {
		.uri = "/admin",
		.method = HTTP_GET,
		.handler = get_admin_handler,
		.user_ctx = NULL};

    httpd_uri_t uri_admin_js_get = {
		.uri = "/admin.js",
		.method = HTTP_GET,
		.handler = get_admin_js_handler,
		.user_ctx = NULL};

    httpd_uri_t uri_logo_get = {
		.uri = "/logo.svg",
		.method = HTTP_GET,
		.handler = get_logo_handler,
		.user_ctx = NULL};

	if (httpd_ssl_start(&wss_server, &ssl_config) == ESP_OK) {
	    httpd_register_uri_handler(wss_server, &ws);
        httpd_register_uri_handler(wss_server, &uri_admin_get);
        httpd_register_uri_handler(wss_server, &uri_admin_js_get);
        httpd_register_uri_handler(wss_server, &uri_logo_get);
	    wss_keep_alive_set_user_ctx(keep_alive, wss_server);
	}
    return wss_server;
}
