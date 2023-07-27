/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <Atom/RPI.Public/View.h>
#include <Atom/RHI/RHISystemInterface.h>
#include <Atom/RPI.Public/RPISystemInterface.h>
#include <Atom/RPI.Public/Shader/ShaderResourceGroup.h>
#include <Atom/RPI.Public/Culling.h>
#include <Atom/RPI.Public/RenderPipeline.h>
#include <Atom/RPI.Public/Pass/Specific/SwapChainPass.h>
#include <Atom/RHI/DrawListTagRegistry.h>

#include <AzCore/Casting/lossy_cast.h>
#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/Math/MatrixUtils.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Jobs/JobCompletion.h>
#include <AzCore/Jobs/JobFunction.h>
#include <AzCore/Task/TaskGraph.h>
#include <Atom_RPI_Traits_Platform.h>

#if AZ_TRAIT_MASKED_OCCLUSION_CULLING_SUPPORTED
#include <MaskedOcclusionCulling/MaskedOcclusionCulling.h>
#endif

namespace AZ
{
    namespace RPI
    {
        // fixed-size software occlusion culling buffer
#if AZ_TRAIT_MASKED_OCCLUSION_CULLING_SUPPORTED
        const uint32_t MaskedSoftwareOcclusionCullingWidth = 1920;
        const uint32_t MaskedSoftwareOcclusionCullingHeight = 1080;
#endif

        ViewPtr View::CreateView(const AZ::Name& name, UsageFlags usage)
        {
            View* view = aznew View(name, usage);
            return ViewPtr(view);
        }

        View::View(const AZ::Name& name, UsageFlags usage)
            : m_name(name)
            , m_usageFlags(usage)
        {
            AZ_Assert(!name.IsEmpty(), "invalid name");

            // Set default matrices 
            SetWorldToViewMatrix(AZ::Matrix4x4::CreateIdentity());
            AZ::Matrix4x4 viewToClipMatrix;
            AZ::MakePerspectiveFovMatrixRH(viewToClipMatrix, AZ::Constants::HalfPi, 1, 0.1f, 1000.f, true);
            SetViewToClipMatrix(viewToClipMatrix);

            if ((usage & UsageFlags::UsageXR))
            {
                SetViewToClipMatrix(AZ::Matrix4x4::CreateIdentity());
            }

            TryCreateShaderResourceGroup();

#if AZ_TRAIT_MASKED_OCCLUSION_CULLING_SUPPORTED
            m_maskedOcclusionCulling = MaskedOcclusionCulling::Create();
            m_maskedOcclusionCulling->SetResolution(MaskedSoftwareOcclusionCullingWidth, MaskedSoftwareOcclusionCullingHeight);
#endif
        }

        View::~View()
        {
#if AZ_TRAIT_MASKED_OCCLUSION_CULLING_SUPPORTED
            if (m_maskedOcclusionCulling)
            {
                MaskedOcclusionCulling::Destroy(m_maskedOcclusionCulling);
                m_maskedOcclusionCulling = nullptr;
            }
#endif
        }

        void View::SetDrawListMask(const RHI::DrawListMask& drawListMask)
        {
            if (m_drawListMask != drawListMask)
            {
                m_drawListMask = drawListMask;
                m_drawListContext.Shutdown();
                m_drawListContext.Init(m_drawListMask);
            }
        }

        void View::Reset()
        {
            m_drawListMask.reset();
            m_drawListContext.Shutdown();
            m_passesByDrawList = nullptr;
        }

        RHI::ShaderResourceGroup* View::GetRHIShaderResourceGroup() const
        {
            return m_shaderResourceGroup->GetRHIShaderResourceGroup();
        }

        Data::Instance<RPI::ShaderResourceGroup> View::GetShaderResourceGroup()
        {
            return m_shaderResourceGroup;
        }

        void View::AddDrawPacket(const RHI::DrawPacket* drawPacket, float depth)
        {
            // This function is thread safe since DrawListContent has storage per thread for draw item data.
            m_drawListContext.AddDrawPacket(drawPacket, depth);
        }        

        void View::AddDrawPacket(const RHI::DrawPacket* drawPacket, Vector3 worldPosition)
        {
            Vector3 cameraToObject = worldPosition - m_position[0];
            float depth = cameraToObject.Dot(-m_viewToWorldMatrix[0].GetBasisZAsVector3());
            AddDrawPacket(drawPacket, depth);
        }

