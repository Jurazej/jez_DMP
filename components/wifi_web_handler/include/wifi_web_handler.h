#ifndef WIFI_WEB_HANDLER
#define WIFI_WEB_HANDLER

extern httpd_handle_t server;
extern httpd_handle_t wss_server;


void start_mdns_service(void);
esp_err_t wifi_init_softap(void);

esp_err_t get_index_handler(httpd_req_t *req);
esp_err_t get_index_js_handler(httpd_req_t *req);
//esp_err_t get_index_css_handler(httpd_req_t *req);
esp_err_t get_logo_handler(httpd_req_t *req);
esp_err_t get_admin_handler(httpd_req_t *req);
//esp_err_t get_admin_css_handler(httpd_req_t *req);
esp_err_t get_admin_js_handler(httpd_req_t *req);
httpd_handle_t setup_server(void);
httpd_handle_t setup_secure_server(void);

#endif
