// For conditions of distribution and use, see copyright notice in license.txt

#include "EC_SkyX.h"
#include "EC_Hydrax.h"

#include "Framework.h"
#include "SceneAPI.h"
#include "IComponentFactory.h"

extern "C"
{

__declspec(dllexport) void TundraPluginMain(Framework *fw)
{
    Framework::SetInstance(fw); // Inside this DLL, remember the pointer to the global framework object.
#if SKYX_ENABLED
    fw->Scene()->RegisterComponentFactory(ComponentFactoryPtr(new GenericComponentFactory<EC_SkyX>));
#endif
#if HYDRAX_ENABLED
    fw->Scene()->RegisterComponentFactory(ComponentFactoryPtr(new GenericComponentFactory<EC_Hydrax>));
#endif
}

} // ~extern "C"
