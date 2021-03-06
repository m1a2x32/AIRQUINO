/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)
   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"
#include "esp_system.h"
#include "sdkconfig.h"


#define GATTS_SERVICE_UUID_TEST_A   0x0010
#define GATTS_CHAR_UUID_TEST_A      0x0001
#define GATTS_NUM_HANDLE_TEST_A     4

#define GATTS_SERVICE_UUID_TEST_B   0x0020
#define GATTS_CHAR_UUID_TEST_B      0x0002
#define GATTS_NUM_HANDLE_TEST_B     4

#define GATTS_DEMO_CHAR_VAL_LEN_MAX 0x40
#define PREPARE_BUF_MAX_SIZE        1024

#define adv_config_flag             (1 << 0)
#define scan_rsp_config_flag        (1 << 1)

#define GATTS_PROFILE_NUM           2
#define GATTS_PROFILE_A_APP_ID      0
#define GATTS_PROFILE_B_APP_ID      1
#define GATTC_PROFILE_NUM           2
#define GATTC_PROFILE_A_APP_ID      0
#define GATTC_PROFILE_B_APP_ID      1

// gattc
const uint8_t REMOTE_SERVICE_UUID[ESP_UUID_LEN_128] = {0x42, 0x00, 0x74, 0xA9, 0xFF, 0x52, 0x10, 0x9B, 
                                       0x33, 0x49, 0x35, 0x9B, 0x00, 0x02, 0x68, 0xEF};

const uint8_t REMOTE_CHAR_UUID[ESP_UUID_LEN_128] = {0x42, 0x00, 0x74, 0xA9, 0xFF, 0x52, 0x10, 0x9B, 
                                       0x33, 0x49, 0x35, 0x9B, 0x04, 0x02, 0x68, 0xEF};

#define INVALID_HANDLE              0
#define GATTS_ADV_NAME              "Zone1"
#define COEX_TAG                    "CL"
#define NOTIFY_ENABLE               0x0001
#define INDICATE_ENABLE             0x0002
#define NOTIFY_INDICATE_DISABLE     0x0000

static bool conn_device_a   = false;
static bool conn_device_b   = false;

static bool get_service_a   = false;
static bool get_service_b   = false;

static bool Isconnecting    = false;
static bool stop_scan  = false;

static esp_gattc_char_elem_t *char_elem_result_a   = NULL;
static esp_gattc_descr_elem_t *descr_elem_result_a = NULL;
static esp_gattc_char_elem_t *char_elem_result_b   = NULL;
static esp_gattc_descr_elem_t *descr_elem_result_b = NULL;

static const char remote_device_name[2][20] = {"Thingy","Thingy2"};

typedef struct {
    uint8_t                 *prepare_buf;
    int                      prepare_len;
} prepare_type_env_t;

struct gatts_profile_inst {
    esp_gatts_cb_t gatts_cb;
    uint16_t gatts_if;
    uint16_t app_id;
    uint16_t conn_id;
    uint16_t service_handle;
    esp_gatt_srvc_id_t service_id;
    uint16_t char_handle;
    esp_bt_uuid_t char_uuid;
    esp_gatt_perm_t perm;
    esp_gatt_char_prop_t property;
    uint16_t descr_handle;
    esp_bt_uuid_t descr_uuid;
};

struct gattc_profile_inst {
    esp_gattc_cb_t gattc_cb;
    uint16_t gattc_if;
    uint16_t app_id;
    uint16_t conn_id;
    uint16_t service_start_handle;
    uint16_t service_end_handle;
    uint16_t char_handle;
    esp_bd_addr_t remote_bda;
};

