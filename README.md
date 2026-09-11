<h1 align="center">Flipendo</h1>

<p align="center">
  <strong>Motor de juego en C++ para macOS con Metal.</strong><br>
  Un fork de <a href="https://github.com/UPBGE/upbge">UPBGE</a> / Blender 4.5 al que le estamos quitando el intérprete de Python.
</p>

<p align="center">
  <img alt="Licencia GPL-2.0-or-later" src="https://img.shields.io/badge/licencia-GPL--2.0--or--later-blue">
  <img alt="macOS Intel con Metal" src="https://img.shields.io/badge/plataforma-macOS%20Intel%20%C2%B7%20Metal-111111">
  <img alt="C++17" src="https://img.shields.io/badge/C%2B%2B-17-00599C">
  <img alt="Cero Python en el juego exportado" src="https://img.shields.io/badge/Python%20en%20el%20juego-0%20ficheros-2ea44f">
  <img alt="47 verificadores integrados en el binario" src="https://img.shields.io/badge/verificadores-47-orange">
</p>

<p align="center">
  <a href="#compilar-macos">Compilar</a> ·
  <a href="#ánima--el-juego-que-se-hace-con-él">El juego</a> ·
  <a href="#metas-cercanas">Metas</a> ·
  <a href="#comparativa-flipendo-upbge-unreal-y-unity">Comparativa</a> ·
  <a href="politicas/">Doctrina y estado</a>
</p>

![El molino de La Mancha, la pieza que fija el listón de calidad del proyecto](docs/img/anima/molino-CamHeroe.jpg)


Blender eliminó el soporte de macOS Intel en la versión 5.0, y con él murió la línea de UPBGE para estos equipos (el último binario oficial es UPBGE 0.44, con el backend Metal a medias). Flipendo continúa esa línea por su cuenta: base de Blender 4.5 + game engine, compilado y probado en hardware real (MacBook Pro 2019, Radeon Pro 5300M).

Lo que empezó como mantener viva una línea abandonada se ha convertido en algo más concreto: **quitarle el intérprete de Python al motor** y dejar una base de C++ sobre la que se pueda construir de verdad.

---

## ÁNIMA — el juego que se hace con él

**ÁNIMA** es un Action-RPG de aventura, con la escala visual de la generación PS2 y Kingdom Hearts como referencia de estructura. Es el proyecto que justifica el motor y el que marca qué hay que arreglar.

### El molino de La Mancha

La pieza que fija el listón de calidad. **67.989 caras**, modelado real y no primitivas: 661 tejas colocadas una a una en 19 hiladas, 120 sillares individuales en el zócalo, aspas con travesaños y lona con panza, y 11 desconchones donde la cal se cayó y asoma la mampostería.

| | |
|---|---|
| ![El molino](docs/img/anima/molino-CamHeroe.jpg) | ![Las aspas](docs/img/anima/molino-CamAspas.jpg) |
| El molino sobre su cerro | Aspas: verga, travesaños y lona |
| ![La puerta](docs/img/anima/molino-CamPuerta.jpg) | ![Postal](docs/img/anima/molino-CamPostal.jpg) |
| Jambas de sillería alternando soga y tizón, herrajes y aldaba | La silueta contra el atardecer |

### Por dentro

| | |
|---|---|
| ![La sala](docs/img/anima/interior-CamSala.jpg) | ![El horno](docs/img/anima/interior-CamHorno.jpg) |
| La sala, con el pan sobre la mesa | El horno encendido |
| ![La escalera](docs/img/anima/interior-CamEscalera.jpg) | ![La mesa](docs/img/anima/interior-CamMesa.jpg) |
| Escalera al piso de la molienda | Detalle de la mesa |

### Terreno 1 — La Mancha

El primer mapa jugable: pueblo, camino, arroyo con puente y la colina de los molinos. 300×300 m.

| | |
|---|---|
| ![La vista](docs/img/anima/t1-CamVista.jpg) | ![La plaza](docs/img/anima/t1-CamPlaza.jpg) |
| La colina de los molinos al atardecer | La plaza, con el pozo y los faroles |
| ![El molino desde el camino](docs/img/anima/t1-CamMolino.jpg) | ![El puente](docs/img/anima/t1-CamPuente.jpg) |
| El camino que sube al molino | El puente sobre el arroyo seco |

> **Honestidad sobre estas imágenes.** El molino es la pieza héroe, con el nivel de detalle al que va todo lo demás. El mapa T1 es de una fase anterior y todavía usa formas simples; el maniquí que se ve en la plaza es un **placeholder** hasta tener el modelo del protagonista, y los enemigos son bultos deliberadamente abstractos porque su diseño está sin decidir. Ninguno de los dos `.blend` contiene una sola línea de Python: el gameplay se ata con componentes nativos en C++.

---

## Metas cercanas

Cuatro, en este orden.

### 1. C++ completo

Ni una línea de Python en el motor. No es una limpieza estética: mientras el keymap por defecto salga de un script, **el juego exportado tiene que cargar un intérprete entero** para saber qué hace la tecla `G`.

