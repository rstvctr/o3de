#include "BlendTreeSwingTwistNode.h"

#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <MCore/Source/FastMath.h>
#include <MCore/Source/Compare.h>
#include <MCore/Source/AzCoreConversions.h>


namespace EMotionFX
{
    AZ_CLASS_ALLOCATOR_IMPL(BlendTreeSwingTwistNode, AnimGraphAllocator)

    BlendTreeSwingTwistNode::BlendTreeSwingTwistNode()
        : AnimGraphNode()
        , m_axisVector(AZ::Vector3::CreateAxisZ())
    {
        // Setup the input ports
        InitInputPorts(1);
        SetupInputPort("Rot", INPUTPORT_ROT, MCore::AttributeQuaternion::TYPE_ID, INPUTPORT_ROT);

        // Setup the output ports
        InitOutputPorts(2);
        SetupOutputPort("Swing", OUTPUTPORT_SWING, MCore::AttributeQuaternion::TYPE_ID, OUTPUTPORT_SWING);
        SetupOutputPort("Twist", OUTPUTPORT_TWIST, MCore::AttributeQuaternion::TYPE_ID, OUTPUTPORT_TWIST);

        if (m_animGraph)
        {
            Reinit();
        }
    }

    BlendTreeSwingTwistNode::~BlendTreeSwingTwistNode()
    {
    }

    void BlendTreeSwingTwistNode::Reinit()
    {
        m_axisVector.NormalizeSafe();
        if (m_axisVector.IsZero())
        {
            m_axisVector = AZ::Vector3::CreateAxisZ();
        }

        AnimGraphNode::Reinit();
    } 

    bool BlendTreeSwingTwistNode::InitAfterLoading(AnimGraph* animGraph)
    {
        if (!AnimGraphNode::InitAfterLoading(animGraph))
        {
            return false;
        }

        InitInternalAttributesForAllInstances();

        Reinit();
        return true;
    }

    const char* BlendTreeSwingTwistNode::GetPaletteName() const
    {
        return "Swing Twist Decompose";
    }

    AnimGraphObject::ECategory BlendTreeSwingTwistNode::GetPaletteCategory() const
    {
        return AnimGraphObject::CATEGORY_MATH;
    }

    void BlendTreeSwingTwistNode::Output(AnimGraphInstance* animGraphInstance)
    {
        AnimGraphNode::Output(animGraphInstance);
        // If there are no incoming connections, there is nothing to do
        if (m_connections.empty())
        {
            return;
        }

        // If both x and y inputs have connections
        AZ::Quaternion swing = AZ::Quaternion::CreateIdentity();
        AZ::Quaternion twist = AZ::Quaternion::CreateIdentity();
        if (m_connections.size() == 1)
        {
            AZ::Quaternion q = GetInputQuaternion(animGraphInstance, INPUTPORT_ROT)->GetValue();
            
            // Source: https://allenchou.net/2018/05/game-math-swing-twist-interpolation-sterp/
            AZ::Vector3 r(q.GetX(), q.GetY(), q.GetZ());
 
            // singularity: rotation by 180 degree
            if (r.GetLengthSq() < AZ::Constants::FloatEpsilon)
            {
                AZ::Vector3 rotatedTwistAxis = q.TransformVector(m_axisVector);
                AZ::Vector3 swingAxis = m_axisVector.Cross(rotatedTwistAxis);
 
                // more singularity: 
                // rotation axis parallel to twist axis
                if (swingAxis.GetLengthSq() > AZ::Constants::FloatEpsilon)
                {
                    float swingAngle = m_axisVector.Angle(rotatedTwistAxis);
                    swing = AZ::Quaternion::CreateFromAxisAngle(swingAxis, swingAngle);
                }
 
                // always twist 180 degree on singularity
                twist = AZ::Quaternion::CreateFromAxisAngle(m_axisVector, AZ::Constants::Pi);
            }
            else
            {
                // meat of swing-twist decomposition
                AZ::Vector3 p = r.GetProjected(m_axisVector);
                twist = AZ::Quaternion(p.GetX(), p.GetY(), p.GetZ(), q.GetW());
                twist.Normalize();
                swing = q * twist.GetInverseFull();
            }
        }

        // Update the output value
        GetOutputQuaternion(animGraphInstance, OUTPUTPORT_SWING)->SetValue(swing);
        GetOutputQuaternion(animGraphInstance, OUTPUTPORT_TWIST)->SetValue(twist);
    }

    void BlendTreeSwingTwistNode::Reflect(AZ::ReflectContext* context)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        if (!serializeContext)
        {
            return;
        }

        serializeContext->Class<BlendTreeSwingTwistNode, AnimGraphNode>()
            ->Version(1)
            ->Field("axisVector", &BlendTreeSwingTwistNode::m_axisVector)
            ;

        AZ::EditContext* editContext = serializeContext->GetEditContext();
        if (!editContext)
        {
            return;
        }

        editContext->Class<BlendTreeSwingTwistNode>("Swing Twist Decompose", "Swing Twist Decompose attributes")
            ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
            ->Attribute(AZ::Edit::Attributes::AutoExpand, "")
            ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::ShowChildrenOnly)
            ->DataElement(AZ::Edit::UIHandlers::Default, &BlendTreeSwingTwistNode::m_axisVector, "Axis Vector", "The axis to use when performing the swing-twist decomposition")
            ->Attribute(AZ::Edit::Attributes::ChangeNotify, &BlendTreeSwingTwistNode::Reinit)
            ;
    }
} // namespace EMotionFX
