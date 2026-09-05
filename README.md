# Flipendo

**Motor de juego derivado de [UPBGE](https://github.com/UPBGE/upbge), mantenido para macOS Intel (x86_64) con backend Metal funcional.**

Blender eliminó el soporte de macOS Intel en la versión 5.0, y con él murió la línea de UPBGE para estos equipos (el último binario oficial es UPBGE 0.44, con el backend Metal a medias). Flipendo continúa esa línea por su cuenta: base moderna de Blender 4.5 + game engine, compilado y probado en hardware real (MacBook Pro 2019, Radeon Pro 5300M).

## Comparativa: Flipendo, UPBGE, Unreal y Unity

Contexto honesto: Unreal y Unity son ecosistemas industriales con miles de personas detrás; Flipendo es un fork mantenido por una persona con un nicho concreto — **que un Mac Intel siga siendo ciudadano de primera y que el flujo de trabajo viva dentro de Blender**. Esta tabla existe para elegir herramienta con datos, no para pretender otra cosa.

| | **Flipendo** | **UPBGE 0.50+** | **Unreal Engine 5** | **Unity 6** |
|---|---|---|---|---|
| **macOS Intel (x86_64)** | ✅ objetivo principal, build nativo Metal | ❌ eliminado (el último fue 0.44, con Metal roto) | ⚠️ funciona, pero sin Nanite/Lumen y con el editor cada vez más pesado; el foco es Apple Silicon | ✅ aún soportado |
| **Editar y jugar sin exportar** (el editor ES la herramienta 3D) | ✅ es Blender: modelas, animas y pulsas P | ✅ ídem | ❌ pipeline de importación desde la DCC | ❌ pipeline de importación desde la DCC |
| **Lenguaje de juego** | Python + logic bricks + nodos | Python + logic bricks + nodos | C++ y Blueprints | C# |
| **Post-proceso en Mac Intel** | ✅ filtros 2D en Metal (arreglado en este fork; acepta también la sintaxis GLSL antigua de los tutoriales) | ❌ en 0.44/macOS ni compilaba; 0.50 no existe para Intel | ✅ | ✅ |
| **Render-to-texture en Mac Intel** (espejos, minimapas, CCTV) | ✅ restaurado en este fork | ❌ (mismo motivo) | ✅ | ✅ |
| **Motor de render** | EEVEE (rasterizador tiempo real de Blender) | EEVEE | Nanite+Lumen (no en Mac Intel), rasterizador clásico como alternativa | URP / HDRP |
| **Plataformas de exportación** | macOS Intel (hoy); el código hereda soporte Win/Linux de UPBGE, sin builds propios aún | Windows, Linux, macOS ARM | Todas: PC, consolas, móvil | Todas: PC, consolas, móvil, web |
| **Licencia y coste** | GPL-2.0+, gratis, código abierto completo | GPL-2.0+, gratis | Gratis hasta 1 M$ de ingresos, luego 5% de royalties; código fuente visible pero no libre | Gratis hasta 200 k$ (Personal); suscripción por asiento después; código cerrado |
| **Asset store / ecosistema** | ❌ (lo que haya para Blender) | pequeño | enorme | enorme |
| **Madurez / riesgo** | ⚠️ fork joven de una persona; historia corta y verificada commit a commit | comunidad pequeña, desarrollo activo | industria AAA | industria, muy extendido en indie/móvil |
| **Para quién tiene sentido** | tienes un Mac Intel, quieres flujo 100% Blender y motor GPL que puedas tocar por dentro | mismo perfil, pero en Windows/Linux/Mac ARM | equipo/proyecto AAA o portfolio industrial, hardware moderno | indie multiplataforma, móvil, ecosistema C# |

## Qué arregla Flipendo respecto a UPBGE 0.44 (el último oficial para Mac Intel)

| Capacidad | UPBGE 0.44 oficial | Flipendo |
|---|---|---|
| Filtros 2D (post-proceso) | ❌ el shader ni compila en Metal | ✅ presets y custom |
| Filtros custom con sintaxis GLSL antigua (`gl_FragColor`, `texture2D`) | ❌ | ✅ traductor integrado |
| `bge.texture` / VideoTexture (render-to-texture) | ❌ `Texture is not available` | ✅ ImageRender vía GPUViewport |
| Cierre del player con `ImageRender` activo | ❌ segfault | ✅ (use-after-free corregido) |

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
