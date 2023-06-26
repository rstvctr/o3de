/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <XR/XRSystemComponent.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Settings/SettingsRegistry.h>
#include <Atom/RPI.Public/XR/XRRenderingInterface.h>
#include <Atom/RHI/ValidationLayer.h>
#include <Atom/RHI/FactoryManagerBus.h>
#include <AzFramework/StringFunc/StringFunc.h>
#include <AzFramework/API/ApplicationAPI.h>
#include <AzFramework/CommandLine/CommandLine.h>

static constexpr char OpenXREnableSetting[] = "/O3DE/Atom/OpenXR/Enable";

namespace XR
{
    void SystemComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("XRSystemService"));
    }

    void SystemComponent::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<SystemComponent, AZ::Component>()->Version(1);
        }
    }

    void SystemComponent::Activate()
    {
        XR::XRSystemComponentRequestBus::Handler::BusConnect();

        // Register XR system interface if openxr is enabled via command line or settings registry
        if (IsOpenXREnabled())
        {
            Start();
        }
    }

    void SystemComponent::Deactivate()
    {
        Shutdown();

        XR::XRSystemComponentRequestBus::Handler::BusDisconnect();
    }

    bool SystemComponent::IsOpenXREnabled()
    {
        const AzFramework::CommandLine* commandLine = nullptr;
        AZStd::string commandLineValue;

        //Check command line option(-openxr=enable)
        AzFramework::ApplicationRequests::Bus::BroadcastResult(commandLine, &AzFramework::ApplicationRequests::GetApplicationCommandLine);
        if (commandLine)
        {
            if (size_t switchCount = commandLine->GetNumSwitchValues("openxr"); switchCount > 0)
            {
                commandLineValue = commandLine->GetSwitchValue("openxr", switchCount - 1);
            }
        }
        bool isOpenXREnabledViaCL = AzFramework::StringFunc::Equal(commandLineValue.c_str(), "enable");

        //Check settings registry
        AZ::SettingsRegistryInterface* settingsRegistry = AZ::SettingsRegistry::Get();
        bool isOpenXREnabledViaSR = false;
        if (settingsRegistry)
        {
            settingsRegistry->Get(isOpenXREnabledViaSR, OpenXREnableSetting);
        }
        return isOpenXREnabledViaSR || isOpenXREnabledViaCL;
    }

    bool SystemComponent::Start()
    {
        if (!m_xrSystem)
        {
            XRSystemComponentNotificationBus::Broadcast(&XRSystemComponentNotifications::OnPreStartXRSystem);

            //Get the validation mode
            AZ::RHI::ValidationMode validationMode = AZ::RHI::ValidationMode::Disabled;
            AZ::RHI::FactoryManagerBus::BroadcastResult(validationMode, &AZ::RHI::FactoryManagerRequest::DetermineValidationMode);

            //Init the XRSystem
            System::Descriptor descriptor;
            descriptor.m_validationMode = validationMode;
            m_xrSystem = aznew System();
            m_xrSystem->Init(descriptor);

            //Register xr system with RPI
            AZ::RPI::XRRegisterInterface::Get()->RegisterXRInterface(m_xrSystem.get());

            XRSystemComponentNotificationBus::Broadcast(&XRSystemComponentNotifications::OnPostStartXRSystem);
        }

        return true;
    }

    void SystemComponent::Shutdown()
    {
        if (m_xrSystem)
        {
            XRSystemComponentNotificationBus::Broadcast(&XRSystemComponentNotifications::OnPreShutdownXRSystem);

            AZ::RPI::XRRegisterInterface::Get()->UnRegisterXRInterface();

            m_xrSystem->Shutdown();
            m_xrSystem.reset();

            XRSystemComponentNotificationBus::Broadcast(&XRSystemComponentNotifications::OnPostShutdownXRSystem);
        }
    }

    AZ::Vector2 SystemComponent::GetPlayspaceBoundingBox()
    {
        if (!m_xrSystem)
            return {};

        if (XR::Session* session = m_xrSystem->GetSession())
        {
            if (XR::Space* space = session->GetSpace())
            {
                return space->GetPlayspaceBounds(session);
            }
        }

        return {};
    }
}
