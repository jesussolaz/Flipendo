/* Test de FL_ArpgCore (Fase A, doctrina C++): verifica paridad con la lógica
 * que antes vivía en arpg_core.py. Compilable y ejecutable de forma aislada. */
#include "../../source/gameengine/Flipendo/FL_ArpgCore.hpp"
#include <cstdio>
#include <vector>

using namespace flipendo::arpg;

static int g_fail = 0;
#define CHECK(cond, msg)                                          \
  do {                                                            \
    if (!(cond)) { std::printf("FALLO: %s\n", msg); ++g_fail; }   \
    else { std::printf("ok: %s\n", msg); }                        \
  } while (0)

int main() {
  /* --- Health: daño, invulnerabilidad, muerte --- */
  {
    Health h(100, 0.35f);
    CHECK(h.damage(25) && h.hp() == 75, "Health: primer golpe entra (100->75)");
    CHECK(!h.damage(10) && h.hp() == 75, "Health: invulnerable justo despues");
    h.update(0.4f);  /* pasa la invuln */
    CHECK(h.damage(75) && h.hp() == 0 && h.dead(), "Health: muere a 0");
    CHECK(!h.damage(5), "Health: muerto no recibe mas");
  }

  /* --- ComboStateMachine: arranque, fase active, finish --- */
  {
    ComboStateMachine c;
    CHECK(c.press_attack() == true, "Combo: press arranca combo");
    CHECK(c.phase() == Phase::Startup, "Combo: empieza en startup");
    /* avanzar hasta active (startup=0.10) */
    ComboEvents e = c.update(0.11f);
    CHECK(e.hit_active && c.phase() == Phase::Active, "Combo: entra en active y dispara golpe");
    /* sin buffer, dejar terminar el primer golpe (total=0.10+0.12+0.30=0.52).
     * Tras update(0.11) t=0.11; este update lleva t=0.56 >= 0.52 -> finished. */
    ComboEvents e2 = c.update(0.45f);
    CHECK(e2.finished && !c.attacking(), "Combo: termina sin encadenar");
  }

  /* --- ComboStateMachine: encadenado con buffer --- */
  {
    ComboStateMachine c;
    c.press_attack();
    c.update(0.11f);          /* active */
    c.press_attack();         /* buffer durante el golpe */
    ComboEvents ev = c.update(0.35f);  /* entra en ventana de link */
    CHECK(ev.chained && c.index() == 1, "Combo: encadena al 2o golpe con buffer");
  }

  /* --- lock-on: elige el mas centrado/cercano dentro del cono --- */
  {
    std::vector<Candidate> cands = {
        {1, 0.0f, 10.0f, 0.0f},   /* justo delante, lejos */
        {2, 1.0f, 3.0f, 0.0f},    /* cerca y casi centrado */
        {3, 20.0f, 1.0f, 0.0f},   /* fuera del cono (lateral) */
    };
    int t = pick_lockon_target(0.0f, 0.0f, 0.0f, 1.0f, cands);
    CHECK(t == 2, "lock-on: elige el cercano y centrado (id=2)");

    std::vector<Candidate> lejos = {{9, 0.0f, 100.0f, 0.0f}};
    CHECK(pick_lockon_target(0.0f, 0.0f, 0.0f, 1.0f, lejos) == -1,
          "lock-on: nada fuera de max_dist");
  }

  /* --- EnemyBrain: idle/chase/attack + cooldown --- */
  {
    EnemyBrain b(18.0f, 2.2f, 1.4f);
    CHECK(b.decide(0.016f, 30.0f, true) == EnemyAction::Idle, "IA: lejos -> idle");
    CHECK(b.decide(0.016f, 10.0f, true) == EnemyAction::Chase, "IA: a la vista -> chase");
    CHECK(b.decide(0.016f, 2.0f, true) == EnemyAction::Attack, "IA: en rango -> attack");
    CHECK(b.decide(0.016f, 2.0f, true) == EnemyAction::Hold, "IA: attack en cooldown -> hold");
  }

  std::printf("\n%s\n", g_fail == 0 ? "TODOS LOS TESTS OK" : "HAY FALLOS");
  return g_fail == 0 ? 0 : 1;
}
