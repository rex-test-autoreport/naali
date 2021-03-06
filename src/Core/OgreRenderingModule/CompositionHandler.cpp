// For conditions of distribution and use, see copyright notice in license.txt
#include "StableHeaders.h"
#include "DebugOperatorNew.h"
#include "CompositionHandler.h"
#include "OgreRenderingModule.h"

#include <OgreCompositorManager.h>
#include <OgreTechnique.h>
#include <OgreCompositionTechnique.h>
#include <QApplication>
#include "MemoryLeakCheck.h"


namespace OgreRenderer
{
    CompositionHandler::CompositionHandler():
        c_manager_(0),
        viewport_(0),
        framework_(0)
    {
    }

    CompositionHandler::~CompositionHandler()
    {
    }

    bool CompositionHandler::Initialize(Framework* framework, Ogre::Viewport *vp)
    {
        framework_ = framework;
        viewport_ = vp;
        c_manager_ = Ogre::CompositorManager::getSingletonPtr();
        if (c_manager_)
            return true;
        else
            return false;
    }

    void CompositionHandler::RemoveCompositorFromViewport(const std::string &compositor, Ogre::Viewport *vp)
    {
        if (c_manager_!=0 && vp != 0)
        {
            c_manager_->setCompositorEnabled(vp, compositor, false);
            c_manager_->removeCompositor(vp, compositor);

            priorities_.erase(compositor);
        }
    }

    void CompositionHandler::RemoveAllCompositors()
    {
        if (c_manager_!=0)
        {
            c_manager_->removeCompositorChain(viewport_);
            priorities_.clear();
        }
    }
    
    void CompositionHandler::CameraChanged(Ogre::Viewport* vp, Ogre::Camera* newCamera)
    {
        if (c_manager_ && vp && newCamera && c_manager_->hasCompositorChain(vp))
            //c_manager_->getCompositorChain(vp)->_notifyViewport(vp);
            /// \todo This is very shitty logic, but Ogre does not seem to give another way to notify a compositor of main viewport camera change.
            c_manager_->_reconstructAllCompositorResources();
    }
    
    bool CompositionHandler::AddCompositorForViewportPriority(const std::string &compositor, int priority)
    {
        priorities_.insert(std::make_pair(compositor, priority));

        // No need to optimize this probably, as it is fast enough for the rare occassion this gets called
        // for the probably low number of compositors. -cm

        // Push compositors into priority ordered vector
        std::vector<Compositor> priorityOrdered;
        std::map<std::string, int>::const_iterator it = priorities_.begin();
        for(; it != priorities_.end() ; ++it)
        {
            Compositor compositor = { it->first, it->second };
            priorityOrdered.push_back( compositor );
        }
        std::sort(priorityOrdered.begin(), priorityOrdered.end());

        // Get position for the compositor in compositor chain, based on the priority
        int position = -1;
        for(int i=0 ; i<(int)priorityOrdered.size() ; ++i)
        {
            if (compositor == priorityOrdered[i].name)
            {
                position = i;
                break;
            }
        }

        return AddCompositor(compositor, viewport_, position);
    }

    bool CompositionHandler::AddCompositorForViewport(const std::string &compositor, Ogre::Viewport *vp, int position)
    {
        //HDR must be first compositor in the chain
        if (compositor == "HDR" || compositor == "Strong HDR")
            position = 0;

        return AddCompositor(compositor, vp, position);
    }

    bool CompositionHandler::AddCompositor(const std::string &compositor, Ogre::Viewport *vp, int position)
    {
        bool succesfull = false;

        if (c_manager_!=0 && vp != 0)
        {
            Ogre::CompositorInstance* comp = c_manager_->addCompositor(vp, compositor, position);
            if(comp != 0)
            {
                if(compositor == "HDR" || compositor == "Strong HDR")
                {
                    comp->addListener(&hdr_listener_);
                    hdr_listener_.notifyViewportSize(vp->getActualWidth(), vp->getActualHeight());
                    hdr_listener_.notifyCompositor(comp);
                }
                else if(compositor == "Gaussian Blur")
                {
                    comp->addListener(&gaussian_listener_);
                    gaussian_listener_.notifyViewportSize(vp->getActualWidth(), vp->getActualHeight());
                }

                c_manager_->setCompositorEnabled(vp, compositor, true);
                succesfull=true;
            }
        }
        
        if (!succesfull)
            ::LogWarning("Failed to enable effect: " + compositor);
        
        return succesfull;
    }

