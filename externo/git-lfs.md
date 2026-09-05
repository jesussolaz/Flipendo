# Git LFS

**Qué es:** extensión de git para binarios grandes; descarga/sube las libs y datafiles pesados del repo.
**Versión:** 3.7.0
**Origen:** https://github.com/git-lfs/git-lfs/releases
**Licencia:** MIT
**Añadido por Flipendo:** sí (herramienta externa, no viene con Blender/UPBGE)
**Palabras clave:** git, lfs, binarios grandes, push, libs

## Dónde vive
`~/Flipendo/dev/toolchain/git-lfs` (binario; en el PATH vía `~/.zshrc`).

## Notas
- Si `git push` falla con *"git-lfs was not found on your path"*, la shell no tiene el toolchain en el PATH:
  `export PATH="$HOME/Flipendo/dev/toolchain:$PATH"`.

## Cómo actualizar
1. Descargar el paquete darwin-amd64 de la página de releases.
2. Colocar el binario en `~/Flipendo/dev/toolchain/`.
3. Actualizar la versión en la cabecera de esta ficha.
