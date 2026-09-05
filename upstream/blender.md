# Código procedente de Blender

**Origen:** https://github.com/blender/blender (espejo de https://projects.blender.org/blender/blender)
**Versión base de Flipendo:** Blender **4.5.0** (`BLENDER_VERSION 405`).
**Commit base UPBGE→Blender:** el árbol parte del commit UPBGE `3c7b891a7282`, que integra Blender 4.5.0.

Blender aporta **el motor de aplicación completo**: render, modelado, animación,
nodos, formato de fichero, Python (bpy), importadores/exportadores y las
bibliotecas del sistema. Todo esto lo mantiene Blender; nosotros lo heredamos.

## Directorios que vienen de Blender (sin cambios de Flipendo, salvo indicado)

| Ruta | Contenido |
|------|-----------|
| `source/blender/` | Núcleo de Blender: `blenkernel`, `editors`, `gpu`, `makesdna`, `makesrna`, `nodes`, `draw`, `python`, `windowmanager`, `imbuf`, … |
| `source/creator/` | Punto de entrada del ejecutable (Blender lo define; UPBGE lo extiende) |
| `intern/` | Módulos internos C/C++: `cycles`, `ghost` (ventanas/entrada), `guardedalloc`, `mikktspace`, `opencolorio`, … |
| `extern/` | Dependencias con código en el árbol: `ceres`, `draco`, `bullet2` (base), `xxhash`, … |
| `release/` | Datafiles, scripts de sistema, iconos, temas |
| `scripts/` | Addons y módulos Python de Blender (`addons_core`, `modules`, `startup`) |
| `intern/cycles/` | Motor Cycles (en Flipendo se compila con `WITH_CYCLES=OFF`, pero el código está) |
| `doc/`, `locale/`, `tests/` | Documentación, traducciones y tests de Blender |

## Bibliotecas precompiladas (imprescindibles para compilar en macOS)

| Ruta | Origen | Rama |
|------|--------|------|
| `lib/macos_x64/` | https://projects.blender.org/blender/lib-macos_x64 | `blender-v4.5-release` |

Contiene Python 3.11, FFmpeg, OpenVDB, Embree, USD, OpenImageIO, etc. **compilados
para Mac Intel**. Es la última rama con soporte x86_64 (Blender 5.0 lo eliminó).

## Cómo aprovechar una actualización de Blender

1. Ver qué versión de Blender queremos (p. ej. 4.5.x LTS con parches).
2. `git fetch` del repo de Blender en el rango de esa versión.
3. Aplicar los cambios **solo a los directorios de la tabla de arriba** (nunca a
   `source/gameengine/`, que es de UPBGE — ver `upbge.md`).
4. Recompilar y pasar la validación (`docs/VALIDACion-*.md`).

**Aviso:** subir de versión mayor de Blender (4.5 → 5.0) rompe el soporte de Mac
Intel (no hay libs x64 para 5.0). Mantenerse en la línea **4.5 LTS** es lo que
conserva vivo este fork en hardware Intel.
