/* FL_ArpgComponents — gameplay ARPG estilo Kingdom Hearts, NATIVO en C++.
 *
 * Migrado de arpg.py (Fase A, doctrina C++). Sustituye los KX_PythonComponent
 * (PlayerController, ThirdPersonCamera, EnemyAI) por componentes C++ que usan
 * FL_ArpgCore (lógica pura) + la API C++ del motor. Cero Python.
 *
 * Atado por propiedad de juego "fl_component" = "PlayerController" | "ThirdPersonCamera" | "EnemyAI". */
#include "FL_Component.hpp"
#include "FL_ArpgCore.hpp"

#include "KX_GameObject.hpp"
#include "KX_Camera.hpp"
#include "KX_Scene.hpp"
#include "KX_Globals.hpp"
#include "KX_KetsjiEngine.hpp"
#include "SCA_IInputDevice.hpp"
#include "SCA_InputEvent.hpp"
#include "PHY_ICharacter.hpp"
#include "PHY_IPhysicsEnvironment.hpp"
#include "EXP_IntValue.hpp"
#include "EXP_StringValue.hpp"
#include "EXP_ListValue.hpp"
#include "MT_Vector3.h"
#include "MT_Matrix3x3.h"

#include <cmath>

namespace flipendo {

/* ------------------------------------------------------------- helpers */

static float TicDt()
{
  double r = KX_GetActiveEngine() ? KX_GetActiveEngine()->GetTicRate() : 60.0;
  return 1.0f / (float)(r > 0 ? r : 60.0);
}

static SCA_IInputDevice *Input()
{
  return KX_GetActiveEngine() ? KX_GetActiveEngine()->GetInputDevice() : nullptr;
}

static bool KeyActive(SCA_IInputDevice::SCA_EnumInputs code)
{
  SCA_IInputDevice *dev = Input();
  return dev && dev->GetInput(code).Find(SCA_InputEvent::ACTIVE);
}

static bool KeyPressed(SCA_IInputDevice::SCA_EnumInputs code)
{
  SCA_IInputDevice *dev = Input();
  return dev && dev->GetInput(code).Find(SCA_InputEvent::JUSTACTIVATED);
}

static void SetIntProp(KX_GameObject *o, const std::string &name, int v)
{
  o->SetProperty(name, new EXP_IntValue(v));
}

static void SetStrProp(KX_GameObject *o, const std::string &name, const std::string &v)
{
  o->SetProperty(name, new EXP_StringValue(v, name));
}

/* Consume una propiedad numérica (leer + borrar). Devuelve true si existía. */
static bool ConsumeInt(KX_GameObject *o, const std::string &name, int &out)
{
  EXP_Value *p = o->GetProperty(name);
  if (!p) {
    return false;
  }
  out = (int)p->GetNumber();
  o->RemoveProperty(name);
  return true;
}

static std::vector<KX_GameObject *> Enemies(KX_Scene *scene)
{
  std::vector<KX_GameObject *> out;
  EXP_ListValue<KX_GameObject> *objs = scene->GetObjectList();
  if (!objs) return out;
  for (KX_GameObject *o : objs) {
    if (o->GetProperty("arpg_enemy")) {
      out.push_back(o);
    }
  }
  return out;
}

/* ------------------------------------------------------------- PlayerController */

class FL_PlayerController : public FL_Component {
 public:
  void Start() override
  {
    m_health = arpg::Health(100);
    m_char = nullptr;
    if (m_owner->GetScene()->GetPhysicsEnvironment()) {
      m_char = m_owner->GetScene()->GetPhysicsEnvironment()->GetCharacterController(m_owner);
    }
    SetIntProp(m_owner, "hp", m_health.hp());
    SetStrProp(m_owner, "combo", "idle");
  }

