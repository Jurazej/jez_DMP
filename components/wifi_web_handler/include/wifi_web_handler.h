#ifndef WIFI_WEB_HANDLER
#define WIFI_WEB_HANDLER

extern httpd_handle_t server;

void start_mdns_service(void);
void wifi_init_softap(void);

esp_err_t get_index_handler(httpd_req_t *req);
esp_err_t get_logo_handler(httpd_req_t *req);
esp_err_t get_admin_handler(httpd_req_t *req);
httpd_handle_t setup_server(void);

#endif
