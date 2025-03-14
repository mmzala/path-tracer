#include "gltf_loader.hpp"
#include "resources/bindless_resources.hpp"
#include "resources/gpu_resources.hpp"
#include "single_time_commands.hpp"
#include "vk_common.hpp"
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/tools.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <spdlog/spdlog.h>
#include <stb_image.h>

ResourceHandle<Image> ProcessImage(const fastgltf::Asset& gltf, const fastgltf::Image& gltfImage, const std::shared_ptr<BindlessResources>& resources, const std::string_view directory)
{
    ImageCreation imageCreation {};
    imageCreation.SetName(gltfImage.name)
        .SetFormat(vk::Format::eR8G8B8A8Unorm)
        .SetUsageFlags(vk::ImageUsageFlagBits::eSampled);

    int32_t width {}, height {}, nrChannels {};

    const auto failedImageLoad = [&](const auto&)
    {
        spdlog::error("[GLTF] Suitable way not found to load image [{}] in gltf [{}]", gltfImage.name, gltf.scenes[0].name);
        return ResourceHandle<Image> {};
    };

    return std::visit(
        fastgltf::visitor {
            [&](const fastgltf::sources::URI& filePath)
            {
                if (filePath.fileByteOffset != 0)
                {
                    spdlog::error("[GLTF] Image [{}] in gltf [{}] uses file byte offset, which is unsupported", gltfImage.name, gltf.scenes[0].name);
                    return ResourceHandle<Image> {};
                }

                const std::string localPath(filePath.uri.path().begin(), filePath.uri.path().end());
                const std::string fullPath = std::string(directory) + "/" + localPath;

                if (!filePath.uri.isLocalPath())
                {
                    spdlog::error("[GLTF] Image [{}] in gltf [{}] is not local to the project, all resources must be local. The path detected: {}", gltfImage.name, gltf.scenes[0].name, fullPath);
                    return ResourceHandle<Image> {};
                }

                unsigned char* stbiData = stbi_load(fullPath.c_str(), &width, &height, &nrChannels, 4);

                if (!stbiData)
                {
                    spdlog::error("[GLTF] Failed to load data from Image [{}] from path [{}] in gltf [{}]", gltfImage.name, fullPath, gltf.scenes[0].name);
                    return ResourceHandle<Image> {};
                }

                std::vector<std::byte> data = std::vector<std::byte>(width * height * 4);
                std::memcpy(data.data(), std::bit_cast<std::byte*>(stbiData), data.size());
                stbi_image_free(stbiData);

                imageCreation.SetSize(width, height)
                    .SetData(data);

                return resources->Images().Create(imageCreation);
            },
            [&](const fastgltf::sources::Array& array)
            {
                unsigned char* stbiData = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(array.bytes.data()), static_cast<int>(array.bytes.size()),
                    &width, &height, &nrChannels, 4);

                if (!stbiData)
                {
                    spdlog::error("[GLTF] Failed to load data from Image [{}] in gltf [{}]", gltfImage.name, gltf.scenes[0].name);
                    return ResourceHandle<Image> {};
                }

                std::vector<std::byte> data = std::vector<std::byte>(width * height * 4);
                std::memcpy(data.data(), std::bit_cast<std::byte*>(stbiData), data.size());
                stbi_image_free(stbiData);

                imageCreation.SetSize(width, height)
                    .SetData(data);

                return resources->Images().Create(imageCreation);
            },
            [&](fastgltf::sources::BufferView& view)
            {
                auto& bufferView = gltf.bufferViews[view.bufferViewIndex];
                auto& buffer = gltf.buffers[bufferView.bufferIndex];

                std::visit(fastgltf::visitor { [&](const fastgltf::sources::Array& array)
                               {
                                   unsigned char* stbiData = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(array.bytes.data() + bufferView.byteOffset),
                                       static_cast<int>(bufferView.byteLength),
                                       &width, &height, &nrChannels, 4);

                                   if (!stbiData)
                                   {
                                       spdlog::error("[GLTF] Failed to load data from Image [{}] in gltf [{}]", gltfImage.name, gltf.scenes[0].name);
                                       return ResourceHandle<Image> {};
                                   }

                                   std::vector<std::byte> data = std::vector<std::byte>(width * height * 4);
                                   std::memcpy(data.data(), std::bit_cast<std::byte*>(stbiData), data.size());
                                   stbi_image_free(stbiData);

                                   imageCreation.SetSize(width, height)
                                       .SetData(data);

                                   return resources->Images().Create(imageCreation);
                               },
                               failedImageLoad },
                    buffer.data);
            },
            failedImageLoad },
        gltfImage.data);
}

