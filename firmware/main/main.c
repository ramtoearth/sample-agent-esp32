#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "audio.h"
#include "driver/gpio.h"
#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mic.h"
#include "pantalla.h"
#include "texto.h"
#include "wifi.h"

#define PIN_BOTON 9 // botón BOOT de la placa: 0 = presionado (pull-up)

static const char *TAG = "main";

typedef enum { ESPERANDO, GRABANDO, PENSANDO, RESPUESTA } estado_t;

// Manda un trozo de audio por USB como "A:<hex>\n". Lo lee la laptop (agent/puente_usb.py).
static void enviar_hex(const int16_t *pcm, size_t n) {
    static char linea[2 + MIC_MUESTRAS_TROZO * 4 + 2]; // "A:" + 2 hex por byte + "\n\0"
    static const char HEX[] = "0123456789abcdef";
    const uint8_t *b = (const uint8_t *)pcm; // little-endian: el mismo orden que espera Transcribe
    char *p = linea;
    *p++ = 'A', *p++ = ':';
    for (size_t i = 0; i < n * 2; i++) *p++ = HEX[b[i] >> 4], *p++ = HEX[b[i] & 0xF];
    *p++ = '\n', *p = 0;
    fputs(linea, stdout);
}

// Junta lo que llega por USB sin bloquear. Devuelve la línea cuando llega un '\n'; si no, NULL.
static const char *leer_linea(void) {
    static char linea[256]; // la respuesta del agente es corta (cabe en la OLED: ~170 letras)
    static size_t largo;
    uint8_t c;
    while (usb_serial_jtag_read_bytes(&c, 1, 0) == 1) { // 0 = no esperar: solo lo que ya llegó
        if (c == '\n') {
            linea[largo] = 0, largo = 0;
            return linea;
        }
        if (c != '\r' && largo < sizeof linea - 1) linea[largo++] = c; // lo que no cabe se descarta
    }
    return NULL;
}

static void iniciar_usb(void) {
    // TX de 8 KB: una línea de audio (6.4 KB) cabe completa y fputs regresa enseguida.
    usb_serial_jtag_driver_config_t cfg = {.tx_buffer_size = 8192, .rx_buffer_size = 512};
    ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&cfg));
    usb_serial_jtag_vfs_use_driver(); // printf/fputs también pasan por el driver
}

void app_main(void) {
    iniciar_usb();
    ESP_ERROR_CHECK(pantalla_iniciar());
    ESP_ERROR_CHECK(mic_iniciar());

    pantalla_mostrar("Conectando WiFi...");
    char texto[64];
    if (wifi_conectar(15000) == ESP_OK) { // si no conecta seguimos: sigue reintentando en segundo plano
        snprintf(texto, sizeof texto, "WiFi OK\n%s", wifi_ip());
    } else {
        snprintf(texto, sizeof texto, "Sin WiFi\n(ver log: motivo)");
    }
    pantalla_mostrar(texto);
    vTaskDelay(pdMS_TO_TICKS(2000)); // 2 s para leer el resultado
    gpio_config_t boton = {.pin_bit_mask = 1ULL << PIN_BOTON, .mode = GPIO_MODE_INPUT, .pull_up_en = true};
    ESP_ERROR_CHECK(gpio_config(&boton));

    static int16_t pcm[MIC_MUESTRAS_TROZO]; // static: 3.2 KB no caben en la pila de app_main
    static const char BARRA[] = "|||||||||||||||||||||";
    _Static_assert(sizeof BARRA - 1 == TEXTO_COLS, "la barra debe medir una línea");
    estado_t estado = ESPERANDO;

    for (int i = 0;; i++) {
        ESP_ERROR_CHECK(mic_leer(pcm, MIC_MUESTRAS_TROZO)); // espera 100 ms de audio
        bool presionado = gpio_get_level(PIN_BOTON) == 0;   // leer cada 100 ms también evita los rebotes
        const char *linea = leer_linea();

        if (presionado && estado != GRABANDO) { // cualquier estado → grabando
            estado = GRABANDO;
            pantalla_mostrar("Escuchando...");
            puts("<<AUDIO");
        } else if (!presionado && estado == GRABANDO) { // grabando → pensando
            estado = PENSANDO;
            puts(">>AUDIO");
            pantalla_mostrar("Pensando...");
        } else if (estado == PENSANDO && linea && strncmp(linea, "R:", 2) == 0) { // pensando → respuesta
            estado = RESPUESTA;
            pantalla_mostrar(linea + 2); // +2 salta el "R:"
        }

        if (estado == GRABANDO) {
            enviar_hex(pcm, MIC_MUESTRAS_TROZO);
        } else if (estado == ESPERANDO) {
            int rms = audio_rms(pcm, MIC_MUESTRAS_TROZO);
            snprintf(texto, sizeof texto, "Listo (BOOT=hablar)\n\nNivel: %d\n%.*s", rms,
                     audio_barras(rms, TEXTO_COLS), BARRA);
            pantalla_mostrar(texto);
            if (i % 10 == 0) ESP_LOGI(TAG, "nivel RMS: %d", rms); // 1 vez por segundo
        }
    }
}
