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

//	Dočasné funkce
void set_number_of_active_lights(int x) {
    printf("Number of active lights: %d\n", x);
}

void set_amount_of_drivers(int x, int y) {
    printf("Light index %d has %d drivers\n", x, y);
}

void set_if_the_light_is_inverted(int x, int y) {
    printf("Light index %d is %sinverted\n", x, y ? "" : "not ");
}

void set_address(int x, int y, int z) {
    printf("Light index %d, driver index %d, address: %d\n", x, y, z);
}

void make_mask(int x, int y, uint16_t z) {
    printf("Light index %d, driver index %d, mask: %d\n", x, y, z);
}

void set_TAIL(int x, int y, int z) {
    printf("Light index %d, TAIL, start: %d, length: %d\n", x, y, z);
}

void set_BREAK(int x, int y, int z) {
    printf("Light index %d, BREAK, start: %d, length: %d\n", x, y, z);
}

void set_TURN(int x, int y, int z) {
    printf("Light index %d, TURN, start: %d, length: %d\n", x, y, z);
}

void set_REVERSE(int x, int y, int z) {
    printf("Light index %d, REVERSE, start: %d, length: %d\n", x, y, z);
}

void set_REARFOG(int x, int y, int z) {
    printf("Light index %d, REARFOG, start: %d, length: %d\n", x, y, z);
}


static void ws_async_send(void *arg)
{
    async_resp_arg_t *resp_arg = arg;

    ESP_LOGI(TAG, "Json out: %s", resp_arg->ws_pkt_in.payload);

    size_t fds = CONFIG_LWIP_MAX_LISTENING_TCP; 		//Získání maximálního možného počtu naslouchajících TCP spojení

    int client_fds[fds];

    esp_err_t ret = httpd_get_client_list(server, &fds, client_fds);	//Získání maximálního možného počtu naslouchajících TCP spojení
    if (ret != ESP_OK) {
        return;
    }

    for (int i = 0; i < fds; i++) {
        int client_info = httpd_ws_get_fd_info(server, client_fds[i]); //Záskání inforamce o klientovi na základě file descriptoru
        if (client_info == HTTPD_WS_CLIENT_WEBSOCKET) {
            httpd_ws_send_frame_async(resp_arg->hd, client_fds[i], &resp_arg->ws_pkt_in); //Odeslání rámce přes WS
        }
    }
    free(resp_arg);
}

//Funkce pro vložení odesílací funkce do pracovní fronty
esp_err_t trigger_async_send(httpd_req_t *req, httpd_ws_frame_t ws_pkt)
{
	async_resp_arg_t *resp_arg = malloc(sizeof(async_resp_arg_t));
    resp_arg->hd = req->handle;
    resp_arg->fd = httpd_req_to_sockfd(req);
    resp_arg->ws_pkt_in = ws_pkt;
    return httpd_queue_work(req->handle, ws_async_send, resp_arg);
}

esp_err_t ws_send_json(httpd_req_t *req, cJSON *json_in)
{
	char *json_str = cJSON_Print(json_in);
	ESP_LOGI(TAG, "Json out: %s", json_str);
	cJSON_Delete(json_in);
	httpd_ws_frame_t ws_pkt;
	memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
	ws_pkt.payload = (uint8_t *)json_str;
	ws_pkt.len = strlen(json_str);
	ws_pkt.type = HTTPD_WS_TYPE_TEXT;
	return httpd_ws_send_frame_async(req->handle, httpd_req_to_sockfd(req), &ws_pkt);
}

