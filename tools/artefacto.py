#!/usr/bin/env python3
"""Devuelve 0 si la captura contiene una línea vertical ajena (ventana del
escritorio superpuesta al borde de la imagen), 1 en caso contrario.

La señal es una columna con una racha CONTINUA de píxeles de tinta (que no
sean el fondo del terminal) que ocupa más del 70 % de la altura: el texto
siempre tiene huecos entre líneas, de modo que nunca llega a eso.
"""
import re
import subprocess
import sys

if len(sys.argv) != 2:
    sys.exit(1)

salida = subprocess.run(["magick", sys.argv[1], "txt:-"],
                        capture_output=True, text=True).stdout

alto = 0
tinta = {}          # columna -> lista de filas con tinta
for linea in salida.splitlines()[1:]:
    if ':' not in linea:
        continue
    xy, resto = linea.split(':', 1)
    try:
        x, y = (int(v) for v in xy.split(','))
    except ValueError:
        continue
    alto = max(alto, y)
    m = re.search(r'\((\d+),(\d+),(\d+)', resto)
    if m:
        r, g, b = (int(v) for v in m.groups())
        if r + g + b > 60:              # no es el fondo negro del terminal
            tinta.setdefault(x, []).append(y)

limite = 0.7 * (alto + 1)
sospechosas = []
for x, filas in tinta.items():
    filas.sort()
    racha = mejor = 1
    for a, b in zip(filas, filas[1:]):
        racha = racha + 1 if b == a + 1 else 1
        mejor = max(mejor, racha)
    if mejor > limite:
        sospechosas.append(x)

if sospechosas:
    print(f"columnas con línea vertical continua: {sorted(sospechosas)[:5]}",
          file=sys.stderr)
    sys.exit(0)                         # hay artefacto
sys.exit(1)                             # captura limpia
