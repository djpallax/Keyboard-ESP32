#include "main.h"

#define FILAS 4
#define COLUMNAS 4
#define ledPin 2
#define HID_DEMO_TAG "HID_DEMO"

static const char *TAG = "ETX_PUSH_BUTTON";
static uint16_t hid_conn_id = 0;
static bool sec_conn = false;
static bool send_volum_up = false;
#define CHAR_DECLARATION_SIZE   (sizeof(uint8_t))

#define LONG_PRESS_THRESHOLD_MS 300 // Umbral de tiempo para considerar una tecla presionada como "mantenida presionada"
#define REPEAT_DELAY_MS 5 // Retardo entre envíos repetidos de la tecla

char last_key = '\0'; // Variable para almacenar la última tecla presionada

static void hidd_event_callback(esp_hidd_cb_event_t event, esp_hidd_cb_param_t *param);

#define HIDD_DEVICE_NAME            "Pallardó Tech TEC01"

#define QUEUE_LENGTH 10
QueueHandle_t key_queue;

static uint8_t hidd_service_uuid128[] = {
    /* LSB <--------------------------------------------------------------------------------> MSB */
    //first uuid, 16bit, [12],[13] is the value
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x12, 0x18, 0x00, 0x00,
};

static esp_ble_adv_data_t hidd_adv_data = {
    .set_scan_rsp = false,
    .include_name = true,
    .include_txpower = true,
    .min_interval = 0x0006, //slave connection min interval, Time = min_interval * 1.25 msec
    .max_interval = 0x0010, //slave connection max interval, Time = max_interval * 1.25 msec
    .appearance = 0x03c0,       //HID Generic,
    .manufacturer_len = 0,
    .p_manufacturer_data =  NULL,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = sizeof(hidd_service_uuid128),
    .p_service_uuid = hidd_service_uuid128,
    .flag = 0x6,
};

