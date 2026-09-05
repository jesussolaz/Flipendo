/* Implementación del manager de componentes nativos. Ver FL_Component.hpp. */
#include "FL_Component.hpp"

#include "EXP_Value.h"
#include "KX_GameObject.h"
#include "KX_Scene.h"
#include "EXP_ListValue.h"

namespace flipendo {

FL_ComponentManager &FL_ComponentManager::Get()
{
  static FL_ComponentManager instance;
  return instance;
}

void FL_ComponentManager::RegisterType(const std::string &name, Factory factory)
{
  m_factories[name] = std::move(factory);
}

void FL_ComponentManager::AttachScene(KX_Scene *scene)
{
  if (!m_builtinsRegistered) {
    m_builtinsRegistered = true;
    FL_RegisterBuiltinComponents(*this);
  }
  for (KX_Scene *s : m_attachedScenes) {
    if (s == scene) {
      return;  /* ya escaneada */
    }
  }
  m_attachedScenes.push_back(scene);

  EXP_ListValue<KX_GameObject> *objs = scene->GetObjectList();
  if (!objs) {
    return;
  }
  for (KX_GameObject *obj : objs) {
    EXP_Value *prop = obj->GetProperty("fl_component");
    if (!prop) {
      continue;
    }
    const std::string name = prop->GetText();
    auto it = m_factories.find(name);
    if (it == m_factories.end()) {
      continue;
    }
    FL_Component *comp = it->second();
    comp->SetOwner(obj);
    comp->Start();
    m_instances.push_back({obj, comp});
  }
}

void FL_ComponentManager::Tick(KX_Scene *scene, float dt)
{
  AttachScene(scene);

  EXP_ListValue<KX_GameObject> *objs = scene->GetObjectList();
  for (auto it = m_instances.begin(); it != m_instances.end();) {
    /* Si el objeto ya no está en la escena (endObject), soltar el componente. */
    if (!objs || !objs->SearchValue(it->owner)) {
      delete it->comp;
      it = m_instances.erase(it);
      continue;
    }
    it->comp->Update(dt);
    ++it;
  }
}

}  // namespace flipendo
