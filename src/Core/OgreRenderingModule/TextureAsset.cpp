// For conditions of distribution and use, see copyright notice in license.txt

#include "StableHeaders.h"
#include "DebugOperatorNew.h"

#include "Profiler.h"
#include "TextureAsset.h"
#include "OgreConversionUtils.h"
#include "AssetCache.h"

#include <QPixmap>
#include <QRect>
#include <QFontMetrics>
#include <QPainter>
#include <QFileInfo>

#include "OgreRenderingModule.h"
#include <Ogre.h>

#if defined(DIRECTX_ENABLED) && defined(WIN32)
#include <d3d9.h>
#include <RenderSystems/Direct3D9/OgreD3D9RenderSystem.h>
#include <RenderSystems/Direct3D9/OgreD3D9HardwarePixelBuffer.h>
#endif

#include "MemoryLeakCheck.h"

#include "LoggingFunctions.h"

TextureAsset::TextureAsset(AssetAPI *owner, const QString &type_, const QString &name_)
:IAsset(owner, type_, name_)
{
    ogreAssetName = OgreRenderer::SanitateAssetIdForOgre(this->Name().toStdString()).c_str();
}

TextureAsset::~TextureAsset()
{
    Unload();
}

bool TextureAsset::DeserializeFromData(const u8 *data, size_t numBytes, const bool allowAsynchronous)
{
    if (!data)
    {
        LogError("TextureAsset::DeserializeFromData failed: Cannot deserialize from input null pointer!");
        return false;
    }
    if (numBytes == 0)
    {
        LogError("TextureAsset::DeserializeFromData failed: numBytes == 0!");
        return false;
    }

    // A NullAssetFactory has been registered on headless mode.
    // We should never be here in headless mode.
    assert(!assetAPI->IsHeadless());

    // Asynchronous loading
    if (allowAsynchronous && (OGRE_THREAD_SUPPORT != 0))
    {
        // We can only do threaded loading from disk, and not any disk location but only from asset cache.
        // local:// refs will return empty string here and those will fall back to the non-threaded loading.
        // Do not change this to do DiskCache() as that directory for local:// refs will not be a known resource location for ogre.
        QString cacheDiskSource = assetAPI->GetAssetCache()->GetDiskSourceByRef(Name());
        if (!cacheDiskSource.isEmpty())
        {
            QFileInfo fileInfo(cacheDiskSource);
            std::string sanitatedAssetRef = fileInfo.fileName().toStdString();             
            loadTicket_ = Ogre::ResourceBackgroundQueue::getSingleton().load(Ogre::TextureManager::getSingleton().getResourceType(),
                              sanitatedAssetRef, OgreRenderer::OgreRenderingModule::CACHE_RESOURCE_GROUP, false, 0, 0, this);
            return true;
        }
    }   

    // Synchronous loading
    try
    {
        // Convert the data into Ogre's own DataStream format.
        std::vector<u8> tempData(data, data + numBytes);
#include "DisableMemoryLeakCheck.h"
        Ogre::DataStreamPtr stream(new Ogre::MemoryDataStream(&tempData[0], tempData.size(), false));
#include "EnableMemoryLeakCheck.h"
        // Load up the image as an Ogre CPU image object.
        Ogre::Image image;
        image.load(stream);

        if (ogreTexture.isNull()) // If we are creating this texture for the first time, create a new Ogre::Texture object.
        {
            ogreAssetName = OgreRenderer::SanitateAssetIdForOgre(this->Name().toStdString()).c_str();
            ogreTexture = Ogre::TextureManager::getSingleton().loadImage(ogreAssetName.toStdString(), Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME, image);
        }
        else // If we're loading on top of an Ogre::Texture we've created before, don't lose the old Ogre::Texture object, but reuse the old.
        {    // This will allow all existing materials to keep referring to this texture, and they'll get the updated texture image immediately.
            ogreTexture->freeInternalResources(); 

            if (image.getWidth() != ogreTexture->getWidth() || image.getHeight() != ogreTexture->getHeight() || image.getFormat() != ogreTexture->getFormat())
            {
                ogreTexture->setWidth(image.getWidth());
                ogreTexture->setHeight(image.getHeight());
                ogreTexture->setFormat(image.getFormat());
            }

            if (ogreTexture->getBuffer().isNull())
            {
                LogError("DeserializeFromData: Failed to create texture " + this->Name().toStdString() + ": OgreTexture::getBuffer() was null!");
                return false;
            }

            Ogre::PixelBox pixelBox(Ogre::Box(0,0, image.getWidth(), image.getHeight()), image.getFormat(), (void*)image.getData());
            ogreTexture->getBuffer()->blitFromMemory(pixelBox);

            ogreTexture->createInternalResources();
        }

        // We did a synchronous load, must call AssetLoadCompleted here.
        assetAPI->AssetLoadCompleted(Name());
        return true;
    }
    catch(Ogre::Exception &e)
    {
        LogError("TextureAsset::DeserializeFromData: Failed to create texture " + this->Name().toStdString() + ": " + std::string(e.what()));
        return false;
    }
}

