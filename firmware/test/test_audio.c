// Prueba de audio.c en la laptop (sin placa):
//   cc -fsanitize=address -I main test/test_audio.c main/audio.c -lm -o /tmp/test_audio && /tmp/test_audio
#include <assert.h>
#include <stdio.h>

#include "audio.h"

int main(void) {
    // El INMP441 alinea sus 24 bits arriba de los 32: el máximo es 0x7FFFFF00.
    int32_t crudo[] = {0x7FFFFF00, -0x7FFFFF00 - 0x100, 0x00010000, -0x00010000, 0};
    int16_t pcm[5];

    audio_a_16bits(crudo, pcm, 5, 16); // sin ganancia: los 16 bits de arriba
    assert(pcm[0] == 32767 && pcm[1] == -32768 && pcm[2] == 1 && pcm[3] == -1 && pcm[4] == 0);

    audio_a_16bits(crudo, pcm, 5, 12); // ×16: los extremos saturan en vez de dar la vuelta
    assert(pcm[0] == 32767 && pcm[1] == -32768 && pcm[2] == 16 && pcm[3] == -16);

    int16_t onda[] = {1000, -1000, 1000, -1000};
    assert(audio_rms(onda, 4) == 1000 && audio_rms(onda, 0) == 0);

    assert(audio_barras(32767, 21) == 21); // 0 dB: barra llena
    assert(audio_barras(33, 21) == 0);     // ≈ -60 dB: vacía
    assert(audio_barras(1036, 21) == 10);  // ≈ -30 dB: a la mitad
    assert(audio_barras(0, 21) == 0);
    puts("ok");
}
