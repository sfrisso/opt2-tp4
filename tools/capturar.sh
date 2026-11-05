#!/usr/bin/env bash
# capturar.sh — Abre una terminal (kitty) en el espacio de trabajo 9 con una
# sesión de tmux, ejecuta un guión de órdenes dentro de ella y toma una
# captura de la ventana con grim.
#
# Uso: capturar.sh <salida.png> <ordenes.sh>
# Variables: ANCHO (ancho útil a capturar), FUENTE, ENTRE, ESPERA, DESPUES.
#
# Formato de <ordenes.sh> (una orden por línea):
#   #wait N        -> esperar N segundos
#   #esperar N     -> esperar hasta N segundos a que el shell vuelva al prompt
#   #split         -> dividir la ventana en dos paneles (arriba/abajo)
#   #focus N       -> pasar el foco al panel índice N (0-based)
#   #key CLAVE     -> enviar una tecla (p.ej. C-c, C-d, Enter)
#   #ya            -> la siguiente orden se escribe SIN esperar al prompt
#                     (para programas que siguen corriendo y leen stdin,
#                     como el cliente del eco)
#   #fin           -> no esperar al prompt al terminar el guión (para cuando
#                     el último programa sigue corriendo a propósito)
#   cualquier otra -> se escribe como orden y se pulsa Intro
set -Eeuo pipefail
# Con -E el ERR trap se hereda en funciones: si algo falla en silencio
# (grim, magick, geometría…) se avisa en lugar de morir sin mensaje.
trap 'echo "ERROR: comando falló en línea ${BASH_LINENO[0]:-?} (código $?) de $0" >&2' ERR

OUT=$1
ORDENES=$2
WS=${WS:-9}                 # espacio de trabajo donde se ejecutan las pruebas
ANCHO=${ANCHO:-1300}        # ancho de la región que se captura (px)
ALTO_MAX=${ALTO_MAX:-1060}  # alto máximo de la región capturada
ENTRE=${ENTRE:-1.3}         # segundos entre órdenes
FUENTE=${FUENTE:-22}
HERE=$(cd "$(dirname "$0")" && pwd)

SESSION="cap$$"
SOCK="captura$$"          # socket tmux dedicado (aislado del tmux del usuario)
tmuxq() { tmux -L "$SOCK" "$@"; }

# Espera a que el panel activo vuelva al prompt del shell (zsh), es decir,
# a que termine la orden que se le estaba ejecutando. El primer parámetro
# es el máximo de segundos a esperar (por defecto 8).
esperar_prompt() {
    local max=${1:-8} t=0 cmd
    while (( t < max * 2 )); do
        cmd=$(tmuxq display-message -p -t "$SESSION" '#{pane_current_command}' 2>/dev/null || true)
        case "$cmd" in
            *zsh*|*"kitty"*) return 0 ;;
        esac
        sleep 0.5
        t=$((t + 1))
    done
    return 1
}

hypr() { hyprctl dispatch "$1" 2>/dev/null || true; }

cleanup() {
    kill "${KITTY_PID:-}" 2>/dev/null || true
    tmuxq kill-server 2>/dev/null || true
    # volver al espacio de trabajo en el que estaba el usuario
    hypr "hl.dsp.focus({ workspace = \"$WS_ORIG\" })"
}
trap cleanup EXIT

WS_ORIG=$(hyprctl activeworkspace -j | python3 -c 'import json,sys; print(json.load(sys.stdin)["id"])')

# Ir al espacio de trabajo de pruebas (vacío => la ventana ocupa la pantalla)
hypr "hl.dsp.focus({ workspace = \"$WS\" })"
sleep 1

# Sesión de tmux que veremos en la terminal. El -f hace que el shell limpio
# (sin los plugins del shell del usuario) se use desde el primer prompt.
tmuxq -f "$HERE/tmux.conf" new-session -d -s "$SESSION" -x 220 -y 60 -c "${PWD}"

kitty --class captura --title "Tema 4 - Sockets" \
      -o font_size="$FUENTE" -o cursor_blink_interval=0 \
      -o background_opacity=1.0 \
      -- tmux -L "$SOCK" attach -t "$SESSION" >/dev/null 2>&1 &
KITTY_PID=$!

# Esperar a que la ventana exista
GEO=""
for _ in $(seq 1 80); do
    GEO=$(python3 "$HERE/geometria.py" || true)
    if [[ -n "$GEO" ]]; then break; fi
    sleep 0.25
done
if [[ -z "$GEO" ]]; then
    echo "ERROR: no apareció la ventana de kitty" >&2
    exit 1
fi
echo "ventana kitty: $GEO"
sleep 1

# Ejecutar las órdenes
while IFS= read -r line || [[ -n "$line" ]]; do
    case "$line" in
        "#wait "*)  sleep "${line#'#wait '}"; continue ;;
        "#esperar "*) esperar_prompt "${line#'#esperar '}"; continue ;;
        "#split")   tmuxq split-window -t "$SESSION" -v; sleep 0.6; continue ;;
        "#focus "*) tmuxq select-pane -t "$SESSION:0.${line#\#focus }"; sleep 0.3; continue ;;
        "#key "*)   tmuxq send-keys -t "$(tmuxq display-message -p -t "$SESSION" '#{pane_id}')" "${line#\#key }"; sleep 0.5; continue ;;
        '#'*|"")    continue ;;
        *)          # esperar a que el shell esté en el prompt antes de escribir
                    esperar_prompt "${ESPERA_ORDEN:-45}"
                    P=$(tmuxq display-message -p -t "$SESSION" '#{pane_id}')
                    tmuxq send-keys -t "$P" -l "$line"; tmuxq send-keys -t "$P" Enter
                    sleep "$ENTRE" ;;
    esac
done < "$ORDENES"

esperar_prompt "${ESPERA_FIN:-120}"
sleep "${DESPUES:-2.5}"

# Leer la geometría final, enfocar la ventana y capturar
read -r X Y W H <<< "$(python3 "$HERE/geometria.py")"
W=$ANCHO
if (( H > ALTO_MAX )); then H=$ALTO_MAX; fi
hypr "hl.dsp.focus({ window = \"class:captura\" })"
sleep 0.6

# Capturar. A veces otra ventana del escritorio se superpone al borde derecho
# y aparece como una línea vertical de color en la imagen: si eso ocurre,
# se espera un poco y se vuelve a capturar (hasta 3 intentos).
for intento in 1 2 3; do
    grim -g "$X,$Y ${W}x${H}" "$OUT"
    if ! python3 "$HERE/artefacto.py" "$OUT"; then
        break
    fi
    echo "intento $intento: hay una línea ajena superpuesta, repito en 4 s" >&2
    sleep 4
done

# Quitar el espacio en blanco que sobra por debajo/a la derecha del texto
magick "$OUT" -fuzz 1% -trim +repage "$OUT"
identify -format "captura guardada en $OUT (%wx%h)\n" "$OUT"
