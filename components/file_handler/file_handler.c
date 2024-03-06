#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_spiffs.h"
#include "cjson.h"

#include "file_handler.h"

static const char *TAG = "File_handler";

files_t *loaded_files;
user_config_t *user_config;

void init_spiffs(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 8,
        .format_if_mount_failed = true};

    ESP_ERROR_CHECK(esp_vfs_spiffs_register(&conf));	//Registrace SPIFFS pomocí nastavené konfigurace
}

void load_config(void)
{
    char *loaded_json_config;

    FILE *config_file = fopen(CONFIG_PATH, "r");
    if (config_file == NULL) {
    	ESP_LOGE(TAG, "Chyba při otevírání souboru %s",CONFIG_PATH);
    	return;
    }
    fseek(config_file, 0, SEEK_END);
    size_t file_size = ftell(config_file);
    rewind(config_file);
    loaded_json_config = (char *)malloc(file_size+1);
    if (fread(loaded_json_config, file_size, 1, config_file) == 0)
    {
    	ESP_LOGE(TAG, "Chyba při čtení souboru %s",CONFIG_PATH);
    	return;
    }
    loaded_json_config[file_size] = '\0';
    fclose(config_file);

    ESP_LOGI(TAG, "Loaded config: %s", loaded_json_config);
    user_config = malloc(sizeof(user_config_t));
	cJSON *root = cJSON_Parse(loaded_json_config);

	cJSON *item;
	item = cJSON_GetObjectItem(root, "ssid");
	if (item != NULL && cJSON_IsString(item)) {
	    size_t ssid_length = strlen(item->valuestring);
	    user_config->ssid = malloc(ssid_length + 1);
	    if (user_config->ssid != NULL) {
	        strcpy(user_config->ssid, item->valuestring);
	    }
	    else {
	    	ESP_LOGE(TAG, "Chyba při alokaci paměti pro config - ssid");
	    	return;
	    }
	}
	else {
		ESP_LOGE(TAG, "Chyba, neplatná hodnota configu pro ssid");
		return;
	}

	item = cJSON_GetObjectItem(root, "wifi_pass");
	if (item != NULL && cJSON_IsString(item)) {
	    size_t ssid_length = strlen(item->valuestring);
	    user_config->wifi_pass = malloc(ssid_length + 1);
	    if (user_config->wifi_pass != NULL) {
	        strcpy(user_config->wifi_pass, item->valuestring);
	    }
	    else {
	    	ESP_LOGE(TAG, "Chyba při alokaci paměti pro config - wifi_pass");
	    	return;
	    }
	}
	else {
		ESP_LOGE(TAG, "Chyba, neplatná hodnota configu pro wifi_pass");
		return;
	}

	user_config->maxcon = cJSON_GetObjectItem(root,"maxcon")->valueint;

	item = cJSON_GetObjectItem(root, "mdns");
	if (item != NULL && cJSON_IsString(item)) {
	    size_t ssid_length = strlen(item->valuestring);
	    user_config->mdns = malloc(ssid_length + 1);
	    if (user_config->mdns != NULL) {
	        strcpy(user_config->mdns, item->valuestring);
	    }
	    else {
	    	ESP_LOGE(TAG, "Chyba při alokaci paměti pro config - mdns");
	    	return;
	    }
	}
	else {
		ESP_LOGE(TAG, "Chyba, neplatná hodnota configu pro mdns");
		return;
	}

	item = cJSON_GetObjectItem(root, "admin_pass");
	if (item != NULL && cJSON_IsString(item)) {
	    size_t ssid_length = strlen(item->valuestring);
	    user_config->admin_pass = malloc(ssid_length + 1);
	    if (user_config->admin_pass != NULL) {
	        strcpy(user_config->admin_pass, item->valuestring);
	    }
	    else {
	    	ESP_LOGE(TAG, "Chyba při alokaci paměti pro config - admin_pass");
	    	return;
	    }
	}
	else {
		ESP_LOGE(TAG, "Chyba, neplatná hodnota configu pro admin_pass");
		return;
	}

    free(loaded_json_config);
    cJSON_Delete(root);
}

