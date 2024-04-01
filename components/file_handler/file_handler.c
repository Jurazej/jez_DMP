#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "esp_log.h"
#include "esp_spiffs.h"

#include "file_handler.h"

static const char *TAG = "File_handler";

lights_t lights_;

files_t *loaded_files;
user_config_t *user_config;

//---------------------------------------------------------
// Inicializace SPIFFS
//---------------------------------------------------------
void init_spiffs(void) {
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 12,
        .format_if_mount_failed = true};

    //Registrace SPIFFS pomocí nastavené konfigurace
    ESP_ERROR_CHECK(esp_vfs_spiffs_register(&conf));
}

//---------------------------------------------------------
// Načtení configu z paměti do proměnné user_config
//---------------------------------------------------------
_Bool load_config(void) {
    char *loaded_json_config;

    FILE *config_file = fopen(CONFIG_PATH, "r");
    if (config_file == NULL) {
    	ESP_LOGE(TAG, "Error while opening file %s",CONFIG_PATH);
    	return false;
    }
    fseek(config_file, 0, SEEK_END);
    size_t file_size = ftell(config_file);
    rewind(config_file);
    loaded_json_config = (char *)malloc(file_size+1);
    if (fread(loaded_json_config, file_size, 1, config_file) == 0)
    {
    	ESP_LOGE(TAG, "Error while reading file %s",CONFIG_PATH);
    	return false;
    }
    loaded_json_config[file_size] = '\0';
    fclose(config_file);

    ESP_LOGI(TAG, "Loaded config: %s", loaded_json_config);
    user_config = malloc(sizeof(user_config_t));
	cJSON *root = cJSON_Parse(loaded_json_config);

	cJSON *item;
	item = cJSON_GetObjectItem(root, "ssid");
	if (item != NULL && cJSON_IsString(item)) {
	    size_t length = strlen(item->valuestring);
	    user_config->ssid = malloc(length+1);
	    if (user_config->ssid != NULL) {
	        strcpy(user_config->ssid, item->valuestring);
	    }
	    else {
	    	ESP_LOGE(TAG, "Failed to allocate memory for ssid");
	    	return false;
	    }
	}
	else {
		ESP_LOGE(TAG, "Invalid value of ssid");
		return false;
	}

	item = cJSON_GetObjectItem(root, "wifi_pass");
	if (item != NULL && cJSON_IsString(item)) {
	    size_t length = strlen(item->valuestring);
	    user_config->wifi_pass = malloc(length+1);
	    if (user_config->wifi_pass != NULL) {
	        strcpy(user_config->wifi_pass, item->valuestring);
	    }
	    else {
	    	ESP_LOGE(TAG, "Failed to allocate memory for wifi_pass");
	    	return false;
	    }
	}
	else {
		ESP_LOGE(TAG, "Invalid value of wifi_pass");
		return false;
	}

	user_config->maxcon = cJSON_GetObjectItem(root,"maxcon")->valueint;

	item = cJSON_GetObjectItem(root, "mdns");
	if (item != NULL && cJSON_IsString(item)) {
	    size_t length = strlen(item->valuestring);
	    user_config->mdns = malloc(length+1);
	    if (user_config->mdns != NULL) {
	        strcpy(user_config->mdns, item->valuestring);
	    }
	    else {
	    	ESP_LOGE(TAG, "Failed to allocate memory for mdns");
	    	return false;
	    }
	}
	else {
		ESP_LOGE(TAG, "Invalid value of mdns");
		return false;
	}

	item = cJSON_GetObjectItem(root, "admin_pass");
	if (item != NULL && cJSON_IsString(item)) {
	    size_t length = strlen(item->valuestring);
	    user_config->admin_pass = malloc(length+1);
	    if (user_config->admin_pass != NULL) {
	        strcpy(user_config->admin_pass, item->valuestring);
	    }
	    else {
	    	ESP_LOGE(TAG, "Failed to allocate memory for admin_pass");
	    	return false;
	    }
	}
	else {
		ESP_LOGE(TAG, "Invalid value of admin_pass");
		return false;
	}

    free(loaded_json_config);
    cJSON_Delete(root);
    return true;
}

