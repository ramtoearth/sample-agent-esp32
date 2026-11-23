import re
from datetime import datetime
from zoneinfo import ZoneInfo

from strands import Agent, tool
from strands.agent.conversation_manager import SlidingWindowConversationManager
from strands.models import BedrockModel

ZONA_HORARIA = "America/Mexico_City"


@tool
def hora_actual() -> str:
    """Devuelve la fecha y hora actual. Úsala SOLO si el usuario pregunta la hora, la fecha o el día."""
    return datetime.now(ZoneInfo(ZONA_HORARIA)).strftime("%Y-%m-%d %H:%M (%A)")


agent = Agent(
    model=BedrockModel(model_id="amazon.nova-micro-v1:0", region_name="us-east-1"),
    system_prompt=(
        "Te llamas Remi y eres un asistente de voz personal en español. Si te preguntan tu nombre "
        "o quién eres, di que eres Remi; si no, no te presentes. Responde en máximo 2 oraciones cortas, "
        "en texto plano, sin markdown, listas ni emojis: tu respuesta se leerá en voz alta "
        "y se mostrará en una pantalla pequeña. Puedes usar libremente cualquier dato "
        "que el usuario te haya dicho en esta conversación, como su nombre."
    ),
    tools=[hora_actual],
    # ponytail: solo los últimos 10 mensajes; súbelo si el agente olvida contexto útil
    conversation_manager=SlidingWindowConversationManager(window_size=10),
    callback_handler=None,  # no imprimir en streaming: el texto crudo trae <thinking>
)


def limpiar(texto: str) -> str:
    """Quita el razonamiento <thinking>…</thinking> que Nova escribe en voz alta."""
    return re.sub(r"<thinking>.*?</thinking>", "", texto, flags=re.DOTALL).strip()


def responder(texto: str) -> str:
    return limpiar(str(agent(texto)))


if __name__ == "__main__":
    while (pregunta := input("\nTú: ").strip()) not in ("", "salir"):
        print("Agente:", responder(pregunta))
