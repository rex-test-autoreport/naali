/**
 *  For conditions of distribution and use, see copyright notice in license.txt
 *
 *  @file   SceneStructureModule.cpp
 *  @brief  Provides Scene Structure and Assets windows and raycast drag-and-drop import of
 *          various content file formats to the main window.
 */

#include "StableHeaders.h"
#include "DebugOperatorNew.h"

#include "SceneStructureModule.h"
#include "SceneStructureWindow.h"
#include "AssetsWindow.h"
#include "SupportedFileTypes.h"
#include "AddContentWindow.h"

#include "SceneAPI.h"
#include "AssetAPI.h"
#include "IAsset.h"
#include "IAssetTransfer.h"
#include "Scene.h"
#include "Entity.h"
#include "ConsoleAPI.h"
#include "InputAPI.h"
#include "OgreRenderingModule.h"
#include "Renderer.h"
#include "SceneImporter.h"
#include "EC_Camera.h"
#include "EC_Placeable.h"
#include "EC_Mesh.h"
#include "UiAPI.h"
#include "UiGraphicsView.h"
#include "UiMainWindow.h"
#include "LoggingFunctions.h"
#include "SceneDesc.h"

#include <QToolTip>
#include <QCursor>

#include <OgreEntity.h>
#include <OgreMesh.h>

#ifdef ASSIMP_ENABLED
#include <OpenAssetImport.h>
#endif

//#include <OgreCamera.h>

#include "MemoryLeakCheck.h"

SceneStructureModule::SceneStructureModule() :
    IModule("SceneStructure"),
    sceneWindow(0),
    assetsWindow(0),
    toolTipWidget(0),
    toolTip(0)
{
}

SceneStructureModule::~SceneStructureModule()
{
    SAFE_DELETE(sceneWindow);
    SAFE_DELETE(toolTipWidget);
}

void SceneStructureModule::PostInitialize()
{
    framework_->Console()->RegisterCommand("scenestruct", "Shows the Scene Structure window, hides it if it's visible.", 
        this, SLOT(ToggleSceneStructureWindow()));
    framework_->Console()->RegisterCommand("assets", "Shows the Assets window, hides it if it's visible.", 
        this, SLOT(ToggleAssetsWindow()));

    // Don't allocate the widget memory for nothing if we are headless.
    if (!framework_->IsHeadless())
    {
        inputContext = framework_->Input()->RegisterInputContext("SceneStructureInput", 102);
        connect(inputContext.get(), SIGNAL(KeyPressed(KeyEvent *)), this, SLOT(HandleKeyPressed(KeyEvent *)));

        connect(framework_->Ui()->GraphicsView(), SIGNAL(DragEnterEvent(QDragEnterEvent *)), SLOT(HandleDragEnterEvent(QDragEnterEvent *)));
        connect(framework_->Ui()->GraphicsView(), SIGNAL(DragLeaveEvent(QDragLeaveEvent *)), SLOT(HandleDragLeaveEvent(QDragLeaveEvent *)));
        connect(framework_->Ui()->GraphicsView(), SIGNAL(DragMoveEvent(QDragMoveEvent *)), SLOT(HandleDragMoveEvent(QDragMoveEvent *)));
        connect(framework_->Ui()->GraphicsView(), SIGNAL(DropEvent(QDropEvent *)), SLOT(HandleDropEvent(QDropEvent *)));

        toolTipWidget = new QWidget(0, Qt::ToolTip);
        toolTipWidget->setLayout(new QHBoxLayout());
        toolTipWidget->layout()->setMargin(0);
        toolTipWidget->layout()->setSpacing(0);
        toolTipWidget->setContentsMargins(0,0,0,0);
        toolTipWidget->setStyleSheet(
            "QWidget { background-color: transparent; } QLabel { padding: 2px; border: 0.5px solid grey; border-radius: 0px; \
            background-color: qlineargradient(spread:pad, x1:0, y1:0, x2:0, y2:1, stop:0 rgba(246, 246, 246, 255), stop:1 rgba(237, 237, 237, 255)); }");

        toolTip = new QLabel(toolTipWidget);
        toolTip->setTextFormat(Qt::RichText);
        toolTipWidget->layout()->addWidget(toolTip);
    }
}

