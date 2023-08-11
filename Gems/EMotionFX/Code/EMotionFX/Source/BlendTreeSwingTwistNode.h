#pragma once

#include <AzCore/Math/Vector3.h>
#include <AzCore/Math/Quaternion.h>
#include "EMotionFXConfig.h"
#include "AnimGraphNode.h"

namespace EMotionFX
{
    class EMFX_API BlendTreeSwingTwistNode
        : public AnimGraphNode
    {
    public:
        AZ_RTTI(BlendTreeSwingTwistNode, "{90BB20F1-7651-4733-83A4-6C2FC2F8C4F3}", AnimGraphNode)
        AZ_CLASS_ALLOCATOR_DECL

        enum
        {
            INPUTPORT_ROT = 0,
            OUTPUTPORT_SWING = 0,
            OUTPUTPORT_TWIST = 1
        };

        BlendTreeSwingTwistNode();
        ~BlendTreeSwingTwistNode();

        void Reinit() override;
        bool InitAfterLoading(AnimGraph* animGraph) override;

        AZ::Color GetVisualColor() const override { return AZ::Color(0.0f, 0.48f, 0.65f, 1.0f); }

        const char* GetPaletteName() const override;
        AnimGraphObject::ECategory GetPaletteCategory() const override;

        void SetDefaultValue(const AZ::Quaternion& value);

        static void Reflect(AZ::ReflectContext* context);

    private:
        AZ::Vector3 m_axisVector;

        void Output(AnimGraphInstance* animGraphInstance) override;
    };
}   // namespace EMotionFX
