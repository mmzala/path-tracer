#pragma once
#include <memory>
#include <vulkan/vulkan.hpp>
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include "vk_common.hpp"
#include "common.hpp"

struct VulkanInitInfo;
struct Buffer;
class VulkanContext;
class SwapChain;

class Renderer
{
public:
    Renderer(const VulkanInitInfo& initInfo, std::shared_ptr<VulkanContext> vulkanContext);
    ~Renderer();
    NON_COPYABLE(Renderer);
    NON_MOVABLE(Renderer);

    void Render();

private:
    struct Vertex
    {
        glm::vec3 position;
    };

    struct AccelerationStructure
    {
        vk::AccelerationStructureKHR vkStructure;
        std::unique_ptr<Buffer> structureBuffer;
        std::unique_ptr<Buffer> scratchBuffer;
        std::unique_ptr<Buffer> instancesBuffer;
    };

    void RecordCommands(const vk::CommandBuffer& commandBuffer, uint32_t swapChainImageIndex);
    void InitializeCommandBuffers();
    void InitializeSynchronizationObjects();

    void InitializeTriangle();
    void InitializeBLAS();
    void InitializeTLAS();
    void InitializeDescriptorSets(glm::ivec2 windowSize);

    std::shared_ptr<VulkanContext> _vulkanContext;
    std::unique_ptr<SwapChain> _swapChain;
    std::array<vk::CommandBuffer, MAX_FRAMES_IN_FLIGHT> _commandBuffers;
    std::array<vk::Semaphore, MAX_FRAMES_IN_FLIGHT> _imageAvailableSemaphores;
    std::array<vk::Semaphore, MAX_FRAMES_IN_FLIGHT> _renderFinishedSemaphores;
    std::array<vk::Fence, MAX_FRAMES_IN_FLIGHT> _inFlightFences;

    uint32_t _currentResourcesFrame = 0;

    std::unique_ptr<Buffer> _vertexBuffer;
    std::unique_ptr<Buffer> _indexBuffer;
    std::unique_ptr<Buffer> _transformBuffer;

    AccelerationStructure _blas{};
    AccelerationStructure _tlas{};

    vk::DescriptorPool _descriptorPool;
    vk::DescriptorSetLayout _descriptorSetLayout;
    vk::DescriptorSet _descriptorSet;

    std::unique_ptr<Buffer> _uniformBuffer;
};