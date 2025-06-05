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

#include "ShadowCamera.h"
#include <glm/ext/quaternion_common.hpp>

using namespace Math;

void ShadowCamera::UpdateMatrix(glm::vec3 LightDirection, glm::vec3 ShadowCenter, glm::vec3 ShadowBounds,
                                uint32_t BufferWidth, uint32_t BufferHeight, uint32_t BufferPrecision)
{
    SetLookDirection(LightDirection, glm::vec3(0.0, 0.0, 1.0));

    // Converts world units to texel units so we can quantize the camera position to whole texel units
    glm::vec3 RcpDimensions = glm::vec3(1.0) / ShadowBounds;
    glm::vec3 QuantizeScale =
        glm::vec3((float)BufferWidth, (float)BufferHeight, (float)((1 << BufferPrecision) - 1)) * RcpDimensions;

    //
    // Recenter the camera at the quantized position
    //

    // Transform to view space
    ShadowCenter = glm::conjugate(GetRotation()) * ShadowCenter;
    // Scale to texel units, truncate fractional part, and scale back to world units
    ShadowCenter = glm::floor(ShadowCenter * QuantizeScale) / QuantizeScale;
    // Transform back into world space
    ShadowCenter = GetRotation() * ShadowCenter;

    SetPosition(ShadowCenter);

    SetProjMatrix(glm::scale(glm::mat4(1.0), glm::vec3(2.0f, 2.0f, 1.0f) * RcpDimensions));

    Update();

    // Transform from clip space to texture space
    m_ShadowMatrix = glm::mat4(AffineTransform(glm::mat3(glm::scale(glm::mat4(1.0), glm::vec3(0.5f, -0.5f, 1.0f))),
                                               glm::vec3(0.5f, 0.5f, 0.0f))) *
                     m_ViewProjMatrix;
}
