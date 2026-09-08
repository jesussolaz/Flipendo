# Mapa revisión SVN ↔ SHA1

Traduce un número de revisión SVN de la era histórica de Blender (r2, 2002-10-12 →
r61240, 2013-11-12, cuando Blender abandonó SVN) a su commit de git, y a la inversa.
**54.358 pares.**

Sirve para lo que el código heredado necesita de verdad: hay comentarios que citan
`rev NNNNN` y hay que poder localizar ese commit.

```
flipendo svn 23331
flipendo svn 854ea35a2498cb35e7cce26e396fab775692196e
flipendo svn 19226.2          # las 7 revisiones de rama también
```

## Qué había antes

Dos ficheros `.py` de 2,7 MB cada uno (`rev_to_sha1.py` y `sha1_to_rev.py`) con un
diccionario literal: **108.736 líneas, el 30% de todo el Python del árbol**, para cero
lógica. Eran la mayor concentración de Python del proyecto.

Ahora son un asset binario de 1,7 MB (`svn_rev_map.bin`) más un lector en C++
(`svn_rev_map.hpp/.cpp`). La doctrina (`politicas/LENGUAJE-CPP.md`) dice que los datos
serializados no son código: lo que no puede quedarse es el `.py`, no la tabla.

## Formato

Little-endian.

| Bloque | Tamaño | Contenido |
|---|---|---|
| Cabecera | 16 B | magic `FLSV`, `u32` versión, `u32` count, `u32` reservado |
| Registros | count × 28 B | `u32 rev_major`, `u8 rev_minor`, `u8 sha1[20]`, 3 B de relleno — ordenados por `(rev_major, rev_minor)` |
| Índice inverso | count × 4 B | índices de registro ordenados por sha1 |

Dos detalles de los datos que el formato tiene que respetar, y que no son obvios:

1. **`rev_minor` no es decoración.** 7 de las claves no son enteros: son revisiones de
   rama escritas como decimal (`18402.1`, `19226.1`, `19226.2`, `20350.1`, `24062.1`,
   `29506.2`, `36866.1`). Con un `u32` a secas se perderían o colisionarían con su
   revisión entera.
2. **23 SHA1 están compartidos por dos revisiones consecutivas.** Por eso la tabla
   directa tiene 54.358 entradas y la inversa 54.335. El diccionario inverso original
   se quedaba con la revisión **mayor**; el índice de aquí ordena los empates de sha1
   con la revisión mayor primero para responder igual.

## Regenerar

```
c++ -std=c++17 -O2 -o svn_rev_map_build svn_rev_map_build.cpp svn_rev_map.cpp
./svn_rev_map_build rev_to_sha1.py svn_rev_map.bin --verificar sha1_to_rev.py
```

El conversor parte de los `.py` originales (conservados en el historial de git), no de
`git log`. Comprueba la ida de los 54.358 pares y contrasta la vuelta contra el
diccionario inverso; falla si hay una sola discrepancia.

## Por qué no hay generador desde git

El original (`tools/git/git_sh1_to_svn_rev.py`, retirado) emparejaba revisiones con
commits **por marca de tiempo al segundo**, quedándose con el último commit de cada
segundo. Contra la historia que hoy alcanza `git log --all` en este árbol eso ya no
funciona: hay miles de colisiones de segundo y el resultado pierde varios miles de
pares **en silencio** — una tabla incompleta con aspecto de correcta. Portarlo a C++
habría sido recrear una herramienta rota.

`tools/git/git_sh1_to_svn_rev.fossils` se conserva como asset de procedencia: es la
fuente original de la que salió la tabla, no la entrada de un proceso vivo.
