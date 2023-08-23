#pragma once

#include <AzSceneDef.h>
#include <AzCore/Memory/Memory.h>
#include <AzCore/Serialization/SerializeContext.h>
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
        using MapType = AZStd::unordered_map<AZStd::string, AZStd::string>;

        class EventHandler : public AZ::SerializeContext::IEventHandler
        {
        public:
            void OnReadEnd(void* classPtr) override;
        };

        friend class EventHandler;

        AZ_RTTI(BoneMap, "{9856FA0A-28AD-4268-A562-57FCA26122D6}")
        AZ_CLASS_ALLOCATOR_DECL

        BoneMap() = default;
        virtual ~BoneMap() = default;

        static void Reflect(AZ::ReflectContext* context);

        void SetSkeletonBoneName(const AZStd::string& mappedBoneName, const AZStd::string& origBoneName);
        void Clear();
        void Remove(const AZStd::string& name);

        // Check if the map has a bone based on its "profile" name
        bool HasBone(const AZStd::string& name) const;

        // Get the name of the original bone (including full path) from the "profile" name
        const AZStd::string& GetOrigBone(const AZStd::string& name) const;

        // Iteration functions
        MapType::const_iterator begin() const
        {
            return m_boneMap.begin();
        }
        MapType::const_iterator end() const
        {
            return m_boneMap.end();
        }


    private:
        // Map from original name in the skeleton to the standardized "profile" name
        // Original name includes full path
        MapType m_boneMap;

        // Convenience map built to allow looking up in reverse
        MapType m_profileToOrigBoneMap;
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

                const BoneMap& GetBoneMap() const { return m_boneMap; }

            protected:
                BoneMap m_boneMap;
            };
        } // Rule
    } // Pipeline
} // EMotionFX