QList<Entity *> SceneStructureModule::InstantiateContent(const QString &filename, const float3 &worldPos, bool clearScene)
{
    return InstantiateContent(QStringList(QStringList() << filename), worldPos, clearScene);
}

QList<Entity *> SceneStructureModule::InstantiateContent(const QStringList &filenames, const float3 &worldPos, bool clearScene)
{
    QList<Entity *> ret;

    const ScenePtr &scene = GetFramework()->Scene()->GetDefaultScene();
    if (!scene)
    {
        LogError("Could not retrieve default world scene.");
        return ret;
    }

    QList<SceneDesc> sceneDescs;

    foreach(const QString &filename, filenames)
    {
        if (!IsSupportedFileType(filename))
        {
            LogError("Unsupported file extension: " + filename.toStdString());
            continue;
        }

        if (filename.endsWith(cOgreSceneFileExtension, Qt::CaseInsensitive))
        {
            ///\todo Implement ogre.scene url drops at some point?
            TundraLogic::SceneImporter importer(scene);
            sceneDescs.append(importer.CreateSceneDescFromScene(filename));
        }
        else if (filename.endsWith(cOgreMeshFileExtension, Qt::CaseInsensitive))
        {
            TundraLogic::SceneImporter importer(scene);
///\todo Perhaps download the mesh before instantiating so we could inspect the mesh binary for materials and skeleton? The path is already there for tundra scene file web drops
            //if (IsUrl(filename)) ...
            sceneDescs.append(importer.CreateSceneDescFromMesh(filename));
        }
        else if (filename.toLower().indexOf(cTundraXmlFileExtension) != -1 && filename.toLower().indexOf(cOgreMeshFileExtension) == -1)
        {
            if (IsUrl(filename))
            {
                AssetTransferPtr transfer = framework_->Asset()->RequestAsset(filename);
                if (transfer.get())
                {
                    urlToDropPos[filename] = worldPos;
                    connect(transfer.get(), SIGNAL(Succeeded(AssetPtr)), SLOT(HandleSceneDescLoaded(AssetPtr)));
                    connect(transfer.get(), SIGNAL(Failed(IAssetTransfer*, QString)), SLOT(HandleSceneDescFailed(IAssetTransfer*, QString)));
                    break; // Only allow one .txml drop at a time
                }
            }
            else
                sceneDescs.append(scene->CreateSceneDescFromXml(filename));
        }
        else if (filename.toLower().indexOf(cTundraBinFileExtension) != -1)
        {
            if (IsUrl(filename))
            {
                AssetTransferPtr transfer = framework_->Asset()->RequestAsset(filename);
                if (transfer.get())
                {
                    urlToDropPos[filename] = worldPos;
                    connect(transfer.get(), SIGNAL(Succeeded(AssetPtr)), SLOT(HandleSceneDescLoaded(AssetPtr)));
                    connect(transfer.get(), SIGNAL(Failed(IAssetTransfer*, QString)), SLOT(HandleSceneDescFailed(IAssetTransfer*, QString)));
                    break; // Only allow one .tbin drop at a time
                }
            }
            else
            {
                sceneDescs.append(scene->CreateSceneDescFromBinary(filename));
            }
        }
        else
        {
#ifdef ASSIMP_ENABLED
            boost::filesystem::path path(filename.toStdString());
            AssImp::OpenAssetImport assimporter;
            QString extension = QString(path.extension().c_str()).toLower();
            if (assimporter.IsSupportedExtension(extension))
            {
                std::string dirname = path.branch_path().string();
                std::vector<AssImp::MeshData> meshNames;
                assimporter.GetMeshData(filename, meshNames);

                TundraLogic::SceneImporter sceneimporter(scene);
                for(size_t i=0 ; i<meshNames.size() ; ++i)
                {
                    EntityPtr entity = sceneimporter.ImportMesh(meshNames[i].file_.toStdString(), dirname, meshNames[i].transform_,
                        std::string(), "local://", AttributeChange::Default, false, meshNames[i].name_.toStdString());
                    if (entity)
                        ret.append(entity.get());
                }

                return ret;
            }
#endif
        }
    }

    if (!sceneDescs.isEmpty())
    {
        AddContentWindow *addContent = new AddContentWindow(framework_, scene);
        addContent->AddDescription(sceneDescs[0]);
        if (worldPos != float3::zero)
            addContent->AddPosition(worldPos);
        addContent->show();
    }

    /** \todo this is always empty list of entities, remove (?!) as we actually don't know the entity count yet.
     *  it is known only after the add content window selections and processing has been done 
     */
    return ret;
}

