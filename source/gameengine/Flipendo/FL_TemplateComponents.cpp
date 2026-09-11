/* FL_TemplateComponents — plantillas de componente NATIVO para el usuario de Flipendo.
 *
 * Sustituyen a scripts/templates_py/, las 40 plantillas de Python que el editor de
 * texto ofrecía en su menú "Templates". Aquella capacidad era "cópiame un ejemplo que
 * funcione para empezar a escribir comportamiento de juego"; sin intérprete, un ejemplo
 * de bpy/bge no sirve de nada, pero la capacidad sigue haciendo falta. Aquí está,
 * reescrita en la forma que de verdad usa un usuario de Flipendo: componentes C++.
 *
 * Doctrina: politicas/LENGUAJE-CPP.md. Cero Python.
 *
 * ---------------------------------------------------------------------------
 * CÓMO SE USA UNA PLANTILLA
 * ---------------------------------------------------------------------------
 * Estas cinco NO son ejemplos muertos: están compiladas y registradas, así que
 * puedes atarlas a un objeto hoy mismo sin tocar código. Para probar una:
 *
 *   1. Selecciona el objeto en el editor.
 *   2. Añádele una propiedad de juego de texto llamada  fl_component
 *   3. Dale como valor el nombre de la plantilla, p. ej.  TemplateHello
 *   4. Dale a P. El componente recibe Start() una vez y Update(dt) cada frame.
 *
 * Para escribir el tuyo a partir de una de ellas:
 *
 *   1. Copia el bloque de la clase a un fichero nuevo  FL_MiJuego.cpp  en este
 *      mismo directorio (source/gameengine/Flipendo/).
 *   2. Cámbiale el nombre a la clase.
 *   3. Escribe una función de registro como la del final de este fichero y
 *      llámala desde FL_ComponentManager::AttachScene (FL_Component.cpp).
 *   4. Añade tu .cpp a source/gameengine/Ketsji/CMakeLists.txt, en el bloque
 *      de rutas ../Flipendo. SIN ESE PASO NO SE COMPILA Y EL ENLACE FALLA.
 *   5. Recompila con  ~/Flipendo/dev/noche/nb install
 *
 * Ejemplos más completos y reales, ya en producción: FL_ArpgComponents.cpp
 * (PlayerController, ThirdPersonCamera, EnemyAI) y FL_RenderToTexture.cpp.
 * --------------------------------------------------------------------------- */

#include "FL_Component.hpp"

#include "KX_GameObject.hpp"
#include "KX_Scene.hpp"
#include "KX_Globals.hpp"
#include "KX_KetsjiEngine.hpp"
#include "SCA_IInputDevice.hpp"
#include "SCA_InputEvent.hpp"
#include "EXP_ListValue.hpp"
#include "EXP_Value.hpp"
#include "MT_Vector3.h"

#include <cmath>
#include <string>

namespace flipendo {

/* ------------------------------------------------------------------ helpers
 * Los mismos que usa FL_ArpgComponents.cpp. Se repiten aquí a propósito: una
 * plantilla tiene que poder copiarse entera y funcionar sin arrastrar nada. */

static SCA_IInputDevice *TemplateInput()
{
  return KX_GetActiveEngine() ? KX_GetActiveEngine()->GetInputDevice() : nullptr;
}

/* Tecla mantenida (se repite todos los frames mientras esté abajo). */
static bool TemplateKeyActive(SCA_IInputDevice::SCA_EnumInputs code)
{
  SCA_IInputDevice *dev = TemplateInput();
  return dev && dev->GetInput(code).Find(SCA_InputEvent::ACTIVE);
}

/* Tecla recién pulsada (se dispara UNA vez por pulsación). */
static bool TemplateKeyPressed(SCA_IInputDevice::SCA_EnumInputs code)
{
  SCA_IInputDevice *dev = TemplateInput();
  return dev && dev->GetInput(code).Find(SCA_InputEvent::JUSTACTIVATED);
}

/* Busca un objeto de la escena por su nombre. Devuelve nullptr si no está. */
static KX_GameObject *TemplateFindByName(KX_Scene *scene, const std::string &name)
{
  if (!scene) {
    return nullptr;
  }
  EXP_ListValue<KX_GameObject> *objs = scene->GetObjectList();
  if (!objs) {
    return nullptr;
  }
  for (KX_GameObject *o : objs) {
    if (o->GetName() == name) {
      return o;
    }
  }
  return nullptr;
}

/* Lee una propiedad de juego numérica; si no existe, devuelve el valor por defecto.
 * Es la forma de hacer un componente ajustable desde el editor sin recompilar. */
static float TemplateGetFloatProp(KX_GameObject *o, const std::string &name, float def)
{
  EXP_Value *p = o ? o->GetProperty(name) : nullptr;
  return p ? (float)p->GetNumber() : def;
}

/* =========================================================================
 * 1. TemplateHello — el esqueleto mínimo.
 *
 * Lo que era operator_simple.py / bmesh_simple.py: el "hola mundo" del que se
 * parte. Un componente no necesita más que esto: heredar de FL_Component y
 * sobrescribir Start() (una vez) y/o Update(dt) (cada frame).
 * Gira el objeto sobre su eje Z para que se vea que está vivo.
 * ========================================================================= */

class FL_TemplateHello : public FL_Component {
 public:
  /* Se llama UNA vez, cuando arranca el juego y el componente se ata al objeto.
   * m_owner ya es válido aquí: el manager llama a SetOwner() antes que a Start(). */
  void Start() override
  {
    /* Velocidad en radianes por segundo. Ajustable con una propiedad de juego
     * llamada "speed" en el objeto; si no la pones, vale 1.0. */
    m_speed = TemplateGetFloatProp(m_owner, "speed", 1.0f);
  }

