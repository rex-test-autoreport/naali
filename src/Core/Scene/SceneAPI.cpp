// For conditions of distribution and use, see copyright notice in license.txt

#include "StableHeaders.h"
#include "DebugOperatorNew.h"

#include "SceneAPI.h"
#include "Scene.h"
#include "IComponentFactory.h"
#include "IComponent.h"
#include "AssetReference.h"
#include "EntityReference.h"
#include "SceneInteract.h"

#include "Color.h"
#include "Math/Quat.h"
#include "Math/float2.h"
#include "Math/float3.h"
#include "Math/float4.h"
#include "Transform.h"
#include "MemoryLeakCheck.h"

SceneAPI::SceneAPI(Framework *framework) :
    QObject(framework),
    framework_(framework)
{
    sceneInteract = new SceneInteract();
    framework->RegisterDynamicObject("sceneinteract", sceneInteract);

    defaultScene_.reset();
    scenes_.clear();
}

SceneAPI::~SceneAPI()
{
    Reset();
}

void SceneAPI::Reset()
{
    SAFE_DELETE(sceneInteract);
    defaultScene_.reset();
    scenes_.clear();
    componentFactories.clear();
    componentFactoriesByTypeid.clear();
}

void SceneAPI::Initialise()
{
    sceneInteract->Initialize(framework_);
}

SceneInteract *SceneAPI::GetSceneInteract() const
{
    return sceneInteract;
}

bool SceneAPI::HasScene(const QString &name) const
{
    return scenes_.find(name) != scenes_.end();
}

void SceneAPI::SetDefaultScene(const QString &name)
{
    ScenePtr scene = GetScene(name);
    if(scene != defaultScene_)
    {
        defaultScene_ = scene;
        emit DefaultWorldSceneChanged(defaultScene_.get());
    }
}

void SceneAPI::SetDefaultScene(const ScenePtr &scene)
{
    if (scene != defaultScene_)
    {
        defaultScene_ = scene;
        emit DefaultWorldSceneChanged(defaultScene_.get());
    }
}

const ScenePtr &SceneAPI::GetDefaultScene() const
{
    return defaultScene_;
}

Scene* SceneAPI::GetDefaultSceneRaw() const
{
    return defaultScene_.get();
}

ScenePtr SceneAPI::GetScene(const QString &name) const
{
    SceneMap::const_iterator scene = scenes_.find(name);
    if (scene != scenes_.end())
        return scene->second;
    return ScenePtr();
}

ScenePtr SceneAPI::CreateScene(const QString &name, bool viewenabled, bool authority)
{
    if (HasScene(name))
        return ScenePtr();

    ScenePtr newScene = ScenePtr(new Scene(name, framework_, viewenabled, authority));
    if (newScene.get())
    {
        scenes_[name] = newScene;

        // Emit signal of creation
        emit SceneAdded(newScene->Name());
    }
    return newScene;
}

void SceneAPI::RemoveScene(const QString &name)
{
    SceneMap::iterator sceneIter = scenes_.find(name);
    if (sceneIter != scenes_.end())
    {
        // Remove entities before the scene subsystems or worlds are erased by various modules
        sceneIter->second->RemoveAllEntities(false);
        
        // Emit signal about removed scene
        emit SceneRemoved(name);
        
        // If default scene is being removed. Reset our ref so it does not keep ref count alive.
        if (defaultScene_ == sceneIter->second)
            defaultScene_.reset();
        scenes_.erase(sceneIter);
    }
}

const SceneMap &SceneAPI::Scenes() const
{
    return scenes_;
}

bool SceneAPI::IsComponentFactoryRegistered(const QString &typeName)
{
    ComponentFactoryMap::iterator existing = componentFactories.find(typeName);
    return (existing != componentFactories.end() ? true : false);
}

void SceneAPI::RegisterComponentFactory(ComponentFactoryPtr factory)
{
    if (factory->TypeName().trimmed() != factory->TypeName() || factory->TypeName().isEmpty() || factory->TypeId() == 0)
    {
        LogError("Cannot add a new ComponentFactory for component typename \"" + factory->TypeName() + "\" and typeid " + QString::number(factory->TypeId()) + ". Invalid input!");
        return;
    }

    ComponentFactoryMap::iterator existing = componentFactories.find(factory->TypeName());
    ComponentFactoryWeakMap::iterator existing2 = componentFactoriesByTypeid.find(factory->TypeId());
    if (existing != componentFactories.end() || (existing2 != componentFactoriesByTypeid.end() && existing2->second.lock()))
    {
        ComponentFactoryPtr existingFactory = existing->second ? existing->second : existing2->second.lock();
        if (existingFactory)
            LogError("Cannot add a new ComponentFactory for component typename \"" + factory->TypeName() + "\" and typeid " + QString::number(factory->TypeId()) + ". Conflicting type factory with typename " + existingFactory->TypeName() + " and typeid " + QString::number(existingFactory->TypeId()) + " already exists!");
        else
            LogError("Cannot add a new ComponentFactory for component typename \"" + factory->TypeName() + "\" and typeid " + QString::number(factory->TypeId()) + ". Conflicting type factory exists!");
        return;
    }

    componentFactories[factory->TypeName()] = factory;
    componentFactoriesByTypeid[factory->TypeId()] = factory;
}