static esp_ble_adv_params_t hidd_adv_params = {
    .adv_int_min        = 0x20,
    .adv_int_max        = 0x30,
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    //.peer_addr            =
    //.peer_addr_type       =
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

// Definir los pines GPIO utilizados para las filas y columnas del teclado matricial
const gpio_num_t columnasPins[FILAS] = {0,4,16,17};            // Pines de filas, input pullup
const gpio_num_t filasPins[COLUMNAS] = {5,18,19,21};     // Pines de columnas, output

// Definir la matriz de teclas
// char keys[FILAS][COLUMNAS] = {
//     {'1', '2', '3', 'A'},
//     {'4', '5', '6', 'B'},
//     {'7', '8', '9', 'C'},
//     {'*', '0', '#', 'D'}
// };

char keys[FILAS][COLUMNAS] = {
    {HID_KEY_1, HID_KEY_2, HID_KEY_3, HID_KEY_A},                   // F1C1, F2C1, F3C1, F4C1
    {HID_KEY_4, HID_KEY_5, HID_KEY_6, HID_KEY_B},                   // F1C2, F2C2, F3C2, F4C2
    {HID_KEY_7, HID_KEY_8, HID_KEY_9, HID_KEY_C},                   // F1C3, F2C3, F3C3, F4C3
    {HID_KEY_DELETE, HID_KEY_0, HID_KEY_SPACEBAR, HID_KEY_D}        // F1C4, F2C4, F3C4, F4C4
};

static void hidd_event_callback(esp_hidd_cb_event_t event, esp_hidd_cb_param_t *param)
{
    switch(event) {
        case ESP_HIDD_EVENT_REG_FINISH: {
            if (param->init_finish.state == ESP_HIDD_INIT_OK) {
                //esp_bd_addr_t rand_addr = {0x04,0x11,0x11,0x11,0x11,0x05};
                esp_bt_dev_set_device_name(HIDD_DEVICE_NAME);
                esp_ble_gap_config_adv_data(&hidd_adv_data);

            }
            break;
        }
        case ESP_BAT_EVENT_REG: {
            break;
        }
        case ESP_HIDD_EVENT_DEINIT_FINISH:
	     break;
		case ESP_HIDD_EVENT_BLE_CONNECT: {
            ESP_LOGI(HID_DEMO_TAG, "ESP_HIDD_EVENT_BLE_CONNECT");
            hid_conn_id = param->connect.conn_id;
            break;
        }
        case ESP_HIDD_EVENT_BLE_DISCONNECT: {
            sec_conn = false;
            ESP_LOGI(HID_DEMO_TAG, "ESP_HIDD_EVENT_BLE_DISCONNECT");
            esp_ble_gap_start_advertising(&hidd_adv_params);
            break;
        }
        case ESP_HIDD_EVENT_BLE_VENDOR_REPORT_WRITE_EVT: {
            ESP_LOGI(HID_DEMO_TAG, "%s, ESP_HIDD_EVENT_BLE_VENDOR_REPORT_WRITE_EVT", __func__);
            ESP_LOG_BUFFER_HEX(HID_DEMO_TAG, param->vendor_write.data, param->vendor_write.length);
            break;
        }
        case ESP_HIDD_EVENT_BLE_LED_REPORT_WRITE_EVT: {
            ESP_LOGI(HID_DEMO_TAG, "ESP_HIDD_EVENT_BLE_LED_REPORT_WRITE_EVT");
            ESP_LOG_BUFFER_HEX(HID_DEMO_TAG, param->led_write.data, param->led_write.length);
            break;
        }
        default:
            break;
    }
    return;
}

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        esp_ble_gap_start_advertising(&hidd_adv_params);
        break;
     case ESP_GAP_BLE_SEC_REQ_EVT:
        for(int i = 0; i < ESP_BD_ADDR_LEN; i++) {
             ESP_LOGD(HID_DEMO_TAG, "%x:",param->ble_security.ble_req.bd_addr[i]);
        }
        esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
	 break;
     case ESP_GAP_BLE_AUTH_CMPL_EVT:
        sec_conn = true;
        esp_bd_addr_t bd_addr;
        memcpy(bd_addr, param->ble_security.auth_cmpl.bd_addr, sizeof(esp_bd_addr_t));
        ESP_LOGI(HID_DEMO_TAG, "remote BD_ADDR: %08x%04x",\
                (bd_addr[0] << 24) + (bd_addr[1] << 16) + (bd_addr[2] << 8) + bd_addr[3],
                (bd_addr[4] << 8) + bd_addr[5]);
        ESP_LOGI(HID_DEMO_TAG, "address type = %d", param->ble_security.auth_cmpl.addr_type);
        ESP_LOGI(HID_DEMO_TAG, "pair status = %s",param->ble_security.auth_cmpl.success ? "success" : "fail");
        if(!param->ble_security.auth_cmpl.success) {
            ESP_LOGE(HID_DEMO_TAG, "fail reason = 0x%x",param->ble_security.auth_cmpl.fail_reason);
        }
        break;
    default:
        break;
    }
}

// Inicializar el pin de salida para el led
void init_led() {
    gpio_set_direction(ledPin, GPIO_MODE_OUTPUT);   // Configurar el pin como salida
    gpio_set_level(ledPin, 1);                      // Inicializar el pin en alto
    vTaskDelay(pdMS_TO_TICKS(500));                  // Delay de 100 ms
    gpio_set_level(ledPin, 0);                      // Inicializar el pin en bajo
}

// Inicializar el teclado matricial
// Filas como input pullup, columnas como output en high
void keypad_init() {
    for (int i = 0; i < FILAS; i++) {
        gpio_set_direction(filasPins[i], GPIO_MODE_INPUT);          // Defino pines de entrada (filas)
        gpio_set_pull_mode(filasPins[i], GPIO_PULLUP_ONLY);         // Habilito el pull-up interno
    }

    for (int i = 0; i < COLUMNAS; i++) {
        gpio_set_direction(columnasPins[i], GPIO_MODE_OUTPUT);      // Defino pines de salida (columnas)
        gpio_set_level(columnasPins[i], 1);                         // Inicio las columnas en alto
    }
}

