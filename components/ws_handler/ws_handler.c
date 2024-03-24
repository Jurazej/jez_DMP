#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_system.h"

#include "keep_alive.h"
#include "ws_handler.h"
#include "file_handler.h"
#include "wifi_web_handler.h"

static const char *TAG = "WS_handler";

//---------------------------------------------------------
// Dočasné funkce určené k testování
//---------------------------------------------------------
uint8_t get_number_of_lights() {
	return 2;
}

void set_segment(uint8_t light,uint8_t index_obj,_Bool state) {
	printf("Segment set: %u %u %d\n", light, index_obj, state);
}

uint8_t get_number_of_segs(uint8_t light) {
	return 4;
}

_Bool test_light(uint8_t light, _Bool segs[]) {
	return true;
}

void set_number_of_active_lights(int x) {
    printf("Number of active lights: %d\n", x);
}

void set_amount_of_drivers(int x, int y) {
    printf("Light index %d has %d drivers\n", x, y);
}

void set_if_the_driver_is_inverted(int x, int y, int z) {
    printf("Light index %d , driver %d is %sinverted\n", x, y, z ? "" : "not ");
}

void set_address(int x, int y, int z) {
    printf("Light index %d, driver index %d, address: %d\n", x, y, z);
}

void make_mask(int x, int y, uint16_t z) {
    printf("Light index %d, driver index %d, mask: %d\n", x, y, z);
}

