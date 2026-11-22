# Arquitectura — Asistente físico

## Flujo

```
ESP32-C3 ──HTTPS + SigV4──► AgentCore Runtime (us-east-1)
 · INMP441 (I2S)             1. Transcribe Streaming (voz→texto, es-MX)
 · OLED (I2C)                2. Strands Agent (Bedrock, Amazon Nova Micro)
 · Botón (push-to-talk)      3. Polly Neural → PCM 16 kHz (cuando llegue el ampli)
 · MAX98357A (I2S)       ◄── respuesta en streaming: texto (OLED) + audio
```

1. Presionas el botón → se graba PCM 16 kHz / 16 bits / mono.
2. Sueltas el botón → POST a `/invocations` (audio en base64).
3. Transcribe → Strands → (Polly) → respuesta en streaming.

## Decisiones

| Tema | Decisión | Motivo |
|---|---|---|
| Activación | Botón (BOOT/GPIO9 mientras no haya botón) | El C3 no puede detectar palabra de activación |
| Agente | Strands en AgentCore Runtime | Hosting administrado, sesiones, streaming |
| Modelo | `amazon.nova-micro-v1:0` (on-demand) | El Nova más barato; soporta tools. Si se queda corto → `amazon.nova-lite-v1:0` |
| `<thinking>` de Nova | Se filtra con regex en `limpiar()` | Nova lo escribe en voz alta; pedirlo en el prompt no es confiable |
| Memoria | En RAM de la sesión (sin AgentCore Memory) | Costo $0; se pierde tras ~15 min de inactividad |
| Historial | `SlidingWindowConversationManager(window_size=10)` | Limita los tokens por turno |
| Herramientas | Solo `hora_actual` (zona `America/Mexico_City`, pendiente de confirmar) | MVP |
| Identidad del dispositivo | IoT Core: cert X.509 → credenciales temporales → SigV4 | No guardar llaves AWS en el chip |
| Cuenta / región | Perfil `isengard`, us-east-1 | Nova Micro y Nova Sonic on-demand + AgentCore |

## Pasos

1. ✅ Agente Strands local con texto en consola → `cd agent && AWS_PROFILE=isengard uv run agent.py`
2. + Transcribe con un WAV desde la laptop.
3. Firmware: grabar con el micrófono y mostrar estados en la OLED.
4. ESP32 → agente en la laptop (solo LAN, temporal).
5. Despliegue en AgentCore + IoT Core/SigV4.
6. + Polly y reproducción I2S (cuando llegue el ampli).

## Pendientes / riesgos

- Futuro: Nova 2 Sonic (voz a voz, Strands BidiAgent) disponible en us-east-1 → reemplazaría Transcribe + Polly.
- Futuro: WebSocket para transcribir en tiempo real, ESP32-S3 para palabra de activación.
- Crear AWS Budget con alerta.