        void View::AddDrawItem(RHI::DrawListTag drawListTag, const RHI::DrawItemProperties& drawItemProperties)
        {
            m_drawListContext.AddDrawItem(drawListTag, drawItemProperties);
        }

        void View::ApplyFlags(uint32_t flags)
        {
            AZStd::atomic_fetch_and(&m_andFlags, flags);
            AZStd::atomic_fetch_or(&m_orFlags, flags);
        }

        void View::ClearFlags(uint32_t flags)
        {
            AZStd::atomic_fetch_or(&m_andFlags, flags);
            AZStd::atomic_fetch_and(&m_orFlags, ~flags);
        }

        void View::ClearAllFlags()
        {
            ClearFlags(0xFFFFFFFF);
        }

        uint32_t View::GetAndFlags()
        {
            return m_andFlags;
        }

        uint32_t View::GetOrFlags()
        {
            return m_orFlags;
        }

        void View::SetWorldToViewMatrix(const AZ::Matrix4x4& worldToView, int index)
        {
            m_viewToWorldMatrix[index] = worldToView.GetInverseFast();
            m_position[index] = m_viewToWorldMatrix[index].GetTranslation();

            m_worldToViewMatrix[index] = worldToView;
            m_worldToClipMatrix[index] = m_viewToClipMatrix[index] * m_worldToViewMatrix[index];
            m_clipToWorldMatrix[index] = m_worldToClipMatrix[index].GetInverseFull();

            m_onWorldToViewMatrixChange.Signal(m_worldToViewMatrix[index]);
            m_onWorldToClipMatrixChange.Signal(m_worldToClipMatrix[index]);
        }

        AZ::Transform View::GetCameraTransform(int index) const
        {
            static const Quaternion yUpToZUp = Quaternion::CreateRotationX(-AZ::Constants::HalfPi);
            return AZ::Transform::CreateFromQuaternionAndTranslation(
                Quaternion::CreateFromMatrix4x4(m_viewToWorldMatrix[index]) * yUpToZUp,
                m_viewToWorldMatrix[index].GetTranslation()
            ).GetOrthogonalized();
        }

        void View::SetCameraTransform(const AZ::Matrix3x4& cameraTransform, int index)
        {
            m_position[index] = cameraTransform.GetTranslation();

            // Before inverting the matrix we must first adjust from Z-up to Y-up. The camera world matrix
            // is in a Z-up world and an identity matrix means that it faces along the positive-Y axis and Z is up.
            // An identity view matrix on the other hand looks along the negative Z-axis.
            // So we adjust for this by rotating the camera world matrix by 90 degrees around the X axis.
            static AZ::Matrix3x4 zUpToYUp = AZ::Matrix3x4::CreateRotationX(AZ::Constants::HalfPi);
            AZ::Matrix3x4 yUpWorld = cameraTransform * zUpToYUp;

            float viewToWorldMatrixRaw[16] = {
                        1,0,0,0,
                        0,1,0,0,
                        0,0,1,0,
                        0,0,0,1 };
            yUpWorld.StoreToRowMajorFloat12(viewToWorldMatrixRaw);
            const AZ::Matrix4x4 prevViewToWorldMatrix = m_viewToWorldMatrix[index];
            m_viewToWorldMatrix[index] = AZ::Matrix4x4::CreateFromRowMajorFloat16(viewToWorldMatrixRaw);

            m_worldToViewMatrix[index] = m_viewToWorldMatrix[index].GetInverseFast();

            m_worldToClipMatrix[index] = m_viewToClipMatrix[index] * m_worldToViewMatrix[index];
            m_clipToWorldMatrix[index] = m_worldToClipMatrix[index].GetInverseFull();

            // Only signal an update when there is a change, otherwise this might block
            // user input from changing the value.
            if (!prevViewToWorldMatrix.IsClose(m_viewToWorldMatrix[index]))
            {
                m_onWorldToViewMatrixChange.Signal(m_worldToViewMatrix[index]);
            }
            m_onWorldToClipMatrixChange.Signal(m_worldToClipMatrix[index]);
        }        

