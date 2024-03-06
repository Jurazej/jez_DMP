#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_http_server.h"
#include "cjson.h"
#include "esp_system.h"

#include "ws_handler.h"
#include "file_handler.h"
#include "wifi_web_handler.h"

static const char *TAG = "WS_handler";

animlight_t animlight[3];

static void ws_async_send(void *arg)
{
    httpd_ws_frame_t ws_pkt;	//Inicializace websocket rámce
    async_resp_arg_t *resp_arg = arg;

    ESP_LOGI(TAG, "Json out: %s", resp_arg->json_in);


    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = (uint8_t *)resp_arg->json_in;	//Nastavení atributu přenášené zprávy na data
    ws_pkt.len = strlen(resp_arg->json_in);					//Nastavení atributu délky podle délky bufferu
    ws_pkt.type = HTTPD_WS_TYPE_TEXT; 			//Nastavení atributu type rámce na text

    size_t fds = CONFIG_LWIP_MAX_LISTENING_TCP; //Získání maximálního možného počtu naslouchajících TCP spojení

    int client_fds[fds];

    esp_err_t ret = httpd_get_client_list(server, &fds, client_fds);	//Získání maximálního možného počtu naslouchajících TCP spojení
    if (ret != ESP_OK) {
        return;
    }

    for (int i = 0; i < fds; i++) {
        int client_info = httpd_ws_get_fd_info(server, client_fds[i]); //Záskání inforamce o klientovi na základě file descriptoru
        if (client_info == HTTPD_WS_CLIENT_WEBSOCKET) {
            httpd_ws_send_frame_async(resp_arg->hd, client_fds[i], &ws_pkt); //Odeslání rámce přes WS
        }
    }
    free(resp_arg);
}

//Funkce pro vložení odesílací funkce do pracovní fronty
esp_err_t trigger_async_send(httpd_req_t *req, char *json_in)
{
	async_resp_arg_t *resp_arg = malloc(sizeof(async_resp_arg_t));
    resp_arg->hd = req->handle;
    resp_arg->fd = httpd_req_to_sockfd(req);
    resp_arg->json_in = json_in;
    return httpd_queue_work(req->handle, ws_async_send, resp_arg);
}