//---------------------------------------------------------
// Načtení parametrů světlometů z paměti do proměnné lights_
//---------------------------------------------------------
_Bool load_ligts_params(void) {
    FILE *fp = fopen(LIGHTS_PATH, "r");
    if (fp == NULL) {
    	ESP_LOGE(TAG, "Error while opening file %s",LIGHTS_PATH);
    	return false;
    }
    fseek(fp, 0, SEEK_END);
    size_t file_size = ftell(fp);
    rewind(fp);
    char *loaded = (char *)malloc(file_size+1);
    if (fread(loaded, file_size, 1, fp) == 0)
    {
    	ESP_LOGE(TAG, "Error while reading file %s",LIGHTS_PATH);
    	fclose(fp);
    	return false;
    }
    loaded[file_size] = '\0';
    fclose(fp);

    cJSON* root = cJSON_Parse(loaded);

    lights_.nol = cJSON_GetObjectItem(root, "nol")->valueint;

    cJSON* light_array = cJSON_GetObjectItem(root, "lights");

    for (int i = 0; i < lights_.nol; i++) {
        cJSON* light_obj = cJSON_GetArrayItem(light_array, i);
        lights_.light[i].index = cJSON_GetObjectItem(light_obj, "index")->valueint;
        lights_.light[i].nop = cJSON_GetObjectItem(light_obj, "nop")->valueint;

        cJSON* part_name_array = cJSON_GetObjectItem(light_obj, "part_name");

        for (int j = 0; j < lights_.light[i].nop; j++) {
            cJSON* part_name_item = cJSON_GetArrayItem(part_name_array, j);
            lights_.light[i].part_name[j] = (char*)malloc(strlen(part_name_item->valuestring) + 1);
            strcpy(lights_.light[i].part_name[j], part_name_item->valuestring);
            lights_.light[i].anim[j].brightness = 125;
            lights_.light[i].anim[j].speed = 5200;
        }
    }
    lights_.json = root;
    free(loaded);
    return true;
}

//---------------------------------------------------------
// Uložení parametrů světlometů z proměnné do paměti
//---------------------------------------------------------
_Bool save_light_params(lights_t* in_lights) {
	FILE *f = fopen(LIGHTS_PATH, "w");
    if (f == NULL) {
    	ESP_LOGE(TAG, "Error while opening file %s",LIGHTS_PATH);
        return false;
    }
    cJSON* root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "nol", in_lights->nol);

    cJSON* light_array = cJSON_AddArrayToObject(root, "lights");
    for (int i = 0; i < in_lights->nol; i++) {
        cJSON* light_obj = cJSON_CreateObject();
        cJSON_AddNumberToObject(light_obj, "index", in_lights->light[i].index);
        cJSON_AddNumberToObject(light_obj, "nop", in_lights->light[i].nop);

        cJSON* part_name_array = cJSON_AddArrayToObject(light_obj, "part_name");
        for (int j = 0; j < in_lights->light[i].nop; j++) {
            cJSON_AddItemToArray(part_name_array, cJSON_CreateString(in_lights->light[i].part_name[j]));
        }
        cJSON_AddItemToArray(light_array, light_obj);
    }

	char *conf_to_save = cJSON_Print(root);
	cJSON_Delete(root);
	if (conf_to_save == NULL) {
	    ESP_LOGE(TAG, "Failed to allocate memory for light parameters string");
	    fclose(f);
	    return false;
	}
	else {
		size_t conf_to_save_len = strlen(conf_to_save);
	    ESP_LOGI(TAG, "Saving light parameters: %s", conf_to_save);
	    if (fwrite(conf_to_save, 1, conf_to_save_len, f) != conf_to_save_len) {
	        ESP_LOGE(TAG, "Failed to save new light parameters file");
	        free(conf_to_save);
	        fclose(f);
	        return false;
	    }
	    else {
	        ESP_LOGI(TAG, "New light parameters file was saved successfully");
	        free(conf_to_save);
	        fclose(f);
	        return true;
	    }
	}
}

