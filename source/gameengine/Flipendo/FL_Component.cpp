/* Implementación del manager de componentes nativos. Ver FL_Component.hpp. */
#include "FL_Component.hpp"

#include "EXP_Value.hpp"
#include "KX_GameObject.hpp"
#include "KX_Scene.hpp"
#include "EXP_ListValue.hpp"
#include "CM_Message.hpp"
#include "FL_RenderToTexture.hpp"

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
    FL_RegisterRenderToTextureComponents(*this);
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
  int pedidos = 0;
  int atados = 0;
  for (KX_GameObject *obj : objs) {
    EXP_Value *prop = obj->GetProperty("fl_component");
    if (!prop) {
      continue;
    }
    ++pedidos;
    if (AttachComponent(obj, prop->GetText())) {
      ++atados;
    }
    else {
      /* Doctrina: sin Python nada falla en silencio. Si el .blend pide un
       * componente que el motor no conoce, se dice. */
      CM_Error("Flipendo: el objeto '" << obj->GetName() << "' pide el componente '"
                                       << prop->GetText() << "' y no existe");
    }
  }

  /* Sonda de verificacion (ver FL_RenderToTexture.hpp). */
  const int porEntorno = FL_RttAttachFromEnvironment(scene, *this);
  atados += porEntorno;
  pedidos += porEntorno;

  CM_Message("Flipendo: componentes nativos atados " << atados << "/" << pedidos << " en la escena '"
                                                     << scene->GetName() << "'");
}

bool FL_ComponentManager::AttachComponent(KX_GameObject *owner, const std::string &name)
{
  auto it = m_factories.find(name);
  if (it == m_factories.end()) {
    return false;
  }
  FL_Component *comp = it->second();
  comp->SetOwner(owner);
  comp->Start();
  m_instances.push_back({owner, comp});
  return true;
}

void FL_ComponentManager::Tick(KX_Scene *scene, float dt)
{
  AttachScene(scene);

  /* Sonda de verificacion de render a textura (no hace nada sin FL_RTT_PROBE). */
  FL_RttProbeTick(scene);

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
