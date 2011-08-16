// For conditions of distribution and use, see copyright notice in license.txt

#include "StableHeaders.h"
#include "DebugOperatorNew.h"

#include "EC_OgreCompositor.h"
#include "Renderer.h"
#include "FrameAPI.h"
#include "OgreRenderingModule.h"
#include "CompositionHandler.h"

#include "LoggingFunctions.h"

#include "MemoryLeakCheck.h"

EC_OgreCompositor::EC_OgreCompositor(Scene* scene) :
    IComponent(scene),
    enabled(this, "Enabled", true),
    compositorref(this, "Compositor ref", ""),
    priority(this, "Priority", -1),
    parameters(this, "Parameters"),
    previous_priority_(-1),
    owner_(0),
    handler_(0)
{
    owner_ = framework->GetModule<OgreRenderer::OgreRenderingModule>();
    assert(owner_ && "No OgrerenderingModule.");
    handler_ = owner_->GetRenderer()->GetCompositionHandler();
    assert (handler_ && "No CompositionHandler.");
    connect(this, SIGNAL(AttributeChanged(IAttribute*, AttributeChange::Type)), SLOT(OnAttributeUpdated(IAttribute*)));
    
    // Ogre sucks. Enable a timed one-time refresh to overcome issue with black screen.
    framework->Frame()->DelayedExecute(0.01f, this, SLOT(OneTimeRefresh()));
}

EC_OgreCompositor::~EC_OgreCompositor()
{
    if ((handler_) && (!previous_ref_.isEmpty()))
        handler_->RemoveCompositorFromViewport(previous_ref_.toStdString());
}

void EC_OgreCompositor::OnAttributeUpdated(IAttribute* attribute)
{
    if (attribute == &enabled)
    {
        UpdateCompositor(compositorref.Get());
        UpdateCompositorParams(compositorref.Get());
        handler_->SetCompositorEnabled(compositorref.Get().toStdString(), enabled.Get());
    }

    if (attribute == &compositorref)
    {
        UpdateCompositor(compositorref.Get());
    }

    if (attribute == &priority)
    {
        UpdateCompositor(compositorref.Get());
    }

    if (attribute == &parameters)
    {
        UpdateCompositorParams(compositorref.Get());
    }
}

void EC_OgreCompositor::UpdateCompositor(const QString &compositor)
{
    if (ViewEnabled() && enabled.Get())
    {
        if ((previous_ref_ != compositorref.Get()) || (previous_priority_ != priority.Get()))
        {
            if (!previous_ref_.isEmpty())
                handler_->RemoveCompositorFromViewport(previous_ref_.toStdString());

            if (!compositorref.Get().isEmpty())
            {
                if (priority.Get() == -1)
                    handler_->AddCompositorForViewport(compositor.toStdString());
                else
                    handler_->AddCompositorForViewportPriority(compositor.toStdString(), priority.Get());
            }
            
            previous_ref_ = compositor;
            previous_priority_ = priority.Get();
        }
    }
}

void EC_OgreCompositor::UpdateCompositorParams(const QString &compositor)
{
    if (ViewEnabled() && enabled.Get())
    {
        QList< std::pair<std::string, Ogre::Vector4> > programParams;
        foreach(QVariant keyvalue, parameters.Get())
        {
            QString params = keyvalue.toString();
            QStringList sepParams = params.split('=');
            if (sepParams.size() > 1)
            {
                try
                {
                    Ogre::Vector4 value(0, 0, 0, 0);
                    QStringList valueList = sepParams[1].split(" ", QString::SkipEmptyParts);
                    if (valueList.size() > 0) value.x = boost::lexical_cast<Ogre::Real>(valueList[0].toStdString());
                    if (valueList.size() > 1) value.y = boost::lexical_cast<Ogre::Real>(valueList[1].toStdString());
                    if (valueList.size() > 2) value.z = boost::lexical_cast<Ogre::Real>(valueList[2].toStdString());
                    if (valueList.size() > 3) value.w = boost::lexical_cast<Ogre::Real>(valueList[3].toStdString());
                    std::string name = sepParams[0].toStdString();

                    programParams.push_back(std::make_pair(name, value));
                }
                catch(boost::bad_lexical_cast &) {}
            }
        }
        handler_->SetCompositorParameter(compositorref.Get().toStdString(), programParams);
        handler_->SetCompositorEnabled(compositorref.Get().toStdString(), false);
        handler_->SetCompositorEnabled(compositorref.Get().toStdString(), true);
    }
}

void EC_OgreCompositor::OneTimeRefresh()
{
    if (!compositorref.Get().isEmpty())
        OnAttributeUpdated(&enabled);
}
