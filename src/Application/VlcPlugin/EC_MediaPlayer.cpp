// For conditions of distribution and use, see copyright notice in license.txt

#include "DebugOperatorNew.h"
#include "EC_MediaPlayer.h"
#include "VlcMediaPlayer.h"

#include "Framework.h"
#include "SceneAPI.h"
#include "SceneInteract.h"
#include "Entity.h"
#include "AttributeMetadata.h"
#include "UiAPI.h"
#include "UiMainWindow.h"
#include "IRenderer.h"
#include "LoggingFunctions.h"

#include "EC_WidgetCanvas.h"
#include "EC_Mesh.h"

#include <QUuid>
#include <QString>

#include "MemoryLeakCheck.h"

EC_MediaPlayer::EC_MediaPlayer(Scene* scene) :
    IComponent(scene),
    mediaPlayer_(0),
    componentPrepared_(false),
    sourceRef(this, "Media Source", AssetReference("", "")),
    renderSubmeshIndex(this, "Render Submesh", 0),
    interactive(this, "Interactive", false),
    illuminating(this, "Illuminating", true)
{
    if (!ViewEnabled() || GetFramework()->IsHeadless())
        return;

    // Set metadata min/max/step
    static AttributeMetadata submeshMetaData;
    static bool metadataInitialized = false;
    if (!metadataInitialized)
    {
        submeshMetaData.minimum = "0";
        submeshMetaData.step = "1";
        metadataInitialized = true;
    }
    renderSubmeshIndex.SetMetadata(&submeshMetaData);

    // Init our internal media player
    mediaPlayer_ = new VlcMediaPlayer();
    connect(mediaPlayer_, SIGNAL(FrameUpdate(QImage)), SLOT(OnFrameUpdate(QImage)), Qt::UniqueConnection);

    // Connect signals from IComponent
    connect(this, SIGNAL(ParentEntitySet()), SLOT(PrepareComponent()), Qt::UniqueConnection);
    connect(this, SIGNAL(AttributeChanged(IAttribute*, AttributeChange::Type)), SLOT(AttributeChanged(IAttribute*, AttributeChange::Type)), Qt::UniqueConnection);

    // Prepare scene interactions
    SceneInteract *sceneInteract = GetFramework()->Scene()->GetSceneInteract();
    if (sceneInteract)
    {
        connect(sceneInteract, SIGNAL(EntityClicked(Entity*, Qt::MouseButton, RaycastResult*)), 
                SLOT(EntityClicked(Entity*, Qt::MouseButton, RaycastResult*)));
    }
}

EC_MediaPlayer::~EC_MediaPlayer()
{
    EC_WidgetCanvas *sceneCanvas = GetSceneCanvasComponent();
    if (sceneCanvas)
    {
        sceneCanvas->RestoreOriginalMeshMaterials();
        sceneCanvas->SetWidget(0);
    }

    if (mediaPlayer_)
    {
        mediaPlayer_->Stop();
        mediaPlayer_->disconnect();
        SAFE_DELETE(mediaPlayer_);
    }

    componentPrepared_ = false;
}

// Public slots

void EC_MediaPlayer::PlayPauseToggle()
{
    // Don't do anything if rendering is not enabled
    if (!ViewEnabled() || GetFramework()->IsHeadless())
        return;
    if (!componentPrepared_)
        return;
    if (!mediaPlayer_ || mediaPlayer_->Media().isEmpty())
        return;
    mediaPlayer_->PlayPause();
}

void EC_MediaPlayer::Stop()
{
    // Don't do anything if rendering is not enabled
    if (!ViewEnabled() || GetFramework()->IsHeadless())
        return;
    if (!componentPrepared_)
        return;
    if (!mediaPlayer_ || mediaPlayer_->Media().isEmpty())
        return;
    mediaPlayer_->Stop();
}

void EC_MediaPlayer::ShowPlayer(bool visible)
{
    if (mediaPlayer_)
        mediaPlayer_->setVisible(visible);
}

QMenu *EC_MediaPlayer::GetContextMenu()
{
    QMenu *actionMenu = new QMenu(0);
    actionMenu->setAttribute(Qt::WA_DeleteOnClose, true);
    actionMenu->addAction(QIcon(":/images/playpause.png"), "Play/Pause", this, SLOT(PlayPauseToggle()));
    actionMenu->addAction(QIcon(":/images/stop.png"), "Stop", this, SLOT(Stop()));
    actionMenu->addAction("Show Player", this, SLOT(ShowPlayer()));
    return actionMenu;
}

// Private slots

