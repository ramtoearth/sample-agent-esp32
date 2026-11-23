"""Entrypoint para AgentCore Runtime (protocolo HTTP: /ping, /invocations y WebSocket /ws en el puerto 8080).

Protocolo del WebSocket, por pregunta:
  cliente → servidor: frames binarios con PCM 16 kHz/16 bits/mono (100 ms c/u), luego el texto "fin"
  servidor → cliente: {"tipo": "transcripcion", "texto": ...} y {"tipo": "respuesta", "texto": ...}
                      o {"tipo": "error", "texto": ...}
La conexión sigue abierta para más preguntas. El agente recuerda mientras dure la sesión
(mismo X-Amzn-Bedrock-AgentCore-Runtime-Session-Id = misma microVM).

Local: `AWS_PROFILE=isengard uv run servidor.py` (escucha solo en 127.0.0.1:8080; ver probar_servidor.py).
"""

import asyncio
import logging

from bedrock_agentcore.runtime import BedrockAgentCoreApp
from starlette.websockets import WebSocket

from agent import responder
from voz import transcribir_en_vivo, trozos_de_cola

logging.basicConfig(level=logging.INFO)
log = logging.getLogger("servidor")
app = BedrockAgentCoreApp()


@app.websocket
async def conversar(ws: WebSocket, context):
    await ws.accept()
    while True:
        # 1. Esperar el primer trozo: Transcribe corta a los 15 s sin audio, así que no se abre antes.
        msg = await ws.receive()
        if msg["type"] == "websocket.disconnect":
            return
        if not msg.get("bytes"):
            continue  # un "fin" sin audio, o texto desconocido: se ignora

        # 2. Transcribir en vivo mientras siguen llegando trozos.
        cola: asyncio.Queue = asyncio.Queue()
        cola.put_nowait(msg["bytes"])
        transcripcion = asyncio.create_task(transcribir_en_vivo(trozos_de_cola(cola)))
        while True:
            msg = await ws.receive()
            if msg["type"] == "websocket.disconnect":
                cola.put_nowait(None)  # que Transcribe termine limpio; nadie espera el resultado
                return
            if msg.get("bytes"):
                cola.put_nowait(msg["bytes"])
            elif msg.get("text") == "fin":
                cola.put_nowait(None)
                break

        # 3. Texto → agente → respuesta.
        try:
            texto = await transcripcion
            await ws.send_json({"tipo": "transcripcion", "texto": texto})
            # El agente bloquea ~1 s: en otro hilo, para que /ping siga respondiendo.
            respuesta = await asyncio.to_thread(responder, texto) if texto else "No te escuché, intenta de nuevo."
            await ws.send_json({"tipo": "respuesta", "texto": respuesta})
            log.info("pregunta respondida (%d caracteres)", len(respuesta))  # sin contenido: puede ser personal
        except Exception as e:
            log.exception("fallo al responder")
            await ws.send_json({"tipo": "error", "texto": str(e)})


if __name__ == "__main__":
    app.run()  # 127.0.0.1 en la Mac; 0.0.0.0 solo dentro de un contenedor (lo decide el SDK)