void bt_init(void)
{
    esp_err_t ret;

    // Iniciar NVS (Non Volatile Storage), si está llena, la limpia
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );

    // Liberar la memoria del controlador BT clásico
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    // Inicializar el controlador BT, si falla, se imprime un mensaje de error
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(HID_DEMO_TAG, "%s initialize controller failed", __func__);
        return;
    }

    // Habilitar el controlador BT, si falla, se imprime un mensaje de error
    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        ESP_LOGE(HID_DEMO_TAG, "%s enable controller failed", __func__);
        return;
    }

    // Inicia el stack de Bluedroid
    esp_bluedroid_config_t bluedroid_cfg = BT_BLUEDROID_INIT_CONFIG_DEFAULT();
    ret = esp_bluedroid_init_with_cfg(&bluedroid_cfg);
    if (ret) {
        ESP_LOGE(HID_DEMO_TAG, "%s init bluedroid failed", __func__);
        return;
    }

    // Habilita el stack de Bluedroid
    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(HID_DEMO_TAG, "%s init bluedroid failed", __func__);
        return;
    }

    // Inicializa el perfil HIDD
    if((ret = esp_hidd_profile_init()) != ESP_OK) {
        ESP_LOGE(HID_DEMO_TAG, "%s init bluedroid failed", __func__);
    }

    // Registrar el callback de eventos GAP (Conexión, desconexión, etc.)
    esp_ble_gap_register_callback(gap_event_handler); 
    // Registrar el callback de eventos HIDD (Recepción de datos, etc.)
    esp_hidd_register_callbacks(hidd_event_callback);

    /* set the security iocap & auth_req & key size & init key response key parameters to the stack*/
    // Configurar los parámetros de seguridad
    esp_ble_auth_req_t auth_req = ESP_LE_AUTH_BOND;     //bonding with peer device after authentication
    esp_ble_io_cap_t iocap = ESP_IO_CAP_NONE;           //set the IO capability to No output No input
    uint8_t key_size = 16;      //the key size should be 7~16 bytes
    uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, sizeof(uint8_t));
    /* If your BLE device act as a Slave, the init_key means you hope which types of key of the master should distribute to you,
    and the response key means which key you can distribute to the Master;
    If your BLE device act as a master, the response key means you hope which types of key of the slave should distribute to you,
    and the init key means which key you can distribute to the slave. */
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key, sizeof(uint8_t));
}

// Leer el valor de la tecla presionada
/* Principalmente, todas las columnas (salida) comienzan en alto.
 * Se arranca con una columna, se pone en bajo y se lee fila por fila.
 * Si la tecla no fue presionada, se leerá un 1 en todas las filas de esa columna.
 * Si la tecla se presionó, se leerá un 0 en la fila correspondiente a la tecla presionada.
 * De ahí, se debe verificar el estado de si alguna otra tecla fue presionada previamente (a implementar)
 */
char keypad_get_key() {
    
    for (int i = 0; i < COLUMNAS; i++) {                // Iteración por cada columna
        gpio_set_level(columnasPins[i], 0);             // Columna i en bajo
        vTaskDelay(pdMS_TO_TICKS(1));                   // Pequeño delay antes de medir las filas
        for (int j = 0; j < FILAS; j++) {               // Iteración por cada fila
            if (gpio_get_level(filasPins[j]) == 0) {    // Si la fila j está en bajo es porque se presionó esa tecla
                gpio_set_level(columnasPins[i], 1);     // Pongo la columna en alto
                vTaskDelay(pdMS_TO_TICKS(5));           // Debounce delay
                return keys[i][j];                       // Retorno la tecla presionada
            }
            vTaskDelay(pdMS_TO_TICKS(1));               // Pequeño delay antes de medir la siguiente fila
        }
        gpio_set_level(columnasPins[i], 1);             // Pongo la columna en alto si no se encontró nada
    }

    return '\0';  // No se ha presionado ninguna tecla
}

