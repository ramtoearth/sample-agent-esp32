#include <stdbool.h>
#include <stdio.h>

#include "agente.h"
#include "audio.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hora.h"
#include "mic.h"
#include "pantalla.h"
#include "texto.h"
#include "wifi.h"

#define PIN_BOTON 9              // botón BOOT de la placa: 0 = presionado (pull-up)
#define REINTENTO_VUELTAS 100    // 100 × 100 ms = reconectar como mucho cada 10 s
#define ESPERA_RESPUESTA 200     // 200 × 100 ms = 20 s sin respuesta → volver a esperar

static const char *TAG = "main";

typedef enum { ESPERANDO, GRABANDO, PENSANDO, RESPUESTA } estado_t;

static void conectar(void) {
    pantalla_mostrar("Conectando...", "Abriendo conexion\ncon el agente");
    if (agente_conectar() != ESP_OK) pantalla_mostrar("Sin agente", "No se pudo conectar\n(ver log)");
}

void app_main(void) {
    ESP_ERROR_CHECK(pantalla_iniciar());
    ESP_ERROR_CHECK(mic_iniciar());
    gpio_config_t boton = {.pin_bit_mask = 1ULL << PIN_BOTON, .mode = GPIO_MODE_INPUT, .pull_up_en = true};
    ESP_ERROR_CHECK(gpio_config(&boton));

    pantalla_mostrar("Iniciando", "Conectando WiFi...");
    if (wifi_conectar(15000) != ESP_OK) {
        pantalla_mostrar("Sin WiFi", "Reintentando...\n(ver log: motivo)"); // sigue reintentando en segundo plano
    } else if (hora_sincronizar(10000) != ESP_OK) {
        pantalla_mostrar("Sin hora", "SNTP no respondio:\nsin hora no se puede\nfirmar con AWS");
    } else {
        conectar(); // la latencia de TLS + firma se paga aquí, al arrancar
    }

    static int16_t pcm[MIC_MUESTRAS_TROZO]; // static: 3.2 KB no caben en la pila de app_main
    static const char BARRA[] = "|||||||||||||||||||||";
    _Static_assert(sizeof BARRA - 1 == TEXTO_COLS, "la barra debe medir una línea");
    char texto[320];
    agente_mensaje_t m;
    estado_t estado = ESPERANDO;
    int desde_reintento = 0, esperando = 0;

    for (int i = 0;; i++) {
        ESP_ERROR_CHECK(mic_leer(pcm, MIC_MUESTRAS_TROZO)); // espera 100 ms de audio
        bool presionado = gpio_get_level(PIN_BOTON) == 0;   // leer cada 100 ms también evita los rebotes
        pantalla_estado(wifi_ip()[0] != 0, agente_conectado()); // íconos de la barra (solo redibuja si cambian)

        // Reconexión: con WiFi, sin WebSocket y sin estar en medio de una pregunta.
        if (!agente_conectado() && estado != GRABANDO && wifi_ip()[0] && ++desde_reintento >= REINTENTO_VUELTAS) {
            desde_reintento = 0;
            conectar();
            continue; // conectar tarda: se descarta este trozo de audio
        }

        if (presionado && estado != GRABANDO) { // cualquier estado → grabando
            if (!agente_conectado()) {
                pantalla_mostrar("Sin agente", "Reintentando...");
            } else {
                while (agente_mensaje(&m)) {} // descartar respuestas viejas (de una pregunta abandonada)
                estado = GRABANDO;
                pantalla_mostrar("Escuchando...", "Suelta BOOT al\nterminar");
            }
        } else if (!presionado && estado == GRABANDO) { // grabando → pensando
            estado = PENSANDO, esperando = 0;
            agente_fin();
            pantalla_mostrar("Pensando...", "");
        }

        if (estado == GRABANDO) {
            if (agente_enviar_audio(pcm, MIC_MUESTRAS_TROZO) != ESP_OK) {
                estado = ESPERANDO;
                pantalla_mostrar("Sin agente", "Se perdio la conexion");
            }
        } else if (estado == PENSANDO) {
            if (agente_mensaje(&m)) {
                if (m.tipo == 't') {
                    snprintf(texto, sizeof texto, "Tu: %s", m.texto);
                    pantalla_mostrar("Pensando...", texto);
                } else { // respuesta o error: fin de la pregunta
                    pantalla_mostrar(m.tipo == 'e' ? "Error" : "Respuesta", m.texto);
                    estado = RESPUESTA;
                }
            } else if (++esperando >= ESPERA_RESPUESTA) {
                pantalla_mostrar("Sin respuesta", "Intenta de nuevo");
                estado = ESPERANDO;
            }
        } else if (estado == ESPERANDO) {
            int rms = audio_rms(pcm, MIC_MUESTRAS_TROZO);
            snprintf(texto, sizeof texto, "BOOT = hablar\n\nNivel: %d\n%.*s", rms, audio_barras(rms, TEXTO_COLS), BARRA);
            pantalla_mostrar(agente_conectado() ? "Listo" : "Sin agente", texto);
            if (i % 10 == 0) ESP_LOGI(TAG, "nivel RMS: %d", rms); // 1 vez por segundo
        }
    }
}