///Declare the static function
static void gatts_profile_a_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
static void gatts_profile_b_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
/* Declare static functions */
static void esp_gattc_cb(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
static void gattc_profile_event_a_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
static void gattc_profile_event_b_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
static void example_write_event_env(esp_gatt_if_t gatts_if, prepare_type_env_t *prepare_write_env, esp_ble_gatts_cb_param_t *param);
static void example_exec_write_event_env(prepare_type_env_t *prepare_write_env, esp_ble_gatts_cb_param_t *param);

static esp_gatt_char_prop_t a_property = 0;
static esp_gatt_char_prop_t b_property = 0;
static prepare_type_env_t a_prepare_write_env;
static prepare_type_env_t b_prepare_write_env;
static uint8_t adv_config_done         = 0;
static uint8_t char1_str[]             = {0x11, 0x22, 0x33 , 0x22};

uint16_t ppm1, voc1, ppm2, voc2;
uint32_t send1, send2;

esp_attr_value_t gatts_demo_char1_val = {
    .attr_max_len = GATTS_DEMO_CHAR_VAL_LEN_MAX,
    .attr_len     = sizeof(char1_str),
    .attr_value   = char1_str,
};

static uint8_t service_uuid128[32] = {
    /* LSB <--------------------------------------------------------------------------------> MSB */
    //first uuid, 16bit, [12],[13] is the value
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0xAB, 0xCD, 0x00, 0x00,
    //second uuid, 32bit, [12], [13], [14], [15] is the value
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0xAB, 0xCD, 0xAB, 0xCD,
};

static esp_ble_adv_data_t adv_data = {
    .set_scan_rsp = false,
    .include_name = true,
    .include_txpower = true,
    .min_interval = 0x05,
    .max_interval = 0x10,
    .appearance = 0x00,
    .manufacturer_len = 0,
    .p_manufacturer_data =  NULL,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = 32,
    .p_service_uuid = service_uuid128,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

// scan response data
static esp_ble_adv_data_t scan_rsp_data = {
    .set_scan_rsp = true,
    .include_name = true,
    .include_txpower = true,
    .min_interval = 0x0006,
    .max_interval = 0x0010,
    .appearance = 0x00,
    .manufacturer_len = 0,
    .p_manufacturer_data =  NULL,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = 0,
    .p_service_uuid = NULL,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

static esp_ble_adv_params_t adv_params = {
    .adv_int_min        = 0x20,
    .adv_int_max        = 0x40,
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

/* One gatt-based profile one app_id and one gatts_if, this array will store the gatts_if returned by ESP_GATTS_REG_EVT */
static struct gatts_profile_inst gatts_profile_tab[GATTS_PROFILE_NUM] = {
    [GATTS_PROFILE_A_APP_ID] = {
        .gatts_cb = gatts_profile_a_event_handler,
        .gatts_if = ESP_GATT_IF_NONE,       /* Not get the gatt_if, so initial is ESP_GATT_IF_NONE */
    },
    [GATTS_PROFILE_B_APP_ID] = {
        .gatts_cb = gatts_profile_b_event_handler,                   /* This demo does not implement, similar as profile A */
        .gatts_if = ESP_GATT_IF_NONE,       /* Not get the gatt_if, so initial is ESP_GATT_IF_NONE */
    },
};

static esp_bt_uuid_t remote_filter_service_uuid = {
    .len = ESP_UUID_LEN_128,
    .uuid.uuid128 = {REMOTE_SERVICE_UUID[0],
                     REMOTE_SERVICE_UUID[1],
                     REMOTE_SERVICE_UUID[2],
                     REMOTE_SERVICE_UUID[3],
                     REMOTE_SERVICE_UUID[4],
                     REMOTE_SERVICE_UUID[5],
                     REMOTE_SERVICE_UUID[6],
                     REMOTE_SERVICE_UUID[7],
                     REMOTE_SERVICE_UUID[8],
                     REMOTE_SERVICE_UUID[9],
                     REMOTE_SERVICE_UUID[10],
                     REMOTE_SERVICE_UUID[11],
                     REMOTE_SERVICE_UUID[12],
                     REMOTE_SERVICE_UUID[13],
                     REMOTE_SERVICE_UUID[14],
                     REMOTE_SERVICE_UUID[15],
    },
};

static esp_bt_uuid_t remote_filter_char_uuid = {
    .len = ESP_UUID_LEN_128,
    .uuid.uuid128 = {REMOTE_CHAR_UUID[0],
                     REMOTE_CHAR_UUID[1],
                     REMOTE_CHAR_UUID[2],
                     REMOTE_CHAR_UUID[3],
                     REMOTE_CHAR_UUID[4],
                     REMOTE_CHAR_UUID[5],
                     REMOTE_CHAR_UUID[6],
                     REMOTE_CHAR_UUID[7],
                     REMOTE_CHAR_UUID[8],
                     REMOTE_CHAR_UUID[9],
                     REMOTE_CHAR_UUID[10],
                     REMOTE_CHAR_UUID[11],
                     REMOTE_CHAR_UUID[12],
                     REMOTE_CHAR_UUID[13],
                     REMOTE_CHAR_UUID[14],
                     REMOTE_CHAR_UUID[15],
    },
};

static esp_bt_uuid_t notify_descr_uuid = {
    .len = ESP_UUID_LEN_16,
    .uuid = {.uuid16 = 0x2902},
};

static esp_ble_scan_params_t ble_scan_params = {
    .scan_type              = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type          = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy     = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval          = 0x50,
    .scan_window            = 0x30,
    //.scan_duplicate         = BLE_SCAN_DUPLICATE_DISABLE
};

/* One gatt-based profile one app_id and one gattc_if, this array will store the gattc_if returned by ESP_GATTS_REG_EVT */
static struct gattc_profile_inst gattc_profile_tab[GATTC_PROFILE_NUM] = {
    [GATTC_PROFILE_A_APP_ID] = {
        .gattc_cb = gattc_profile_event_a_handler,
        .gattc_if = ESP_GATT_IF_NONE,       /* Not get the gatt_if, so initial is ESP_GATT_IF_NONE */
    },
    [GATTC_PROFILE_B_APP_ID] = {
        .gattc_cb = gattc_profile_event_b_handler,
        .gattc_if = ESP_GATT_IF_NONE,       /* Not get the gatt_if, so initial is ESP_GATT_IF_NONE */
    },
};

static void start_scan(void)
{
    stop_scan = false;
    Isconnecting = false;
    uint32_t duration = 30;
    esp_ble_gap_start_scanning(duration);
}

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    uint8_t *adv_name = NULL;
    uint8_t adv_name_len = 0;

    switch (event) {

    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        adv_config_done &= (~adv_config_flag);
        if (adv_config_done == 0) {
            esp_ble_gap_start_advertising(&adv_params);
        }
        break;
    case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
        adv_config_done &= (~scan_rsp_config_flag);
        if (adv_config_done == 0) {
            esp_ble_gap_start_advertising(&adv_params);
        }
        break;

    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        //advertising start complete event to indicate advertising start successfully or failed
        if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(COEX_TAG, "Advertising start failed\n");
        }
        ESP_LOGI(COEX_TAG, "Advertising start successfully\n");
        break;
    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(COEX_TAG, "Advertising stop failed\n");
        } else {
            ESP_LOGI(COEX_TAG, "Stop adv successfully\n");
        }
        break;
    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
         ESP_LOGI(COEX_TAG, "update connection params status = %d, min_int = %d, max_int = %d,conn_int = %d,latency = %d, timeout = %d\n",
                  param->update_conn_params.status,
                  param->update_conn_params.min_int,
                  param->update_conn_params.max_int,
                  param->update_conn_params.conn_int,
                  param->update_conn_params.latency,
                  param->update_conn_params.timeout);
        break;
    case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
        if (param->scan_stop_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(COEX_TAG, "scan stop failed, error status = %x\n", param->scan_stop_cmpl.status);
            break;
        }
        break;
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT: {
        uint32_t duration = 30;
        esp_ble_gap_start_scanning(duration);
        break;
    }
    case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
        if (param->scan_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(COEX_TAG, "scan start failed, error status = %x\n", param->scan_start_cmpl.status);
            break;
        }
        break;
    case ESP_GAP_BLE_SCAN_RESULT_EVT: {
        esp_ble_gap_cb_param_t *scan_result = (esp_ble_gap_cb_param_t *)param;
        switch (scan_result->scan_rst.search_evt) {
        case ESP_GAP_SEARCH_INQ_RES_EVT:
            adv_name = esp_ble_resolve_adv_data(scan_result->scan_rst.ble_adv,
                                                ESP_BLE_AD_TYPE_NAME_CMPL, &adv_name_len);
            if (Isconnecting){
                break;
            }
            if (conn_device_a && conn_device_b && !stop_scan){
                stop_scan = true;
                esp_ble_gap_stop_scanning();
                ESP_LOGI(COEX_TAG, "all devices are connected");
                break;
            }
            if (adv_name != NULL) {
                if (strlen(remote_device_name[0]) == adv_name_len && strncmp((char *)adv_name, remote_device_name[0], adv_name_len) == 0) {
                    if (conn_device_a == false) {
                        conn_device_a = true;
                        ESP_LOGI(COEX_TAG, "connect to the remote device %s\n", remote_device_name[0]);
                        esp_ble_gap_stop_scanning();
                        esp_ble_gattc_open(gattc_profile_tab[GATTC_PROFILE_A_APP_ID].gattc_if, scan_result->scan_rst.bda, scan_result->scan_rst.ble_addr_type, true);
                        Isconnecting = true;
                    }
                    break;
                }
                else if(strlen(remote_device_name[1]) == adv_name_len && strncmp((char *)adv_name, remote_device_name[1], adv_name_len) == 0){
                    if (conn_device_b == false) {
                        conn_device_b = true;
                        ESP_LOGI(COEX_TAG, "connect to the remote device %s\n", remote_device_name[1]);
                        esp_ble_gap_stop_scanning();
                        esp_ble_gattc_open(gattc_profile_tab[GATTC_PROFILE_B_APP_ID].gattc_if, scan_result->scan_rst.bda, scan_result->scan_rst.ble_addr_type, true);
                        Isconnecting = true;
                    }
                    break;
                }
            }
            break;
        case ESP_GAP_SEARCH_INQ_CMPL_EVT:
             ESP_LOGI(COEX_TAG, "ESP_GAP_SEARCH_INQ_CMPL_EVT, scan stop\n");
            break;
        default:
            break;
        }
        break;
    }
    default:
        break;
    }
}

static void gattc_profile_event_a_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param)
{
    esp_ble_gattc_cb_param_t *p_data = (esp_ble_gattc_cb_param_t *)param;

    switch (event) {
    case ESP_GATTC_REG_EVT:
        ESP_LOGI(COEX_TAG, "REG_EVT\n");
        esp_err_t scan_ret = esp_ble_gap_set_scan_params(&ble_scan_params);
        if (scan_ret) {
            ESP_LOGE(COEX_TAG, "set scan params error, error code = %x", scan_ret);
        }
        break;
    case ESP_GATTC_CONNECT_EVT:
        break;
    case ESP_GATTC_OPEN_EVT:
        if (param->open.status != ESP_GATT_OK) {
            ESP_LOGE(COEX_TAG, "open failed, status %d\n", p_data->open.status);
            break;
        }
        memcpy(gattc_profile_tab[GATTC_PROFILE_A_APP_ID].remote_bda, p_data->open.remote_bda, 6);
        gattc_profile_tab[GATTC_PROFILE_A_APP_ID].conn_id = p_data->open.conn_id;
        ESP_LOGI(COEX_TAG, "Profile A adr:");
        esp_log_buffer_hex(COEX_TAG, gattc_profile_tab[GATTC_PROFILE_A_APP_ID].remote_bda, sizeof(esp_bd_addr_t));
        esp_err_t mtu_ret = esp_ble_gattc_send_mtu_req (gattc_if, p_data->open.conn_id);
        if (mtu_ret){
            ESP_LOGE(COEX_TAG, "config MTU error, error code = %x", mtu_ret);
        }
        break;
    case ESP_GATTC_DIS_SRVC_CMPL_EVT:
        if (param->dis_srvc_cmpl.status != ESP_GATT_OK) {
            ESP_LOGE(COEX_TAG, "discover service failed, status %d\n", param->dis_srvc_cmpl.status);
            break;
        }
        esp_ble_gattc_search_service(gattc_if, param->cfg_mtu.conn_id, &remote_filter_service_uuid);
        break;
    case ESP_GATTC_CFG_MTU_EVT:
        if (param->cfg_mtu.status != ESP_GATT_OK) {
            ESP_LOGE(COEX_TAG,"config mtu failed, error status = %x\n", param->cfg_mtu.status);
        }
        break;
    case ESP_GATTC_SEARCH_RES_EVT: {
        if (p_data->search_res.srvc_id.uuid.len == ESP_UUID_LEN_128 && 
        p_data->search_res.srvc_id.uuid.uuid.uuid128[0] == REMOTE_SERVICE_UUID[0] && 
        p_data->search_res.srvc_id.uuid.uuid.uuid128[1] == REMOTE_SERVICE_UUID[1] && 
        p_data->search_res.srvc_id.uuid.uuid.uuid128[2] == REMOTE_SERVICE_UUID[2] && 
        p_data->search_res.srvc_id.uuid.uuid.uuid128[3] == REMOTE_SERVICE_UUID[3] && 
        p_data->search_res.srvc_id.uuid.uuid.uuid128[4] == REMOTE_SERVICE_UUID[4] && 
        p_data->search_res.srvc_id.uuid.uuid.uuid128[5] == REMOTE_SERVICE_UUID[5] && 
        p_data->search_res.srvc_id.uuid.uuid.uuid128[6] == REMOTE_SERVICE_UUID[6]) {
            get_service_a = true;
            gattc_profile_tab[GATTC_PROFILE_A_APP_ID].service_start_handle = p_data->search_res.start_handle;
            gattc_profile_tab[GATTC_PROFILE_A_APP_ID].service_end_handle = p_data->search_res.end_handle;
        }
        break;
    }
    case ESP_GATTC_SEARCH_CMPL_EVT:
        if (p_data->search_cmpl.status != ESP_GATT_OK) {
            ESP_LOGE(COEX_TAG, "search service failed, error status = %x\n", p_data->search_cmpl.status);
            break;
        }
        if (get_service_a) {
            uint16_t count = 0;
            esp_gatt_status_t status = esp_ble_gattc_get_attr_count( gattc_if,
                                                                     p_data->search_cmpl.conn_id,
                                                                     ESP_GATT_DB_CHARACTERISTIC,
                                                                     gattc_profile_tab[GATTC_PROFILE_A_APP_ID].service_start_handle,
                                                                     gattc_profile_tab[GATTC_PROFILE_A_APP_ID].service_end_handle,
                                                                     INVALID_HANDLE,
                                                                     &count);
            if (status != ESP_GATT_OK) {
                ESP_LOGE(COEX_TAG, "esp_ble_gattc_get_attr_count error\n");
            }

            if (count > 0) {
                char_elem_result_a = (esp_gattc_char_elem_t *)malloc(sizeof(esp_gattc_char_elem_t) * count);
                if (!char_elem_result_a) {
                    ESP_LOGE(COEX_TAG, "gattc no mem\n");
                }else {
                    status = esp_ble_gattc_get_char_by_uuid( gattc_if,
                                                             p_data->search_cmpl.conn_id,
                                                             gattc_profile_tab[GATTC_PROFILE_A_APP_ID].service_start_handle,
                                                             gattc_profile_tab[GATTC_PROFILE_A_APP_ID].service_end_handle,
                                                             remote_filter_char_uuid,
                                                             char_elem_result_a,
                                                             &count);
                    if (status != ESP_GATT_OK) {
                        ESP_LOGE(COEX_TAG, "esp_ble_gattc_get_char_by_uuid error\n");
                    }

                    /*  Every service have only one char in our 'ESP_GATTS_DEMO' demo, so we used first 'char_elem_result_a' */
                    if (count > 0 && (char_elem_result_a[0].properties & ESP_GATT_CHAR_PROP_BIT_NOTIFY)) {
                        gattc_profile_tab[GATTC_PROFILE_A_APP_ID].char_handle = char_elem_result_a[0].char_handle;
                        esp_ble_gattc_register_for_notify (gattc_if, gattc_profile_tab[GATTC_PROFILE_A_APP_ID].remote_bda, char_elem_result_a[0].char_handle);
                    }
                }
                /* free char_elem_result_a */
                free(char_elem_result_a);
            } else {
                ESP_LOGE(COEX_TAG, "no char found\n");
            }
        }
         break;
    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
        if (p_data->reg_for_notify.status != ESP_GATT_OK) {
            ESP_LOGE(COEX_TAG, "REG FOR NOTIFY failed: error status = %d\n", p_data->reg_for_notify.status);
            break;
        }
            uint16_t count = 0;
            uint16_t notify_en = 1;
            esp_gatt_status_t ret_status = esp_ble_gattc_get_attr_count( gattc_if,
                                                                         gattc_profile_tab[GATTC_PROFILE_A_APP_ID].conn_id,
                                                                         ESP_GATT_DB_DESCRIPTOR,
                                                                         gattc_profile_tab[GATTC_PROFILE_A_APP_ID].service_start_handle,
                                                                         gattc_profile_tab[GATTC_PROFILE_A_APP_ID].service_end_handle,
                                                                         gattc_profile_tab[GATTC_PROFILE_A_APP_ID].char_handle,
                                                                         &count);
            if (ret_status != ESP_GATT_OK) {
                ESP_LOGE(COEX_TAG, "esp_ble_gattc_get_attr_count error\n");
            }
            if (count > 0) {
                descr_elem_result_a = malloc(sizeof(esp_gattc_descr_elem_t) * count);
                if (!descr_elem_result_a) {
                    ESP_LOGE(COEX_TAG, "malloc error, gattc no mem\n");
                } else {
                    ret_status = esp_ble_gattc_get_descr_by_char_handle( gattc_if,
                                                                         gattc_profile_tab[GATTC_PROFILE_A_APP_ID].conn_id,
                                                                         p_data->reg_for_notify.handle,
                                                                         notify_descr_uuid,
                                                                         descr_elem_result_a,
                                                                         &count);
                    if (ret_status != ESP_GATT_OK) {
                        ESP_LOGE(COEX_TAG, "esp_ble_gattc_get_descr_by_char_handle error\n");
                    }
                    /* Every char has only one descriptor in our 'ESP_GATTS_DEMO' demo, so we used first 'descr_elem_result_a' */
                    if (count > 0 && descr_elem_result_a[0].uuid.len == ESP_UUID_LEN_16 && descr_elem_result_a[0].uuid.uuid.uuid16 == ESP_GATT_UUID_CHAR_CLIENT_CONFIG) {
                        ret_status = esp_ble_gattc_write_char_descr( gattc_if,
                                                                     gattc_profile_tab[GATTC_PROFILE_A_APP_ID].conn_id,
                                                                     descr_elem_result_a[0].handle,
                                                                     sizeof(notify_en),
                                                                     (uint8_t *)&notify_en,
                                                                     ESP_GATT_WRITE_TYPE_RSP,
                                                                     ESP_GATT_AUTH_REQ_NONE);
                    }

                    if (ret_status != ESP_GATT_OK) {
                        ESP_LOGE(COEX_TAG, "esp_ble_gattc_write_char_descr error\n");
                    }

                    /* free descr_elem_result_a */
                    free(descr_elem_result_a);
                }
            } else {
                ESP_LOGE(COEX_TAG, "decsr not found\n");
            }

        break;
    }
    case ESP_GATTC_NOTIFY_EVT:
        ppm1 = p_data->notify.value[1] << 8 | p_data->notify.value[0];
        voc1 = p_data->notify.value[3] << 8 | p_data->notify.value[2];
        ESP_LOGI(COEX_TAG, "Device A received: PPM - %d VOC - %d", ppm1, voc1);        
        break;
    case ESP_GATTC_WRITE_DESCR_EVT:
        break;
    case ESP_GATTC_SRVC_CHG_EVT: {
        esp_bd_addr_t bda;
        memcpy(bda, p_data->srvc_chg.remote_bda, sizeof(esp_bd_addr_t));
        break;
    }
    case ESP_GATTC_WRITE_CHAR_EVT:
        if(!conn_device_b){
            start_scan();
        }
        break;
    case ESP_GATTC_DISCONNECT_EVT: {
        //start_scan();
        if (memcmp(p_data->disconnect.remote_bda, gattc_profile_tab[GATTC_PROFILE_A_APP_ID].remote_bda, 6) == 0){
            ESP_LOGI(COEX_TAG, "device a disconnect");
            conn_device_a = false;
            get_service_a = false;
        }
        break;
    }
    default:
        break;
    }
}

