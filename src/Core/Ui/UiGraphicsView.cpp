// For conditions of distribution and use, see copyright notice in license.txt

#include "DebugOperatorNew.h"

#include "UiGraphicsView.h"

#include <QRect>
#include <QList>
#include <QEvent>
#include <QResizeEvent>
#include <QGraphicsItem>

#include <utility>

#include "CoreException.h"

#include "MemoryLeakCheck.h"

using namespace std;

UiGraphicsView::UiGraphicsView(QWidget *parent)
:QGraphicsView(parent), backBuffer(0)
{
    setAutoFillBackground(false);
    setUpdatesEnabled(false);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_PaintOnScreen, true);

    setFocusPolicy(Qt::StrongFocus);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setLineWidth(0);

    setAttribute(Qt::WA_DontShowOnScreen, true);
}

UiGraphicsView::~UiGraphicsView()
{
    delete backBuffer;
}

QImage *UiGraphicsView::BackBuffer()
{ 
    return backBuffer;
}

void UiGraphicsView::MarkViewUndirty()
{
    dirtyRectangle = QRectF(-1, -1, -1, -1);
}

bool UiGraphicsView::IsViewDirty() const
{
    return dirtyRectangle.right() >= 0;
}

QRectF UiGraphicsView::DirtyRectangle() const
{ 
    return dirtyRectangle;
}

void UiGraphicsView::drawBackground(QPainter *painter, const QRectF &rect)
{
    // Default backgroudBrush for QGraphicsScene and QGraphicsView is NoBrush,
    // so this will essentially just paint the animated ether login screen background.
    // Inworld and other scene should not do anything.
    return QGraphicsView::drawBackground(painter, rect);
}

bool UiGraphicsView::event(QEvent *event)
{
    if (event->type() == QEvent::UpdateRequest || event->type() == QEvent::Paint || event->type() == QEvent::Wheel)// || event->type() == QEvent::Resize)
        return true;
/*
    if (event->type() == QEvent::MetaCall)
    {
        QMetaCallEvent *mce = static_cast<QMetaCallEvent*>(event);
        if (mce->signalId() == 31)
            return true;
    }
*/
    return QGraphicsView::event(event);
}

void UiGraphicsView::Resize(int newWidth, int newHeight)
{
    newWidth = max(1, newWidth);
    newHeight = max(1, newHeight);

    setGeometry(0, 0, newWidth, newHeight);
    viewport()->setGeometry(0, 0, newWidth, newHeight);
    scene()->setSceneRect(viewport()->rect());          
    dirtyRectangle = QRectF(0, 0, newWidth, newHeight);

    delete backBuffer;
    backBuffer = new QImage(newWidth, newHeight, QImage::Format_ARGB32);

    emit WindowResized(newWidth, newHeight);
}

void UiGraphicsView::resizeEvent(QResizeEvent *e)
{
    QGraphicsView::resizeEvent(e);
/*
    if (!viewport() || !scene())
        return; ///\todo LogWarning.

    int newWidth = max(1, width());
    int newHeight = max(1, height());
#if 0 ///\todo enable.
    // Resize render window and UI texture to match this
    if (view_)
    {
        view_->ResizeWindow(width(), height());
        view_->ResizeOverlay(viewport()->width(), viewport()->height());
        main_window_->resize(width(), height());
//            if (viewport()->width() != width() || viewport()->height() != height())
//                viewport()->setGeometry(0, 0, width(), height());
//            viewport()->resize(width(), height());

//        if (horizontalScrollBarPolicy() != Qt::ScrollBarAlwaysOff)
//            setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
//        horizontalScrollBar()->setValue(0);
//        horizontalScrollBar()->setRange(0, 0);

//       if (verticalScrollBarPolicy() != Qt::ScrollBarAlwaysOff)
//            setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
//        verticalScrollBar()->setValue(0);
//        verticalScrollBar()->setRange(0, 0);

    }
#endif    
    viewport()->setGeometry(0, 0, width(), height());
    scene()->setSceneRect(viewport()->rect());          
    dirtyRectangle = QRectF(0, 0, width(), height());

    delete backBuffer;
    backBuffer = new QImage(newWidth, newHeight, QImage::Format_ARGB32);

    emit WindowResized(newWidth, newHeight);

//        viewport()->setGeometry(0, 0, backBuffer->width()-1, backBuffer->height()-1);
*/
}

void UiGraphicsView::HandleSceneChanged(const QList<QRectF> &rectangles)
{
    using namespace std;

#ifndef DIRECTX_ENABLED
    // We received an unknown-sized scene change message. Mark everything dirty! (I've no idea what Qt
    // means when it sends a message saying 'nothing changed').
    if (rectangles.size() == 0)
        dirtyRectangle = QRectF(0, 0, width(), height());
#endif

    if (!IsViewDirty() && rectangles.size() > 0)
        dirtyRectangle = rectangles[0];

    // Include an extra guardband pixel to avoid graphical artifacts from occurring when redrawing.
    const int guardbandWidth = 5;

    for(int i = 0; i < rectangles.size(); ++i)
    {
        dirtyRectangle.setLeft(min(dirtyRectangle.left(), rectangles[i].left()-guardbandWidth));
        dirtyRectangle.setTop(min(dirtyRectangle.top(), rectangles[i].top()-guardbandWidth));
        dirtyRectangle.setRight(max(dirtyRectangle.right(), rectangles[i].right()+guardbandWidth));
        dirtyRectangle.setBottom(max(dirtyRectangle.bottom(), rectangles[i].bottom()+guardbandWidth));
    }
    dirtyRectangle.setLeft(max<int>(dirtyRectangle.left(), 0));
    dirtyRectangle.setTop(max<int>(dirtyRectangle.top(), 0));
    dirtyRectangle.setRight(min<int>(dirtyRectangle.right(), width()));
    dirtyRectangle.setBottom(min<int>(dirtyRectangle.bottom(), height()));
}

