/**
 *  For conditions of distribution and use, see copyright notice in license.txt
 *
 *  @file   SceneStructureModule.h
 *  @brief  Provides Scene Structure and Assets windows and raycast drag-and-drop import of
 *          various content file formats to the main window.
 */

#pragma once

#include "IModule.h"
#include "SceneFwd.h"
#include "InputFwd.h"
#include "AssetFwd.h"
#include "AssetReference.h"

#include <QPointer>
#include <QWidget>
#include <QLabel>
#include <QHash>

class QDragEnterEvent;
class QDragMoveEvent;
class QDropEvent;

class SceneStructureWindow;
class AssetsWindow;
struct SceneDesc;
class float3;

class EC_Mesh;

struct SceneMaterialDropData
{
    SceneMaterialDropData() : mesh(0) {}
    EC_Mesh *mesh;
    AssetReferenceList materials;
    QList<uint> affectedIndexes;
};

/// Provides Scene Structure and Assets windows and raycast drag-and-drop import of
/// various content file formats to the main window.
class SceneStructureModule : public IModule
{
    Q_OBJECT

public:
    SceneStructureModule();
    ~SceneStructureModule();
    void PostInitialize();

public slots:
    /// Instantiates new content from file to the scene.
    /** @param filename File name.
        @param worldPos Destination in-world position.
        @param clearScene Do we want to clear the scene before adding new content.
        @return List of created entities. */
    QList<Entity *> InstantiateContent(const QString &filename, const float3 &worldPos, bool clearScene);

    /// This is an overloaded function
    /** @param filenames List of scene files. */
    QList<Entity *> InstantiateContent(const QStringList &filenames, const float3 &worldPos, bool clearScene);

    /// Centralizes group of entities around same center point. The entities must have EC_Placeable component present.
    /** @param pos Center point for entities.
        @param entities List of entities. */
    static void CentralizeEntitiesTo(const float3 &pos, const QList<Entity *> &entities);

    /// Returns true of the file extension of @c fileRef is supported file type for importing.
    /** @param fileRef File name or url. */
    static bool IsSupportedFileType(const QString &fileRef);

    /// Returns true of the file extension of @c fileRef is supported material file for importing.
    /** @param fileRef File name or url. */
    static bool IsMaterialFile(const QString &fileRef);

    /// Returns true if the @c fileRef is a http:// or https:// scema url. 
    /** @param fileRef File name or url. */
    static bool IsUrl(const QString &fileRef);

    /// Cleans the @c fileRef
    /** @param fileRef File name or url. */
    static void CleanReference(QString &fileRef);

    /// Toggles visibility of Scene Structure window.
    void ToggleSceneStructureWindow();

    /// Toggles visibility of Assets window.
    void ToggleAssetsWindow();

private:
    QPointer<SceneStructureWindow> sceneWindow; ///< Scene Structure window.
    QPointer<AssetsWindow> assetsWindow;///< Assets window.
    boost::shared_ptr<InputContext> inputContext; ///< Input context.

    SceneMaterialDropData materialDropData;
    QHash<QString, float3> urlToDropPos;

    QWidget *toolTipWidget;
    QLabel *toolTip;
    QString currentToolTipSource;
    QString currentToolTipDestination;

private slots:
    /// Handles KeyPressed() signal from input context.
    void HandleKeyPressed(KeyEvent *e);

    /// Handles main window drag enter event.
    /** If event's MIME data contains URL which path has supported file extension we accept it. */
    void HandleDragEnterEvent(QDragEnterEvent *e);

    /// Handles main window drag leave event.
    void HandleDragLeaveEvent(QDragLeaveEvent *e);

    /// Handles main window drag move event.
    /** If event's MIME data contains URL which path has supported file extension we accept it. */
    void HandleDragMoveEvent(QDragMoveEvent *e);

    /// Handles drop event.
    /** If event's MIME data contains URL which path has supported file extension we accept it. */
    void HandleDropEvent(QDropEvent *e);

    /// Handles material drop event.
    /** If event's MIME data contains a single URL which is a material the drop is redirected to this function.
        @param e Event.
        @param materialRef Dropped material file or url. */
    void HandleMaterialDropEvent(QDropEvent *e, const QString &materialRef);

    /// Finishes a material drop
    /** @param apply If drop content should be appliead or not. */
    void FinishMaterialDrop(bool apply, const QString &materialBaseUrl);

    void HandleSceneDescLoaded(AssetPtr asset);

    void HandleSceneDescFailed(IAssetTransfer *transfer, QString reason);
};
