#pragma once

#include <AzCore/EBus/EBus.h>

namespace AZ
{
    namespace SceneAPI
    {
        namespace Containers
        {
            class Scene;
        }
    }

    namespace RPI
    {
        class JointRemapEvents
            : public EBusTraits
        {
        public:
            // EBus Configuration
            static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
            static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;

            virtual void SkinDataRemap(const SceneAPI::Containers::Scene& scene, const AZStd::string& meshName, AZStd::unordered_map<AZStd::string, uint16_t>& jointNameToIndexMap) = 0;
        };

        using JointRemapBus = AZ::EBus<JointRemapEvents>;
    } // namespace RPI
} // namespace AZ
