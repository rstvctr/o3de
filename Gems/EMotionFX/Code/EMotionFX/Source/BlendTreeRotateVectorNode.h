#pragma once

#include <AzCore/Math/Vector3.h>
#include <AzCore/Math/Quaternion.h>
#include "EMotionFXConfig.h"
#include "AnimGraphNode.h"

namespace EMotionFX
{
    class EMFX_API BlendTreeRotateVectorNode
        : public AnimGraphNode
    {
    public:
        AZ_RTTI(BlendTreeRotateVectorNode, "{234B67D8-3A8D-44EE-A4C8-30DAA318BC41}", AnimGraphNode)
        AZ_CLASS_ALLOCATOR_DECL

        enum
        {
            INPUTPORT_VEC = 0,
            INPUTPORT_ROT = 1,
            OUTPUTPORT_VEC = 0,
        };

        BlendTreeRotateVectorNode();
        ~BlendTreeRotateVectorNode();

        void Reinit() override;
        bool InitAfterLoading(AnimGraph* animGraph) override;

        AZ::Color GetVisualColor() const override { return AZ::Color(0.0f, 0.48f, 0.65f, 1.0f); }

        const char* GetPaletteName() const override;
        AnimGraphObject::ECategory GetPaletteCategory() const override;

        void SetDefaultValue(const AZ::Quaternion& value);

        static void Reflect(AZ::ReflectContext* context);

    private:
        void Output(AnimGraphInstance* animGraphInstance) override;
    };
}   // namespace EMotionFX