        void View::SetViewToClipMatrix(const AZ::Matrix4x4& viewToClip, int index)
        {
            m_viewToClipMatrix[index] = viewToClip;
            m_clipToViewMatrix[index] = m_viewToClipMatrix[index].GetInverseFull();
            m_worldToClipMatrix[index] = m_viewToClipMatrix[index] * m_worldToViewMatrix[index];
            m_clipToWorldMatrix[index] = m_worldToClipMatrix[index].GetInverseFull();

            // Update z depth constant simultaneously
            // zNear -> n, zFar -> f
            // A = f / (n - f), B = nf / (n - f)
            double A = m_viewToClipMatrix[index].GetElement(2, 2);
            double B = m_viewToClipMatrix[index].GetElement(2, 3);

            // Based on linearZ = fn / (depth*(f-n) - f)
            m_linearizeDepthConstants[index].SetX(float(B / A)); //<------------n
            m_linearizeDepthConstants[index].SetY(float(B / (A + 1.0))); //<---------f
            m_linearizeDepthConstants[index].SetZ(float((B * B) / (A * (A + 1.0)))); //<-----nf
            m_linearizeDepthConstants[index].SetW(float(-B / (A * (A + 1.0)))); //<------f-n

            // For reverse depth the expression we dont have to do anything different as m_linearizeDepthConstants works out to be the same.
            // A = n / (f - n), B = nf / (f - n)
            // Based on linearZ = fn / (depth*(n-f) - n)
            //m_linearizeDepthConstants.SetX(float(B / A)); //<----f
            //m_linearizeDepthConstants.SetY(float(B / (A + 1.0))); //<----n
            //m_linearizeDepthConstants.SetZ(float((B * B) / (A * (A + 1.0)))); //<----nf
            //m_linearizeDepthConstants.SetW(float(-B / (A * (A + 1.0)))); //<-----n-f

            double tanHalfFovX = m_clipToViewMatrix[index].GetElement(0, 0);
            double tanHalfFovY = m_clipToViewMatrix[index].GetElement(1, 1);

            // The constants below try to remapping 0---1 to -1---+1 and multiplying with inverse of projection.
            // Assuming that inverse of projection matrix only has value in the first column for first row
            // x = (2u-1)*ProjInves[0][0]
            // Assuming that inverse of projection matrix only has value in the second column for second row
            // y = (1-2v)*ProjInves[1][1]
            m_unprojectionConstants[index].SetX(float(2.0 * tanHalfFovX));
            m_unprojectionConstants[index].SetY(float(-2.0 * tanHalfFovY));
            m_unprojectionConstants[index].SetZ(float(-tanHalfFovX));
            m_unprojectionConstants[index].SetW(float(tanHalfFovY));

            m_onWorldToClipMatrixChange.Signal(m_worldToClipMatrix[index]);
        }

        void View::SetStereoscopicViewToClipMatrix(const AZ::Matrix4x4& viewToClip, bool reverseDepth, int index)
        {
            m_viewToClipMatrix[index] = viewToClip;
            m_clipToViewMatrix[index] = m_viewToClipMatrix[index].GetInverseFull();
            
            m_worldToClipMatrix[index] = m_viewToClipMatrix[index] * m_worldToViewMatrix[index];
            m_clipToWorldMatrix[index] = m_worldToClipMatrix[index].GetInverseFull();

            // Update z depth constant simultaneously
            if(reverseDepth)
            {       
                // zNear -> n, zFar -> f
                // A = 2n/(f-n), B = 2fn / (f - n)
                // the formula of A and B should be the same as projection matrix's definition
                // currently defined in CreateStereoscopicProjection in XRUtils.cpp
                double A = m_viewToClipMatrix[index].GetElement(2, 2);
                double B = m_viewToClipMatrix[index].GetElement(2, 3);

                // Based on linearZ = 2fn / (depth*(n-f) - 2n)
                m_linearizeDepthConstants[index].SetX(float(B / A)); //<----f
                m_linearizeDepthConstants[index].SetY(float((2 * B) / (A + 2.0))); //<--- 2n
                m_linearizeDepthConstants[index].SetZ(float((2 * B * B) / (A * (A + 2.0)))); //<-----2fn
                m_linearizeDepthConstants[index].SetW(float((-2 * B) / (A * (A + 2.0)))); //<------n-f
            }
            else
            {
                // A = -(f+n)/(f-n), B = -2fn / (f - n)
                double A = m_viewToClipMatrix[index].GetElement(2, 2);
                double B = m_viewToClipMatrix[index].GetElement(2, 3);

                //Based on linearZ = 2fn / (depth*(f-n) - (-f-n))
                m_linearizeDepthConstants[index].SetX(float(B / (A + 1.0))); //<----f
                m_linearizeDepthConstants[index].SetY(float((-2 * B * A)/ ((A + 1.0) * (A - 1.0)))); //<--- -f-n
                m_linearizeDepthConstants[index].SetZ(float((2 * B * B) / ((A - 1.0) * (A + 1.0)))); //<-----2fn
                m_linearizeDepthConstants[index].SetW(float((-2 * B) / ((A - 1.0) * (A + 1.0)))); //<------f-n
            }     

            // The constants below try to remap 0---1 to -1---+1 and multiply with inverse of projection.
            // Assuming that inverse of projection matrix only has value in the first column for first row
            // x = (2u-1)*ProjInves[0][0] + ProjInves[0][3]
            // Assuming that inverse of projection matrix only has value in the second column for second row
            // y = (1-2v)*ProjInves[1][1] + ProjInves[1][3]
            float multiplierConstantX = 2.0f * m_clipToViewMatrix[index].GetElement(0, 0);
            float multiplierConstantY = -2.0f * m_clipToViewMatrix[index].GetElement(1, 1);
            float additionConstantX = m_clipToViewMatrix[index].GetElement(0, 3) - m_clipToViewMatrix[index].GetElement(0, 0);
            float additionConstantY = m_clipToViewMatrix[index].GetElement(1, 1) + m_clipToViewMatrix[index].GetElement(1, 3);

            m_unprojectionConstants[index].SetX(multiplierConstantX);
            m_unprojectionConstants[index].SetY(multiplierConstantY);
            m_unprojectionConstants[index].SetZ(additionConstantX);
            m_unprojectionConstants[index].SetW(additionConstantY);

            m_onWorldToClipMatrixChange.Signal(m_worldToClipMatrix[index]);
        }
        