void EC_MediaPlayer::OnFrameUpdate(QImage frame)
{
    // Don't do anything if rendering is not enabled
    if (!ViewEnabled() || GetFramework()->IsHeadless())
        return;
    if (!componentPrepared_)
        return;

    // Get needed components, something is fatally wrong if these are not present but componentPrepared_ is true.
    EC_Mesh *mesh = GetMeshComponent();
    EC_WidgetCanvas *sceneCanvas = GetSceneCanvasComponent();
    if (!mesh || !sceneCanvas)
    {
        // In the case someone destroyed EC_WidgetCanvas or EC_Mesh from our entity
        // lets stop our running timer (if its running), so we don't unnecessarily poll here.
        componentPrepared_ = false;
        return;
    }

    // Validate submesh index from EC_Mesh
    uint submeshIndex = (uint)getrenderSubmeshIndex();
    if (submeshIndex >= mesh->GetNumSubMeshes())
    {
        /// \note ResetSubmeshIndex() is called with a small delay here, or the ec editor UI wont react to it. Resetting the index back to 0 will call Render() again.
        LogWarning("Render submesh index " + QString::number(submeshIndex).toStdString() + " is illegal, restoring default value.");
        QTimer::singleShot(1, this, SLOT(ResetSubmeshIndex()));
        return;
    }

    // Set submesh to EC_WidgetCanvas if different from current
    if (!sceneCanvas->GetSubMeshes().contains(submeshIndex))
        sceneCanvas->SetSubmesh(submeshIndex);

    sceneCanvas->Update(frame);
}

void EC_MediaPlayer::PrepareComponent()
{
    // Don't do anything if rendering is not enabled
    if (!ViewEnabled() || GetFramework()->IsHeadless())
        return;

    // Some security checks
    if (componentPrepared_)
    {
        LogWarning("EC_MediaPlayer: Preparations seem to be done already, you might not want to do this multiple times.");
    }
    if (!mediaPlayer_)
    {
        LogError("EC_MediaPlayer: Cannot start preparing, webview object is null. This should never happen!");
        return;
    }

    // Get parent and connect to the component removed signal.
    Entity *parent = ParentEntity();
    if (parent)
        connect(parent, SIGNAL(ComponentRemoved(IComponent*, AttributeChange::Type)), SLOT(ComponentRemoved(IComponent*, AttributeChange::Type)), Qt::UniqueConnection);
    else
    {
        LogError("EC_MediaPlayer: Could not get parent entity pointer!");
        return;
    }

    // Get EC_Mesh component
    EC_Mesh *mesh = GetMeshComponent();
    if (!mesh)
    {
        // Wait for EC_Mesh to be added.
        connect(parent, SIGNAL(ComponentAdded(IComponent*, AttributeChange::Type)), SLOT(ComponentAdded(IComponent*, AttributeChange::Type)), Qt::UniqueConnection);
        return;
    }
    else
    {
        // Inspect if this mesh is ready for rendering. EC_Mesh being present != being loaded into Ogre and ready for rendering.
        if (!mesh->GetEntity())
        {
            connect(mesh, SIGNAL(MeshChanged()), SLOT(TargetMeshReady()), Qt::UniqueConnection);
            return;
        }
        else
            connect(mesh, SIGNAL(MaterialChanged(uint, const QString&)), SLOT(TargetMeshMaterialChanged(uint, const QString&)), Qt::UniqueConnection);
    }

    if (sceneCanvasName_.isEmpty())
        sceneCanvasName_ = "VlcMediaPlayerCanvas-" + QUuid::createUuid().toString().replace("{", "").replace("}", "");

    // Get or create local EC_WidgetCanvas component
    ComponentPtr iComponent = parent->GetOrCreateComponent(EC_WidgetCanvas::TypeNameStatic(), sceneCanvasName_, AttributeChange::LocalOnly, false);
    EC_WidgetCanvas *sceneCanvas = dynamic_cast<EC_WidgetCanvas*>(iComponent.get());
    if (!sceneCanvas)
    {
        LogError("EC_MediaPlayer: Could not get or create EC_WidgetCanvas component!");
        return;
    }
    sceneCanvas->SetTemporary(true);
    sceneCanvas->SetSelfIllumination(getilluminating());

    // All the needed components are present, mark prepared as true.
    componentPrepared_ = true;

    // Update to get the start image.
    mediaPlayer_->ForceUpdateImage();
}

void EC_MediaPlayer::TargetMeshReady()
{
    if (!componentPrepared_)
        PrepareComponent();
}

