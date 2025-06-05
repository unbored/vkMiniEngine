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

#include "BoundingSphere.h"
#include <glm/gtc/quaternion.hpp>

namespace Math
{
    // Orthonormal basis (just rotation via quaternion) and translation
    class OrthogonalTransform;

    // A 3x4 matrix that allows for asymmetric skew and scale
    class AffineTransform;

    // Uniform scale and translation that can be compactly represented in a vec4
    class ScaleAndTranslation;

    // Uniform scale, rotation (quaternion), and translation that fits in two vec4s
    class UniformTransform;

    // This transform strictly prohibits non-uniform scale.  Scale itself is barely tolerated.
    class OrthogonalTransform
    {
    public:
        INLINE OrthogonalTransform() : 
            m_rotation(glm::quat(1.0, glm::vec3(0))), m_translation(glm::vec3(0)) {}
        INLINE OrthogonalTransform( glm::quat rotate ) : 
            m_rotation(rotate), m_translation(glm::vec3(0)) {}
        INLINE OrthogonalTransform( glm::vec3 translate ) : 
            m_rotation(glm::quat(1.0, glm::vec3(0))), m_translation(translate) {}
        INLINE OrthogonalTransform( glm::quat rotate, glm::vec3 translate ) : 
            m_rotation(rotate), m_translation(translate) {}
        INLINE OrthogonalTransform( const glm::mat3& mat ) : 
            m_rotation(mat), m_translation(glm::vec3(0)) {}
        INLINE OrthogonalTransform( const glm::mat3& mat, glm::vec3 translate ) : 
            m_rotation(mat), m_translation(translate) {}
        INLINE OrthogonalTransform( EIdentityTag ) : 
            m_rotation(glm::quat(1.0, glm::vec3(0))), m_translation(glm::vec3(0)) {}
        INLINE explicit OrthogonalTransform( const glm::mat4& mat ) { *this = OrthogonalTransform( glm::mat3(mat), mat[3] ); }
        INLINE void SetRotation( glm::quat q ) { m_rotation = q; }
        INLINE void SetTranslation( glm::vec3 v ) { m_translation = v; }

        INLINE glm::quat GetRotation() const { return m_rotation; }
        INLINE glm::vec3 GetTranslation() const { return m_translation; }

        static INLINE OrthogonalTransform MakeXRotation( float angle ) { return OrthogonalTransform(glm::quat(glm::vec3(angle, 0.0, 0.0))); }
        static INLINE OrthogonalTransform MakeYRotation( float angle ) { return OrthogonalTransform(glm::quat(glm::vec3(0.0, angle, 0.0))); }
        static INLINE OrthogonalTransform MakeZRotation( float angle ) { return OrthogonalTransform(glm::quat(glm::vec3(0.0, 0.0, angle))); }
        static INLINE OrthogonalTransform MakeTranslation( glm::vec3 translate ) { return OrthogonalTransform(translate); }

        INLINE glm::vec3 operator* ( glm::vec3 vec ) const { return m_rotation * vec + m_translation; }
        INLINE glm::vec4 operator* ( glm::vec4 vec ) const { return
            glm::vec4(glm::vec3(m_rotation * vec), 0.0) +
            glm::vec4(m_translation, 1.0) * vec.w;
        }
        INLINE BoundingSphere operator* ( BoundingSphere sphere ) const {
            return BoundingSphere(*this * sphere.GetCenter(), sphere.GetRadius());
        }

        INLINE OrthogonalTransform operator* ( const OrthogonalTransform& xform ) const {
            return OrthogonalTransform( m_rotation * xform.m_rotation, m_rotation * xform.m_translation + m_translation );
        }

        INLINE OrthogonalTransform operator~ () const { glm::quat invertedRotation = glm::conjugate(m_rotation);
            return OrthogonalTransform( invertedRotation, invertedRotation * -m_translation );
        }

    private:

        glm::quat m_rotation;
        glm::vec3 m_translation;
    };

    //
    // A transform that lacks rotation and has only uniform scale.
    //
    class ScaleAndTranslation
    {
    public:
        INLINE ScaleAndTranslation()
        {}
        INLINE ScaleAndTranslation( EIdentityTag )
            : m_repr(glm::vec4(0.0, 0.0, 0.0, 1.0)) {}
        INLINE ScaleAndTranslation(float tx, float ty, float tz, float scale)
            : m_repr(tx, ty, tz, scale) {}
        INLINE ScaleAndTranslation(glm::vec3 translate, float scale)
        {
            m_repr = glm::vec4(translate, 1.0);
            m_repr.w = scale;
        }
        INLINE explicit ScaleAndTranslation(const glm::vec4& v)
            : m_repr(v) {}

        INLINE void SetScale(float s) { m_repr.w = s; }
        INLINE void SetTranslation(glm::vec3 t) { m_repr = glm::vec4(t, m_repr.w); }

        INLINE float GetScale() const { return m_repr.w; }
        INLINE glm::vec3 GetTranslation() const { return (glm::vec3)m_repr; }

        INLINE BoundingSphere operator*(const BoundingSphere& sphere) const
        {
            glm::vec4 scaledSphere = (glm::vec4)sphere * (glm::vec4)GetScale();
            glm::vec4 translation = m_repr;
            translation.w = 0;
            return BoundingSphere(scaledSphere + translation);
        }

    private:
        glm::vec4 m_repr;
    };

