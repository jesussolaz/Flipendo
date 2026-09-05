# UPBGE Key — registro de versiones

Línea propia de UPBGE para **MacBookPro16,1** (i7-9750H · Radeon Pro 5300M · macOS 26.3.1, x86_64).

## Por qué existe esta línea

Blender retiró el soporte de **macOS Intel en la versión 5.0**: la 4.5 LTS es la última que
publica `macos-x64`, y de 5.0 en adelante solo hay `macos-arm64`. Como UPBGE 0.50 está basado
en Blender 5.0.1, **no existe ni existirá un build oficial de UPBGE posterior a 0.44 para Intel**.

UPBGE 0.44 (base Blender 4.4.3) es por tanto el techo oficial en este equipo, y el punto de
partida de nuestra propia línea.

---

## key-1.0 — «UPBGE Key 1.0» · 2026-09-04

Primera versión de la línea. Base: `0.44-vanilla`, sin recompilar.

**Cambios**
- Identidad propia: `CFBundleName` = `UPBGE Key`, bundle id `org.upbge.key` (player: `org.upbge.key.player`).
- Versionado propio: `CFBundleShortVersionString` = `1.0`, independiente del `4.4.3` de base.
- `NSSupportsAutomaticGraphicsSwitching = false` — fuerza explícitamente la **Radeon Pro 5300M**
  discreta. Antes funcionaba por *ausencia* de la clave; ahora es intencionado y no se puede
  perder por accidente en un build futuro.
- **Firma ad-hoc** (`codesign -s -`). El build oficial venía sin firmar. Con identidad estable,
  macOS no trata cada build modificado como una app nueva y no vuelve a pedir permisos de
  disco/cámara/micro en cada iteración.
- Cuarentena de Gatekeeper retirada.

**Verificado**
- Arranca en GUI y en `--background`.
- Backend **Metal** (es el único que soporta este build: `--gpu-backend` sólo acepta `metal`;
  OpenGL ya no está compilado).
- Escritura/lectura de `.blend` correcta.

**Sin cambios**
- Binarios y librerías idénticos al 0.44 oficial. Esto **no** es una recompilación.

---

## 0.44-vanilla — referencia prístina · protegida

UPBGE 0.44 oficial tal cual salió del DMG. Punto de retorno; no se modifica nunca.

- Origen: `upbge-0.44-macos-x86_64.dmg` (release oficial v0.44)
- SHA-512 verificado: `b943b29a…7b31a2da`
- Build upstream: 2025-05-05, rama `upbge-v0.44-release`, hash `22781af88461`

---

## Uso

```
upbge-key list                     ver versiones
upbge-key snapshot <n> -m "..."    congelar el estado actual como versión nueva
upbge-key activate <n>             instalar esa versión en /Applications
upbge-key diff <a> <b>             qué ficheros cambian entre dos versiones
upbge-key run                      abrir UPBGE Key
```

Los snapshots son **clones APFS**: cada uno cuesta ~0 bytes hasta que los ficheros difieren.

---

## Fase 0/4 del plan maestro · 2026-09-05

**Fase 0 (preparación) — casi completa**
- Material de sesión rescatado del scratchpad volátil a `dev/refs/` (112 MB: benchmarks, scripts de verificación, notas) y `addons/keyfx/`.
- Toolchain sin Homebrew en `dev/toolchain/`: CMake 3.31.6, ninja 1.12.1, git-lfs 3.7.0 (LFS inicializado). En PATH vía ~/.zshrc.
- Clones en curso: fuente UPBGE (historia completa, base fork `3c7b891a`) y libs `lib-macos_x64` rama `blender-v4.5-release`.
- PENDIENTE: remotes de GitHub (no hay `gh` instalado — requiere login del usuario).

**Fase 4 (Key Assistant) — MVP funcional**
- Addon `key_assistant` v0.1.0 (386 líneas) instalado y habilitado de forma persistente en UPBGE 4.4.
- Panel en Vista 3D > N > "Key Assistant": chat, adjuntar imagen/URL, captura de viewport ("ojos"), confirmación antes de ejecutar (por defecto), autosave + undo por cada ejecución, lista negra de llamadas peligrosas, autocorrección de errores (hasta 3 intentos).
- Backend: binario `claude` de la extensión VS Code (auth existente del usuario), `-p --output-format json` + `--resume` para continuidad de sesión.
- `keyfx` instalado como módulo (`scripts/modules/keyfx.py`).
- **Verificado ciclo completo headless**: petición "crea una esfera TEST_KA…" → Claude → bloque bpy-exec → ejecución → esfera con material toon en escena. CICLO_COMPLETO_OK.

## Fase 5.2 — Plantilla "Key ARPG" funcional · 2026-09-05

- `game/template/components/arpg_core.py`: lógica pura testeada fuera del motor (combos con ventanas de encadenado y buffer de input, lock-on por cono, salud con i-frames, cerebro enemigo). 5/5 tests unitarios.
- `game/template/components/arpg.py` (297 líneas): componentes UPBGE — PlayerController (WASD relativo a cámara, dash, salto, combo x3, aplicar golpes por alcance+ángulo), ThirdPersonCamera (órbita con ratón, colisión por raycast, encuadre lock-on, autosanado de cámara activa), EnemyAI (percibir→perseguir→atacar, knockback, muerte).
- `game/template/KeyARPG.blend` generado por script (GUI + autocierre): arena, jugador CHARACTER, 3 enemigos, componentes registrados, 60fps.
- **7/7 tests de runtime en blenderplayer**: componentes viven, enemigo muere al golpe, jugador recibe daño, cámara activa y siguiendo, enemigo persigue.
- Bug conocido de UPBGE esquivado: registro de componentes/sensores crashea en `--background`; el generador corre en GUI con salida por timer.

## key-2.0 — 2026-09-05
Motor propio compilado desde fuente en este Mac (rama `key-fase3-filtros`, base Blender 4.5).
- Filtros 2D funcionando en Metal: presets y custom con sintaxis nueva (`fragColor`, `texture()`, `bgl_TexCoord`). Verificado con capturas.
- `bge.texture`/VideoTexture restaurado: ImageRender a textura de material, probado con escena CCTV (pantalla que muestra otra cámara).
- InitTextures por árbol de nodos: adiós al "Texture is not available".
- Pendiente: traductor de sintaxis GLSL vieja; segfault al cerrar player con Texture activa; feedback cámara-objeto descartado por Metal (limitación documentada).

## key-2.1 — 2026-09-05
- Filtros 2D custom aceptan la sintaxis GLSL antigua (`gl_FragColor`, `texture2D()`, `gl_TexCoord[0]` con `.st`/`.xy`): los tutoriales pre-0.50 funcionan tal cual.
- Arreglado el segfault al cerrar el player con `ImageRender` activo (use-after-free en el GC final de Python). 3/3 cierres limpios.

## flipendo-2.1 — 2026-09-05 (Rebautizo integral)
Capa de identidad renombrada de "UPBGE Key" a **Flipendo**:
- Apps: `Flipendo.app` y `Flipendo Player.app` (bundle `org.flipendo.flipendo` / `org.flipendo.player`).
- Carpeta de trabajo: `~/Flipendo` (antes `~/UPBGE-Key`); todas las rutas absolutas corregidas (symlink de libs, build cache, .zshrc, .active).
- Gestor de versiones: comando `flipendo` (antes `upbge-key`).
- Motor interno (ejecutable, `--version`, config) sigue siendo Blender/UPBGE: es funcional y lo exige la GPL. Pendiente opcional: título de ventana y splash propios (requiere recompilar).