ResourceHandle<Material> ProcessMaterial(const fastgltf::Material& gltfMaterial, const std::vector<fastgltf::Texture>& gltfTextures, const std::vector<ResourceHandle<Image>>& textures, const std::shared_ptr<BindlessResources>& resources)
{
    auto MapTextureIndexToImageIndex = [](uint32_t textureIndex, const std::vector<fastgltf::Texture>& gltfTextures) -> uint32_t
    {
        return gltfTextures[textureIndex].imageIndex.value();
    };

    MaterialCreation materialCreation {};

    if (gltfMaterial.pbrData.baseColorTexture.has_value())
    {
        uint32_t index = MapTextureIndexToImageIndex(gltfMaterial.pbrData.baseColorTexture.value().textureIndex, gltfTextures);
        materialCreation.albedoMap = textures[index];
    }

    if (gltfMaterial.pbrData.metallicRoughnessTexture.has_value())
    {
        uint32_t index = MapTextureIndexToImageIndex(gltfMaterial.pbrData.metallicRoughnessTexture.value().textureIndex, gltfTextures);
        materialCreation.metallicRoughnessMap = textures[index];
    }

    if (gltfMaterial.normalTexture.has_value())
    {
        uint32_t index = MapTextureIndexToImageIndex(gltfMaterial.normalTexture.value().textureIndex, gltfTextures);
        materialCreation.normalMap = textures[index];
    }

    if (gltfMaterial.occlusionTexture.has_value())
    {
        uint32_t index = MapTextureIndexToImageIndex(gltfMaterial.occlusionTexture.value().textureIndex, gltfTextures);
        materialCreation.occlusionMap = textures[index];
    }

    if (gltfMaterial.emissiveTexture.has_value())
    {
        uint32_t index = MapTextureIndexToImageIndex(gltfMaterial.emissiveTexture.value().textureIndex, gltfTextures);
        materialCreation.emissiveMap = textures[index];
    }

    materialCreation.SetAlbedoFactor(glm::vec4(gltfMaterial.pbrData.baseColorFactor.x(), gltfMaterial.pbrData.baseColorFactor.y(), gltfMaterial.pbrData.baseColorFactor.z(), gltfMaterial.pbrData.baseColorFactor.w()))
        .SetMetallicFactor(gltfMaterial.pbrData.metallicFactor)
        .SetRoughnessFactor(gltfMaterial.pbrData.roughnessFactor)
        .SetNormalScale(gltfMaterial.normalTexture.has_value() ? gltfMaterial.normalTexture.value().scale : 0.0f)
        .SetEmissiveFactor(glm::vec3(gltfMaterial.emissiveFactor.x(), gltfMaterial.emissiveFactor.y(), gltfMaterial.emissiveFactor.z()))
        .SetOcclusionStrength(gltfMaterial.occlusionTexture.has_value()
                ? gltfMaterial.occlusionTexture.value().strength
                : 1.0f);

    return resources->Materials().Create(materialCreation);
}

Mesh ProcessMesh(const fastgltf::Asset& gltf, const fastgltf::Mesh& gltfMesh, const std::vector<ResourceHandle<Material>>& materials, std::vector<Model::Vertex>& vertices, std::vector<uint32_t>& indices)
{
    Mesh mesh {};
    mesh.firstIndex = indices.size();

    for (const auto& primitive : gltfMesh.primitives)
    {
        size_t initialVertex = vertices.size();

        if (primitive.indicesAccessor.has_value())
        {
            const fastgltf::Accessor& indexAccessor = gltf.accessors[primitive.indicesAccessor.value()];
            indices.reserve(indices.size() + indexAccessor.count);
            mesh.indexCount += indexAccessor.count;

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

            fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, positionAccessor,
                [&](glm::vec3 position, size_t index)
                {
                    vertices[initialVertex + index].position = position;
                });
        }

        // load vertex normals
        {
            auto normals = primitive.findAttribute("NORMAL");
            if (normals != primitive.attributes.end())
            {
                const fastgltf::Accessor& normalAccessor = gltf.accessors[normals->accessorIndex];
                fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, normalAccessor,
                    [&](glm::vec3 normal, size_t index)
                    {
                        vertices[initialVertex + index].normal = normal;
                    });
            }
        }

        // load UVs
        {
            auto uvs = primitive.findAttribute("TEXCOORD_0");
            if (uvs != primitive.attributes.end())
            {
                const fastgltf::Accessor& uvAccessor = gltf.accessors[uvs->accessorIndex];
                fastgltf::iterateAccessorWithIndex<glm::vec2>(gltf, uvAccessor,
                    [&](glm::vec2 uv, size_t index)
                    {
                        vertices[initialVertex + index].texCoord = uv;
                    });
            }
        }

        // Get material
        if (primitive.materialIndex.has_value())
        {
            if (mesh.material.IsNull())
            {
                mesh.material = materials[primitive.materialIndex.value()];
            }
            else
            {
                spdlog::error("[GLTF] Mesh [{}] uses multiple different materials. This is not supported!", gltfMesh.name);
            }
        }
    }

    return mesh;
}