void set_part(int x, int p, int y, int z) {
    printf("Light index %d, part %d, start: %d, length: %d\n", x, p, y, z);
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

//---------------------------------------------------------
// Kontrola JSON objektu
//---------------------------------------------------------
_Bool check_json_object(cJSON *obj, int type) {
    if (obj == NULL) {
        return true;
    }
    else {
    	if (type!=obj->type) {
    		return true;
    	}
    	else {
    		return false;
    	}
    }
}

//---------------------------------------------------------
// Odeslání WS odpovědi  všem připojeným klientům
//---------------------------------------------------------
static void ws_async_send(void *arg) {
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

//---------------------------------------------------------
// Vložení odesílací funkce do pracovní fronty
//---------------------------------------------------------
esp_err_t trigger_async_send(httpd_req_t *req, httpd_ws_frame_t ws_pkt) {
	async_resp_arg_t *resp_arg = malloc(sizeof(async_resp_arg_t));
    resp_arg->hd = req->handle;
    resp_arg->fd = httpd_req_to_sockfd(req);
    resp_arg->ws_pkt_in = ws_pkt;
    return httpd_queue_work(req->handle, ws_async_send, resp_arg);
}

//---------------------------------------------------------
// Odeslání JSON objektu určutému klientovi přes WS
//---------------------------------------------------------
esp_err_t ws_send_json(httpd_req_t *req, cJSON *json_in) {
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

//---------------------------------------------------------
// Zpracování a vyhodnocení přijaté WS zprávy
//---------------------------------------------------------
esp_err_t handle_ws_req(httpd_req_t *req) {
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
    	cJSON *in_type_obj = cJSON_GetObjectItemCaseSensitive(json_root,"type");
    	if (check_json_object(in_type_obj,cJSON_String)) {
    		FREE_AND_RETURN_FAIL
    	}
    	char *in_type = in_type_obj->valuestring;

    	// Podle položky "type" v přijatém JSONu se provede určitá operace
    	if (strcmp(in_type, "status_sync") == 0) {
    		cJSON *status;
			cJSON_AddStringToObject(status=cJSON_CreateObject(), "type", in_type);
    		cJSON_AddItemToObject(status, "params", cJSON_Duplicate(lights_.json,true));
    		cJSON_AddNumberToObject(status, "nol", lights_.nol);
    		cJSON* light_array = cJSON_AddArrayToObject(status, "lights");
    	    for (int i = 0; i < lights_.nol; i++) {
    	        cJSON* light_obj = cJSON_CreateObject();
    	        cJSON_AddNumberToObject(light_obj, "nop", lights_.light[i].nop);
    	        cJSON* part_array = cJSON_AddArrayToObject(light_obj, "parts");
    	        for (int j = 0; j < lights_.light[i].nop; j++) {
    	        	cJSON* anim_array = cJSON_AddArrayToObject(part_array, "anim");
    	            cJSON_AddItemToArray(anim_array, cJSON_CreateNumber(lights_.light[i].anim[j].type));
    	            cJSON_AddItemToArray(anim_array, cJSON_CreateNumber(lights_.light[i].anim[j].brightness));
    	            cJSON_AddItemToArray(anim_array, cJSON_CreateNumber(lights_.light[i].anim[j].speed));
    	        }
    	        cJSON_AddItemToArray(light_array, light_obj);
    	    }
    		cJSON_Delete(json_root);
    		free(ws_pkt.payload);
    		return ws_send_json(req, status);
    	}

    	else if (strcmp(in_type, "animate") == 0) {
        	cJSON *in_id_obj = cJSON_GetObjectItemCaseSensitive(json_root,"id");
        	if (check_json_object(in_id_obj,cJSON_Number)) {
        		FREE_AND_RETURN_FAIL
        	}
        	uint8_t in_id = in_id_obj->valueint;

    		cJSON *in_anim= cJSON_GetObjectItemCaseSensitive(json_root,"value");
        	if (check_json_object(in_anim,cJSON_Object)) {
        		FREE_AND_RETURN_FAIL
        	}
        	cJSON *brightness_obj = cJSON_GetObjectItemCaseSensitive(in_anim,"brightness");
        	if (check_json_object(brightness_obj,cJSON_Number)) {
        		FREE_AND_RETURN_FAIL
        	}
        	cJSON *speed_obj = cJSON_GetObjectItemCaseSensitive(in_anim,"speed");
        	if (check_json_object(speed_obj,cJSON_Number)) {
        		FREE_AND_RETURN_FAIL
        	}
        	cJSON *type_obj = cJSON_GetObjectItemCaseSensitive(in_anim,"type");
        	if (check_json_object(type_obj,cJSON_Number)) {
        		FREE_AND_RETURN_FAIL
        	}
        	cJSON *part_obj = cJSON_GetObjectItemCaseSensitive(in_anim,"part");
        	if (check_json_object(part_obj,cJSON_Number)) {
        		FREE_AND_RETURN_FAIL
        	}
        	uint8_t part_id = part_obj->valueint;

			lights_.light[in_id].anim[part_id].brightness = brightness_obj->valueint;
			lights_.light[in_id].anim[part_id].speed = speed_obj->valueint;
			lights_.light[in_id].anim[part_id].type = type_obj->valueint;

			cJSON_Delete(json_root);
			return trigger_async_send(req, ws_pkt);
    	}
    }
    return ESP_OK;
}

//---------------------------------------------------------
// Zpracování a vyhodnocení přijaté WSS zprávy
//---------------------------------------------------------
esp_err_t handle_wss_req(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "Handshake done, the new connection was opened");
        return ESP_OK;
    }
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));

    // First receive the full ws message
    /* Set max_len = 0 to get the frame len */
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "httpd_ws_recv_frame failed to get frame len with %d", ret);
        return ret;
    }
    ESP_LOGI(TAG, "frame len is %d", ws_pkt.len);
    if (ws_pkt.len) {
        /* ws_pkt.len + 1 is for NULL termination as we are expecting a string */
    	ws_pkt.payload = calloc(1, ws_pkt.len + 1);
        if (ws_pkt.payload == NULL) {
            ESP_LOGE(TAG, "Failed to calloc memory for buf");
            return ESP_ERR_NO_MEM;
        }
        /* Set max_len = ws_pkt.len to get the frame payload */
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "httpd_ws_recv_frame failed with %d", ret);
            free(ws_pkt.payload);
            return ret;
        }
    }
    // If it was a PONG, update the keep-alive
    if (ws_pkt.type == HTTPD_WS_TYPE_PONG) {
        ESP_LOGD(TAG, "Received PONG message");
        free(ws_pkt.payload);
        return wss_keep_alive_client_is_active(httpd_get_global_user_ctx(req->handle),
                httpd_req_to_sockfd(req));
    }
    else if (ws_pkt.type == HTTPD_WS_TYPE_PING) {
        ESP_LOGI(TAG, "Got a WS PING frame, Replying PONG");
        ws_pkt.type = HTTPD_WS_TYPE_PONG;
        ret = httpd_ws_send_frame(req, &ws_pkt);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "httpd_ws_send_frame failed with %d", ret);
        }
        free(ws_pkt.payload);
        return ret;
	}
	else if (ws_pkt.type == HTTPD_WS_TYPE_CLOSE) {
		// Response CLOSE packet with no payload to peer
		ws_pkt.len = 0;
		ws_pkt.payload = NULL;
        ret = httpd_ws_send_frame(req, &ws_pkt);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "httpd_ws_send_frame failed with %d", ret);
        }
        free(ws_pkt.payload);
        return ret;
	}
	else if (ws_pkt.type == HTTPD_WS_TYPE_TEXT) {
		ESP_LOGI(TAG, "Received packet with message: %s", ws_pkt.payload);
		cJSON *json_root = cJSON_Parse((char *)ws_pkt.payload);

		cJSON *in_type_obj = cJSON_GetObjectItemCaseSensitive(json_root,"type");
		if (check_json_object(in_type_obj,cJSON_String)) {
			FREE_AND_RETURN_FAIL
		}
		char *in_type = in_type_obj->valuestring;
		if (strcmp(in_type, "login") == 0) {
			cJSON *pass_obj = cJSON_GetObjectItemCaseSensitive(json_root,"value");
			if (check_json_object(pass_obj,cJSON_String)) {
				FREE_AND_RETURN_FAIL
			}

			cJSON *return_json =cJSON_CreateObject();
			cJSON_AddStringToObject(return_json, "type", "login");
			if (strcmp(pass_obj->valuestring, user_config->admin_pass) == 0){
				cJSON_AddTrueToObject(return_json, "login_status");
				cJSON *currentconfig, *debugdata, *segments;
				cJSON_AddItemToObject(return_json, "currentconfig", currentconfig=cJSON_CreateObject());
				cJSON_AddItemToObject(return_json, "debugdata", debugdata=cJSON_CreateObject());
				cJSON_AddStringToObject(currentconfig, "ssid", user_config->ssid);
				cJSON_AddStringToObject(currentconfig, "wifi_pass", user_config->wifi_pass);
				cJSON_AddNumberToObject(currentconfig, "maxcon", user_config->maxcon);
				cJSON_AddStringToObject(currentconfig, "mdns", user_config->mdns);
				uint8_t nol = get_number_of_lights();
				cJSON_AddNumberToObject(debugdata, "nol", nol);
				cJSON_AddItemToObject(debugdata, "seg", segments=cJSON_CreateObject());
				char index_string[5];
				for (int i = 0; i < nol; i++) {
					sprintf(index_string, "%d", i+1);
					cJSON_AddNumberToObject(segments, index_string , get_number_of_segs(nol));
				}
			}
			else {
				cJSON_AddFalseToObject(return_json, "login_status");
			}
			cJSON_Delete(json_root);
			free(ws_pkt.payload);
			return ws_send_json(req, return_json);
		}

		else if (strcmp(in_type, "config") == 0) {
			ESP_LOGI(TAG, "Type is %s", in_type);
			cJSON *in_config = cJSON_GetObjectItemCaseSensitive(json_root,"value");
			if (check_json_object(in_config,cJSON_Object)) {
				FREE_AND_RETURN_FAIL
			}

			cJSON *pass_obj = cJSON_GetObjectItemCaseSensitive(in_config,"reqpass");
			if (check_json_object(pass_obj,cJSON_String)) {
				FREE_AND_RETURN_FAIL
			}

			cJSON *return_json = cJSON_CreateObject();
			cJSON_AddStringToObject(return_json, "type", "config_change_status");
			if (strcmp(pass_obj->valuestring, user_config->admin_pass) == 0){
				cJSON_AddTrueToObject(return_json, "login_status");
				user_config_t edited_u_conf;
				change_of_config_t config_changed;
				cJSON *in_ssid = cJSON_GetObjectItemCaseSensitive(in_config,"ssid");
				if (cJSON_IsString(in_ssid) && (in_ssid->valuestring != NULL)){
					edited_u_conf.ssid = in_ssid->valuestring;
					config_changed.ssid = true;
				}
				else {
					config_changed.ssid = false;
				}
				cJSON *in_wifi_pass = cJSON_GetObjectItemCaseSensitive(in_config,"wifi_pass");
				if (cJSON_IsString(in_wifi_pass) && (in_wifi_pass->valuestring != NULL)) {
					edited_u_conf.wifi_pass = in_wifi_pass->valuestring;
					config_changed.wifi_pass = true;
				}
				else {
					config_changed.wifi_pass = false;
				}
				cJSON *in_maxcon = cJSON_GetObjectItemCaseSensitive(in_config,"maxcon");
				if (cJSON_IsNumber(in_maxcon) && (in_maxcon->valueint != 0)){
					edited_u_conf.maxcon = in_maxcon->valueint;
					config_changed.maxcon = true;
				}
				else {
					config_changed.maxcon = false;
				}
				cJSON *in_mdns = cJSON_GetObjectItemCaseSensitive(in_config,"mdns");
				if (cJSON_IsString(in_mdns) && (in_mdns->valuestring != NULL)){
					edited_u_conf.mdns = in_mdns->valuestring;
					config_changed.mdns = true;
				}
				else {
					config_changed.mdns = false;
				}
				cJSON *in_admin_pass = cJSON_GetObjectItemCaseSensitive(in_config,"admin_pass");
				if (cJSON_IsString(in_admin_pass) && (in_admin_pass->valuestring != NULL)) {
					edited_u_conf.admin_pass = in_admin_pass->valuestring;
					config_changed.admin_pass = true;
				}
				else {
					config_changed.admin_pass = false;
				}
				cJSON_AddNumberToObject(return_json, "change_success", save_config(edited_u_conf, config_changed));
			}
			else{
				cJSON_AddFalseToObject(return_json, "login_status");
			}
			free(ws_pkt.payload);
			cJSON_Delete(json_root);
			return ws_send_json(req, return_json);
		}

		else if (strcmp(in_type, "restart") == 0) {
			cJSON *in_value = cJSON_GetObjectItemCaseSensitive(json_root,"value");
			if (check_json_object(in_value,cJSON_Object)) {
				FREE_AND_RETURN_FAIL
			}
			cJSON *pass_obj = cJSON_GetObjectItemCaseSensitive(in_value,"reqpass");
			if (check_json_object(pass_obj,cJSON_String)) {
				FREE_AND_RETURN_FAIL
			}

			if (strcmp(pass_obj->valuestring, user_config->admin_pass) == 0){
				ESP_LOGI(TAG, "Device will restart...");
				esp_restart();
			}
		}

		else if (strcmp(in_type, "default_config") == 0) {
			cJSON *in_value = cJSON_GetObjectItemCaseSensitive(json_root,"value");
			if (check_json_object(in_value,cJSON_Object)) {
				FREE_AND_RETURN_FAIL
			}
			cJSON *pass_obj = cJSON_GetObjectItemCaseSensitive(in_value,"reqpass");
			if (check_json_object(pass_obj,cJSON_String)) {
				FREE_AND_RETURN_FAIL
			}
			cJSON *return_json = cJSON_CreateObject();
			cJSON_AddStringToObject(return_json, "type", "default_config_status");
			if (strcmp(pass_obj->valuestring, user_config->admin_pass) == 0){
				cJSON_AddTrueToObject(return_json, "login_status");
				cJSON_AddNumberToObject(return_json, "success", load_default_config());
			}
			else {
				cJSON_AddFalseToObject(return_json, "login_status");
			}
			free(ws_pkt.payload);
			cJSON_Delete(json_root);
			return ws_send_json(req, return_json);
		}

		else if (strcmp(in_type, "debug") == 0) {
			cJSON *in_value = cJSON_GetObjectItemCaseSensitive(json_root,"value");
			if (check_json_object(in_value,cJSON_Object)) {
				FREE_AND_RETURN_FAIL
			}
			cJSON *pass_obj = cJSON_GetObjectItemCaseSensitive(in_value,"reqpass");
			if (check_json_object(pass_obj,cJSON_String)) {
				FREE_AND_RETURN_FAIL
			}
			if (strcmp(pass_obj->valuestring, user_config->admin_pass) == 0){
				cJSON *index_obj = cJSON_GetObjectItemCaseSensitive(in_value,"index");
				if (check_json_object(index_obj,cJSON_Number)) {
					FREE_AND_RETURN_FAIL
				}
				cJSON *light_obj = cJSON_GetObjectItemCaseSensitive(in_value,"light");
				if (check_json_object(light_obj,cJSON_Number)) {
					FREE_AND_RETURN_FAIL
				}
				cJSON *state_obj = cJSON_GetObjectItemCaseSensitive(in_value,"state");
				if (!check_json_object(state_obj,cJSON_True)) {
					set_segment(light_obj->valueint-1,index_obj->valueint-1,true);
				}
				else if (!check_json_object(state_obj,cJSON_False)) {
					set_segment(light_obj->valueint-1,index_obj->valueint-1,false);
				}
				else {
					FREE_AND_RETURN_FAIL
				}
			}
			free(ws_pkt.payload);
			cJSON_Delete(json_root);
			return ESP_OK;
		}

		else if (strcmp(in_type, "light_params") == 0) {
			cJSON *in_params = cJSON_GetObjectItemCaseSensitive(json_root,"value");
			if (check_json_object(in_params,cJSON_Object)) {
				FREE_AND_RETURN_FAIL
			}
			cJSON *pass_obj = cJSON_GetObjectItemCaseSensitive(in_params,"reqpass");
			if (check_json_object(pass_obj,cJSON_String)) {
				FREE_AND_RETURN_FAIL
			}

			cJSON *return_json = cJSON_CreateObject();
			cJSON_AddStringToObject(return_json, "type", "param_change_status");
			if (strcmp(pass_obj->valuestring, user_config->admin_pass) == 0){
				cJSON_AddTrueToObject(return_json, "login_status");

				cJSON *num_lights_obj = cJSON_GetObjectItemCaseSensitive(in_params,"numLights");
				if (check_json_object(num_lights_obj,cJSON_Number)) {
					FREE_AND_RETURN_FAIL
				}
				lights_t lights_ts;
				lights_ts.nol = num_lights_obj->valueint;
				set_number_of_active_lights(num_lights_obj->valueint);

				cJSON* lights = cJSON_GetObjectItemCaseSensitive(in_params, "lights");
				if (check_json_object(lights,cJSON_Array)) {
					FREE_AND_RETURN_FAIL
				}

				for (int i = 0; i < cJSON_GetArraySize(lights); i++) {
					cJSON* light = cJSON_GetArrayItem(lights, i);
					if (check_json_object(light,cJSON_Object)) {
						FREE_AND_RETURN_FAIL
					}

					cJSON *num_drivers_obj = cJSON_GetObjectItemCaseSensitive(light,"numDrivers");
					if (check_json_object(num_drivers_obj,cJSON_Number)) {
						FREE_AND_RETURN_FAIL
					}
					set_amount_of_drivers(i, num_drivers_obj->valueint);

					cJSON* drivers = cJSON_GetObjectItemCaseSensitive(light, "drivers");
					if (check_json_object(drivers,cJSON_Array)) {
						FREE_AND_RETURN_FAIL
					}
					if (drivers != NULL) {
						for (int j = 0; j < cJSON_GetArraySize(drivers); j++) {
							cJSON* driver = cJSON_GetArrayItem(drivers, j);
							if (check_json_object(driver,cJSON_Object)) {
								FREE_AND_RETURN_FAIL
							}
							cJSON *inverted = cJSON_GetObjectItemCaseSensitive(driver, "inverted");
							_Bool inverted_bool = cJSON_IsTrue(inverted);
							set_if_the_driver_is_inverted(i,j, inverted_bool);

							cJSON *address_obj = cJSON_GetObjectItemCaseSensitive(driver,"address");
							if (check_json_object(address_obj,cJSON_Number)) {
								FREE_AND_RETURN_FAIL
							}
							set_address(i, j, address_obj->valueint);

							cJSON *mask_obj = cJSON_GetObjectItemCaseSensitive(driver,"mask");
							if (check_json_object(mask_obj,cJSON_String)) {
								FREE_AND_RETURN_FAIL
							}
							char *endptr;
							uint16_t mask_bin = strtol(mask_obj->valuestring, &endptr, 2);
							make_mask(i, j, mask_bin);
						}
					}
					cJSON* num_parts = cJSON_GetObjectItemCaseSensitive(light, "numParts");
					if (check_json_object(num_parts,cJSON_Number)) {
						FREE_AND_RETURN_FAIL
					}
					lights_ts.light[i].nop = num_parts->valueint;

					cJSON* parts = cJSON_GetObjectItemCaseSensitive(light, "parts");
					if (check_json_object(parts,cJSON_Array)) {
						FREE_AND_RETURN_FAIL
					}
					if (parts != NULL) {
						for (int j = 0; j < cJSON_GetArraySize(parts); j++) {
							cJSON* part = cJSON_GetArrayItem(parts, j);
							if (check_json_object(part,cJSON_Object)) {
								FREE_AND_RETURN_FAIL
							}
							cJSON *start_obj = cJSON_GetObjectItemCaseSensitive(part, "start");
							if (check_json_object(start_obj,cJSON_Number)) {
								FREE_AND_RETURN_FAIL
							}
							cJSON *length_obj = cJSON_GetObjectItemCaseSensitive(part, "length");
							if (check_json_object(length_obj,cJSON_Number)) {
								FREE_AND_RETURN_FAIL
							}
							cJSON *name_obj = cJSON_GetObjectItemCaseSensitive(part, "name");
							if (check_json_object(name_obj,cJSON_String)) {
								FREE_AND_RETURN_FAIL
							}
							lights_ts.light[i].part_name[j] = malloc(strlen(name_obj->valuestring)+1);
							strcpy(lights_ts.light[i].part_name[j],name_obj->valuestring);

							set_part(i,j, start_obj->valueint, length_obj->valueint);
						}
					}
				}
				cJSON_AddNumberToObject(return_json, "change_success", true);
				save_light_params(&lights_ts);
			}
			else {
				cJSON_AddFalseToObject(return_json, "login_status");
			}
			cJSON_Delete(json_root);
			free(ws_pkt.payload);
			return ws_send_json(req, return_json);
		}
	}
    return ESP_OK;
}
