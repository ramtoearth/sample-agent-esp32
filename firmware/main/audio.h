// Procesamiento de audio sin dependencias de hardware (se prueba en la laptop: test/test_audio.c).
#pragma once
#include <stddef.h>
#include <stdint.h>

// Convierte muestras de 32 bits del INMP441 a PCM de 16 bits desplazando `desplazamiento` bits.
// 16 = sin ganancia; cada bit menos duplica el volumen. Satura en vez de desbordar.
void audio_a_16bits(const int32_t *crudo, int16_t *pcm, size_t n, int desplazamiento);

// Volumen RMS (raíz del promedio de los cuadrados): 0 = silencio, 32767 = máximo.
int audio_rms(const int16_t *pcm, size_t n);

// Cuántos segmentos de 0 a `max` mostrar para un RMS, en escala de decibeles (-60 dB a 0 dB).
int audio_barras(int rms, int max);