QGraphicsItem *UiGraphicsView::GetVisibleItemAtCoords(int x, int y) const
{
    // Silently just ignore any invalid coordinates we get. (and we do get them, it seems!)
    if (x < 0 || y < 0 || x >= width() || y >= height())
        return 0;

    QGraphicsItem *item = 0;
    ///\bug Not sure if this function returns the items in the proper depth order! We might not get the topmost window
    /// when this loop finishes.
    QList<QGraphicsItem *> itemsUnderCoord = items(x, y);
    for(int i = 0; i < itemsUnderCoord.size(); ++i)
        if (itemsUnderCoord[i]->isVisible())
        {
            item = itemsUnderCoord[i];
            break;
        }    

    if (!item)
        return 0;

    if (!backBuffer)
        throw Exception("UiGraphicsView::backBuffer not initialized properly!");

    // Do alpha keying: If we have clicked on a transparent part of a widget, act as if we didn't click on a widget at all.
    // This allows clicks to go through to the 3D scene from transparent parts of a widget.
    if (x < backBuffer->width() && y < backBuffer->height() && (backBuffer->pixel(x, y) & 0xFF000000) == 0x00000000)
        item = 0;

    return item;
}

void UiGraphicsView::dropEvent(QDropEvent *e)
{
    // Check whether the drop occurred on top of a QGraphicsView widget or on top of the 3D scene.
    if (GetVisibleItemAtCoords(e->pos().x(), e->pos().y()))
    {
        // Emit the UI drag event, this enabled graphics items to implement drag and drop
        // functionality more easily than to subclass QWidget/QGraphicsWidget.
        QGraphicsView::dropEvent(e);
        emit UiDropEvent(e);
        return;
    }

    // There was no widget on top of the are where the event occurred. Pass the drop event on as a global "3D scene drag-n-drop event".
    // Applications can register to this signal to perform custom 3D scene drag-n-drop handling.
    emit DropEvent(e);
}

void UiGraphicsView::dragEnterEvent(QDragEnterEvent *e)
{
    // Check whether the drop occurred on top of a QGraphicsView widget or on top of the 3D scene.
    if (GetVisibleItemAtCoords(e->pos().x(), e->pos().y()))
    {
        // Emit the UI drag event, this enabled graphics items to implement drag and drop
        // functionality more easily than to subclass QWidget/QGraphicsWidget.
        QGraphicsView::dragEnterEvent(e);
        emit UiDragEnterEvent(e);
        return;
    }

    // There was no widget on top of the are where the event occurred. Pass the drop event on as a global "3D scene drag-n-drop event".
    // Applications can register to this signal to perform custom 3D scene drag-n-drop handling.
    emit DragEnterEvent(e);
}   

void UiGraphicsView::dragLeaveEvent(QDragLeaveEvent *e)
{
    emit DragLeaveEvent(e);
    emit UiDragLeaveEvent(e);
    QGraphicsView::dragLeaveEvent(e);
}

void UiGraphicsView::dragMoveEvent(QDragMoveEvent *e)
{
    // Check whether the drop occurred on top of a QGraphicsView widget or on top of the 3D scene.
    if (GetVisibleItemAtCoords(e->pos().x(), e->pos().y()))
    {
        // Emit the UI drag event, this enabled graphics items to implement drag and drop
        // functionality more easily than to subclass QWidget/QGraphicsWidget.
        QGraphicsView::dragMoveEvent(e);
        emit UiDragMoveEvent(e);
        return;
    }

    // There was no widget on top of the are where the event occurred. Pass the drop event on as a global "3D scene drag-n-drop event".
    // Applications can register to this signal to perform custom 3D scene drag-n-drop handling.
    emit DragMoveEvent(e);
}

#ifdef Q_WS_MAC
void UiGraphicsView::mousePressEvent(QMouseEvent *event)
{
    QGraphicsView::mousePressEvent(event);
}

void UiGraphicsView::mouseReleaseEvent(QMouseEvent *event)
{
    QGraphicsView::mouseReleaseEvent(event);
}

void UiGraphicsView::mouseDoubleClickEvent(QMouseEvent *event)
{
    QGraphicsView::mouseDoubleClickEvent(event);
}

void UiGraphicsView::mouseMoveEvent(QMouseEvent *event)
{
    QGraphicsView::mouseMoveEvent(event);
}

void UiGraphicsView::wheelEvent(QWheelEvent *event)
{
    QGraphicsView::wheelEvent(event);
}
#endif