        void View::SetClipSpaceOffset(float xOffset, float yOffset)
        {
            m_clipSpaceOffset.Set(xOffset, yOffset);
        }

        const AZ::Matrix4x4& View::GetWorldToViewMatrix(int index) const
        {
            return m_worldToViewMatrix[index];
        }

        const AZ::Matrix4x4& View::GetViewToWorldMatrix(int index) const
        {
            return m_viewToWorldMatrix[index];
        }

        AZ::Matrix3x4 View::GetWorldToViewMatrixAsMatrix3x4(int index) const
        {
            return AZ::Matrix3x4::UnsafeCreateFromMatrix4x4(m_worldToViewMatrix[index]);
        }

        AZ::Matrix3x4 View::GetViewToWorldMatrixAsMatrix3x4(int index) const
        {
            return AZ::Matrix3x4::UnsafeCreateFromMatrix4x4(m_viewToWorldMatrix[index]);
        }

        const AZ::Matrix4x4& View::GetViewToClipMatrix(int index) const
        {
            return m_viewToClipMatrix[index];
        }

        const AZ::Matrix4x4& View::GetWorldToClipMatrix(int index) const
        {
            return m_worldToClipMatrix[index];
        }

        const AZ::Matrix4x4& View::GetClipToWorldMatrix(int index) const
        {
            return m_clipToWorldMatrix[index];
        }

        bool View::HasDrawListTag(RHI::DrawListTag drawListTag)
        {
            return drawListTag.IsValid() && m_drawListMask[drawListTag.GetIndex()];
        }

        RHI::DrawListView View::GetDrawList(RHI::DrawListTag drawListTag)
        {
            return m_drawListContext.GetList(drawListTag);
        }

        void View::FinalizeDrawListsTG(AZ::TaskGraphEvent& finalizeDrawListsTGEvent)
        {
            AZ_PROFILE_SCOPE(RPI, "View: FinalizeDrawLists");
            m_drawListContext.FinalizeLists();
            SortFinalizedDrawListsTG(finalizeDrawListsTGEvent);
        }
        void View::FinalizeDrawListsJob(AZ::Job* parentJob)
        {
            AZ_PROFILE_SCOPE(RPI, "View: FinalizeDrawLists");
            m_drawListContext.FinalizeLists();
            SortFinalizedDrawListsJob(parentJob);
        }

