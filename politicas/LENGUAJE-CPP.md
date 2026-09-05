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

## Condición final

La migración no está completa mientras exista código estructural propio de Flipendo
en Python o C. La meta no es "la mayor parte en C++": es **todo Flipendo en C++**.
Blender y UPBGE son el material de origen; Flipendo es el resultado unificado.

> El plan por fases y el estado de la migración están en
> [`MIGRACION-CPP.md`](MIGRACION-CPP.md) (generado del análisis del repo).
