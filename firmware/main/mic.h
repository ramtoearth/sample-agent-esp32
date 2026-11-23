// Micrófono INMP441 por I2S. Entrega PCM 16 kHz, mono, 16 bits (el formato de todo el proyecto).
#pragma once
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#define MIC_FRECUENCIA 16000
#define MIC_MUESTRAS_TROZO (MIC_FRECUENCIA / 10) // 100 ms, el mismo trozo que se manda a Transcribe

esp_err_t mic_iniciar(void);
// Bloquea hasta tener `n` muestras (n ≤ MIC_MUESTRAS_TROZO).
esp_err_t mic_leer(int16_t *pcm, size_t n);
