#pragma once

#include <AzSceneDef.h>
#include <AzCore/Memory/Memory.h>
#include <SceneAPI/SceneCore/DataTypes/Rules/IRule.h>

namespace AZ
{
    class ReflectContext;
}

namespace EMotionFX
{
    class BoneMap
    {
    public:
        AZ_RTTI(BoneMap, "{9856FA0A-28AD-4268-A562-57FCA26122D6}")
        AZ_CLASS_ALLOCATOR_DECL

        BoneMap() = default;
        virtual ~BoneMap() = default;

        static void Reflect(AZ::ReflectContext* context);

        AZStd::unordered_map<AZStd::string, AZStd::string> m_boneMap;
    };

    namespace Pipeline
    {
        namespace Rule
        {
            class SkeletonRemapRule
                : public AZ::SceneAPI::DataTypes::IRule
            {
            public:
                AZ_RTTI(SkeletonRemapRule, "{71A76B1E-4C4B-4C82-A671-2AD4DD353A9E}", IRule);
                AZ_CLASS_ALLOCATOR_DECL

                SkeletonRemapRule() = default;
                ~SkeletonRemapRule() override = default;

                static void Reflect(AZ::ReflectContext* context);

            protected:
                BoneMap m_boneMap;
            };
        } // Rule
    } // Pipeline
} // EMotionFX
