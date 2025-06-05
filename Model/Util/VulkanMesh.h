#pragma once

#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>

#include <memory>
#include <vector>

size_t BytesPerElement(vk::Format fmt) noexcept;

void ComputeInputLayout(const std::vector<vk::VertexInputAttributeDescription> &desc, uint32_t *offsets,
                        uint32_t *strides);

class VBReader
{
public:
    VBReader() noexcept(false);
    VBReader(VBReader &&) noexcept;
    VBReader &operator=(VBReader &&) noexcept;

    VBReader(VBReader const &) = delete;
    VBReader &operator=(VBReader const &) = delete;

    ~VBReader();

    bool Initialize(const std::vector<vk::VertexInputAttributeDescription> &desc);

    bool AddStream(void *vb, size_t nVerts, size_t binding, size_t stride = 0);

    bool Read(float *buffer, uint32_t location, size_t count) const;
    bool Read(glm::vec2 *buffer, uint32_t location, size_t count) const;
    bool Read(glm::vec3 *buffer, uint32_t location, size_t count) const;
    bool Read(glm::vec4 *buffer, uint32_t location, size_t count) const;

    void Release() noexcept;

private:
    // Private implementation.
    class Impl;

    std::unique_ptr<Impl> pImpl;
};

class VBWriter
{
public:
    VBWriter() noexcept(false);
    VBWriter(VBWriter &&) noexcept;
    VBWriter &operator=(VBWriter &&) noexcept;

    VBWriter(VBWriter const &) = delete;
    VBWriter &operator=(VBWriter const &) = delete;

    ~VBWriter();

    bool Initialize(const std::vector<vk::VertexInputAttributeDescription> &desc);

    bool AddStream(void *vb, size_t nVerts, size_t binding, size_t stride = 0);

    bool Write(float *buffer, uint32_t location, size_t count) const;
    bool Write(glm::vec2 *buffer, uint32_t location, size_t count) const;
    bool Write(glm::vec3 *buffer, uint32_t location, size_t count) const;
    bool Write(glm::vec4 *buffer, uint32_t location, size_t count) const;

    void Release() noexcept;

private:
    // Private implementation.
    class Impl;

    std::unique_ptr<Impl> pImpl;
};

bool ComputeNormals(const uint16_t *indices, size_t nFaces, const glm::vec3 *positions, size_t nVerts,
                    glm::vec3 *normals) noexcept;

bool ComputeNormals(const uint32_t *indices, size_t nFaces, const glm::vec3 *positions, size_t nVerts,
                    glm::vec3 *normals) noexcept;

bool ComputeTangentFrame(const uint16_t *indices, size_t nFaces, const glm::vec3 *positions, const glm::vec3 *normals,
                         const glm::vec2 *texcoords, size_t nVerts, glm::vec4 *tangents) noexcept;

bool ComputeTangentFrame(const uint32_t *indices, size_t nFaces, const glm::vec3 *positions, const glm::vec3 *normals,
                         const glm::vec2 *texcoords, size_t nVerts, glm::vec4 *tangents) noexcept;
