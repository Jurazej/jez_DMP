#ifndef WS_HANDLER
#define WS_HANDLER

typedef struct {
    httpd_handle_t hd;
    int fd;
    httpd_ws_frame_t ws_pkt_in;
}async_resp_arg_t;

typedef struct {
    uint8_t type, brightness;
    uint16_t speed;

}animlight_t;

esp_err_t handle_ws_req(httpd_req_t *req);

#endif
