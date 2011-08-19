// For conditions of distribution and use, see copyright notice in license.txt

#pragma once

#include "UiFwd.h"
#include "UiApiExport.h"

#include <QGraphicsView>

class QDropEvent;
class QDragEnterEvent;
class QDragMoveEvent;
class QMouseEvent;
class QWheelEvent;

/// The main view consists of a single QGraphicsView that spans the whole viewport.
/** UiGraphicsView implements pixel-perfect alpha-tested mouse hotspots for Qt widgets to
    determine whether clicks should go to the scene or to Qt widgets.
*/
class UI_API UiGraphicsView : public QGraphicsView
{
    Q_OBJECT;

public:
    explicit UiGraphicsView(QWidget *parent);

    ~UiGraphicsView();

    /// Returns the currently shown UI content as an image.
    QImage *BackBuffer();

    /// Returns true if there are rectangles pending repaint in the view.
    bool IsViewDirty() const;

    /// Marks the whole UI screen dirty, pending a full repaint.
    void MarkViewUndirty();

    /// Returns the rectangle that represents the dirty area of the screen, pending a Qt repaint.
    QRectF DirtyRectangle() const;

public slots:
    /// Returns the topmost visible QGraphicsItem in the given application main window coordinates.
    QGraphicsItem *GetVisibleItemAtCoords(int x, int y) const;

    /// Sets a new size for this widget. Will emit the WindowResized signal.
    void Resize(int newWidth, int newHeight);

signals:
    /// Emitted when this widget has been resized to a new size.
    void WindowResized(int newWidth, int newHeight);    

    /// Emitted when DragEnterEvent is received for the main window.
    /// @note This signal is only emitted if no graphics items are under the mouse
    /// see UiDragEnterEvent for signal that is emitted while on top ui.
    /// @note If your graphics item is not full screen in the graphics scene, you
    /// need to accept with DragEnterEvent and track with UiDragMoveEvent.
    /// @param e Event.
    void DragEnterEvent(QDragEnterEvent *e);

    /// Emitted when DragLeaveEvent is received for the main window.
    /// @note This signal is only emitted if no graphics items are under the mouse
    /// see UiDragLeaveEvent for signal that is emitted while on top ui.
    /// @param e Event.
    void DragLeaveEvent(QDragLeaveEvent *e);

    /// Emitted when DragMoveEvent is received for the main window.
    /// @note This signal is only emitted if no graphics items are under the mouse
    /// see UiDragMoveEvent for signal that is emitted while on top ui.
    /// @param e Event.
    void DragMoveEvent(QDragMoveEvent *e);

    /// Emitted when DropEvent is received for the main window.
    /// @note This signal is only emitted if no graphics items are under the mouse
    /// see UiDropEvent for signal that is emitted while on top ui.
    /// @param e Event.
    void DropEvent(QDropEvent *e);

    /// Emitted when DragEnterEvent is received for the main window 
    /// and there is a graphics item under the drag.
    /// @note See also DragEnterEvent.
    /// @param e Event.
    void UiDragEnterEvent(QDragEnterEvent *e);

    /// Emitted when DragLeaveEvent is received for the main window
    /// and there is a graphics item under the drag.
    /// @note See also DragLeaveEvent.
    /// @param e Event.
    void UiDragLeaveEvent(QDragLeaveEvent *e);

    /// Emitted when DragMoveEvent is received for the main window
    /// and there is a graphics item under the drag.
    /// @note See also DragMoveEvent.
    /// @param e Event.
    void UiDragMoveEvent(QDragMoveEvent *e);

    /// Emitted when DropEvent is received for the main window
    /// and there is a graphics item under the drag.
    /// @note See also DropEvent.
    /// @param e Event.
    void UiDropEvent(QDropEvent *e);

private:
    QImage *backBuffer;
    QRectF dirtyRectangle;

    /// This virtual function is overridden from the QGraphicsView original to disable any background drawing functionality.
    /// The main QGraphicsView background displays the 3D scene rendered using Ogre.
    void drawBackground(QPainter *, const QRectF &);

    /// Overridden to disable QEvent::UpdateRequest, QEvent::Paint and QEvent::Wheel events from being processed in the base class,
    /// which cause flickering on the screen (Qt internals have some hard-coded full-screen rectangle repaints on these signals).
    bool event(QEvent *event);

    void resizeEvent(QResizeEvent *e);

    // We override the Qt widget drag-n-drop events to be able to expose them as Qt signals (DragEnterEvent, DragMoveEvent and DropEvent)
    // to all client applications. The individual modules can listen to those signals to be able to perform drag-n-drop
    // handling of custom mime types.
    void dragEnterEvent(QDragEnterEvent *e);
    void dragLeaveEvent(QDragLeaveEvent *e);
    void dragMoveEvent(QDragMoveEvent *e);
    void dropEvent(QDropEvent *e);

private slots:
    void HandleSceneChanged(const QList<QRectF> &rectangles);
#ifdef Q_WS_MAC
public:
    void mousePressEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void mouseDoubleClickEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void wheelEvent(QWheelEvent *event);
#endif
};

