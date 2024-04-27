#include "main.h"

#define FILAS 4
#define COLUMNAS 4
#define ledPin 2

static const char *TAG = "ETX_PUSH_BUTTON";

// Definir los pines GPIO utilizados para las filas y columnas del teclado matricial
//const gpio_num_t rowPins[ROWS] = {GPIO_NUM_0, GPIO_NUM_1, /*GPIO_NUM_2*/ GPIO_NUM_8, GPIO_NUM_3};
//const gpio_num_t colPins[COLS] = {GPIO_NUM_4, GPIO_NUM_5, GPIO_NUM_6, GPIO_NUM_7};

// const gpio_num_t filasPins[FILAS] = {0,4,16,17};            // Pines de filas, input pullup
// const gpio_num_t columnasPins[COLUMNAS] = {5,18,19,21};     // Pines de columnas, output

const gpio_num_t columnasPins[FILAS] = {0,4,16,17};            // Pines de filas, input pullup
const gpio_num_t filasPins[COLUMNAS] = {5,18,19,21};     // Pines de columnas, output

// Definir la matriz de teclas
char keys[FILAS][COLUMNAS] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}
};

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

// Leer el valor de la tecla presionada
/* Principalmente, todas las columnas (salida) comienzan en alto.
 * Se arranca con una columna, se pone en bajo y se lee fila por fila.
 * Si la tecla no fue presionada, se leerá un 1 en todas las filas de esa columna.
 * Si la tecla se presionó, se leerá un 0 en la fila correspondiente a la tecla presionada.
 * De ahí, se debe verificar el estado de si alguna otra tecla fue presionada previamente (a implementar)
 */
char keypad_get_key() {
    for (int i = 0; i < COLUMNAS; i++) {                    // Iteración por cada columna
        gpio_set_level(columnasPins[i], 0);                 // Columna i en bajo
        //vTaskDelay(pdMS_TO_TICKS(1));
        for (int j = 0; j < FILAS; j++) {                // Iteración por cada fila
            if (gpio_get_level(filasPins[j]) == 0) {     // Si la fila j está en bajo es porque se presionó esa tecla
                gpio_set_level(columnasPins[i], 1);      // Pongo la columna en alto
                vTaskDelay(pdMS_TO_TICKS(20));           // Debounce delay
                return keys[i][j];                       // Retorno la tecla presionada
            }
        }

        gpio_set_level(columnasPins[i], 1);             // Pongo la columna en alto si no se encontró nada
    }

    return '\0';  // No se ha presionado ninguna tecla
}

// Tarea para detectar las teclas presionadas
void keypad_task(void *pvParameters) {
    while (1) {
        char key = keypad_get_key();

        if (key != '\0') {
            ESP_LOGI(TAG, "Tecla presionada: %c", key);
            gpio_set_level(ledPin, 1);  // Enciendo el led
            vTaskDelay(pdMS_TO_TICKS(20)); // delay 10 ms
            gpio_set_level(ledPin, 0);  // Apago el led
        }
        // else{
        //     ESP_LOGI(TAG, "Nada che...");
        // }

        //vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void app_main() {
    init_led();     // Configuro el pin de salida para el led
    keypad_init();  // Configuro pines de entrada, salida y estados

    ESP_LOGI(TAG, "Iniciando tarea principal...");

    xTaskCreate(keypad_task, "keypad_task", 2048, NULL, 5, NULL);   // Tarea principal
}