static void gattc_profile_event_b_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param)
{
    esp_ble_gattc_cb_param_t *p_data = (esp_ble_gattc_cb_param_t *)param;

    switch (event) {
    case ESP_GATTC_REG_EVT:
        ESP_LOGI(COEX_TAG, "REG_EVT\n");
        break;
    case ESP_GATTC_CONNECT_EVT:
        break;
    case ESP_GATTC_OPEN_EVT:
        if (p_data->open.status != ESP_GATT_OK){
            ESP_LOGE(COEX_TAG, "connect device failed, status %d", p_data->open.status);
            conn_device_b = false;
            //start_scan();
            break;
        }
        memcpy(gattc_profile_tab[GATTC_PROFILE_B_APP_ID].remote_bda, p_data->open.remote_bda, 6);
        gattc_profile_tab[GATTC_PROFILE_B_APP_ID].conn_id = p_data->open.conn_id;
        ESP_LOGI(COEX_TAG, "Profile B adr:");
        esp_log_buffer_hex(COEX_TAG, p_data->open.remote_bda, sizeof(esp_bd_addr_t));
        esp_err_t mtu_ret = esp_ble_gattc_send_mtu_req(gattc_if, p_data->open.conn_id);
        if (mtu_ret){
            ESP_LOGE(COEX_TAG, "config MTU error, error code = %x", mtu_ret);
        }
        break;
    case ESP_GATTC_DIS_SRVC_CMPL_EVT:
        if (param->dis_srvc_cmpl.status != ESP_GATT_OK) {
            ESP_LOGE(COEX_TAG, "discover service failed, status %d\n", param->dis_srvc_cmpl.status);
            break;
        }
        esp_ble_gattc_search_service(gattc_if, param->cfg_mtu.conn_id, &remote_filter_service_uuid);
        break;
    case ESP_GATTC_CFG_MTU_EVT:
        if (param->cfg_mtu.status != ESP_GATT_OK) {
            ESP_LOGE(COEX_TAG,"config mtu failed, error status = %x\n", param->cfg_mtu.status);
        }
        break;
    case ESP_GATTC_SEARCH_RES_EVT: {
        if (p_data->search_res.srvc_id.uuid.len == ESP_UUID_LEN_128 && 
        p_data->search_res.srvc_id.uuid.uuid.uuid128[0] == REMOTE_SERVICE_UUID[0] && 
        p_data->search_res.srvc_id.uuid.uuid.uuid128[1] == REMOTE_SERVICE_UUID[1] && 
        p_data->search_res.srvc_id.uuid.uuid.uuid128[2] == REMOTE_SERVICE_UUID[2] && 
        p_data->search_res.srvc_id.uuid.uuid.uuid128[3] == REMOTE_SERVICE_UUID[3] && 
        p_data->search_res.srvc_id.uuid.uuid.uuid128[4] == REMOTE_SERVICE_UUID[4] && 
        p_data->search_res.srvc_id.uuid.uuid.uuid128[5] == REMOTE_SERVICE_UUID[5] && 
        p_data->search_res.srvc_id.uuid.uuid.uuid128[6] == REMOTE_SERVICE_UUID[6]) {
            get_service_b = true;
            gattc_profile_tab[GATTC_PROFILE_B_APP_ID].service_start_handle = p_data->search_res.start_handle;
            gattc_profile_tab[GATTC_PROFILE_B_APP_ID].service_end_handle = p_data->search_res.end_handle;
        }
        break;
    }
    case ESP_GATTC_SEARCH_CMPL_EVT:
        if (p_data->search_cmpl.status != ESP_GATT_OK) {
            ESP_LOGE(COEX_TAG, "search service failed, error status = %x\n", p_data->search_cmpl.status);
            break;
        }
        ESP_LOGI(COEX_TAG, "ESP_GATTC_SEARCH_CMPL_EVT\n");
        if (get_service_b) {
            uint16_t count = 0;
            esp_gatt_status_t status = esp_ble_gattc_get_attr_count( gattc_if,
                                                                     p_data->search_cmpl.conn_id,
                                                                     ESP_GATT_DB_CHARACTERISTIC,
                                                                     gattc_profile_tab[GATTC_PROFILE_B_APP_ID].service_start_handle,
                                                                     gattc_profile_tab[GATTC_PROFILE_B_APP_ID].service_end_handle,
                                                                     INVALID_HANDLE,
                                                                     &count);
            if (status != ESP_GATT_OK) {
                ESP_LOGE(COEX_TAG, "esp_ble_gattc_get_attr_count error\n");
            }

            if (count > 0) {
                char_elem_result_b = (esp_gattc_char_elem_t *)malloc(sizeof(esp_gattc_char_elem_t) * count);
                if (!char_elem_result_b) {
                    ESP_LOGE(COEX_TAG, "gattc no mem\n");
                }else {
                    status = esp_ble_gattc_get_char_by_uuid( gattc_if,
                                                             p_data->search_cmpl.conn_id,
                                                             gattc_profile_tab[GATTC_PROFILE_B_APP_ID].service_start_handle,
                                                             gattc_profile_tab[GATTC_PROFILE_B_APP_ID].service_end_handle,
                                                             remote_filter_char_uuid,
                                                             char_elem_result_b,
                                                             &count);
                    if (status != ESP_GATT_OK) {
                        ESP_LOGE(COEX_TAG, "esp_ble_gattc_get_char_by_uuid error\n");
                    }

                    /*  Every service have only one char in our 'ESP_GATTS_DEMO' demo, so we used first 'char_elem_result_b' */
                    if (count > 0 && (char_elem_result_b[0].properties & ESP_GATT_CHAR_PROP_BIT_NOTIFY)) {
                        gattc_profile_tab[GATTC_PROFILE_B_APP_ID].char_handle = char_elem_result_b[0].char_handle;
                        esp_ble_gattc_register_for_notify (gattc_if, gattc_profile_tab[GATTC_PROFILE_B_APP_ID].remote_bda, char_elem_result_b[0].char_handle);
                    }
                }
                /* free char_elem_result_b */
                free(char_elem_result_b);
            } else {
                ESP_LOGE(COEX_TAG, "no char found\n");
            }
        }
         break;
    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
        ESP_LOGI(COEX_TAG, "ESP_GATTC_REG_FOR_NOTIFY_EVT\n");
        if (p_data->reg_for_notify.status != ESP_GATT_OK) {
            ESP_LOGE(COEX_TAG, "REG FOR NOTIFY failed: error status = %d\n", p_data->reg_for_notify.status);
        } else {
            uint16_t count = 0;
            uint16_t notify_en = 1;
            esp_gatt_status_t ret_status = esp_ble_gattc_get_attr_count( gattc_if,
                                                                         gattc_profile_tab[GATTC_PROFILE_B_APP_ID].conn_id,
                                                                         ESP_GATT_DB_DESCRIPTOR,
                                                                         gattc_profile_tab[GATTC_PROFILE_B_APP_ID].service_start_handle,
                                                                         gattc_profile_tab[GATTC_PROFILE_B_APP_ID].service_end_handle,
                                                                         gattc_profile_tab[GATTC_PROFILE_B_APP_ID].char_handle,
                                                                         &count);
            if (ret_status != ESP_GATT_OK) {
                ESP_LOGE(COEX_TAG, "esp_ble_gattc_get_attr_count error\n");
            }
            if (count > 0) {
                descr_elem_result_b = malloc(sizeof(esp_gattc_descr_elem_t) * count);
                if (!descr_elem_result_b) {
                    ESP_LOGE(COEX_TAG, "malloc error, gattc no mem\n");
                } else {
                    ret_status = esp_ble_gattc_get_descr_by_char_handle( gattc_if,
                                                                         gattc_profile_tab[GATTC_PROFILE_B_APP_ID].conn_id,
                                                                         p_data->reg_for_notify.handle,
                                                                         notify_descr_uuid,
                                                                         descr_elem_result_b,
                                                                         &count);
                    if (ret_status != ESP_GATT_OK) {
                        ESP_LOGE(COEX_TAG, "esp_ble_gattc_get_descr_by_char_handle error\n");
                    }
                    /* Every char has only one descriptor in our 'ESP_GATTS_DEMO' demo, so we used first 'descr_elem_result_b' */
                    if (count > 0 && descr_elem_result_b[0].uuid.len == ESP_UUID_LEN_16 && descr_elem_result_b[0].uuid.uuid.uuid16 == ESP_GATT_UUID_CHAR_CLIENT_CONFIG) {
                        ret_status = esp_ble_gattc_write_char_descr( gattc_if,
                                                                     gattc_profile_tab[GATTC_PROFILE_B_APP_ID].conn_id,
                                                                     descr_elem_result_b[0].handle,
                                                                     sizeof(notify_en),
                                                                     (uint8_t *)&notify_en,
                                                                     ESP_GATT_WRITE_TYPE_RSP,
                                                                     ESP_GATT_AUTH_REQ_NONE);
                    }

                    if (ret_status != ESP_GATT_OK) {
                        ESP_LOGE(COEX_TAG, "esp_ble_gattc_write_char_descr error\n");
                    }

                    /* free descr_elem_result_b */
                    free(descr_elem_result_b);
                }
            } else {
                ESP_LOGE(COEX_TAG, "decsr not found\n");
            }

        }
        break;
    }
    case ESP_GATTC_NOTIFY_EVT:
        ppm2 = p_data->notify.value[1] << 8 | p_data->notify.value[0];
        voc2 = p_data->notify.value[3] << 8 | p_data->notify.value[2];
        ESP_LOGI(COEX_TAG, "Device B received notify value: PPM - %d VOC - %d", ppm2, voc2);
        break;
    case ESP_GATTC_WRITE_DESCR_EVT:
        if (p_data->write.status != ESP_GATT_OK){
                ESP_LOGE(COEX_TAG, "write descr failed, error status = %x", p_data->write.status);
                break;
            }
            ESP_LOGI(COEX_TAG, "write descr success");
            uint8_t write_char_data[35];
            for (int i = 0; i < sizeof(write_char_data); ++i)
            {
                write_char_data[i] = i % 256;
            }
            esp_ble_gattc_write_char( gattc_if,
                                    gattc_profile_tab[GATTC_PROFILE_B_APP_ID].conn_id,
                                    gattc_profile_tab[GATTC_PROFILE_B_APP_ID].char_handle,
                                    sizeof(write_char_data),
                                    write_char_data,
                                    ESP_GATT_WRITE_TYPE_RSP,
                                    ESP_GATT_AUTH_REQ_NONE);
        break;
    case ESP_GATTC_SRVC_CHG_EVT: {
        esp_bd_addr_t bda;
        memcpy(bda, p_data->srvc_chg.remote_bda, sizeof(esp_bd_addr_t));
        ESP_LOGI(COEX_TAG, "ESP_GATTC_SRVC_CHG_EVT, bd_addr:");
        esp_log_buffer_hex(COEX_TAG, bda, sizeof(esp_bd_addr_t));
        break;
    }
    case ESP_GATTC_WRITE_CHAR_EVT:
        start_scan();
        break;
    case ESP_GATTC_DISCONNECT_EVT: {
        if (memcmp(p_data->disconnect.remote_bda, gattc_profile_tab[GATTC_PROFILE_B_APP_ID].remote_bda, 6) == 0){
            ESP_LOGI(COEX_TAG, "device b disconnect");
            conn_device_b = false;
            get_service_b = false;
        }
        break;
    }
    default:
        break;
    }
}

