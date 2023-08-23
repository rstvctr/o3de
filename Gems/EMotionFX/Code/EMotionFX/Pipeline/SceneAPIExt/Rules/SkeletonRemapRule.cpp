#include <AzCore/Serialization/EditContext.h>
#include <AzCore/RTTI/ReflectContext.h>
#include <AzCore/Memory/SystemAllocator.h>
#include <SceneAPI/SceneCore/DataTypes/GraphData/IBoneData.h>
#include <SceneAPIExt/Rules/SkeletonRemapRule.h>

namespace EMotionFX
{
    AZ_CLASS_ALLOCATOR_IMPL(BoneMap, AZ::SystemAllocator)

    void BoneMap::EventHandler::OnReadEnd(void* classPtr)
    {
        BoneMap* boneMap = static_cast<BoneMap*>(classPtr);

        // Re-generate the reverse map after serialization completes
        boneMap->m_profileToOrigBoneMap.clear();
        for (const auto& [origBoneName, profileBoneName] : boneMap->m_boneMap)
        {
            boneMap->m_profileToOrigBoneMap.emplace(profileBoneName, origBoneName);
        }
    }

    void BoneMap::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<BoneMap>()
                ->Version(1)
                ->EventHandler<BoneMap::EventHandler>()
                ->Field("BoneMap", &BoneMap::m_boneMap);
        }
    }

    void BoneMap::SetSkeletonBoneName(const AZStd::string& mappedBoneName, const AZStd::string& origBoneName)
    {
        m_boneMap.emplace(origBoneName, mappedBoneName);
        m_profileToOrigBoneMap.emplace(mappedBoneName, origBoneName);
    }

    void BoneMap::Clear()
    {
        m_boneMap.clear();
        m_profileToOrigBoneMap.clear();
    }

    void BoneMap::Remove(const AZStd::string& name)
    {
        if (auto it = m_profileToOrigBoneMap.find(name); it != m_profileToOrigBoneMap.end())
        {
            m_boneMap.erase(it->second);
            m_profileToOrigBoneMap.erase(it);
        }
    }

    bool BoneMap::HasBone(const AZStd::string& name) const
    {
        return m_profileToOrigBoneMap.find(name) != m_profileToOrigBoneMap.end();
    }

    const AZStd::string& BoneMap::GetOrigBone(const AZStd::string& name) const
    {
        return m_profileToOrigBoneMap.at(name);
    }

    namespace Pipeline
    {
        namespace Rule
        {
            AZ_CLASS_ALLOCATOR_IMPL(SkeletonRemapRule, AZ::SystemAllocator)

            void SkeletonRemapRule::Reflect(AZ::ReflectContext* context)
            {
                BoneMap::Reflect(context);

                AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
                if (!serializeContext)
                {
                    return;
                }

                serializeContext->Class<SkeletonRemapRule, IRule>()
                    ->Version(1)
                    ->Field("BoneMap", &SkeletonRemapRule::m_boneMap);

                AZ::EditContext* editContext = serializeContext->GetEditContext();
                if (editContext)
                {
                    editContext->Class<SkeletonRemapRule>("Skeleton Remap", "Remap skeleton bone names to a standard naming convention.")
                        ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                        ->Attribute(AZ::Edit::Attributes::NameLabelOverride, "")
                        ->DataElement(AZ_CRC_CE("BoneMapHandler"), &SkeletonRemapRule::m_boneMap, "Bone Map", "");
                }
            }
        } // Rule
    } // Pipeline
} // EMotionFX
