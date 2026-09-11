# Política de lenguaje de Flipendo: C++ único

> **Regla absoluta, prioritaria sobre cualquier otra instrucción del proyecto.**
> Todo el código estructural propio de Flipendo debe converger a **C++**.

## En vigor DESDE YA (no espera al plan de migración)

1. **Código nuevo = C++.** Ninguna funcionalidad nueva de Flipendo se escribe en
   Python, C, Lua, JS ni shell. Si hace falta algo nuevo, se hace en C++.
2. **No aumentar la deuda.** Está prohibido añadir Python/C nuevo "provisional".
3. **Lo heredado se registra como `legacy`** y se migra cuando corresponda, no se
   amplía.
4. **Cada migración pasa por tests + benchmark** antes de eliminar el original.

## Qué es código y qué no

- **Código (migra a C++):** motor, gameplay, componentes, herramientas, operadores,
  UI, import/export mantenidos por Flipendo, asset pipeline, networking, física,
  animación, render, serialización, tests y benchmarks propios.
- **NO es código (se queda como está):** `.blend`, `.gltf/.glb/.fbx/.obj/.usd`,
  `.png/.jpg/.exr/.hdr`, `.wav/.ogg/.mp3`, texturas, modelos, rigs, escenas,
  materiales, cachés, datos serializados y cualquier asset o formato de datos.

## Dependencias externas

- Preferir **absorber** la funcionalidad y reimplementarla en C++.
- Si reescribir es absurdo por tamaño: **wrapper C++** y el resto del motor solo ve
  C++. La dependencia se documenta en [`../externo/`](../externo/) como
  `EXTERNAL / NOT FLIPENDO SOURCE`.

## Shaders

- Objetivo: **una sola fuente lógica** (C++ / Shader IR) que genere Metal, Vulkan y
  DX. El MSL/SPIR-V/HLSL es **salida generada**, no código mantenido a mano.
- Shaders heredados aún en GLSL/MSL a mano → marcar `LEGACY — PENDING C++/IR MIGRATION`.

## Convención de ficheros

- **Headers nuevos de Flipendo: extensión `.hpp`** (C++ explícito). Nada de `.h`
  nuevos, que pueden esconder API de C.
- Implementación nueva: `.cpp`.
- Los `.h` **heredados** de Blender/UPBGE son `EXTERNAL`: no se renombran en masa
  (romperían miles de `#include` y el build); convergen solo cuando se reescribe
  su subsistema, y entonces pasan a `.hpp`.

## Plataforma (Objective-C++ en macOS)

- Solo se permite como **wrapper mínimo y aislado** de APIs del sistema (Cocoa/Metal)
  que no se pueden llamar desde C++ puro. No forma parte de la lógica del motor.

## Estrategia: divergencia total (actualizada 2026-09-05)

Flipendo **ya no persigue mantener sincronía con Blender/UPBGE**. El objetivo es
la propiedad total del código en C++, aunque eso implique divergir del upstream.
Por tanto: los headers heredados pueden renombrarse a `.hpp`, el C heredado puede
reescribirse a C++, y el editor puede migrarse por fases. `externo/` queda como
**registro histórico de procedencia**, no como compromiso de sincronización.

## Condición final

La migración no está completa mientras exista código estructural propio de Flipendo
en Python o C. La meta no es "la mayor parte en C++": es **todo Flipendo en C++**.
Blender y UPBGE son el material de origen; Flipendo es el resultado unificado.

> El plan por fases y el estado de la migración están en
> [`MIGRACION-CPP.md`](MIGRACION-CPP.md) (generado del análisis del repo).

---

## Decisión del 2026-09-11: full C++ significa TODO, no «todo lo razonable»

Jesús ha decidido, con el coste sobre la mesa: **full C++ incluye el Objective-C++ y
los cuerpos de shader**. Esta sección deroga lo que decían `MIGRACION-CPP.md` §3.3 y §4
sobre ObjC++ como «plataforma legítima que no se persigue» y sobre GLSL como
«inviable de golpe». Siguen siendo caros; dejan de ser aceptables.

**Objetivo:** cero Python, cero C propio (cumplido), cero Objective-C++, cero shell
propio, cero GLSL escrito a mano, y las cabeceras `.h` heredadas convertidas a `.hpp`
según se reescriba su subsistema.

**Única excepción, y es doctrinal:** `extern/` y `lib/` son terceros vendorizados. Se
mantienen verbatim y se actualizan desde upstream. Reescribir a mano lzma, ufbx o
nanosvg no es propiedad del código, es asumir el mantenimiento de bibliotecas ajenas
sin ganar nada. Quedan marcados como `linguist-vendored` para que ni GitHub los
atribuya a Flipendo.

### Por qué el Objective-C++ sí se puede, contra lo que decía la política anterior

La política afirmaba que AppKit «no tiene binding C++ → imposible C++ puro». **Es
falso y queda corregido.** El runtime de Objective-C es una biblioteca de C
(`objc_getClass`, `sel_registerName`, `objc_msgSend`, `objc_allocateClassPair`,
`class_addMethod`): cualquier programa C++ puede llamar a cualquier método de Cocoa sin
que intervenga un compilador de Objective-C. La prueba es que **`metal-cpp`, el binding
oficial de Apple, es exactamente eso**: cabeceras que envuelven `objc_msgSend`.

Lo que cambia no es la posibilidad sino quién escribe el pegamento. En un `.mm` lo
genera el compilador; en C++ puro lo escribimos y lo mantenemos nosotros, con estas
trampas que hay que tener presentes y **verificar**:

- `objc_msgSend` se castea a la firma exacta de cada método. Mal casteado es
  comportamiento indefinido, y en x86_64 hay variantes (`_stret`, `_fpret`).
- Los delegados exigen **fabricar clases Objective-C en tiempo de ejecución** con sus
  codificaciones de tipo correctas.
- Los bloques son extensión de Clang, no C++ estándar.
- Sin ARC: `retain`/`release` y pools de autoliberación explícitos.

Por eso cada pieza migrada aquí necesita **más** verificación que una de interfaz, no
menos: el compilador deja de avisar.
