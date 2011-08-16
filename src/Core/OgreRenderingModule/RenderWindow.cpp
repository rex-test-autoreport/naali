// For conditions of distribution and use, see copyright notice in license.txt

#include "StableHeaders.h"
#include "DebugOperatorNew.h"
#include "Profiler.h"

#include "RenderWindow.h"
#include "CoreStringUtils.h"

#include <QWidget>
#include <QImage>

#include <utility>

#ifdef Q_WS_X11
#include <QX11Info>
#endif

#include "MemoryLeakCheck.h"

using namespace std;

namespace
{
    const char rttTextureName[] = "MainWindow RTT";
    const char rttMaterialName[] = "MainWindow Material";
}

RenderWindow::RenderWindow()
:renderWindow(0),
overlay(0),
overlayContainer(0)
{
}

void RenderWindow::CreateRenderWindow(QWidget *targetWindow, const QString &name, int width, int height, int left, int top, bool fullscreen)
{
    Ogre::NameValuePairList params;

#ifdef WIN32
    params["externalWindowHandle"] = Ogre::StringConverter::toString((unsigned int)targetWindow->winId());
#endif

#ifdef Q_WS_MAC
// qt docs say it's a HIViewRef on carbon,
// carbon docs say HIViewGetWindow gets a WindowRef out of it
    Ogre::String winhandle;

    QWidget* nativewin = targetWindow;

    while(nativewin->parentWidget())
        nativewin = nativewin->parentWidget();
#if 0
    HIViewRef vref = (HIViewRef) nativewin-> winId ();
    WindowRef wref = HIViewGetWindow(vref);
    winhandle = Ogre::StringConverter::toString(
       (unsigned long) (HIViewGetRoot(wref)));
#else
    // according to
    // http://www.ogre3d.org/forums/viewtopic.php?f=2&t=27027 does
    winhandle = Ogre::StringConverter::toString(
                 (unsigned long) nativewin->winId());
#endif
    //Add the external window handle parameters to the existing params set.
    params["externalWindowHandle"] = winhandle;
#endif

#ifdef Q_WS_X11
    QWidget *parent = targetWindow;
    while(parent->parentWidget())
        parent = parent->parentWidget();

    // GLX - According to Ogre Docs:
    // poslong:posint:poslong:poslong (display*:screen:windowHandle:XVisualInfo*)
    QX11Info info = targetWindow->x11Info();

    Ogre::String winhandle = Ogre::StringConverter::toString((unsigned long)(info.display()));
    winhandle += ":" + Ogre::StringConverter::toString((unsigned int)(info.screen()));
    winhandle += ":" + Ogre::StringConverter::toString((unsigned long)parent->winId());

    //Add the external window handle parameters to the existing params set.
    params["parentWindowHandle"] = winhandle;
#endif

    // Window position to params
    if (left != -1)
        params["left"] = ToString(left);
    if (top != -1)
        params["top"] = ToString(top);

#ifdef USE_NVIDIA_PERFHUD
    params["useNVPerfHUD"] = "true";
    params["Rendering Device"] = "NVIDIA PerfHUD";
#endif

    renderWindow = Ogre::Root::getSingletonPtr()->createRenderWindow(name.toStdString().c_str(), width, height, fullscreen, &params);
    renderWindow->setDeactivateOnFocusChange(false);

    CreateRenderTargetOverlay(width, height);
}