static void example_write_event_env(esp_gatt_if_t gatts_if, prepare_type_env_t *prepare_write_env, esp_ble_gatts_cb_param_t *param) {
    esp_gatt_status_t status = ESP_GATT_OK;
    if (param->write.need_rsp) {
        if (param->write.is_prep) {
            if (prepare_write_env->prepare_buf == NULL) {
                prepare_write_env->prepare_buf = (uint8_t *)malloc(PREPARE_BUF_MAX_SIZE*sizeof(uint8_t));
                prepare_write_env->prepare_len = 0;
                if (prepare_write_env->prepare_buf == NULL) {
                    ESP_LOGE(COEX_TAG, "Gatt_server prep no mem\n");
                    status = ESP_GATT_NO_RESOURCES;
                }
            } else {
                if(param->write.offset > PREPARE_BUF_MAX_SIZE) {
                    status = ESP_GATT_INVALID_OFFSET;
                } else if ((param->write.offset + param->write.len) > PREPARE_BUF_MAX_SIZE) {
                    status = ESP_GATT_INVALID_ATTR_LEN;
                }
            }

            esp_gatt_rsp_t *gatt_rsp = (esp_gatt_rsp_t *)malloc(sizeof(esp_gatt_rsp_t));
            gatt_rsp->attr_value.len = param->write.len;
            gatt_rsp->attr_value.handle = param->write.handle;
            gatt_rsp->attr_value.offset = param->write.offset;
            gatt_rsp->attr_value.auth_req = ESP_GATT_AUTH_REQ_NONE;
            memcpy(gatt_rsp->attr_value.value, param->write.value, param->write.len);
            esp_err_t response_err = esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, status, gatt_rsp);
            if (response_err != ESP_OK) {
               ESP_LOGE(COEX_TAG, "Send response error\n");
            }
            free(gatt_rsp);
            if (status != ESP_GATT_OK) {
                return;
            }
            memcpy(prepare_write_env->prepare_buf + param->write.offset,
                   param->write.value,
                   param->write.len);
            prepare_write_env->prepare_len += param->write.len;

        } else {
            esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, status, NULL);
        }
    }
}

