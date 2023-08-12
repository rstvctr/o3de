/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <EMotionFX/Source/EMotionFXConfig.h>
#include <EMotionFX/Source/AnimGraphNode.h>


namespace EMotionFX
{
    /**
     *
     */
    class EMFX_API BlendTreeVector3ConstantNode
        : public AnimGraphNode
    {
    public:
        AZ_RTTI(BlendTreeVector3ConstantNode, "{637E43A9-4599-4A76-B9EB-D593BFF56026}", AnimGraphNode)
        AZ_CLASS_ALLOCATOR_DECL

        enum
        {
            OUTPUTPORT_RESULT = 0
        };

        enum
        {
            PORTID_OUTPUT_RESULT = 0
        };

        BlendTreeVector3ConstantNode();
        ~BlendTreeVector3ConstantNode();

        bool InitAfterLoading(AnimGraph* animGraph) override;
        void Reinit() override;

        AZ::Color GetVisualColor() const override;
        bool GetSupportsDisable() const override;

        const char* GetPaletteName() const override;
        AnimGraphObject::ECategory GetPaletteCategory() const override;

        void SetValue(AZ::Vector3 value);
        static void Reflect(AZ::ReflectContext* context);

    private:
        void Update(AnimGraphInstance* animGraphInstance, float timePassedInSeconds) override;

        AZ::Vector3 m_value;
    };

}   // namespace EMotionFX
