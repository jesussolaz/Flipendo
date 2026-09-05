/* FL_ArpgCore — lógica pura del ARPG estilo Kingdom Hearts, en C++.
 *
 * Migrado de arpg_core.py (Fase A, doctrina C++). Sin dependencias del motor:
 * máquinas de estado y matemáticas, testeables de forma aislada. Los componentes
 * del motor (FL_PlayerController, etc.) consumirán esta capa.
 *
 * Header-only C++17. Namespace flipendo::arpg. */
#ifndef __FL_ARPGCORE_HPP__
#define __FL_ARPGCORE_HPP__

#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace flipendo {
namespace arpg {

/* ---------------------------------------------------------------- combos */

struct HitSpec {
  float startup;   /* s antes de conectar */
  float active;    /* s en que conecta */
  float recovery;  /* s de recuperación */
  float link;      /* ventana de encadenado al final */
  int dmg;
  float reach;
};

enum class Phase { Idle, Startup, Active, Recovery };

/* Eventos que devuelve ComboStateMachine::update en un tick. */
struct ComboEvents {
  bool hit_active = false;  /* justo entró en fase active: aplica daño */
  bool chained = false;     /* encadenó al siguiente golpe */
  int chain_index = -1;
  bool finished = false;    /* combo terminó */
};

class ComboStateMachine {
 public:
  ComboStateMachine()
      : m_hits{{{0.10f, 0.12f, 0.30f, 0.35f, 10, 2.2f},
                {0.08f, 0.12f, 0.30f, 0.35f, 12, 2.4f},
                {0.14f, 0.16f, 0.55f, 0.00f, 20, 2.8f}}} {}

  explicit ComboStateMachine(std::vector<HitSpec> hits) : m_hits(std::move(hits)) {}

  bool attacking() const { return m_attacking; }
  int index() const { return m_index; }

  const HitSpec *hit() const {
    if (m_index >= 0 && m_index < static_cast<int>(m_hits.size())) {
      return &m_hits[m_index];
    }
    return nullptr;
  }

  Phase phase() const {
    const HitSpec *h = hit();
    if (!h || !m_attacking) {
      return Phase::Idle;
    }
    if (m_t < h->startup) {
      return Phase::Startup;
    }
    if (m_t < h->startup + h->active) {
      return Phase::Active;
    }
    return Phase::Recovery;
  }

  /* Devuelve true si arrancó un combo nuevo; si ya atacaba, almacena el input. */
  bool press_attack() {
    if (!m_attacking) {
      m_attacking = true;
      m_index = 0;
      m_t = 0.0f;
      m_buffered = false;
      return true;
    }
    m_buffered = true;
    return false;
  }

  ComboEvents update(float dt) {
    ComboEvents ev;
    if (!m_attacking) {
      return ev;
    }
    const HitSpec *h = hit();
    const Phase prev = phase();
    m_t += dt;
    const Phase now = phase();
    if (prev != Phase::Active && now == Phase::Active) {
      ev.hit_active = true;
    }
    const float total = h->startup + h->active + h->recovery;
    const bool in_link = (m_t >= total - h->link) && h->link > 0.0f;
    if (m_buffered && in_link && m_index + 1 < static_cast<int>(m_hits.size())) {
      m_index += 1;
      m_t = 0.0f;
      m_buffered = false;
      ev.chained = true;
      ev.chain_index = m_index;
    }
    else if (m_t >= total) {
      m_attacking = false;
      m_index = -1;
      m_buffered = false;
      ev.finished = true;
    }
    return ev;
  }

 private:
  std::vector<HitSpec> m_hits;
  bool m_attacking = false;
  int m_index = -1;
  float m_t = 0.0f;
  bool m_buffered = false;
};

/* ---------------------------------------------------------------- lock-on */

struct Candidate {
  int id;
  float x, y, z;
};

/* Elige objetivo dentro del cono de visión, más cercano ponderado por ángulo.
 * Devuelve el id elegido o -1. Solo matemáticas. */
inline int pick_lockon_target(float px, float py, float fx, float fy,
                              const std::vector<Candidate> &candidates,
                              float max_dist = 25.0f, float half_angle_deg = 55.0f) {
  float fl = std::hypot(fx, fy);
  if (fl == 0.0f) {
    fl = 1.0f;
  }
  fx /= fl;
  fy /= fl;
  int best = -1;
  float best_score = 0.0f;
  bool have = false;
  for (const Candidate &c : candidates) {
    const float dx = c.x - px;
    const float dy = c.y - py;
    const float d = std::hypot(dx, dy);
    if (d < 0.001f || d > max_dist) {
      continue;
    }
    float cosang = (dx * fx + dy * fy) / d;
    cosang = std::fmax(-1.0f, std::fmin(1.0f, cosang));
    const float ang = std::acos(cosang) * 180.0f / static_cast<float>(M_PI);
    if (ang > half_angle_deg) {
      continue;
    }
    const float score = d * (1.0f + ang / 90.0f);
    if (!have || score < best_score) {
      best = c.id;
      best_score = score;
      have = true;
    }
  }
  return best;
}

/* ---------------------------------------------------------------- salud */

class Health {
 public:
  explicit Health(int hp = 100, float invuln = 0.35f)
      : m_max(hp), m_hp(hp), m_invuln(invuln) {}

  int max_hp() const { return m_max; }
  int hp() const { return m_hp; }
  bool dead() const { return m_hp <= 0; }

  void update(float dt) {
    if (m_inv_t > 0.0f) {
      m_inv_t -= dt;
    }
  }

  /* True si el daño entró (no invulnerable, vivo). */
  bool damage(int amount) {
    if (m_inv_t > 0.0f || m_hp <= 0) {
      return false;
    }
    m_hp = m_hp - amount;
    if (m_hp < 0) {
      m_hp = 0;
    }
    m_inv_t = m_invuln;
    return true;
  }

 private:
  int m_max;
  int m_hp;
  float m_invuln;
  float m_inv_t = 0.0f;
};

/* ---------------------------------------------------------------- IA */

enum class EnemyAction { Idle, Chase, Attack, Hold };

class EnemyBrain {
 public:
  EnemyBrain(float sight = 18.0f, float attack_range = 2.2f, float attack_cd = 1.4f)
      : m_sight(sight), m_range(attack_range), m_cd_max(attack_cd) {}

  float sight() const { return m_sight; }
  EnemyAction state() const { return m_state; }

  EnemyAction decide(float dt, float dist_to_player, bool sees_player) {
    m_cd = std::fmax(0.0f, m_cd - dt);
    if (dist_to_player <= m_range && sees_player) {
      if (m_cd <= 0.0f) {
        m_cd = m_cd_max;
        m_state = EnemyAction::Attack;
        return EnemyAction::Attack;
      }
      m_state = EnemyAction::Hold;
      return EnemyAction::Hold;
    }
    if (sees_player && dist_to_player <= m_sight) {
      m_state = EnemyAction::Chase;
      return EnemyAction::Chase;
    }
    m_state = EnemyAction::Idle;
    return EnemyAction::Idle;
  }

 private:
  float m_sight, m_range, m_cd_max;
  float m_cd = 0.0f;
  EnemyAction m_state = EnemyAction::Idle;
};

}  // namespace arpg
}  // namespace flipendo

#endif  // __FL_ARPGCORE_HPP__
