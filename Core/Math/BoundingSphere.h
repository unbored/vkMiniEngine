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

#include "Common.h"
#include <glm/glm.hpp>

namespace Math
{
class BoundingSphere
{
public:
    BoundingSphere() {}
    BoundingSphere(float x, float y, float z, float r) : m_repr(x, y, z, r) {}
    BoundingSphere(const float *array) : m_repr(array[0], array[1], array[2], array[3]) {}
    BoundingSphere(glm::vec3 center, float radius);
    BoundingSphere(EZeroTag) : m_repr(glm::vec4(0.0)) {}
    explicit BoundingSphere(const glm::vec4 &v) : m_repr(v) {}
    // explicit BoundingSphere( const XMFLOAT4& f4 ) : m_repr(f4) {}
    // explicit BoundingSphere( glm::vec4 sphere ) : m_repr(sphere) {}
    explicit operator glm::vec4() const { return glm::vec4(m_repr); }

    glm::vec3 GetCenter(void) const { return glm::vec3(m_repr); }
    float GetRadius(void) const { return m_repr.w; }

    BoundingSphere Union(const BoundingSphere &rhs);

private:
    glm::vec4 m_repr;
};

//=======================================================================================================
// Inline implementations
//

inline BoundingSphere::BoundingSphere(glm::vec3 center, float radius)
{
    m_repr = glm::vec4(center, 1.0);
    m_repr.w = glm::vec4(radius).w;
}

} // namespace Math