  /* Se llama cada frame. dt son los segundos transcurridos desde el anterior.
   * Multiplica SIEMPRE por dt: si no, la velocidad depende de los FPS. */
  void Update(float dt) override
  {
    m_owner->ApplyRotation(MT_Vector3(0.0f, 0.0f, m_speed * dt), true /* local */);
  }

 private:
  float m_speed = 1.0f;
};

/* =========================================================================
 * 2. TemplateKeyInput — mover el objeto con el teclado.
 *
 * Lo que eran gamelogic.py, gamelogic_simple.py y gamelogic_module.py: los
 * controladores de Python colgados de los ladrillos lógicos. Aquí no hay
 * ladrillos ni sensores: se pregunta al dispositivo de entrada directamente,
 * que es más directo y no pasa por ninguna capa intermedia.
 *
 * WASD mueve, ESPACIO (recién pulsado) da un salto de posición.
 * ========================================================================= */

class FL_TemplateKeyInput : public FL_Component {
 public:
  void Start() override
  {
    m_speed = TemplateGetFloatProp(m_owner, "speed", 5.0f);
  }

  void Update(float dt) override
  {
    MT_Vector3 move(0.0f, 0.0f, 0.0f);

    /* Tecla mantenida: se consulta con ACTIVE, se cumple todos los frames. */
    if (TemplateKeyActive(SCA_IInputDevice::WKEY)) {
      move.y() += 1.0f;
    }
    if (TemplateKeyActive(SCA_IInputDevice::SKEY)) {
      move.y() -= 1.0f;
    }
    if (TemplateKeyActive(SCA_IInputDevice::AKEY)) {
      move.x() -= 1.0f;
    }
    if (TemplateKeyActive(SCA_IInputDevice::DKEY)) {
      move.x() += 1.0f;
    }

    if (!move.fuzzyZero()) {
      move.normalize();
      /* true = en el sistema de coordenadas local del objeto. */
      m_owner->ApplyMovement(move * (m_speed * dt), true);
    }

    /* Tecla recién pulsada: JUSTACTIVATED, se cumple UNA vez por pulsación.
     * Es la diferencia que más confunde al empezar. */
    if (TemplateKeyPressed(SCA_IInputDevice::SPACEKEY)) {
      MT_Vector3 pos = m_owner->NodeGetWorldPosition();
      m_owner->NodeSetWorldPosition(pos + MT_Vector3(0.0f, 0.0f, 1.0f));
    }
  }

 private:
  float m_speed = 5.0f;
};

/* =========================================================================
 * 3. TemplateTimer — hacer algo cada N segundos.
 *
 * Lo que era operator_modal_timer.py. Un componente ya se ejecuta cada frame,
 * así que no hace falta registrar ningún temporizador en el motor: basta con
 * acumular dt. Este patrón vale para disparar, reaparecer enemigos, refrescar
 * una interfaz, etc.
 * ========================================================================= */

class FL_TemplateTimer : public FL_Component {
 public:
  void Start() override
  {
    m_interval = TemplateGetFloatProp(m_owner, "interval", 1.0f);
    m_elapsed = 0.0f;
    m_ticks = 0;
  }

  void Update(float dt) override
  {
    m_elapsed += dt;
    if (m_elapsed < m_interval) {
      return;
    }
    /* Se resta el intervalo en vez de poner a cero: así no se pierde el resto
     * y el ritmo no se va desviando con el tiempo. */
    m_elapsed -= m_interval;
    m_ticks++;

    OnTick();
  }

 private:
  /* Aquí va lo que quieras que ocurra cada intervalo. */
  void OnTick()
  {
    /* Ejemplo visible sin tocar nada más: alternar la visibilidad del objeto. */
    m_owner->SetVisible((m_ticks % 2) == 0, false /* recursivo */);
  }

