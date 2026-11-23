"""Voz ↔ texto. Formato de audio en todo el proyecto: PCM 16 kHz, mono, 16 bits (little-endian)."""

import asyncio
from collections.abc import AsyncIterator

import boto3
from aws_sdk_transcribe_streaming.aio.client import AsyncTranscribeStreamingClient
from aws_sdk_transcribe_streaming.aio.config import AsyncTranscribeStreamingConfig
from aws_sdk_transcribe_streaming.models import (
    AudioEvent,
    AudioStreamAudioEvent,
    StartStreamTranscriptionInput,
    TranscriptResultStreamTranscriptEvent,
)
from smithy_aws_core.aio.identity.static import AsyncStaticCredentialsResolver
from smithy_aws_core.identity import AWSCredentialsIdentity
from smithy_http.aio.crt import AWSCRTHTTPClient

REGION = "us-east-1"
IDIOMA = "es-US"
SAMPLE_RATE = 16000
CHUNK = SAMPLE_RATE * 2 // 10  # 100 ms de audio (2 bytes por muestra)


def _credenciales() -> AsyncStaticCredentialsResolver:
    # El SDK de Transcribe no lee perfiles ni `aws login`; boto3 sí (y en AgentCore, el rol IAM).
    c = boto3.Session().get_credentials().get_frozen_credentials()
    return AsyncStaticCredentialsResolver(
        AWSCredentialsIdentity(access_key_id=c.access_key, secret_access_key=c.secret_key, session_token=c.token)
    )


async def transcribir_en_vivo(trozos: AsyncIterator[bytes]) -> str:
    """Manda los trozos a Transcribe conforme llegan. Deben llegar a ritmo ~real:
    si se mandan de golpe, el envío HTTP/2 se traba de forma intermitente."""
    client = AsyncTranscribeStreamingClient(
        config=await AsyncTranscribeStreamingConfig.resolve(
            region=REGION,
            aws_credentials_identity_resolver=_credenciales(),
            transport=AWSCRTHTTPClient(),  # aiohttp (el default) no soporta streaming bidireccional
        )
    )
    stream = await client.start_stream_transcription(
        StartStreamTranscriptionInput(
            language_code=IDIOMA, media_sample_rate_hertz=SAMPLE_RATE, media_encoding="pcm"
        )
    )

    async def enviar():
        async for trozo in trozos:
            await stream.input_stream.send(AudioStreamAudioEvent(AudioEvent(audio_chunk=trozo)))
        # Un evento vacío es la señal de "fin del audio"; sin él Transcribe espera 15 s y corta.
        await stream.input_stream.send(AudioStreamAudioEvent(AudioEvent(audio_chunk=b"")))
        await stream.input_stream.close()

    async def recibir() -> str:
        _, eventos = await stream.await_output()
        frases = []
        async for evento in eventos:
            if isinstance(evento, TranscriptResultStreamTranscriptEvent):
                for r in evento.value.transcript.results:
                    if not r.is_partial:  # los parciales son borradores que Transcribe va corrigiendo
                        frases.append(r.alternatives[0].transcript)
        return " ".join(frases)

    envio = asyncio.create_task(enviar())
    texto = await recibir()  # si Transcribe manda un error, sale aquí sin esperar a `envio`
    await envio
    return texto


# ponytail: loop persistente en vez de asyncio.run(). Si la red se traba, una tarea interna de
# awscrt se queda escribiendo y no acepta cancelación; asyncio.run() la esperaría para siempre.
# Así el error llega al usuario y la tarea trabada queda huérfana (fuga pequeña, solo en errores).
_loop = asyncio.new_event_loop()


async def _desde_bloque(pcm: bytes) -> AsyncIterator[bytes]:
    """Reparte un audio ya grabado a velocidad real (lo único estable, ver docs)."""
    for i in range(0, len(pcm), CHUNK):
        yield pcm[i : i + CHUNK]
        await asyncio.sleep(0.1)


async def _desde_mic() -> AsyncIterator[bytes]:
    """Entrega trozos de 100 ms del micrófono hasta que el usuario presiona Enter."""
    import sounddevice as sd  # solo en la laptop (dependencia --dev)

    loop = asyncio.get_running_loop()
    cola: asyncio.Queue[bytes | None] = asyncio.Queue()
    # El callback corre en un hilo de audio: call_soon_threadsafe pasa el trozo al loop de forma segura.
    with sd.RawInputStream(samplerate=SAMPLE_RATE, channels=1, dtype="int16", blocksize=CHUNK // 2,
                           callback=lambda data, *_: loop.call_soon_threadsafe(cola.put_nowait, bytes(data))):
        # input() bloquearía el loop (y el envío); se espera en otro hilo.
        fin = loop.run_in_executor(None, input, "🎙  grabando... (Enter para terminar)")
        fin.add_done_callback(lambda _: cola.put_nowait(None))
        while (trozo := await cola.get()) is not None:
            yield trozo


def transcribir(pcm: bytes) -> str:
    return _loop.run_until_complete(transcribir_en_vivo(_desde_bloque(pcm)))


def escuchar() -> str:
    """Graba del micrófono y transcribe al mismo tiempo."""
    return _loop.run_until_complete(transcribir_en_vivo(_desde_mic()))


if __name__ == "__main__":
    from agent import responder

    while input("\nEnter para hablar ('q' + Enter para salir): ").strip() != "q":
        try:
            texto = escuchar()
        except Exception as e:
            print("Error de Transcribe:", e)
            continue
        print("Transcripción:", texto or "(no se entendió nada)")
        if texto:
            print("Agente:", responder(texto))
