# Validación de Flipendo 2.1 (base Blender 4.5)

Fecha: 2026-09-05. Hardware: MacBook Pro 2019, Radeon Pro 5300M, Metal.

Objetivo: confirmar que el cambio de motor (0.44 → Flipendo 2.1, salto de base
Blender 4.4 → 4.5) no rompe lo que ya funcionaba, antes de construir encima.

## 1. Regresión funcional — plantilla ARPG

La plantilla `KeyARPG.blend` (validada originalmente sobre el motor 0.44) se
ejecuta en el player de Flipendo 2.1 **sin modificar nada**. 7/7 tests en verde:

| Test | Qué comprueba | Resultado |
|------|---------------|-----------|
| T1 | Objetos de escena (player, cámara, 3 enemigos) | OK |
| T2 | Componentes vivos (hp=100, combo=idle) | OK |
| T3 | Cámara activa correcta | OK |
| T4 | Enemigo muere al recibir daño letal | OK |
| T5 | El jugador recibe daño (100 → 75) | OK |
| T6 | La cámara sigue al jugador | OK |
| T7 | IA de persecución (enemigo 6.9 → 2.2) | OK |

## 2. Rendimiento — escena pesada (1299 objetos, 1080p, 16 luces con sombra)

Reparto del frame (media de 2 pasadas de ~480 muestras cada una):

| Categoría | 0.44 | Flipendo 2.1 | Δ |
|-----------|------|--------------|---|
| Depsgraph (CPU) | 2.81 | 2.79 | −0.02 ms |
| Física (CPU) | 1.17 | 1.16 | −0.01 ms |
| Rasterizer (GPU) | 5.83 | 6.21 | +0.38 ms |
| Lógica | 0.04 | 0.04 | ±0.00 ms |
| **Trabajo real** | **9.85** | **10.20** | **+0.35 ms** |
| GPU Latency (espera vsync) | 6.77 | 6.42 | −0.35 ms |

- Techo teórico: 102 fps (0.44) → 98 fps (Flipendo 2.1).
- Margen sobre el objetivo de 60 fps: 41% → **39%**.

**Conclusión:** EEVEE de Blender 4.5 rasteriza ~0.4 ms más caro que el de 4.4
en esta escena (más trabajo por frame a cambio de mejor calidad de sombras/GI).
El impacto es despreciable: seguimos con ~39% de margen sobre 60 fps. El objetivo
de 60 fps se mantiene holgado. Luz verde para seguir construyendo sobre 2.1.