void SceneStructureModule::CentralizeEntitiesTo(const float3 &pos, const QList<Entity *> &entities)
{
    float3 minPos(1e9f, 1e9f, 1e9f);
    float3 maxPos(-1e9f, -1e9f, -1e9f);

    QList<Entity*> filteredEntities;
    foreach(Entity *e, entities)
    {
        EC_Placeable *p = e->GetComponent<EC_Placeable>().get();
        // Ignore entities that doesn't have placable component or
        // they are a child of another placable.
        if (p && p->getparentRef().IsEmpty())
            filteredEntities.push_back(e);
    }

    foreach(Entity *e, filteredEntities)
    {
        EC_Placeable *p = e->GetComponent<EC_Placeable>().get();
        if (p)
        {
            float3 pos = p->transform.Get().pos;
            minPos.x = std::min(minPos.x, pos.x);
            minPos.y = std::min(minPos.y, pos.y);
            minPos.z = std::min(minPos.z, pos.z);
            maxPos.x = std::max(maxPos.x, pos.x);
            maxPos.y = std::max(maxPos.y, pos.y);
            maxPos.z = std::max(maxPos.z, pos.z);
        }
    }

    // We assume that world's up axis is Y-coordinate axis.
    float3 importPivotPos = float3((minPos.x + maxPos.x) / 2, minPos.y, (minPos.z + maxPos.z) / 2);
    float3 offset = pos - importPivotPos;

    foreach(Entity *e, filteredEntities)
    {
        EC_Placeable *p = e->GetComponent<EC_Placeable>().get();
        if (p)
        {
            Transform t = p->transform.Get();
            t.pos += offset;
            p->transform.Set(t, AttributeChange::Default);
        }
    }
}

bool SceneStructureModule::IsSupportedFileType(const QString &fileRef)
{
    if (fileRef.endsWith(cTundraXmlFileExtension, Qt::CaseInsensitive) ||
        fileRef.endsWith(cTundraBinFileExtension, Qt::CaseInsensitive) ||
        fileRef.endsWith(cOgreMeshFileExtension, Qt::CaseInsensitive) ||
        fileRef.endsWith(cOgreSceneFileExtension, Qt::CaseInsensitive))
    {
        return true;
    }
    else
    {
#ifdef ASSIMP_ENABLED
        boost::filesystem::path path(fileRef.toStdString());
        AssImp::OpenAssetImport assimporter;
        QString extension = QString(path.extension().c_str()).toLower();
        if (assimporter.IsSupportedExtension(extension))
            return true;
#endif
        return false;
    }
}

bool SceneStructureModule::IsMaterialFile(const QString &fileRef)
{
    if (fileRef.toLower().endsWith(".material"))
        return true;
    return false;
}

bool SceneStructureModule::IsUrl(const QString &fileRef)
{
    if (fileRef.startsWith("http://") || fileRef.startsWith("https://"))
        return true;
    return false;
}

void SceneStructureModule::CleanReference(QString &fileRef)
{
    if (!IsUrl(fileRef))
    {
        QUrl fileUrl(fileRef);
        fileRef = fileUrl.path();
#ifdef _WINDOWS
        // We have '/' as the first char on windows and the filename
        // is not identified as a file properly. But on other platforms the '/' is valid/required.
        fileRef = fileRef.mid(1);
#endif
    }
}

void SceneStructureModule::ToggleSceneStructureWindow()
{
    if (framework_->IsHeadless())
    {
        LogError("Cannot show scene structure window in headless mode.");
        return;
    }

    if (sceneWindow)
    {
        sceneWindow->setVisible(!sceneWindow->isVisible());
        if (!sceneWindow->isVisible())
            sceneWindow->close();
        return;
    }

    sceneWindow = new SceneStructureWindow(framework_, framework_->Ui()->MainWindow());
    sceneWindow->setWindowFlags(Qt::Tool);
    sceneWindow->SetScene(GetFramework()->Scene()->GetDefaultScene());
    sceneWindow->show();
}