ComponentPtr SceneAPI::CreateComponentByName(Scene* scene, const QString &componentTypename, const QString &newComponentName)
{
    ComponentFactoryPtr factory = GetFactory(componentTypename);
    if (!factory)
    {
        LogError("Cannot create component for type \"" + componentTypename + "\" - no factory exists!");
        return ComponentPtr();
    }
    return factory->Create(scene, newComponentName);
}

ComponentPtr SceneAPI::CreateComponentById(Scene* scene, u32 componentTypeid, const QString &newComponentName)
{
    ComponentFactoryPtr factory = GetFactory(componentTypeid);
    if (!factory)
    {
        LogError("Cannot create component for typeid \"" + QString::number(componentTypeid) + "\" - no factory exists!");
        return ComponentPtr();
    }
    return factory->Create(scene, newComponentName);
}

QString SceneAPI::GetComponentTypeName(u32 componentTypeid)
{
    ComponentFactoryPtr factory = GetFactory(componentTypeid);
    if (factory)
        return factory->TypeName();
    else
        return "";
}

u32 SceneAPI::GetComponentTypeId(const QString &componentTypename)
{
    ComponentFactoryPtr factory = GetFactory(componentTypename);
    if (factory)
        return factory->TypeId();
    else
        return 0;
}

/*
ComponentPtr SceneAPI::CloneComponent(const ComponentPtr &component, const QString &newComponentName)
{
    ComponentFactoryPtr factory = GetFactory(component->TypeId());
    if (!factory)
        return ComponentPtr();

    return factory->Clone(component.get(), newComponentName);
}
*/

IAttribute *SceneAPI::CreateAttribute(IComponent *owner, const QString &attributeTypename, const QString &newAttributeName)
{
    // The dynamically created attributes are deleted at the EC_DynamicComponent dtor.
    IAttribute *attribute = 0;
    if (attributeTypename == "string")
        attribute = new Attribute<QString>(owner, newAttributeName.toStdString().c_str());
    else if (attributeTypename == "int")
        attribute = new Attribute<int>(owner, newAttributeName.toStdString().c_str());
    else if (attributeTypename == "real")
        attribute = new Attribute<float>(owner, newAttributeName.toStdString().c_str());
    else if (attributeTypename == "color")
        attribute = new Attribute<Color>(owner, newAttributeName.toStdString().c_str());
    else if (attributeTypename == "float2")
        attribute = new Attribute<float2>(owner, newAttributeName.toStdString().c_str());
    else if (attributeTypename == "float3")
        attribute = new Attribute<float3>(owner, newAttributeName.toStdString().c_str());
    else if (attributeTypename == "float4")
        attribute = new Attribute<float4>(owner, newAttributeName.toStdString().c_str());
    else if (attributeTypename == "bool")
        attribute = new Attribute<bool>(owner, newAttributeName.toStdString().c_str());
    else if (attributeTypename == "uint")
        attribute = new Attribute<uint>(owner, newAttributeName.toStdString().c_str());
    else if (attributeTypename == "quat")
        attribute = new Attribute<Quat>(owner, newAttributeName.toStdString().c_str());
    else if (attributeTypename == "assetreference")
        attribute = new Attribute<AssetReference>(owner, newAttributeName.toStdString().c_str());
    else if (attributeTypename == "assetreferencelist")
        attribute = new Attribute<AssetReferenceList>(owner, newAttributeName.toStdString().c_str());
    else if (attributeTypename == "entityreference")
        attribute = new Attribute<EntityReference>(owner, newAttributeName.toStdString().c_str());
    else if (attributeTypename == "qvariant")
        attribute = new Attribute<QVariant>(owner, newAttributeName.toStdString().c_str());
    else if (attributeTypename == "qvariantlist")
        attribute = new Attribute<QVariantList >(owner, newAttributeName.toStdString().c_str());
    else if (attributeTypename == "transform")
        attribute = new Attribute<Transform>(owner, newAttributeName.toStdString().c_str());
    else
        LogError("Cannot create attribute of type \"" + attributeTypename + "\"! This type is not known to SceneAPI::CreateAttribute!");
    return attribute;
}

QStringList SceneAPI::AttributeTypes() const
{
    QStringList attrTypes;
    attrTypes << "string" << "int" << "real" << "color" << "float2" << "float3" << "float4" << "bool" << "uint" << "quat" <<
        "assetreference" << "assetreferencelist" << "entityreference" << "qvariant" << "qvariantlist" << "transform";
    return attrTypes;
}

QStringList SceneAPI::ComponentTypes() const
{
    QStringList componentTypes;
    for(ComponentFactoryMap::const_iterator iter = componentFactories.begin(); iter != componentFactories.end(); ++iter)
        componentTypes << iter->first;
    return componentTypes;
}

ComponentFactoryPtr SceneAPI::GetFactory(const QString &typeName)
{
    ComponentFactoryMap::iterator factory = componentFactories.find(typeName);
    if (factory == componentFactories.end())
        return ComponentFactoryPtr();
    else
        return factory->second;
}

ComponentFactoryPtr SceneAPI::GetFactory(u32 typeId)
{
    ComponentFactoryWeakMap::iterator factory = componentFactoriesByTypeid.find(typeId);
    if (factory == componentFactoriesByTypeid.end())
        return ComponentFactoryPtr();
    else
        return factory->second.lock();
}