        void View::SortFinalizedDrawListsTG(AZ::TaskGraphEvent& finalizeDrawListsTGEvent)
        {
            AZ_PROFILE_SCOPE(RPI, "View: SortFinalizedDrawLists");
            RHI::DrawListsByTag& drawListsByTag = m_drawListContext.GetMergedDrawListsByTag();

            AZ::TaskGraph drawListSortTG{ "DrawList Sort" };
            AZ::TaskDescriptor drawListSortTGDescriptor{"RPI_View_SortFinalizedDrawLists", "Graphics"};
            for (size_t idx = 0; idx < drawListsByTag.size(); ++idx)
            {
                if (drawListsByTag[idx].size() > 1)
                {
                    drawListSortTG.AddTask(drawListSortTGDescriptor, [this, &drawListsByTag, idx]()
                    {
                        AZ_PROFILE_SCOPE(RPI, "View: SortDrawList Task");
                        SortDrawList(drawListsByTag[idx], RHI::DrawListTag(idx));
                    });
                }
            }
            if (!drawListSortTG.IsEmpty())
            {
                drawListSortTG.Detach();
                drawListSortTG.Submit(&finalizeDrawListsTGEvent);
            }
        }

        void View::SortFinalizedDrawListsJob(AZ::Job* parentJob)
        {
            AZ_PROFILE_SCOPE(RPI, "View: SortFinalizedDrawLists");
            RHI::DrawListsByTag& drawListsByTag = m_drawListContext.GetMergedDrawListsByTag();

            AZ::JobCompletion jobCompletion;
            for (size_t idx = 0; idx < drawListsByTag.size(); ++idx)
            {
                if (drawListsByTag[idx].size() > 1)
                {
                    auto jobLambda = [this, &drawListsByTag, idx]()
                    {
                        AZ_PROFILE_SCOPE(RPI, "View: SortDrawList Job");
                        SortDrawList(drawListsByTag[idx], RHI::DrawListTag(idx));
                    };
                    Job* jobSortDrawList = aznew JobFunction<decltype(jobLambda)>(jobLambda, true, nullptr); // Auto-deletes
                    if (parentJob)
                    {
                        parentJob->StartAsChild(jobSortDrawList);
                    }
                    else
                    {
                        jobSortDrawList->SetDependent(&jobCompletion);
                        jobSortDrawList->Start();
                    }
                }
            }
            if (parentJob)
            {
                parentJob->WaitForChildren();
            }
            else
            {
                jobCompletion.StartAndWaitForCompletion();
            }
        }

        void View::SortDrawList(RHI::DrawList& drawList, RHI::DrawListTag tag)
        {
            // Note: it's possible that the m_passesByDrawList doesn't have a pass for the input tag.
            // This is because a View can be used for multiple render pipelines.
            // So it may contains draw list tag which exists in one render pipeline but not others. 
            auto itr = m_passesByDrawList->find(tag);
            if (itr != m_passesByDrawList->end())
            {
                itr->second->SortDrawList(drawList);
            }
        }

        void View::ConnectWorldToViewMatrixChangedHandler(MatrixChangedEvent::Handler& handler)
        {
            handler.Connect(m_onWorldToViewMatrixChange);
        }

        void View::ConnectWorldToClipMatrixChangedHandler(MatrixChangedEvent::Handler& handler)
        {
            handler.Connect(m_onWorldToClipMatrixChange);
        }

        // [GFX TODO] This function needs unit tests and might need to be reworked 
        RHI::DrawItemSortKey View::GetSortKeyForPosition(const Vector3& positionInWorld) const
        {
            // We are using fixed-point depth representation for the u64 sort key

            // Compute position in clip space
            const Vector4 worldPosition4 = Vector4::CreateFromVector3(positionInWorld);
            const Vector4 clipSpacePosition = m_worldToClipMatrix[0] * worldPosition4;

            // Get a depth value guaranteed to be in the range 0 to 1
            float normalizedDepth = clipSpacePosition.GetZ() / clipSpacePosition.GetW();
            normalizedDepth = (normalizedDepth + 1.0f) * 0.5f;
            normalizedDepth = AZStd::clamp<float>(normalizedDepth, 0.f, 1.f);

            // Convert the depth into a uint64
            RHI::DrawItemSortKey sortKey = static_cast<RHI::DrawItemSortKey>(normalizedDepth * azlossy_cast<double>(std::numeric_limits<RHI::DrawItemSortKey>::max()));

            return sortKey;
        }

