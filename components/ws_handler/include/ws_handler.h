#ifndef WS_HANDLER
#define WS_HANDLER

typedef struct {
    httpd_handle_t hd;
    int fd;
    char *json_in;
}async_resp_arg_t;

esp_err_t handle_ws_req(httpd_req_t *req);

#endif