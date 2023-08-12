#include "BlendTreeRotateVectorNode.h"

#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <MCore/Source/FastMath.h>
#include <MCore/Source/Compare.h>
#include <MCore/Source/AzCoreConversions.h>

namespace EMotionFX
{
    AZ_CLASS_ALLOCATOR_IMPL(BlendTreeRotateVectorNode, AnimGraphAllocator)

    BlendTreeRotateVectorNode::BlendTreeRotateVectorNode()
        : AnimGraphNode()
    {
        // Setup the input ports
        InitInputPorts(2);
        SetupInputPort("Vec", INPUTPORT_VEC, MCore::AttributeVector3::TYPE_ID, INPUTPORT_VEC);
        SetupInputPort("Rot", INPUTPORT_ROT, MCore::AttributeQuaternion::TYPE_ID, INPUTPORT_ROT);

        // Setup the output ports
        InitOutputPorts(1);
        SetupOutputPort("Vec", OUTPUTPORT_VEC, MCore::AttributeVector3::TYPE_ID, OUTPUTPORT_VEC);

        if (m_animGraph)
        {
            Reinit();
        }
    }

    BlendTreeRotateVectorNode::~BlendTreeRotateVectorNode()
    {
    }

    void BlendTreeRotateVectorNode::Reinit()
    {
        AnimGraphNode::Reinit();
    } 

    bool BlendTreeRotateVectorNode::InitAfterLoading(AnimGraph* animGraph)
    {
        if (!AnimGraphNode::InitAfterLoading(animGraph))
        {
            return false;
        }

        InitInternalAttributesForAllInstances();

        Reinit();
        return true;
    }

    const char* BlendTreeRotateVectorNode::GetPaletteName() const
    {
        return "Rotate Vector";
    }

    AnimGraphObject::ECategory BlendTreeRotateVectorNode::GetPaletteCategory() const
    {
        return AnimGraphObject::CATEGORY_MATH;
    }

    void BlendTreeRotateVectorNode::Output(AnimGraphInstance* animGraphInstance)
    {
        AnimGraphNode::Output(animGraphInstance);
        // If there are no incoming connections, there is nothing to do
        if (m_connections.empty())
        {
            return;
        }

        AZ::Vector3 vec;
        TryGetInputVector3(animGraphInstance, INPUTPORT_VEC, vec);

        if (auto rotPort = GetInputQuaternion(animGraphInstance, INPUTPORT_ROT))
        {
            vec = rotPort->GetValue().TransformVector(vec);
        }

        // Update the output value
        GetOutputVector3(animGraphInstance, OUTPUTPORT_VEC)->SetValue(vec);
    }

    void BlendTreeRotateVectorNode::Reflect(AZ::ReflectContext* context)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        if (!serializeContext)
        {
            return;
        }

        serializeContext->Class<BlendTreeRotateVectorNode, AnimGraphNode>()
            ->Version(1)
            ;

        AZ::EditContext* editContext = serializeContext->GetEditContext();
        if (!editContext)
        {
            return;
        }

        editContext->Class<BlendTreeRotateVectorNode>("Rotate Vector", "Rotate Vector attributes")
            ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
            ->Attribute(AZ::Edit::Attributes::AutoExpand, "")
            ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::ShowChildrenOnly)
            ;
    }
} // namespace EMotionFX
