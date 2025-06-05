#pragma once

#include <glm/glm.hpp>

struct alignas(256) MeshConstants
{
    glm::mat4 World;   // Object to world
    glm::mat4 WorldIT; // Object normal to world normal. Only mat3 will be used
};

// The order of textures for PBR materials
enum
{
    kBaseColor,
    kMetallicRoughness,
    kOcclusion,
    kEmissive,
    kNormal,
    kNumTextures
};

struct alignas(256) MaterialConstants
{
    float baseColorFactor[4]; // default=[1,1,1,1]
    float emissiveFactor[3];  // default=[0,0,0]
    float normalTextureScale; // default=1
    float metallicFactor;     // default=1
    float roughnessFactor;    // default=1
    union {
        uint32_t flags;
        struct
        {
            // UV0 or UV1 for each texture
            uint32_t baseColorUV : 1;
            uint32_t metallicRoughnessUV : 1;
            uint32_t occlusionUV : 1;
            uint32_t emissiveUV : 1;
            uint32_t normalUV : 1;

            // Three special modes
            uint32_t twoSided : 1;
            uint32_t alphaTest : 1;
            uint32_t alphaBlend : 1;

            uint32_t _pad : 8;

            uint32_t alphaRef : 16; // half float
        };
    };
};

// we should consider aligning, thus
// vec3 should be avoid or carefully packed
struct alignas(256) GlobalConstants
{
    glm::mat4 ViewProjMatrix;
    glm::mat4 SunShadowMatrix;
    glm::vec4 CameraPos;    // only vec3 will be used
    glm::vec4 SunDirection; // only vec3 will be used
    glm::vec3 SunIntensity; // the last vec3 will be aligned with next float
    float IBLRange;
    float IBLBias;
};