    bool CompositionHandler::AddCompositorForViewport(const std::string &compositor, int position)
    {
        return AddCompositorForViewport(compositor, viewport_, position);
    }

    void CompositionHandler::RemoveCompositorFromViewport(const std::string &compositor)
    {
        return RemoveCompositorFromViewport(compositor, viewport_);
    }

    void CompositionHandler::SetCompositorParameter(const std::string &compositorName, const QList< std::pair<std::string, Ogre::Vector4> > &source) const
    {
        // find compositor materials
        Ogre::CompositorPtr compositor = c_manager_->getByName(compositorName);
        if (compositor.get())
        {
            compositor->load();
            for(uint t=0 ; t<compositor->getNumTechniques () ; ++t)
            {
                Ogre::CompositionTechnique *ct = compositor->getTechnique(t);
                if (ct)
                {
                    for(uint tp=0 ; tp<ct->getNumTargetPasses () ; ++tp)
                    {
                        Ogre:: CompositionTargetPass *ctp = ct->getTargetPass (tp);
                        SetCompositorTargetParameters(ctp, source);
                    }
                    SetCompositorTargetParameters(ct->getOutputTargetPass(), source);
                }
            }
        }
    }

    void CompositionHandler::SetCompositorEnabled(const std::string &compositor, bool enable) const
    {
        if (c_manager_ && viewport_)
            c_manager_->setCompositorEnabled(viewport_, compositor, enable);
    }

    void CompositionHandler::SetCompositorTargetParameters(Ogre::CompositionTargetPass *target, const QList< std::pair<std::string, Ogre::Vector4> > &source) const
    {
        if (target)
            for(uint p=0 ; p<target->getNumPasses() ; ++p)
            {
                Ogre::CompositionPass *pass = target->getPass(p);
                if (pass)
                    SetMaterialParameters(pass->getMaterial(), source);
            }
    }

    void CompositionHandler::SetMaterialParameters(const Ogre::MaterialPtr &material, const QList< std::pair<std::string, Ogre::Vector4> > &source) const
    {
        assert (material.get());
        material->load();
        for(ushort t=0 ; t<material->getNumTechniques() ; ++t)
        {
            Ogre::Technique *technique = material->getTechnique(t);
            if (technique)
            {
                for(ushort p=0 ; p<technique->getNumPasses() ; ++p)
                {
                    Ogre::Pass *pass = technique->getPass(p);
                    if (pass)
                    {
                        if (pass->hasVertexProgram())
                        {
                            Ogre::GpuProgramParametersSharedPtr destination = pass->getVertexProgramParameters();
                            for(int i=0 ; i<source.size() ; ++i)
                            {
                                if (destination->_findNamedConstantDefinition(source[i].first, false))
                                    destination->setNamedConstant(source[i].first, source[i].second);
                            }

                        }
                        if (pass->hasFragmentProgram())
                        {
                            Ogre::GpuProgramParametersSharedPtr destination = pass->getFragmentProgramParameters();
                            for(int i=0 ; i<source.size() ; ++i)
                            {
                                if (destination->_findNamedConstantDefinition(source[i].first, false))
                                    destination->setNamedConstant(source[i].first, source[i].second);
                            }
                        }
                    }
                }
            }
        }
    }


    /*************************************************************************
    HDRListener Methods
    *************************************************************************/

    HDRListener::HDRListener()
    {
    }

    HDRListener::~HDRListener()
    {
    }

    void HDRListener::notifyViewportSize(int width, int height)
    {
        mVpWidth = width;
        mVpHeight = height;
    }

