// Copyright (c) Henry Cooney 2017
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.
//
// Adapted to O3DE by Will Hahne
//

#include "RangeLimitedFABRIK.h"

#include "IK.h"

namespace EMotionFX::IK
{

bool RangeLimitedFABRIK::SolveRangeLimitedFABRIK(
    const AZStd::vector<Transform>& inTransforms,
    const AZStd::vector<IKBoneConstraint*>& constraints,
    const AZ::Vector3& effectorTargetLocation,
    AZStd::vector<Transform>& outTransforms,
    float maxRootDragDistance,
    float rootDragStiffness,
    float precision,
    int32_t maxIterations)
{
    outTransforms.clear();

    // Number of points in the chain. Number of bones = NumPoints - 1
    size_t numPoints = inTransforms.size();

    // Gather bone transforms
    outTransforms.reserve(numPoints);
    for (const Transform& transform : inTransforms)
    {
        outTransforms.push_back(transform);
    }

    if (numPoints < 2)
    {
        // Need at least one bone to do IK!
        return false;
    }

    // Gather bone lengths. BoneLengths contains the length of the bone ENDING at this point,
    // i.e., BoneLengths[i] contains the distance between point i-1 and point i
    AZStd::vector<float> boneLengths;
    ComputeBoneLengths(inTransforms, boneLengths);

    bool boneLocationUpdated = false;
    size_t effectorIndex = numPoints - 1;

    // Check distance between tip location and effector location
    float slop = outTransforms[effectorIndex].m_position.GetDistance(effectorTargetLocation);
    if (slop > precision)
    {
        // Set tip bone at end effector location.
        outTransforms[effectorIndex].m_position = effectorTargetLocation;

        int32_t iterationCount = 0;
        while ((slop > precision) && (iterationCount++ < maxIterations))
        {
            // "Forward Reaching" stage - adjust bones from end effector.
            FABRIKForwardPass(inTransforms, constraints, boneLengths, outTransforms);

            // Drag the root if enabled
            DragPointTethered(inTransforms[0], outTransforms[1], boneLengths[1], maxRootDragDistance, rootDragStiffness, outTransforms[0]);

            // "Backward Reaching" stage - adjust bones from root.
            FABRIKBackwardPass(inTransforms, constraints, boneLengths, outTransforms);

            slop =
                AZ::Abs(boneLengths[effectorIndex] - outTransforms[effectorIndex - 1].m_position.GetDistance(effectorTargetLocation));
        }

        // Place effector based on how close we got to the target
        AZ::Vector3 effectorLocation = outTransforms[effectorIndex].m_position;
        AZ::Vector3 effectorParentLocation = outTransforms[effectorIndex - 1].m_position;
        effectorLocation =
            effectorParentLocation + (effectorLocation - effectorParentLocation).GetNormalized() * boneLengths[effectorIndex];
        outTransforms[effectorIndex].m_position = effectorLocation;

        boneLocationUpdated = true;
    }

    // Update bone rotations
    if (boneLocationUpdated)
    {
        for (size_t pointIndex = 0; pointIndex < numPoints - 1; ++pointIndex)
        {
            if (!AZ::IsClose(boneLengths[pointIndex + 1], 0))
            {
                UpdateParentRotation(
                    outTransforms[pointIndex], inTransforms[pointIndex], outTransforms[pointIndex + 1], inTransforms[pointIndex + 1]);
            }
        }
    }

    return boneLocationUpdated;
}

bool RangeLimitedFABRIK::SolveClosedLoopFABRIK(
    const AZStd::vector<Transform>& inTransforms,
    const AZStd::vector<IKBoneConstraint*>& constraints,
    const AZ::Vector3& effectorTargetLocation,
    AZStd::vector<Transform>& outTransforms,
    float maxRootDragDistance,
    float rootDragStiffness,
    float precision,
    int32_t maxIterations)
{
    outTransforms.clear();

    // Number of points in the chain. Number of bones = NumPoints - 1
    size_t numPoints = inTransforms.size();
    size_t effectorIndex = numPoints - 1;

    // Gather bone transforms
    outTransforms.reserve(numPoints);
    for (const Transform& transform : inTransforms)
    {
        outTransforms.push_back(transform);
    }

    if (numPoints < 2)
    {
        // Need at least one bone to do IK!
        return false;
    }
    // Gather bone lengths. BoneLengths contains the length of the bone ENDING at this point,

    // i.e., BoneLengths[i] contains the distance between point i-1 and point i
    AZStd::vector<float> boneLengths;
    ComputeBoneLengths(inTransforms, boneLengths);
    float rootToEffectorLength = inTransforms[0].m_position.GetDistance(inTransforms[effectorIndex].m_position);

    bool boneLocationUpdated = false;

    // Check distance between tip location and effector location
    float slop = outTransforms[effectorIndex].m_position.GetDistance(effectorTargetLocation);
    if (slop > precision)
    {
        // The closed loop method is identical, except the root is dragged a second time to maintain
        // distance with the effector.

        // Set tip bone at end effector location.
        outTransforms[effectorIndex].m_position = effectorTargetLocation;

        int32_t iterationCount = 0;
        while ((slop > precision) && (iterationCount++ < maxIterations))
        {
            // "Forward Reaching" stage - adjust bones from end effector.
            FABRIKForwardPass(inTransforms, constraints, boneLengths, outTransforms);

            // Drag the root if enabled
            DragPointTethered(inTransforms[0], outTransforms[1], boneLengths[1], maxRootDragDistance, rootDragStiffness, outTransforms[0]);

            // Drag the root again, toward the effector (since they're connected in a closed loop)
            DragPointTethered(
                inTransforms[0],
                outTransforms[effectorIndex],
                rootToEffectorLength,
                maxRootDragDistance,
                rootDragStiffness,
                outTransforms[0]);

            // "Backward Reaching" stage - adjust bones from root.
            FABRIKBackwardPass(inTransforms, constraints, boneLengths, outTransforms);

            slop = outTransforms[effectorIndex].m_position.GetDistance(effectorTargetLocation);
        }

        boneLocationUpdated = true;
    }

    // Update bone rotations
    if (boneLocationUpdated)
    {
        for (size_t pointIndex = 0; pointIndex < numPoints - 1; ++pointIndex)
        {
            if (!AZ::IsClose(boneLengths[pointIndex + 1], 0))
            {
                UpdateParentRotation(
                    outTransforms[pointIndex], inTransforms[pointIndex], outTransforms[pointIndex + 1], inTransforms[pointIndex + 1]);
            }
        }

        // Update the last bone's rotation. Unlike normal fabrik, it's assumed to point toward the root bone,
        // so it's rotation must be updated
        if (!AZ::IsClose(rootToEffectorLength, 0))
        {
            UpdateParentRotation(outTransforms[effectorIndex], inTransforms[effectorIndex], outTransforms[0], inTransforms[0]);
        }
    }

    return boneLocationUpdated;
};

bool RangeLimitedFABRIK::SolveNoisyThreePoint(
    const NoisyThreePointClosedLoop& inClosedLoop,
    const Transform& effectorAReference,
    const Transform& effectorBReference,
    NoisyThreePointClosedLoop& outClosedLoop,
    float maxRootDragDistance,
    float rootDragStiffness,
    float precision,
    int32_t maxIterations)
{
    // Temporary transforms for each point
    Transform a = inClosedLoop.m_effectorATransform;
    Transform b = inClosedLoop.m_effectorBTransform;
    Transform root = inClosedLoop.m_rootTransform;

    // Compute bone lengths
    float distAToRoot = inClosedLoop.m_targetRootADistance;
    float distBToRoot = inClosedLoop.m_targetRootBDistance;
    float distAToB = inClosedLoop.m_targetABDistance;
    float distARef = a.m_position.GetDistance(effectorAReference.m_position);
    float distBRef = b.m_position.GetDistance(effectorBReference.m_position);

    // Now start the noisy solver method. The idea here is that A, B, and Root are out of whack;
    // move them so inter-joint distances are satisfied again. Keep doing this until things settle down.

    // See www.andreasaristidou.com/publications/papers/Extending_FABRIK_with_Model_Cοnstraints.pdf Figure 9 for
    // description of each phase. Hopefully I'm implementing this right; unfortunatley the paper is vague

    AZ::Vector3 lastA = a.m_position;
    AZ::Vector3 lastB = b.m_position;

    int32_t iterationCount = 0;

    // Phase 1 (Fig. 9 b): go around the loop
    DragPoint(root, distAToRoot, a);
    DragPoint(a, distAToB, b);
    DragPointTethered(inClosedLoop.m_rootTransform, b, distBToRoot, maxRootDragDistance, rootDragStiffness, root);
    DragPoint(root, distAToRoot, a);

    // Phase 2 (Fig. 9 c): Reset root and go other way
    root.m_position = inClosedLoop.m_rootTransform.m_position;
    DragPoint(root, distBToRoot, b);
    DragPoint(b, distAToB, a);

    // Phase 3 (Fig. 9 d): Drag both effectors such that their distances to reference points (outside the closed loop)
    // and distances from root are maintained
    DragPoint(root, distAToRoot, a);
    DragPoint(effectorAReference, distARef, a);
    DragPoint(root, distBToRoot, b);
    DragPoint(effectorBReference, distBRef, b);

    // Phase 4 (Fig. 9 b): Same as phase 1
    DragPoint(root, distAToRoot, a);
    DragPoint(a, distAToB, b);
    DragPointTethered(inClosedLoop.m_rootTransform, b, distBToRoot, maxRootDragDistance, rootDragStiffness, root);
    DragPoint(root, distAToRoot, a);

    // Phase 5 (Fig. 9 c): Same as phase 2, but don't reset root
    DragPoint(root, distBToRoot, b);
    DragPoint(b, distAToB, a);

    float precisionSq = precision * precision;
    float delta = AZStd::max(a.m_position.GetDistanceSq(lastA), b.m_position.GetDistance(lastB));
    lastA = a.m_position;
    lastB = b.m_position;

    while ((delta > precisionSq) && (iterationCount++ < maxIterations))
    {
        // Iterate phases 3-5 only
        // Phase 3
        DragPoint(root, distAToRoot, a);
        DragPoint(effectorAReference, distARef, a);
        DragPoint(root, distBToRoot, b);
        DragPoint(effectorBReference, distBRef, b);

        // Phase 4
        DragPoint(root, distAToRoot, a);
        DragPoint(a, distAToB, b);
        DragPointTethered(inClosedLoop.m_rootTransform, b, distBToRoot, maxRootDragDistance, rootDragStiffness, root);
        DragPoint(root, distAToRoot, a);

        // Phase 5
        DragPoint(root, distBToRoot, b);
        DragPoint(b, distAToB, a);

        delta = AZStd::max(a.m_position.GetDistanceSq(lastA), b.m_position.GetDistance(lastB));
        lastA = a.m_position;
        lastB = b.m_position;
    }

    // Update rotations
    if (!AZ::IsClose(distAToRoot, 0))
    {
        UpdateParentRotation(root, inClosedLoop.m_rootTransform, a, inClosedLoop.m_effectorATransform);
    }

    if (!AZ::IsClose(distAToB, 0))
    {
        UpdateParentRotation(a, inClosedLoop.m_effectorATransform, b, inClosedLoop.m_effectorBTransform);
    }

    if (!AZ::IsClose(distBToRoot, 0))
    {
        UpdateParentRotation(b, inClosedLoop.m_effectorBTransform, root, inClosedLoop.m_rootTransform);
    }

    // Copy transforms to output
    outClosedLoop.m_effectorATransform = a;
    outClosedLoop.m_effectorBTransform = b;
    outClosedLoop.m_rootTransform = root;

    return true;
}

void RangeLimitedFABRIK::FABRIKForwardPass(
    const AZStd::vector<Transform>& inTransforms,
    const AZStd::vector<IKBoneConstraint*>& constraints,
    const AZStd::vector<float>& boneLengths,
    AZStd::vector<Transform>& outTransforms)
{
    size_t numPoints = inTransforms.size();
    size_t effectorIndex = numPoints - 1;

    for (size_t pointIndex = effectorIndex - 1; pointIndex > 0; --pointIndex)
    {
        Transform& currentPoint = outTransforms[pointIndex];
        Transform& childPoint = outTransforms[pointIndex + 1];

        // Move the parent to maintain starting bone lengths
        DragPoint(childPoint, boneLengths[pointIndex + 1], currentPoint);

        // Enforce parent's constraint any time child is moved
        IKBoneConstraint* currentConstraint = constraints[pointIndex - 1];
        if (currentConstraint != nullptr && currentConstraint->m_enabled)
        {
            currentConstraint->SetupFn(pointIndex - 1, inTransforms, constraints, outTransforms);

            currentConstraint->EnforceConstraint(pointIndex - 1, inTransforms, constraints, outTransforms);
        }
    }
}

void RangeLimitedFABRIK::FABRIKBackwardPass(
    const AZStd::vector<Transform>& inTransforms,
    const AZStd::vector<IKBoneConstraint*>& constraints,
    const AZStd::vector<float>& boneLengths,
    AZStd::vector<Transform>& outTransforms)
{
    size_t numPoints = inTransforms.size();
    size_t effectorIndex = numPoints - 1;

    for (int32_t pointIndex = 1; pointIndex < effectorIndex; pointIndex++)
    {
        Transform& parentPoint = outTransforms[pointIndex - 1];
        Transform& currentPoint = outTransforms[pointIndex];

        // Move the child to maintain starting bone lengths
        DragPoint(parentPoint, boneLengths[pointIndex], currentPoint);

        // Enforce parent's constraint any time child is moved
        IKBoneConstraint* currentConstraint = constraints[pointIndex - 1];
        if (currentConstraint != nullptr && currentConstraint->m_enabled)
        {
            currentConstraint->SetupFn(pointIndex - 1, inTransforms, constraints, outTransforms);

            currentConstraint->EnforceConstraint(pointIndex - 1, inTransforms, constraints, outTransforms);
        }
    }
}

void RangeLimitedFABRIK::DragPoint(const Transform& maintainDistancePoint, float boneLength, Transform& pointToMove)
{
    pointToMove.m_position =
        maintainDistancePoint.m_position +
        (pointToMove.m_position - maintainDistancePoint.m_position).GetNormalized() * boneLength;
}

void RangeLimitedFABRIK::DragPointTethered(
    const Transform& startingTransform,
    const Transform& maintainDistancePoint,
    float boneLength,
    float maxDragDistance,
    float dragStiffness,
    Transform& pointToDrag)
{
    if (maxDragDistance < AZ::Constants::Tolerance || dragStiffness < AZ::Constants::Tolerance)
    {
        pointToDrag = startingTransform;
        return;
    }

    AZ::Vector3 target;
    if (AZ::IsClose(boneLength, 0))
    {
        target = maintainDistancePoint.m_position;
    }
    else
    {
        target = maintainDistancePoint.m_position +
            (pointToDrag.m_position - maintainDistancePoint.m_position).GetNormalized() * boneLength;
    }

    AZ::Vector3 displacement = target - startingTransform.m_position;

    // Root drag stiffness 'pulls' the root back (set to 1.0 to disable)
    displacement /= dragStiffness;

    // limit root displacement to drag length
    AZ::Vector3 limitedDisplacement = displacement;
    limitedDisplacement.SetLength(AZStd::min(limitedDisplacement.GetLength(), maxDragDistance));
    pointToDrag.m_position = startingTransform.m_position + limitedDisplacement;
}

void RangeLimitedFABRIK::UpdateParentRotation(
    Transform& newParentTransform,
    const Transform& oldParentTransform,
    const Transform& newChildTransform,
    const Transform& oldChildTransform)
{
    AZ::Vector3 oldDir = (oldChildTransform.m_position - oldParentTransform.m_position).GetNormalized();
    AZ::Vector3 newDir = (newChildTransform.m_position - newParentTransform.m_position).GetNormalized();

    AZ::Vector3 rotationAxis = oldDir.Cross(newDir).GetNormalizedSafe();
    float rotationAngle = AZ::Acos(oldDir.Dot(newDir));
    AZ::Quaternion deltaRotation(rotationAxis, rotationAngle);

    newParentTransform.m_rotation = (deltaRotation * oldParentTransform.m_rotation).GetNormalized();
}

void RangeLimitedFABRIK::ComputeBoneLengths(const AZStd::vector<Transform>& inTransforms, AZStd::vector<float>& outBoneLengths)
{
    size_t numPoints = inTransforms.size();
    outBoneLengths.clear();
    outBoneLengths.reserve(numPoints);

    // Root always has zero length
    outBoneLengths.push_back(0.0f);

    for (size_t i = 1; i < numPoints; ++i)
    {
        outBoneLengths.push_back(inTransforms[i - 1].m_position.GetDistance(inTransforms[i].m_position));
    }
}

} // namespace VRGame::IK
