#include "BlendTreeRangeLimitedFABRIKNode.h"

#include "AnimGraphManager.h"
#include "EventManager.h"
#include "Node.h"
#include "RangeLimitedFABRIK.h"
#include "Recorder.h"
#include "TransformData.h"

#include <EMotionFX/Source/AnimGraph.h>
#include <EMotionFX/Source/DebugDraw.h>
#include <MCore/Source/LogManager.h>
#include <MCore/Source/Vector.h>

#include <AzCore/Math/Plane.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace EMotionFX
{
    AZ_CLASS_ALLOCATOR_IMPL(BlendTreeRangeLimitedFABRIKNode, AnimGraphAllocator)
    AZ_CLASS_ALLOCATOR_IMPL(BlendTreeRangeLimitedFABRIKNode::UniqueData, AnimGraphObjectUniqueDataAllocator)

    BlendTreeRangeLimitedFABRIKNode::UniqueData::UniqueData(AnimGraphNode* node, AnimGraphInstance* animGraphInstance)
        : AnimGraphNodeData(node, animGraphInstance)
    {
    }

    void BlendTreeRangeLimitedFABRIKNode::UniqueData::Update()
    {
        BlendTreeRangeLimitedFABRIKNode* ikNode = azdynamic_cast<BlendTreeRangeLimitedFABRIKNode*>(m_object);
        AZ_Assert(ikNode, "Unique data linked to incorrect node type.");

        const ActorInstance* actorInstance = m_animGraphInstance->GetActorInstance();
        const Actor* actor = actorInstance->GetActor();
        const Skeleton* skeleton = actor->GetSkeleton();

        // don't update the next time again
        m_nodeIndices.clear();
        SetHasError(true);

        // Find the joints
        const AZStd::vector<AZStd::string>& jointNames = ikNode->GetJointNames();
        for (const auto& jointName : jointNames)
        {
            const Node* joint = skeleton->FindNodeByName(jointName);
            if (!joint)
            {
                return;
            }
            m_nodeIndices.push_back(joint->GetNodeIndex());
        }

        const AZStd::string& jointToeName = ikNode->GetJointToeName();
        const Node* jointToe = skeleton->FindNodeByName(jointToeName);
        if (!jointToe)
        {
            return;
        }
        m_jointToeIndex = jointToe->GetNodeIndex();

        SetHasError(false);
    }

    BlendTreeRangeLimitedFABRIKNode::BlendTreeRangeLimitedFABRIKNode()
        : AnimGraphNode()
    {
        // setup the input ports
        InitInputPorts(3);
        SetupInputPort("Pose", INPUTPORT_POSE, AttributePose::TYPE_ID, INPUTPORT_POSE);
        SetupInputPortAsVector3("Goal Pos", INPUTPORT_GOALPOS, INPUTPORT_GOALPOS);
        SetupInputPortAsNumber("Weight", INPUTPORT_WEIGHT, INPUTPORT_WEIGHT);

        // setup the output ports
        InitOutputPorts(1);
        SetupOutputPortAsPose("Output Pose", OUTPUTPORT_POSE, OUTPUTPORT_POSE);
    }

    BlendTreeRangeLimitedFABRIKNode::~BlendTreeRangeLimitedFABRIKNode()
    {
    }

    bool BlendTreeRangeLimitedFABRIKNode::InitAfterLoading(AnimGraph* animGraph)
    {
        if (!AnimGraphNode::InitAfterLoading(animGraph))
        {
            return false;
        }

        InitInternalAttributesForAllInstances();

        Reinit();
        return true;
    }

    // get the palette name
    const char* BlendTreeRangeLimitedFABRIKNode::GetPaletteName() const
    {
        return "Range Limited FABRIK";
    }

    // get the category
    AnimGraphObject::ECategory BlendTreeRangeLimitedFABRIKNode::GetPaletteCategory() const
    {
        return AnimGraphObject::CATEGORY_CONTROLLERS;
    }

    // the main process method of the final node
    void BlendTreeRangeLimitedFABRIKNode::Output(AnimGraphInstance* animGraphInstance)
    {
        AnimGraphPose* outputPose;

        // make sure we have at least an input pose, otherwise output the bind pose
        if (GetInputPort(INPUTPORT_POSE).m_connection == nullptr)
        {
            RequestPoses(animGraphInstance);
            outputPose = GetOutputPose(animGraphInstance, OUTPUTPORT_POSE)->GetValue();
            ActorInstance* actorInstance = animGraphInstance->GetActorInstance();
            outputPose->InitFromBindPose(actorInstance);
            return;
        }

        // get the weight
        float weight = 1.0f;
        if (GetInputPort(INPUTPORT_WEIGHT).m_connection)
        {
            OutputIncomingNode(animGraphInstance, GetInputNode(INPUTPORT_WEIGHT));
            weight = GetInputNumberAsFloat(animGraphInstance, INPUTPORT_WEIGHT);
            weight = MCore::Clamp<float>(weight, 0.0f, 1.0f);
        }

        // if the IK weight is near zero, we can skip all calculations and act like a pass-trough node
        if (weight < MCore::Math::epsilon || m_disabled)
        {
            OutputIncomingNode(animGraphInstance, GetInputNode(INPUTPORT_POSE));
            const AnimGraphPose* inputPose = GetInputPose(animGraphInstance, INPUTPORT_POSE)->GetValue();
            RequestPoses(animGraphInstance);
            outputPose = GetOutputPose(animGraphInstance, OUTPUTPORT_POSE)->GetValue();
            *outputPose = *inputPose;
            return;
        }

        // get the input pose and copy it over to the output pose
        OutputIncomingNode(animGraphInstance, GetInputNode(INPUTPORT_POSE));
        const AnimGraphPose* inputPose = GetInputPose(animGraphInstance, INPUTPORT_POSE)->GetValue();
        RequestPoses(animGraphInstance);
        outputPose = GetOutputPose(animGraphInstance, OUTPUTPORT_POSE)->GetValue();
        *outputPose = *inputPose;

        //------------------------------------
        // get the node indices to work on
        //------------------------------------
        UniqueData* uniqueData = static_cast<UniqueData*>(FindOrCreateUniqueNodeData(animGraphInstance));
        if (uniqueData->GetHasError())
        {
            if (GetEMotionFX().GetIsInEditorMode())
            {
                SetHasError(uniqueData, true);
            }
            return;
        }

        // get the goal
        OutputIncomingNode(animGraphInstance, GetInputNode(INPUTPORT_GOALPOS));
        AZ::Vector3 goal = AZ::Vector3::CreateZero();
        TryGetInputVector3(animGraphInstance, INPUTPORT_GOALPOS, goal);

        // there is no error, as we have all we need to solve this
        if (GetEMotionFX().GetIsInEditorMode())
        {
            SetHasError(uniqueData, false);
        }

        //------------------------------------
        // perform the main calculation part
        //------------------------------------
        const Pose& inTransformPose = inputPose->GetPose();
        AZStd::vector<AZ::Vector3> inputPositions;
        inputPositions.reserve(uniqueData->m_nodeIndices.size());
        AZStd::vector<Transform> transforms;
        transforms.reserve(uniqueData->m_nodeIndices.size());
        for (const auto& nodeIndex : uniqueData->m_nodeIndices)
        {
            const Transform& transform = inTransformPose.GetModelSpaceTransform(nodeIndex);
            inputPositions.push_back(transform.m_position);
            transforms.push_back(transform);
        }

        // perform IK, try to find a solution by calculating the new middle node position
        AZStd::vector<AZ::Vector3> outputPositions;
        outputPositions.resize(inputPositions.size());
        AZStd::vector<IK::IKBoneConstraint*> constraints;
        constraints.resize(inputPositions.size());
        IK::RangeLimitedFABRIK::SolveRangeLimitedFABRIK(
            inputPositions,
            constraints,
            goal,
            outputPositions,
            m_maxRootDragDist,
            m_rootDragStiffness,
            m_precision,
            m_maxIterations
        );

        if (m_enableKneeCorrection && inputPositions.size() == 3 && uniqueData->m_jointToeIndex != InvalidIndex)
        {
            // Pre-IK positions
            AZ::Vector3 hipCSPre      = inputPositions[0];
            AZ::Vector3 kneeCSPre     = inputPositions[1];
            AZ::Vector3 footCSPre     = inputPositions[2];
            AZ::Vector3 toeCSPre      = inTransformPose.GetModelSpaceTransform(uniqueData->m_jointToeIndex).m_position;

            // Post-IK positions
            AZ::Vector3 hipCSPost       = outputPositions[0];
            AZ::Vector3 kneeCSPost      = outputPositions[1];
            AZ::Vector3 footCSPost      = outputPositions[2];
            AZ::Vector3 toeCSPost       = toeCSPre - footCSPre + footCSPost;

            // Thigh and shin before correction
            AZ::Vector3 oldThighVec = (kneeCSPost - hipCSPost).GetNormalized();
            AZ::Vector3 oldShinVec  = (footCSPost - kneeCSPost).GetNormalized();
	
            // If the leg is fully extended or fully folded, early out (correction is never needed)
            if (AZ::IsClose(AZ::Abs(oldThighVec.Dot(oldShinVec)), 1.0f))
            {
                return;
            }
        
            // To correct the knee:
            // - Project everything onto the plane normal to the vector from thigh to foot
            // - Find the angle, in the base pose, between the direction of the foot and the direction of the knee. If the leg is fully extended, assume this angle is 0.	
            // - Rotate the IKed knee angle so that it maintains the same angle with the IKed foot	

            // Project everything onto the plane defined by the axis between the hip and the foot. The knee can be rotated
            // around this axis without changing the position of the effector (the foot).	
	
            // Define each plane	
            AZ::Vector3 hipFootAxisPre    = footCSPre - hipCSPre;
            hipFootAxisPre.NormalizeSafe();
            if (hipFootAxisPre.IsZero())
            {
#if ENABLE_IK_DEBUG
                AZ_Warning("LogRTIK", false, "Knee Correction - HipFootAxisPre Normalization Failure");
#endif // ENABLE_IK_DEBUG_VERBOSE
                hipFootAxisPre        = AZ::Vector3(0.0f, 0.0f, 1.0f);
            }
            AZ::Vector3 centerPre         = hipCSPre + (kneeCSPre - hipCSPre).GetProjectedOnNormal(hipFootAxisPre);
            AZ::Vector3 kneeDirectionPre  = (kneeCSPre - centerPre).GetNormalized();

            AZ::Vector3 hipFootAxisPost   = footCSPost - hipCSPost;
            hipFootAxisPost.NormalizeSafe();
            if (hipFootAxisPost.IsZero())
            {
#if ENABLE_IK_DEBUG
                AZ_Warning("LogRTIK", false, "Knee Correction - HipFootAxisPost Normalization Failure");
#endif // ENABLE_IK_DEBUG_VERBOSE
                hipFootAxisPost       = AZ::Vector3(0.0f, 0.0f, 1.0f);
            }
            AZ::Vector3 centerPost        = hipCSPost + (kneeCSPost - hipCSPost).GetProjectedOnNormal(hipFootAxisPost);
            AZ::Vector3 kneeDirectionPost = (kneeCSPost - centerPost).GetNormalized();
	
            // Get the projected foot-toe vectors
            AZ::Vector3 footToePre = AZ::Plane::CreateFromNormalAndDistance(hipFootAxisPre, 0.0f).GetProjected(toeCSPre - footCSPre);
            footToePre.NormalizeSafe();
            if (footToePre.IsZero())
            {
#if ENABLE_IK_DEBUG
                AZ_Warning("LogRTIK", false, "Knee Correction - FootToePre Normalization Failure");
#endif // ENABLE_IK_DEBUG_VERBOSE
                footToePre = kneeDirectionPre;
            }

            // Rotate the foot according to how the hip-foot axis is changed. Without this, the foot direction
            // may be reversed when projected onto the rotation plane	
            float hipAxisRad = AZ::Acos(hipFootAxisPre.Dot(hipFootAxisPost));
            AZ::Vector3 footToeRotationAxis = hipFootAxisPre.Cross(hipFootAxisPost);
            AZ::Vector3 footCSPostRotated = footCSPost;
            AZ::Vector3 toeCSPostRotated = toeCSPost;
            footToeRotationAxis.Normalize();
            if (!footToeRotationAxis.IsZero())
            {
                AZ::Quaternion footToeRotation = AZ::Quaternion::CreateFromAxisAngle(footToeRotationAxis, hipAxisRad);
                AZ::Vector3 footDirection = footCSPost - hipCSPost;
                AZ::Vector3 toeDirection = toeCSPost - hipCSPost;
                footCSPostRotated = hipCSPost + footToeRotation.TransformVector(footDirection);
                toeCSPostRotated = hipCSPost + footToeRotation.TransformVector(toeDirection);
            }
   	
            AZ::Vector3 footToePost = AZ::Plane::CreateFromNormalAndDistance(hipFootAxisPost, 0.0f).GetProjected(toeCSPostRotated - footCSPostRotated);
            footToePost.NormalizeSafe();
            if (footToePost.IsZero())
            {
#if ENABLE_IK_DEBUG_VERBOSE
                AZ_Warning("LogRTIK", false, "Knee Correction - FootToePost Normalization Failure");
#endif // ENABLE_IK_DEBUG_VERBOSE
                footToePost = kneeDirectionPost;
            }

            // No need to failsafe -- we've already checked that the leg isn't completely straight
            AZ::Vector3 kneePre = (kneeCSPre - centerPre).GetNormalized();
	
            // Rotate the post-IK foot to find the corrected knee direction (on the hip-foot plane)
            float footKneeRad  = AZ::Acos(footToePre.Dot(kneePre));
            AZ::Vector3 rotationAxis = footToePre.Cross(kneePre);

            rotationAxis.NormalizeSafe();
            if (rotationAxis.IsZero())
            {		

#if ENABLE_IK_DEBUG_VERBOSE
                AZ_Warning("LogRTIK", false, "Knee correction -- rotation Axis normalization failure ");
#endif

                if (footToePre.Dot(kneePre) < 0.0f)
                {
                    // Knee and foot point in opposite directions
                    rotationAxis  = hipFootAxisPost;
                    footKneeRad = AZ::Constants::Pi;
                }
                else
                {
                    // Foot any knee point in same direction (no rotation needed)
                    rotationAxis  = hipFootAxisPost;
                    footKneeRad = 0.0f;
                }
            }

            AZ::Quaternion footKneeRotPost = AZ::Quaternion::CreateFromAxisAngle(rotationAxis, footKneeRad);
            AZ::Vector3 newKneeDirection = footKneeRotPost.TransformVector(footToePost);
            
            // Transform back to component space
            AZ::Vector3 newKneeCS = centerPost + (newKneeDirection * (kneeCSPost - centerPost).GetLength());

            outputPositions[1] = newKneeCS;
        }

        // Update the rotations to match the new positions
        for (int i = 0; i < transforms.size() - 1; i++)
        {
            AZ::Vector3 oldDir = (inputPositions[i + 1] - inputPositions[i]).GetNormalized();
            AZ::Vector3 newDir = (outputPositions[i + 1] - outputPositions[i]).GetNormalized();

            AZ::Vector3 rotationAxis = oldDir.Cross(newDir).GetNormalizedSafe();
            float rotationAngle = AZ::Acos(oldDir.Dot(newDir));
            AZ::Quaternion deltaRotation = AZ::Quaternion::CreateFromAxisAngle(rotationAxis, rotationAngle);

            transforms[i].m_rotation = (deltaRotation * transforms[i].m_rotation).GetNormalized(); 
            transforms[i].m_position = outputPositions[i];
        }
        // set last position
        transforms[transforms.size() - 1].m_position = outputPositions[transforms.size() - 1];

        Pose& outTransformPose = outputPose->GetPose();

        for (int i = 0; i < transforms.size(); i++)
        {
            outTransformPose.SetModelSpaceTransform(uniqueData->m_nodeIndices[i], transforms[i]);
        }

        // only blend when needed
        if (weight < 0.999f)
        {
            // get the original input pose
            const Pose& inputTransformPose = inputPose->GetPose();

            for (const auto& nodeIndex : uniqueData->m_nodeIndices)
            {
                // get the original input transforms
                Transform finalTransform = inputTransformPose.GetLocalSpaceTransform(nodeIndex);

                // blend them into the new transforms after IK
                finalTransform.Blend(outTransformPose.GetLocalSpaceTransform(nodeIndex), weight);

                // copy them to the output transforms
                outTransformPose.SetLocalSpaceTransform(nodeIndex, finalTransform);
            }
        }
    }

    void BlendTreeRangeLimitedFABRIKNode::Reflect(AZ::ReflectContext* context)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        if (!serializeContext)
        {
            return;
        }

        serializeContext->Class<BlendTreeRangeLimitedFABRIKNode, AnimGraphNode>()
            ->Field("JointNames", &BlendTreeRangeLimitedFABRIKNode::m_jointNames)
            ->Field("MaxRootDragDist", &BlendTreeRangeLimitedFABRIKNode::m_maxRootDragDist)
            ->Field("RootDragStiffness", &BlendTreeRangeLimitedFABRIKNode::m_rootDragStiffness)
            ->Field("Precision", &BlendTreeRangeLimitedFABRIKNode::m_precision)
            ->Field("MaxIterations", &BlendTreeRangeLimitedFABRIKNode::m_maxIterations)
            ->Field("EnableKneeCorrection", &BlendTreeRangeLimitedFABRIKNode::m_enableKneeCorrection)
            ->Field("JointToeName", &BlendTreeRangeLimitedFABRIKNode::m_jointToeName)
            ->Version(1);

        AZ::EditContext* editContext = serializeContext->GetEditContext();
        if (!editContext)
        {
            return;
        }

        editContext->Class<BlendTreeRangeLimitedFABRIKNode>("Range Limited FABRIK", "Range Limited FABRIK attributes")
            ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
            ->Attribute(AZ::Edit::Attributes::AutoExpand, "")
            ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::ShowChildrenOnly)
            ->DataElement(AZ_CRC_CE("ActorNodes"), &BlendTreeRangeLimitedFABRIKNode::m_jointNames, "Joint Names", "The joint names in the chain.")
            ->Attribute(AZ::Edit::Attributes::ChangeNotify, &BlendTreeRangeLimitedFABRIKNode::Reinit)
            ->Attribute(AZ::Edit::Attributes::ChangeNotify, AZ::Edit::PropertyRefreshLevels::EntireTree)
            ->DataElement(AZ::Edit::UIHandlers::Default, &BlendTreeRangeLimitedFABRIKNode::m_maxRootDragDist, "Max Root Drag Dist", "")
            ->DataElement(AZ::Edit::UIHandlers::Default, &BlendTreeRangeLimitedFABRIKNode::m_rootDragStiffness, "Root Drag Stiffness", "")
            ->DataElement(AZ::Edit::UIHandlers::Default, &BlendTreeRangeLimitedFABRIKNode::m_precision, "Precision", "")
            ->DataElement(AZ::Edit::UIHandlers::Default, &BlendTreeRangeLimitedFABRIKNode::m_maxIterations, "Max Iteration", "")
            ->DataElement(AZ::Edit::UIHandlers::Default, &BlendTreeRangeLimitedFABRIKNode::m_enableKneeCorrection, "Knee Correction", "Apply knee correction. Requires 3 joints selected (thigh, shin and foot) and toe joint specified.")
            ->DataElement(AZ::Edit::UIHandlers::Default, &BlendTreeRangeLimitedFABRIKNode::m_jointToeName, "Toe Joint", "Toe joint for knee correction, optional otherwise.");
    }
} // namespace EMotionFX