std::vector<Node> ProcessNodes(const fastgltf::Asset& gltf)
{
    std::vector<Node> nodes {};
    nodes.reserve(gltf.nodes.size());

    for (const fastgltf::Node& gltfNode : gltf.nodes)
    {
        Node& node = nodes.emplace_back();

        fastgltf::math::fmat4x4 gltfTransform = fastgltf::getTransformMatrix(gltfNode);
        node.localMatrix = glm::make_mat4(gltfTransform.data());

        if (gltfNode.meshIndex.has_value())
        {
            node.meshIndex = gltfNode.meshIndex.value();
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
            Node& childNode = nodes[gltfNodeChildIndex];
            childNode.parent = &node;
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

GLTFLoader::GLTFLoader(const std::shared_ptr<BindlessResources>& bindlessResources, const std::shared_ptr<VulkanContext>& vulkanContext)
    : _bindlessResources(bindlessResources)
    , _vulkanContext(vulkanContext)
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
    constexpr fastgltf::Options options = fastgltf::Options::DecomposeNodeMatrices | fastgltf::Options::LoadExternalBuffers;
    auto loadedGltf = _parser.loadGltf(fileStream, directory, options);

    if (!loadedGltf)
    {
        spdlog::error("[GLTF] Failed to parse GLTF file {}", path);
        return nullptr;
    }

    const fastgltf::Asset& gltf = loadedGltf.get();
    return ProcessModel(gltf, directory);
}

std::shared_ptr<Model> GLTFLoader::ProcessModel(const fastgltf::Asset& gltf, const std::string_view directory)
{
    std::shared_ptr<Model> model = std::make_shared<Model>();

    for (const fastgltf::Image& gltfImage : gltf.images)
    {
        model->textures.push_back(ProcessImage(gltf, gltfImage, _bindlessResources, directory));
    }

    for (const fastgltf::Material& gltfMaterial : gltf.materials)
    {
        model->materials.push_back(ProcessMaterial(gltfMaterial, gltf.textures, model->textures, _bindlessResources));
    }

    std::vector<Model::Vertex> vertices {};
    std::vector<uint32_t> indices {};

    for (const fastgltf::Mesh& gltfMesh : gltf.meshes)
    {
        model->meshes.push_back(ProcessMesh(gltf, gltfMesh, model->materials, vertices, indices));
    }

    // Process vertex and index data
    {
        model->verticesCount = vertices.size();
        model->indexCount = indices.size();

        // Staging buffers
        BufferCreation vertexStagingBufferCreation {};
        vertexStagingBufferCreation.SetName(gltf.nodes[0].name + " - Vertex Staging Buffer")
            .SetUsageFlags(vk::BufferUsageFlagBits::eTransferSrc)
            .SetMemoryUsage(VMA_MEMORY_USAGE_CPU_ONLY)
            .SetIsMappable(true)
            .SetSize(sizeof(Model::Vertex) * vertices.size());
        Buffer vertexStagingBuffer(vertexStagingBufferCreation, _vulkanContext);
        memcpy(vertexStagingBuffer.mappedPtr, vertices.data(), sizeof(Model::Vertex) * vertices.size());

        BufferCreation indexStagingBufferCreation {};
        indexStagingBufferCreation.SetName(gltf.nodes[0].name + " - Index Staging Buffer")
            .SetUsageFlags(vk::BufferUsageFlagBits::eTransferSrc)
            .SetMemoryUsage(VMA_MEMORY_USAGE_CPU_ONLY)
            .SetIsMappable(true)
            .SetSize(sizeof(uint32_t) * indices.size());
        Buffer indexStagingBuffer(indexStagingBufferCreation, _vulkanContext);
        memcpy(indexStagingBuffer.mappedPtr, indices.data(), sizeof(uint32_t) * indices.size());

        // GPU buffers
        vk::BufferUsageFlags bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress;

        BufferCreation vertexBufferCreation {};
        vertexBufferCreation.SetName(gltf.nodes[0].name + " - Vertex Buffer")
            .SetUsageFlags(vk::BufferUsageFlagBits::eVertexBuffer | bufferUsage)
            .SetMemoryUsage(VMA_MEMORY_USAGE_GPU_ONLY)
            .SetIsMappable(false)
            .SetSize(sizeof(Model::Vertex) * vertices.size());
        model->vertexBuffer = std::make_unique<Buffer>(vertexBufferCreation, _vulkanContext);

        BufferCreation indexBufferCreation {};
        indexBufferCreation.SetName(gltf.nodes[0].name + " - Index Buffer")
            .SetUsageFlags(vk::BufferUsageFlagBits::eIndexBuffer | bufferUsage)
            .SetMemoryUsage(VMA_MEMORY_USAGE_GPU_ONLY)
            .SetIsMappable(false)
            .SetSize(sizeof(uint32_t) * indices.size());
        model->indexBuffer = std::make_unique<Buffer>(indexBufferCreation, _vulkanContext);

        SingleTimeCommands commands(_vulkanContext);
        commands.Record([&](vk::CommandBuffer commandBuffer)
            {
                VkCopyBufferToBuffer(commandBuffer, vertexStagingBuffer.buffer, model->vertexBuffer->buffer, sizeof(Model::Vertex) * vertices.size());
                VkCopyBufferToBuffer(commandBuffer, indexStagingBuffer.buffer, model->indexBuffer->buffer, sizeof(uint32_t) * indices.size()); });
        commands.Submit();
    }

    model->nodes = ProcessNodes(gltf);

    return model;
}
