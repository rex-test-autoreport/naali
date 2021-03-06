/**
 *  For conditions of distribution and use, see copyright notice in license.txt
 *  @file   EnvironmentModule.h
 *  @brief  Environment module. Environment module is be responsible for visual environment features like terrain, sky & water.
 */

#pragma once

#include "EnvironmentModuleApi.h"
#include "IModule.h"

#include "SceneFwd.h"

namespace Environment
{
    class ENVIRONMENT_MODULE_API EnvironmentModule : public IModule
    {
        Q_OBJECT

    public:
        EnvironmentModule();
        virtual ~EnvironmentModule();

        void Load();

    private:
        Q_DISABLE_COPY(EnvironmentModule);
    };
}
