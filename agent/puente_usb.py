"""Puente USB: ESP32 ↔ Transcribe + agente, con la laptop en medio (temporal, hasta tener WiFi).

Uso: AWS_PROFILE=isengard uv run puente_usb.py [puerto]   (por defecto el primer /dev/cu.usbmodem*)
Protocolo:  ESP32 → laptop: "<<AUDIO", "A:<pcm en hex>" cada 100 ms, ">>AUDIO"; el resto son logs.
            laptop → ESP32: "R:<respuesta>"
"""

import asyncio
import glob
import os
import sys
import threading

import serial  # pyserial (dependencia --dev)

from agent import responder
from voz import transcribir_en_vivo, trozos_de_cola


async def principal(esp: serial.Serial):
    loop = asyncio.get_running_loop()
    lineas: asyncio.Queue[str] = asyncio.Queue()

    def leer():  # pyserial bloquea: se lee en otro hilo y se pasa al loop (como el micrófono en voz.py)
        while True:
            if linea := esp.readline():
                loop.call_soon_threadsafe(lineas.put_nowait, linea.decode(errors="replace").rstrip())

    threading.Thread(target=leer, daemon=True).start()

    trozos = transcripcion = None  # mientras hay grabación: cola de audio y tarea de Transcribe
    while True:
        linea = await lineas.get()
        if linea == "<<AUDIO":
            trozos = asyncio.Queue()
            transcripcion = asyncio.create_task(transcribir_en_vivo(trozos_de_cola(trozos)))  # empieza ya
            print("\n🎙  escuchando...")
        elif linea.startswith("A:") and trozos is not None:
            trozos.put_nowait(bytes.fromhex(linea[2:]))
        elif linea == ">>AUDIO" and trozos is not None:
            trozos.put_nowait(None)  # fin del audio
            try:
                texto = await transcripcion
            except Exception as e:
                print("Error de Transcribe:", e)
                texto = ""
            trozos = transcripcion = None
            print("Tú:", texto or "(no se entendió)")
            try:
                # El agente bloquea (~1 s): en otro hilo, para seguir recibiendo logs mientras tanto.
                respuesta = await asyncio.to_thread(responder, texto) if texto else "No te escuché, intenta de nuevo."
            except Exception as e:
                respuesta = f"Error del agente: {e}"
            print("Agente:", respuesta)
            esp.write(("R:" + respuesta.replace("\n", " ") + "\n").encode())
        elif linea:
            print(linea)


if __name__ == "__main__":
    puerto = sys.argv[1] if len(sys.argv) > 1 else sorted(glob.glob("/dev/cu.usbmodem*"))[0]
    print(f"Puente en {puerto} (Ctrl+C para salir). Mantén BOOT presionado y habla.")
    with serial.Serial(puerto, timeout=1) as esp:
        try:
            asyncio.run(principal(esp))
        except KeyboardInterrupt:
            os._exit(0)  # salir sin esperar tareas de awscrt trabadas (ver voz.py)
