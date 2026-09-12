/* FL_AnimaComponents — componentes de escena de ÁNIMA, NATIVOS en C++.
 *
 * Piezas pequeñas de «vida» para los escenarios (el charco, los molinos): cosas que
 * flotan y cosas que giran. No son gameplay; son lo que hace que una escena parada
 * parezca un sitio. Se atan por la propiedad de juego "fl_component":
 *
 *   "Flotar"  sube y baja el objeto en Z alrededor de donde estaba al arrancar.
 *             fl_flotar_amp      amplitud en metros            (defecto 0,25)
 *             fl_flotar_periodo  segundos por ciclo completo   (defecto 3)
 *             fl_flotar_giro     giro lento sobre su Z local en grados/s (defecto 0)
 *             La FASE sale de la posición inicial del objeto: dos objetos con el
 *             mismo periodo no suben y bajan a la vez, sin que nadie tenga que
 *             ponerles una fase a mano. Es determinista: misma posición, misma fase.
 *
 *   "Girar"   rota el objeto sobre un eje LOCAL a velocidad constante.
 *             fl_girar_eje       "X" | "Y" | "Z"               (defecto "Y")
 *             fl_girar_grados_s  grados por segundo            (defecto 12)
 *
 * Las dos multiplican por dt: la velocidad no depende de los fps. Flotar solo toca
 * la Z del objeto (lee X e Y cada frame), para que otro sistema pueda moverlo en el
 * plano sin que este lo devuelva al origen.
 *
 * Doctrina: politicas/LENGUAJE-CPP.md. Cero Python. */
#include "FL_Component.hpp"

#include "CM_Message.hpp"
#include "EXP_Value.hpp"
#include "KX_GameObject.hpp"
#include "MT_Vector3.h"

#include <cmath>
#include <string>

namespace flipendo {

static float AnimaFloatProp(KX_GameObject *o, const std::string &name, float def)
{
  EXP_Value *p = o ? o->GetProperty(name) : nullptr;
  return p ? (float)p->GetNumber() : def;
}

static std::string AnimaStrProp(KX_GameObject *o, const std::string &name)
{
  EXP_Value *p = o ? o->GetProperty(name) : nullptr;
  return p ? p->GetText() : std::string();
}

/* ------------------------------------------------------------- Flotar */

class FL_Flotar : public FL_Component {
 public:
  void Start() override
  {
    m_amp = AnimaFloatProp(m_owner, "fl_flotar_amp", 0.25f);
    m_period = AnimaFloatProp(m_owner, "fl_flotar_periodo", 3.0f);
    if (m_period < 0.05f) {
      CM_Error("Flotar: fl_flotar_periodo=" << m_period << " en '" << m_owner->GetName()
                                              << "' es demasiado corto; se usa 3 s");
      m_period = 3.0f;
    }
    m_spinDeg = AnimaFloatProp(m_owner, "fl_flotar_giro", 0.0f);
    const MT_Vector3 p = m_owner->NodeGetWorldPosition();
    m_baseZ = p.z();
    /* Fase por posición: una combinación irracional de las tres coordenadas,
     * reducida a [0, 2pi). Objetos en sitios distintos -> fases distintas. */
    const float raw = p.x() * 0.7f + p.y() * 1.3f + p.z() * 0.4f;
    m_phase = std::fmod(std::fabs(raw), 2.0f * (float)M_PI);
    m_t = 0.0f;
  }

  void Update(float dt) override
  {
    m_t += dt;
    MT_Vector3 p = m_owner->NodeGetWorldPosition();
    p.z() = m_baseZ + m_amp * std::sin(2.0f * (float)M_PI * m_t / m_period + m_phase);
    m_owner->NodeSetWorldPosition(p);
    if (m_spinDeg != 0.0f) {
      m_owner->ApplyRotation(MT_Vector3(0.0f, 0.0f, m_spinDeg * (float)M_PI / 180.0f * dt), true);
    }
  }

 private:
  float m_amp = 0.25f;
  float m_period = 3.0f;
  float m_spinDeg = 0.0f;
  float m_baseZ = 0.0f;
  float m_phase = 0.0f;
  float m_t = 0.0f;
};

/* ------------------------------------------------------------- Girar */

class FL_Girar : public FL_Component {
 public:
  void Start() override
  {
    std::string axis = AnimaStrProp(m_owner, "fl_girar_eje");
    if (axis.empty()) {
      axis = "Y";
    }
    const char a = axis[0];
    if (a == 'X' || a == 'x') {
      m_axis = MT_Vector3(1, 0, 0);
    }
    else if (a == 'Z' || a == 'z') {
      m_axis = MT_Vector3(0, 0, 1);
    }
    else {
      if (!(a == 'Y' || a == 'y')) {
        CM_Error("Girar: fl_girar_eje='" << axis << "' en '" << m_owner->GetName()
                                         << "' no es X, Y ni Z; se usa Y");
      }
      m_axis = MT_Vector3(0, 1, 0);
    }
    m_degPerSec = AnimaFloatProp(m_owner, "fl_girar_grados_s", 12.0f);
  }

  void Update(float dt) override
  {
    const float rad = m_degPerSec * (float)M_PI / 180.0f * dt;
    m_owner->ApplyRotation(m_axis * rad, true /* eje local */);
  }

 private:
  MT_Vector3 m_axis{0, 1, 0};
  float m_degPerSec = 12.0f;
};

/* ------------------------------------------------------------- registro
 * Explícito y llamado desde FL_ComponentManager::AttachScene, por la misma razón
 * que FL_RegisterBuiltinComponents: en una biblioteca estática el enlazador tira
 * la unidad de traducción si nadie la referencia. */
void FL_RegisterAnimaComponents(FL_ComponentManager &mgr)
{
  mgr.RegisterType("Flotar", []() -> FL_Component * { return new FL_Flotar(); });
  mgr.RegisterType("Girar", []() -> FL_Component * { return new FL_Girar(); });
}

}  // namespace flipendo
