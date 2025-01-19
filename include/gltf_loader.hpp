#pragma once
#include "common.hpp"
#include "gpu_resources.hpp"
#include <fastgltf/core.hpp>
#include <glm/vec3.hpp>

class VulkanContext;

struct GLTFMesh
{
    struct Vertex
    {
        glm::vec3 position {};
    };

    std::unique_ptr<Buffer> vertexBuffer;
    std::unique_ptr<Buffer> indexBuffer;
};

class GLTFLoader
{
public:
    GLTFLoader(std::shared_ptr<VulkanContext> vulkanContext);
    ~GLTFLoader() = default;
    NON_COPYABLE(GLTFLoader);
    NON_MOVABLE(GLTFLoader);

    std::shared_ptr<GLTFMesh> LoadFromFile(std::string_view path);

private:
    std::shared_ptr<GLTFMesh> ProcessMesh(const fastgltf::Asset& gltf);

    std::shared_ptr<VulkanContext> _vulkanContext;
    fastgltf::Parser _parser;
};