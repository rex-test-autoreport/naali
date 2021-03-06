/**
 *  For conditions of distribution and use, see copyright notice in license.txt
 *
 *  @file   SceneStructureWindow.h
 *  @brief  Window with tree view showing every entity in a scene.
 *
 *          This class will only handle adding and removing of entities and components and updating
 *          their names. The SceneTreeWidget implements most of the functionality.
 */

#pragma once

#include "SceneFwd.h"

#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QMap>

class QTreeWidgetItem;
class SceneTreeWidget;
class Framework;

/// Window with tree view showing every entity in a scene.
/** This class will only handle adding and removing of entities and components and updating
    their names. The SceneTreeWidget implements most of the functionality.
*/
class SceneStructureWindow : public QWidget
{
    Q_OBJECT

public:
    /// Constructs the window.
    /** @param fw Framework.
        @parent parent Parent widget.
    */
    explicit SceneStructureWindow(Framework *fw, QWidget *parent = 0);

    /// Destructor.
    ~SceneStructureWindow();

    /// Sets new scene to be shown in the tree view.
    /** Populates tree view with entities.
        If scene is set to 0, the tree view is cleared and signal connections are disconnected.
        @param s Scene.
    */
    void SetScene(const ScenePtr &s);

    /// Event filter to catch and react to child widget events
    virtual bool eventFilter(QObject *obj, QEvent *e);

public slots:
    /// Sets do we want to show components in the tree view.
    /** @param show Visibility of components in the tree view.
    */
    void ShowComponents(bool show);

    /// Sets do we want to show asset references in the tree view.
    /** @param show Visibility of asset references in the tree view.
    */
    void ShowAssetReferences(bool show);

protected:
    /// QWidget override.
    void changeEvent(QEvent* e);

private:
    /// Populates tree widget with all entities.
    void Populate();

    /// Clears tree widget.
    void Clear();

    /// Creates asset reference items.
    void CreateAssetReferences();

    /// Clears i.e. deletes all asset reference items.
    void ClearAssetReferences();

    /// Create asset reference item to the tree widget.
    /** @param parentItem Parent item, can be entity or component item.
        @param attr AssetReference attribute.
    */
    void CreateAssetItem(QTreeWidgetItem *parentItem, IAttribute *attr);

    /// Decorates an entity item: changes its color and appends the text with extra information.
    /** @param entity Entity Entity for which the item is created for.
        @param item Entity item to be decorated.
    */
    void DecorateEntityItem(Entity *entity, QTreeWidgetItem *item) const;

    /// Decorates a component item: changes its color and appends the text with extra information.
    /** @param comp comp Component for which the item is created for.
        @param item Component item to be decorated.
    */
    void DecorateComponentItem(IComponent *comp, QTreeWidgetItem *item) const;

    Framework *framework; ///< Framework.
    SceneWeakPtr scene; ///< Scene which we are showing the in tree widget currently.
    SceneTreeWidget *treeWidget; ///< Scene tree widget.
    bool showComponents; ///< Do we show components also in the tree view.
    bool showAssets; ///< Do we show asset references also in the tree view.
    QLineEdit *searchField; ///< Search field line edit.
    QPushButton *expandAndCollapseButton; ///< Expand/collapse all button.

private slots:
    /// Adds the entity to the tree widget.
    /** @param entity Entity to be added.
    */
    void AddEntity(Entity *entity);

    /// Removes entity from the tree widget.
    /** @param entity Entity to be removed.
    */
    void RemoveEntity(Entity *entity);

    /// Adds the entity to the tree widget.
    /** @param entity Altered entity.
        @param comp Component which was added.
    */
    void AddComponent(Entity *entity, IComponent *comp);

    /// Removes entity from the tree widget.
    /** @param entity Altered entity.
        @param comp Component which was removed.
    */
    void RemoveComponent(Entity *entity, IComponent *comp);

    /// This is an overloaded function.
    /** This is called only by EC_DynamicComponent when asset ref attribute is added to it.
        @param attr AssetReference attribute.
    */
    void AddAssetReference(IAttribute *attr);

    /// Removes asset reference from the tree widget.
    /** This is called only by EC_DynamicComponent when asset ref attribute is removed from it.
        @param attr AssetReference attribute.
    */
    void RemoveAssetReference(IAttribute *attr);

    /// When asset reference attribute changes, update UI accordingly.
    /** @param attr AssetReference attribute.
    */
    void UpdateAssetReference(IAttribute *attr);

    /// Updates entity's name in the tree widget if entity's EC_Name component's "name" attribute has changed.
    /** EC_Name component's AttributeChanged() signal is connected to this slot.
        @param attr EC_Name's attribute which was changed.
    */
    void UpdateEntityName(IAttribute *attr);

    /// Updates component's name in the tree widget if components name has changed.
    /** @param oldName Old component name.
        @param newName New component name.
    */
    void UpdateComponentName(const QString &oldName, const QString &newName);

    /// Sort items in the tree widget. The outstanding sort order is used.
    /** @param criteria Sorting criteria. Currently tr("ID") and tr("Name") are supported.
    */
    void Sort(const QString &criteria);

    /// Searches for items containing @c text (case-insensitive) and toggles their visibility.
    /** If match is found the item is set visible and expanded, otherwise it's hidden.
        @param filter Text used as a filter.
    */
    void Search(const QString &filter);

    /// Expands or collapses the whole tree view, depending on the previous action.
    void ExpandOrCollapseAll();

    /// Checks the expand status to mark it to the expand/collapse button
    void CheckTreeExpandStatus(QTreeWidgetItem *item);
};