//---------------------------------------------------------
// Načtení souborů pro webové stránky
//---------------------------------------------------------
_Bool load_web(void) {
	loaded_files = malloc(sizeof(files_t));
    // Načtení index.html z paměti
    FILE *fp = fopen(INDEX_PATH, "r");
    if (fp == NULL) {
    	ESP_LOGE(TAG, "Error while opening file %s",INDEX_PATH);
    	return false;
    }
    fseek(fp, 0, SEEK_END);
    size_t file_size = ftell(fp);
    rewind(fp);
    loaded_files->index_html = (char *)malloc(file_size+1);
    if (fread(loaded_files->index_html, file_size, 1, fp) == 0)
    {
    	ESP_LOGE(TAG, "Error while reading file %s",INDEX_PATH);
    	fclose(fp);
    	return false;
    }
    loaded_files->index_html[file_size] = '\0';
    fclose(fp);

    // Načtení index.js z paměti
    fp = fopen(INDEX_JS_PATH, "r");
    if (fp == NULL) {
    	ESP_LOGE(TAG, "Error while opening file %s",INDEX_JS_PATH);
    	return false;
    }
    fseek(fp, 0, SEEK_END);
    file_size = ftell(fp);
    rewind(fp);
    loaded_files->index_js = (char *)malloc(file_size+1);
    if (fread(loaded_files->index_js, file_size, 1, fp) == 0)
    {
    	ESP_LOGE(TAG, "Error while reading file %s",INDEX_JS_PATH);
    	fclose(fp);
    	return false;
    }
    loaded_files->index_js[file_size] = '\0';
    fclose(fp);

    // Načtení logo.svg z paměti
    fp = fopen(LOGO_PATH, "r");
    if (fp == NULL) {
    	ESP_LOGE(TAG, "Error while opening file %s",LOGO_PATH);
    	return false;
    }
    fseek(fp, 0, SEEK_END);
    file_size = ftell(fp);
    rewind(fp);
    loaded_files->logo_svg = (char *)malloc(file_size+1);
    if (fread(loaded_files->logo_svg, file_size, 1, fp) == 0)
    {
    	ESP_LOGE(TAG, "Error while reading file %s",LOGO_PATH);
    	fclose(fp);
    	return false;
    }
    loaded_files->logo_svg[file_size] = '\0';
    fclose(fp);

    // Načtení hella_logo.svg z paměti
    fp = fopen(LOGO_HELLA_PATH, "r");
    if (fp == NULL) {
    	ESP_LOGE(TAG, "Error while opening file %s",LOGO_HELLA_PATH);
    	return false;
    }
    fseek(fp, 0, SEEK_END);
    file_size = ftell(fp);
    rewind(fp);
    loaded_files->logo_hella_svg = (char *)malloc(file_size+1);
    if (fread(loaded_files->logo_hella_svg, file_size, 1, fp) == 0)
    {
    	ESP_LOGE(TAG, "Error while reading file %s",LOGO_HELLA_PATH);
    	fclose(fp);
    	return false;
    }
    loaded_files->logo_hella_svg[file_size] = '\0';
    fclose(fp);

    // Načtení info_icon.svg z paměti
    fp = fopen(INFO_ICON_PATH, "r");
    if (fp == NULL) {
    	ESP_LOGE(TAG, "Error while opening file %s",INFO_ICON_PATH);
    	return false;
    }
    fseek(fp, 0, SEEK_END);
    file_size = ftell(fp);
    rewind(fp);
    loaded_files->info_icon_svg = (char *)malloc(file_size+1);
    if (fread(loaded_files->info_icon_svg, file_size, 1, fp) == 0)
    {
    	ESP_LOGE(TAG, "Error while reading file %s",INFO_ICON_PATH);
    	fclose(fp);
    	return false;
    }
    loaded_files->info_icon_svg[file_size] = '\0';
    fclose(fp);

    // Načtení admin.html z paměti
    fp = fopen(ADMIN_PATH, "r");
    if (fp == NULL) {
    	ESP_LOGE(TAG, "Error while opening file %s",ADMIN_PATH);
    	return false;
    }
    fseek(fp, 0, SEEK_END);
    file_size = ftell(fp);
    rewind(fp);
    loaded_files->admin_html = (char *)malloc(file_size+1);
    if (fread(loaded_files->admin_html, file_size, 1, fp) == 0)
    {
    	ESP_LOGE(TAG, "Error while reading file %s",ADMIN_PATH);
    	fclose(fp);
    	return false;
    }
    loaded_files->admin_html[file_size] = '\0';
    fclose(fp);

    // Načtení admin.js z paměti
    fp = fopen(ADMIN_JS_PATH, "r");
    if (fp == NULL) {
    	ESP_LOGE(TAG, "Error while opening file %s",ADMIN_JS_PATH);
    	return false;
    }
    fseek(fp, 0, SEEK_END);
    file_size = ftell(fp);
    rewind(fp);
    loaded_files->admin_js = (char *)malloc(file_size+1);
    if (fread(loaded_files->admin_js, file_size, 1, fp) == 0)
    {
    	ESP_LOGE(TAG, "Error while reading file %s",ADMIN_JS_PATH);
    	fclose(fp);
    	return false;
    }
    loaded_files->admin_js[file_size] = '\0';
    fclose(fp);

    return true;
}

