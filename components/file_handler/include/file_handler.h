#ifndef FILE_HANDLER
#define FILE_HANDLER

// Umístění souborů
#define INDEX_PATH "/spiffs/index.html"
#define LOGO_PATH "/spiffs/logo.svg"
#define ADMIN_PATH "/spiffs/admin.html"
#define DEFAULTCONFIG_PATH "/spiffs/default_config.txt"
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

extern files_t *loaded_files;
extern user_config_t *user_config;

void load_web(void);
void init_spiffs(void);
void load_config(void);
int save_config(user_config_t edited_u_conf);

#endif