static void example_exec_write_event_env(prepare_type_env_t *prepare_write_env, esp_ble_gatts_cb_param_t *param) {
    if (param->exec_write.exec_write_flag == ESP_GATT_PREP_WRITE_EXEC) {
        esp_log_buffer_hex(COEX_TAG, prepare_write_env->prepare_buf, prepare_write_env->prepare_len);
    } else {
        ESP_LOGI(COEX_TAG,"ESP_GATT_PREP_WRITE_CANCEL\n");
    }
    if (prepare_write_env->prepare_buf) {
        free(prepare_write_env->prepare_buf);
        prepare_write_env->prepare_buf = NULL;
    }
    prepare_write_env->prepare_len = 0;
}

static void gatts_profile_a_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
    switch (event) {
    case ESP_GATTS_REG_EVT:
        ESP_LOGI(COEX_TAG, "REGISTER_APP_EVT, status %d, app_id %d\n", param->reg.status, param->reg.app_id);
        gatts_profile_tab[GATTS_PROFILE_A_APP_ID].service_id.is_primary = true;
        gatts_profile_tab[GATTS_PROFILE_A_APP_ID].service_id.id.inst_id = 0x00;
        gatts_profile_tab[GATTS_PROFILE_A_APP_ID].service_id.id.uuid.len = ESP_UUID_LEN_16;
        gatts_profile_tab[GATTS_PROFILE_A_APP_ID].service_id.id.uuid.uuid.uuid16 = GATTS_SERVICE_UUID_TEST_A;

        esp_err_t set_dev_name_ret = esp_ble_gap_set_device_name(GATTS_ADV_NAME);
        if (set_dev_name_ret) {
            ESP_LOGE(COEX_TAG, "set device name failed, error code = %x\n", set_dev_name_ret);
        }
#ifdef CONFIG_SET_RAW_ADV_DATA
        esp_err_t raw_adv_ret = esp_ble_gap_config_adv_data_raw(raw_adv_data, sizeof(raw_adv_data));
        if (raw_adv_ret) {
            ESP_LOGE(COEX_TAG, "config raw adv data failed, error code = %x \n", raw_adv_ret);
        }
        adv_config_done |= adv_config_flag;
        esp_err_t raw_scan_ret = esp_ble_gap_config_scan_rsp_data_raw(raw_scan_rsp_data, sizeof(raw_scan_rsp_data));
        if (raw_scan_ret) {
            ESP_LOGE(COEX_TAG, "config raw scan rsp data failed, error code = %x\n", raw_scan_ret);
        }
        adv_config_done |= scan_rsp_config_flag;
#else
        //config adv data
        esp_err_t ret = esp_ble_gap_config_adv_data(&adv_data);
        if (ret) {
            ESP_LOGE(COEX_TAG, "config adv data failed, error code = %x\n", ret);
        }
        adv_config_done |= adv_config_flag;
        //config scan response data
        ret = esp_ble_gap_config_adv_data(&scan_rsp_data);
        if (ret) {
            ESP_LOGE(COEX_TAG, "config scan response data failed, error code = %x\n", ret);
        }
        adv_config_done |= scan_rsp_config_flag;

#endif
        esp_ble_gatts_create_service(gatts_if, &gatts_profile_tab[GATTS_PROFILE_A_APP_ID].service_id, GATTS_NUM_HANDLE_TEST_A);
        break;
    case ESP_GATTS_READ_EVT: {
        send1 = (ppm1<<16) | voc1;
        esp_gatt_rsp_t rsp;
        memset(&rsp, 0, sizeof(esp_gatt_rsp_t));
        rsp.attr_value.handle = param->read.handle;
        rsp.attr_value.len = sizeof(send1); 
        memcpy(&rsp.attr_value.value, &send1, sizeof(send1));
        esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id,
                                    ESP_GATT_OK, &rsp);
        break;
    }
    case ESP_GATTS_WRITE_EVT: {
        ESP_LOGI(COEX_TAG, "GATT_WRITE_EVT, conn_id %d, trans_id %d, handle %d\n", param->write.conn_id, param->write.trans_id, param->write.handle);
        if (!param->write.is_prep) {
            ESP_LOGI(COEX_TAG, "GATT_WRITE_EVT, value len %d, value :", param->write.len);
            esp_log_buffer_hex(COEX_TAG, param->write.value, param->write.len);
            if (gatts_profile_tab[GATTS_PROFILE_A_APP_ID].descr_handle == param->write.handle && param->write.len == 2) {
                uint16_t descr_value = param->write.value[1]<<8 | param->write.value[0];
                if (descr_value == NOTIFY_ENABLE) {
                    if (a_property & ESP_GATT_CHAR_PROP_BIT_NOTIFY) {
                        ESP_LOGI(COEX_TAG, "notify enable\n");
                        uint8_t notify_data[15];
                        for (int i = 0; i < sizeof(notify_data); ++ i) {
                            notify_data[i] = i%0xff;
                        }
                        esp_ble_gatts_send_indicate(gatts_if, param->write.conn_id, gatts_profile_tab[GATTS_PROFILE_A_APP_ID].char_handle,
                                                sizeof(notify_data), notify_data, false);
                    }
                } else if (descr_value == INDICATE_ENABLE) {
                    if (a_property & ESP_GATT_CHAR_PROP_BIT_INDICATE) {
                        ESP_LOGI(COEX_TAG, "indicate enable\n");
                        uint8_t indicate_data[15];
                        for (int i = 0; i < sizeof(indicate_data); ++ i) {
                            indicate_data[i] = i%0xff;
                        }
                        //the size of indicate_data[] need less than MTU size
                        esp_ble_gatts_send_indicate(gatts_if, param->write.conn_id, gatts_profile_tab[GATTS_PROFILE_A_APP_ID].char_handle,
                                                    sizeof(indicate_data), indicate_data, true);
                    }
                } else if (descr_value == NOTIFY_INDICATE_DISABLE) {
                    ESP_LOGI(COEX_TAG, "notify/indicate disable \n");
                } else {
                    ESP_LOGE(COEX_TAG, "unknown descr value\n");
                    esp_log_buffer_hex(COEX_TAG, param->write.value, param->write.len);
                }

            }
        }
        example_write_event_env(gatts_if, &a_prepare_write_env, param);
        break;
    }
    case ESP_GATTS_EXEC_WRITE_EVT:
        esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
        example_exec_write_event_env(&a_prepare_write_env, param);
        break;
    case ESP_GATTS_MTU_EVT:
        ESP_LOGI(COEX_TAG, "ESP_GATTS_MTU_EVT, MTU %d\n", param->mtu.mtu);
        break;
    case ESP_GATTS_CREATE_EVT:
        ESP_LOGI(COEX_TAG, "CREATE_SERVICE_EVT, status %d,  service_handle %d\n", param->create.status, param->create.service_handle);
        gatts_profile_tab[GATTS_PROFILE_A_APP_ID].service_handle = param->create.service_handle;
        gatts_profile_tab[GATTS_PROFILE_A_APP_ID].char_uuid.len = ESP_UUID_LEN_16;
        gatts_profile_tab[GATTS_PROFILE_A_APP_ID].char_uuid.uuid.uuid16 = GATTS_CHAR_UUID_TEST_A;

        esp_ble_gatts_start_service(gatts_profile_tab[GATTS_PROFILE_A_APP_ID].service_handle);
        a_property = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_NOTIFY;
        esp_err_t add_char_ret = esp_ble_gatts_add_char(gatts_profile_tab[GATTS_PROFILE_A_APP_ID].service_handle, &gatts_profile_tab[GATTS_PROFILE_A_APP_ID].char_uuid,
                                                        ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                                        a_property,
                                                        &gatts_demo_char1_val, NULL);
        if (add_char_ret) {
            ESP_LOGE(COEX_TAG, "add char failed, error code =%x\n",add_char_ret);
        }
        break;
    case ESP_GATTS_ADD_CHAR_EVT: {
        gatts_profile_tab[GATTS_PROFILE_A_APP_ID].char_handle = param->add_char.attr_handle;
        gatts_profile_tab[GATTS_PROFILE_A_APP_ID].descr_uuid.len = ESP_UUID_LEN_16;
        gatts_profile_tab[GATTS_PROFILE_A_APP_ID].descr_uuid.uuid.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;

        esp_err_t add_descr_ret = esp_ble_gatts_add_char_descr(gatts_profile_tab[GATTS_PROFILE_A_APP_ID].service_handle, &gatts_profile_tab[GATTS_PROFILE_A_APP_ID].descr_uuid,
                                                                ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, NULL, NULL);
        if (add_descr_ret) {
            ESP_LOGE(COEX_TAG, "add char descr failed, error code =%x\n", add_descr_ret);
        }
        break;
    }
    case ESP_GATTS_ADD_CHAR_DESCR_EVT:
        gatts_profile_tab[GATTS_PROFILE_A_APP_ID].descr_handle = param->add_char_descr.attr_handle;
        break;
    case ESP_GATTS_START_EVT:
        break;
    case ESP_GATTS_CONNECT_EVT: {
        break;
    }
    case ESP_GATTS_DISCONNECT_EVT:
        esp_ble_gap_start_advertising(&adv_params);
        break;
    case ESP_GATTS_CONF_EVT:
        if (param->conf.status != ESP_GATT_OK) {
            esp_log_buffer_hex(COEX_TAG, param->conf.value, param->conf.len);
        }
        break;
    case ESP_GATTS_OPEN_EVT:
    default:
        break;
    }
}

