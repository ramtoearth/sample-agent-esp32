// Conexión con el agente en AgentCore: WebSocket abierto con URL prefirmada (SigV4).
// Protocolo (ver agent/servidor.py): audio en frames binarios, "fin" como texto, respuestas JSON.
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

typedef struct {
    char tipo; // 't' = transcripción, 'r' = respuesta, 'e' = error
    char texto[256];
} agente_mensaje_t;

// Renueva credenciales si hace falta, firma una URL nueva y conecta (bloquea hasta ~15 s).
esp_err_t agente_conectar(void);
bool agente_conectado(void);
esp_err_t agente_enviar_audio(const int16_t *pcm, size_t n);
esp_err_t agente_fin(void);
// Siguiente mensaje recibido, sin bloquear. false si no hay.
bool agente_mensaje(agente_mensaje_t *m);