// Tarea para detectar las teclas presionadas
void keypad_task(void *pvParameters) {
    char last_key = '\0';
    TickType_t last_key_tick = 0;
    bool key_repeating = false;
    TickType_t repeat_tick = 0;

    while (1) {
        char key = keypad_get_key(); // Qué tecla está presionada actualmente

        if (key != '\0' && key != last_key) {
            ESP_LOGI(TAG, "Tecla presionada: %c", key);
            gpio_set_level(ledPin, 1); // Enciende el LED
            vTaskDelay(pdMS_TO_TICKS(10)); // Delay de 10 ms
            gpio_set_level(ledPin, 0); // Apaga el LED
            xQueueSend(key_queue, &key, portMAX_DELAY); // Envía la tecla a la cola
            last_key = key; // Actualiza la última tecla presionada
            last_key_tick = xTaskGetTickCount(); // Actualiza el tiempo de la última tecla presionada
            key_repeating = false; // Reinicia el estado de la repetición de tecla
            repeat_tick = 0; // Reinicia el contador de repetición de tecla
        } else if (key != '\0' && key == last_key) {
            TickType_t current_tick = xTaskGetTickCount();
            if (current_tick - last_key_tick >= pdMS_TO_TICKS(LONG_PRESS_THRESHOLD_MS) && !key_repeating) {
                // La tecla se ha mantenido presionada durante más de LONG_PRESS_THRESHOLD_MS
                key_repeating = true; // Activa la repetición de tecla
                repeat_tick = current_tick + pdMS_TO_TICKS(REPEAT_DELAY_MS); // Establece el tiempo para empezar a repetir la tecla
            } else if (key_repeating && current_tick >= repeat_tick) {
                // Empieza a repetir la tecla
                ESP_LOGI(TAG, "Repetición de tecla: %c", key);
                xQueueSend(key_queue, &key, portMAX_DELAY); // Envía la tecla a la cola
                repeat_tick = current_tick + pdMS_TO_TICKS(REPEAT_DELAY_MS); // Actualiza el tiempo para la siguiente repetición
            }
        } else {
            // Si la tecla se ha soltado o no se ha presionado, reinicia los estados relevantes
            last_key = '\0';
            key_repeating = false;
            repeat_tick = 0;
        }
    }
}

void bt_hid_task(void *pvParameters)
{
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    while(1) {
        //vTaskDelay(2000 / portTICK_PERIOD_MS);

        // Si hay conexión
        if (sec_conn) {
            char key; // Declare the variable "key"
            if(xQueueReceive(key_queue, &key, portMAX_DELAY) == pdTRUE) {
                ESP_LOGI(HID_DEMO_TAG, "Send the key: %c", key);
                uint8_t key_vaule = {key};
                esp_hidd_send_keyboard_value(hid_conn_id, 0, &key_vaule, 1);
                // esp_hidd_send_consumer_value(hid_conn_id, &key_vaule, true);
                vTaskDelay(1 / portTICK_PERIOD_MS);
                // esp_hidd_send_consumer_value(hid_conn_id, &key_vaule, false);
                esp_hidd_send_keyboard_value(hid_conn_id, 0, &key_vaule, 0);

                
                //hid_dev_send_report(hidd_le_env.gatt_if, HID_RPT_ID_KEY_IN, HID_REPORT_TYPE_INPUT, CHAR_DECLARATION_SIZE, &key_vaule);
                vTaskDelay(1 / portTICK_PERIOD_MS);
                //xQueueReset(key_queue);
            }
            //ESP_LOGI(HID_DEMO_TAG, "Send the volume");
            //ESP_LOGI(HID_DEMO_TAG, "Send the key");
            //send_volum_up = true;
                    // uint8_t key_vaule_a = {HID_KEY_A};
                    // ESP_LOGI(HID_DEMO_TAG, "Send the key: %d", key_vaule_a);
                    // esp_hidd_send_keyboard_value(hid_conn_id, 0, &key_vaule_a, 1);
                    // vTaskDelay(200 / portTICK_PERIOD_MS);
            //esp_hidd_send_consumer_value(hid_conn_id, HID_CONSUMER_VOLUME_UP, true);
            //vTaskDelay(3000 / portTICK_PERIOD_MS);
            // if (send_volum_up) {
            //     send_volum_up = false;
            //     esp_hidd_send_consumer_value(hid_conn_id, HID_CONSUMER_VOLUME_UP, false);
            //     esp_hidd_send_consumer_value(hid_conn_id, HID_CONSUMER_VOLUME_DOWN, true);
            //     vTaskDelay(3000 / portTICK_PERIOD_MS);
            //     esp_hidd_send_consumer_value(hid_conn_id, HID_CONSUMER_VOLUME_DOWN, false);
            // }
        }
        //esp_task_wdt_reset();
    }
}

void app_main() {

    key_queue = xQueueCreate(QUEUE_LENGTH, sizeof(char));  // Creo la cola de teclas

    init_led();     // Configuro el pin de salida para el led
    keypad_init();  // Configuro pines de entrada, salida y estados
    bt_init();      // Inicializo el Bluetooth

    ESP_LOGI(TAG, "Iniciando tarea principal...");

    xTaskCreate(&keypad_task, "keypad_task", 8192, NULL, 5, NULL);   // Tarea del teclado matricial
    xTaskCreate(&bt_hid_task, "bt_hid_task", 8192, NULL, 5, NULL);   // Tarea de la comunicaicón Bluetooth HID
}