  void Update(float) override
  {
    const float dt = TicDt();
    KX_Scene *scene = m_owner->GetScene();
    m_health.update(dt);

    /* combo: J o click izquierdo */
    const bool atk = KeyActive(SCA_IInputDevice::JKEY) || KeyActive(SCA_IInputDevice::LEFTMOUSE);
    if (atk && !m_prevAtk) {
      m_combo.press_attack();
    }
    m_prevAtk = atk;
    arpg::ComboEvents ev = m_combo.update(dt);
    if (ev.hit_active) {
      ApplyHits(scene);
    }
    SetStrProp(m_owner, "combo",
               std::string(PhaseName(m_combo.phase())) + ":" + std::to_string(m_combo.index()));

    /* movimiento WASD relativo a cámara */
    MT_Vector3 mv = MoveInput();
    if (mv.length() > 0.001f) {
      MT_Vector3 world = CamRelative(scene, mv);
      Walk(world * m_speed);
      MT_Vector3 flat(world.x(), world.y(), 0.0f);
      if (flat.length() > 0.001f) {
        m_owner->AlignAxisToVect(flat.normalized(), 1, 0.25f);
        m_owner->AlignAxisToVect(MT_Vector3(0, 0, 1), 2, 1.0f);
      }
    }
    else {
      Walk(MT_Vector3(0, 0, 0));
    }

    /* salto */
    const bool jp = KeyActive(SCA_IInputDevice::SPACEKEY);
    if (jp && !m_prevJump && m_char && m_char->OnGround()) {
      m_char->Jump();
    }
    m_prevJump = jp;

    /* daño recibido (propiedad puesta por el enemigo o el test) */
    int dmg;
    if (ConsumeInt(m_owner, "arpg_player_hit", dmg)) {
      if (m_health.damage(dmg)) {
        SetIntProp(m_owner, "hp", m_health.hp());
      }
    }
  }

 private:
  static const char *PhaseName(arpg::Phase p)
  {
    switch (p) {
      case arpg::Phase::Startup: return "startup";
      case arpg::Phase::Active: return "active";
      case arpg::Phase::Recovery: return "recovery";
      default: return "idle";
    }
  }

  MT_Vector3 MoveInput()
  {
    float x = (KeyActive(SCA_IInputDevice::DKEY) ? 1.f : 0.f) - (KeyActive(SCA_IInputDevice::AKEY) ? 1.f : 0.f);
    float y = (KeyActive(SCA_IInputDevice::WKEY) ? 1.f : 0.f) - (KeyActive(SCA_IInputDevice::SKEY) ? 1.f : 0.f);
    MT_Vector3 v(x, y, 0);
    return v.length() > 0 ? v.normalized() : v;
  }

  MT_Vector3 CamRelative(KX_Scene *scene, const MT_Vector3 &mv)
  {
    KX_Camera *cam = scene->GetActiveCamera();
    if (!cam) return MT_Vector3(mv.x(), mv.y(), 0);
    MT_Matrix3x3 o = cam->NodeGetWorldOrientation();
    MT_Vector3 f = o.getColumn(1); f.z() = 0;
    MT_Vector3 r = o.getColumn(0); r.z() = 0;
    if (f.length() < 0.001f) { f = o.getColumn(2) * -1.0f; f.z() = 0; }
    f = f.normalized(); r = r.normalized();
    MT_Vector3 out = r * mv.x() + f * mv.y();
    return out.length() > 0 ? out.normalized() : out;
  }

  void Walk(const MT_Vector3 &vel)
  {
    const float inv = TicDt();
    if (m_char) {
      m_char->SetWalkDirection(vel * inv);
    }
    else {
      m_owner->ApplyMovement(vel * inv, false);
    }
  }

  void ApplyHits(KX_Scene *scene)
  {
    const arpg::HitSpec *h = m_combo.hit();
    if (!h) return;
    const MT_Vector3 pos = m_owner->NodeGetWorldPosition();
    const MT_Vector3 fwd = m_owner->NodeGetWorldOrientation().getColumn(1);
    for (KX_GameObject *e : Enemies(scene)) {
      MT_Vector3 d = e->NodeGetWorldPosition() - pos;
      if (d.length() <= h->reach && d.normalized().dot(fwd) > 0.25f) {
        SetIntProp(e, "arpg_hit", h->dmg);
      }
    }
  }

  arpg::ComboStateMachine m_combo;
  arpg::Health m_health{100};
  PHY_ICharacter *m_char = nullptr;
  float m_speed = 7.0f;
  bool m_prevAtk = false, m_prevJump = false;
};

/* ------------------------------------------------------------- ThirdPersonCamera */

class FL_ThirdPersonCamera : public FL_Component {
 public:
  void Start() override
  {
    KX_Scene *scene = m_owner->GetScene();
    if (KX_Camera *self = dynamic_cast<KX_Camera *>(m_owner)) {
      scene->SetActiveCamera(self);
    }
  }