void SceneStructureModule::ToggleAssetsWindow()
{
    if (framework_->IsHeadless())
    {
        LogError("Cannot show assets window in headless mode.");
        return;
    }

    if (assetsWindow)
    {
        assetsWindow->setVisible(!assetsWindow->isVisible());
        if (!assetsWindow->isVisible())
            assetsWindow->close();
        return;
    }

    assetsWindow = new AssetsWindow(framework_, framework_->Ui()->MainWindow());
    assetsWindow->setWindowFlags(Qt::Tool);
    assetsWindow->show();
}

void SceneStructureModule::HandleKeyPressed(KeyEvent *e)
{
    if (e->eventType != KeyEvent::KeyPressed || e->keyPressCount > 1)
        return;

    InputAPI &input = *framework_->Input();

    const QKeySequence &showSceneStruct = input.KeyBinding("ShowSceneStructureWindow", QKeySequence(Qt::ShiftModifier + Qt::Key_S));
    const QKeySequence &showAssets = input.KeyBinding("ShowAssetsWindow", QKeySequence(Qt::ShiftModifier + Qt::Key_A));
    if (e->Sequence()== showSceneStruct)
    {
        ToggleSceneStructureWindow();
        e->handled = true;
    }
    if (e->Sequence() == showAssets)
    {
        ToggleAssetsWindow();
        e->handled = true;
    }
}

void SceneStructureModule::HandleDragEnterEvent(QDragEnterEvent *e)
{
    // If at least one file is supported, accept.
    bool accept = false;

    int acceptedCount = 0;
    QString dropResourceNames;
    currentToolTipSource.clear();
    if (e->mimeData()->hasUrls())
    {   
        foreach(QUrl url, e->mimeData()->urls())
        {
            if (IsSupportedFileType(url.path()))
            {
                dropResourceNames.append(url.toString().split("/").last() + ", ");
                acceptedCount++;
                accept = true;
            }
            // Accept .material only if a single material is being dropped
            else if (IsMaterialFile(url.path()))
            {
                if (e->mimeData()->urls().count() == 1)
                {
                    dropResourceNames.append(url.toString().split("/").last());
                    acceptedCount++;
                    accept = true;
                }
            }
        }
    }
    
    if (accept)
    {
        if (dropResourceNames.endsWith(", "))
            dropResourceNames.chop(2);
        if (dropResourceNames.count(",") >= 2 && acceptedCount > 2)
        {
            int from = dropResourceNames.indexOf(",");
            dropResourceNames = dropResourceNames.left(dropResourceNames.indexOf(",", from+1));
            dropResourceNames.append(QString("... (%1 assets)").arg(acceptedCount));
        }
        if (!dropResourceNames.isEmpty())
        {
            if (dropResourceNames.count(",") > 0)
                currentToolTipSource = "<p style='white-space:pre'><span style='font-weight:bold;'>Sources:</span> " + dropResourceNames;
            else
                currentToolTipSource = "<p style='white-space:pre'><span style='font-weight:bold;'>Source:</span> " + dropResourceNames;
        }
    }

    e->setAccepted(accept);
}

void SceneStructureModule::HandleDragLeaveEvent(QDragLeaveEvent *e)
{
    if (!toolTipWidget)
        return;
    toolTipWidget->hide();
    currentToolTipSource.clear();
    currentToolTipDestination.clear();
}

