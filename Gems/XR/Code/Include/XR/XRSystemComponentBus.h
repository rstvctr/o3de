#pragma once

#include <AzCore/EBus/EBus.h>
#include <AzCore/Interface/Interface.h>

namespace XR
{

    class XRSystemComponentRequests
    {
    public:
        AZ_RTTI(XRSystemComponentRequests, "{E56234D0-B008-4A69-B870-33FDB890DCE9}");
        virtual ~XRSystemComponentRequests() = default;

        virtual bool Start() = 0;
        virtual void Shutdown() = 0;
        virtual bool IsOpenXREnabled() = 0;
    };

    class XRSystemComponentBusTraits : public AZ::EBusTraits
    {
    public:
        //////////////////////////////////////////////////////////////////////////
        // EBusTraits overrides
        static constexpr AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
        static constexpr AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;
        //////////////////////////////////////////////////////////////////////////
    };

    using XRSystemComponentRequestBus = AZ::EBus<XRSystemComponentRequests, XRSystemComponentBusTraits>;
    using XRSystemComponentInterface = AZ::Interface<XRSystemComponentRequests>;

} // namespace VRGame
