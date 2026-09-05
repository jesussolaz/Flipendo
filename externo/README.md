# Externo — de dónde viene cada cosa

Flipendo es un fork: **casi nada se escribe desde cero**. Esta carpeta es el
registro de todo lo que viene de fuera, para saber **dónde buscar** cuando
necesitemos más prestaciones y **de dónde salió** cada cosa.

## Regla

> **Una ficha `.md` por cada cosa externa que metamos APARTE** (una librería, una
> herramienta, código copiado, un asset). Nombre de fichero = nombre de la cosa,
> para encontrarla al instante con `find externo -name "ffmpeg*"`, `grep audio externo/*`
> o el buscador de GitHub.
>
> **Las librerías que ya vienen dentro de Blender o UPBGE NO llevan ficha** — están
> listadas en [`blender.md`](blender.md) y [`upbge.md`](upbge.md). Solo documentamos
> aparte lo que añadimos nosotros.

## Bases del motor

| Ficha | Qué es |
|-------|--------|
| [`blender.md`](blender.md) | Núcleo Blender 4.5 (render, nodos, .blend, libs incluidas) |
| [`upbge.md`](upbge.md) | Game engine de UPBGE + ficheros importados de UPBGE 0.50 |

## Cosas externas añadidas por Flipendo (una ficha cada una)

| Ficha | Qué es | Palabras clave |
|-------|--------|----------------|
| [`cmake.md`](cmake.md) | Configurador del build | build, compilar |
| [`ninja.md`](ninja.md) | Ejecutor del build | build, compilar |
| [`git-lfs.md`](git-lfs.md) | Git para binarios grandes | git, push, libs |

_(Aún no hemos añadido ninguna librería de terceros aparte; cuando lo hagamos, se crea aquí su ficha.)_

## Añadir una cosa externa nueva (checklist)

1. ¿Ya viene con Blender/UPBGE? Míralo en `blender.md` / `upbge.md`. Si está, úsala — no crees ficha.
2. Si es algo nuevo que traemos aparte: copia [`_TEMPLATE.md`](_TEMPLATE.md) a `externo/<nombre>.md`.
3. Rellena versión exacta, origen (URL), licencia (compatible con GPL-2.0-or-later), qué prestación da, dónde vive y qué parches lleva.
4. Añade la fila a la tabla de arriba.
5. Commit: `Externo: añadido <nombre> vX.Y desde <origen>`.
