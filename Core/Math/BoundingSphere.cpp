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

#include "BoundingSphere.h"

using namespace Math;

BoundingSphere BoundingSphere::Union( const BoundingSphere& rhs )
{
    float radA = GetRadius();
    if (radA == 0.0f)
        return rhs;

    float radB = rhs.GetRadius();
    if (radB == 0.0f)
        return *this;

    glm::vec3 diff = GetCenter() - rhs.GetCenter();
    float dist = glm::length(diff);

    // Safe normalize vector between sphere centers
    diff = dist < 1e-6f ? CreateXUnitVector() : diff / dist;

    glm::vec3 extremeA = GetCenter() + diff * glm::max(radA, radB - dist);
    glm::vec3 extremeB = rhs.GetCenter() - diff * glm::max(radB, radA - dist);

    return BoundingSphere((extremeA + extremeB) * 0.5f, glm::length(extremeA - extremeB) * 0.5f);
}
