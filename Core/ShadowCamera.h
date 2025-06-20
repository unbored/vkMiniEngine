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

#pragma once

#include "Camera.h"

class ShadowCamera : public Math::BaseCamera
{
public:
    ShadowCamera() {}

    void UpdateMatrix(
        glm::vec3 LightDirection, // Direction parallel to light, in direction of travel
        glm::vec3 ShadowCenter,   // Center location on far bounding plane of shadowed region
        glm::vec3 ShadowBounds,   // Width, height, and depth in world space represented by the shadow buffer
        uint32_t BufferWidth,     // Shadow buffer width
        uint32_t BufferHeight,    // Shadow buffer height--usually same as width
        uint32_t BufferPrecision  // Bit depth of shadow buffer--usually 16 or 24
    );

    // Used to transform world space to texture space for shadow sampling
    const glm::mat4 &GetShadowMatrix() const { return m_ShadowMatrix; }

private:
    glm::mat4 m_ShadowMatrix;
};
