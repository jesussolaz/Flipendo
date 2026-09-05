# Procedencia del código (upstream)

Flipendo es un fork y **casi todo su código viene de otros dos proyectos**. Este
directorio documenta qué parte viene de dónde, para poder **aprovechar sus futuras
actualizaciones** sin rehacer el trabajo.

Cadena de derivación:

```
Blender (blender.org)  ──►  UPBGE (añade el game engine)  ──►  Flipendo (este fork)
```

- [`blender.md`](blender.md) — todo lo que nace en **Blender** (la mayor parte del árbol) y cómo actualizarlo.
- [`upbge.md`](upbge.md) — lo que es propio de **UPBGE** (el game engine) y los ficheros que hemos importado de UPBGE 0.50.

Regla práctica para futuras actualizaciones:
- Una mejora de **render, escultura, geometry nodes, EEVEE, formato .blend, importadores** → viene de Blender → ver `blender.md`.
- Una mejora de **lógica de juego, física BGE, filtros 2D, VideoTexture, logic bricks** → viene de UPBGE → ver `upbge.md`.
