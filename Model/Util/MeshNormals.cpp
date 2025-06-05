#include "VulkanMesh.h"

//---------------------------------------------------------------------------------
// Compute normals with weighting by angle
//---------------------------------------------------------------------------------
template <class index_t>
bool ComputeNormalsWeightedByAngle(const index_t *indices, size_t nFaces, const glm::vec3 *positions, size_t nVerts,
                                   glm::vec3 *normals)
{
    auto temp = std::make_unique<glm::vec3[]>(nVerts);
    if (!temp)
    {
        return false;
    }

    glm::vec3 *vertNormals = temp.get();
    memset(vertNormals, 0, sizeof(glm::vec3) * nVerts);

    for (size_t face = 0; face < nFaces; ++face)
    {
        index_t i0 = indices[face * 3];
        index_t i1 = indices[face * 3 + 1];
        index_t i2 = indices[face * 3 + 2];

        if (i0 == index_t(-1) || i1 == index_t(-1) || i2 == index_t(-1))
            continue;

        if (i0 >= nVerts || i1 >= nVerts || i2 >= nVerts)
            return false;

        const glm::vec3 p0 = positions[i0];
        const glm::vec3 p1 = positions[i1];
        const glm::vec3 p2 = positions[i2];

        const glm::vec3 u = p1 - p0;
        const glm::vec3 v = p2 - p0;

        const glm::vec3 faceNormal = glm::normalize(glm::cross(u, v));

        // Corner 0 -> 1 - 0, 2 - 0
        const glm::vec3 a = glm::normalize(u);
        const glm::vec3 b = glm::normalize(v);
        float w0 = glm::dot(a, b);
        w0 = glm::clamp(w0, -1.0f, 1.0f);
        w0 = glm::acos(w0);

        // Corner 1 -> 2 - 1, 0 - 1
        const glm::vec3 c = glm::normalize(p2 - p1);
        const glm::vec3 d = glm::normalize(p0 - p1);
        float w1 = glm::dot(c, d);
        w1 = glm::clamp(w1, -1.0f, 1.0f);
        w1 = glm::acos(w1);

        // Corner 2 -> 0 - 2, 1 - 2
        const glm::vec3 e = glm::normalize(p0 - p2);
        const glm::vec3 f = glm::normalize(p1 - p2);
        float w2 = glm::dot(e, f);
        w2 = glm::clamp(w2, -1.0f, 1.0f);
        w2 = glm::acos(w2);

        vertNormals[i0] = faceNormal * w0 + vertNormals[i0];
        vertNormals[i1] = faceNormal * w1 + vertNormals[i1];
        vertNormals[i2] = faceNormal * w2 + vertNormals[i2];
    }

    // Store results

    for (size_t vert = 0; vert < nVerts; ++vert)
    {
        const glm::vec3 n = glm::normalize(vertNormals[vert]);
        normals[vert] = n;
    }

    return true;
}

bool ComputeNormals(const uint16_t *indices, size_t nFaces, const glm::vec3 *positions, size_t nVerts,
                    glm::vec3 *normals) noexcept
{
    if (!indices || !positions || !nFaces || !nVerts || !normals)
        return false;

    if (nVerts >= UINT16_MAX)
        return false;

    if ((uint64_t(nFaces) * 3) >= UINT32_MAX)
        return false;

    const bool cw = false; // TODO: wind_cw

    return ComputeNormalsWeightedByAngle<uint16_t>(indices, nFaces, positions, nVerts, normals);
}

bool ComputeNormals(const uint32_t *indices, size_t nFaces, const glm::vec3 *positions, size_t nVerts,
                    glm::vec3 *normals) noexcept
{
    if (!indices || !positions || !nFaces || !nVerts || !normals)
        return false;

    if (nVerts >= UINT32_MAX)
        return false;

    if ((uint64_t(nFaces) * 3) >= UINT32_MAX)
        return false;

    const bool cw = false; // TODO: wind_cw

    return ComputeNormalsWeightedByAngle<uint32_t>(indices, nFaces, positions, nVerts, normals);
}