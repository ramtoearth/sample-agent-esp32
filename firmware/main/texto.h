// Dibuja texto en un framebuffer con el formato del SSD1306. Sin dependencias de hardware:
// se puede probar en la laptop (ver firmware/test/test_texto.c).
#pragma once
#include <stdint.h>

#define TEXTO_ANCHO 128
#define TEXTO_ALTO 64
#define TEXTO_FB_BYTES (TEXTO_ANCHO * TEXTO_ALTO / 8) // 1 bit por píxel = 1024 bytes
#define TEXTO_COLS (TEXTO_ANCHO / 6)                  // 5 px de letra + 1 de espacio = 21
#define TEXTO_FILAS (TEXTO_ALTO / 8)                  // 1 fila de texto = 1 página = 8

// Borra el framebuffer y escribe `utf8` con salto de línea por palabras. '\n' fuerza salto.
// Lo que no cabe en las 8 filas se descarta.
void texto_dibujar(uint8_t fb[TEXTO_FB_BYTES], const char *utf8);
