#include "gltf_loader.hpp"
#include <fastgltf/tools.hpp>
#include <spdlog/spdlog.h>

GLTFLoader::GLTFLoader(std::shared_ptr<VulkanContext> vulkanContext)
    : _vulkanContext(vulkanContext)
{
}

std::shared_ptr<GLTFMesh> GLTFLoader::LoadFromFile(std::string_view path)
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
    return ProcessMesh(gltf);
}

std::shared_ptr<GLTFMesh> GLTFLoader::ProcessMesh(const fastgltf::Asset& gltf)
{
    std::vector<uint32_t> indices {};
    std::vector<GLTFMesh::Vertex> vertices {};

    const fastgltf::Mesh& gltfMesh = *gltf.meshes.begin();

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
            return nullptr;
        }

        // Load positions
        {
            const fastgltf::Accessor& positionAccessor = gltf.accessors[primitive.findAttribute("POSITION")->accessorIndex];
            vertices.resize(vertices.size() + positionAccessor.count);

            fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(gltf, positionAccessor,
                [&](fastgltf::math::fvec3 position, size_t index)
                {
                    GLTFMesh::Vertex vertex;
                    vertex.position = glm::vec3(position.x(), position.y(), position.z());
                    vertices[initialVertex + index] = vertex;
                });
        }
    }

    // TODO: Upload to GPU friendly memory

    std::shared_ptr<GLTFMesh> mesh = std::make_shared<GLTFMesh>();
    mesh->verticesCount = vertices.size();
    mesh->indicesCount = indices.size();

    BufferCreation vertexBufferCreation {};
    vertexBufferCreation.SetName(gltfMesh.name + " - Vertex Buffer")
        .SetUsageFlags(vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress)
        .SetMemoryUsage(VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE)
        .SetIsMappable(true)
        .SetSize(sizeof(GLTFMesh::Vertex) * vertices.size());

    mesh->vertexBuffer = std::make_unique<Buffer>(vertexBufferCreation, _vulkanContext);
    memcpy(mesh->vertexBuffer->mappedPtr, vertices.data(), sizeof(GLTFMesh::Vertex) * vertices.size());

    BufferCreation indexBufferCreation {};
    indexBufferCreation.SetName(gltfMesh.name + " - Index Buffer")
        .SetUsageFlags(vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress)
        .SetMemoryUsage(VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE)
        .SetIsMappable(true)
        .SetSize(sizeof(uint32_t) * indices.size());

    mesh->indexBuffer = std::make_unique<Buffer>(indexBufferCreation, _vulkanContext);
    memcpy(mesh->indexBuffer->mappedPtr, indices.data(), sizeof(uint32_t) * indices.size());

    return mesh;
}