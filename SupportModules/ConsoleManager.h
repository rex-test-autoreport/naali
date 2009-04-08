// For conditions of distribution and use, see copyright notice in license.txt

#ifndef incl_ConsoleConsoleManager_h
#define incl_ConsoleConsoleManager_h

#include "ConsoleServiceInterface.h"
#include "Native.h"
#include "OgreOverlay.h"
#include "CommandManager.h"

namespace Console
{
    //! Native debug console
    class ConsoleManager : public Console::ConsoleServiceInterface
    {
        friend class ConsoleModule;
    private:
        ConsoleManager();
        ConsoleManager(const ConsoleManager &other);

        //! constructor that takes a parent module
        ConsoleManager(Foundation::ModuleInterface *parent)
        {
            parent_ = parent;
            command_manager_ = CommandManagerPtr(new CommandManager(parent_, this));
            native_ = ConsolePtr(new Native(command_manager_.get()));
            ogre_ = ConsolePtr(new OgreOverlay(parent));
        }

    public:
        //! destructor
        virtual ~ConsoleManager() {};

        //! If console manager gets created in preinit, or at time when not all needed services are present,
        //! this functions can be used for delayed initialization / creation.
        void CreateDelayed() { checked_static_cast<OgreOverlay*>(ogre_.get())->Create(); }

        __inline virtual void Update()
        {
            command_manager_->Update();
        }

        //! Prints text to the console (all consoles)
        __inline virtual void Print(const std::string &text)
        {
            native_->Print(text);
            ogre_->Print(text);
        }

        virtual void Scroll(int rel)
        {
            native_->Scroll(rel);
            ogre_->Scroll(rel);
        }

        virtual void SetVisible(bool visible)
        {
            ogre_->SetVisible(visible);
        }

        virtual bool IsVisible() const
        {
            return ogre_->IsVisible();
        }

        //! Returns command manager
        CommandManagerPtr GetCommandManager() const {return command_manager_; }

    private:
        //! native debug console
        ConsolePtr native_;

        //! Ogre debug console
        ConsolePtr ogre_;

        //! command manager
        CommandManagerPtr command_manager_;

        //! parent module
        Foundation::ModuleInterface *parent_;
    };
}

#endif