  float m_interval = 1.0f;
  float m_elapsed = 0.0f;
  int m_ticks = 0;
};

/* =========================================================================
 * 4. TemplateProperty — un componente configurable desde el editor.
 *
 * Lo que era toda la familia ui_panel.py / ui_list.py: exponer ajustes para
 * que el usuario los toque sin programar. En un componente nativo no se
 * dibuja un panel: se leen propiedades de juego del objeto, que es lo que el
 * editor ya sabe editar. Cambias el valor en el editor y no recompilas.
 *
 * Propiedades que lee: "amplitude" (float), "frequency" (float).
 * ========================================================================= */

class FL_TemplateProperty : public FL_Component {
 public:
  void Start() override
  {
    m_amplitude = TemplateGetFloatProp(m_owner, "amplitude", 1.0f);
    m_frequency = TemplateGetFloatProp(m_owner, "frequency", 1.0f);
    m_origin = m_owner->NodeGetWorldPosition();
    m_phase = 0.0f;
  }

  void Update(float dt) override
  {
    m_phase += dt * m_frequency;
    /* Oscila en Z alrededor de donde estaba al arrancar. */
    MT_Vector3 pos = m_origin;
    pos.z() += m_amplitude * std::sin(m_phase);
    m_owner->NodeSetWorldPosition(pos);
  }

 private:
  float m_amplitude = 1.0f;
  float m_frequency = 1.0f;
  float m_phase = 0.0f;
  MT_Vector3 m_origin;
};

/* =========================================================================
 * 5. TemplateFollow — leer la escena y reaccionar a otro objeto.
 *
 * Lo que eran los ejemplos que hurgaban en los datos (bmesh_simple.py,
 * operator_mesh_add.py, custom_nodes.py): acceder al contenido de la escena.
 * En tiempo de ejecución lo que hay es la lista de objetos de KX_Scene.
 *
 * Sigue suavemente al objeto llamado "Player" (cámbialo por el que quieras,
 * o ponlo en una propiedad de texto "target").
 * ========================================================================= */

class FL_TemplateFollow : public FL_Component {
 public:
  void Start() override
  {
    /* Buscar por nombre es caro: se hace UNA vez en Start, no en Update.
     * Se guarda el puntero, no el nombre. */
    KX_Scene *scene = m_owner->GetScene();
    m_target = TemplateFindByName(scene, "Player");
    m_distance = TemplateGetFloatProp(m_owner, "distance", 3.0f);
  }

  void Update(float dt) override
  {
    /* Trampa importante: el objetivo puede haber sido destruido (endObject)
     * despues de Start(), y entonces el puntero que guardaste cuelga. KX_GameObject
     * NO tiene un IsBeingDestroyed(): el idioma del motor es preguntarle a la lista
     * de objetos de la escena si sigue ahi. Es lo mismo que hace
     * FL_ComponentManager::Tick para soltar los componentes huerfanos. */
    KX_Scene *scene = m_owner->GetScene();
    EXP_ListValue<KX_GameObject> *objs = scene ? scene->GetObjectList() : nullptr;
    if (!m_target || !objs || !objs->SearchValue(m_target)) {
      m_target = nullptr;
      return;
    }

    MT_Vector3 tpos = m_target->NodeGetWorldPosition();
    MT_Vector3 cur = m_owner->NodeGetWorldPosition();

    MT_Vector3 delta = tpos - cur;
    float dist = delta.length();
    if (dist <= m_distance) {
      return; /* Ya está lo bastante cerca. */
    }

    /* Interpolación suave: cuanto mayor es a, más rápido pega el tirón.
     * Se escala con dt para que no dependa de los FPS. */
    const float a = 2.0f * dt;
    m_owner->NodeSetWorldPosition(cur + delta * (a > 1.0f ? 1.0f : a));
  }

 private:
  KX_GameObject *m_target = nullptr;
  float m_distance = 3.0f;
};

/* -------------------------------------------------------------- registro
 * Registro explícito, igual que FL_RegisterBuiltinComponents. NO se usa el
 * macro FL_REGISTER_COMPONENT con inicializadores estáticos: en una biblioteca
 * estática el enlazador descarta la unidad de traducción entera si nadie la
 * referencia, y los componentes desaparecerían sin previo aviso. Esta función
 * la llama FL_ComponentManager::AttachScene, y esa llamada es la referencia
 * que mantiene vivo el fichero. */
void FL_RegisterTemplateComponents(FL_ComponentManager &mgr)
{
  mgr.RegisterType("TemplateHello", []() -> FL_Component * { return new FL_TemplateHello(); });
  mgr.RegisterType("TemplateKeyInput", []() -> FL_Component * { return new FL_TemplateKeyInput(); });
  mgr.RegisterType("TemplateTimer", []() -> FL_Component * { return new FL_TemplateTimer(); });
  mgr.RegisterType("TemplateProperty", []() -> FL_Component * { return new FL_TemplateProperty(); });
  mgr.RegisterType("TemplateFollow", []() -> FL_Component * { return new FL_TemplateFollow(); });
}

}  // namespace flipendo