void TextureAsset::operationCompleted(Ogre::BackgroundProcessTicket ticket, const Ogre::BackgroundProcessResult &result)
{
    if (ticket != loadTicket_)
        return;

    const QString assetRef = Name();
    ogreAssetName = QString::fromStdString(OgreRenderer::SanitateAssetIdForOgre(assetRef));
    if (!result.error)
    {
        ogreTexture = Ogre::TextureManager::getSingleton().getByName(ogreAssetName.toStdString(), OgreRenderer::OgreRenderingModule::CACHE_RESOURCE_GROUP);
        if (!ogreTexture.isNull())
        {
            assetAPI->AssetLoadCompleted(assetRef);
            return;
        }
        else
            LogError("TextureAsset asynch load: Ogre::Texture was null after threaded loading: " + assetRef);
    }
    else
        LogError("TextureAsset asynch load: Ogre failed to do threaded loading: " + result.message);

    DoUnload();
    assetAPI->AssetLoadFailed(assetRef);
}

/*
void TextureAsset::RegenerateAllMipLevels()
{
    if (ogreTexture.isNull())
        return;

///\todo This function does not quite work, since ogreTexture->getNumMipmaps() will return 0 to denote a "full mipmap chain".

    for(int f = 0; f < ogreTexture->getNumFaces(); ++f)
        for(int i = 1; i < ogreTexture->getNumMipmaps(); ++i)
        {
            Ogre::HardwarePixelBufferSharedPtr src = ogreTexture->getBuffer(f, i-1);
            Ogre::Box srcSize(0, 0, src->getWidth(), src->getHeight());
            Ogre::HardwarePixelBufferSharedPtr dst = ogreTexture->getBuffer(f, i);
            Ogre::Box dstSize(0, 0, dst->getWidth(), dst->getHeight());
            dst->blit(src, srcSize, dstSize);
        }
}
*/
bool TextureAsset::SerializeTo(std::vector<u8> &data, const QString &serializationParameters) const
{
    if (ogreTexture.isNull())
    {
        LogWarning("SerializeTo: Called on an unloaded texture \"" + Name().toStdString() + "\".");
        return false;
    }

    try
    {
        Ogre::Image new_image;
                
        // From Ogre 1.7 Texture::convertToImage()
        size_t numMips = 1;
        size_t dataSize = Ogre::Image::calculateSize(numMips,
            ogreTexture->getNumFaces(), ogreTexture->getWidth(), ogreTexture->getHeight(), ogreTexture->getDepth(), ogreTexture->getFormat());
        void* pixData = OGRE_MALLOC(dataSize, Ogre::MEMCATEGORY_GENERAL);
        // if there are multiple faces and mipmaps we must pack them into the data
        // faces, then mips
        void* currentPixData = pixData;
        for(size_t face = 0; face < ogreTexture->getNumFaces(); ++face)
        {
            for(size_t mip = 0; mip < numMips; ++mip)
            {
                size_t mipDataSize = Ogre::PixelUtil::getMemorySize(ogreTexture->getWidth(), ogreTexture->getHeight(), ogreTexture->getDepth(), ogreTexture->getFormat());
                Ogre::PixelBox pixBox(ogreTexture->getWidth(), ogreTexture->getHeight(), ogreTexture->getDepth(), ogreTexture->getFormat(), currentPixData);
                ogreTexture->getBuffer(face, mip)->blitToMemory(pixBox);
                currentPixData = (void*)((char*)currentPixData + mipDataSize);
            }
        }
        // load, and tell Image to delete the memory when it's done.
        new_image.loadDynamicImage((Ogre::uchar*)pixData, ogreTexture->getWidth(), ogreTexture->getHeight(), ogreTexture->getDepth(), ogreTexture->getFormat(), true, 
            ogreTexture->getNumFaces(), numMips - 1);

        Ogre::DataStreamPtr imageStream = new_image.encode(serializationParameters.toStdString());
        if (imageStream.get() && imageStream->size() > 0)
        {
            data.resize(imageStream->size());
            imageStream->read(&data[0], data.size());
        }
    } catch(std::exception &e)
    {
        LogError("SerializeTo: Failed to export Ogre texture " + Name().toStdString() + ":");
        if (e.what())
            LogError(e.what());
        return false;
    }
    return true;
}

