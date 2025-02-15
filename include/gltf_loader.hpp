#pragma once
#include "common.hpp"
#include "gpu_resources.hpp"
#include <fastgltf/core.hpp>
#include <glm/vec3.hpp>

class VulkanContext;

struct GLTFModel
{
    struct Vertex
    {
        glm::vec3 position {};
    };

    std::unique_ptr<Buffer> vertexBuffer;
    std::unique_ptr<Buffer> indexBuffer;

    uint32_t verticesCount {};
    uint32_t indicesCount {};
};

class GLTFLoader
{
public:
    GLTFLoader(const std::shared_ptr<VulkanContext>& vulkanContext);
    ~GLTFLoader() = default;
    NON_COPYABLE(GLTFLoader);
    NON_MOVABLE(GLTFLoader);

    [[nodiscard]] std::shared_ptr<GLTFModel> LoadFromFile(std::string_view path);

private:
    [[nodiscard]] std::shared_ptr<GLTFModel> ProcessModel(const fastgltf::Asset& gltf);

    std::shared_ptr<VulkanContext> _vulkanContext;
    fastgltf::Parser _parser;
};
