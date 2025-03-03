#include "gltf_loader.hpp"
#include "resources/bindless_resources.hpp"
#include "resources/gpu_resources.hpp"
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

    std::visit(
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

    return ResourceHandle<Image> {};
}

Mesh ProcessMesh(const fastgltf::Asset& gltf, const fastgltf::Mesh& gltfMesh, std::vector<Model::Vertex>& vertices, std::vector<uint32_t>& indices)
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

            fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(gltf, positionAccessor,
                [&](fastgltf::math::fvec3 position, size_t index)
                {
                    Model::Vertex vertex;
                    vertex.position = glm::vec3(position.x(), position.y(), position.z());
                    vertices[initialVertex + index] = vertex;
                });
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
    std::vector<Model::Vertex> vertices {};
    std::vector<uint32_t> indices {};

    for (const fastgltf::Image& gltfImage : gltf.images)
    {
        model->textures.push_back(ProcessImage(gltf, gltfImage, _bindlessResources, directory));
    }

    for (const fastgltf::Mesh& gltfMesh : gltf.meshes)
    {
        model->meshes.push_back(ProcessMesh(gltf, gltfMesh, vertices, indices));
    }

    // Process vertex and index data
    {
        model->verticesCount = vertices.size();
        model->indexCount = indices.size();

        // TODO: Upload to GPU friendly memory
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
    }

    model->nodes = ProcessNodes(gltf);

    return model;
}
