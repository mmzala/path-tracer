#include "model_loader.hpp"
#include "resources/bindless_resources.hpp"
#include "resources/gpu_resources.hpp"
#include "single_time_commands.hpp"
#include "vk_common.hpp"
#include "../build/x64-Release/_deps/assimp-src/include/assimp/GltfMaterial.h"

#include <filesystem>
#include <glm/gtc/type_ptr.hpp>
#include <spdlog/spdlog.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <stb_image.h>

ResourceHandle<Image> ProcessImage(const std::string_view localPath, const std::string_view directory, const std::shared_ptr<BindlessResources>& resources)
{
    ImageCreation imageCreation {};
    imageCreation.SetName(localPath)
        .SetFormat(vk::Format::eR8G8B8A8Unorm)
        .SetUsageFlags(vk::ImageUsageFlagBits::eSampled);

    int32_t width {}, height {}, nrChannels {};

    const std::string fullPath = std::string(directory) + "/" + std::string{ localPath.begin(), localPath.end() };

    unsigned char* stbiData = stbi_load(fullPath.c_str(), &width, &height, &nrChannels, 4);

    if (!stbiData)
    {
        spdlog::error("[GLTF] Failed to load data from image [{}] from path [{}]", imageCreation.name, fullPath);
        return ResourceHandle<Image> {};
    }

    std::vector<std::byte> data = std::vector<std::byte>(width * height * 4);
    std::memcpy(data.data(), std::bit_cast<std::byte*>(stbiData), data.size());
    stbi_image_free(stbiData);

    imageCreation.SetSize(width, height)
        .SetData(data);

    return resources->Images().Create(imageCreation);
}

ResourceHandle<Image> LoadTexture(const std::string_view localPath, const std::string_view directory, const std::shared_ptr<BindlessResources>& resources, std::vector<ResourceHandle<Image>>& textures, std::unordered_map<std::string_view, ResourceHandle<Image>>& imageCache)
{
    const auto it = imageCache.find(localPath);

    if (it == imageCache.end())
    {
        ResourceHandle<Image> image = ProcessImage(localPath, directory, resources);
        textures.push_back(image);
        return image;
    }

    return it->second;
}

ResourceHandle<Material> ProcessMaterial(const aiMaterial* material, const std::string_view directory, const std::shared_ptr<BindlessResources>& resources, std::vector<ResourceHandle<Image>>& textures, std::unordered_map<std::string_view, ResourceHandle<Image>>& imageCache)
{
    MaterialCreation materialCreation {};

    // Textures

    aiString texturePath {};

    if (material->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath) == AI_SUCCESS)
    {
        materialCreation.albedoMap = LoadTexture(texturePath.C_Str(), directory, resources, textures, imageCache);
    }

    if (material->GetTexture(aiTextureType_GLTF_METALLIC_ROUGHNESS, 0, &texturePath) == AI_SUCCESS)
    {
        materialCreation.metallicRoughnessMap = LoadTexture(texturePath.C_Str(), directory, resources, textures, imageCache);
    }

    if (material->GetTexture(aiTextureType_NORMALS, 0, &texturePath) == AI_SUCCESS)
    {
        materialCreation.normalMap = LoadTexture(texturePath.C_Str(), directory, resources, textures, imageCache);
    }

    if (material->GetTexture(aiTextureType_AMBIENT_OCCLUSION, 0, &texturePath) == AI_SUCCESS)
    {
        materialCreation.occlusionMap = LoadTexture(texturePath.C_Str(), directory, resources, textures, imageCache);
    }

    if (material->GetTexture(aiTextureType_EMISSIVE, 0, &texturePath) == AI_SUCCESS)
    {
        materialCreation.emissiveMap = LoadTexture(texturePath.C_Str(), directory, resources, textures, imageCache);
    }

    // Properties

    aiColor4D color {};
    float factor {};

    if (material->Get(AI_MATKEY_BASE_COLOR, color) == AI_SUCCESS)
    {
        materialCreation.albedoFactor = glm::vec4(color.r, color.g, color.b, color.a);
    }

    if (material->Get(AI_MATKEY_METALLIC_FACTOR, factor) == AI_SUCCESS)
    {
        materialCreation.metallicFactor = factor;
    }

    if (material->Get(AI_MATKEY_ROUGHNESS_FACTOR, factor) == AI_SUCCESS)
    {
        materialCreation.roughnessFactor = factor;
    }

    if (material->Get(AI_MATKEY_GLTF_TEXTURE_SCALE(aiTextureType_NORMALS, 0), factor) == AI_SUCCESS)
    {
        materialCreation.normalScale = factor;
    }

    if (material->Get(AI_MATKEY_COLOR_EMISSIVE, color) == AI_SUCCESS)
    {
        materialCreation.emissiveFactor = glm::vec3(color.r, color.g, color.b);
    }

    if (material->Get(AI_MATKEY_GLTF_TEXTURE_SCALE(aiTextureType_AMBIENT_OCCLUSION, 0), factor) == AI_SUCCESS)
    {
        materialCreation.occlusionStrength = factor;
    }

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

ModelLoader::ModelLoader(const std::shared_ptr<BindlessResources>& bindlessResources, const std::shared_ptr<VulkanContext>& vulkanContext)
    : _bindlessResources(bindlessResources)
    , _vulkanContext(vulkanContext)
{
}

std::shared_ptr<Model> ModelLoader::LoadFromFile(std::string_view path)
{
    spdlog::info("[FILE] Loading model file {}", path);

    const aiScene* scene = _importer.ReadFile({path.begin(), path.end()}, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenNormals);

    if(!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
    {
        spdlog::error("[FILE] Failed to load model file {} with error: {}", path, _importer.GetErrorString());
        return nullptr;
    }

    _imageCache.clear(); // Clear image cache for a new load
    std::string_view directory = path.substr(0, path.find_last_of('/'));
    return ProcessModel(scene, directory);
}

std::shared_ptr<Model> ModelLoader::ProcessModel(const aiScene* scene, const std::string_view directory)
{
    std::shared_ptr<Model> model = std::make_shared<Model>();

    for (uint32_t i = 0; i < scene->mNumMaterials; ++i)
    {
        model->materials.push_back(ProcessMaterial(scene->mMaterials[i], directory, _bindlessResources, model->textures, _imageCache));
    }

    std::vector<Model::Vertex> vertices {};
    std::vector<uint32_t> indices {};

    for (uint32_t i = 0; i < scene->mNumMeshes; ++i)
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