        float View::CalculateSphereAreaInClipSpace(const AZ::Vector3& sphereWorldPosition, float sphereRadius) const
        {
            // Projection of a sphere to clip space 
            // Derived from https://www.iquilezles.org/www/articles/sphereproj/sphereproj.htm

            if (sphereRadius <= 0.0f)
            {
                return 0.0f;
            }

            const AZ::Matrix4x4& worldToViewMatrix = GetWorldToViewMatrix();
            const AZ::Matrix4x4& viewToClipMatrix = GetViewToClipMatrix();

            // transform to camera space (eye space)
            const Vector4 worldPosition4 = Vector4::CreateFromVector3(sphereWorldPosition);
            const Vector4 viewSpacePosition = worldToViewMatrix * worldPosition4;

            float zDist = -viewSpacePosition.GetZ();    // in our view space Z is negative in front of the camera

            if (zDist < 0.0f)
            {
                // sphere center is behind camera.
                if (zDist < -sphereRadius)
                {
                    return 0.0f;    // whole of sphere is behind camera so zero coverage
                }
                else
                {
                    return 1.0f;    // camera is inside sphere so treat as covering whole view
                }
            }
            else
            {
                if (zDist < sphereRadius)
                {
                    return 1.0f;   // camera is inside sphere so treat as covering whole view
                }
            }

            // Element 1,1 of the projection matrix is equal to :  1 / tan(fovY/2) AKA cot(fovY/2)
            // See https://stackoverflow.com/questions/46182845/field-of-view-aspect-ratio-view-matrix-from-projection-matrix-hmd-ost-calib
            float cotHalfFovY = viewToClipMatrix.GetElement(1, 1);

            float radiusSq = sphereRadius * sphereRadius;
            float depthSq = zDist * zDist;
            float distanceSq = viewSpacePosition.GetAsVector3().GetLengthSq();
            float cotHalfFovYSq = cotHalfFovY * cotHalfFovY;

            float radiusSqSubDepthSq = radiusSq - depthSq;

            const float epsilon = 0.00001f;
            if (fabsf(radiusSqSubDepthSq) < epsilon)
            {
                // treat as covering entire view since we don't want to divide by zero
                return 1.0f;
            }

            // This will return 1.0f when an area equal in size to the viewport height squared is covered.
            // So to get actual pixels covered do : coverage * viewport-resolution-y * viewport-resolution-y
            // The actual math computes the area of an ellipse as a percentage of the view area, see the paper above for the steps
            // to simplify the equations into this calculation.
            return  -0.25f * cotHalfFovYSq * AZ::Constants::Pi * radiusSq * sqrt(fabsf((distanceSq - radiusSq)/radiusSqSubDepthSq))/radiusSqSubDepthSq;
        }

        void View::UpdateSrg()
        {
            if (m_usageFlags & UsageFlags::UsageXR)
            {
                UpdateSrgStereo();
            }
            else
            {
                UpdateSrgMono();
            }
        }