Estado real, medido y verificable en el propio binario:

| | |
|---|---|
| C mantenido (fuera de `extern/`) | **0 ficheros** |
| Componentes de gameplay | **100% C++** (`FL_Component`; el sistema Python fue eliminado) |
| Mapa de teclado por defecto | **248/248 keymaps en C++**, idénticos a los que generaba Python |
| Sistema de herramientas | **100% C++, sin una sola línea de Python**: catálogo, activación, consultas, operadores, barra, ajustes, cabecera, reserva y keymap del popup. Verificado contra el Python real (416 + 474 + 416 + 912 casos), la barra y la cabecera píxel a píxel. Retiradas 5.606 líneas de Python |
| El build necesita Python | **no** (el último generador pasó a C++) |
| Player sin CPython | **compila y juega** — 0 símbolos `_Py`, 536 MB frente a 771 MB |
| Python restante | **191.224 líneas**, todas del editor (eran 493.742 al empezar el proyecto) |
| Ficheros `.py` dentro del juego exportado | **0** |
| Objective-C++ | 30.304 líneas; la capa de cabeceras de Metal ya es C++ puro y el resto son **441 envíos de mensaje**, no 21.000 líneas |
| C, shell y presets propios | **0** |
| Verificadores integrados en el binario | **47** opciones `--fl-check-*` / `--fl-dump-*` |

**Cómo se verifica.** Nada se da por migrado leyendo el código: se congela lo que produce el Python y el C++ tiene que reproducirlo exactamente. Para la interfaz, eso significa capturas. Cada par de columnas es la barra de herramientas del mismo editor y modo, dibujada por **Python a la izquierda** y por **C++ a la derecha** — vista 3D en modo objeto, edición y escultura; editor UV, nodos y secuenciador:

![Barra de herramientas: Python frente a C++, idénticas píxel a píxel](docs/img/flipendo/barra-python-vs-cpp.png)

*Las seis parejas son idénticas píxel a píxel. La barra ya no ejecuta Python en cada redibujado.*

Lo que queda es el editor: paneles, operadores y el puente CPython. La infraestructura para migrarlo ya existe (`FL_ui_registry`) y el editor de lógica es el piloto que la valida.

### 2. Velocidad, y ganarle a la competencia en Mac

Que abra antes, cargue antes y vaya más fluido que las alternativas **en el hardware que la gente tiene**, no en una estación de trabajo. Menos indirección, menos capas, y el intérprete fuera del bucle de frame.

### 3. Instalador rápido y bien comprimido

Un instalador nativo de macOS, pequeño y que termine rápido. Quitar CPython ya se llevó 235 MB por delante; el objetivo es seguir bajando y que instalar sea cuestión de segundos.

### 4. Ordenar la casa

Blender y UPBGE arrastran décadas de capas superpuestas. Organizar lo heredado y lo añadido en una base coherente, para que "añadir una función" no signifique pelearse con tres subsistemas a la vez. Sin esto, las otras tres metas se deshacen con el tiempo.

---

## Comparativa: Flipendo, UPBGE, Unreal y Unity

Contexto honesto: Unreal y Unity son ecosistemas industriales con miles de personas detrás; Flipendo lo mantiene una persona, con un nicho concreto — **que un Mac siga siendo ciudadano de primera y que el flujo de trabajo viva dentro de Blender**. Esta tabla existe para elegir herramienta con datos, no para pretender otra cosa.

| | **Flipendo** | **UPBGE 0.50+** | **Unreal Engine 5** | **Unity 6** |
|---|---|---|---|---|
| **macOS Intel (x86_64)** | ✅ objetivo principal, build nativo Metal | ❌ eliminado (el último fue 0.44, con Metal roto) | ⚠️ funciona, pero sin Nanite/Lumen y con el editor cada vez más pesado; el foco es Apple Silicon | ✅ aún soportado |
| **Editar y jugar sin exportar** (el editor ES la herramienta 3D) | ✅ es Blender: modelas, animas y pulsas P | ✅ ídem | ❌ pipeline de importación desde la DCC | ❌ pipeline de importación desde la DCC |
| **Lenguaje de juego** | **C++ nativo** (`FL_Component`); el sistema de componentes Python fue eliminado | Python + logic bricks + nodos | C++ y Blueprints | C# |
| **Intérprete en el juego exportado** | **ninguno** — el Player sin CPython compila y juega | CPython embebido | ninguno (C++ compilado) | Mono/IL2CPP |
| **Tamaño del runtime** | **536 MB** y bajando (era 771 MB con CPython) | ~770 MB | ~1 GB+ por proyecto vacío | varía según backend |
| **Post-proceso en Mac Intel** | ✅ filtros 2D en Metal (arreglado en este fork; acepta también la sintaxis GLSL antigua de los tutoriales) | ❌ en 0.44/macOS ni compilaba; 0.50 no existe para Intel | ✅ | ✅ |
| **Render-to-texture en Mac Intel** (espejos, minimapas, CCTV) | ✅ restaurado **y nativo en C++**: funciona en el Player sin intérprete | ❌ (mismo motivo) | ✅ | ✅ |
| **Motor de render** | EEVEE (rasterizador tiempo real de Blender) | EEVEE | Nanite+Lumen (no en Mac Intel), rasterizador clásico como alternativa | URP / HDRP |
| **Plataformas de exportación** | macOS (hoy); el código heredó soporte Win/Linux de UPBGE pero se **podó a propósito** para simplificar | Windows, Linux, macOS ARM | Todas: PC, consolas, móvil | Todas: PC, consolas, móvil, web |
| **Licencia y coste** | GPL-2.0+, gratis, código abierto completo | GPL-2.0+, gratis | Gratis hasta 1 M$ de ingresos, luego 5% de royalties; código fuente visible pero no libre | Gratis hasta 200 k$ (Personal); suscripción por asiento después; código cerrado |
| **Asset store / ecosistema** | ❌ (lo que haya para Blender) | pequeño | enorme | enorme |
| **Add-ons de terceros** | ❌ y **por diseño**: sin intérprete no hay add-ons Python | ✅ | ✅ plugins C++ | ✅ paquetes C# |
| **Madurez / riesgo** | ⚠️ fork joven de una persona; historia corta y verificada commit a commit | comunidad pequeña, desarrollo activo | industria AAA | industria, muy extendido en indie/móvil |
| **Para quién tiene sentido** | tienes un Mac, quieres flujo 100% Blender y un motor GPL sin intérprete que puedas tocar por dentro | mismo perfil, pero quieres scripting en Python y multiplataforma | equipo/proyecto AAA o portfolio industrial, hardware moderno | indie multiplataforma, móvil, ecosistema C# |

