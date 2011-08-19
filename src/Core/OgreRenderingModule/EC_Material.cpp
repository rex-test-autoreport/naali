// For conditions of distribution and use, see copyright notice in license.txt

#include "StableHeaders.h"
#include "DebugOperatorNew.h"
#include "MemoryLeakCheck.h"
#include "OgreRenderingModule.h"
#include "OgreMaterialAsset.h"
#include "FrameAPI.h"
#include "Entity.h"
#include "EC_Material.h"
#include "EC_Mesh.h"
#include "LoggingFunctions.h"
#include "AssetAPI.h"
#include "AssetReference.h"

EC_Material::EC_Material(Scene* scene) :
    IComponent(scene),
    parameters(this, "Parameters", QVariantList()),
    inputMat(this, "Input Material", ""),
    outputMat(this, "Output Material", "")
{
    materialAsset = AssetRefListenerPtr(new AssetRefListener());
    connect(materialAsset.get(), SIGNAL(Loaded(AssetPtr)), this, SLOT(OnMaterialAssetLoaded(AssetPtr)), Qt::UniqueConnection);

    connect(this, SIGNAL(AttributeChanged(IAttribute*, AttributeChange::Type)),
            SLOT(OnAttributeUpdated(IAttribute*)));
    
    connect(this, SIGNAL(ParentEntitySet()), SLOT(OnParentEntitySet()));

}

EC_Material::~EC_Material()
{
}

void EC_Material::OnParentEntitySet()
{
    Entity* parent = ParentEntity();
    if (!parent)
        return;
    
    connect(parent, SIGNAL(ComponentAdded(IComponent*, AttributeChange::Type)), this, SLOT(OnComponentAdded(IComponent*, AttributeChange::Type)), Qt::UniqueConnection);
    
    // Check for mesh immediately
    EC_Mesh* mesh = parent->GetComponent<EC_Mesh>().get();
    if (mesh)
        connect(mesh, SIGNAL(AttributeChanged(IAttribute*, AttributeChange::Type)), this, SLOT(OnMeshAttributeUpdated(IAttribute*)), Qt::UniqueConnection);
}

void EC_Material::OnComponentAdded(IComponent* component, AttributeChange::Type change)
{
    EC_Mesh* mesh = dynamic_cast<EC_Mesh*>(component);
    if (mesh)
        connect(mesh, SIGNAL(AttributeChanged(IAttribute*, AttributeChange::Type)), this, SLOT(OnMeshAttributeUpdated(IAttribute*)), Qt::UniqueConnection);
}

void EC_Material::OnAttributeUpdated(IAttribute* attribute)
{
    if (attribute == &inputMat)
        CheckForInputMaterial();
    
    if ((attribute == &outputMat) || (attribute == &parameters))
    {
        // If output material or parameters change, and input asset exists, can apply parameters
        AssetPtr material = materialAsset->Asset();
        OgreMaterialAsset* srcMatAsset = dynamic_cast<OgreMaterialAsset*>(material.get());
        if (srcMatAsset && srcMatAsset->IsLoaded())
            ApplyParameters(srcMatAsset);
    }
}

void EC_Material::OnMeshAttributeUpdated(IAttribute* attribute)
{
    // If the mesh component has changed its material(s), and we are using its material, do a new input material request
    if (attribute->Name() == "Mesh materials") ///< todo Quite dangerous? What if the attribute name changes?
        if (GetSubmeshNumber() >= 0)
            CheckForInputMaterial();
}

int EC_Material::GetSubmeshNumber() const
{
    QString inputMatName = inputMat.Get();
    // If input material is empty, it is submesh 0
    if (inputMatName.isEmpty())
        return 0;
    bool ok = false;
    int subMesh = inputMatName.toInt(&ok);
    if (ok)
        return subMesh;
    else
        return -1; // Not using EC_Mesh material
}