    void HDRListener::notifyCompositor(Ogre::CompositorInstance* instance)
    {
        // Get some RTT dimensions for later calculations
        Ogre::CompositionTechnique::TextureDefinitionIterator defIter =
            instance->getTechnique()->getTextureDefinitionIterator();
        while(defIter.hasMoreElements())
        {
            Ogre::CompositionTechnique::TextureDefinition* def =
                defIter.getNext();
            if(def->name == "rt_bloom0")
            {
                mBloomSize = (int)def->width; // should be square
                // Calculate gaussian texture offsets & weights
                float deviation = 3.0f;
                float texelSize = 1.0f / (float)mBloomSize;

                // central sample, no offset
                mBloomTexOffsetsHorz[0][0] = 0.0f;
                mBloomTexOffsetsHorz[0][1] = 0.0f;
                mBloomTexOffsetsVert[0][0] = 0.0f;
                mBloomTexOffsetsVert[0][1] = 0.0f;
                mBloomTexWeights[0][0] = mBloomTexWeights[0][1] =
                    mBloomTexWeights[0][2] = Ogre::Math::gaussianDistribution(0, 0, deviation);
                mBloomTexWeights[0][3] = 1.0f;

                // 'pre' samples
                for(int i = 1; i < 8; ++i)
                {
                    mBloomTexWeights[i][0] = mBloomTexWeights[i][1] =
                        mBloomTexWeights[i][2] = 1.25f * Ogre::Math::gaussianDistribution((Ogre::Real)i, 0, deviation);
                    mBloomTexWeights[i][3] = 1.0f;
                    mBloomTexOffsetsHorz[i][0] = i * texelSize;
                    mBloomTexOffsetsHorz[i][1] = 0.0f;
                    mBloomTexOffsetsVert[i][0] = 0.0f;
                    mBloomTexOffsetsVert[i][1] = i * texelSize;
                }
                // 'post' samples
                for(int i = 8; i < 15; ++i)
                {
                    mBloomTexWeights[i][0] = mBloomTexWeights[i][1] =
                        mBloomTexWeights[i][2] = mBloomTexWeights[i - 7][0];
                    mBloomTexWeights[i][3] = 1.0f;

                    mBloomTexOffsetsHorz[i][0] = -mBloomTexOffsetsHorz[i - 7][0];
                    mBloomTexOffsetsHorz[i][1] = 0.0f;
                    mBloomTexOffsetsVert[i][0] = 0.0f;
                    mBloomTexOffsetsVert[i][1] = -mBloomTexOffsetsVert[i - 7][1];
                }
            }
        }
    }

    void HDRListener::notifyMaterialSetup(Ogre::uint32 pass_id, Ogre::MaterialPtr &mat)
    {
        // Prepare the fragment params offsets
        switch(pass_id)
        {
        //case 994: // rt_lum4
        case 993: // rt_lum3
        case 992: // rt_lum2
        case 991: // rt_lum1
        case 990: // rt_lum0
            break;
        case 800: // rt_brightpass
            break;
        case 701: // rt_bloom1
        {
            // horizontal bloom
            mat->load();
            Ogre::GpuProgramParametersSharedPtr fparams =
                mat->getBestTechnique()->getPass(0)->getFragmentProgramParameters();
            const Ogre::String& progName = mat->getBestTechnique()->getPass(0)->getFragmentProgramName();
            UNREFERENCED_PARAM(progName);
            fparams->setNamedConstant("sampleOffsets", mBloomTexOffsetsHorz[0], 15);
            fparams->setNamedConstant("sampleWeights", mBloomTexWeights[0], 15);
            break;
        }
        case 700: // rt_bloom0
        {
            // vertical bloom
            mat->load();
            Ogre::GpuProgramParametersSharedPtr fparams =
                mat->getTechnique(0)->getPass(0)->getFragmentProgramParameters();
            const Ogre::String& progName = mat->getBestTechnique()->getPass(0)->getFragmentProgramName();
            UNREFERENCED_PARAM(progName);
            fparams->setNamedConstant("sampleOffsets", mBloomTexOffsetsVert[0], 15);
            fparams->setNamedConstant("sampleWeights", mBloomTexWeights[0], 15);
            break;
        }
        }
    }