//Funkce pro zpracování přijaté zprávy
esp_err_t handle_ws_req(httpd_req_t *req)
{
    if (req->method == HTTP_GET)
    {
        ESP_LOGI(TAG, "Handshake done, the new connection was opened");
        return ESP_OK;
    }
    httpd_ws_frame_t ws_pkt;
    uint8_t *buf = NULL;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));

    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0); //Získání pouze délky WS rámce z req (max_len = 0 - zjistí pouze délku rámce)
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "httpd_ws_recv_frame failed to get frame len with %d", ret);
        return ret;
    }

    if (ws_pkt.len)
    {
        buf = calloc(1, ws_pkt.len + 1);
        if (buf == NULL)
        {
            ESP_LOGE(TAG, "Failed to calloc memory for buf");
            return ESP_ERR_NO_MEM;
        }

        ws_pkt.payload = buf;

        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "httpd_ws_recv_frame failed with %d", ret);
            free(buf);
            return ret;
        }
        ESP_LOGI(TAG, "Got packet with message: %s", ws_pkt.payload);
    }

    if (ws_pkt.type == HTTPD_WS_TYPE_TEXT){
    	cJSON *root = cJSON_Parse((char *)ws_pkt.payload);
    	char *json_in = cJSON_Print(root);

    	char *typew = cJSON_GetObjectItem(root,"type")->valuestring;

    	if (strcmp(typew, "status_sync") == 0){
    		cJSON *status, *anim;
			status = cJSON_CreateObject();
			cJSON_AddStringToObject(status, "type", typew);
    		cJSON_AddItemToObject(status, "value", anim=cJSON_CreateObject());
    		int anim_segs = (sizeof(animlight) / sizeof(animlight[0]));
    		cJSON *animn[anim_segs];
    		char i_string[3];
    		for (int i = 0; i < anim_segs; i++) {
    			sprintf(i_string, "%d", i+1);
    			cJSON_AddItemToObject(anim, i_string, animn[i]=cJSON_CreateObject());
    			cJSON_AddNumberToObject(animn[i], "type", animlight[i].type);
    			cJSON_AddNumberToObject(animn[i], "speed", animlight[i].speed);
    			cJSON_AddNumberToObject(animn[i], "brightness", animlight[i].brightness);
    		}

			char *statusjsonout = cJSON_Print(status);
			ESP_LOGI(TAG, "Json out: %s", statusjsonout);
			cJSON_Delete(status);
			httpd_ws_frame_t ws_pkt;
			memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
			ws_pkt.payload = (uint8_t *)statusjsonout;
			ws_pkt.len = strlen(statusjsonout);
			ws_pkt.type = HTTPD_WS_TYPE_TEXT;
			httpd_ws_send_frame_async(req->handle, httpd_req_to_sockfd(req), &ws_pkt);
    	}

    	else if (strcmp(typew, "svalue") == 0){
    		ESP_LOGI(TAG, "Type is %s", typew);
    		int idw = cJSON_GetObjectItem(root,"id")->valueint;
    		ESP_LOGI(TAG, "ID is %d", idw);
    		int valuew = cJSON_GetObjectItem(root,"value")->valueint;
    		ESP_LOGI(TAG, "Value is %d", valuew);
			animlight[idw-1].brightness = valuew;
			animlight[idw-1].speed = 0;
			animlight[idw-1].type = 0;
			trigger_async_send(req, json_in); //Odeslání dat na web
    	}

    	else if (strcmp(typew, "animate") == 0){
    		ESP_LOGI(TAG, "Type is %s", typew);
    		int idw = cJSON_GetObjectItem(root,"id")->valueint;
    		ESP_LOGI(TAG, "ID is %d", idw);
    		cJSON *in_params = cJSON_GetObjectItem(root,"value");
    		int brightness = cJSON_GetObjectItem(in_params,"brightness")->valueint;
    		ESP_LOGI(TAG, "Brightness is %d", brightness);
    		int speed = cJSON_GetObjectItem(in_params,"speed")->valueint;
			ESP_LOGI(TAG, "Speed is %d", speed);
			int type = cJSON_GetObjectItem(in_params,"type")->valueint;
			ESP_LOGI(TAG, "Type of animation is %d", type);
			animlight[idw-1].brightness = brightness;
			animlight[idw-1].speed = speed;
			animlight[idw-1].type = type;

			trigger_async_send(req, json_in); //Odeslání dat na web
    	}

		else if (strcmp(typew, "login") == 0){
			ESP_LOGI(TAG, "Type is %s", typew);
			char *pass = cJSON_GetObjectItem(root,"value")->valuestring;
			_Bool loginval = 0;
			if (strcmp(pass, user_config->admin_pass) == 0){
				loginval = 1;
			}
			cJSON *loginjson, *currentconfig;
			loginjson=cJSON_CreateObject();
			cJSON_AddStringToObject(loginjson, "type", "login");
			cJSON_AddNumberToObject(loginjson, "value", loginval);
			if (loginval == 1) {
				cJSON_AddItemToObject(loginjson, "currentconfig", currentconfig=cJSON_CreateObject());
				cJSON_AddStringToObject(currentconfig, "ssid", user_config->ssid);
				cJSON_AddStringToObject(currentconfig, "wifi_pass", user_config->wifi_pass);
				cJSON_AddNumberToObject(currentconfig, "maxcon", user_config->maxcon);
				cJSON_AddStringToObject(currentconfig, "mdns", user_config->mdns);
			}
			free(buf);
			char *loginjsonout = cJSON_Print(loginjson);
			ESP_LOGI(TAG, "Json out: %s", loginjsonout);
			cJSON_Delete(loginjson);
			httpd_ws_frame_t ws_pkt;
			memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
			ws_pkt.payload = (uint8_t *)loginjsonout;	//Nastavení atributu přenášené zprávy na data
			//ws_pkt.payload = (uint8_t *)data;			//Nastavení atributu přenášené zprávy na data
			ws_pkt.len = strlen(loginjsonout);					//Nastavení atributu délky podle délky bufferu
			ws_pkt.type = HTTPD_WS_TYPE_TEXT;
			httpd_ws_send_frame_async(req->handle, httpd_req_to_sockfd(req), &ws_pkt);
		}
		else if (strcmp(typew, "config") == 0){

			ESP_LOGI(TAG, "Type is %s", typew);
			cJSON *in_config = cJSON_GetObjectItem(root,"value");
			char *reqpass = cJSON_GetObjectItem(in_config,"reqpass")->valuestring;
			int loginval = 0;
			if (strcmp(reqpass, user_config->admin_pass) == 0){
				loginval = 1;
			}
			cJSON *loginjson;
			loginjson=cJSON_CreateObject();
			cJSON_AddStringToObject(loginjson, "type", "config_change_status");
			cJSON_AddNumberToObject(loginjson, "value", loginval);
			if (loginval == 1) {
				user_config_t edited_u_conf;
				cJSON *in_ssid = cJSON_GetObjectItemCaseSensitive(in_config,"ssid");
			    if (cJSON_IsString(in_ssid) && (in_ssid->valuestring != NULL)) {
			    	edited_u_conf.ssid = cJSON_GetObjectItem(in_config,"ssid")->valuestring;
			    }
				cJSON *in_wifi_pass = cJSON_GetObjectItemCaseSensitive(in_config,"wifi_pass");
			    if (cJSON_IsString(in_wifi_pass) && (in_wifi_pass->valuestring != NULL)) {
			    	edited_u_conf.wifi_pass = cJSON_GetObjectItem(in_config,"wifi_pass")->valuestring;
			    }
			    cJSON *in_maxcon = cJSON_GetObjectItemCaseSensitive(in_config,"maxcon");
				if (cJSON_IsNumber(in_maxcon) && (in_maxcon->valueint != 0)) {
					edited_u_conf.maxcon = cJSON_GetObjectItem(in_config,"maxcon")->valueint;
				}
				else {
					edited_u_conf.maxcon = 0;
				}
				cJSON *in_mdns = cJSON_GetObjectItemCaseSensitive(in_config,"mdns");
			    if (cJSON_IsString(in_mdns) && (in_mdns->valuestring != NULL)) {
			    	edited_u_conf.mdns = cJSON_GetObjectItem(in_config,"mdns")->valuestring;
			    }
			    cJSON *in_admin_pass = cJSON_GetObjectItemCaseSensitive(in_config,"admin_pass");
				if (cJSON_IsString(in_admin_pass) && (in_admin_pass->valuestring != NULL)) {
					edited_u_conf.admin_pass = cJSON_GetObjectItem(in_config,"admin_pass")->valuestring;
				}
				int change_success = save_config(edited_u_conf);
				cJSON_AddNumberToObject(loginjson, "change_success", change_success);
			}
			free(buf);
			char *loginjsonout = cJSON_Print(loginjson);
			ESP_LOGI(TAG, "Json out: %s", loginjsonout);
			cJSON_Delete(loginjson);
			httpd_ws_frame_t ws_pkt;
			memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
			ws_pkt.payload = (uint8_t *)loginjsonout;
			ws_pkt.len = strlen(loginjsonout);
			ws_pkt.type = HTTPD_WS_TYPE_TEXT;
			httpd_ws_send_frame_async(req->handle, httpd_req_to_sockfd(req), &ws_pkt);
		}
		else if (strcmp(typew, "restart") == 0){

			ESP_LOGI(TAG, "Type is %s", typew);
			cJSON *in_value = cJSON_GetObjectItem(root,"value");
			char *reqpass = cJSON_GetObjectItem(in_value,"reqpass")->valuestring;
			if (strcmp(reqpass, user_config->admin_pass) == 0){
				ESP_LOGI(TAG, "Device will restart...");
				esp_restart();
			}
		}
    	free(buf);
    	cJSON_Delete(root);
    }
    return ESP_OK;
}
