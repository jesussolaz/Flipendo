# Librerías de terceros (catálogo vivo)

Registro de **librerías que añadimos directamente a Flipendo** para ganar
prestaciones que Blender/UPBGE no dan de serie. Cuando necesites algo nuevo
(audio, red, físicas, formatos de asset...), mira primero aquí.

## Añadidas por Flipendo

_(ninguna todavía — Flipendo aún usa solo lo que trae Blender/UPBGE)_

| Librería | Versión | Origen | Licencia | Capacidad | Ubicación | Parches |
|----------|---------|--------|----------|-----------|-----------|---------|
| — | — | — | — | — | — | — |

## Librerías que YA tenemos vía Blender (no hace falta re-importarlas)

Blender empaqueta muchas dependencias en `lib/macos_x64/` (ver [`blender.md`](blender.md)).
Antes de traer una librería nueva, comprueba si ya está aquí:

- **Python 3.11** — scripting (`bge`, `bpy`)
- **FFmpeg** — vídeo y audio (usado por VideoTexture)
- **OpenAL / SDL** — audio del juego
- **Bullet** — física (motor del BGE)
- **OpenImageIO, OpenColorIO** — imágenes y gestión de color
- **OpenVDB, Embree** — volúmenes y trazado (render)
- **USD, Alembic, OpenSubdiv** — intercambio de escenas y geometría
- **Draco** — compresión de malla (glTF)

## Antes de añadir una librería nueva

1. ¿Está ya en la lista de Blender de arriba? Úsala.
2. ¿Licencia compatible con GPL-2.0-or-later? (MIT, BSD, Apache-2.0, LGPL, GPL sí;
   propietaria o Apache-2.0-solo-con-patentes revisar). Si no, no entra.
3. Traerla, integrarla en el build, y **registrarla en la tabla de arriba** con
   su versión, origen y por qué.
