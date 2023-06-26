#pragma once

#include <AzCore/EBus/EBus.h>
#include <AzCore/Interface/Interface.h>
#include <AzCore/Math/Vector2.h>

namespace XR
{
    class System;

    class XRSystemComponentRequests : public AZ::EBusTraits
    {
    public:
        AZ_RTTI(XRSystemComponentRequests, "{E56234D0-B008-4A69-B870-33FDB890DCE9}");
        virtual ~XRSystemComponentRequests() = default;

        static constexpr AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
        static constexpr AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;

        virtual bool Start() = 0;
        virtual void Shutdown() = 0;
        virtual bool IsOpenXREnabled() = 0;
        virtual AZ::Vector2 GetPlayspaceBoundingBox() = 0;
    };

    class XRSystemComponentNotifications : public AZ::EBusTraits
    {
    public:
        AZ_RTTI(XRSystemComponentNotifications, "{666424AB-2C66-4296-8E68-EA5180ED7119}");
        virtual ~XRSystemComponentNotifications() = default;

        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Multiple;
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;

        //! Notification when the XR system is about to start
        virtual bool OnPreStartXRSystem() {}

        //! Notification when the XR system has finished starting
        virtual bool OnPostStartXRSystem() {}

        //! Notification when the XR system is about to shutdown
        virtual bool OnPreShutdownXRSystem() {}

        //! Notification when the XR system has fully shutdown
        virtual bool OnPostShutdownXRSystem() {}
    };

    using XRSystemComponentRequestBus = AZ::EBus<XRSystemComponentRequests>;
    using XRSystemComponentNotificationBus = AZ::EBus<XRSystemComponentNotifications>;

    using XRSystemComponentInterface = AZ::Interface<XRSystemComponentRequests>;

} // namespace VRGame
