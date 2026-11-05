#!/usr/bin/env python3
"""Imprime 'x y w h' (posición y tamaño) de la ventana 'captura'.
Si la ventana todavía no existe, no imprime nada."""
import json
import subprocess
import sys

try:
    clients = json.loads(subprocess.run(
        ["hyprctl", "clients", "-j"], capture_output=True, text=True, timeout=5).stdout)
except Exception:
    sys.exit(0)

for c in clients:
    if c.get("class") == "captura" and c.get("mapped"):
        x, y = c["at"]
        w, h = c["size"]
        print(x, y, w, h)
        sys.exit(0)