void TextureAsset::DoUnload()
{
    if (!ogreTexture.isNull())
        ogreAssetName = ogreTexture->getName().c_str();

    ogreTexture = Ogre::TexturePtr();
    try
    {
        Ogre::TextureManager::getSingleton().remove(ogreAssetName.toStdString());
    }
    catch(...) {}
}

bool TextureAsset::IsLoaded() const
{
    return ogreTexture.get() != 0;
}

QImage TextureAsset::ToQImage(size_t faceIndex, size_t mipmapLevel) const
{
    if (!ogreTexture.get())
    {
        LogError("TextureAsset::ToQImage: Can't convert texture to QImage, Ogre texture is not initialized for asset \"" + ToString() + "\"!");
        return QImage();
    }

    Ogre::HardwarePixelBufferSharedPtr pixelBuffer = ogreTexture->getBuffer(faceIndex, mipmapLevel);
    QImage::Format fmt;
    switch(pixelBuffer->getFormat())
    {
    case Ogre::PF_X8R8G8B8: fmt = QImage::Format_RGB32; break;
    case Ogre::PF_A8R8G8B8: fmt = QImage::Format_ARGB32; break;
    case Ogre::PF_R5G6B5: fmt = QImage::Format_RGB16; break;
    case Ogre::PF_R8G8B8: fmt = QImage::Format_RGB888; break;
    default:
        LogError("TextureAsset::ToQImage: Cannot convert Ogre TextureAsset \"" + Name() + "\" to QImage: Unsupported Ogre format of type " + (int)pixelBuffer->getFormat());
        return QImage();
    }

    void *data = pixelBuffer->lock(Ogre::HardwareBuffer::HBL_READ_ONLY);
    if (!data)
    {
        LogError("TextureAsset::ToQImage: Failed to lock Ogre TextureAsset \"" + Name() + "\" for reading!");
        return QImage();
    }
    QImage img((uchar*)data, pixelBuffer->getWidth(), pixelBuffer->getHeight(), fmt);
    pixelBuffer->unlock();
    return img;
}

void TextureAsset::SetContentsFillSolidColor(int newWidth, int newHeight, u32 color, Ogre::PixelFormat ogreFormat, bool regenerateMipmaps, bool dynamic)
{
    if (newWidth == 0 || newHeight == 0)
    {
        Unload();
        return;
    }
    ///\todo Could optimize a lot here, don't create this temporary vector.
    ///\todo This only works for 32bpp images.
    std::vector<u32> data(newWidth * newHeight, color);
    SetContents(newWidth, newHeight, (const u8*)&data[0], data.size() * sizeof(u32), ogreFormat, regenerateMipmaps, dynamic);
}