La fila que más cambia el cálculo es la de add-ons: quitar Python **cuesta** el ecosistema de scripts de terceros. Es un intercambio deliberado — arranque más rápido, runtime más pequeño y una sola base de código — no un descuido.

## Qué arregla Flipendo respecto a UPBGE 0.44 (el último oficial para Mac Intel)

| Capacidad | UPBGE 0.44 oficial | Flipendo |
|---|---|---|
| Filtros 2D (post-proceso) | ❌ el shader ni compila en Metal | ✅ presets y custom |
| Filtros custom con sintaxis GLSL antigua (`gl_FragColor`, `texture2D`) | ❌ | ✅ traductor integrado |
| `bge.texture` / VideoTexture (render-to-texture) | ❌ `Texture is not available` | ✅ ImageRender vía GPUViewport, **con fachada C++**: funciona sin Python |
| Interfaz de juego | `bgui`, en Python | ✅ lienzo y widgets **nativos en C++**, dibujados sin intérprete |
| Publicar y exportar el juego | addons Python (dos de los cinco, rotos desde 2019) | ✅ operadores nativos en C++, verificados publicando el juego real |
| Cierre del player con `ImageRender` activo | ❌ segfault | ✅ (use-after-free corregido) |
| Componentes de gameplay | Python | ✅ C++ nativo |
| Mapa de teclado | script de 8.669 líneas | ✅ C++, verificado atajo a atajo |
| Catálogo de herramientas | script de 3.752 líneas | ✅ C++, verificado herramienta a herramienta (el script ya no existe) |

Todo verificado con capturas y tests en Metal — ver los mensajes de commit, que documentan cada verificación.

## Estructura

- Árbol base: snapshot de UPBGE en el commit upstream `3c7b891a` (Blender 4.5.0 alpha).
- Cada mejora es un commit encima, con la explicación técnica en el mensaje.
- Doctrina y estado de la migración: [`politicas/`](politicas/).

## Compilar (macOS)

```
git clone https://github.com/jesussolaz/Flipendo.git
cd Flipendo
# librerías precompiladas de Blender (rama blender-v4.5-release):
git clone --depth 1 --branch blender-v4.5-release \
  https://projects.blender.org/blender/lib-macos_x64.git lib/macos_x64
cmake -S . -B ../build -G Ninja -C build_files/cmake/config/blender_release.cmake \
  -DWITH_GAMEENGINE=ON -DWITH_PLAYER=ON -DWITH_CYCLES=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build ../build --target install
```

### Dos configuraciones: editor y Player de distribución

El árbol se compila de dos maneras distintas según para qué sea el binario:

| | Editor | Player de distribución |
|---|---|---|
| Flags extra | *(los de arriba)* | `-DWITH_PYTHON=OFF -DWITH_USD=OFF -DWITH_HYDRA=OFF -DWITH_MATERIALX=OFF` |
| Lleva CPython | sí | **no** |
| Para qué | modelar, montar la escena, pulsar P | empaquetar el juego |

El juego se **hace** con el editor y se **envía** con el Player sin CPython. El `.blend` es el mismo; solo tiene que no depender de Python (gameplay como componentes nativos atados con la propiedad de juego `fl_component`, no como scripts). Qué se gana y qué se pierde exactamente: [`politicas/PLAYER-SIN-CPYTHON.md`](politicas/PLAYER-SIN-CPYTHON.md).

## Licencia

GPL-2.0-or-later, heredada de Blender/UPBGE.