void EC_MediaPlayer::TargetMeshMaterialChanged(uint index, const QString &material)
{
    if (!componentPrepared_)
        return;
    if (sceneCanvasName_.isEmpty())
        return;
    if (!ParentEntity())
        return;

    if (index == getrenderSubmeshIndex())
    {
        EC_WidgetCanvas *sceneCanvas = GetSceneCanvasComponent();
        if (sceneCanvas)
        {
            // This will make 3DCanvas to update its internals, which means
            // our material is re-applied to the submesh.
            sceneCanvas->SetSubmesh(getrenderSubmeshIndex());
            if (mediaPlayer_)
                mediaPlayer_->ForceUpdateImage();
        }
    }
}

void EC_MediaPlayer::ResetSubmeshIndex()
{
    setrenderSubmeshIndex(0);
}

void EC_MediaPlayer::ComponentAdded(IComponent *component, AttributeChange::Type change)
{
    if (component->TypeName() == EC_Mesh::TypeNameStatic())
    {
        if (!componentPrepared_)
            PrepareComponent();
    }
}

void EC_MediaPlayer::ComponentRemoved(IComponent *component, AttributeChange::Type change)
{
    // If this component is being removed we need to reset to the target meshes original materials and remove the comp.
    if (component == this)
    {
        // Reset EC_WidgetCanvas
        EC_WidgetCanvas *canvasSource = GetSceneCanvasComponent();
        if (canvasSource)
        {
            canvasSource->RestoreOriginalMeshMaterials();
            canvasSource->SetWidget(0);
        }

        // Clean up our EC_WidgetCanvas component from the entity
        if (ParentEntity() && !sceneCanvasName_.isEmpty())
            ParentEntity()->RemoveComponent(EC_WidgetCanvas::TypeNameStatic(), sceneCanvasName_, AttributeChange::LocalOnly);
        
        componentPrepared_ = false;
    }
    /// \todo Add check if this component has another EC_Mesh then init EC_WidgetCanvas again for that! Now we just blindly stop this EC.
    else if (component->TypeName() == EC_Mesh::TypeNameStatic())
    {
        componentPrepared_ = false;
    }
}

void EC_MediaPlayer::AttributeChanged(IAttribute *attribute, AttributeChange::Type changeType)
{
    if (attribute == &sourceRef)
    {
        if (!mediaPlayer_)
            return;

        // Load the 'getwebviewUrl' page to our QWebView if it's not empty.
        QString source = getsourceRef().ref.simplified();
        if (source.isEmpty())
        {
            mediaPlayer_->Stop();

            // Restore the original materials from the mesh if user sets url to empty string.
            EC_WidgetCanvas *sceneCanvas = GetSceneCanvasComponent();
            if (sceneCanvas)
                sceneCanvas->RestoreOriginalMeshMaterials();
            return;
        }

        // Only load source if source if not the same.
        if (!mediaPlayer_->LoadMedia(source))
            LogError("EC_MediaPlayer: Source not supported: " + source.toStdString());
    }
    else if (attribute == &illuminating)
    {
        EC_WidgetCanvas *canvas = GetSceneCanvasComponent();
        if (canvas)
            canvas->SetSelfIllumination(getilluminating());
    }
}

EC_Mesh *EC_MediaPlayer::GetMeshComponent()
{
    if (!ParentEntity())
        return 0;
    EC_Mesh *mesh = ParentEntity()->GetComponent<EC_Mesh>().get();
    return mesh;
}

EC_WidgetCanvas *EC_MediaPlayer::GetSceneCanvasComponent()
{
    if (!ParentEntity())
        return 0;
    if (sceneCanvasName_.isEmpty())
        return 0;
    IComponent *comp = ParentEntity()->GetComponent(EC_WidgetCanvas::TypeNameStatic(), sceneCanvasName_).get();
    EC_WidgetCanvas *sceneCanvas = dynamic_cast<EC_WidgetCanvas*>(comp);
    return sceneCanvas;
}

void EC_MediaPlayer::EntityClicked(Entity *entity, Qt::MouseButton button, RaycastResult *raycastResult)
{
    if (!getinteractive() || !ParentEntity())
        return;

    // We are only interested in left clicks on our entity.
    if (!raycastResult)
        return;
    if (button != Qt::LeftButton)
        return;

    if (entity == ParentEntity())
    {
        // We are only interested in clicks to our target submesh index.
        if (raycastResult->submesh != (unsigned)getrenderSubmeshIndex())
            return;

        // Entities have EC_Highlight if it is being manipulated.
        // At this situation we don't want to show any ui.
        if (entity->GetComponent("EC_Highlight"))
            return;

        QMenu *popupInteract = GetContextMenu();
        if (!popupInteract->actions().empty())
            popupInteract->exec(QCursor::pos());
    }
}