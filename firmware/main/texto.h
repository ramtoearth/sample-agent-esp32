// Dibuja texto en un framebuffer con el formato del SSD1306. Sin dependencias de hardware:
// se puede probar en la laptop (ver firmware/test/test_texto.c).
#pragma once
#include <stdint.h>

#define TEXTO_ANCHO 128
#define TEXTO_ALTO 64
#define TEXTO_FB_BYTES (TEXTO_ANCHO * TEXTO_ALTO / 8) // 1 bit por píxel = 1024 bytes
#define TEXTO_COLS (TEXTO_ANCHO / 6)                  // 5 px de letra + 1 de espacio = 21
#define TEXTO_FILAS (TEXTO_ALTO / 8)                  // 1 fila de texto = 1 página = 8

#define TEXTO_ICONO_WIFI "\x01" // íconos de la fuente (fuente.h), en códigos de control de ASCII
#define TEXTO_ICONO_AGENTE "\x02"

// Borra desde la fila `fila` hasta abajo y escribe `utf8` ahí, con salto de línea por palabras.
// '\n' fuerza salto. Lo que no cabe se descarta.
void texto_dibujar(uint8_t fb[TEXTO_FB_BYTES], int fila, const char *utf8);

// Escribe una sola línea en (fila, col) sin borrar nada ni saltar de línea: corta a `max_cols` letras.
void texto_linea(uint8_t fb[TEXTO_FB_BYTES], int fila, int col, int max_cols, const char *utf8);