void RenderWindow::CreateRenderTargetOverlay(int width, int height)
{
    width = max(1, width);
    height = max(1, height);

    Ogre::TexturePtr renderTarget = Ogre::TextureManager::getSingleton().createManual(
        rttTextureName, Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
        Ogre::TEX_TYPE_2D, width, height, 0, Ogre::PF_A8R8G8B8, Ogre::TU_DYNAMIC_WRITE_ONLY_DISCARDABLE);

    Ogre::MaterialPtr rttMaterial = Ogre::MaterialManager::getSingleton().create(
        rttMaterialName, Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);

    Ogre::TextureUnitState *rttTuState = rttMaterial->getTechnique(0)->getPass(0)->createTextureUnitState();

    rttTuState->setTextureName(rttTextureName);
    rttTuState->setTextureFiltering(Ogre::TFO_NONE);
    rttTuState->setNumMipmaps(1);
    rttTuState->setTextureAddressingMode(Ogre::TextureUnitState::TAM_CLAMP);

    rttMaterial->setFog(true, Ogre::FOG_NONE); ///\todo Check, shouldn't here be false?
    rttMaterial->setReceiveShadows(false);
    rttMaterial->setTransparencyCastsShadows(false);

    rttMaterial->getTechnique(0)->getPass(0)->setSceneBlending(Ogre::SBF_SOURCE_ALPHA, Ogre::SBF_ONE_MINUS_SOURCE_ALPHA);
    rttMaterial->getTechnique(0)->getPass(0)->setDepthWriteEnabled(false);
    rttMaterial->getTechnique(0)->getPass(0)->setDepthCheckEnabled(false);
    rttMaterial->getTechnique(0)->getPass(0)->setLightingEnabled(false);
    rttMaterial->getTechnique(0)->getPass(0)->setCullingMode(Ogre::CULL_NONE);

    overlayContainer = Ogre::OverlayManager::getSingleton().createOverlayElement("Panel", "MainWindow Overlay Panel");
    overlayContainer->setMaterialName(rttMaterialName);
    overlayContainer->setMetricsMode(Ogre::GMM_PIXELS);
    overlayContainer->setPosition(0, 0);

    overlay = Ogre::OverlayManager::getSingleton().create("MainWindow Overlay");
    overlay->add2D(static_cast<Ogre::OverlayContainer *>(overlayContainer));
    overlay->setZOrder(500);
    overlay->show();

//    ResizeOverlay(width, height);
}

Ogre::RenderWindow *RenderWindow::OgreRenderWindow() const
{
    return renderWindow;
}

Ogre::Overlay *RenderWindow::OgreOverlay() const
{
    return overlay;
}

std::string RenderWindow::OverlayTextureName() const
{
    return rttTextureName;
}

void RenderWindow::UpdateOverlayImage(const QImage &src)
{
    PROFILE(RenderWindow_UpdateOverlayImage);

    Ogre::Box bounds(0, 0, src.width(), src.height());
    Ogre::PixelBox bufbox(bounds, Ogre::PF_A8R8G8B8, (void *)src.bits());

    Ogre::TextureManager &mgr = Ogre::TextureManager::getSingleton();
    Ogre::TexturePtr texture = mgr.getByName(rttTextureName);
    assert(texture.get());
    texture->getBuffer()->blitFromMemory(bufbox);
}

void RenderWindow::ShowOverlay(bool visible)
{
    if (overlayContainer)
    {
        if (!visible)
            overlayContainer->hide();
        else
            overlayContainer->show();
    }
}

void RenderWindow::Resize(int width, int height)
{
    renderWindow->resize(width, height);
    renderWindow->windowMovedOrResized();

    if (Ogre::TextureManager::getSingletonPtr() && Ogre::OverlayManager::getSingletonPtr())
    {
        PROFILE(RenderWindow_Resize);

        // recenter the overlay
//        int left = (renderWindow->getWidth() - width) / 2;
//        int top = (renderWindow->getHeight() - height) / 2;

        // resize the container
 //       overlayContainer->setDimensions(width, height);
  //      overlayContainer->setPosition(left, top);
        overlayContainer->setDimensions((Ogre::Real)width, (Ogre::Real)height);
        overlayContainer->setPosition(0,0);

        // resize the backing texture
        Ogre::TextureManager &mgr = Ogre::TextureManager::getSingleton();
        Ogre::TexturePtr texture = mgr.getByName(rttTextureName);
        assert(texture.get());

        texture->freeInternalResources();
        texture->setWidth(width);
        texture->setHeight(height);
        texture->createInternalResources();
    }
}
