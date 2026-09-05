# Look "Kingdom Hearts" — post-proceso nativo de Flipendo

`flipfx.py` es un filtro 2D en un solo pase que da el aspecto característico
del estilo Kingdom Hearts: colores vivos, glow (bloom) en las zonas brillantes,
contraste y viñeta. Corre en Metal/macOS gracias a los filtros 2D arreglados
en Flipendo (imposible en UPBGE 0.44 oficial).

## Uso

Como componente sobre la cámara del juego (Object Properties > Game > Components):
`flipfx.PostFX`

O por script, una vez al arrancar:
```python
import flipfx
flipfx.install(scene)
```

## Antes / después (escena de la plantilla ARPG)

| Sin filtro | Con look KH |
|---|---|
| ![antes](antes.png) | ![después](despues.png) |

## Verificado (Radeon Pro 5300M, Metal)

- Saturación media +20 %, esquinas 11 % más oscuras (viñeta).
- Bloom: brillo del halo alrededor de una fuente emisiva ×7 en el anillo
  cercano, ×18 en el medio; muestreo radial de 100 taps (20 direcciones ×
  5 anillos, rotados para no dejar artefactos de estrella).
- Coste en GPU por debajo del ruido de medición (~0,5 ms a 1080p): 60 fps intactos.