        void View::UpdateSrgStereo()
        {
            if (m_shaderResourceGroup)
            {
                if (m_clipSpaceOffset.IsZero())
                {
                    AZStd::array<Matrix4x4, 2> worldToClipPrevMatrix;
                    worldToClipPrevMatrix[0] = m_viewToClipPrevMatrix[0] * m_worldToViewPrevMatrix[0];
                    worldToClipPrevMatrix[1] = m_viewToClipPrevMatrix[1] * m_worldToViewPrevMatrix[1];
                    m_shaderResourceGroup->SetConstantArray(m_worldToClipPrevMatrixConstantIndex, worldToClipPrevMatrix);
                    m_shaderResourceGroup->SetConstantArray(m_viewProjectionMatrixConstantIndex, m_worldToClipMatrix);
                    m_shaderResourceGroup->SetConstantArray(m_projectionMatrixConstantIndex, m_viewToClipMatrix);
                    m_shaderResourceGroup->SetConstantArray(m_clipToWorldMatrixConstantIndex, m_clipToWorldMatrix);
                    m_shaderResourceGroup->SetConstantArray(m_projectionMatrixInverseConstantIndex, m_clipToViewMatrix);
                }
                else
                {
                    // Offset the current and previous frame clip matrices
                    AZStd::array<Matrix4x4, 2> offsetViewToClipMatrix = m_viewToClipMatrix;
                    offsetViewToClipMatrix[0].SetElement(0, 2, m_clipSpaceOffset.GetX());
                    offsetViewToClipMatrix[0].SetElement(1, 2, m_clipSpaceOffset.GetY());
                    offsetViewToClipMatrix[1].SetElement(0, 2, m_clipSpaceOffset.GetX());
                    offsetViewToClipMatrix[1].SetElement(1, 2, m_clipSpaceOffset.GetY());

                    AZStd::array<Matrix4x4, 2> offsetViewToClipPrevMatrix = m_viewToClipPrevMatrix;
                    offsetViewToClipPrevMatrix[0].SetElement(0, 2, m_clipSpaceOffset.GetX());
                    offsetViewToClipPrevMatrix[0].SetElement(1, 2, m_clipSpaceOffset.GetY());
                    offsetViewToClipPrevMatrix[1].SetElement(0, 2, m_clipSpaceOffset.GetX());
                    offsetViewToClipPrevMatrix[1].SetElement(1, 2, m_clipSpaceOffset.GetY());

                    // Build other matrices dependent on the view to clip matrices
                    AZStd::array<Matrix4x4, 2> offsetWorldToClipMatrix;
                    offsetWorldToClipMatrix[0] = offsetViewToClipMatrix[0] * m_worldToViewMatrix[0];
                    offsetWorldToClipMatrix[1] = offsetViewToClipMatrix[1] * m_worldToViewMatrix[1];
                    AZStd::array<Matrix4x4, 2> offsetWorldToClipPrevMatrix;
                    offsetWorldToClipPrevMatrix[0] = offsetViewToClipPrevMatrix[0] * m_worldToViewPrevMatrix[0];
                    offsetWorldToClipPrevMatrix[1] = offsetViewToClipPrevMatrix[1] * m_worldToViewPrevMatrix[1];
                    AZStd::array<Matrix4x4, 2> offsetClipToWorldMatrix;
                    offsetClipToWorldMatrix[0] = offsetWorldToClipMatrix[0].GetInverseFull();
                    offsetClipToWorldMatrix[1] = offsetWorldToClipMatrix[1].GetInverseFull();
                    AZStd::array<Matrix4x4, 2> offsetClipToViewMatrix;
                    offsetClipToViewMatrix[0] = offsetViewToClipMatrix[0].GetInverseFull();
                    offsetClipToViewMatrix[1] = offsetViewToClipMatrix[1].GetInverseFull();

                    m_shaderResourceGroup->SetConstantArray(m_worldToClipPrevMatrixConstantIndex, offsetWorldToClipPrevMatrix);
                    m_shaderResourceGroup->SetConstantArray(m_viewProjectionMatrixConstantIndex, offsetWorldToClipMatrix);
                    m_shaderResourceGroup->SetConstantArray(m_projectionMatrixConstantIndex, offsetViewToClipMatrix);
                    m_shaderResourceGroup->SetConstantArray(m_clipToWorldMatrixConstantIndex, offsetClipToWorldMatrix);
                    m_shaderResourceGroup->SetConstantArray(m_projectionMatrixInverseConstantIndex, offsetClipToViewMatrix);
                }

                // Set these individually because a Vector3 is actually 16bytes for efficiency reasons I presume
                m_shaderResourceGroup->SetConstantArray(m_worldPositionConstantIndex, m_position);
                m_shaderResourceGroup->SetConstantArray(m_viewMatrixConstantIndex, m_worldToViewMatrix);
                m_shaderResourceGroup->SetConstantArray(m_viewMatrixInverseConstantIndex, m_viewToWorldMatrix);
                m_shaderResourceGroup->SetConstantArray(m_zConstantsConstantIndex, m_linearizeDepthConstants);
                m_shaderResourceGroup->SetConstantArray(m_unprojectionConstantsIndex, m_unprojectionConstants);

                m_shaderResourceGroup->Compile();
            }

            m_viewToClipPrevMatrix[0] = m_viewToClipMatrix[0];
            m_worldToViewPrevMatrix[0] = m_worldToViewMatrix[0];
            m_viewToClipPrevMatrix[1] = m_viewToClipMatrix[1];
            m_worldToViewPrevMatrix[1] = m_worldToViewMatrix[1];

            m_clipSpaceOffset.Set(0);
        }

