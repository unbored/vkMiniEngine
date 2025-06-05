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
#include "Transform.h"

namespace Math
{
    class BoundingPlane
    {
    public:

        BoundingPlane() {}
        BoundingPlane( glm::vec3 normalToPlane, float distanceFromOrigin ) : m_repr(normalToPlane, distanceFromOrigin) {}
        BoundingPlane( glm::vec3 pointOnPlane, glm::vec3 normalToPlane );
        BoundingPlane( float A, float B, float C, float D ) : m_repr(A, B, C, D) {}
        BoundingPlane( const BoundingPlane& plane ) : m_repr(plane.m_repr) {}
        explicit BoundingPlane( glm::vec4 plane ) : m_repr(plane) {}

        INLINE operator glm::vec4() const { return m_repr; }

        // Returns the direction the plane is facing.  (Warning:  might not be normalized.)
        glm::vec3 GetNormal( void ) const { return glm::vec3(m_repr); }

        // Returns the point on the plane closest to the origin
        glm::vec3 GetPointOnPlane( void ) const { return -GetNormal() * m_repr.w; }

        // Distance from 3D point
        float DistanceFromPoint( glm::vec3 point ) const
        {
            return glm::dot(point, GetNormal()) + m_repr.w;
        }

        // Distance from homogeneous point
        float DistanceFromPoint(glm::vec4 point) const
        {
            return glm::dot(point, m_repr);
        }

        // Most efficient way to transform a plane.  (Involves one quaternion-vector rotation and one dot product.)
        friend BoundingPlane operator* ( const OrthogonalTransform& xform, BoundingPlane plane )
        {
            glm::vec3 normalToPlane = xform.GetRotation() * plane.GetNormal();
            float distanceFromOrigin = plane.m_repr.w - glm::dot(normalToPlane, xform.GetTranslation());
            return BoundingPlane(normalToPlane, distanceFromOrigin);
        }

        // Less efficient way to transform a plane (but handles affine transformations.)
        friend BoundingPlane operator* ( const glm::mat4& mat, BoundingPlane plane )
        {
            return BoundingPlane( glm::transpose(glm::inverse(mat)) * plane.m_repr );
        }

    private:

        glm::vec4 m_repr;
    };

    //=======================================================================================================
    // Inline implementations
    //
    inline BoundingPlane::BoundingPlane( glm::vec3 pointOnPlane, glm::vec3 normalToPlane )
    {
        // Guarantee a normal.  This constructor isn't meant to be called frequently, but if it is, we can change this.
        normalToPlane = glm::normalize(normalToPlane);	
        m_repr = glm::vec4(normalToPlane, -glm::dot(pointOnPlane, normalToPlane));
    }

    //=======================================================================================================
    // Functions operating on planes
    //
    inline BoundingPlane PlaneFromPointsCCW( glm::vec3 A, glm::vec3 B, glm::vec3 C )
    {
        return BoundingPlane( A, glm::cross(B - A, C - A) );
    }


} // namespace Math
