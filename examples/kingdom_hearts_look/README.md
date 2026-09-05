# Look "Kingdom Hearts" — filtro 2D NATIVO de Flipendo

El aspecto Kingdom Hearts (bloom + color grading + viñeta) es un **filtro 2D
integrado en el motor**, escrito en C++/GLSL como Sobel o Blur. No hay Python.

## Uso

Desde logic bricks (actuador 2D Filter) o desde el runtime:

```python
scene.filterManager.addFilter(0, bge.logic.RAS_2DFILTER_FLIPENDOKH)
```

Es un modo integrado (`RAS_2DFilterManager::FILTER_FLIPENDOKH`); el cuerpo del
shader vive en `source/gameengine/Rasterizer/RAS_OpenGLFilters/RAS_FlipendoKH2DFilter.glsl`
y cross-compila a Metal/Vulkan por el pipeline del motor.

## Antes / después (plantilla ARPG)

| Sin filtro | Con look KH (nativo) |
|---|---|
| ![antes](antes.png) | ![después](despues.png) |

## Historia (doctrina C++)

Nació como `flipfx.py` (Python, Fase 3). **Migrado a filtro nativo en la Fase A**
de la doctrina C++ (ver `politicas/MIGRACION-CPP.md`): el Python se eliminó, el
look es ahora código del motor. Verificado sobre la escena ARPG (saturación +20%,
viñeta, bloom) con coste GPU por debajo del ruido de medición.
