# `bl_operators/rigidbody.py` a C++ — qué había, cómo se verificó, trampas, deuda

Carril C (Flipendo, noche del 2026-09-10/11). Los tres operadores de
`scripts/startup/bl_operators/rigidbody.py` (316 líneas) pasan a
`source/blender/editors/physics/rigidbody_tools.cc`.

| idname | C++ | Verificado |
|---|---|---|
| `rigidbody.object_settings_copy` | `RIGIDBODY_OT_object_settings_copy` | ✅ por ejecución, con cifras |
| `rigidbody.connect` | `RIGIDBODY_OT_connect` | ✅ por ejecución, con cifras |
| `rigidbody.bake_to_keyframes` | `RIGIDBODY_OT_bake_to_keyframes` | ⚠️ **deuda**: no se puede ejecutar desde un guion (ver abajo) |

Declarados en `physics_intern.hh`, registrados en `ED_operatortypes_physics()`. Mismos
idnames, mismas propiedades, mismos textos, mismos flags y el mismo `poll`
(`obj and obj.rigid_body`). Se retira el `.py` y `"rigidbody"` sale de `_modules` en
`bl_operators/__init__.py`.

## Cómo se verificó

Arnés nuevo: `--fl-selftest-rigidbody-ops` / `--fl-check-rigidbody-ops`
(`editors/physics/fl_rigidbody_ops_selftest.cc`,
`editors/include/FL_rigidbody_ops_selftest.hh`). Cinco escenas deterministas construidas
llamando solo a operadores por idname; se vuelca el estado observable de toda la escena:
cada objeto con su tipo, selección, si es el activo, su posición y su tipo de empty; los
dieciocho ajustes de cuerpo rígido **leídos por RNA con los mismos identificadores que
usaba el Python**; y los constraints generados con su tipo y sus dos objetos.

Además, `--fl-check-optypes` compara la superficie de registro (nombre, descripción,
contexto de traducción y todas las propiedades con tipo, subtipo, defecto, rangos y
enums) contra la del binario con Python. Ahí sí entran los tres, `bake_to_keyframes`
incluido: no se puede ejecutar, pero sí se puede exigir que se registre igual.

Línea base en `tests/flipendo/rigidbody/baseline-python.txt` y
`tests/flipendo/optypes/baseline-python.txt`, capturadas contra `/tmp/Blender-ref.app`
(el binario anterior a la migración, con los `.py` dentro de su bundle).

## Trampas

1. **`WM_operator_name_call()` arrastra las propiedades de la última ejecución.** Los
   operadores a los que llaman estos (`rigidbody.objects_add`,
   `rigidbody.constraint_add`, `anim.keyframe_insert_by_name`) son `OPTYPE_REGISTER`, y
   `WM_operator_last_properties_init()` rellena lo que no se ponga con lo de la llamada
   anterior. `bpy.ops...()` no hace eso. Todas las llamadas desde C++ ponen **todas** las
   propiedades que les importan, explícitas.
2. **Asignar por RNA, no por DNA.** `setattr(rb_to, attr, ...)` de Python pasa por el
   setter de RNA *y* por `RNA_property_update()`. Varios de los dieciocho ajustes tienen
   setter propio (`type` y `collision_shape` liberan el objeto de física y lo marcan para
   revalidar). Escribir `rbo->type` a pelo compila, funciona "casi" y deja la simulación
   con el cuerpo viejo. Se copia propiedad a propiedad por identificador RNA.
3. **`list.sort()` de Python es estable.** El patrón `CHAIN_DISTANCE` ordena por distancia
   al último objeto; con distancias iguales, el orden de partida decide. `std::sort` no
   garantiza eso: hay que usar `std::stable_sort` o la cadena sale distinta.
4. **Euler compatible.** `mat.to_euler(mode, prev)` elige, de los ocho eulers que
   representan la misma rotación, el más cercano al del fotograma anterior. Con
   `mat4_to_eulO()` a secas la curva pega saltos de 2π al interpolar; hay que usar
   `mat4_to_compatible_eulO()`. Lo mismo con el signo del cuaternión (`q` y `-q` son la
   misma rotación pero interpolan por caminos opuestos).

## Deuda con nombre y apellidos: `rigidbody.bake_to_keyframes`

**No está verificado por ejecución, y no por falta de ganas.** El operador llama a
`bpy.ops.anim.keyframe_insert_by_name(type='BUILTIN_KSI_LocRot')`, cuyo `poll`
(`modify_key_op_poll`, `editors/animation/keyframing.cc:152`) exige un `ScrArea` activo:

```c
ScrArea *area = CTX_wm_area(C);
Scene *scene = CTX_data_scene(C);
if (ELEM(nullptr, area, scene)) { return false; }
```

Comprobado que **ni el original en Python se puede invocar desde un guion**:

- `--background`: no hay ventana ni área → `RuntimeError: Operator
  bpy.ops.anim.keyframe_insert_by_name.poll() failed, context is incorrect`.
- Modo gráfico con `-P`: el guion corre antes de que el contexto tenga área
  (`bpy.context.area is None`) → el mismo fallo.
- `bpy.context.temp_override(area=...)` con un área sacada de `bpy.data.screens` en
  `--background`: `RuntimeError: Area not found in screen`, porque no hay ventana a la que
  pertenezca esa pantalla.

Por tanto no hay forma de capturar una línea base de su comportamiento. Lo que sí está
verificado es que se **registra** igual (`--fl-check-optypes`), y lo que queda pendiente
es una comprobación **manual en la interfaz**:

> Receta: escena con un plano PASSIVE y un cubo ACTIVE cayendo, fotogramas 1-30, cubo
> seleccionado y activo, Object ▸ Rigid Body ▸ Bake to Keyframes con `step=2`. Comprobar
> que aparecen claves de localización y rotación en los fotogramas 1,3,5…29, que su
> interpolación es LINEAR, que las claves intermedias redundantes se han quitado, que el
> cubo ya no tiene cuerpo rígido y que el fotograma actual vuelve a ser el de partida.
> Repetir con `rotation_mode` en QUATERNION y en AXIS_ANGLE.

Se decidió retirar igualmente `rigidbody.py` entero en vez de dejar el módulo Python vivo
solo por esta clase: si se dejara, el registro de Python taparía al de C++ (gana el que
registra último) y el C++ quedaría como código muerto, sin usarse y sin poder probarse ni
a mano. Es preferible que esté vivo y con la deuda escrita aquí.

## Lo que queda

- La comprobación manual de `bake_to_keyframes` descrita arriba.
- `fl_rigidbody_ops_selftest.cc` no cubre `bake_to_keyframes` por lo mismo; si algún día
  el arnés corre con una ventana de verdad, añadir el caso es media hora.
