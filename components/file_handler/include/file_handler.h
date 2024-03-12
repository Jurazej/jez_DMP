#ifndef FILE_HANDLER
#define FILE_HANDLER

// Umístění souborů
#define INDEX_PATH "/spiffs/index.html"
#define LOGO_PATH "/spiffs/logo.svg"
#define ADMIN_PATH "/spiffs/admin.html"
#define DEFAULT_CONFIG_PATH "/spiffs/default_config.txt"
#define CONFIG_PATH "/spiffs/config.txt"

typedef struct {
	char* index_html;
	char* logo_svg;
	char* admin_html;
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

extern files_t *loaded_files;
extern user_config_t *user_config;

void init_spiffs(void);
_Bool load_web(void);
_Bool load_config(void);
_Bool load_default_config(void);
_Bool save_config(user_config_t edited_u_conf, change_of_config_t config_changed);

#endif
