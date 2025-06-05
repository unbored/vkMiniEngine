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

//#include "VectorMath.h"
#include "Transform.h"

namespace Math
{
    class AxisAlignedBox
    {
    public:
        AxisAlignedBox() : m_min(FLT_MAX, FLT_MAX, FLT_MAX), m_max(-FLT_MAX, -FLT_MAX, -FLT_MAX) {}
        AxisAlignedBox(EZeroTag) : m_min(FLT_MAX, FLT_MAX, FLT_MAX), m_max(-FLT_MAX, -FLT_MAX, -FLT_MAX) {}
        AxisAlignedBox( glm::vec3 min, glm::vec3 max ) : m_min(min), m_max(max) {}

        void AddPoint( glm::vec3 point )
        {
            m_min = glm::min(point, m_min);
            m_max = glm::max(point, m_max);
        }

        void AddBoundingBox( const AxisAlignedBox& box )
        {
            AddPoint(box.m_min);
            AddPoint(box.m_max);
        }

        AxisAlignedBox Union( const AxisAlignedBox& box )
        {
            return AxisAlignedBox(glm::min(m_min, box.m_min), glm::max(m_max, box.m_max));
        }

        glm::vec3 GetMin() const { return m_min; }
        glm::vec3 GetMax() const { return m_max; }
        glm::vec3 GetCenter() const { return (m_min + m_max) * 0.5f; }
        glm::vec3 GetDimensions() const { return glm::max(m_max - m_min, glm::vec3(0.0)); }

    private:

        glm::vec3 m_min;
        glm::vec3 m_max;
    };

    class OrientedBox
    {
    public:
        OrientedBox() {}

        OrientedBox( const AxisAlignedBox& box )
        {
            glm::vec3 v = box.GetMax() - box.GetMin();
            glm::mat3 s(1.0);
            s[0][0] = v[0];
            s[1][1] = v[1];
            s[2][2] = v[2];
            m_repr.SetBasis(s);
            m_repr.SetTranslation(box.GetMin());
        }

        friend OrientedBox operator* (const AffineTransform& xform, const OrientedBox& obb )
        {
            OrientedBox ret;
            ret.m_repr = xform * obb.m_repr;
            return ret;
        }

        glm::vec3 GetDimensions() const { return m_repr.GetX() + m_repr.GetY() + m_repr.GetZ(); }
        glm::vec3 GetCenter() const { return m_repr.GetTranslation() + GetDimensions() * 0.5f; }

    private:
        AffineTransform m_repr;
    };

    INLINE OrientedBox operator* (const UniformTransform& xform, const OrientedBox& obb )
    {
        return AffineTransform(xform) * obb;
    }

    INLINE OrientedBox operator* (const UniformTransform& xform, const AxisAlignedBox& aabb )
    {
        return AffineTransform(xform) * OrientedBox(aabb);
    }

} // namespace Math
