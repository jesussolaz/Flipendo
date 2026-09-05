#!/bin/bash
# flipendo — gestor de versiones del motor Flipendo (Mac Intel)
set -euo pipefail
ROOT="$HOME/Flipendo"; VDIR="$ROOT/versions"; APPS="/Applications"
ACTIVE_FILE="$ROOT/.active"
c_b=$'\033[1m'; c_g=$'\033[32m'; c_y=$'\033[33m'; c_r=$'\033[31m'; c_0=$'\033[0m'

die(){ echo "${c_r}error:${c_0} $*" >&2; exit 1; }
active(){ [ -f "$ACTIVE_FILE" ] && basename "$(cat "$ACTIVE_FILE")" || echo ""; }
exists(){ [ -d "$VDIR/$1" ]; }
appin(){ find "$1" -maxdepth 1 -name "*.app" -not -name "*Player.app" | head -1; }
protected(){ [ -f "$VDIR/$1/VERSION.json" ] && grep -q '"protegido": *true' "$VDIR/$1/VERSION.json"; }

cmd_list(){
  local a; a=$(active)
  printf "${c_b}%-16s %-10s %-10s %-12s %s${c_0}\n" VERSION UPBGE BLENDER TAMANO ESTADO
  for d in "$VDIR"/*/; do
    [ -d "$d" ] || continue
    local n; n=$(basename "$d")
    local j="$d/VERSION.json" u="?" b="?"
    if [ -f "$j" ]; then
      u=$(python3 -c "import json;print(json.load(open('$j')).get('base_upbge','?'))" 2>/dev/null || echo "?")
      b=$(python3 -c "import json;print(json.load(open('$j')).get('base_blender','?'))" 2>/dev/null || echo "?")
    fi
    local sz; sz=$(du -sh "$d" 2>/dev/null | cut -f1)
    local st=""
    [ "$n" = "$a" ] && st="${c_g}<- ACTIVA${c_0}"
    protected "$n" && st="$st ${c_y}[protegida]${c_0}"
    printf "%-16s %-10s %-10s %-12s %b\n" "$n" "$u" "$b" "$sz" "$st"
  done
  echo; echo "espacio real en disco (clones APFS comparten bloques):"
  du -sh "$VDIR" 2>/dev/null | awk '{print "  suma logica: " $1}'
  df -h / | tail -1 | awk '{print "  libre en /:  " $4}'
}

cmd_snapshot(){
  local name="${1:-}"; shift || true
  local msg=""; [ "${1:-}" = "-m" ] && { msg="${2:-}"; }
  [ -n "$name" ] || die "uso: flipendo snapshot <nombre> [-m \"mensaje\"]"
  exists "$name" && die "la version '$name' ya existe"
  local app; app=$(appin "$APPS" 2>/dev/null || true)
  [ -d "$APPS/Flipendo.app" ] || die "no encuentro /Applications/Flipendo.app"
  mkdir -p "$VDIR/$name"
  echo "clonando estado actual de /Applications -> $name ..."
  cp -c -R "$APPS/Flipendo.app" "$VDIR/$name/Flipendo.app"
  [ -d "$APPS/Flipendo Player.app" ] && cp -c -R "$APPS/Flipendo Player.app" "$VDIR/$name/Flipendo Player.app"
  local ver; ver=$(/usr/libexec/PlistBuddy -c "Print :CFBundleShortVersionString" "$VDIR/$name/Flipendo.app/Contents/Info.plist" 2>/dev/null || echo "?")
  python3 - "$VDIR/$name/VERSION.json" "$name" "$(active)" "$msg" "$ver" <<'PY'
import json,sys,subprocess
out,name,padre,msg,ver = sys.argv[1:6]
fecha = subprocess.check_output(["date","+%Y-%m-%d %H:%M"]).decode().strip()
json.dump({"nombre":name,"titulo":f"UPBGE Key {ver}","fecha":fecha,"padre":padre,
           "version_bundle":ver,"mensaje":msg,"modificaciones":[],"protegido":False},
          open(out,"w"), indent=2, ensure_ascii=False)
PY
  echo "${c_g}snapshot creado:${c_0} $name  (coste en disco ~0, clon APFS)"
}

cmd_activate(){
  local name="${1:-}"; [ -n "$name" ] || die "uso: flipendo activate <nombre>"
  exists "$name" || die "no existe la version '$name'  (flipendo list)"
  pgrep -f "Flipendo.app/Contents/MacOS/Blender" >/dev/null && die "cierra UPBGE Key antes de cambiar de version"
  local src="$VDIR/$name"
  local mainapp; mainapp=$(find "$src" -maxdepth 1 -name "*.app" -not -name "*Player.app" | head -1)
  [ -n "$mainapp" ] || die "la version '$name' no contiene ningun .app"
  echo "activando $name ..."
  rm -rf "$APPS/Flipendo.app" "$APPS/Flipendo Player.app"
  cp -c -R "$mainapp" "$APPS/Flipendo.app"
  local pl; pl=$(find "$src" -maxdepth 1 -name "*Player.app" | head -1)
  [ -n "$pl" ] && cp -c -R "$pl" "$APPS/Flipendo Player.app"
  xattr -dr com.apple.quarantine "$APPS/Flipendo.app" 2>/dev/null || true
  [ -d "$APPS/Flipendo Player.app" ] && xattr -dr com.apple.quarantine "$APPS/Flipendo Player.app" 2>/dev/null || true
  echo "$src" > "$ACTIVE_FILE"
  echo "${c_g}activa:${c_0} $name"
}

cmd_info(){
  local name="${1:-$(active)}"; exists "$name" || die "no existe '$name'"
  [ -f "$VDIR/$name/VERSION.json" ] && python3 -m json.tool "$VDIR/$name/VERSION.json" || echo "(sin VERSION.json)"
}

cmd_diff(){
  local a="${1:-}" b="${2:-}"; [ -n "$a" ] && [ -n "$b" ] || die "uso: flipendo diff <a> <b>"
  exists "$a" || die "no existe '$a'"; exists "$b" || die "no existe '$b'"
  echo "${c_b}ficheros que difieren entre $a y $b:${c_0}"
  diff -rq "$VDIR/$a" "$VDIR/$b" 2>/dev/null | sed 's|'"$VDIR"'/||g' | head -60
  echo "(solo se listan los primeros 60)"
}

cmd_delete(){
  local name="${1:-}"; [ -n "$name" ] || die "uso: flipendo delete <nombre>"
  exists "$name" || die "no existe '$name'"
  protected "$name" && die "'$name' esta protegida; edita su VERSION.json si de verdad quieres borrarla"
  [ "$name" = "$(active)" ] && die "'$name' es la version activa"
  read -r -p "borrar '$name' definitivamente? [s/N] " r
  [ "$r" = "s" ] || { echo "cancelado"; exit 0; }
  rm -rf "${VDIR:?}/$name"; echo "borrada"
}

cmd_run(){ open -a "$APPS/Flipendo.app" "$@"; }
cmd_status(){ echo "activa: $(active)"; echo "raiz:   $ROOT"; ls -d "$APPS/Flipendo.app" 2>/dev/null || echo "(no instalada en /Applications)"; }

case "${1:-help}" in
  list|ls) shift; cmd_list "$@";;
  snapshot|snap) shift; cmd_snapshot "$@";;
  activate|use) shift; cmd_activate "$@";;
  info) shift; cmd_info "$@";;
  diff) shift; cmd_diff "$@";;
  delete|rm) shift; cmd_delete "$@";;
  run) shift; cmd_run "$@";;
  status) shift; cmd_status "$@";;
  *) cat <<EOF
${c_b}upbge-key${c_0} — gestor de versiones de UPBGE Key (Mac Intel)

  flipendo list                       lista las versiones
  flipendo snapshot <n> [-m "msg"]    congela el estado actual como version nueva
  flipendo activate <n>               instala esa version en /Applications
  flipendo info [n]                   metadatos de una version
  flipendo diff <a> <b>               ficheros que cambian entre dos versiones
  flipendo delete <n>                 borra una version
  flipendo run                        abre UPBGE Key
  flipendo status                     version activa

Los snapshots usan clones APFS: cada uno cuesta ~0 bytes hasta que los ficheros difieren.
EOF
  ;;
esac
