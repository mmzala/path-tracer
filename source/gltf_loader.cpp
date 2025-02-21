#include "gltf_loader.hpp"
#include <fastgltf/tools.hpp>
#include <spdlog/spdlog.h>
#include <glm/gtc/type_ptr.hpp>

void ProcessMesh(const fastgltf::Asset& gltf, const fastgltf::Mesh& gltfMesh, std::vector<Model::Vertex>& vertices, std::vector<uint32_t>& indices)
{
    for (auto& primitive : gltfMesh.primitives)
    {
        size_t initialVertex = vertices.size();

        if (primitive.indicesAccessor.has_value())
        {
            const fastgltf::Accessor& indexAccessor = gltf.accessors[primitive.indicesAccessor.value()];
            indices.reserve(indices.size() + indexAccessor.count);

            fastgltf::iterateAccessor<uint32_t>(gltf, indexAccessor,
                [&](uint32_t idx)
                {
                    indices.push_back(idx + initialVertex);
                });
        }
        else
        {
            spdlog::error("[GLTF] Primitive on mesh \"{}\" doesn't have any indices!", gltfMesh.name);
        }

        // Load positions
        {
            const fastgltf::Accessor& positionAccessor = gltf.accessors[primitive.findAttribute("POSITION")->accessorIndex];
            vertices.resize(vertices.size() + positionAccessor.count);

            fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(gltf, positionAccessor,
                [&](fastgltf::math::fvec3 position, size_t index)
                {
                    Model::Vertex vertex;
                    vertex.position = glm::vec3(position.x(), position.y(), position.z());
                    vertices[initialVertex + index] = vertex;
                });
        }
    }
}

std::vector<Node> ProcessNodes(const fastgltf::Asset& gltf)
{
    std::vector<Node> nodes {};
    nodes.reserve(gltf.nodes.size());

    for (const fastgltf::Node& gltfNode : gltf.nodes)
    {
        Node node = nodes.emplace_back();

        fastgltf::math::fmat4x4 gltfTransform = fastgltf::getTransformMatrix(gltfNode);
        node.localMatrix = glm::make_mat4(gltfTransform.data());

        if (node.meshIndex.has_value())
        {
            node.meshIndex = node.meshIndex.value();
        }
    }

    // Run loop again to set up hierarchy ( could be done recursively (: )
    for (uint32_t i = 0; i < nodes.size(); ++i)
    {
        const fastgltf::Node& gltfNode = gltf.nodes[i];
        Node& node = nodes[i];

        // Since we have the same order in our own vector, we can use the same index to assign parents
        for (const auto& gltfNodeChildIndex : gltfNode.children)
        {
            node.parent = &nodes[gltfNodeChildIndex];
        }
    }

    return nodes;
}

glm::mat4 Node::GetWorldMatrix() const
{
    glm::mat4 matrix = localMatrix;
    const Node* p = parent;

    while (p)
    {
        matrix = p->localMatrix * matrix;
        p = p->parent;
    }

    return matrix;
}

GLTFLoader::GLTFLoader(const std::shared_ptr<VulkanContext>& vulkanContext)
    : _vulkanContext(vulkanContext)
{
}

std::shared_ptr<Model> GLTFLoader::LoadFromFile(std::string_view path)
{
    spdlog::info("[FILE] Loading GLTF file {}", path);

    fastgltf::GltfFileStream fileStream { path };

    if (!fileStream.isOpen())
    {
        spdlog::error("[FILE] Failed to open file from path: {}", path);
        return nullptr;
    }

    std::string_view directory = path.substr(0, path.find_last_of('/'));
    constexpr fastgltf::Options options = fastgltf::Options::DecomposeNodeMatrices | fastgltf::Options::LoadExternalBuffers | fastgltf::Options::LoadExternalImages;
    auto loadedGltf = _parser.loadGltf(fileStream, directory, options);

    if (!loadedGltf)
    {
        spdlog::error("[GLTF] Failed to parse GLTF file {}", path);
        return nullptr;
    }

    const fastgltf::Asset& gltf = loadedGltf.get();
    return ProcessModel(gltf);
}

std::shared_ptr<Model> GLTFLoader::ProcessModel(const fastgltf::Asset& gltf)
{
    std::vector<Model::Vertex> vertices {};
    std::vector<uint32_t> indices {};

    for (const fastgltf::Mesh& gltfMesh : gltf.meshes)
    {
        ProcessMesh(gltf, gltfMesh, vertices, indices);
    }

    // TODO: Upload to GPU friendly memory

    std::shared_ptr<Model> model = std::make_shared<Model>();
    model->verticesCount = vertices.size();
    model->indicesCount = indices.size();

    BufferCreation vertexBufferCreation {};
    vertexBufferCreation.SetName(gltf.nodes[0].name + " - Vertex Buffer")
        .SetUsageFlags(vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress)
        .SetMemoryUsage(VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE)
        .SetIsMappable(true)
        .SetSize(sizeof(Model::Vertex) * vertices.size());

    model->vertexBuffer = std::make_unique<Buffer>(vertexBufferCreation, _vulkanContext);
    memcpy(model->vertexBuffer->mappedPtr, vertices.data(), sizeof(Model::Vertex) * vertices.size());

    BufferCreation indexBufferCreation {};
    indexBufferCreation.SetName(gltf.nodes[0].name + " - Index Buffer")
        .SetUsageFlags(vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress)
        .SetMemoryUsage(VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE)
        .SetIsMappable(true)
        .SetSize(sizeof(uint32_t) * indices.size());

    model->indexBuffer = std::make_unique<Buffer>(indexBufferCreation, _vulkanContext);
    memcpy(model->indexBuffer->mappedPtr, indices.data(), sizeof(uint32_t) * indices.size());

    model->nodes = ProcessNodes(gltf);

    return model;
}