static void gatts_profile_b_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
    switch (event) {
    case ESP_GATTS_REG_EVT:
        ESP_LOGI(COEX_TAG, "REGISTER_APP_EVT, status %d, app_id %d\n", param->reg.status, param->reg.app_id);
        gatts_profile_tab[GATTS_PROFILE_B_APP_ID].service_id.is_primary = true;
        gatts_profile_tab[GATTS_PROFILE_B_APP_ID].service_id.id.inst_id = 0x00;
        gatts_profile_tab[GATTS_PROFILE_B_APP_ID].service_id.id.uuid.len = ESP_UUID_LEN_16;
        gatts_profile_tab[GATTS_PROFILE_B_APP_ID].service_id.id.uuid.uuid.uuid16 = GATTS_SERVICE_UUID_TEST_B;

        esp_ble_gatts_create_service(gatts_if, &gatts_profile_tab[GATTS_PROFILE_B_APP_ID].service_id, GATTS_NUM_HANDLE_TEST_B);
        break;
    case ESP_GATTS_READ_EVT: {
        send2 = (ppm2<<16) | voc2;
        esp_gatt_rsp_t rsp;
        memset(&rsp, 0, sizeof(esp_gatt_rsp_t));
        rsp.attr_value.handle = param->read.handle;
        rsp.attr_value.len = sizeof(send2);
        memcpy(&rsp.attr_value.value, &send2, sizeof(send2));
        esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id,
                                    ESP_GATT_OK, &rsp);
        break;
    }
    case ESP_GATTS_WRITE_EVT: {
        ESP_LOGI(COEX_TAG, "GATT_WRITE_EVT, conn_id %d, trans_id %d, handle %d\n", param->write.conn_id, param->write.trans_id, param->write.handle);
        if (!param->write.is_prep) {
            ESP_LOGI(COEX_TAG, "GATT_WRITE_EVT, value len %d, value :", param->write.len);
            esp_log_buffer_hex(COEX_TAG, param->write.value, param->write.len);
            if (gatts_profile_tab[GATTS_PROFILE_B_APP_ID].descr_handle == param->write.handle && param->write.len == 2) {
                uint16_t descr_value= param->write.value[1]<<8 | param->write.value[0];
                if (descr_value == NOTIFY_ENABLE) {
                    if (b_property & ESP_GATT_CHAR_PROP_BIT_NOTIFY) {
                        ESP_LOGI(COEX_TAG, "notify enable\n");
                        uint8_t notify_data[15];
                        for (int i = 0; i < sizeof(notify_data); ++ i) {
                            notify_data[i] = i%0xff;
                        }
                        //the size of notify_data[] need less than MTU size
                        esp_ble_gatts_send_indicate(gatts_if, param->write.conn_id, gatts_profile_tab[GATTS_PROFILE_B_APP_ID].char_handle,
                                                sizeof(notify_data), notify_data, false);
                    }
                } else if (descr_value == INDICATE_ENABLE) {
                    if (b_property & ESP_GATT_CHAR_PROP_BIT_INDICATE) {
                        ESP_LOGI(COEX_TAG, "indicate enable\n");
                        uint8_t indicate_data[15];
                        for (int i = 0; i < sizeof(indicate_data); ++ i) {
                            indicate_data[i] = i%0xff;
                        }
                        //the size of indicate_data[] need less than MTU size
                        esp_ble_gatts_send_indicate(gatts_if, param->write.conn_id, gatts_profile_tab[GATTS_PROFILE_B_APP_ID].char_handle,
                                                    sizeof(indicate_data), indicate_data, true);
                    }
                } else if (descr_value == NOTIFY_INDICATE_DISABLE) {
                    ESP_LOGI(COEX_TAG, "notify/indicate disable \n");
                } else {
                    ESP_LOGE(COEX_TAG, "unknown value\n");
                }

            }
        }
        example_write_event_env(gatts_if, &b_prepare_write_env, param);
        break;
    }
    case ESP_GATTS_EXEC_WRITE_EVT:
        ESP_LOGI(COEX_TAG,"ESP_GATTS_EXEC_WRITE_EVT\n");
        esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
        example_exec_write_event_env(&b_prepare_write_env, param);
        break;
    case ESP_GATTS_MTU_EVT:
        ESP_LOGI(COEX_TAG, "ESP_GATTS_MTU_EVT, MTU %d\n", param->mtu.mtu);
        break;
    case ESP_GATTS_UNREG_EVT:
        break;
    case ESP_GATTS_CREATE_EVT:
        gatts_profile_tab[GATTS_PROFILE_B_APP_ID].service_handle = param->create.service_handle;
        gatts_profile_tab[GATTS_PROFILE_B_APP_ID].char_uuid.len = ESP_UUID_LEN_16;
        gatts_profile_tab[GATTS_PROFILE_B_APP_ID].char_uuid.uuid.uuid16 = GATTS_CHAR_UUID_TEST_B;

        esp_ble_gatts_start_service(gatts_profile_tab[GATTS_PROFILE_B_APP_ID].service_handle);
        b_property = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_NOTIFY;
        esp_err_t add_char_ret =esp_ble_gatts_add_char( gatts_profile_tab[GATTS_PROFILE_B_APP_ID].service_handle, &gatts_profile_tab[GATTS_PROFILE_B_APP_ID].char_uuid,
                                                        ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                                        b_property,
                                                        NULL, NULL);
        if (add_char_ret) {
            ESP_LOGE(COEX_TAG, "add char failed, error code =%x\n",add_char_ret);
        }
        break;
    case ESP_GATTS_ADD_INCL_SRVC_EVT:
        break;
    case ESP_GATTS_ADD_CHAR_EVT:
        gatts_profile_tab[GATTS_PROFILE_B_APP_ID].char_handle = param->add_char.attr_handle;
        gatts_profile_tab[GATTS_PROFILE_B_APP_ID].descr_uuid.len = ESP_UUID_LEN_16;
        gatts_profile_tab[GATTS_PROFILE_B_APP_ID].descr_uuid.uuid.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
        esp_ble_gatts_add_char_descr(gatts_profile_tab[GATTS_PROFILE_B_APP_ID].service_handle, &gatts_profile_tab[GATTS_PROFILE_B_APP_ID].descr_uuid,
                                     ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                     NULL, NULL);
        break;
    case ESP_GATTS_ADD_CHAR_DESCR_EVT:
        gatts_profile_tab[GATTS_PROFILE_B_APP_ID].descr_handle = param->add_char_descr.attr_handle;
        break;
    case ESP_GATTS_DELETE_EVT:
        break;
    case ESP_GATTS_START_EVT:
        break;
    case ESP_GATTS_STOP_EVT:
        break;
    case ESP_GATTS_CONNECT_EVT:
        ESP_LOGI(COEX_TAG, "CONNECT_EVT, conn_id %d, remote %02x:%02x:%02x:%02x:%02x:%02x\n",
                 param->connect.conn_id,
                 param->connect.remote_bda[0], param->connect.remote_bda[1], param->connect.remote_bda[2],
                 param->connect.remote_bda[3], param->connect.remote_bda[4], param->connect.remote_bda[5]);
                 gatts_profile_tab[GATTS_PROFILE_B_APP_ID].conn_id = param->connect.conn_id;
        break;
    case ESP_GATTS_CONF_EVT:
        ESP_LOGI(COEX_TAG, "ESP_GATTS_CONF_EVT status %d attr_handle %d\n", param->conf.status, param->conf.handle);
        if (param->conf.status != ESP_GATT_OK) {
            esp_log_buffer_hex(COEX_TAG, param->conf.value, param->conf.len);
        }
        break;
    case ESP_GATTS_DISCONNECT_EVT:
        break;
    case ESP_GATTS_OPEN_EVT:
        break;
    default:
        break;
    }
}