void SceneStructureModule::HandleDragMoveEvent(QDragMoveEvent *e)
{
    if (!e->mimeData()->hasUrls())
    {
        e->ignore();
        return;
    }

    currentToolTipDestination.clear();
    foreach(const QUrl &url, e->mimeData()->urls())
    {
        if (IsSupportedFileType(url.path()))
            e->accept();
        else if (IsMaterialFile(url.path()))
        {
            e->setAccepted(false);
            currentToolTipDestination = "<br><span style='font-weight:bold;'>Destination:</span> ";
            // Raycast to see if there is a submesh under the material drop
            OgreRenderer::RendererPtr renderer = framework_->GetModule<OgreRenderer::OgreRenderingModule>()->GetRenderer();
            if (renderer)
            {
                RaycastResult* res = renderer->Raycast(e->pos().x(), e->pos().y());
                if (res->entity)
                {
                    EC_Mesh *mesh = res->entity->GetComponent<EC_Mesh>().get();
                    if (mesh)
                    {
                        currentToolTipDestination.append("Submesh " + QString::number(res->submesh));
                        if (!mesh->Name().isEmpty())
                            currentToolTipDestination.append(" on " + mesh->Name());
                        else if (!mesh->ParentEntity()->Name().isEmpty())
                            currentToolTipDestination.append(" on " + mesh->ParentEntity()->Name());
                        currentToolTipDestination.append("</p>");
                        e->accept();
                    }
                }
            }
            if (!e->isAccepted())
            {
                currentToolTipDestination.append("None</p>");
                e->ignore();
            }
        }
    }

    if (e->isAccepted() && currentToolTipDestination.isEmpty())
    {
        OgreRenderer::RendererPtr renderer = framework_->GetModule<OgreRenderer::OgreRenderingModule>()->GetRenderer();
        if (renderer)
        {
            RaycastResult* res = renderer->Raycast(e->pos().x(), e->pos().y());
            if (res)
            {
                if (res->entity)
                {
                    QString entityName = res->entity->Name();
                    currentToolTipDestination = "<br><span style='font-weight:bold;'>Destination:</span> ";
                    if (!entityName.isEmpty())
                        currentToolTipDestination.append(entityName + " ");
                    QString xStr = QString::number(res->pos.x);
                    xStr = xStr.left(xStr.indexOf(".")+3);
                    QString yStr = QString::number(res->pos.y);
                    yStr = yStr.left(yStr.indexOf(".")+3);
                    QString zStr = QString::number(res->pos.z);
                    zStr = zStr.left(zStr.indexOf(".")+3);
                    currentToolTipDestination.append(QString("(%2 %3 %4)</p>").arg(xStr, yStr, zStr));
                }
                else
                    currentToolTipDestination = "<br><span style='font-weight:bold;'>Destination:</span> Dropping in front of camera</p>";
            }
        }
    }
    
    if (toolTipWidget && !currentToolTipSource.isEmpty())
    {
        if (currentToolTipDestination.isEmpty())
            currentToolTipDestination = "</p>";
        if (toolTip->text() != currentToolTipSource + currentToolTipDestination)
        {
            toolTip->setText(currentToolTipSource + currentToolTipDestination);
            toolTipWidget->resize(1,1);
        }
        toolTipWidget->move(QPoint(QCursor::pos().x()+25, QCursor::pos().y()+25));

        if (!toolTipWidget->isVisible())
            toolTipWidget->show();
    }
}

void SceneStructureModule::HandleDropEvent(QDropEvent *e)
{
    if (toolTipWidget)
        toolTipWidget->hide();

    if (e->mimeData()->hasUrls())
    {
        // Handle materials with own handler
        if (e->mimeData()->urls().count() == 1)
        {
            QString fileRef = e->mimeData()->urls().first().toString();
            if (IsMaterialFile(fileRef))
            {
                CleanReference(fileRef);
                HandleMaterialDropEvent(e, fileRef);
                return;
            }
        }

        // Handle other supported file types
        QList<Entity *> importedEntities;

        OgreRenderer::RendererPtr renderer = framework_->GetModule<OgreRenderer::OgreRenderingModule>()->GetRenderer();
        if (!renderer)
            return;

        float3 worldPos;
        RaycastResult* res = renderer->Raycast(e->pos().x(), e->pos().y());
        if (!res->entity)
        {
            // No entity hit, use camera's position with hard-coded offset.
            const ScenePtr &scene = GetFramework()->Scene()->GetDefaultScene();
            if (!scene)
                return;

            foreach(const EntityPtr &cam, scene->GetEntitiesWithComponent(EC_Camera::TypeNameStatic()))
                if (cam->GetComponent<EC_Camera>()->IsActive())
                {
                    EC_Placeable *placeable = cam->GetComponent<EC_Placeable>().get();
                    if (placeable)
                    {
                        //Ogre::Ray ray = cam->GetComponent<EC_Camera>()->GetCamera()->getCameraToViewportRay(e->pos().x(), e->pos().y());
                        Quat q = placeable->WorldOrientation();
                        float3 v = q * scene->ForwardVector();
                        //Ogre::Vector3 oV = ray.getPoint(20);
                        worldPos = /*float3(oV.x, oV.y, oV.z);*/ placeable->Position() + v * 20;
                        break;
                    }
                }
        }
        else
            worldPos = res->pos;

        foreach (const QUrl &url, e->mimeData()->urls())
        {
            QString fileRef = url.toString();
            CleanReference(fileRef);
            importedEntities.append(InstantiateContent(fileRef, worldPos/*float3()*/, false));
        }

        // Calculate import pivot and offset for new content
        //if (importedEntities.size())
        //    CentralizeEntitiesTo(worldPos, importedEntities);

        e->acceptProposedAction();
    }
}

