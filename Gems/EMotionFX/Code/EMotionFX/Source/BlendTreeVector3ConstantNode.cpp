/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzFramework/StringFunc/StringFunc.h>
#include <EMotionFX/Source/BlendTreeVector3ConstantNode.h>


namespace EMotionFX
{
    AZ_CLASS_ALLOCATOR_IMPL(BlendTreeVector3ConstantNode, AnimGraphAllocator)

    BlendTreeVector3ConstantNode::BlendTreeVector3ConstantNode()
        : AnimGraphNode()
        , m_value()
    {
        InitOutputPorts(1);
        SetupOutputPort("Output", OUTPUTPORT_RESULT, MCore::AttributeVector3::TYPE_ID, PORTID_OUTPUT_RESULT);
    }


    BlendTreeVector3ConstantNode::~BlendTreeVector3ConstantNode()
    {
    }


    void BlendTreeVector3ConstantNode::Reinit()
    {
        AnimGraphNode::Reinit();
    }


    bool BlendTreeVector3ConstantNode::InitAfterLoading(AnimGraph* animGraph)
    {
        if (!AnimGraphNode::InitAfterLoading(animGraph))
        {
            return false;
        }

        InitInternalAttributesForAllInstances();

        Reinit();
        return true;
    }


    const char* BlendTreeVector3ConstantNode::GetPaletteName() const
    {
        return "Vector3 Constant";
    }


    AnimGraphObject::ECategory BlendTreeVector3ConstantNode::GetPaletteCategory() const
    {
        return AnimGraphObject::CATEGORY_SOURCES;
    }


    void BlendTreeVector3ConstantNode::Update(AnimGraphInstance* animGraphInstance, float timePassedInSeconds)
    {
        AZ_UNUSED(timePassedInSeconds);
        GetOutputVector3(animGraphInstance, OUTPUTPORT_RESULT)->SetValue(m_value);
    }


    AZ::Color BlendTreeVector3ConstantNode::GetVisualColor() const
    {
        return AZ::Color(0.5f, 1.0f, 1.0f, 1.0f);
    }


    bool BlendTreeVector3ConstantNode::GetSupportsDisable() const
    {
        return false;
    }

    void BlendTreeVector3ConstantNode::SetValue(AZ::Vector3 value)
    {
        m_value = value;
    }

    void BlendTreeVector3ConstantNode::Reflect(AZ::ReflectContext* context)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        if (!serializeContext)
        {
            return;
        }

        serializeContext->Class<BlendTreeVector3ConstantNode, AnimGraphNode>()
            ->Version(1)
            ->Field("value", &BlendTreeVector3ConstantNode::m_value)
            ;


        AZ::EditContext* editContext = serializeContext->GetEditContext();
        if (!editContext)
        {
            return;
        }

        editContext->Class<BlendTreeVector3ConstantNode>("Vector3 Constant", "Vector3 constant attributes")
            ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                ->Attribute(AZ::Edit::Attributes::AutoExpand, "")
                ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::ShowChildrenOnly)
            ->DataElement(AZ::Edit::UIHandlers::Default, &BlendTreeVector3ConstantNode::m_value, "Constant Value", "The value that the node will output.")
                ->Attribute(AZ::Edit::Attributes::ChangeNotify, &BlendTreeVector3ConstantNode::Reinit)
            ;
    }
} // namespace EMotionFX