static void esp_gattc_cb(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param)
{
    /* If event is register event, store the gattc_if for each profile */
    if (event == ESP_GATTC_REG_EVT) {
        if (param->reg.status == ESP_GATT_OK) {
            gattc_profile_tab[param->reg.app_id].gattc_if = gattc_if;
        } else {
            ESP_LOGI(COEX_TAG, "reg app failed, app_id %04x, status %d\n",
                    param->reg.app_id,
                    param->reg.status);
            return;
        }
    }

    /* If the gattc_if equal to profile A, call profile A cb handler,
     * so here call each profile's callback */
    do {
        int idx;
        for (idx = 0; idx < GATTC_PROFILE_NUM; idx ++) {
            if (gattc_if == ESP_GATT_IF_NONE || /* ESP_GATT_IF_NONE, not specify a certain gatt_if, need to call every profile cb function */
                    gattc_if == gattc_profile_tab[idx].gattc_if) {
                if (gattc_profile_tab[idx].gattc_cb) {
                    gattc_profile_tab[idx].gattc_cb(event, gattc_if, param);
                }
            }
        }
    } while (0);
}

static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    /* If event is register event, store the gatts_if for each profile */
    if (event == ESP_GATTS_REG_EVT) {
        if (param->reg.status == ESP_GATT_OK) {
            gatts_profile_tab[param->reg.app_id].gatts_if = gatts_if;
        } else {
            ESP_LOGI(COEX_TAG, "Reg app failed, app_id %04x, status %d\n",
                    param->reg.app_id,
                    param->reg.status);
            return;
        }
    }

    /* If the gatts_if equal to profile A, call profile A cb handler,
     * so here call each profile's callback */
    do {
        int idx;
        for (idx = 0; idx < GATTS_PROFILE_NUM; idx ++) {
            if (gatts_if == ESP_GATT_IF_NONE || /* ESP_GATT_IF_NONE, not specify a certain gatt_if, need to call every profile cb function */
                    gatts_if == gatts_profile_tab[idx].gatts_if) {
                if (gatts_profile_tab[idx].gatts_cb) {
                    gatts_profile_tab[idx].gatts_cb(event, gatts_if, param);
                }
            }
        }
    } while (0);
}

