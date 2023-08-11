#pragma once

// include the required headers
#include "EMotionFXConfig.h"
#include "AnimGraphNode.h"


namespace EMotionFX
{
    /**
     *
     */
    class EMFX_API BlendTreeRangeLimitedFABRIKNode
        : public AnimGraphNode
    {
    public:
        AZ_RTTI(BlendTreeRangeLimitedFABRIKNode, "{CEB1CC2B-48A5-48CF-87B3-9231F992C555}", AnimGraphNode)
        AZ_CLASS_ALLOCATOR_DECL

        enum : uint16
        {
            INPUTPORT_POSE      = 0,
            INPUTPORT_GOALPOS   = 1,
            INPUTPORT_WEIGHT    = 2,
            OUTPUTPORT_POSE     = 0
        };

        class EMFX_API UniqueData
            : public AnimGraphNodeData
        {
            EMFX_ANIMGRAPHOBJECTDATA_IMPLEMENT_LOADSAVE
        public:
            AZ_CLASS_ALLOCATOR_DECL

            UniqueData(AnimGraphNode* node, AnimGraphInstance* animGraphInstance);
            ~UniqueData() = default;

            void Update() override;

        public:
            AZStd::vector<size_t> m_nodeIndices;
        };

        BlendTreeRangeLimitedFABRIKNode();
        ~BlendTreeRangeLimitedFABRIKNode();

        bool InitAfterLoading(AnimGraph* animGraph) override;

        AnimGraphObjectData* CreateUniqueData(AnimGraphInstance* animGraphInstance) override { return aznew UniqueData(this, animGraphInstance); }
        bool GetSupportsVisualization() const override          { return true; }
        bool GetHasOutputPose() const override                  { return true; }
        bool GetSupportsDisable() const override                { return true; }
        AZ::Color GetVisualColor() const override               { return AZ::Color(1.0f, 0.0f, 0.0f, 1.0f); }
        AnimGraphPose* GetMainOutputPose(AnimGraphInstance* animGraphInstance) const override     { return GetOutputPose(animGraphInstance, OUTPUTPORT_POSE)->GetValue(); }

        const char* GetPaletteName() const override;
        AnimGraphObject::ECategory GetPaletteCategory() const override;

        const AZStd::vector<AZStd::string>& GetJointNames() const { return m_jointNames; }

        static void Reflect(AZ::ReflectContext* context);

    private:
        void Output(AnimGraphInstance* animGraphInstance) override;

        AZStd::vector<AZStd::string>    m_jointNames;
        float                           m_maxRootDragDist = 0.0f;
        float                           m_rootDragStiffness = 1.0f;
        float                           m_precision = 0.001f;
        int32_t                         m_maxIterations = 10;
        bool                            m_enableKneeCorrection = true;
    };
} // namespace EMotionFX
