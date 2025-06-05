#include "VulkanMesh.h"
#include <glm/geometric.hpp>

template <class index_t>
bool ComputeTangentFrameImpl(const index_t *indices, size_t nFaces, const glm::vec3 *positions,
                             const glm::vec3 *normals, const glm::vec2 *texcoords, size_t nVerts,
                             glm::vec4 *tangents4) noexcept
{
    if (!indices || !nFaces || !positions || !normals || !texcoords || !nVerts)
        return false;

    if (nVerts >= index_t(-1))
        return false;

    if ((uint64_t(nFaces) * 3) >= UINT32_MAX)
        return false;

    static constexpr float EPSILON = 0.0001f;
    static constexpr glm::vec4 s_flips(1.f, -1.f, -1.f, 1.f);

    auto temp = std::make_unique<glm::vec4[]>(nVerts * 2);
    if (!temp)
    {
        return false;
    }
    memset(temp.get(), 0, sizeof(glm::vec4) * nVerts * 2);

    glm::vec4 *tangent1 = temp.get();
    glm::vec4 *tangent2 = temp.get() + nVerts;

    for (size_t face = 0; face < nFaces; ++face)
    {
        index_t i0 = indices[face * 3];
        index_t i1 = indices[face * 3 + 1];
        index_t i2 = indices[face * 3 + 2];

        if (i0 == index_t(-1) || i1 == index_t(-1) || i2 == index_t(-1))
            continue;

        if (i0 >= nVerts || i1 >= nVerts || i2 >= nVerts)
            return false;

        const glm::vec2 t0 = texcoords[i0];
        const glm::vec2 t1 = texcoords[i1];
        const glm::vec2 t2 = texcoords[i2];

        glm::vec2 t10 = t1 - t0;
        glm::vec2 t20 = t2 - t0;
        glm::vec4 s = glm::vec4(t10.x, t20.x, t10.y, t20.y);

        glm::vec4 tmp = s;

        float d = tmp.x * tmp.w - tmp.z * tmp.y;
        d = (fabsf(d) <= EPSILON) ? 1.f : (1.f / d);
        s = s * d;
        s = s * s_flips;

        glm::mat4 m0; // glm mat4 is column major
        m0[0] = glm::vec4(s[3], s[2], 0.0, 0.0);
        m0[1] = glm::vec4(s[1], s[0], 0.0, 0.0);
        m0[2] = m0[3] = glm::vec4(0.0);

        const glm::vec4 p0 = glm::vec4(positions[i0], 0.0);
        const glm::vec4 p1 = glm::vec4(positions[i1], 0.0);
        glm::vec4 p2 = glm::vec4(positions[i2], 0.0);

        glm::mat4 m1;
        m1[0] = p1 - p0;
        m1[1] = p2 - p0;
        m1[2] = m1[3] = glm::vec4(0.0);

        const glm::mat4 uv = m1 * m0; // transposed matrix reverse order when multiply

        tangent1[i0] = tangent1[i0] + uv[0];
        tangent1[i1] = tangent1[i1] + uv[0];
        tangent1[i2] = tangent1[i2] + uv[0];

        tangent2[i0] = tangent2[i0] + uv[1];
        tangent2[i1] = tangent2[i1] + uv[1];
        tangent2[i2] = tangent2[i2] + uv[1];
    }

    for (size_t j = 0; j < nVerts; ++j)
    {
        // Gram-Schmidt orthonormalization
        glm::vec3 b0 = normals[j];
        b0 = glm::normalize(b0);

        const glm::vec3 tan1 = tangent1[j];
        glm::vec3 b1 = tan1 - glm::dot(b0, tan1) * b0;
        b1 = glm::normalize(b1);

        const glm::vec3 tan2 = tangent2[j];
        glm::vec3 b2 = tan2 - glm::dot(b0, tan2) * b0 - glm::dot(b1, tan2) * b1;
        b2 = glm::normalize(b2);

        // handle degenerate vectors
        const float len1 = glm::length(b1);
        const float len2 = glm::length(b2);

        if ((len1 <= EPSILON) || (len2 <= EPSILON))
        {
            if (len1 > 0.5f)
            {
                // Reset bi-tangent from tangent and normal
                b2 = glm::cross(b0, b1);
            }
            else if (len2 > 0.5f)
            {
                // Reset tangent from bi-tangent and normal
                b1 = glm::cross(b2, b0);
            }
            else
            {
                // Reset both tangent and bi-tangent from normal
                glm::vec3 axis;

                const float d0 = fabsf(glm::dot(glm::vec3(1.0, 0.0, 0.0), b0));
                const float d1 = fabsf(glm::dot(glm::vec3(0.0, 1.0, 0.0), b0));
                const float d2 = fabsf(glm::dot(glm::vec3(0.0, 0.0, 1.0), b0));
                if (d0 < d1)
                {
                    axis = (d0 < d2) ? glm::vec3(1.0, 0.0, 0.0) : glm::vec3(0.0, 0.0, 1.0);
                }
                else if (d1 < d2)
                {
                    axis = glm::vec3(0.0, 1.0, 0.0);
                }
                else
                {
                    axis = glm::vec3(0.0, 0.0, 2.0);
                }

                b1 = glm::cross(b0, axis);
                b2 = glm::cross(b0, b1);
            }
        }

        if (tangents4)
        {
            glm::vec3 bi = glm::cross(b0, tan1);
            const float w = glm::dot(bi, tan2) < 0 ? -1.f : 1.f;

            tangents4[j] = glm::vec4(b1, w);
        }
    }

    return true;
}

bool ComputeTangentFrame(const uint16_t *indices, size_t nFaces, const glm::vec3 *positions, const glm::vec3 *normals,
                         const glm::vec2 *texcoords, size_t nVerts, glm::vec4 *tangents) noexcept
{
    if (!tangents)
    {
        return false;
    }
    return ComputeTangentFrameImpl<uint16_t>(indices, nFaces, positions, normals, texcoords, nVerts, tangents);
}

bool ComputeTangentFrame(const uint32_t *indices, size_t nFaces, const glm::vec3 *positions, const glm::vec3 *normals,
                         const glm::vec2 *texcoords, size_t nVerts, glm::vec4 *tangents) noexcept
{
    if (!tangents)
    {
        return false;
    }
    return ComputeTangentFrameImpl<uint32_t>(indices, nFaces, positions, normals, texcoords, nVerts, tangents);
}