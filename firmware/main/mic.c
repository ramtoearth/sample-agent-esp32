#include "mic.h"

#include "audio.h"
#include "driver/i2s_std.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h" // portMAX_DELAY = esperar sin límite

#define PIN_BCLK 4 // INMP441 SCK
#define PIN_WS 5
#define PIN_SD 6
// Calibración: 16 = sin ganancia; cada bit menos duplica el volumen (12 = ×16).
// Ajústalo viendo la barra de la OLED: al hablar normal a ~20 cm debería llegar a la mitad.
#define DESPLAZAMIENTO 12

static const char *TAG = "mic";
static i2s_chan_handle_t rx;
static int32_t crudo[MIC_MUESTRAS_TROZO]; // 6.4 KB: global, no cabe en la pila de la tarea

esp_err_t mic_iniciar(void) {
    // Solo recepción. Cuando llegue el ampli: crear TX y RX juntos (comparten BCLK y WS).
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    ESP_RETURN_ON_ERROR(i2s_new_channel(&chan_cfg, NULL, &rx), TAG, "canal");

    i2s_std_config_t cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(MIC_FRECUENCIA),
        // Philips: el dato empieza 1 pulso después del cambio de WS (así lo manda el INMP441).
        // 32 bits: el INMP441 manda 24 bits útiles dentro de un hueco de 32.
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = PIN_BCLK,
            .ws = PIN_WS,
            .dout = I2S_GPIO_UNUSED,
            .din = PIN_SD,
        },
    };
    cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT; // L/R del INMP441 a GND = canal izquierdo
    ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(rx, &cfg), TAG, "modo estándar");
    return i2s_channel_enable(rx);
}

esp_err_t mic_leer(int16_t *pcm, size_t n) {
    if (n > MIC_MUESTRAS_TROZO) return ESP_ERR_INVALID_ARG; // no cabe en `crudo`
    size_t bytes;
    ESP_RETURN_ON_ERROR(i2s_channel_read(rx, crudo, n * sizeof crudo[0], &bytes, portMAX_DELAY), TAG, "leer");
    audio_a_16bits(crudo, pcm, bytes / sizeof crudo[0], DESPLAZAMIENTO);
    return ESP_OK;
}
