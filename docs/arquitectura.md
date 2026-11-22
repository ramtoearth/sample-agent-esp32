# Arquitectura — Asistente físico

## Flujo

```
ESP32-C3 ──WebSocket + SigV4─► AgentCore Runtime (us-east-1)
 · INMP441 (I2S)             1. Transcribe Streaming (voz→texto, es-US)
 · OLED (I2C)                2. Strands Agent (Bedrock, Amazon Nova Micro)
 · Botón (push-to-talk)      3. Polly Neural → PCM 16 kHz (cuando llegue el ampli)
 · MAX98357A (I2S)       ◄── respuesta en streaming: texto (OLED) + audio
```

1. Mientras mantienes el botón → trozos de 100 ms (PCM 16 kHz / 16 bits / mono) van en vivo a Transcribe.
2. Sueltas el botón → evento vacío = fin del audio → texto final en ~0.2 s.
3. Strands → (Polly) → respuesta en streaming.

## Decisiones

| Tema | Decisión | Motivo |
|---|---|---|
| Activación | Botón (BOOT/GPIO9 mientras no haya botón) | El C3 no puede detectar palabra de activación |
| Agente | Strands en AgentCore Runtime | Hosting administrado, sesiones, streaming |
| Modelo | `amazon.nova-micro-v1:0` (on-demand) | El Nova más barato; soporta tools. Si se queda corto → `amazon.nova-lite-v1:0` |
| `<thinking>` de Nova | Se filtra con regex en `limpiar()` | Nova lo escribe en voz alta; pedirlo en el prompt no es confiable |
| Transcribe | `aws-sdk-transcribe-streaming` (Python 3.12, transporte CRT, idioma es-US) | boto3 no soporta streaming; SDK en preview → versión fijada en `uv.lock` |
| Envío de audio | En vivo mientras se graba (no grabar todo y mandar al final) | Transcribe solo es estable a ritmo real; mandar de golpe se traba. Latencia tras soltar: 4 s → 0.2 s |
| Memoria | En RAM de la sesión (sin AgentCore Memory) | Costo $0; se pierde tras ~15 min de inactividad |
| Historial | `SlidingWindowConversationManager(window_size=10)` | Limita los tokens por turno |
| Herramientas | Solo `hora_actual` (zona `America/Mexico_City`, pendiente de confirmar) | MVP |
| Identidad del dispositivo | IoT Core: cert X.509 → credenciales temporales → SigV4 | No guardar llaves AWS en el chip |
| Cuenta / región | Perfil `isengard`, us-east-1 | Nova Micro y Nova Sonic on-demand + AgentCore |

## Pasos

1. ✅ Agente Strands local con texto en consola → `cd agent && AWS_PROFILE=isengard uv run agent.py`
2. ✅ + Transcribe con el micrófono de la Mac → `AWS_PROFILE=isengard uv run voz.py`
3. Firmware: grabar con el micrófono y mostrar estados en la OLED.
4. ESP32 → agente en la laptop por WebSocket (solo LAN, temporal).
5. Despliegue en AgentCore + IoT Core/SigV4.
6. + Polly y reproducción I2S (cuando llegue el ampli).

## Pendientes / riesgos

- Futuro: Nova 2 Sonic (voz a voz, Strands BidiAgent) disponible en us-east-1 → reemplazaría Transcribe + Polly.
- Futuro: ESP32-S3 para palabra de activación.
- Si la red del SDK se traba, una tarea de awscrt queda huérfana (ver `voz.py`); vigilar memoria en AgentCore.
- Crear AWS Budget con alerta.
