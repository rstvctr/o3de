#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/RTTI/ReflectContext.h>
#include <AzCore/Memory/SystemAllocator.h>
#include <SceneAPI/SceneCore/DataTypes/GraphData/IBoneData.h>
#include <SceneAPIExt/Rules/SkeletonRemapRule.h>

namespace EMotionFX
{
    AZ_CLASS_ALLOCATOR_IMPL(BoneMap, AZ::SystemAllocator)

    void BoneMap::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<BoneMap>()
                ->Version(1)
                ->Field("BoneMap", &BoneMap::m_boneMap);
        }
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