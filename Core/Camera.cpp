//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
// Developed by Minigraph
//
// Author:  James Stanard
//

#include "Camera.h"
#include <cmath>

using namespace Math;

void BaseCamera::SetLookDirection(glm::vec3 forward, glm::vec3 up)
{
    // Given, but ensure normalization
    float forwardLenSq = LengthSquare(forward);
    forward = glm::mix(forward * glm::inversesqrt(forwardLenSq), -CreateZUnitVector(), forwardLenSq < 0.000001f);

    // Deduce a valid, orthogonal right vector
    glm::vec3 right = glm::cross(forward, up);
    float rightLenSq = LengthSquare(forward);
    right = glm::mix(right * glm::inversesqrt(rightLenSq),
                     glm::quat(glm::vec3(0.0, -glm::half_pi<float>(), 0.0)) * forward, rightLenSq < 0.000001f);

    // Compute actual up vector
    up = glm::cross(right, forward);

    // Finish constructing basis
    m_Basis = glm::mat3(right, up, -forward);
    m_CameraToWorld.SetRotation(glm::quat(m_Basis));
}

void BaseCamera::Update()
{
    m_PreviousViewProjMatrix = m_ViewProjMatrix;
    auto invCTW = ~m_CameraToWorld;
    m_ViewMatrix = glm::mat4(invCTW.GetRotation());
    m_ViewMatrix[3] = glm::vec4(invCTW.GetTranslation(), 1.0);
    m_ViewProjMatrix = m_ProjMatrix * m_ViewMatrix;
    m_ReprojectMatrix = m_PreviousViewProjMatrix * glm::inverse(GetViewProjMatrix());

    m_FrustumVS = Frustum(m_ProjMatrix);
    m_FrustumWS = m_CameraToWorld * m_FrustumVS;
}

void Camera::UpdateProjMatrix(void)
{
    float Y = 1.0f / std::tanf(m_VerticalFOV * 0.5f);
    float X = Y * m_AspectRatio;

    float Q1, Q2;

    // ReverseZ puts far plane at Z=0 and near plane at Z=1.  This is never a bad idea, and it's
    // actually a great idea with F32 depth buffers to redistribute precision more evenly across
    // the entire range.  It requires clearing Z to 0.0f and using a GREATER variant depth test.
    // Some care must also be done to properly reconstruct linear W in a pixel shader from hyperbolic Z.
    if (m_ReverseZ)
    {
        if (m_InfiniteZ)
        {
            Q1 = 0.0f;
            Q2 = m_NearClip;
        }
        else
        {
            Q1 = m_NearClip / (m_FarClip - m_NearClip);
            Q2 = Q1 * m_FarClip;
        }
    }
    else
    {
        if (m_InfiniteZ)
        {
            Q1 = -1.0f;
            Q2 = -m_NearClip;
        }
        else
        {
            Q1 = m_FarClip / (m_NearClip - m_FarClip);
            Q2 = Q1 * m_NearClip;
        }
    }

    SetProjMatrix(glm::mat4(glm::vec4(X, 0.0f, 0.0f, 0.0f), glm::vec4(0.0f, Y, 0.0f, 0.0f),
                            glm::vec4(0.0f, 0.0f, Q1, -1.0f), glm::vec4(0.0f, 0.0f, Q2, 0.0f)));
}
