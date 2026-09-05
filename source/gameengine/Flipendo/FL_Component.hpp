/* FL_Component — sistema de componentes de juego NATIVO en C++ para Flipendo.
 *
 * El motor heredado solo tiene KX_PythonComponent (Python, bajo #ifdef WITH_PYTHON).
 * Esta es la alternativa nativa exigida por la doctrina C++: componentes con
 * Start()/Update() en C++ puro, atados a los objetos por una propiedad de juego
 * "fl_component" y tickeados desde KX_Scene::LogicUpdateFrame.
 *
 * Doctrina: politicas/LENGUAJE-CPP.md. */
#ifndef __FL_COMPONENT_HPP__
#define __FL_COMPONENT_HPP__

#include <functional>
#include <map>
#include <string>
#include <vector>

class KX_GameObject;
class KX_Scene;

namespace flipendo {

/* Base de todo componente nativo. Sin Python. */
class FL_Component {
 public:
  virtual ~FL_Component() = default;
  void SetOwner(KX_GameObject *owner) { m_owner = owner; }
  KX_GameObject *Owner() const { return m_owner; }
  virtual void Start() {}
  virtual void Update(float dt) {}

 protected:
  KX_GameObject *m_owner = nullptr;
};

/* Registro + orquestación. Un componente se registra por nombre con una factoría;
 * al arrancar una escena, cada objeto con propiedad "fl_component" == <nombre>
 * recibe una instancia, y el manager la tickea cada frame. */
class FL_ComponentManager {
 public:
  using Factory = std::function<FL_Component *()>;

  static FL_ComponentManager &Get();

  void RegisterType(const std::string &name, Factory factory);
  /* Escanea los objetos de la escena (una sola vez) y ata componentes. */
  void AttachScene(KX_Scene *scene);
  /* Tickea todos los componentes vivos; descarta los de objetos ya destruidos. */
  void Tick(KX_Scene *scene, float dt);

 private:
  struct Instance {
    KX_GameObject *owner;
    FL_Component *comp;
  };
  std::map<std::string, Factory> m_factories;
  std::vector<Instance> m_instances;
  std::vector<KX_Scene *> m_attachedScenes;
  bool m_builtinsRegistered = false;
};

/* Helper de registro estático. */
/* Definida en FL_ArpgComponents.cpp: registra los componentes integrados. */
void FL_RegisterBuiltinComponents(FL_ComponentManager &mgr);

struct FL_AutoRegister {
  FL_AutoRegister(const std::string &name, FL_ComponentManager::Factory f) {
    FL_ComponentManager::Get().RegisterType(name, std::move(f));
  }
};

#define FL_REGISTER_COMPONENT(NAME, CLASS) \
  static ::flipendo::FL_AutoRegister _fl_reg_##CLASS(NAME, []() -> ::flipendo::FL_Component * { return new CLASS(); })

}  // namespace flipendo

#endif  // __FL_COMPONENT_HPP__
