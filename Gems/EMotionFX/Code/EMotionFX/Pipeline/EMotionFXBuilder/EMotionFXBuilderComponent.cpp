/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <EMotionFX/Pipeline/EMotionFXBuilder/EMotionFXBuilderComponent.h>

#include <EMotionFX/Source/Allocators.h>
#include <Integration/Assets/ActorAsset.h>
#include <Integration/Assets/MotionAsset.h>
#include <Integration/Assets/MotionSetAsset.h>
#include <Integration/Assets/AnimGraphAsset.h>

#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContextConstants.inl>

#include <EMotionFX/Source/AnimGraphObjectFactory.h>
#include <EMotionFX/Source/Parameter/ParameterFactory.h>

// FIXME: These are all for the joint remapping BS that is "temporarily" tacked on here
#include <SceneAPI/SceneCore/Containers/Scene.h>
#include <SceneAPI/SceneCore/Containers/Utilities/Filters.h>
#include <EMotionFX/Pipeline/SceneAPIExt/Groups/IActorGroup.h>
#include <EMotionFX/Pipeline/SceneAPIExt/Rules/SkeletonRemapRule.h>
#include <AzCore/Debug/Trace.h>

namespace EMotionFX
{
    namespace EMotionFXBuilder
    {
        void EMotionFXBuilderComponent::Activate()
        {
            AZ::RPI::JointRemapBus::Handler::BusConnect();
            m_motionSetBuilderWorker.RegisterBuilderWorker();
            m_animGraphBuilderWorker.RegisterBuilderWorker();

            // Initialize asset handlers.
            m_assetHandlers.emplace_back(aznew EMotionFX::Integration::ActorAssetHandler);
            m_assetHandlers.emplace_back(aznew EMotionFX::Integration::MotionAssetHandler);
            m_assetHandlers.emplace_back(aznew EMotionFX::Integration::MotionSetAssetBuilderHandler);
            m_assetHandlers.emplace_back(aznew EMotionFX::Integration::AnimGraphAssetBuilderHandler);

            // Add asset types and extensions to AssetCatalog.
            auto assetCatalog = AZ::Data::AssetCatalogRequestBus::FindFirstHandler();
            if (assetCatalog)
            {
                assetCatalog->EnableCatalogForAsset(azrtti_typeid<EMotionFX::Integration::ActorAsset>());
                assetCatalog->EnableCatalogForAsset(azrtti_typeid<EMotionFX::Integration::MotionAsset>());
                assetCatalog->EnableCatalogForAsset(azrtti_typeid<EMotionFX::Integration::MotionSetAsset>());
                assetCatalog->EnableCatalogForAsset(azrtti_typeid<EMotionFX::Integration::AnimGraphAsset>());

                assetCatalog->AddExtension("actor");        // Actor
                assetCatalog->AddExtension("motion");       // Motion
                assetCatalog->AddExtension("motionset");    // Motion set
                assetCatalog->AddExtension("animgraph");    // Anim graph
            }
        }

        void EMotionFXBuilderComponent::Deactivate()
        {
            AZ::RPI::JointRemapBus::Handler::BusDisconnect();
            m_motionSetBuilderWorker.BusDisconnect();
            m_animGraphBuilderWorker.BusDisconnect();

            m_assetHandlers.clear();
        }

        void EMotionFXBuilderComponent::Reflect(AZ::ReflectContext* context)
        {
            if (AZ::SerializeContext* serialize = azrtti_cast<AZ::SerializeContext*>(context))
            {
                serialize->Class<EMotionFXBuilderComponent, AZ::Component>()
                    ->Version(1)
                    ->Attribute(AZ::Edit::Attributes::SystemComponentTags, AZStd::vector<AZ::Crc32>({ AssetBuilderSDK::ComponentTags::AssetBuilder }));
            }
        }

        // FIXME: This code shouldn't be here but it is
        void EMotionFXBuilderComponent::SkinDataRemap(const AZ::SceneAPI::Containers::Scene& scene, const AZStd::string& meshName, AZStd::unordered_map<AZStd::string, uint16_t>& jointNameToIndexMap)
        {
            const auto& manifest = scene.GetManifest();
            auto view = AZ::SceneAPI::Containers::MakeDerivedFilterView<Pipeline::Group::IActorGroup>(manifest.GetValueStorage());

            for (const Pipeline::Group::IActorGroup& actorGroup : view)
            {        
                if (actorGroup.GetName() == meshName)
                {
                    const auto skeletonRemapRule = actorGroup.GetRuleContainerConst().FindFirstByType<Pipeline::Rule::SkeletonRemapRule>();
                    if (skeletonRemapRule)
                    {
                        const auto& graph = scene.GetGraph();
                        for (const auto& [skeletonBoneName, profileBoneName] : skeletonRemapRule->GetBoneMap())
                        {
                            AZ::SceneAPI::Containers::SceneGraph::NodeIndex nodeIndex = graph.Find(skeletonBoneName);
                            if (!nodeIndex.IsValid())
                            {
                                AZ_Warning("SkinDataRemap", false, "Bone to remap %s is not stored in the scene. Skipping it.", skeletonBoneName.c_str());
                                continue;
                            }

                            const AZ::SceneAPI::Containers::SceneGraph::Name& nodeName = graph.GetNodeName(nodeIndex);
                            if (auto it = jointNameToIndexMap.find(nodeName.GetName()); it != jointNameToIndexMap.end())
                            {
                                uint16_t index = it->second;
                                jointNameToIndexMap.erase(it);
                                jointNameToIndexMap.emplace(profileBoneName, index);
                            }
                            else
                            {
                                AZ_Warning("SkinDataRemap", false, "Bone to remap %s is not in the jointNameToIndexMap. Skipping it.", nodeName.GetName());
                            }
                        }
                    }
                    break;
                }
            }
        }

    }
}
