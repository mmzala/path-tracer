#pragma once
#include "common.hpp"
#include "resources/resource_manager.hpp"
#include <fastgltf/core.hpp>
#include <glm/vec3.hpp>
#include <glm/matrix.hpp>
#include <optional>

class VulkanContext;
class BindlessResources;
struct Buffer;
struct Image;

struct Node
{
    const Node* parent = nullptr;
    glm::mat4 localMatrix {};
    std::optional<uint32_t> meshIndex {};

    [[nodiscard]] glm::mat4 GetWorldMatrix() const;
};

struct Mesh
{
    uint32_t indexCount {};
    uint32_t firstIndex {};
};

struct Model
{
    struct Vertex
    {
        glm::vec3 position {};
    };

    std::unique_ptr<Buffer> vertexBuffer;
    std::unique_ptr<Buffer> indexBuffer;
    uint32_t verticesCount {};
    uint32_t indexCount {};

    std::vector<Node> nodes {};
    std::vector<Mesh> meshes {};
    std::vector<ResourceHandle<Image>> textures {};
};

class GLTFLoader
{
public:
    GLTFLoader(const std::shared_ptr<BindlessResources>& bindlessResources, const std::shared_ptr<VulkanContext>& vulkanContext);
    ~GLTFLoader() = default;
    NON_COPYABLE(GLTFLoader);
    NON_MOVABLE(GLTFLoader);

    [[nodiscard]] std::shared_ptr<Model> LoadFromFile(std::string_view path);

private:
    [[nodiscard]] std::shared_ptr<Model> ProcessModel(const fastgltf::Asset& gltf, const std::string_view directory);

    std::shared_ptr<VulkanContext> _vulkanContext;
    std::shared_ptr<BindlessResources> _bindlessResources;
    fastgltf::Parser _parser;
};