    //
    // This transform allows for rotation, translation, and uniform scale
    // 
    class UniformTransform
    {
    public:
        INLINE UniformTransform()
        {}
        INLINE UniformTransform( EIdentityTag )
            : m_rotation(glm::quat(1.0, glm::vec3(0))), m_translationScale(kIdentity) {}
        INLINE UniformTransform(glm::quat rotation, ScaleAndTranslation transScale)
            : m_rotation(rotation), m_translationScale(transScale)
        {}
        INLINE UniformTransform(glm::quat rotation, float scale, glm::vec3 translation)
            : m_rotation(rotation), m_translationScale(translation, scale)
        {}

        INLINE void SetRotation(glm::quat r) { m_rotation = r; }
        INLINE void SetScale(float s) { m_translationScale.SetScale(s); }
        INLINE void SetTranslation(glm::vec3 t) { m_translationScale.SetTranslation(t); }


        INLINE glm::quat GetRotation() const { return m_rotation; }
        INLINE float GetScale() const { return m_translationScale.GetScale(); }
        INLINE glm::vec3 GetTranslation() const { return m_translationScale.GetTranslation(); }


        INLINE glm::vec3 operator*( glm::vec3 vec ) const
        {
            return m_rotation * (vec * glm::vec3(m_translationScale.GetScale())) + m_translationScale.GetTranslation();
        }

        INLINE BoundingSphere operator*( BoundingSphere sphere ) const
        {
            return BoundingSphere(*this * sphere.GetCenter(), GetScale() * sphere.GetRadius() );
        }

    private:
        glm::quat m_rotation;
        ScaleAndTranslation m_translationScale;
    };

    // A AffineTransform is a 3x4 matrix with an implicit 4th row = [0,0,0,1].  This is used to perform a change of
    // basis on 3D points.  An affine transformation does not have to have orthonormal basis vectors.
    class AffineTransform
    {
    public:
        INLINE AffineTransform()
            {}
        INLINE AffineTransform( glm::vec3 x, glm::vec3 y, glm::vec3 z, glm::vec3 w )
            : m_basis(x, y, z), m_translation(w) {}
        INLINE AffineTransform( glm::vec3 translate )
            : m_basis(glm::mat3(1.0)), m_translation(translate) {}
        INLINE AffineTransform( const glm::mat3& mat, glm::vec3 translate = glm::vec3(0) )
            : m_basis(mat), m_translation(translate) {}
        INLINE AffineTransform( glm::quat rot, glm::vec3 translate = glm::vec3(0) )
            : m_basis(rot), m_translation(translate) {}
        INLINE AffineTransform( const OrthogonalTransform& xform )
            : m_basis(xform.GetRotation()), m_translation(xform.GetTranslation()) {}
        INLINE AffineTransform( const UniformTransform& xform )
        {
            glm::mat3 rotate(xform.GetRotation());
            glm::vec3 scale(xform.GetScale());
            rotate[0][0] *= scale[0];
            rotate[1][1] *= scale[1];
            rotate[2][2] *= scale[2];
            m_basis = rotate;
            m_translation = xform.GetTranslation();
        }
        INLINE AffineTransform( EIdentityTag )
            : m_basis(glm::mat3(1.0)), m_translation(glm::vec3(0)) {}
        INLINE explicit AffineTransform( const glm::mat4& mat )
            : m_basis(mat), m_translation(mat[3]) {}

        INLINE operator glm::mat4() const { glm::mat4 ret = m_basis; ret[3] = glm::vec4(m_translation, 1.0); return ret; }

        INLINE void SetX(glm::vec3 x) { m_basis[0] = x; }
        INLINE void SetY(glm::vec3 y) { m_basis[1] = y; }
        INLINE void SetZ(glm::vec3 z) { m_basis[2] = z; }
        INLINE void SetTranslation(glm::vec3 w) { m_translation = w; }
        INLINE void SetBasis(const glm::mat3& basis) { m_basis = basis; }

        INLINE glm::vec3 GetX() const { return m_basis[0]; }
        INLINE glm::vec3 GetY() const { return m_basis[1]; }
        INLINE glm::vec3 GetZ() const { return m_basis[2]; }
        INLINE glm::vec3 GetTranslation() const { return m_translation; }
        INLINE const glm::mat3& GetBasis() const { return (const glm::mat3&)*this; }

        static INLINE AffineTransform MakeXRotation( float angle ) { return AffineTransform(MakeXRotation(angle)); }
        static INLINE AffineTransform MakeYRotation( float angle ) { return AffineTransform(MakeYRotation(angle)); }
        static INLINE AffineTransform MakeZRotation( float angle ) { return AffineTransform(MakeZRotation(angle)); }
        static INLINE AffineTransform MakeScale( float scale ) { return AffineTransform(glm::mat3(scale)); }
        static INLINE AffineTransform MakeScale(glm::vec3 scale) { glm::mat3 ret(1.0); ret[0][0] = scale[0]; ret[1][1] = scale[1]; ret[2][2] = scale[2]; return AffineTransform(ret); }
        static INLINE AffineTransform MakeTranslation( glm::vec3 translate ) { return AffineTransform(translate); }

        INLINE glm::vec3 operator* ( glm::vec3 vec ) const { return m_basis * vec + m_translation; }
        INLINE AffineTransform operator* ( const AffineTransform& mat ) const {
            return AffineTransform( m_basis * mat.m_basis, *this * mat.GetTranslation() );
        }

    private:
        glm::mat3 m_basis;
        glm::vec3 m_translation;
    };
}
