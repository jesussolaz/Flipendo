# Código procedente de UPBGE

**Origen:** https://github.com/UPBGE/upbge
**Versión base de Flipendo:** UPBGE **0.44 / commit `3c7b891a7282`** (rama equivalente a `blender-v4.5-release`, `UPBGE_VERSION 45`).
**Ports adicionales:** subsistemas concretos traídos de **UPBGE 0.50** (`upbge-v0.50-release`).

UPBGE añade a Blender **el motor de juego (BGE)**: runtime en tiempo real, física
BGE, lógica (logic bricks + Python `bge`), filtros 2D, VideoTexture y el reproductor
standalone. Es la razón de ser de Flipendo. Todo lo de este documento se sincroniza
desde UPBGE, **no** desde Blender.

## Directorio que es 100% de UPBGE

| Ruta | Contenido |
|------|-----------|
| `source/gameengine/` | **El game engine completo.** Subdirectorios: |
| `  ├─ Ketsji/` | Núcleo del runtime (KX_*): escena, objetos, cámara, materiales de juego |
| `  ├─ Rasterizer/` | Rasterizado del BGE, shaders, **filtros 2D** (RAS_*) |
| `  ├─ Physics/` | Integración de física (Bullet) del BGE |
| `  ├─ Converter/` | Conversión de datos Blender → estructuras del BGE |
| `  ├─ GameLogic/` | Logic bricks (sensores, controladores, actuadores) |
| `  ├─ Expressions/` | Sistema de propiedades y expresiones |
| `  ├─ VideoTexture/` | `bge.texture`: render-to-texture, vídeo, ImageRender |
| `  ├─ SceneGraph/` | Grafo de escena rápido del BGE |
| `  ├─ Launcher/` `GamePlayer/` | Arranque del juego y reproductor standalone |
| `  ├─ BlenderRoutines/` `Common/` `Device/` | Puente con Blender, utilidades, entrada |

Además, UPBGE modifica algunos ficheros dentro de directorios de Blender para
enganchar el game engine (RNA del juego, UI de propiedades de juego, arranque en
`source/creator/` y `source/blenderplayer/`). Esos cambios se resuelven al
sincronizar desde UPBGE, no desde Blender.

## Ficheros importados de UPBGE 0.50 hacia Flipendo (Fase 3)

Estos ficheros se trajeron de `upbge-v0.50-release` y se **adaptaron** a la base
Blender 4.5 (tipos GPU `blender::gpu::*` → C `GPU*`, firmas de API). Ver commits
`61f87dd`, `5655c3e`, `132fcd0`.

**Filtros 2D (arreglo Metal):**
- `source/gameengine/Rasterizer/RAS_Shader.{cpp,h}`
- `source/gameengine/Rasterizer/RAS_2DFilter.cpp`
- `source/gameengine/Rasterizer/RAS_2DFilterFrameBuffer.{cpp,h}`
- `source/gameengine/Rasterizer/RAS_Texture.h`
- `source/gameengine/Rasterizer/RAS_OpenGLFilters/*.glsl`
- `source/gameengine/Ketsji/KX_2DFilter.cpp`, `KX_2DFilterFrameBuffer.{cpp,h}`

**VideoTexture (restaurado en Metal):**
- `source/gameengine/VideoTexture/` (14 ficheros: Texture, ImageRender, ImageViewport, ImageBase, ImageBuff, VideoBase, VideoFFmpeg, DeckLink, blendVideoTex, CMakeLists)
- `source/gameengine/Ketsji/KX_BlenderMaterial.{cpp,h}`, `BL_Texture.{cpp,h}`

> Nota: estos ficheros llevan **parches propios de Flipendo** encima del código de
> UPBGE 0.50 (adaptación a 4.5 y fix del segfault de cierre). Al sincronizar una
> versión más nueva de UPBGE, hay que **re-aplicar esos parches** — ver el diff de
> los commits citados.

## Cómo aprovechar una actualización de UPBGE

1. `git fetch` de github.com/UPBGE/upbge en la rama/tag deseado.
2. Traer cambios **solo de `source/gameengine/`** (y de los hooks del game engine).
3. Re-aplicar los parches de Flipendo sobre los ficheros de la lista anterior.
4. Recompilar y pasar la validación (regresión ARPG + benchmark).

**Aviso:** UPBGE 0.50+ se basa en Blender 5.0 (sin Mac Intel). Por eso Flipendo
**no** hace merge directo de 0.50: importa ficheros sueltos y los adapta a 4.5.
Esa es exactamente la estrategia que documenta este fichero.
