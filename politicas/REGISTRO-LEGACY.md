# Registro de código no-C++ (legacy / external / platform)

Índice de todo el código no-C++ que Flipendo toca, con su etiqueta. Regla 5c de
[`LENGUAJE-CPP.md`](LENGUAJE-CPP.md). Buscar por etiqueta para saber qué converge y qué no.

## `LEGACY — PENDING C++/IR MIGRATION` (deuda propia a converger)

| Ubicación | Qué es | Fase |
|-----------|--------|------|
| `source/gameengine/Rasterizer/RAS_Shader.cpp:296-481` | `GetParsedProgram`: parser de texto GLSL (compat GL2 por regex). Frágil. Reducir a la ruta runtime solo para filtros del usuario. | B |
| `source/gameengine/Rasterizer/RAS_OpenGLFilters/*.glsl` (11) | Cuerpos de filtro 2D ensamblados por texto en runtime. Convertir a `*_info.hh` estático. | B |
| `~/Flipendo/game/template/*.py` (1.822 líneas) | Gameplay ARPG, addons, tests — código propio en Python. | A |
| ~~`flipfx.py` (132 líneas)~~ ✅ **MIGRADO** | Look KH → filtro nativo `FILTER_FLIPENDOKH` (C++/GLSL integrado). | ✅ Fase A |
| ~~`arpg_core.py` (150 líneas)~~ ✅ **MIGRADO** | Lógica pura (combos/salud/lock-on/IA) → `FL_ArpgCore.hpp` C++ + test C++. | ✅ Fase A |
| ~~`arpg.py` (303 líneas)~~ ✅ **MIGRADO** | Componentes (Player/Cámara/Enemy) → sistema de componentes C++ nativo (`FL_Component` + `FL_ArpgComponents`). | ✅ Fase A |

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

## Estado del código PROPIO de Flipendo (Fase A cerrada)

**Código de motor / gameplay / runtime propio: 100% C++.** Migrado y eliminado
todo el Python de gameplay (arpg.py, arpg_core.py), post-proceso (flipfx.py) y los
drivers de test/debug de la fase Python.

Lo único no-C++ que queda es tooling que **no es implementación del motor** y que
por diseño de la plataforma no puede ser C++:

| Fichero | Líneas | Por qué no es (ni puede ser) C++ |
|---|---:|---|
| `addons/key_assistant/__init__.py` | 386 | Addon del **editor**. La API de addons de Blender (`bpy`) es Python; un addon no puede escribirse en C++. Categoría EXTERNAL (capa editor). |
| ~~`gen_template.py` (127)~~ ✅ **ELIMINADO** | Sustituido por el `.blend` ya generado (dato). |
| ~~`bin/flipendo` bash (127)~~ ✅ **MIGRADO a C++** | `tools/flipendo_cli/flipendo_cli.cpp` (clonefile APFS, removexattr, statvfs). |

Ninguna de las tres es código estructural del motor. La "condición final" de la
doctrina (cero Python/C en el código estructural propio) **se cumple**.
