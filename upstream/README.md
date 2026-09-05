# Procedencia del código y de todo lo externo (upstream)

Flipendo es un fork y **casi nada se escribe desde cero**. Este directorio es el
**registro único de todo lo que traemos de fuera**: motores base, librerías de
terceros, herramientas de compilación y assets. Así, cuando necesitemos más
prestaciones en algo, sabemos **dónde ir a buscarlo y de dónde salió**.

Cadena de derivación del motor:

```
Blender (blender.org)  ──►  UPBGE (añade el game engine)  ──►  Flipendo (este fork)
```

## Regla de oro

> **Todo lo que entre en Flipendo desde una fuente externa se anota aquí,
> en un fichero `.md` por origen.** Sin excepción: una librería, un trozo de
> código copiado, un shader de un tutorial, un modelo, una herramienta.

## Ficheros de este registro

| Fichero | Qué documenta |
|---------|---------------|
| [`blender.md`](blender.md) | Todo lo que nace en **Blender** (la mayor parte del árbol) |
| [`upbge.md`](upbge.md) | El **game engine** de UPBGE y los ficheros importados de UPBGE 0.50 |
| [`build-tools.md`](build-tools.md) | Herramientas para compilar (cmake, ninja, git-lfs) |
| [`libraries.md`](libraries.md) | Librerías de terceros añadidas directamente (catálogo vivo) |
| [`_TEMPLATE.md`](_TEMPLATE.md) | Plantilla para registrar una fuente nueva |

## Cómo añadir una importación nueva (checklist)

1. Copia `_TEMPLATE.md` a `upstream/<nombre>.md` (o añade una fila a `libraries.md`
   si es una librería suelta).
2. Anota: **qué es, versión exacta, URL de origen, licencia, por qué la traemos,
   dónde vive en el árbol y qué parches propios lleva** (si los lleva).
3. Añade la fila a la tabla de arriba si es un fichero nuevo.
4. Commit con mensaje `Upstream: importado <nombre> vX.Y desde <origen>`.

## Regla práctica para actualizaciones

- Mejora de **render, nodos, EEVEE, .blend, importadores** → viene de Blender → `blender.md`.
- Mejora de **lógica de juego, física BGE, filtros 2D, VideoTexture** → viene de UPBGE → `upbge.md`.
- Necesito **una capacidad nueva** (audio avanzado, red, físicas mejores, formato de asset) → mirar si hay librería en `libraries.md`; si no, traerla y **registrarla aquí**.
