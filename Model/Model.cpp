#include "Model.h"
#include <Math/BoundingSphere.h>
#include <vulkan/vulkan_handles.hpp>

using namespace Math;
using namespace Renderer;

void Model::Destroy()
{
    m_BoundingSphere = BoundingSphere(kZero);
    m_DataBuffer.Destroy();
    m_MaterialConstants.Destroy();
    m_NumNodes = 0;
    m_NumMeshes = 0;
    m_MeshData = nullptr;
    m_SceneGraph = nullptr;
}

void Model::Render(MeshSorter &sorter, const UniformBuffer &meshConstants, const ScaleAndTranslation sphereTransforms[],
                   const Joint *skeleton) const
{
    // Pointer to current mesh
    const uint8_t *pMesh = m_MeshData.get();

    const Frustum &frustum = sorter.GetViewFrustum();
    const AffineTransform &viewMat = AffineTransform(sorter.GetViewMatrix());

    for (uint32_t i = 0; i < m_NumMeshes; ++i)
    {
        const Mesh &mesh = *(const Mesh *)pMesh;

        const ScaleAndTranslation &sphereXform = sphereTransforms[mesh.meshUB];
        BoundingSphere sphereLS(mesh.bounds);                                                           // local space
        BoundingSphere sphereWS = sphereXform * sphereLS;                                               // world space
        BoundingSphere sphereVS = BoundingSphere(viewMat * sphereWS.GetCenter(), sphereWS.GetRadius()); // view space

        // only intersections will be add
        if (frustum.IntersectSphere(sphereVS))
        {
            float distance = -sphereVS.GetCenter().z - sphereVS.GetRadius();
            vk::DescriptorBufferInfo meshInfo;
            meshInfo.buffer = meshConstants.GetBuffer();
            meshInfo.offset = mesh.meshUB * sizeof(MeshConstants);
            meshInfo.range = sizeof(MeshConstants);
            vk::DescriptorBufferInfo matInfo;
            matInfo.buffer = m_MaterialConstants.GetBuffer();
            matInfo.offset = mesh.materialUB * sizeof(MaterialConstants);
            matInfo.range = sizeof(MaterialConstants);
            sorter.AddMesh(mesh, distance, meshInfo, matInfo, m_DataBuffer, skeleton);
        }

        pMesh += sizeof(Mesh) + (mesh.numDraws - 1) * sizeof(Mesh::Draw);
    }
}

void ModelInstance::Render(MeshSorter &sorter) const
{
    if (m_Model != nullptr)
    {
        // const Frustum& frustum = sorter.GetWorldFrustum();
        m_Model->Render(sorter, m_MeshConstantsGPU, (const ScaleAndTranslation *)m_BoundingSphereTransforms.get(),
                        m_Skeleton.get());
    }
}

ModelInstance::ModelInstance(std::shared_ptr<const Model> sourceModel) { *this = sourceModel; }

ModelInstance::ModelInstance(const ModelInstance &modelInstance) : ModelInstance(modelInstance.m_Model) {}

ModelInstance &ModelInstance::operator=(std::shared_ptr<const Model> sourceModel)
{
    if (this->m_Model == sourceModel)
    {
        return *this;
    }
    m_Model = sourceModel;
    m_Locator = UniformTransform(kIdentity);

    static_assert((alignof(MeshConstants) & 255) == 0, "Uniform Buffers needs 256 byte alignment");
    if (sourceModel == nullptr)
    {
        m_MeshConstantsCPU.Destroy();
        m_MeshConstantsGPU.Destroy();
        m_BoundingSphereTransforms = nullptr;
        m_AnimGraph = nullptr;
        m_AnimState.clear();
        m_Skeleton = nullptr;
    }
    else
    {
        m_MeshConstantsCPU.Create(sourceModel->m_NumNodes * sizeof(MeshConstants));
        m_MeshConstantsGPU.Create(sourceModel->m_NumNodes * sizeof(MeshConstants));
        m_BoundingSphereTransforms.reset(new glm::vec4[sourceModel->m_NumNodes]);
        m_Skeleton.reset(new Joint[sourceModel->m_NumJoints]);

        if (sourceModel->m_NumAnimations > 0)
        {
            m_AnimGraph.reset(new GraphNode[sourceModel->m_NumNodes]);
            std::memcpy(m_AnimGraph.get(), sourceModel->m_SceneGraph.get(),
                        sourceModel->m_NumNodes * sizeof(GraphNode));
            m_AnimState.resize(sourceModel->m_NumAnimations);
        }
        else
        {
            m_AnimGraph = nullptr;
            m_AnimState.clear();
        }
    }
    return *this;
}