void SceneStructureModule::HandleMaterialDropEvent(QDropEvent *e, const QString &materialRef)
{   
    // Raycast to see if there is a submesh under the material drop
    OgreRenderer::RendererPtr renderer = framework_->GetModule<OgreRenderer::OgreRenderingModule>()->GetRenderer();
    if (renderer)
    {
        RaycastResult* res = renderer->Raycast(e->pos().x(), e->pos().y());
        if (res->entity)
        {
            EC_Mesh *mesh = res->entity->GetComponent<EC_Mesh>().get();
            if (mesh)
            {
                uint subMeshCount = mesh->GetNumSubMeshes();
                uint subMeshIndex = res->submesh;
                if (subMeshIndex < subMeshCount)
                {
                    materialDropData.affectedIndexes.clear();

                    // Get the filename
                    QString matName = materialRef;
                    matName.replace("\\", "/");
                    matName = matName.split("/").last();

                    QString cleanMaterialRef = matName;

                    // Add our dropped material to the raycasted submesh,
                    // append empty string or the current material string to the rest of them
                    AssetReferenceList currentMaterials = mesh->getmeshMaterial();
                    AssetReferenceList afterMaterials;
                    for(uint i=0; i<subMeshCount; ++i)
                    {
                        if (i == subMeshIndex)
                        {
                            afterMaterials.Append(AssetReference(cleanMaterialRef));
                            materialDropData.affectedIndexes.append(i);
                        }
                        else if (i < (uint)currentMaterials.Size())
                            afterMaterials.Append(currentMaterials[i]);
                        else
                            afterMaterials.Append(AssetReference());
                    }
                    // Clear our any empty ones from the end
                    for(uint i=afterMaterials.Size(); i>0; --i)
                    {
                        AssetReference assetRef = afterMaterials[i-1];
                        if (assetRef.ref.isEmpty())
                            afterMaterials.RemoveLast();
                        else
                            break;
                    }

                    // Url: Finish now
                    // File: Finish when add content dialog gives Completed signal
                    materialDropData.mesh = mesh;
                    materialDropData.materials = afterMaterials;

                    if (IsUrl(materialRef))
                    {
                        QString baseUrl = materialRef.left(materialRef.length() - cleanMaterialRef.length());
                        FinishMaterialDrop(true, baseUrl);
                    }
                    else
                    {
                        const ScenePtr &scene = GetFramework()->Scene()->GetDefaultScene();
                        if (!scene)
                        {
                            LogError("Could not retrieve default world scene.");
                            return;
                        }

                        // Open source file for reading
                        QFile materialFile(materialRef);
                        if (!materialFile.open(QIODevice::ReadOnly))
                        {
                            LogError("Could not open dropped material file.");
                            return;
                        }

                        // Create scene description
                        SceneDesc sceneDesc;
                        sceneDesc.filename = materialRef;

                        // Add our material asset to scene description
                        AssetDesc ad;
                        ad.typeName = "material";
                        ad.source = materialRef;
                        ad.destinationName = matName;
                        ad.data = materialFile.readAll();
                        ad.dataInMemory = true;

                        sceneDesc.assets[qMakePair(ad.source, ad.subname)] = ad;
                        materialFile.close();

                        // Add texture assets to scene description
                        TundraLogic::SceneImporter importer(scene);
                        QSet<QString> textures = importer.ProcessMaterialForTextures(ad.data);
                        if (!textures.empty())
                        {
                            QString dropFolder = materialRef;
                            dropFolder = dropFolder.replace("\\", "/");
                            dropFolder = dropFolder.left(dropFolder.lastIndexOf("/")+1);

                            foreach(const QString &textureName, textures)
                            {
                                AssetDesc ad;
                                ad.typeName = "texture";
                                ad.source = dropFolder + textureName;
                                ad.destinationName = textureName;
                                ad.dataInMemory = false;
                                sceneDesc.assets[qMakePair(ad.source, ad.subname)] = ad;
                            }
                        }

                        // Show add content window
                        AddContentWindow *addMaterials = new AddContentWindow(framework_, scene);
                        connect(addMaterials, SIGNAL(Completed(bool, const QString&)), SLOT(FinishMaterialDrop(bool, const QString&)));
                        addMaterials->AddDescription(sceneDesc);
                        addMaterials->show();
                    }
                    e->acceptProposedAction();
                }
            }
        }
    }
}

