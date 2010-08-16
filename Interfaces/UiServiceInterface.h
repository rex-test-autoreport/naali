/**
 *  For conditions of distribution and use, see copyright notice in license.txt
 *
 *  @file   UiServiceInterface.h
 *  @brief  Interface for Naali's user interface ulitizing Qt's QWidgets.
 *          If you want to see your QWidgets external to the main application just call show() for them.
 */

#ifndef incl_Interfaces_UiServiceInterface_h
#define incl_Interfaces_UiServiceInterface_h

#include "ServiceInterface.h"
#include "UiWidgetProperties.h"

#include <QObject>

class QWidget;
class QGraphicsProxyWidget;
class QGraphicsScene;

class UiProxyWidget;

namespace Foundation
{
    class UiServiceInterface;
    typedef boost::shared_ptr<UiServiceInterface> UiServicePtr;
    typedef boost::weak_ptr<UiServiceInterface> UiServiceWeakPtr;

    /** Interface for Naali's user interface ulitizing Qt's QWidgets.
     *  If you want to see your QWidgets external to the main application just call show() for them.
     */
    class UiServiceInterface : public QObject, public Foundation::ServiceInterface
    {
        Q_OBJECT

    public:
        /// Default destructor.
        UiServiceInterface() {}

        /// Destructor.
        virtual ~UiServiceInterface() {}

    public slots:
        /** Adds widget to scene.
         *  @param  widget Widget.
         *  @param  flags Window flags. Qt::Dialog is used as default.
         *          It creates movable proxy widget which has title bar and frames.
         *          If you want add widget without title bar and frames, use Qt::Widget.
         *          For further information, see @see http://doc.qt.nokia.com/4.6/qt.html#WindowType-enum
         *  @return Proxy widget of the added widget.
         */
        virtual UiProxyWidget *AddWidgetToScene(QWidget *widget, Qt::WindowFlags flags = Qt::Dialog) = 0;

        /** Adds user-created UiProxyWidget to the scene.
         *  @param widget Proxy widget.
         */
        virtual void AddWidgetToScene(UiProxyWidget *widget) = 0;

        /** Adds widget to menu.
         *  @param widget Widget.
         *  @param properties Widget properties.
         *  @note Doesn't add the widget to the scene.
         */
        virtual void AddWidgetToMenu(QWidget *widget, const UiServices::UiWidgetProperties &properties) = 0;

        /** Adds widget to menu without properties. Generates default properties. Takes name from the window title.
         *  @param widget Widget.
         *  @note Doesn't add the widget to the scene.
         */
        virtual void AddWidgetToMenu(QWidget *widget) = 0;

        /** Adds widget to menu.
         *  @param widget Widget.
         *  @param menu Name of the menu entry.
         *  @param menu Name of the menu. If the menu doesn't exist, it is created. If no name is given the entry is
         *  added to the root menu.
         *
         *  @note Doesn't add the widget to the scene.
         */
        virtual void AddWidgetToMenu(QWidget *widget, const QString &entry, const QString &menu = "") = 0;

        /** This is an overloaded function.
         *  @param widget Proxy widget.
         *  @param menu Name of the menu entry.
         *  @param menu Name of the menu. If the menu doesn't exist, it is created. If no name is given the entry is
         *  added to the root menu.
         */
        virtual void AddWidgetToMenu(QGraphicsProxyWidget *widget, const QString &entry, const QString &menu = "") = 0;

        /** Removes widget from the scene.
         *  @param widget Widget.
         */
        virtual void RemoveWidgetFromScene(QWidget *widget) = 0;

        /** This is an overloaded function.
         *  @param widget Proxy widget.
         */
        virtual void RemoveWidgetFromScene(QGraphicsProxyWidget *widget) = 0;

        /** Shows the widget's proxy widget in the scene.
         *  @param widget Widget.
         */
        virtual void ShowWidget(QWidget *widget) const = 0;

        /** Hides the widget's proxy widget in the scene.
         *  @param widget Widget.
         */
        virtual void HideWidget(QWidget *widget) const = 0;

        /** Brings the widget's proxy widget to front in the and sets focus to it.
         *  @param widget Widget.
         */
        virtual void BringWidgetToFront(QWidget *widget) const = 0;

        /** This is an overloaded function.
         *  Brings the proxy widget to front in the scene and sets focus to it.
         *  @param widget Proxy widget.
         */
        virtual void BringWidgetToFront(QGraphicsProxyWidget *widget) const = 0;

        /** Returns scene with the requested name.
         *  @param name Name of the scene.
         *  @return Graphic scene with the requested name, or null if not found.
         */
        virtual QGraphicsScene *GetScene(const QString &name) const = 0;

        /** Registers new scene.
         *  The instance which creates new scene is also responsible for its deletion.
         *  @param name Name of the scene.
         *  @param scene Graphics scene.
         *  @sa UnregisterScene.
         */
        virtual void RegisterScene(const QString &name, QGraphicsScene *scene) = 0;

        /** Unregisters graphics scene.
         *  @param name Name of the scene.
         *  @return True if the scene was found and deleted succesfully, false otherwise.
         *  @note Does not delete the scene.
         */
        virtual bool UnregisterScene(const QString &name) = 0;

        /** Switches the active scene.
         *  @param name Name of the scene.
         *  @return True if the scene existed and was activate ok, false otherwise.
         */
        virtual bool SwitchToScene(const QString &name) = 0;

    signals:
        /** Emitted when scene is changed.
         *  @param oldName Name of the old scene.
         *  @param newName Name of the new scene.
         */
        void SceneChanged(const QString &oldName, const QString &newName);
    };
}

#endif
