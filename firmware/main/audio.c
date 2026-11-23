#include "audio.h"

#include <math.h>

void audio_a_16bits(const int32_t *crudo, int16_t *pcm, size_t n, int desplazamiento) {
    for (size_t i = 0; i < n; i++) {
        int32_t v = crudo[i] >> desplazamiento; // con signo: el compilador conserva el signo (desplazamiento aritmético)
        pcm[i] = v > INT16_MAX ? INT16_MAX : v < INT16_MIN ? INT16_MIN : (int16_t)v; // saturar, no "dar la vuelta"
    }
}

int audio_rms(const int16_t *pcm, size_t n) {
    if (n == 0) return 0;
    int64_t suma = 0; // 32767² × 1600 muestras ≈ 1.7·10¹² no cabe en 32 bits
    for (size_t i = 0; i < n; i++) suma += (int32_t)pcm[i] * pcm[i];
    return (int)sqrt((double)suma / n);
}

int audio_barras(int rms, int max) {
    if (rms <= 0) return 0;
    double db = 20 * log10(rms / 32767.0); // 0 dB = máximo posible, -60 dB ≈ silencio de una habitación
    int b = (int)((db + 60) * max / 60);
    return b < 0 ? 0 : b > max ? max : b;
}
