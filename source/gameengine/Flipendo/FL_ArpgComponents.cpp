/* FL_ArpgComponents — gameplay ARPG estilo Kingdom Hearts, NATIVO en C++.
 *
 * Migrado de arpg.py (Fase A, doctrina C++). Sustituye los KX_PythonComponent
 * (PlayerController, ThirdPersonCamera, EnemyAI) por componentes C++ que usan
 * FL_ArpgCore (lógica pura) + la API C++ del motor. Cero Python.
 *
 * Atado por propiedad de juego "fl_component" = "PlayerController" | "ThirdPersonCamera" | "EnemyAI".
 *
 * Noche de ÁNIMA (2026-09-12), carril CPP — lo que se añadió y por qué:
 *
 *  - ANIMACIÓN DEL PERSONAJE. El PlayerController busca en sus hijos (recursivo) el
 *    objeto con la propiedad "fl_rig" (el armature) y lee en él las propiedades
 *    "fl_anim_<estado>" = "Accion:inicio:fin". Con eso lleva una máquina de estados
 *    de animación (Idle/Andar/Correr en bucle, Ataque1-3/Salto/Golpe de un tiro).
 *    Los ataques se reproducen a la velocidad que hace que la acción dure EXACTAMENTE
 *    lo que dice el HitSpec del combo: playback_speed = (frames/fps) / (startup+active+
 *    recovery). Así el frame de impacto del animador cae en la fase «active» sin que
 *    nadie tenga que cuadrar números a mano. Sin rig todo sigue como antes.
 *    Convención completa: politicas/ARPG-ANIMACION.md.
 *
 *  - CÁMARA ORBITAL. El ratón orbita alrededor del jugador (yaw libre, pitch entre
 *    -10° y 60°), con suavizado, sin invertir. El cursor se esconde y se recentra
 *    cada frame para que no se salga de la ventana. Con FL_UI_EXIT (el Player
 *    conducido por `jugar`) NO se toca el puntero: si no, cada verificación de la
 *    noche secuestraría el ratón de quien esté usando el ordenador.
 *
 *  - FL_ARPG_AUTOPLAY. Guion de pulsaciones por frame ("w:30-150;shift:90-150;j:170")
 *    que el PlayerController interpreta como teclado. Es la forma de que una captura y
 *    un volcado demuestren que el personaje anda, corre y pega sin nadie al teclado.
 *
 * Prioridades de BL_Action: NÚMERO MENOR = MÁS PRIORIDAD. Una acción con prioridad 1
 * (locomoción) no puede arrancar mientras otra de prioridad 0 (ataque) no ha
 * terminado; al revés sí. Es lo que bloquea el bucle de andar durante el golpe. */
#include "FL_Component.hpp"
#include "FL_ArpgCore.hpp"

#include "KX_GameObject.hpp"
#include "KX_Camera.hpp"
#include "KX_Scene.hpp"
#include "KX_Globals.hpp"
#include "KX_KetsjiEngine.hpp"
#include "BL_Action.hpp"
#include "SCA_IInputDevice.hpp"
#include "SCA_InputEvent.hpp"
#include "PHY_ICharacter.hpp"
#include "PHY_IPhysicsEnvironment.hpp"
#include "RAS_ICanvas.hpp"
#include "CM_Message.hpp"
#include "EXP_IntValue.hpp"
#include "EXP_StringValue.hpp"
#include "EXP_ListValue.hpp"
#include "MT_Vector3.h"
#include "MT_Matrix3x3.h"

#include <cmath>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

