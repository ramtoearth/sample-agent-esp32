from agent import limpiar

assert limpiar("<thinking>uso la tool</thinking> Son las 3.") == "Son las 3."
assert limpiar("<thinking>a\nb</thinking>Hola <thinking>x</thinking>Ana") == "Hola Ana"
assert limpiar("Sin etiquetas.") == "Sin etiquetas."
print("ok")
