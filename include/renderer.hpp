#pragma once
#include <memory>
#include <vulkan/vulkan.hpp>
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include <glm/mat4x4.hpp>
#include "vk_common.hpp"
#include "common.hpp"

struct VulkanInitInfo;
struct Buffer;
struct Image;
class VulkanContext;
class SwapChain;
class GLTFLoader;
struct GLTFModel;

class Renderer
{
public:
    Renderer(const VulkanInitInfo& initInfo, const std::shared_ptr<VulkanContext>& vulkanContext);
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

    struct CameraUniformData
    {
        glm::mat4 viewInverse {};
        glm::mat4 projInverse {};
    };

    void RecordCommands(const vk::CommandBuffer& commandBuffer, uint32_t swapChainImageIndex);
    void InitializeCommandBuffers();
    void InitializeSynchronizationObjects();
    void InitializeRenderTarget();

    void InitializeTransformBuffer();
    void InitializeBLAS();
    void InitializeTLAS();
    void InitializeDescriptorSets();
    void InitializePipeline();
    void InitializeShaderBindingTable();

    std::shared_ptr<VulkanContext> _vulkanContext;
    std::unique_ptr<SwapChain> _swapChain;
    std::array<vk::CommandBuffer, MAX_FRAMES_IN_FLIGHT> _commandBuffers;
    std::array<vk::Semaphore, MAX_FRAMES_IN_FLIGHT> _imageAvailableSemaphores;
    std::array<vk::Semaphore, MAX_FRAMES_IN_FLIGHT> _renderFinishedSemaphores;
    std::array<vk::Fence, MAX_FRAMES_IN_FLIGHT> _inFlightFences;
    std::unique_ptr<Image> _renderTarget;

    uint32_t _currentResourcesFrame = 0;

    std::unique_ptr<GLTFLoader> _gltfLoader;
    std::shared_ptr<GLTFModel> _gltfModel;
    std::unique_ptr<Buffer> _transformBuffer;

    AccelerationStructure _blas{};
    AccelerationStructure _tlas{};

    vk::DescriptorPool _descriptorPool;
    vk::DescriptorSetLayout _descriptorSetLayout;
    vk::DescriptorSet _descriptorSet;

    std::unique_ptr<Buffer> _uniformBuffer;

    std::unique_ptr<Buffer> _raygenSBT;
    std::unique_ptr<Buffer> _missSBT;
    std::unique_ptr<Buffer> _hitSBT;
    vk::StridedDeviceAddressRegionKHR _raygenAddressRegion {};
    vk::StridedDeviceAddressRegionKHR _missAddressRegion {};
    vk::StridedDeviceAddressRegionKHR _hitAddressRegion {};

    vk::PipelineLayout _pipelineLayout;
    vk::Pipeline _pipeline;

    uint32_t _windowWidth = 0;
    uint32_t _windowHeight = 0;
};
