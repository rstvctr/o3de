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

/*
 * Contains basic IK structures and definitions.
 */

#pragma once

#include <AzCore/Math/Vector3.h>
#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/RTTI/RTTI.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/functional.h>
#include <AzCore/std/string/string.h>
#include "EMotionFXConfig.h"

#define ENABLE_IK_DEBUG 1
#define ENABLE_IK_DEBUG_VERBOSE 1

namespace EMotionFX
{

class Skeleton;

}

namespace EMotionFX::IK
{

/*
 * Specifies what IK should do if the target is unreachable
 */
enum class IKUnreachableRule : uint8_t
{
    // Abort IK, return to pre-IK pose
    IK_Abort,

    // Reach as far toward the target as possible without moving the root bone
    IK_Reach,

    // Drag the root bone toward the target so it can be reached (caution, this is likely to give weird results)
    IK_DragRoot
};

/*
 *	How the ROM constraint should behave.
 */
enum class IKROMConstraintMode : uint8_t
{
    // Constrain both pitch and yaw rotations
    IKROM_Pitch_And_Yaw,

    // Constrain pitch rotation; allow no yaw rotation
    IKROM_Pitch_Only,

    // Constrain yaw rotation; allow no pitch rotation
    IKROM_Yaw_Only,

    // Constrain yaw rotation; allow no pitch rotation
    // IKROM_Twist_Only,

    // Do not constrain rotation
    IKROM_No_Constraint
};

enum class IKBoneAxis : uint8_t
{
    IKBA_X,
    IKBA_Y,
    IKBA_Z,
    IKBA_XNeg,
    IKBA_YNeg,
    IKBA_ZNeg
};

// IK utility functions
struct IKUtil
{
public:

	// Convert an EIKBoneAxis to a unit vector.
	static AZ::Vector3 IKBoneAxisToVector(IKBoneAxis InBoneAxis);
};

/*
 * A range-of-motion constraint on a bone used in IK.
 *
 * ROM constraints have access to the entire bone chain, before and after IK, and
 * may modify and and all transforms in the chain.
 *
 * The base constraint type does nothing.
 */

class IKBoneConstraint
{
public:
	AZ_CLASS_ALLOCATOR(IKBoneConstraint, AZ::SystemAllocator);
    AZ_RTTI(EMotionFX::IK::IKBoneConstraint, "{34DC3B3D-EB8F-4E11-A5C6-BDF918C8175A}");

    // Constraint should only be enforced if this is set to true
    bool m_enabled;

    bool m_enableDebugDraw;

    IKBoneConstraint()
        : m_enabled(true)
        , m_enableDebugDraw(false)
    {
    }

    virtual ~IKBoneConstraint()
    {
    }

    // Initialize the constraint. This function must be called before
    // the constraint is used. Returns initialization success.
    virtual bool Initialize()
    {
        return true;
    }

    // Enforces the constraint. Will modify OutCSTransforms if needed.
    // @param Index - The index of this constraint in Constraints; should correspond to the same bone in in InCSTransforms and
    // OutCSTransforms
    // @param ReferenceCSTransforms - Array of bone transforms before skeletal controls (e.g., IK) are applied. Not necessarily in the
    // reference pose (although they might be, depending on your needs)
    // @param Constraints - Array of constraints for each bone (including this one, at index Index)
    // @param CSTransforms - Array of transforms as skeletal controls (e.g., IK) are being applied; this array will be modified in place
    // @param Character - Optional owning Character; may to left to nullptr, but is required for debug drawing.
    virtual void EnforceConstraint(
        [[maybe_unused]] size_t index,
        [[maybe_unused]] const AZStd::vector<Transform>& referenceCSTransforms,
        [[maybe_unused]] const AZStd::vector<IKBoneConstraint*>& constraints,
        [[maybe_unused]] AZStd::vector<Transform>& csTransforms)
    {
    }

    // Optional lambda to evaluate before the constraint is enforced. It can set up examine the chain and set
    // things up appropriately. Returns a bool; the constraint should only be enforced if it returns true.
    AZStd::function<void(
        size_t,
        const AZStd::vector<Transform>&,
        const AZStd::vector<IKBoneConstraint*>&,
        AZStd::vector<Transform>&)>
        SetupFn = []([[maybe_unused]] size_t index,
                     [[maybe_unused]] const AZStd::vector<Transform>& referenceCSTransforms,
                     [[maybe_unused]] const AZStd::vector<IKBoneConstraint*>& constraints,
                     [[maybe_unused]] AZStd::vector<Transform>& csTransforms)
    {
    };
};

/*
* A bone used in IK.
*
* Range of motion constraints can be specified, but are not used unless the bone is being used
* with an IK method that supports them.
*
*/
struct IKBone
{
	bool InitIfInvalid(const EMotionFX::Skeleton& skeleton);
	
	// Initialize this IK Bone. Must be called before use.
	bool Init(const EMotionFX::Skeleton& skeleton);

	bool IsValid(const EMotionFX::Skeleton& skeleton);

    AZStd::string m_boneName;
	size_t m_boneIndex = InvalidIndex;
	IKBoneConstraint* m_constraint = nullptr;
};

/*
* A basic IK chain. Doesn't contain any data yet, just an interface for testing validity. 
*
* The InitBoneReferences function must be called by the using animnode before use.
* This function should initialize bone references, and assign the RootBone and EffectorBone as needed.
*/
class IKModChain 
{
public:
	AZ_CLASS_ALLOCATOR(IKModChain, AZ::SystemAllocator);
    AZ_RTTI(EMotionFX::IK::IKModChain, "{D38750DE-8419-493E-A5D6-A715FFDEAE69}");
		   
	// Checks if this chain is valid; if not, attempts to initialize it and checks again.
    // Returns true if valid or initialization succeeds.
	virtual bool InitIfInvalid(const EMotionFX::Skeleton& skeleton);
	
	// Initialize all bones used in this chain. Must be called before use.
	// Subclasses must override this.
	virtual bool InitBoneReferences(const EMotionFX::Skeleton& skeleton);
	
	// Check whether this chain is valid to use. Should be called in the IsValid method of your animnode.
	// Subclasses must override this.
	virtual bool IsValid(const EMotionFX::Skeleton& skeleton);	
};

} // namespace VRGame::IK
