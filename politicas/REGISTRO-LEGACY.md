# Registro de código no-C++ (legacy / external / platform)

Índice de todo el código no-C++ que Flipendo toca, con su etiqueta. Regla 5c de
[`LENGUAJE-CPP.md`](LENGUAJE-CPP.md). Buscar por etiqueta para saber qué converge y qué no.

## `LEGACY — PENDING C++/IR MIGRATION` (deuda propia a converger)

| Ubicación | Qué es | Fase |
|-----------|--------|------|
| `source/gameengine/Rasterizer/RAS_Shader.cpp:296-481` | `GetParsedProgram`: parser de texto GLSL (compat GL2 por regex). Frágil. Reducir a la ruta runtime solo para filtros del usuario. | B |
| `source/gameengine/Rasterizer/RAS_OpenGLFilters/*.glsl` (11) | Cuerpos de filtro 2D ensamblados por texto en runtime. Convertir a `*_info.hh` estático. | B |
| `~/Flipendo/game/template/*.py` (1.954 líneas) | Gameplay ARPG, post-proceso `flipfx`, addons, tests — código propio en Python. | A |

## `PLATFORM` (wrapper de plataforma inevitable — permitido, no es deuda)

| Ubicación | Qué es | Por qué no es C++ puro |
|-----------|--------|------------------------|
| `intern/ghost/intern/*Cocoa*.mm` (~4.340 líneas) | Ventana, entrada, rutas en macOS | AppKit/Cocoa no tiene binding C++ |
| `source/blender/gpu/metal/*.mm` (~20.950 líneas) | Backend GPU Metal | API Metal es ObjC (posible metal-cpp, coste alto) |
| `intern/cycles/device/metal/*.mm` (~5.060) | Cycles Metal (render offline) | Candidato a excluir del build de juego |

## `EXTERNAL` (terceros / editor heredado — no se reescribe)

| Ubicación | Qué es |
|-----------|--------|
| `extern/` (ufbx, lzma, lzo, cuew/hipew, xxhash…) | Librerías vendorizadas; actualizar desde upstream |
| `scripts/` (~292.753 líneas Python) | Editor heredado de Blender (bl_ui, bl_operators, addons_core, modules) |
| CPython embebido | Intérprete del Player/Editor durante la transición (ver Fase C/D) |

> Detalle y roadmap completo en [`MIGRACION-CPP.md`](MIGRACION-CPP.md).
