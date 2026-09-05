# Herramientas de compilación (build tools)

No forman parte del binario de Flipendo, pero **hacen falta para compilarlo**.
Viven en `~/Flipendo/dev/toolchain/` (fuera del repo, por su tamaño) y se añaden
al PATH desde `~/.zshrc`.

| Herramienta | Versión | Origen | Licencia | Para qué |
|-------------|---------|--------|----------|----------|
| CMake | 3.31.6 (macos-universal) | https://cmake.org/download/ · https://github.com/Kitware/CMake/releases | BSD-3-Clause | Sistema de configuración del build |
| Ninja | 1.12.1 | https://github.com/ninja-build/ninja/releases | Apache-2.0 | Ejecutor del build (rápido, incremental) |
| Git LFS | 3.7.0 | https://github.com/git-lfs/git-lfs/releases | MIT | Descargar/subir binarios grandes (libs, datafiles) del repo |

**Compilador:** Clang del sistema (Xcode Command Line Tools) — no lo empaquetamos.

## Notas de uso

- Si `git push` falla con *"git-lfs was not found on your path"*, es que el
  toolchain no está en el PATH de esa shell: `export PATH="$HOME/Flipendo/dev/toolchain:$PATH"`.
- Estas versiones son las probadas con la base Blender 4.5. Subirlas suele ser
  seguro, pero ante un fallo raro de build, volver a estas.

## Cómo actualizar

1. Descargar el binario de la web/releases del origen (macOS x86_64/universal).
2. Colocar en `~/Flipendo/dev/toolchain/` y actualizar el PATH en `~/.zshrc` si cambia el nombre de carpeta.
3. Anotar la versión nueva en la tabla de arriba.