  void Update(float) override
  {
    KX_Scene *scene = m_owner->GetScene();
    KX_GameObject *target = FindByName(scene, "Player");
    if (!target) return;

    MT_Vector3 tpos = target->NodeGetWorldPosition();
    MT_Vector3 off(std::sin(-m_yaw) * std::cos(m_pitch),
                   -std::cos(m_yaw) * std::cos(m_pitch),
                   std::sin(m_pitch));
    off = off * m_dist;
    MT_Vector3 desired = tpos + MT_Vector3(0, 0, m_height) + off;

    MT_Vector3 cur = m_owner->NodeGetWorldPosition();
    const float a = 0.15f;
    m_owner->NodeSetWorldPosition(cur * (1.0f - a) + desired * a);

    MT_Vector3 look = tpos + MT_Vector3(0, 0, m_height * 0.7f);
    m_owner->AlignAxisToVect(m_owner->NodeGetWorldPosition() - look, 2, 1.0f);
    m_owner->AlignAxisToVect(MT_Vector3(0, 0, 1), 1, 1.0f);
  }

 private:
  static KX_GameObject *FindByName(KX_Scene *scene, const std::string &name)
  {
    EXP_ListValue<KX_GameObject> *objs = scene->GetObjectList();
    if (!objs) return nullptr;
    for (KX_GameObject *o : objs) {
      if (o->GetName() == name) return o;
    }
    return nullptr;
  }

  float m_dist = 7.0f, m_height = 2.6f;
  float m_yaw = 0.0f, m_pitch = 0.31416f;
};

/* ------------------------------------------------------------- EnemyAI */

class FL_EnemyAI : public FL_Component {
 public:
  void Start() override
  {
    m_brain = arpg::EnemyBrain(18.0f, 2.2f, 1.4f);
    m_health = arpg::Health(40, 0.15f);
    SetIntProp(m_owner, "arpg_enemy", 1);
    SetIntProp(m_owner, "hp", m_health.hp());
  }

  void Update(float) override
  {
    const float dt = TicDt();
    KX_Scene *scene = m_owner->GetScene();
    m_health.update(dt);

    int dmg;
    if (ConsumeInt(m_owner, "arpg_hit", dmg)) {
      if (m_health.damage(dmg)) {
        SetIntProp(m_owner, "hp", m_health.hp());
      }
    }
    if (m_health.dead()) {
      m_owner->GetScene()->DelayedRemoveObject(m_owner);
      return;
    }

    KX_GameObject *player = FindByName(scene, "Player");
    if (!player) return;
    MT_Vector3 d = player->NodeGetWorldPosition() - m_owner->NodeGetWorldPosition();
    const float dist = d.length();
    const bool sees = dist < m_brain.sight();
    arpg::EnemyAction act = m_brain.decide(dt, dist, sees);
    if (act == arpg::EnemyAction::Chase) {
      MT_Vector3 flat(d.x(), d.y(), 0);
      if (flat.length() > 0.001f) {
        MT_Vector3 step = flat.normalized() * (m_speed * dt);
        m_owner->ApplyMovement(step, false);
        m_owner->AlignAxisToVect(flat.normalized(), 1, 0.2f);
        m_owner->AlignAxisToVect(MT_Vector3(0, 0, 1), 2, 1.0f);
      }
    }
    else if (act == arpg::EnemyAction::Attack) {
      SetIntProp(player, "arpg_player_hit", m_dmg);
    }
  }

 private:
  static KX_GameObject *FindByName(KX_Scene *scene, const std::string &name)
  {
    EXP_ListValue<KX_GameObject> *objs = scene->GetObjectList();
    if (!objs) return nullptr;
    for (KX_GameObject *o : objs) {
      if (o->GetName() == name) return o;
    }
    return nullptr;
  }

  arpg::EnemyBrain m_brain;
  arpg::Health m_health{40};
  float m_speed = 4.0f;
  int m_dmg = 8;
};

/* ------------------------------------------------------------- registro */

/* Registro explícito (evita que el linker descarte esta unidad de traducción,
 * como pasaría con inicializadores estáticos no referenciados). */
void FL_RegisterBuiltinComponents(FL_ComponentManager &mgr)
{
  mgr.RegisterType("PlayerController", []() -> FL_Component * { return new FL_PlayerController(); });
  mgr.RegisterType("ThirdPersonCamera", []() -> FL_Component * { return new FL_ThirdPersonCamera(); });
  mgr.RegisterType("EnemyAI", []() -> FL_Component * { return new FL_EnemyAI(); });
}

}  // namespace flipendo
