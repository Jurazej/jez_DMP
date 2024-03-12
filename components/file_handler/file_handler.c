#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "esp_log.h"
#include "esp_spiffs.h"
#include "cjson.h"

#include "file_handler.h"

static const char *TAG = "File_handler";

files_t *loaded_files;
user_config_t *user_config;

void init_spiffs(void) {
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 8,
        .format_if_mount_failed = true};

    ESP_ERROR_CHECK(esp_vfs_spiffs_register(&conf));	//Registrace SPIFFS pomocí nastavené konfigurace
}

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
    return true;
}

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
	ESP_LOGI(TAG, "New config: %s", conf_to_save);
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