        void View::UpdateSrgMono()
        {
            if (m_shaderResourceGroup)
            {
                if (m_clipSpaceOffset.IsZero())
                {
                    Matrix4x4 worldToClipPrevMatrix = m_viewToClipPrevMatrix[0] * m_worldToViewPrevMatrix[0];
                    m_shaderResourceGroup->SetConstant(m_worldToClipPrevMatrixConstantIndex, worldToClipPrevMatrix, 0);
                    m_shaderResourceGroup->SetConstant(m_viewProjectionMatrixConstantIndex, m_worldToClipMatrix[0], 0);
                    m_shaderResourceGroup->SetConstant(m_projectionMatrixConstantIndex, m_viewToClipMatrix[0], 0);
                    m_shaderResourceGroup->SetConstant(m_clipToWorldMatrixConstantIndex, m_clipToWorldMatrix[0], 0);
                    m_shaderResourceGroup->SetConstant(m_projectionMatrixInverseConstantIndex, m_clipToViewMatrix[0], 0);
                }
                else
                {
                    // Offset the current and previous frame clip matrices
                    Matrix4x4 offsetViewToClipMatrix = m_viewToClipMatrix[0];
                    offsetViewToClipMatrix.SetElement(0, 2, m_clipSpaceOffset.GetX());
                    offsetViewToClipMatrix.SetElement(1, 2, m_clipSpaceOffset.GetY());

                    Matrix4x4 offsetViewToClipPrevMatrix = m_viewToClipPrevMatrix[0];
                    offsetViewToClipPrevMatrix.SetElement(0, 2, m_clipSpaceOffset.GetX());
                    offsetViewToClipPrevMatrix.SetElement(1, 2, m_clipSpaceOffset.GetY());

                    // Build other matrices dependent on the view to clip matrices
                    Matrix4x4 offsetWorldToClipMatrix = offsetViewToClipMatrix * m_worldToViewMatrix[0];
                    Matrix4x4 offsetWorldToClipPrevMatrix = offsetViewToClipPrevMatrix * m_worldToViewPrevMatrix[0];

                    m_shaderResourceGroup->SetConstant(m_worldToClipPrevMatrixConstantIndex, offsetWorldToClipPrevMatrix, 0);
                    m_shaderResourceGroup->SetConstant(m_viewProjectionMatrixConstantIndex, offsetWorldToClipMatrix, 0);
                    m_shaderResourceGroup->SetConstant(m_projectionMatrixConstantIndex, offsetViewToClipMatrix, 0);
                    m_shaderResourceGroup->SetConstant(m_clipToWorldMatrixConstantIndex, offsetWorldToClipMatrix.GetInverseFull(), 0);
                    m_shaderResourceGroup->SetConstant(m_projectionMatrixInverseConstantIndex, offsetViewToClipMatrix.GetInverseFull(), 0);
                }

                m_shaderResourceGroup->SetConstant(m_worldPositionConstantIndex, m_position[0], 0);
                m_shaderResourceGroup->SetConstant(m_viewMatrixConstantIndex, m_worldToViewMatrix[0], 0);
                m_shaderResourceGroup->SetConstant(m_viewMatrixInverseConstantIndex, m_viewToWorldMatrix[0], 0);
                m_shaderResourceGroup->SetConstant(m_zConstantsConstantIndex, m_linearizeDepthConstants[0], 0);
                m_shaderResourceGroup->SetConstant(m_unprojectionConstantsIndex, m_unprojectionConstants[0], 0);

                m_shaderResourceGroup->Compile();
            }

            m_viewToClipPrevMatrix[0] = m_viewToClipMatrix[0];
            m_worldToViewPrevMatrix[0] = m_worldToViewMatrix[0];

            m_clipSpaceOffset.Set(0);
        }

        void View::BeginCulling()
        {
#if AZ_TRAIT_MASKED_OCCLUSION_CULLING_SUPPORTED
            AZ_PROFILE_SCOPE(RPI, "View: ClearMaskedOcclusionBuffer");
            m_maskedOcclusionCulling->ClearBuffer();
#endif
        }

        MaskedOcclusionCulling* View::GetMaskedOcclusionCulling()
        {
            return m_maskedOcclusionCulling;
        }

        void View::TryCreateShaderResourceGroup()
        {
            if (!m_shaderResourceGroup)
            {
                if (auto rpiSystemInterface = RPISystemInterface::Get())
                {
                    
                    if (Data::Asset<ShaderAsset> viewSrgShaderAsset = rpiSystemInterface->GetCommonShaderAssetForSrgs();
                        viewSrgShaderAsset.IsReady())
                    {
                        m_shaderResourceGroup =
                            ShaderResourceGroup::Create(viewSrgShaderAsset, rpiSystemInterface->GetViewSrgLayout()->GetName());
                    }
                }
            }
        }

        void View::OnAddToRenderPipeline()
        {
            TryCreateShaderResourceGroup();
            if (!m_shaderResourceGroup)
            {
                AZ_Warning("RPI::View", false, "Shader Resource Group failed to initialize");
            }
        }
    } // namespace RPI
} // namespace AZ