void SceneStructureModule::FinishMaterialDrop(bool apply, const QString &materialBaseUrl)
{
    if (apply)
    {
        EC_Mesh *mesh = materialDropData.mesh;
        if (mesh)
        {
            // Inspect the base url where the assets were uploaded
            // rewrite the affeced materials to have the base url in front
            if (!materialDropData.affectedIndexes.empty())
            {
                AssetReferenceList rewrittenMats;
                AssetReferenceList mats = materialDropData.materials;
                for(uint i=0; i<(uint)mats.Size(); ++i)
                {
                    if (materialDropData.affectedIndexes.contains(i))
                    {
                        QString newRef = materialBaseUrl;
                        if (!newRef.endsWith("/")) // just to be sure
                            newRef.append("/");
                        newRef.append(mats[i].ref);
                        rewrittenMats.Append(AssetReference(newRef));
                    }
                    else
                        rewrittenMats.Append(mats[i]);

                }
                mesh->setmeshMaterial(rewrittenMats);
            }
        }
    }
    materialDropData.mesh = 0;
    materialDropData.materials = AssetReferenceList();
    materialDropData.affectedIndexes.clear();
}

void SceneStructureModule::HandleSceneDescLoaded(AssetPtr asset)
{
    QApplication::restoreOverrideCursor();

    const ScenePtr &scene = GetFramework()->Scene()->GetDefaultScene();
    if (!scene)
    {
        LogError("Could not retrieve default world scene.");
        return;
    }

    // Resolve the adjust raycast pos of this drop
    float3 adjustPos = float3::zero;
    if (urlToDropPos.contains(asset->Name()))
    {
        adjustPos = urlToDropPos[asset->Name()];
        urlToDropPos.remove(asset->Name());
    }

    // Get xml data
    std::vector<u8> data;
    asset->SerializeTo(data);
    if (data.empty())
    {
        LogError("Failed to serialize txml.");
        return;
    }

    QByteArray data_qt((const char *)&data[0], data.size());
    if (data_qt.isEmpty())
    {
        LogError("Failed to convert txml data to QByteArray.");
        return;
    }

    // Init description
    SceneDesc sceneDesc;
    sceneDesc.filename = asset->Name();

    // Get data
    if (sceneDesc.filename.toLower().endsWith(cTundraXmlFileExtension))
        sceneDesc = scene->CreateSceneDescFromXml(data_qt, sceneDesc); 
    else if(sceneDesc.filename.toLower().endsWith(cTundraBinFileExtension))
        sceneDesc = scene->CreateSceneDescFromBinary(data_qt, sceneDesc);
    else
    {
        LogError("Somehow other that " + cTundraXmlFileExtension.toStdString() + " or " + cTundraBinFileExtension.toStdString() + 
            " file got drag and dropped to the scene? Cannot proceed with add content dialog.");
        return;
    }

    // Show add content window
    AddContentWindow *addContent = new AddContentWindow(framework_, scene);
    addContent->AddDescription(sceneDesc);
    addContent->AddPosition(adjustPos);
    addContent->show();
}

void SceneStructureModule::HandleSceneDescFailed(IAssetTransfer *transfer, QString reason)
{
    QApplication::restoreOverrideCursor();
    QString error = QString("Failed to download %1 with reason %2").arg(transfer->source.ref, reason);
    LogError(error.toStdString());

    if (urlToDropPos.contains(transfer->GetSourceUrl()))
        urlToDropPos.remove(transfer->GetSourceUrl());
}
