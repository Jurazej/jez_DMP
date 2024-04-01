#ifndef FILE_HANDLER
#define FILE_HANDLER

// Umístění souborů
#define INDEX_PATH "/spiffs/index.html"
#define INDEX_JS_PATH "/spiffs/index.js"
#define LOGO_PATH "/spiffs/logo.svg"
#define LOGO_HELLA_PATH "/spiffs/hella_logo.svg"
#define INFO_ICON_PATH "/spiffs/info_icon.svg"
#define ADMIN_PATH "/spiffs/admin.html"
#define ADMIN_JS_PATH "/spiffs/admin.js"
#define DEFAULT_CONFIG_PATH "/spiffs/default_config.txt"
#define CONFIG_PATH "/spiffs/config.txt"
#define LIGHTS_PATH "/spiffs/lights.txt"

#include "cjson.h"

typedef struct {
	char* index_html;
	char* index_js;
	char* logo_svg;
	char* admin_html;
	char* admin_js;
	char* logo_hella_svg;
	char* info_icon_svg;
}files_t;

typedef struct {
	char* ssid;
	char* wifi_pass;
	int maxcon;
	char* mdns;
	char* admin_pass;
}user_config_t;

typedef struct {
	_Bool ssid;
	_Bool wifi_pass;
	_Bool maxcon;
	_Bool mdns;
	_Bool admin_pass;
}change_of_config_t;

typedef struct {
    uint8_t type;
    uint8_t brightness;
    uint8_t light;
    uint16_t speed;
}animlight_t;

typedef struct {
	uint8_t index;
	uint8_t nop;
	char *part_name[5];
	animlight_t anim[5];
}light_t;

typedef struct {
	uint8_t nol;
	light_t light[4];
	cJSON *json;
}lights_t;

extern files_t *loaded_files;
extern user_config_t *user_config;
extern lights_t lights_;

void init_spiffs(void);
_Bool load_web(void);
_Bool load_config(void);
_Bool load_default_config(void);
_Bool save_config(user_config_t edited_u_conf, change_of_config_t config_changed);
_Bool save_light_params(lights_t* in_lights);
_Bool load_ligts_params(void);

#endif