void TextureAsset::SetContents(int newWidth, int newHeight, const u8 *data, size_t numBytes, Ogre::PixelFormat ogreFormat, bool regenerateMipMaps, bool dynamic)
{
    PROFILE(TextureAsset_SetContents);

    int usage = dynamic ? Ogre::TU_DYNAMIC_WRITE_ONLY_DISCARDABLE : Ogre::TU_STATIC_WRITE_ONLY;
    if (regenerateMipMaps)
        usage |= Ogre::TU_AUTOMIPMAP;

    if (numBytes != newWidth * newHeight * 4)
    {
        LogError("TextureAsset::SetContents failed: Inputted " + QString::number(numBytes) + " bytes of data, but " + QString::number(newWidth) + "x" + QString::number(newHeight)
            + " at 4 bytes per pixel requires " + QString::number(newWidth * newHeight * 4) + " bytes!");
        return;
    }

    if (!ogreTexture.get())
    {
        ogreTexture = Ogre::TextureManager::getSingleton().createManual(Name().toStdString(), Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME, Ogre::TEX_TYPE_2D,
            newWidth, newHeight, regenerateMipMaps ? Ogre::MIP_UNLIMITED : 0, ogreFormat, usage);
        if (!ogreTexture.get())
        {
            LogError("TextureAsset::SetContents failed: Cannot create texture asset \"" + ToString() + "\" to name \"" + Name() + "\" and size " + QString::number(newWidth) + "x" + QString::number(newHeight) + "!");
            return;
        }
    }

    bool needRecreate = (newWidth != ogreTexture->getWidth() || newHeight != ogreTexture->getHeight() || ogreFormat != ogreTexture->getFormat());
//    if (newWidth == ogreTexture->getWidth() && newHeight == ogreTexture->getHeight() && ogreFormat == ogreTexture->getFormat())
//        return;

    if (needRecreate)
    {
        ogreTexture->freeInternalResources(); 
        ogreTexture->setWidth(newWidth);
        ogreTexture->setHeight(newHeight);
        ogreTexture->setFormat(ogreFormat);
    }
    if (ogreTexture->getBuffer().isNull())
    {
        LogError("DeserializeFromData: Failed to create texture " + this->Name().toStdString() + ": OgreTexture::getBuffer() was null!");
        return;
    }

    if (data)
    {
#if defined(DIRECTX_ENABLED) && defined(WIN32)
        Ogre::HardwarePixelBufferSharedPtr pb = ogreTexture->getBuffer();
        Ogre::D3D9HardwarePixelBuffer *pixelBuffer = dynamic_cast<Ogre::D3D9HardwarePixelBuffer*>(pb.get());
        assert(pixelBuffer);

        LPDIRECT3DSURFACE9 surface = pixelBuffer->getSurface(Ogre::D3D9RenderSystem::getActiveD3D9Device());
        if (surface)
        {
            D3DSURFACE_DESC desc;
            HRESULT hr = surface->GetDesc(&desc);
            if (SUCCEEDED(hr))
            {
                D3DLOCKED_RECT lock;
                HRESULT hr = surface->LockRect(&lock, 0, 0);
                if (SUCCEEDED(hr))
                {
                    const int bytesPerPixel = 4; ///\todo Count from Ogre::PixelFormat!
                    const int sourceStride = bytesPerPixel * newWidth;
                    if (lock.Pitch == sourceStride)
                        memcpy(lock.pBits, data, sourceStride * newHeight);
                    else
                        for(int y = 0; y < newHeight; ++y)
                            memcpy((u8*)lock.pBits + lock.Pitch * y, data + sourceStride * y, sourceStride);
                    surface->UnlockRect();
                }
            }
        }
#else        
        ///\todo Review Ogre internals of whether the const_cast here is safe!
        Ogre::PixelBox pixelBox(Ogre::Box(0,0, newWidth, newHeight), ogreFormat, const_cast<u8*>(data));
        ogreTexture->getBuffer()->blitFromMemory(pixelBox);
#endif
    }

    if (needRecreate)
        ogreTexture->createInternalResources();
}

void TextureAsset::SetContentsDrawText(int newWidth, int newHeight, QString text, const QColor &textColor, const QFont &font, const QBrush &backgroundBrush, const QPen &borderPen, int flags, bool generateMipmaps, bool dynamic)
{
    text = text.replace("\\n", "\n");

    // Create transparent pixmap
    QImage image(newWidth, newHeight, QImage::Format_ARGB32);
    image.fill(Qt::transparent);

    {
        // Init painter with pixmap as the paint device
        QPainter painter(&image);

        // Ask painter the rect for the text
        painter.setFont(font);
        QRect rect = painter.boundingRect(image.rect(), flags, text);

        // Set background brush
        painter.setBrush(backgroundBrush);
        painter.setPen(borderPen);
        painter.drawRoundedRect(rect, 20.0, 20.0, Qt::RelativeSize);

        // Draw text
        painter.setPen(textColor);
        painter.drawText(rect, flags, text);
    }

    SetContents(newWidth, newHeight, image.bits(), image.byteCount(), Ogre::PF_A8R8G8B8, generateMipmaps, dynamic);
}