//---------------------------------------------------------
// Uložení nového configu do paměti
//---------------------------------------------------------
_Bool save_config(user_config_t edited_u_conf, change_of_config_t config_changed) {
	FILE *f = fopen(CONFIG_PATH, "w");
    if (f == NULL) {
    	ESP_LOGE(TAG, "Error while opening file %s",CONFIG_PATH);
        return false;
    }
    cJSON *conf_json = cJSON_CreateObject();
	if (config_changed.ssid == 1){
		ESP_LOGI(TAG, "Changing ssid: %s", edited_u_conf.ssid);
		cJSON_AddStringToObject(conf_json,"ssid",edited_u_conf.ssid);
	}
	else {
		ESP_LOGI(TAG, "Leaving ssid: %s", user_config->ssid);
		cJSON_AddStringToObject(conf_json,"ssid",user_config->ssid);
	}
	if (config_changed.wifi_pass == 1){
		ESP_LOGI(TAG, "Changing wifi_pass: %s", edited_u_conf.wifi_pass);
		cJSON_AddStringToObject(conf_json,"wifi_pass",edited_u_conf.wifi_pass);
	}
	else {
		ESP_LOGI(TAG, "Leaving wifi_pass: %s", user_config->wifi_pass);
		cJSON_AddStringToObject(conf_json,"wifi_pass",user_config->wifi_pass);
	}
	if (config_changed.maxcon == 1){
		ESP_LOGI(TAG, "Changing maxcon: %d", edited_u_conf.maxcon);
		cJSON_AddNumberToObject(conf_json,"maxcon",edited_u_conf.maxcon);
	}
	else {
		ESP_LOGI(TAG, "Leaving maxcon: %d", user_config->maxcon);
		cJSON_AddNumberToObject(conf_json,"maxcon",user_config->maxcon);
	}
	if (config_changed.mdns == 1){
		ESP_LOGI(TAG, "Changing mdns: %s", edited_u_conf.mdns);
		cJSON_AddStringToObject(conf_json,"mdns",edited_u_conf.mdns);
	}
	else {
		ESP_LOGI(TAG, "Leaving mdns: %s", user_config->mdns);
		cJSON_AddStringToObject(conf_json,"mdns",user_config->mdns);
	}
	if (config_changed.admin_pass == 1){
		ESP_LOGI(TAG, "Changing admin_pass: %s", edited_u_conf.admin_pass);
		cJSON_AddStringToObject(conf_json,"admin_pass",edited_u_conf.admin_pass);
	}
	else {
		ESP_LOGI(TAG, "Leaving admin_pass: %s", user_config->admin_pass);
		cJSON_AddStringToObject(conf_json,"admin_pass",user_config->admin_pass);
	}

	char *conf_to_save = cJSON_Print(conf_json);
	cJSON_Delete(conf_json);
	if (conf_to_save == NULL) {
	    ESP_LOGE(TAG, "Failed to allocate memory for config string");
	    fclose(f);
	    return false;
	}
	else {
		size_t conf_to_save_len = strlen(conf_to_save);
	    ESP_LOGI(TAG, "Saving config: %s", conf_to_save);
	    if (fwrite(conf_to_save, 1, conf_to_save_len, f) != conf_to_save_len) {
	        ESP_LOGE(TAG, "Failed to save new config file");
	        free(conf_to_save);
	        fclose(f);
	        return false;
	    }
	    else {
	        ESP_LOGI(TAG, "New config file was saved successfully");
	        free(conf_to_save);
	        fclose(f);
	        return true;
	    }
	}
}

//---------------------------------------------------------
// Načtení a uložení výchozího configu
//---------------------------------------------------------
_Bool load_default_config(void) {
	char *loaded_deaf_config;
    FILE *deaf_config_file = fopen(DEFAULT_CONFIG_PATH, "r");
    if (deaf_config_file == NULL) {
    	ESP_LOGE(TAG, "Error while opening file %s",DEFAULT_CONFIG_PATH);
    	return false;
    }
    fseek(deaf_config_file, 0, SEEK_END);
    size_t file_size = ftell(deaf_config_file);
    rewind(deaf_config_file);
    loaded_deaf_config = (char *)malloc(file_size+1);
    if (fread(loaded_deaf_config, file_size, 1, deaf_config_file) == 0)
    {
    	ESP_LOGE(TAG, "Error while reading file %s",DEFAULT_CONFIG_PATH);
    	return false;
    }
    fclose(deaf_config_file);

	FILE *config_file = fopen(CONFIG_PATH, "w");
    if (config_file == NULL) {
    	ESP_LOGE(TAG, "Error while opening file %s",CONFIG_PATH);
        return false;
    }
    if (fwrite(loaded_deaf_config, 1, file_size, config_file) != file_size) {
        ESP_LOGE(TAG, "Failed to set default config file");
        free(loaded_deaf_config);
        fclose(config_file);
        return false;
    }
    else {
        ESP_LOGI(TAG, "Default config file was set successfully");
        free(loaded_deaf_config);
        fclose(config_file);
        return true;
    }
}
