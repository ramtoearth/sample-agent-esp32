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
| Transporte del audio | WebSocket (no MQTT) | AgentCore acepta WebSocket; con MQTT haría falta un worker 24/7 para sostener el stream de Transcribe |
| Base: [aws-iot-esp32-example](https://github.com/ramtoearth/aws-iot-esp32-example) | Se reutiliza: Terraform (Thing + cert + política mínima), secretos en archivo ignorado por git, certs incrustados al compilar, opciones de depuración en `sdkconfig.defaults` | Ya probado por el usuario |
| Firmware | ESP-IDF v5.4.2; OLED con esp_lcd + fuente 5×7 propia; consola por USB-Serial-JTAG | Sin dependencias externas; UART0 (GPIO20/21) libre para la OLED. Ajustes en `firmware/sdkconfig.defaults` |
| Infraestructura | CDK en Python (`infra/`), un solo stack `Asistente` | Fuente única de verdad: `cdk deploy`/`destroy` crea/borra todo |
| Llave del dispositivo | ECC P-256 generada en la Mac; a AWS solo va la CSR | La llave privada nunca sale de la Mac ni pasa por CloudFormation |
| Protocolo del WebSocket | Binario = PCM de 100 ms; texto `fin`; respuestas JSON `{tipo, texto}` | La misma conexión sirve para varias preguntas; Transcribe arranca con el primer trozo |
| Runtime | Red PUBLIC, invocación solo con SigV4 (403 sin firma), logs con retención de 7 días | Sin costo fijo; VPC costaría ~$15–35 al mes |
| Conexión del ESP32 | URL prefirmada + WebSocket siempre abierto; reconexión propia (no la de la librería) | La latencia de TLS (~1.6 s) se paga al arrancar; la URL caduca a los 5 min pero solo importa al conectar |
| Cuenta / región | Perfil `isengard`, us-east-1 | Nova Micro y Nova Sonic on-demand + AgentCore |

## Pasos

1. ✅ Agente Strands local con texto en consola → `cd agent && AWS_PROFILE=isengard uv run agent.py`
2. ✅ + Transcribe con el micrófono de la Mac → `AWS_PROFILE=isengard uv run voz.py`
3. ✅ Firmware: micrófono I2S + OLED + botón BOOT (la prueba con puente USB quedó en el historial: commit `64fff65`)
4. ESP32 → AWS directo (sin servidor en la LAN: sería accesible a toda la red WiFi sin auth estándar):
   - ✅ 4a WiFi · ✅ 4b credenciales temporales de IoT Core desde el ESP32 (TLS mutuo + SNTP)
   - ✅ 4c agente en AgentCore (contenedor arm64 con Finch, mismo stack de CDK; probado con `probar_servidor.py --arn`)
   - ✅ 4d el ESP32 abre el WebSocket con URL prefirmada (SigV4 en C, `firmware/main/sigv4.c`) y lo mantiene abierto; reconecta con URL nueva
5. + Polly y reproducción I2S (cuando llegue el ampli).

## Pendientes / riesgos

- Futuro: Nova 2 Sonic (voz a voz, Strands BidiAgent) disponible en us-east-1 → reemplazaría Transcribe + Polly.
- Futuro: ESP32-S3 para palabra de activación.
- Si la red del SDK se traba, una tarea de awscrt queda huérfana (ver `voz.py`); vigilar memoria en AgentCore.
- Nivel RMS en silencio alto (3000–14000) aunque el audio suena bien: revisar offset de DC.
- WiFi + WebSocket agregan ~600 KB: agrandar la partición de la app (flash de 4 MB).
- La llave privada va incrustada en la flash: activar **flash encryption** antes de dar el dispositivo por terminado. Si se pierde la placa: revocar el certificado en IoT Core.
- Al final del proyecto: borrar lo que quedó del agente viejo *nimbo* en us-west-2 (rol IAM `nimbo-mcpon-review-runtime-role` y repo ECR `nimbo-mcpon-review-agent`; el runtime ya se borró).
- Crear AWS Budget con alerta.
