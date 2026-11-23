"""Cliente de prueba del WebSocket de servidor.py: simula al ESP32 con un archivo de audio.

Local:     uv run probar_servidor.py audio.wav
AgentCore: AWS_PROFILE=isengard uv run probar_servidor.py audio.wav --arn <ARN del runtime>
El audio: WAV o PCM crudo de 16 kHz, 16 bits, mono.
"""

import argparse
import asyncio
import json
import time
import uuid
import wave

import websockets

TROZO = 3200  # 100 ms

p = argparse.ArgumentParser()
p.add_argument("audio")
p.add_argument("--arn", help="ARN del runtime en AgentCore (si falta: servidor local)")
args = p.parse_args()

if args.audio.endswith(".wav"):
    with wave.open(args.audio) as w:
        pcm = w.readframes(w.getnframes())
else:
    pcm = open(args.audio, "rb").read()

if args.arn:
    from bedrock_agentcore.runtime import AgentCoreRuntimeClient

    # Firma SigV4 con las credenciales del perfil; el session id elige la microVM (y su memoria).
    url, headers = AgentCoreRuntimeClient(region="us-east-1").generate_ws_connection(
        runtime_arn=args.arn, session_id=f"prueba-{uuid.uuid4()}"
    )
else:
    url, headers = "ws://127.0.0.1:8080/ws", {}


async def main():
    async with websockets.connect(url, additional_headers=headers, max_size=None) as ws:
        t0 = time.time()
        for i in range(0, len(pcm), TROZO):  # a velocidad real, como el micrófono
            await ws.send(pcm[i : i + TROZO])
            await asyncio.sleep(0.1)
        await ws.send("fin")
        t_fin = time.time()
        print(f"audio enviado: {len(pcm) / 32000:.1f} s en {t_fin - t0:.1f} s")
        for _ in range(2):
            m = json.loads(await ws.recv())
            print(f"[+{time.time() - t_fin:.1f} s tras 'fin'] {m['tipo']}: {m['texto']}")
            if m["tipo"] == "error":
                break


asyncio.run(main())
