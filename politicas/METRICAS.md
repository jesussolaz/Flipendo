# Métricas de migración a C++ (regeneradas del árbol)

Contador: `tools/flipendo_metrics/flipendo_metrics.cpp` (C++). Ejecutar:
`flipendo-metrics ~/Flipendo/dev/upbge`

## Baseline 2026-09-05 (tras migrar bullet.cc, clog.cc; headers GE .hpp)

### Flipendo mantenido (sin extern/)
| lenguaje | ficheros | líneas |
|---|---:|---:|
| C++ (.cpp/.cc) | 4.201 | 2.514.971 |
| .hh (header C++) | 1.825 | 295.199 |
| .h (header C-style) | 1.441 | 269.571 |
| **Python** | 1.389 | **493.742** |
| .hpp | 287 | 34.167 |
| Objective-C++ | 37 | 30.850 |
| **C** | **22** | **5.457** |
| GLSL | 745 | 69.136 |
| MSL | 4 | 1.726 |

### extern/ (terceros reales, EXTERNAL)
| lenguaje | ficheros | líneas |
|---|---:|---:|
| .h | 1.393 | 497.350 |
| C++ | 773 | 298.938 |
| C | 24 | 58.211 |
| .hpp | 19 | 27.416 |
| Python | 18 | 1.335 |

## Objetivos vivos (deuda abierta)
- **C mantenido no-extern: 22 ficheros / 5.457 líneas** → migrar los compilados en Mac (auditoría en curso). Ya migrados: `bullet.cc`, `clog.cc`. Deuda con evidencia: `dna_defaults.c` (datos formato .blend).
- **Python mantenido: 493.742** → editor Blender. Ver `BACKLOG-EDITOR-PYTHON.md`. Objetivo de medio plazo real: Player sin CPython.
- **Objective-C++: 30.850** → plataforma (GHOST/Metal). Auditoría en curso para reducir/aislar/metal-cpp.
- **.h C-style: 269.571** → auditar y convertir a .hpp los de subsistemas C++ absorbidos.

> Evolución esperada: C 5.457 → 0 (salvo datos con evidencia); Python 493.742 → runtime 0, editor por lotes.
