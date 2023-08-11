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

#include "IK.h"

#include <AzCore/Math/Transform.h>
#include "Node.h"
#include "Skeleton.h"

namespace EMotionFX::IK
{

AZ::Vector3 IKUtil::IKBoneAxisToVector(IKBoneAxis InBoneAxis)
{
	switch (InBoneAxis) 
	{
	case IKBoneAxis::IKBA_X: return AZ::Vector3(1.0f, 0.0f, 0.0f);
	case IKBoneAxis::IKBA_Y: return AZ::Vector3(0.0f, 1.0f, 0.0f);
	case IKBoneAxis::IKBA_Z: return AZ::Vector3(0.0f, 0.0f, 1.0f);
	case IKBoneAxis::IKBA_XNeg: return AZ::Vector3(-1.0f, 0.0f, 0.0f);
	case IKBoneAxis::IKBA_YNeg: return AZ::Vector3(0.0f, -1.0f, 0.0f);
	case IKBoneAxis::IKBA_ZNeg: return AZ::Vector3(0.0f, 0.0f, -1.0f);		
	}
	
	return AZ::Vector3(0.0f, 0.0f, 0.0f);
}

bool IKBone::InitIfInvalid(const EMotionFX::Skeleton& skeleton)
{
	if (IsValid(skeleton))
	{
		return true;
	}
	
	return Init(skeleton);
}
	
bool IKBone::Init(const EMotionFX::Skeleton& skeleton)
{
	if (m_constraint != nullptr && !m_constraint->Initialize())
	{
#if ENABLE_IK_DEBUG
		AZ_Warning("LogRTIK", false, "FIKBone::Init -- Constraint for bone %s failed to initialize",
			m_boneName.c_str());
#endif // ENABLE_IK_DEBUG
	}

	if (EMotionFX::Node* node = skeleton.FindNodeByName(m_boneName))
	{
		m_boneIndex = node->GetNodeIndex();
		return true;
	}
	else
	{
#if ENABLE_IK_DEBUG
		AZ_Warning("LogRTIK", false, "FIKBone::Init -- IK Bone initialization failed for bone: %s", m_boneName.c_str());
#endif // ENABLE_IK_DEBUG
		return false;
	}
}

bool IKBone::IsValid(const EMotionFX::Skeleton& skeleton)
{
	bool valid = skeleton.FindNodeByName(m_boneName) != nullptr;
	
#if ENABLE_IK_DEBUG_VERBOSE
	if (!valid)
	{
		AZ_Warning("LogRTIK", false, "FIKBone::IsValid -- IK Bone %s was invalid", m_boneName.c_str());
	}
#endif // ENABLE_IK_DEBUG_VERBOSE
	return valid;
}

bool IKModChain::InitIfInvalid(const EMotionFX::Skeleton& skeleton)
{
	if (IsValid(skeleton))
	{
		return true;
	}

	return InitBoneReferences(skeleton);
}

bool IKModChain::InitBoneReferences([[maybe_unused]] const EMotionFX::Skeleton& skeleton)
{
	return false;
}

bool IKModChain::IsValid([[maybe_unused]] const EMotionFX::Skeleton& skeleton)
{
	return false; 
}

}