QString EC_Material::GetInputMaterialName() const
{
    // Check whether using an own material reference, or EC_Mesh's materials
    int submesh = GetSubmeshNumber();
    if (submesh < 0)
        return inputMat.Get();
    
    Entity* parent = ParentEntity();
    if (!parent)
        return QString();
    EC_Mesh* mesh = dynamic_cast<EC_Mesh*>(parent->GetComponent<EC_Mesh>().get());
    if (!mesh)
        return QString();
    
    const AssetReferenceList& materialList = mesh->meshMaterial.Get();
    if (submesh >= materialList.Size())
    {
        LogWarning("EC_Material referring to a non-existent submesh material.");
        return QString();
    }
    else
        return materialList[submesh].ref;
}

void EC_Material::CheckForInputMaterial()
{
    QString inputMatName = GetInputMaterialName();
    if (inputMatName.isEmpty())
        return; // Empty material ref, and could not be interrogated from the EC_Mesh, so can't do anything
    materialAsset->HandleAssetRefChange(framework->Asset(), inputMatName, "OgreMaterial");
}

void EC_Material::OnMaterialAssetLoaded(AssetPtr material)
{
    OgreMaterialAsset* srcMatAsset = dynamic_cast<OgreMaterialAsset*>(material.get());
    // When input asset is loaded, can apply parameters
    if (srcMatAsset && srcMatAsset->IsLoaded())
        ApplyParameters(srcMatAsset);
}

void EC_Material::ApplyParameters(OgreMaterialAsset* srcMatAsset)
{
    AssetAPI* assetAPI = framework->Asset();
    QString outputMatName = outputMat.Get();
    
    // If output material name contains ?, replace it with entity ID
    int questionMark = outputMatName.indexOf('?');
    if (questionMark >= 0)
    {
        Entity* parentEntity = ParentEntity();
        if (parentEntity)
            outputMatName.replace(questionMark, 1, QString::number(parentEntity->Id()));
    }
    
    OgreMaterialAsset* destMatAsset = 0;
    if (!outputMatName.isEmpty())
    {
        // If dest. asset does not exist, create it now
        AssetPtr destAsset = assetAPI->GetAsset(assetAPI->ResolveAssetRef("", outputMatName));
        if (!destAsset)
            destAsset = assetAPI->CreateNewAsset("OgreMaterial", outputMatName);
        
        destMatAsset = dynamic_cast<OgreMaterialAsset*>(destAsset.get());
        // Copy source material to destination, however check that src & dest aren't same
        if ((destMatAsset) && (srcMatAsset != destMatAsset))
            destMatAsset->CopyContent(AssetPtr(srcMatAsset->shared_from_this()));
    }
    else
    {
        // If dest. material is empty, apply in-place
        destMatAsset = srcMatAsset;
    }
    
    if (!destMatAsset)
        return;
    
    // Apply parameters
    const QVariantList& params = parameters.Get();
    for (int i = 0; i < params.size(); ++i)
    {
        QString param = params[i].toString();
        QStringList parts = param.split('=');
        if (parts.size() == 2)
            destMatAsset->SetAttribute(parts[0], parts[1]);
    }

    // There is a race between EC_Material and EC_Mesh when a output material is generated.
    // Either EC_Material is first to generate the material into asset system, before EC_Mesh requests it,
    // or EC_Mesh requests before EC_Material has time to generate the material. In the latter case
    // the asset request will fail and the material breaks. To fix this we should inspect our
    // mesh and reapply generated materials.
    
    /// @note We could and possibly should look for all EC_Meshes in the scene, as another mesh than our
    /// own (in the same entity) could have the generated output material set!
    if (!outputMatName.isEmpty() && ParentEntity())
    {
        ComponentPtr meshComp = ParentEntity()->GetComponent("EC_Mesh");
        EC_Mesh *mesh = dynamic_cast<EC_Mesh*>(meshComp.get());
        if (mesh)
        {
            QString outputResolved = framework->Asset()->ResolveAssetRef("", outputMatName);
            AssetReferenceList materials = mesh->getmeshMaterial();
            for(int i=0; i<materials.Size(); i++)
            {
                QString materialResolved = framework->Asset()->ResolveAssetRef("", materials[i].ref);
                if (materialResolved == outputResolved)
                {
                    // Re-apply to make sure EC_Mesh finds the generated material via AssetAPI.
                    mesh->setmeshMaterial(materials);
                    break;
                }
            }
        }
    }
}
