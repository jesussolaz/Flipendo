# Flipendo

**Motor de juego derivado de [UPBGE](https://github.com/UPBGE/upbge), mantenido para macOS Intel (x86_64) con backend Metal funcional.**

Blender eliminó el soporte de macOS Intel en la versión 5.0, y con él murió la línea de UPBGE para estos equipos (el último binario oficial es UPBGE 0.44, con el backend Metal a medias). Flipendo continúa esa línea por su cuenta: base moderna de Blender 4.5 + game engine, compilado y probado en hardware real (MacBook Pro 2019, Radeon Pro 5300M).

## Qué arregla Flipendo respecto a UPBGE 0.44 en macOS

| Capacidad | UPBGE 0.44 oficial | Flipendo |
|---|---|---|
| Filtros 2D (post-proceso) | ❌ el shader ni compila en Metal | ✅ presets y custom |
| Filtros custom con sintaxis GLSL antigua (`gl_FragColor`, `texture2D`) | ❌ | ✅ traductor integrado |
| `bge.texture` / VideoTexture (render-to-texture) | ❌ `Texture is not available` | ✅ ImageRender vía GPUViewport |
| Cierre del player con `ImageRender` activo | — | ✅ (use-after-free corregido) |

Todo verificado con capturas de pantalla y tests de píxel en Metal — ver los mensajes de commit, que documentan cada verificación.

## Estructura

- Árbol base: snapshot de UPBGE en el commit upstream `3c7b891a` (Blender 4.5.0 alpha).
- Cada mejora es un commit encima, con la explicación técnica en el mensaje.

## Compilar (macOS Intel)

```
git clone https://github.com/jesussolaz/Flipendo.git
cd Flipendo
# librerías precompiladas de Blender (rama blender-v4.5-release):
git clone --depth 1 --branch blender-v4.5-release \
  https://projects.blender.org/blender/lib-macos_x64.git lib/macos_x64
cmake -S . -B ../build -G Ninja -C build_files/cmake/config/blender_release.cmake \
  -DWITH_GAMEENGINE=ON -DWITH_PLAYER=ON -DWITH_CYCLES=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build ../build --target install
```

## Licencia

GPL-2.0-or-later, heredada de Blender/UPBGE.