//Funkce pro zpracování přijaté zprávy
esp_err_t handle_ws_req(httpd_req_t *req)
{
    if (req->method == HTTP_GET){
        ESP_LOGI(TAG, "Handshake done, the new connection was opened");
        return ESP_OK;
    }
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));

    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0); //Získání pouze délky WS rámce z req (max_len = 0 - zjistí pouze délku rámce)
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "httpd_ws_recv_frame failed to get frame len with %d", ret);
        return ret;
    }

    if (ws_pkt.len) {
    	ws_pkt.payload = calloc(1, ws_pkt.len + 1);
        if (ws_pkt.payload == NULL) {
            ESP_LOGE(TAG, "Failed to calloc memory for ws_pkt.payload");
            return ESP_ERR_NO_MEM;
        }

        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "httpd_ws_recv_frame failed with %d", ret);
            free(ws_pkt.payload);
            return ret;
        }
        ESP_LOGI(TAG, "Got packet with message: %s", ws_pkt.payload);
    }

    if (ws_pkt.type == HTTPD_WS_TYPE_TEXT) {
    	cJSON *json_root = cJSON_Parse((char *)ws_pkt.payload);

    	char *typew = cJSON_GetObjectItemCaseSensitive(json_root,"type")->valuestring;

    	if (strcmp(typew, "status_sync") == 0) {
    		cJSON *status, *anim;
			cJSON_AddStringToObject(status=cJSON_CreateObject(), "type", typew);
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
    		cJSON_Delete(json_root);
    		free(ws_pkt.payload);
    		return ws_send_json(req, status);
    	}

    	else if (strcmp(typew, "svalue") == 0) {
    		ESP_LOGI(TAG, "Type is %s", typew);
    		int idw = cJSON_GetObjectItemCaseSensitive(json_root,"id")->valueint;
    		ESP_LOGI(TAG, "ID is %d", idw);
    		int valuew = cJSON_GetObjectItemCaseSensitive(json_root,"value")->valueint;
    		ESP_LOGI(TAG, "Value is %d", valuew);
			animlight[idw-1].brightness = valuew;
			animlight[idw-1].speed = 0;
			animlight[idw-1].type = 0;
			cJSON_Delete(json_root);
			return trigger_async_send(req, ws_pkt);
    	}

    	else if (strcmp(typew, "animate") == 0) {
    		ESP_LOGI(TAG, "Type is %s", typew);
    		int idw = cJSON_GetObjectItemCaseSensitive(json_root,"id")->valueint;
    		ESP_LOGI(TAG, "ID is %d", idw);
    		cJSON *in_params = cJSON_GetObjectItemCaseSensitive(json_root,"value");
    		int brightness = cJSON_GetObjectItemCaseSensitive(in_params,"brightness")->valueint;
    		ESP_LOGI(TAG, "Brightness is %d", brightness);
    		int speed = cJSON_GetObjectItemCaseSensitive(in_params,"speed")->valueint;
			ESP_LOGI(TAG, "Speed is %d", speed);
			int type = cJSON_GetObjectItemCaseSensitive(in_params,"type")->valueint;
			ESP_LOGI(TAG, "Type of animation is %d", type);
			animlight[idw-1].brightness = brightness;
			animlight[idw-1].speed = speed;
			animlight[idw-1].type = type;

			cJSON_Delete(json_root);
			return trigger_async_send(req, ws_pkt);
    	}

		else if (strcmp(typew, "login") == 0) {
			ESP_LOGI(TAG, "Type is %s", typew);
			char *pass = cJSON_GetObjectItemCaseSensitive(json_root,"value")->valuestring;
			cJSON *return_json =cJSON_CreateObject();
			cJSON_AddStringToObject(return_json, "type", "login");
			if (strcmp(pass, user_config->admin_pass) == 0){
				cJSON_AddTrueToObject(return_json, "login_status");
				cJSON *currentconfig;
				cJSON_AddItemToObject(return_json, "currentconfig", currentconfig=cJSON_CreateObject());
				cJSON_AddStringToObject(currentconfig, "ssid", user_config->ssid);
				cJSON_AddStringToObject(currentconfig, "wifi_pass", user_config->wifi_pass);
				cJSON_AddNumberToObject(currentconfig, "maxcon", user_config->maxcon);
				cJSON_AddStringToObject(currentconfig, "mdns", user_config->mdns);
			}
			else {
				cJSON_AddFalseToObject(return_json, "login_status");
			}
			cJSON_Delete(json_root);
			free(ws_pkt.payload);
			return ws_send_json(req, return_json);
		}

		else if (strcmp(typew, "config") == 0) {
			ESP_LOGI(TAG, "Type is %s", typew);
			cJSON *in_config = cJSON_GetObjectItemCaseSensitive(json_root,"value");
			char *reqpass = cJSON_GetObjectItemCaseSensitive(in_config,"reqpass")->valuestring;
			cJSON *return_json = cJSON_CreateObject();
			cJSON_AddStringToObject(return_json, "type", "config_change_status");
			if (strcmp(reqpass, user_config->admin_pass) == 0){
				cJSON_AddTrueToObject(return_json, "login_status");
				user_config_t edited_u_conf;
				cJSON *in_ssid = cJSON_GetObjectItemCaseSensitive(in_config,"ssid");
			    if (cJSON_IsString(in_ssid) && (in_ssid->valuestring != NULL)){
			    	edited_u_conf.ssid = cJSON_GetObjectItemCaseSensitive(in_config,"ssid")->valuestring;
			    }
				cJSON *in_wifi_pass = cJSON_GetObjectItemCaseSensitive(in_config,"wifi_pass");
			    if (cJSON_IsString(in_wifi_pass) && (in_wifi_pass->valuestring != NULL)) {
			    	edited_u_conf.wifi_pass = cJSON_GetObjectItemCaseSensitive(in_config,"wifi_pass")->valuestring;
			    }
			    cJSON *in_maxcon = cJSON_GetObjectItemCaseSensitive(in_config,"maxcon");
				if (cJSON_IsNumber(in_maxcon) && (in_maxcon->valueint != 0)){
					edited_u_conf.maxcon = cJSON_GetObjectItemCaseSensitive(in_config,"maxcon")->valueint;
				}
				else {
					edited_u_conf.maxcon = 0;
				}
				cJSON *in_mdns = cJSON_GetObjectItemCaseSensitive(in_config,"mdns");
			    if (cJSON_IsString(in_mdns) && (in_mdns->valuestring != NULL)){
			    	edited_u_conf.mdns = cJSON_GetObjectItemCaseSensitive(in_config,"mdns")->valuestring;
			    }
			    cJSON *in_admin_pass = cJSON_GetObjectItemCaseSensitive(in_config,"admin_pass");
				if (cJSON_IsString(in_admin_pass) && (in_admin_pass->valuestring != NULL)) {
					edited_u_conf.admin_pass = cJSON_GetObjectItemCaseSensitive(in_config,"admin_pass")->valuestring;
				}
				cJSON_AddNumberToObject(return_json, "change_success", save_config(edited_u_conf));
			}
			else{
				cJSON_AddFalseToObject(return_json, "login_status");
			}
			free(ws_pkt.payload);
			cJSON_Delete(json_root);
			return ws_send_json(req, return_json);
		}
		else if (strcmp(typew, "restart") == 0) {
			ESP_LOGI(TAG, "Type is %s", typew);
			cJSON *in_value = cJSON_GetObjectItemCaseSensitive(json_root,"value");
			char *reqpass = cJSON_GetObjectItemCaseSensitive(in_value,"reqpass")->valuestring;
			if (strcmp(reqpass, user_config->admin_pass) == 0){
				ESP_LOGI(TAG, "Device will restart...");
				esp_restart();
			}
		}
		else if (strcmp(typew, "light_params") == 0) {
			ESP_LOGI(TAG, "Type is %s", typew);
			cJSON *in_params= cJSON_GetObjectItemCaseSensitive(json_root,"value");
			char *reqpass = cJSON_GetObjectItemCaseSensitive(in_params,"reqpass")->valuestring;
			cJSON *return_json = cJSON_CreateObject();
			cJSON_AddStringToObject(return_json, "type", "param_change_status");
			if (strcmp(reqpass, user_config->admin_pass) == 0){
				cJSON_AddTrueToObject(return_json, "login_status");
			    int num_lights = cJSON_GetObjectItemCaseSensitive(in_params, "numLights")->valueint;
			    set_number_of_active_lights(num_lights);

			    cJSON* lights = cJSON_GetObjectItemCaseSensitive(in_params, "lights");
			    if (lights == NULL) {
			        printf("Error: 'lights' array not found\n");
				    free(ws_pkt.payload);
					cJSON_Delete(json_root);
			        return ESP_FAIL;
			    }

			    for (int i = 0; i < cJSON_GetArraySize(lights); i++) {
			        cJSON* light = cJSON_GetArrayItem(lights, i);

			        cJSON *inverted = cJSON_GetObjectItemCaseSensitive(light, "inverted");
			        _Bool inverted_bool = cJSON_IsTrue(inverted);
			        set_if_the_light_is_inverted(i, inverted_bool);

			        int num_drivers = cJSON_GetObjectItemCaseSensitive(light, "numDrivers")->valueint;
			        set_amount_of_drivers(i, num_drivers);

			        cJSON* drivers = cJSON_GetObjectItemCaseSensitive(light, "drivers");
			        if (drivers != NULL) {
			            for (int j = 0; j < cJSON_GetArraySize(drivers); j++) {
			                cJSON* driver = cJSON_GetArrayItem(drivers, j);
			                int address = cJSON_GetObjectItemCaseSensitive(driver, "address")->valueint;
			                set_address(i, j, address);
			                const char* mask = cJSON_GetObjectItemCaseSensitive(driver, "mask")->valuestring;
			                char *endptr;
			                uint16_t mask_bin = strtol(mask, &endptr, 2);
			                make_mask(i, j, mask_bin);
			            }
			        }
			        cJSON* parts = cJSON_GetObjectItemCaseSensitive(light, "parts");
			        if (parts != NULL) {
			            for (int j = 0; j < cJSON_GetArraySize(parts); j++) {
			                cJSON* part = cJSON_GetArrayItem(parts, j);
			                const char* type = cJSON_GetObjectItemCaseSensitive(part, "type")->valuestring;
			                int start = cJSON_GetObjectItemCaseSensitive(part, "start")->valueint;
			                int length = cJSON_GetObjectItemCaseSensitive(part, "length")->valueint;
			                if (strcmp(type, "Tail") == 0) {
			                    set_TAIL(i, start, length);
			                }
			                else if (strcmp(type, "Brake") == 0) {
			                    set_BREAK(i, start, length);
			                }
			                else if (strcmp(type, "Turn") == 0) {
			                    set_TURN(i, start, length);
			                }
			                else if (strcmp(type, "Reverse") == 0) {
			                    set_REVERSE(i, start, length);
			                }
			                else if (strcmp(type, "Rear fog") == 0) {
			                    set_REARFOG(i, start, length);
			                }
			            }
			        }
			    }
			    cJSON_AddNumberToObject(return_json, "change_success", 1);
			}
			else{
				cJSON_AddFalseToObject(return_json, "login_status");
			}
			cJSON_Delete(json_root);
			free(ws_pkt.payload);
			return ws_send_json(req, return_json);
		}
    }
    return ESP_OK;
}