void app_main(void)
{
    esp_err_t ret;

    // Initialize NVS.
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(COEX_TAG, "%s initialize controller failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        ESP_LOGE(COEX_TAG, "%s enable controller failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_bluedroid_init();
    if (ret) {
        ESP_LOGE(COEX_TAG, "%s init bluetooth failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(COEX_TAG, "%s enable bluetooth failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_ble_gap_register_callback(gap_event_handler);
    if (ret) {
        ESP_LOGE(COEX_TAG, "gap register error, error code = %x", ret);
        return;
    }

    // gatts register
    ret = esp_ble_gatts_register_callback(gatts_event_handler);
    if (ret) {
        ESP_LOGE(COEX_TAG, "gatts register error, error code = %x", ret);
        return;
    }

    ret = esp_ble_gatts_app_register(GATTS_PROFILE_A_APP_ID);
    if (ret) {
        ESP_LOGE(COEX_TAG, "gatts app register error, error code = %x", ret);
        return;
    }

    ret = esp_ble_gatts_app_register(GATTS_PROFILE_B_APP_ID);
    if (ret) {
        ESP_LOGE(COEX_TAG, "gatts app register error, error code = %x", ret);
        return;
    }

    // gattc regisrter
    ret = esp_ble_gattc_register_callback(esp_gattc_cb);
    if(ret) {
        ESP_LOGE(COEX_TAG, "%s gattc register failed, error code = %x\n", __func__, ret);
        return;
    }

    ret = esp_ble_gattc_app_register(GATTC_PROFILE_A_APP_ID);
    if (ret) {
        ESP_LOGE(COEX_TAG, "%s gattc app register failed, error code = %x\n", __func__, ret);
    }
    
    ret = esp_ble_gattc_app_register(GATTC_PROFILE_B_APP_ID);
    if (ret) {
        ESP_LOGE(COEX_TAG, "%s gattc app register failed, error code = %x\n", __func__, ret);
    }

    esp_err_t local_mtu_ret = esp_ble_gatt_set_local_mtu(200);
    if (local_mtu_ret) {
        ESP_LOGE(COEX_TAG, "set local  MTU failed, error code = %x", local_mtu_ret);
    }

    return;
}