void ModelInstance::Update(GraphicsContext &gfxContext, float deltaTime)
{
    if (m_Model == nullptr)
        return;

    static const size_t kMaxStackDepth = 32;

    size_t stackIdx = 0;
    glm::mat4 matrixStack[kMaxStackDepth];
    glm::mat4 ParentMatrix = glm::mat4((AffineTransform)m_Locator);

    ScaleAndTranslation *boundingSphereTransforms = (ScaleAndTranslation *)m_BoundingSphereTransforms.get();
    MeshConstants *cb = (MeshConstants *)m_MeshConstantsCPU.Map();

    if (m_AnimGraph)
    {
        // UpdateAnimations(deltaTime);

        for (uint32_t i = 0; i < m_Model->m_NumNodes; ++i)
        {
            GraphNode &node = m_AnimGraph[i];

            // Regenerate the 3x3 matrix if it has scale or rotation
            if (node.staleMatrix)
            {
                node.staleMatrix = false;
                node.xform = glm::mat3(node.rotation) * glm::mat3(glm::scale(glm::mat4(1.0), node.scale));
            }
        }
    }

    const GraphNode *sceneGraph = m_AnimGraph ? m_AnimGraph.get() : m_Model->m_SceneGraph.get();

    // Traverse the scene graph in depth first order.  This is the same as linear order
    // for how the nodes are stored in memory.  Uses a matrix stack instead of recursion.
    for (const GraphNode *Node = sceneGraph;; ++Node)
    {
        glm::mat4 xform = Node->xform;
        if (!Node->skeletonRoot)
            xform = ParentMatrix * xform;

        // Concatenate the transform with the parent's matrix and update the matrix list
        {
            // Scoped so that I don't forget that I'm pointing to write-combined memory and
            // should not read from it.
            MeshConstants &cbv = cb[Node->matrixIdx];
            cbv.World = xform;
            cbv.WorldIT = glm::transpose(glm::inverse(glm::mat3(xform)));

            float scaleXSqr = LengthSquare(glm::vec3(xform[0]));
            float scaleYSqr = LengthSquare(glm::vec3(xform[1]));
            float scaleZSqr = LengthSquare(glm::vec3(xform[2]));
            float sphereScale = std::sqrt(std::max(std::max(scaleXSqr, scaleYSqr), scaleZSqr));
            boundingSphereTransforms[Node->matrixIdx] = ScaleAndTranslation(glm::vec3(xform[3]), sphereScale);
        }

        // If the next node will be a descendent, replace the parent matrix with our new matrix
        if (Node->hasChildren)
        {
            // ...but if we have siblings, make sure to backup our current parent matrix on the stack
            if (Node->hasSibling)
            {
                ASSERT(stackIdx < kMaxStackDepth, "Overflowed the matrix stack");
                matrixStack[stackIdx++] = ParentMatrix;
            }
            ParentMatrix = xform;
        }
        else if (!Node->hasSibling)
        {
            // There are no more siblings.  If the stack is empty, we are done.  Otherwise, pop
            // a matrix off the stack and continue.
            if (stackIdx == 0)
                break;

            ParentMatrix = matrixStack[--stackIdx];
        }
    }

    // Update skeletal joints
    for (uint32_t i = 0; i < m_Model->m_NumJoints; ++i)
    {
        Joint &joint = m_Skeleton[i];
        joint.posXform = cb[m_Model->m_JointIndices[i]].World * m_Model->m_JointIBMs[i];
        joint.nrmXform = glm::transpose(glm::inverse(glm::mat3(joint.posXform)));
    }

    m_MeshConstantsCPU.Unmap();

    // gfxContext.TransitionResource(m_MeshConstantsGPU, D3D12_RESOURCE_STATE_COPY_DEST, true);
    // gfxContext.GetCommandList()->CopyBufferRegion(m_MeshConstantsGPU.GetResource(), 0,
    // m_MeshConstantsCPU.GetResource(),
    //                                               0, m_MeshConstantsCPU.GetBufferSize());
    // gfxContext.TransitionResource(m_MeshConstantsGPU, D3D12_RESOURCE_STATE_GENERIC_READ);
    gfxContext.InitializeBuffer(m_MeshConstantsGPU, m_MeshConstantsCPU, 0);
}

void ModelInstance::Resize(float newRadius)
{
    if (m_Model == nullptr)
        return;

    m_Locator.SetScale(newRadius / m_Model->m_BoundingSphere.GetRadius());
}

glm::vec3 ModelInstance::GetCenter() const
{
    if (m_Model == nullptr)
        return glm::vec3(kOrigin);

    return m_Locator * m_Model->m_BoundingSphere.GetCenter();
}

float ModelInstance::GetRadius() const
{
    if (m_Model == nullptr)
        return float(kZero);

    return m_Locator.GetScale() * m_Model->m_BoundingSphere.GetRadius();
}

Math::BoundingSphere ModelInstance::GetBoundingSphere() const
{
    if (m_Model == nullptr)
        return BoundingSphere(kZero);

    return m_Locator * m_Model->m_BoundingSphere;
}

Math::OrientedBox ModelInstance::GetBoundingBox() const
{
    if (m_Model == nullptr)
        return AxisAlignedBox(glm::vec3(kZero), glm::vec3(kZero));

    return m_Locator * m_Model->m_BoundingBox;
}