    /*************************************************************************
    GaussianListener Methods
    *************************************************************************/

    GaussianListener::GaussianListener()
    {
    }

    GaussianListener::~GaussianListener()
    {
    }

    void GaussianListener::notifyViewportSize(int width, int height)
    {
        mVpWidth = width;
        mVpHeight = height;
        // Calculate gaussian texture offsets & weights
        float deviation = 3.0f;
        float texelSize = 1.0f / (float)std::min(mVpWidth, mVpHeight);

        // central sample, no offset
        mBloomTexOffsetsHorz[0][0] = 0.0f;
        mBloomTexOffsetsHorz[0][1] = 0.0f;
        mBloomTexOffsetsVert[0][0] = 0.0f;
        mBloomTexOffsetsVert[0][1] = 0.0f;
        mBloomTexWeights[0][0] = mBloomTexWeights[0][1] =
            mBloomTexWeights[0][2] = Ogre::Math::gaussianDistribution(0, 0, deviation);
        mBloomTexWeights[0][3] = 1.0f;

        // 'pre' samples
        for(int i = 1; i < 8; ++i)
        {
            mBloomTexWeights[i][0] = mBloomTexWeights[i][1] =
                mBloomTexWeights[i][2] = Ogre::Math::gaussianDistribution((Ogre::Real)i, 0, deviation);
            mBloomTexWeights[i][3] = 1.0f;
            mBloomTexOffsetsHorz[i][0] = i * texelSize;
            mBloomTexOffsetsHorz[i][1] = 0.0f;
            mBloomTexOffsetsVert[i][0] = 0.0f;
            mBloomTexOffsetsVert[i][1] = i * texelSize;
        }
        // 'post' samples
        for(int i = 8; i < 15; ++i)
        {
            mBloomTexWeights[i][0] = mBloomTexWeights[i][1] =
                mBloomTexWeights[i][2] = mBloomTexWeights[i - 7][0];
            mBloomTexWeights[i][3] = 1.0f;

            mBloomTexOffsetsHorz[i][0] = -mBloomTexOffsetsHorz[i - 7][0];
            mBloomTexOffsetsHorz[i][1] = 0.0f;
            mBloomTexOffsetsVert[i][0] = 0.0f;
            mBloomTexOffsetsVert[i][1] = -mBloomTexOffsetsVert[i - 7][1];
        }
    }

    void GaussianListener::notifyMaterialSetup(Ogre::uint32 pass_id, Ogre::MaterialPtr &mat)
    {
        // Prepare the fragment params offsets
        switch(pass_id)
        {
        case 701: // blur horz
        {
            // horizontal bloom
            mat->load();
            Ogre::GpuProgramParametersSharedPtr fparams =
                mat->getBestTechnique()->getPass(0)->getFragmentProgramParameters();
            const Ogre::String& progName = mat->getBestTechnique()->getPass(0)->getFragmentProgramName();
            UNREFERENCED_PARAM(progName);
            fparams->setNamedConstant("sampleOffsets", mBloomTexOffsetsHorz[0], 15);
            fparams->setNamedConstant("sampleWeights", mBloomTexWeights[0], 15);
            break;
        }
        case 700: // blur vert
        {
            // vertical bloom
            mat->load();
            Ogre::GpuProgramParametersSharedPtr fparams =
                mat->getTechnique(0)->getPass(0)->getFragmentProgramParameters();
            const Ogre::String& progName = mat->getBestTechnique()->getPass(0)->getFragmentProgramName();
            UNREFERENCED_PARAM(progName);
            fparams->setNamedConstant("sampleOffsets", mBloomTexOffsetsVert[0], 15);
            fparams->setNamedConstant("sampleWeights", mBloomTexWeights[0], 15);
            break;
        }
        }
    }
}