namespace flipendo {

/* ------------------------------------------------------------- helpers */

static float TicDt()
{
  double r = KX_GetActiveEngine() ? KX_GetActiveEngine()->GetTicRate() : 60.0;
  return 1.0f / (float)(r > 0 ? r : 60.0);
}

/* Fotogramas por segundo a los que el motor reproduce las acciones (el fps de la
 * escena, 30 por convención de ÁNIMA). Es la cifra que convierte «frames de una
 * acción» en segundos, así que la velocidad de los ataques se calcula con ella y
 * no con un 30 escrito a mano: si la escena no está a 30, sigue cuadrando. */
static float AnimFps()
{
  double r = KX_GetActiveEngine() ? KX_GetActiveEngine()->GetAnimFrameRate() : 30.0;
  return (float)(r > 0 ? r : 30.0);
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

static void SetIntProp(KX_GameObject *o, const std::string &name, int v)
{
  o->SetProperty(name, new EXP_IntValue(v));
}

static void SetStrProp(KX_GameObject *o, const std::string &name, const std::string &v)
{
  o->SetProperty(name, new EXP_StringValue(v, name));
}

static std::string GetStrProp(KX_GameObject *o, const std::string &name)
{
  EXP_Value *p = o ? o->GetProperty(name) : nullptr;
  return p ? p->GetText() : std::string();
}

static float GetFloatProp(KX_GameObject *o, const std::string &name, float def)
{
  EXP_Value *p = o ? o->GetProperty(name) : nullptr;
  return p ? (float)p->GetNumber() : def;
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

static KX_GameObject *FindByName(KX_Scene *scene, const std::string &name)
{
  EXP_ListValue<KX_GameObject> *objs = scene ? scene->GetObjectList() : nullptr;
  if (!objs) return nullptr;
  for (KX_GameObject *o : objs) {
    if (o->GetName() == name) return o;
  }
  return nullptr;
}

/* ¿Sigue el objeto en la escena? KX_GameObject no tiene IsBeingDestroyed(): el idioma
 * del motor es preguntarle a la lista (lo mismo que hace FL_ComponentManager::Tick). */
static bool StillInScene(KX_Scene *scene, KX_GameObject *o)
{
  EXP_ListValue<KX_GameObject> *objs = scene ? scene->GetObjectList() : nullptr;
  return o && objs && objs->SearchValue(o);
}

/* ------------------------------------------------------------- FL_ARPG_AUTOPLAY
 *
 * Guion de pulsaciones por frame lógico:  "w:30-150;shift:90-150;j:170;j:200"
 *   tecla:frame        pulsada solo en ese frame (un toque)
 *   tecla:desde-hasta  mantenida entre ambos, inclusive
 *   teclas: w a s d shift j space
 * El frame es el número de tics lógicos desde que arrancó el componente (el mismo
 * contador que FL_GAME_DUMP_FRAME y FL_UI_SHOT_FRAME, que también cuentan tics).
 * Solo se lee si la variable está; si no, la clase no hace nada. */

class FL_ArpgAutoplay {
 public:
  bool Active() const
  {
    return m_active;
  }

  int Count() const
  {
    return (int)m_pulses.size();
  }

  void Parse(const char *script)
  {
    m_active = true;
    std::string s = script ? script : "";
    size_t pos = 0;
    while (pos <= s.size()) {
      size_t sep = s.find(';', pos);
      if (sep == std::string::npos) sep = s.size();
      std::string tok = Trim(s.substr(pos, sep - pos));
      pos = sep + 1;
      if (tok.empty()) continue;
      size_t colon = tok.find(':');
      if (colon == std::string::npos) {
        CM_Error("FL_ARPG_AUTOPLAY: pulso sin ':' -> '" << tok << "' (se ignora)");
        continue;
      }
      Pulse p;
      p.key = Lower(Trim(tok.substr(0, colon)));
      std::string spec = Trim(tok.substr(colon + 1));
      if (!KnownKey(p.key)) {
        CM_Error("FL_ARPG_AUTOPLAY: tecla desconocida '" << p.key
                                                       << "' (valen w a s d shift j space)");
        continue;
      }
      size_t dash = spec.find('-');
      if (dash == std::string::npos) {
        p.from = p.to = atoi(spec.c_str());
      }
      else {
        p.from = atoi(spec.substr(0, dash).c_str());
        p.to = atoi(spec.substr(dash + 1).c_str());
        if (p.to < p.from) std::swap(p.from, p.to);
      }
      if (p.from <= 0) {
        CM_Error("FL_ARPG_AUTOPLAY: frame no válido en '" << tok << "' (se ignora)");
        continue;
      }
      m_pulses.push_back(p);
    }
  }

  bool Held(const char *key, int frame) const
  {
    for (const Pulse &p : m_pulses) {
      if (frame >= p.from && frame <= p.to && p.key == key) {
        return true;
      }
    }
    return false;
  }

 private:
  struct Pulse {
    std::string key;
    int from = 0, to = 0;
  };

  static std::string Trim(const std::string &s)
  {
    size_t a = s.find_first_not_of(" \t\r\n");
    size_t b = s.find_last_not_of(" \t\r\n");
    return (a == std::string::npos) ? std::string() : s.substr(a, b - a + 1);
  }

  static std::string Lower(std::string s)
  {
    for (char &c : s) {
      if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    }
    return s;
  }

  static bool KnownKey(const std::string &k)
  {
    return k == "w" || k == "a" || k == "s" || k == "d" || k == "shift" || k == "j" ||
           k == "space";
  }

  std::vector<Pulse> m_pulses;
  bool m_active = false;
};

/* ------------------------------------------------------------- clips de animación
 *
 * "Accion:inicio:fin", tal cual va en la propiedad fl_anim_*. Inicio y fin son frames
 * de la acción (los que ve el animador); la duración en segundos es (fin-inicio)/fps. */

struct AnimClip {
  std::string name;
  float start = 1.0f;
  float end = 1.0f;
  bool ok = false;

  float Frames() const
  {
    return std::fabs(end - start);
  }
};

static AnimClip ParseClip(const std::string &spec)
{
  AnimClip c;
  size_t a = spec.find(':');
  size_t b = (a == std::string::npos) ? a : spec.find(':', a + 1);
  if (a == std::string::npos || b == std::string::npos) {
    return c;
  }
  c.name = spec.substr(0, a);
  c.start = (float)atof(spec.substr(a + 1, b - a - 1).c_str());
  c.end = (float)atof(spec.substr(b + 1).c_str());
  c.ok = !c.name.empty() && c.end > c.start;
  return c;
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

    /* Rig y clips. Sin rig, nada de esto se usa y el componente funciona como antes. */
    m_rig = FindRig();
    if (m_rig) {
      m_idle = ReadClip("fl_anim_idle");
      m_walk = ReadClip("fl_anim_walk");
      m_run = ReadClip("fl_anim_run");
      m_attack[0] = ReadClip("fl_anim_attack1");
      m_attack[1] = ReadClip("fl_anim_attack2");
      m_attack[2] = ReadClip("fl_anim_attack3");
      m_jump = ReadClip("fl_anim_jump");
      m_hit = ReadClip("fl_anim_hit");
      CM_Message("Flipendo: PlayerController con rig '"
                 << m_rig->GetName() << "' — idle=" << Describe(m_idle) << " walk="
                 << Describe(m_walk) << " run=" << Describe(m_run) << " attack="
                 << Describe(m_attack[0]) << "," << Describe(m_attack[1]) << ","
                 << Describe(m_attack[2]) << " jump=" << Describe(m_jump)
                 << " hit=" << Describe(m_hit) << " (fps anim " << AnimFps() << ")");
    }
    else {
      CM_Message("Flipendo: PlayerController '" << m_owner->GetName()
                                                << "' sin rig (ningún hijo con fl_rig): "
                                                   "locomoción y combo sin animación");
    }

    const char *script = std::getenv("FL_ARPG_AUTOPLAY");
    if (script && script[0]) {
      m_auto.Parse(script);
      CM_Message("FL_ARPG_AUTOPLAY: " << m_auto.Count() << " pulsos programados en '" << script
                                      << "'");
    }
  }

  void Update(float) override
  {
    const float dt = TicDt();
    ++m_frame;
    KX_Scene *scene = m_owner->GetScene();
    m_health.update(dt);
    const bool autoOn = m_auto.Active();
    const bool rigOk = m_rig && StillInScene(scene, m_rig);
    if (!rigOk) {
      m_rig = nullptr;
    }

    /* combo: J o click izquierdo (o el guion) */
    const bool atk = KeyActive(SCA_IInputDevice::JKEY) || KeyActive(SCA_IInputDevice::LEFTMOUSE) ||
                     (autoOn && m_auto.Held("j", m_frame));
    bool newCombo = false;
    if (atk && !m_prevAtk) {
      newCombo = m_combo.press_attack();
    }
    m_prevAtk = atk;
    arpg::ComboEvents ev = m_combo.update(dt);
    if (ev.hit_active) {
      ApplyHits(scene);
    }
    SetStrProp(m_owner, "combo",
               std::string(PhaseName(m_combo.phase())) + ":" + std::to_string(m_combo.index()));
    if (rigOk) {
      if (newCombo) {
        PlayAttack(0);
      }
      else if (ev.chained) {
        PlayAttack(ev.chain_index);
      }
    }

    /* Mientras pega, los pies quietos (solo cuando hay animación de ataque que
     * bloquear: sin rig se mueve como siempre). */
    const bool locked = rigOk && m_attack[0].ok && m_combo.attacking();

    /* movimiento WASD relativo a cámara; SHIFT corre (x1,7) */
    const bool run = KeyActive(SCA_IInputDevice::LEFTSHIFTKEY) ||
                     (autoOn && m_auto.Held("shift", m_frame));
    MT_Vector3 mv = locked ? MT_Vector3(0, 0, 0) : MoveInput(autoOn);
    bool moving = false;
    if (mv.length() > 0.001f) {
      moving = true;
      MT_Vector3 world = CamRelative(scene, mv);
      Walk(world * (m_speed * (run ? m_runMul : 1.0f)));
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
    const bool jp = KeyActive(SCA_IInputDevice::SPACEKEY) ||
                    (autoOn && m_auto.Held("space", m_frame));
    if (jp && !m_prevJump && !locked && m_char && m_char->OnGround()) {
      m_char->Jump();
      if (rigOk && m_jump.ok) {
        PlayOneShot(m_jump, Anim::Jump, 1);
      }
    }
    m_prevJump = jp;

    /* daño recibido (propiedad puesta por el enemigo o el test) */
    int dmg;
    if (ConsumeInt(m_owner, "arpg_player_hit", dmg)) {
      if (m_health.damage(dmg)) {
        SetIntProp(m_owner, "hp", m_health.hp());
        /* El golpe recibido no interrumpe el combo: el ataque ya tiene su acción y
         * su prioridad, y pisarla dejaría el combo sin animación. */
        if (rigOk && m_hit.ok && !m_combo.attacking()) {
          PlayOneShot(m_hit, Anim::Hit, 0);
        }
      }
    }

    if (rigOk) {
      UpdateAnim(moving, run);
    }
  }

 private:
  enum class Anim { None, Idle, Walk, Run, Attack, Jump, Hit };

  static const char *PhaseName(arpg::Phase p)
  {
    switch (p) {
      case arpg::Phase::Startup: return "startup";
      case arpg::Phase::Active: return "active";
      case arpg::Phase::Recovery: return "recovery";
      default: return "idle";
    }
  }

  static std::string Describe(const AnimClip &c)
  {
    if (!c.ok) return "-";
    return c.name + "[" + std::to_string((int)c.start) + ".." + std::to_string((int)c.end) + "]";
  }

  /* El rig es el propio objeto o el primer descendiente con la propiedad fl_rig. */
  KX_GameObject *FindRig()
  {
    if (m_owner->GetProperty("fl_rig")) {
      return m_owner;
    }
    for (KX_GameObject *child : m_owner->GetChildrenRecursive()) {
      if (child->GetProperty("fl_rig")) {
        return child;
      }
    }
    return nullptr;
  }

  AnimClip ReadClip(const char *prop)
  {
    const std::string spec = GetStrProp(m_rig, prop);
    if (spec.empty()) {
      return AnimClip();
    }
    AnimClip c = ParseClip(spec);
    if (!c.ok) {
      CM_Error("Flipendo: " << prop << "='" << spec
                            << "' no es 'Accion:inicio:fin' con fin > inicio (se ignora)");
    }
    return c;
  }

  MT_Vector3 MoveInput(bool autoOn)
  {
    const bool d = KeyActive(SCA_IInputDevice::DKEY) || (autoOn && m_auto.Held("d", m_frame));
    const bool a = KeyActive(SCA_IInputDevice::AKEY) || (autoOn && m_auto.Held("a", m_frame));
    const bool w = KeyActive(SCA_IInputDevice::WKEY) || (autoOn && m_auto.Held("w", m_frame));
    const bool s = KeyActive(SCA_IInputDevice::SKEY) || (autoOn && m_auto.Held("s", m_frame));
    float x = (d ? 1.f : 0.f) - (a ? 1.f : 0.f);
    float y = (w ? 1.f : 0.f) - (s ? 1.f : 0.f);
    MT_Vector3 v(x, y, 0);
    return v.length() > 0 ? v.normalized() : v;
  }

  MT_Vector3 CamRelative(KX_Scene *scene, const MT_Vector3 &mv)
  {
    KX_Camera *cam = scene->GetActiveCamera();
    if (!cam) return MT_Vector3(mv.x(), mv.y(), 0);
    /* Una cámara mira por su -Z; su Y es ARRIBA. Antes se tomaba la columna Y como
     * «adelante», que valía cuando AlignAxisToVect dejaba la cámara nivelada, pero con
     * la orientación de mirada construida a mano esa columna apunta al cielo y su
     * resto horizontal mira HACIA ATRÁS cuando la cámara está por debajo del punto
     * que mira: el jugador corría hacia la cámara (medido en el charco: W lo llevaba
     * de y=-31 a y=-57 con la cámara a y=-37). */
    MT_Matrix3x3 o = cam->NodeGetWorldOrientation();
    MT_Vector3 f = o.getColumn(2) * -1.0f; f.z() = 0;
    MT_Vector3 r = o.getColumn(0); r.z() = 0;
    if (f.length() < 0.001f) { f = o.getColumn(1); f.z() = 0; }
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

  /* ---- máquina de estados de animación ---- */

  /* Ataque N en modo PLAY, prioridad 0 (la más alta), a la velocidad que hace que la
   * acción dure lo que el HitSpec: speed = (frames/fps) / (startup+active+recovery).
   * Se para la capa antes de reproducir: BL_Action::Play se niega a relanzar «la
   * misma acción con los mismos parámetros» mientras no ha terminado, y un combo
   * nuevo que empieza justo cuando acaba el Ataque1 anterior se quedaría sin
   * animación. El blend-in parte de la pose actual del armature, no de la acción
   * anterior, así que parar la capa no da un salto visible. */
  void PlayAttack(int index)
  {
    if (index < 0) index = 0;
    if (index > 2) index = 2;
    const AnimClip &c = m_attack[index];
    if (!c.ok) {
      return;
    }
    const arpg::HitSpec *h = m_combo.hit();
    const float total = h ? (h->startup + h->active + h->recovery) : 0.5f;
    const float frames = c.Frames();
    const float speed = (frames > 0.0f && total > 0.0f) ? (frames / AnimFps()) / total : 1.0f;
    m_rig->StopAction(0);
    m_rig->PlayAction(c.name, c.start, c.end, 0, 0, 3.0f, BL_Action::ACT_MODE_PLAY, 0.0f, 0,
                      speed, BL_Action::ACT_BLEND_BLEND);
    m_anim = Anim::Attack;
    m_oneShot = false;
  }

  void PlayOneShot(const AnimClip &c, Anim which, short priority)
  {
    m_rig->StopAction(0);
    m_rig->PlayAction(c.name, c.start, c.end, 0, priority, 4.0f, BL_Action::ACT_MODE_PLAY, 0.0f,
                      0, 1.0f, BL_Action::ACT_BLEND_BLEND);
    m_anim = which;
    m_oneShot = true;
  }

  void UpdateAnim(bool moving, bool run)
  {
    /* El ataque manda mientras el combo esté vivo. */
    if (m_anim == Anim::Attack && m_combo.attacking()) {
      return;
    }
    /* Salto/Golpe: se dejan terminar. */
    if (m_oneShot) {
      if (!m_rig->IsActionDone(0)) {
        return;
      }
      m_oneShot = false;
    }
    Anim wanted = Anim::Idle;
    const AnimClip *c = &m_idle;
    if (moving) {
      if (run && m_run.ok) {
        wanted = Anim::Run;
        c = &m_run;
      }
      else if (m_walk.ok) {
        wanted = Anim::Walk;
        c = &m_walk;
      }
    }
    if (!c->ok) {
      return;
    }
    /* Llamar cada frame es barato: BL_Action::Play devuelve false sin tocar nada si
     * la misma acción ya está sonando con los mismos parámetros. Y si un ataque
     * (prioridad 0) aún no ha terminado, también dice que no: se reintenta al frame
     * siguiente, que es justo lo que se quiere. */
    const float blendin = (wanted == Anim::Idle) ? 8.0f : 5.0f;
    const bool played = m_rig->PlayAction(c->name, c->start, c->end, 0, 1, blendin,
                                          BL_Action::ACT_MODE_LOOP, 0.0f, 0, 1.0f,
                                          BL_Action::ACT_BLEND_BLEND);
    if (played || m_anim == wanted) {
      m_anim = wanted;
    }
  }

  arpg::ComboStateMachine m_combo;
  arpg::Health m_health{100};
  PHY_ICharacter *m_char = nullptr;
  float m_speed = 7.0f;
  float m_runMul = 1.7f;
  bool m_prevAtk = false, m_prevJump = false;

  KX_GameObject *m_rig = nullptr;
  AnimClip m_idle, m_walk, m_run, m_jump, m_hit;
  AnimClip m_attack[3];
  Anim m_anim = Anim::None;
  bool m_oneShot = false;

  int m_frame = 0;
  FL_ArpgAutoplay m_auto;
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
    m_dist = GetFloatProp(m_owner, "fl_cam_dist", 7.0f);
    m_height = GetFloatProp(m_owner, "fl_cam_altura", 2.6f);
    /* Elevación de la órbita en grados. La escena del charco la pide a -5° para que
     * el horizonte quede al 40 % del encuadre y el sol se refleje delante del
     * personaje; el defecto, 18°, es la vista clásica por encima del hombro. */
    m_pitch = GetFloatProp(m_owner, "fl_cam_pitch", 18.0f) * (float)M_PI / 180.0f;
    if (m_pitch < kPitchMin) m_pitch = kPitchMin;
    if (m_pitch > kPitchMax) m_pitch = kPitchMax;
    /* rad por píxel: 0,0025 -> un barrido de 1280 px son 183°. */
    m_sens = GetFloatProp(m_owner, "fl_cam_sens", 0.0025f);
    m_yawTarget = m_yaw;
    m_pitchTarget = m_pitch;

    /* Con FL_UI_EXIT el Player lo conduce un guion (`jugar`), no una persona: no se
     * esconde ni se recentra el puntero, que es de quien esté usando el ordenador. */
    const char *e = std::getenv("FL_UI_EXIT");
    m_mouse = !(e && e[0] && e[0] != '0');
    if (m_mouse) {
      if (RAS_ICanvas *canvas = KX_GetActiveEngine() ? KX_GetActiveEngine()->GetCanvas() : nullptr) {
        canvas->SetMouseState(RAS_ICanvas::MOUSE_INVISIBLE);
        canvas->SetMousePosition(canvas->GetWidth() / 2, canvas->GetHeight() / 2);
      }
    }
  }

  void Update(float) override
  {
    const float dt = TicDt();
    KX_Scene *scene = m_owner->GetScene();
    KX_GameObject *target = FindByName(scene, "Player");
    if (!target) return;

    /* La cámara activa se reafirma cada tic: con otra cámara en la escena (la de los
     * renders del editor) el Player arrancaba pintando desde ella aunque Start() ya
     * hubiera activado esta. Cuesta una comparación de punteros. */
    if (KX_Camera *self = dynamic_cast<KX_Camera *>(m_owner)) {
      if (scene->GetActiveCamera() != self) {
        scene->SetActiveCamera(self);
      }
    }

    if (m_mouse) {
      Orbit();
    }
    /* Suavizado independiente de los fps: k = 1 - e^(-14 dt) es 0,21 a 60 Hz. */
    const float k = 1.0f - std::exp(-14.0f * dt);
    m_yaw += (m_yawTarget - m_yaw) * k;
    m_pitch += (m_pitchTarget - m_pitch) * k;

    MT_Vector3 tpos = target->NodeGetWorldPosition();
    MT_Vector3 off(std::sin(-m_yaw) * std::cos(m_pitch),
                   -std::cos(m_yaw) * std::cos(m_pitch),
                   std::sin(m_pitch));
    off = off * m_dist;
    MT_Vector3 desired = tpos + MT_Vector3(0, 0, m_height) + off;

    MT_Vector3 cur = m_owner->NodeGetWorldPosition();
    const float a = 1.0f - std::exp(-10.0f * dt); /* 0,15 a 60 Hz, como antes */
    m_owner->NodeSetWorldPosition(cur * (1.0f - a) + desired * a);

    /* Orientación construida a mano: una cámara mira por su -Z, así que +Z es
     * «hacia atrás» (de la mira a la cámara), +X = arriba_mundo × atrás y +Y lo que
     * cierra la terna. ANTES se hacía con dos AlignAxisToVect (Z a la mira y luego Y
     * a la vertical, ambas con fac 1): la segunda deshace la primera y la cámara
     * queda mirando al horizonte — la captura del Player salía toda cielo, con el
     * suelo como una franja abajo (medido en el frame 60, 2026-09-12). Con el pitch
     * acotado a [-10°, 60°] «atrás» nunca es vertical y la terna no degenera. */
    MT_Vector3 look = tpos + MT_Vector3(0, 0, m_height * 0.7f);
    MT_Vector3 back = m_owner->NodeGetWorldPosition() - look;
    if (back.length() > 0.001f) {
      back = back.normalized();
      MT_Vector3 right = MT_Vector3(0, 0, 1).cross(back);
      if (right.length() > 0.001f) {
        right = right.normalized();
        MT_Vector3 up = back.cross(right);
        m_owner->NodeSetGlobalOrientation(MT_Matrix3x3(right.x(), up.x(), back.x(),
                                                       right.y(), up.y(), back.y(),
                                                       right.z(), up.z(), back.z()));
      }
    }
  }

 private:
  /* Órbita con el ratón: desplazamiento respecto al centro de la ventana y vuelta
   * al centro. Solo se mira el ratón en los frames en que hubo evento (m_values
   * trae más de un valor); si no, tras recentrar sin evento se aplicaría el mismo
   * desplazamiento una y otra vez. En macOS el recentrado (GHOST setCursorPosition)
   * empuja él mismo un evento al centro, así que el siguiente desplazamiento sale
   * limpio. Sin invertir: ratón a la derecha gira la vista a la derecha; ratón
   * abajo sube la cámara (mira hacia abajo). */
  void Orbit()
  {
    KX_KetsjiEngine *engine = KX_GetActiveEngine();
    SCA_IInputDevice *dev = engine ? engine->GetInputDevice() : nullptr;
    RAS_ICanvas *canvas = engine ? engine->GetCanvas() : nullptr;
    if (!dev || !canvas) {
      return;
    }
    const SCA_InputEvent &ex = dev->GetInput(SCA_IInputDevice::MOUSEX);
    const SCA_InputEvent &ey = dev->GetInput(SCA_IInputDevice::MOUSEY);
    const bool moved = ex.m_values.size() > 1 || ey.m_values.size() > 1;
    if (!moved || ex.m_values.empty() || ey.m_values.empty()) {
      return;
    }
    const int cx = canvas->GetWidth() / 2;
    const int cy = canvas->GetHeight() / 2;
    const int dx = ex.m_values.back() - cx;
    const int dy = ey.m_values.back() - cy;
    if (m_primed) {
      m_yawTarget += (float)dx * m_sens;
      m_pitchTarget += (float)dy * m_sens;
      if (m_pitchTarget < kPitchMin) m_pitchTarget = kPitchMin;
      if (m_pitchTarget > kPitchMax) m_pitchTarget = kPitchMax;
    }
    m_primed = true;
    if (dx != 0 || dy != 0) {
      canvas->SetMousePosition(cx, cy);
    }
  }

  static constexpr float kPitchMin = -10.0f * (float)M_PI / 180.0f;
  static constexpr float kPitchMax = 60.0f * (float)M_PI / 180.0f;

  float m_dist = 7.0f, m_height = 2.6f;
  float m_yaw = 0.0f, m_pitch = 0.31416f;
  float m_yawTarget = 0.0f, m_pitchTarget = 0.31416f;
  float m_sens = 0.0025f;
  bool m_mouse = true;
  bool m_primed = false;
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