void load_web(void)
{
	loaded_files = malloc(sizeof(files_t));
    // Načtení index.html z paměti
    FILE *fp = fopen(INDEX_PATH, "r");
    if (fp == NULL) {
    	ESP_LOGE(TAG, "Chyba při otevírání souboru %s",INDEX_PATH);
    	return;
    }
    fseek(fp, 0, SEEK_END);
    size_t file_size = ftell(fp);
    rewind(fp);
    loaded_files->index_html = (char *)malloc(file_size+1);
    if (fread(loaded_files->index_html, file_size, 1, fp) == 0)
    {
    	ESP_LOGE(TAG, "Chyba při čtení souboru %s",INDEX_PATH);
    	fclose(fp);
    	return;
    }
    loaded_files->index_html[file_size] = '\0';
    fclose(fp);

    // Načtení logo.svg z paměti
    fp = fopen(LOGO_PATH, "r");
    if (fp == NULL) {
    	ESP_LOGE(TAG, "Chyba při otevírání souboru %s",LOGO_PATH);
    	return;
    }
    fseek(fp, 0, SEEK_END);
    file_size = ftell(fp);
    rewind(fp);
    loaded_files->logo_svg = (char *)malloc(file_size+1);
    if (fread(loaded_files->logo_svg, file_size, 1, fp) == 0)
    {
    	ESP_LOGE(TAG, "Chyba při čtení souboru %s",LOGO_PATH);
    	fclose(fp);
    	return;
    }
    loaded_files->logo_svg[file_size] = '\0';
    fclose(fp);

    // Načtení admin.html z paměti
    fp = fopen(ADMIN_PATH, "r");
    if (fp == NULL) {
    	ESP_LOGE(TAG, "Chyba při otevírání souboru %s",ADMIN_PATH);
    	return;
    }
    fseek(fp, 0, SEEK_END);
    file_size = ftell(fp);
    rewind(fp);
    loaded_files->admin_html = (char *)malloc(file_size+1);
    if (fread(loaded_files->admin_html, file_size, 1, fp) == 0)
    {
    	ESP_LOGE(TAG, "Chyba při čtení souboru %s",ADMIN_PATH);
    	fclose(fp);
    	return;
    }
    loaded_files->admin_html[file_size] = '\0';
    fclose(fp);
}

int save_config(user_config_t edited_u_conf){
	FILE *f = fopen(CONFIG_PATH, "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return 0;
    }
    cJSON *conf_json = cJSON_CreateObject();
	if (edited_u_conf.ssid != NULL){
		ESP_LOGI(TAG, "Changing ssid: %s", edited_u_conf.ssid);
		cJSON_AddStringToObject(conf_json,"ssid",edited_u_conf.ssid);
	}
	else {
		cJSON_AddStringToObject(conf_json,"ssid",user_config->ssid);
	}
	if (edited_u_conf.wifi_pass != NULL){
		ESP_LOGI(TAG, "Changing wifi_pass: %s", edited_u_conf.wifi_pass);
		cJSON_AddStringToObject(conf_json,"wifi_pass",edited_u_conf.wifi_pass);
	}
	else {
		cJSON_AddStringToObject(conf_json,"wifi_pass",user_config->wifi_pass);
	}
	if (edited_u_conf.maxcon != 0){
		ESP_LOGI(TAG, "Changing maxcon: %d", edited_u_conf.maxcon);
		cJSON_AddNumberToObject(conf_json,"maxcon",edited_u_conf.maxcon);
	}
	else {
		cJSON_AddNumberToObject(conf_json,"maxcon",user_config->maxcon);
	}
	if (edited_u_conf.mdns != NULL){
		ESP_LOGI(TAG, "Changing mdns: %s", edited_u_conf.mdns);
		cJSON_AddStringToObject(conf_json,"mdns",edited_u_conf.mdns);
	}
	else {
		cJSON_AddStringToObject(conf_json,"mdns",user_config->mdns);
	}
	if (edited_u_conf.admin_pass != NULL){
		ESP_LOGI(TAG, "Changing admin_pass: %s", edited_u_conf.admin_pass);
		cJSON_AddStringToObject(conf_json,"admin_pass",edited_u_conf.admin_pass);
	}
	else {
		cJSON_AddStringToObject(conf_json,"admin_pass",user_config->admin_pass);
	}

	char *conf_to_save = cJSON_Print(conf_json);
	ESP_LOGI(TAG, "Saving config: %s", conf_to_save);
    if (fwrite(conf_to_save, 1, strlen(conf_to_save), f) != strlen(conf_to_save)) {
        ESP_LOGE(TAG, "Failed to save new config file");
        fclose(f);
        return 0;
    } else {
        ESP_LOGI(TAG, "New config file was saved successfully");
        fclose(f);
        return 1;
    }
}
