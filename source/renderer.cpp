#include "renderer.hpp"
#include "swap_chain.hpp"
#include "vulkan_context.hpp"
#include "gpu_resources.hpp"
#include "single_time_commands.hpp"
#include "shader.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

Renderer::Renderer(const VulkanInitInfo& initInfo, std::shared_ptr<VulkanContext> vulkanContext)
    : _vulkanContext(vulkanContext)
{
    _windowWidth = initInfo.width;
    _windowHeight = initInfo.height;

    _swapChain = std::make_unique<SwapChain>(vulkanContext, glm::uvec2 { initInfo.width, initInfo.height });
    InitializeCommandBuffers();
    InitializeSynchronizationObjects();
    InitializeRenderTarget();

    InitializeTriangle();
    InitializeBLAS();
    InitializeTLAS();
    InitializeDescriptorSets();
    InitializePipeline();
    InitializeShaderBindingTable();
}

Renderer::~Renderer()
{
    _vulkanContext->Device().destroyPipeline(_pipeline);
    _vulkanContext->Device().destroyPipelineLayout(_pipelineLayout);

    _vulkanContext->Device().destroyDescriptorSetLayout(_descriptorSetLayout);
    _vulkanContext->Device().destroyDescriptorPool(_descriptorPool);

    _vulkanContext->Device().destroyAccelerationStructureKHR(_tlas.vkStructure, nullptr, _vulkanContext->Dldi());
    _vulkanContext->Device().destroyAccelerationStructureKHR(_blas.vkStructure, nullptr, _vulkanContext->Dldi());

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
        _vulkanContext->Device().destroy(_inFlightFences.at(i));
        _vulkanContext->Device().destroy(_renderFinishedSemaphores.at(i));
        _vulkanContext->Device().destroy(_imageAvailableSemaphores.at(i));
    }
}

void Renderer::Render()
{
    VkCheckResult(_vulkanContext->Device().waitForFences(1, &_inFlightFences.at(_currentResourcesFrame), vk::True,
                      std::numeric_limits<uint64_t>::max()),
        "[VULKAN] Failed waiting on in flight fence!");

    uint32_t swapChainImageIndex {};
    VkCheckResult(_vulkanContext->Device().acquireNextImageKHR(_swapChain->GetSwapChain(), std::numeric_limits<uint64_t>::max(),
                      _imageAvailableSemaphores.at(_currentResourcesFrame), nullptr, &swapChainImageIndex),
        "[VULKAN] Failed to acquire swap chain image!");

    VkCheckResult(_vulkanContext->Device().resetFences(1, &_inFlightFences.at(_currentResourcesFrame)), "[VULKAN] Failed resetting fences!");

    vk::CommandBuffer commandBuffer = _commandBuffers.at(_currentResourcesFrame);
    commandBuffer.reset();

    vk::CommandBufferBeginInfo commandBufferBeginInfo {};
    VkCheckResult(commandBuffer.begin(&commandBufferBeginInfo), "[VULKAN] Failed to begin recording command buffer!");
    RecordCommands(commandBuffer, swapChainImageIndex);
    commandBuffer.end();

    vk::Semaphore waitSemaphore = _imageAvailableSemaphores.at(_currentResourcesFrame);
    vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    vk::Semaphore signalSemaphore = _renderFinishedSemaphores.at(_currentResourcesFrame);

    vk::SubmitInfo submitInfo {};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &waitSemaphore;
    submitInfo.pWaitDstStageMask = &waitStage;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &signalSemaphore;
    VkCheckResult(_vulkanContext->GraphicsQueue().submit(1, &submitInfo, _inFlightFences.at(_currentResourcesFrame)), "[VULKAN] Failed submitting to graphics queue!");

    vk::SwapchainKHR swapchain = _swapChain->GetSwapChain();
    vk::PresentInfoKHR presentInfo {};
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &signalSemaphore;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain;
    presentInfo.pImageIndices = &swapChainImageIndex;
    VkCheckResult(_vulkanContext->PresentQueue().presentKHR(&presentInfo), "[VULKAN] Failed to present swap chain image!");

    _currentResourcesFrame = (_currentResourcesFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Renderer::RecordCommands(const vk::CommandBuffer& commandBuffer, uint32_t swapChainImageIndex)
{
    VkTransitionImageLayout(commandBuffer, _renderTarget->image, _renderTarget->format,
        vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);

    commandBuffer.bindPipeline(vk::PipelineBindPoint::eRayTracingKHR, _pipeline);
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eRayTracingKHR, _pipelineLayout, 0, _descriptorSet, nullptr);

    commandBuffer.traceRaysKHR(_raygenAddressRegion, _missAddressRegion, _hitAddressRegion, {}, _windowWidth, _windowHeight, 1, _vulkanContext->Dldi());

    VkTransitionImageLayout(commandBuffer, _swapChain->GetImage(swapChainImageIndex), _swapChain->GetFormat(),
        vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
    VkTransitionImageLayout(commandBuffer, _renderTarget->image, _renderTarget->format,
        vk::ImageLayout::eGeneral, vk::ImageLayout::eTransferSrcOptimal);

    vk::Extent2D extent = { _windowWidth, _windowHeight };
    VkCopyImageToImage(commandBuffer, _renderTarget->image, _swapChain->GetImage(swapChainImageIndex), extent, extent);

    VkTransitionImageLayout(commandBuffer, _swapChain->GetImage(swapChainImageIndex), _swapChain->GetFormat(),
        vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::ePresentSrcKHR);
}

void Renderer::InitializeCommandBuffers()
{
    vk::CommandBufferAllocateInfo commandBufferAllocateInfo {};
    commandBufferAllocateInfo.commandPool = _vulkanContext->CommandPool();
    commandBufferAllocateInfo.level = vk::CommandBufferLevel::ePrimary;
    commandBufferAllocateInfo.commandBufferCount = _commandBuffers.size();

    VkCheckResult(_vulkanContext->Device().allocateCommandBuffers(&commandBufferAllocateInfo, _commandBuffers.data()), "[VULKAN] Failed allocating command buffer!");
}

void Renderer::InitializeSynchronizationObjects()
{
    vk::SemaphoreCreateInfo semaphoreCreateInfo {};
    vk::FenceCreateInfo fenceCreateInfo {};
    fenceCreateInfo.flags = vk::FenceCreateFlagBits::eSignaled;

    std::string errorMsg { "[VULKAN] Failed creating sync object!" };
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
        VkCheckResult(_vulkanContext->Device().createSemaphore(&semaphoreCreateInfo, nullptr, &_imageAvailableSemaphores[i]), errorMsg);
        VkCheckResult(_vulkanContext->Device().createSemaphore(&semaphoreCreateInfo, nullptr, &_renderFinishedSemaphores[i]), errorMsg);
        VkCheckResult(_vulkanContext->Device().createFence(&fenceCreateInfo, nullptr, &_inFlightFences[i]), errorMsg);
    }
}

void Renderer::InitializeRenderTarget()
{
    ImageCreation imageCreation{};
    imageCreation.SetName("Render Target")
        .SetSize(_windowWidth, _windowHeight)
        .SetFormat(_swapChain->GetFormat())
        .SetUsageFlags(vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eStorage);

    _renderTarget = std::make_unique<Image>(imageCreation, _vulkanContext);
}

void Renderer::InitializeTriangle()
{
    const std::vector<Vertex> vertices =
    {
        { { 1.0f, 1.0f, 0.0f } },
        { { -1.0f, 1.0f, 0.0f } },
        { { 0.0f, -1.0f, 0.0f } }
    };

    const std::vector<uint32_t> indices = { 0, 1, 2 };

    const VkTransformMatrixKHR transformMatrix =
    {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f
    };


    // TODO: Upload to GPU friendly memory

    BufferCreation vertexBufferCreation {};
    vertexBufferCreation.SetName("Vertex Buffer")
        .SetUsageFlags(vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress)
        .SetMemoryUsage(VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE)
        .SetIsMappable(true)
        .SetSize(sizeof(Vertex) * vertices.size());

    _vertexBuffer = std::make_unique<Buffer>(vertexBufferCreation, _vulkanContext);
    memcpy(_vertexBuffer->mappedPtr, vertices.data(), sizeof(Vertex) * vertices.size());

    BufferCreation indexBufferCreation {};
    indexBufferCreation.SetName("Index Buffer")
        .SetUsageFlags(vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress)
        .SetMemoryUsage(VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE)
        .SetIsMappable(true)
        .SetSize(sizeof(uint32_t) * indices.size());

    _indexBuffer = std::make_unique<Buffer>(indexBufferCreation, _vulkanContext);
    memcpy(_indexBuffer->mappedPtr, indices.data(), sizeof(uint32_t) * indices.size());

    BufferCreation transformBufferCreation {};
    transformBufferCreation.SetName("Transform Buffer")
        .SetUsageFlags(vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress)
        .SetMemoryUsage(VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE)
        .SetIsMappable(true)
        .SetSize(sizeof(uint32_t) * indices.size());

    _transformBuffer = std::make_unique<Buffer>(transformBufferCreation, _vulkanContext);
    memcpy(_transformBuffer->mappedPtr, &transformMatrix, sizeof(VkTransformMatrixKHR));
}

void Renderer::InitializeBLAS()
{
    vk::DeviceOrHostAddressConstKHR vertexBufferDeviceAddress {};
    vk::DeviceOrHostAddressConstKHR indexBufferDeviceAddress {};
    vk::DeviceOrHostAddressConstKHR transformBufferDeviceAddress {};

    vertexBufferDeviceAddress.deviceAddress = _vulkanContext->Device().getBufferAddress(_vertexBuffer->buffer);
    indexBufferDeviceAddress.deviceAddress = _vulkanContext->Device().getBufferAddress(_indexBuffer->buffer);
    transformBufferDeviceAddress.deviceAddress = _vulkanContext->Device().getBufferAddress(_transformBuffer->buffer);

    vk::AccelerationStructureGeometryTrianglesDataKHR trianglesData {};
    trianglesData.vertexFormat = vk::Format::eR32G32B32Sfloat;
    trianglesData.vertexData = vertexBufferDeviceAddress;
    trianglesData.vertexStride = sizeof(Vertex);
    trianglesData.maxVertex = 2;
    trianglesData.indexType = vk::IndexType::eUint32;
    trianglesData.indexData = indexBufferDeviceAddress;
    trianglesData.transformData = transformBufferDeviceAddress;

    vk::AccelerationStructureGeometryKHR accelerationStructureGeometry{};
    accelerationStructureGeometry.flags=vk::GeometryFlagBitsKHR::eOpaque;
    accelerationStructureGeometry.geometryType = vk::GeometryTypeKHR::eTriangles;
    accelerationStructureGeometry.geometry.triangles = trianglesData;

    vk::AccelerationStructureBuildGeometryInfoKHR buildInfo{};
    buildInfo.type = vk::AccelerationStructureTypeKHR::eBottomLevel;
    buildInfo.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
    buildInfo.mode = vk::BuildAccelerationStructureModeKHR::eBuild;
    buildInfo.srcAccelerationStructure = nullptr;
    buildInfo.dstAccelerationStructure = nullptr;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries = &accelerationStructureGeometry;
    buildInfo.scratchData = {};

    vk::AccelerationStructureBuildSizesInfoKHR buildSizesInfo = _vulkanContext->Device().getAccelerationStructureBuildSizesKHR(
        vk::AccelerationStructureBuildTypeKHR::eDevice, buildInfo, 1, _vulkanContext->Dldi());

    BufferCreation structureBufferCreation {};
    structureBufferCreation.SetName("BLAS Structure Buffer")
        .SetUsageFlags(vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress)
        .SetMemoryUsage(VMA_MEMORY_USAGE_GPU_ONLY)
        .SetIsMappable(false)
        .SetSize(buildSizesInfo.accelerationStructureSize);
    _blas.structureBuffer = std::make_unique<Buffer>(structureBufferCreation, _vulkanContext);

    BufferCreation scratchBufferCreation {};
    scratchBufferCreation.SetName("BLAS Scratch Buffer")
    .SetUsageFlags(vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress)
        .SetMemoryUsage(VMA_MEMORY_USAGE_GPU_ONLY)
        .SetIsMappable(false)
        .SetSize(buildSizesInfo.buildScratchSize);
    _blas.scratchBuffer = std::make_unique<Buffer>(scratchBufferCreation, _vulkanContext);

    vk::AccelerationStructureCreateInfoKHR createInfo {};
    createInfo.buffer = _blas.structureBuffer->buffer;
    createInfo.offset = 0;
    createInfo.size = buildSizesInfo.accelerationStructureSize;
    createInfo.type = vk::AccelerationStructureTypeKHR::eBottomLevel;
    _blas.vkStructure = _vulkanContext->Device().createAccelerationStructureKHR(createInfo, nullptr, _vulkanContext->Dldi());

    buildInfo.dstAccelerationStructure = _blas.vkStructure;
    buildInfo.scratchData.deviceAddress = _vulkanContext->Device().getBufferAddress(_blas.scratchBuffer->buffer);

    vk::AccelerationStructureBuildRangeInfoKHR buildRangeInfo {};
    buildRangeInfo.primitiveCount = 1;
    buildRangeInfo.primitiveOffset = 0;
    buildRangeInfo.firstVertex = 0;
    buildRangeInfo.transformOffset = 0;
    std::vector<vk::AccelerationStructureBuildRangeInfoKHR*> accelerationBuildStructureRangeInfos = { &buildRangeInfo };

    SingleTimeCommands singleTimeCommands{ _vulkanContext };
    singleTimeCommands.Record([&](vk::CommandBuffer commandBuffer)
    {
        commandBuffer.buildAccelerationStructuresKHR(1, &buildInfo, accelerationBuildStructureRangeInfos.data(), _vulkanContext->Dldi());
    });
    singleTimeCommands.Submit();
}

void Renderer::InitializeTLAS()
{
    vk::AccelerationStructureGeometryKHR accelerationStructureGeometry{};
    accelerationStructureGeometry.flags = vk::GeometryFlagBitsKHR::eOpaque;
    accelerationStructureGeometry.geometryType = vk::GeometryTypeKHR::eInstances;
    // Somehow not set to the correct type, need to set myself otherwise validation layers complain
    accelerationStructureGeometry.geometry.instances.sType = vk::StructureType::eAccelerationStructureGeometryInstancesDataKHR;
    accelerationStructureGeometry.geometry.instances.arrayOfPointers = false;

    vk::AccelerationStructureBuildGeometryInfoKHR buildInfo{};
    buildInfo.type = vk::AccelerationStructureTypeKHR::eTopLevel;
    buildInfo.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
    buildInfo.mode = vk::BuildAccelerationStructureModeKHR::eBuild;
    buildInfo.srcAccelerationStructure = nullptr;
    buildInfo.dstAccelerationStructure = nullptr;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries = &accelerationStructureGeometry;

    vk::AccelerationStructureBuildSizesInfoKHR buildSizesInfo = _vulkanContext->Device().getAccelerationStructureBuildSizesKHR(
        vk::AccelerationStructureBuildTypeKHR::eDevice, buildInfo, 1, _vulkanContext->Dldi());

    BufferCreation structureBufferCreation {};
    structureBufferCreation.SetName("TLAS Structure Buffer")
        .SetUsageFlags(vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress)
        .SetMemoryUsage(VMA_MEMORY_USAGE_GPU_ONLY)
        .SetIsMappable(false)
        .SetSize(buildSizesInfo.accelerationStructureSize);
    _tlas.structureBuffer = std::make_unique<Buffer>(structureBufferCreation, _vulkanContext);

    BufferCreation scratchBufferCreation {};
    scratchBufferCreation.SetName("TLAS Scratch Buffer")
    .SetUsageFlags(vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress)
        .SetMemoryUsage(VMA_MEMORY_USAGE_GPU_ONLY)
        .SetIsMappable(false)
        .SetSize(buildSizesInfo.buildScratchSize);
    _tlas.scratchBuffer = std::make_unique<Buffer>(scratchBufferCreation, _vulkanContext);

    vk::AccelerationStructureCreateInfoKHR createInfo {};
    createInfo.buffer= _tlas.structureBuffer->buffer;
    createInfo.offset = 0;
    createInfo.size = buildSizesInfo.accelerationStructureSize;
    createInfo.type = vk::AccelerationStructureTypeKHR::eTopLevel;
    _tlas.vkStructure = _vulkanContext->Device().createAccelerationStructureKHR(createInfo, nullptr, _vulkanContext->Dldi());

    const VkTransformMatrixKHR identityMatrix =
    {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f
    };

    vk::AccelerationStructureInstanceKHR accelerationStructureInstance {};
    accelerationStructureInstance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR; // vk::GeometryInstanceFlagBitsKHR::eTriangleFacingCullDisable
    accelerationStructureInstance.transform = identityMatrix;
    accelerationStructureInstance.instanceCustomIndex = 0;
    accelerationStructureInstance.mask = 0xFF;
    accelerationStructureInstance.instanceShaderBindingTableRecordOffset = 0;
    accelerationStructureInstance.accelerationStructureReference = _vulkanContext->Device().getAccelerationStructureAddressKHR(_blas.vkStructure, _vulkanContext->Dldi());

    BufferCreation instancesBufferCreation {};
    instancesBufferCreation.SetName("TLAS Instances Buffer")
        .SetUsageFlags(vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress)
        .SetMemoryUsage(VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE)
        .SetIsMappable(true)
        .SetSize(buildSizesInfo.accelerationStructureSize);
    _tlas.instancesBuffer = std::make_unique<Buffer>(instancesBufferCreation, _vulkanContext);
    memcpy(_tlas.instancesBuffer->mappedPtr, &accelerationStructureInstance, sizeof(vk::AccelerationStructureInstanceKHR));

    buildInfo.dstAccelerationStructure = _tlas.vkStructure;
    buildInfo.scratchData.deviceAddress = _vulkanContext->Device().getBufferAddress(_tlas.scratchBuffer->buffer);

    vk::AccelerationStructureGeometryInstancesDataKHR instancesData{};
    instancesData.arrayOfPointers = vk::False;
    instancesData.data = _vulkanContext->Device().getBufferAddress(_tlas.instancesBuffer->buffer, _vulkanContext->Dldi());
    accelerationStructureGeometry.geometry.instances = instancesData;

    vk::AccelerationStructureBuildRangeInfoKHR buildRangeInfo {};
    buildRangeInfo.primitiveCount = 1;
    buildRangeInfo.primitiveOffset = 0;
    buildRangeInfo.firstVertex = 0;
    buildRangeInfo.transformOffset = 0;
    std::vector<vk::AccelerationStructureBuildRangeInfoKHR*> accelerationBuildStructureRangeInfos = { &buildRangeInfo };

    SingleTimeCommands singleTimeCommands{ _vulkanContext };
    singleTimeCommands.Record([&](vk::CommandBuffer commandBuffer)
    {
        commandBuffer.buildAccelerationStructuresKHR(1, &buildInfo, accelerationBuildStructureRangeInfos.data(), _vulkanContext->Dldi());
    });
    singleTimeCommands.Submit();
}

void Renderer::InitializeDescriptorSets()
{
    CameraUniformData cameraData {};
    cameraData.viewInverse = glm::inverse(glm::lookAt(glm::vec3(0.0f, 0.0f, -2.5f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f)));
    cameraData.projInverse = glm::inverse(glm::perspective(glm::radians(60.0f), static_cast<float>(_windowWidth) / static_cast<float>(_windowHeight), 0.0f, 1000.0f));

    constexpr vk::DeviceSize uniformBufferSize = sizeof(CameraUniformData);
    BufferCreation uniformBufferCreation {};
    uniformBufferCreation.SetName("Camera Uniform Buffer")
        .SetUsageFlags(vk::BufferUsageFlagBits::eUniformBuffer)
        .SetMemoryUsage(VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE)
        .SetIsMappable(true)
        .SetSize(uniformBufferSize);
    _uniformBuffer = std::make_unique<Buffer>(uniformBufferCreation, _vulkanContext);
    memcpy(_uniformBuffer->mappedPtr, &cameraData, sizeof(CameraUniformData));

    std::array<vk::DescriptorSetLayoutBinding, 3> bindingLayouts {};

    vk::DescriptorSetLayoutBinding& imageLayout = bindingLayouts.at(0);
    imageLayout.binding = 0;
    imageLayout.descriptorType = vk::DescriptorType::eStorageImage;
    imageLayout.descriptorCount = 1;
    imageLayout.stageFlags = vk::ShaderStageFlagBits::eRaygenKHR;

    vk::DescriptorSetLayoutBinding& accelerationStructureLayout = bindingLayouts.at(1);
    accelerationStructureLayout.binding = 1;
    accelerationStructureLayout.descriptorType = vk::DescriptorType::eAccelerationStructureKHR;
    accelerationStructureLayout.descriptorCount = 1;
    accelerationStructureLayout.stageFlags = vk::ShaderStageFlagBits::eRaygenKHR;

    vk::DescriptorSetLayoutBinding& cameraLayout = bindingLayouts.at(2);
    cameraLayout.binding = 2;
    cameraLayout.descriptorType = vk::DescriptorType::eUniformBuffer;
    cameraLayout.descriptorCount = 1;
    cameraLayout.stageFlags = vk::ShaderStageFlagBits::eRaygenKHR;

    vk::DescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo {};
    descriptorSetLayoutCreateInfo.bindingCount = bindingLayouts.size();
    descriptorSetLayoutCreateInfo.pBindings = bindingLayouts.data();
    _descriptorSetLayout = _vulkanContext->Device().createDescriptorSetLayout(descriptorSetLayoutCreateInfo);

    std::array<vk::DescriptorPoolSize, 3> poolSizes {};

    vk::DescriptorPoolSize& imagePoolSize = poolSizes.at(0);
    imagePoolSize.type = vk::DescriptorType::eStorageImage;
    imagePoolSize.descriptorCount = 1;

    vk::DescriptorPoolSize& accelerationStructureSize = poolSizes.at(1);
    accelerationStructureSize.type = vk::DescriptorType::eAccelerationStructureKHR;
    accelerationStructureSize.descriptorCount = 1;

    vk::DescriptorPoolSize& cameraSize = poolSizes.at(2);
    cameraSize.type = vk::DescriptorType::eUniformBuffer;
    cameraSize.descriptorCount = 1;

    vk::DescriptorPoolCreateInfo descriptorPoolCreateInfo {};
    descriptorPoolCreateInfo.maxSets = 1;
    descriptorPoolCreateInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    descriptorPoolCreateInfo.pPoolSizes = poolSizes.data();
    _descriptorPool = _vulkanContext->Device().createDescriptorPool(descriptorPoolCreateInfo);

    vk::DescriptorSetAllocateInfo descriptorSetAllocateInfo {};
    descriptorSetAllocateInfo.descriptorPool = _descriptorPool;
    descriptorSetAllocateInfo.descriptorSetCount = 1;
    descriptorSetAllocateInfo.pSetLayouts = &_descriptorSetLayout;
    _descriptorSet = _vulkanContext->Device().allocateDescriptorSets(descriptorSetAllocateInfo).front();

    vk::DescriptorImageInfo descriptorImageInfo {};
    descriptorImageInfo.imageView = _renderTarget->view;
    descriptorImageInfo.imageLayout = vk::ImageLayout::eGeneral;

    vk::WriteDescriptorSetAccelerationStructureKHR descriptorAccelerationStructureInfo {};
    descriptorAccelerationStructureInfo.accelerationStructureCount = 1;
    descriptorAccelerationStructureInfo.pAccelerationStructures = &_tlas.vkStructure;

    vk::DescriptorBufferInfo descriptorBufferInfo {};
    descriptorBufferInfo.buffer = _uniformBuffer->buffer;
    descriptorBufferInfo.offset = 0;
    descriptorBufferInfo.range = uniformBufferSize;

    std::array<vk::WriteDescriptorSet, 3> descriptorWrites {};

    vk::WriteDescriptorSet& imageWrite = descriptorWrites.at(0);
    imageWrite.dstSet = _descriptorSet;
    imageWrite.dstBinding = 0;
    imageWrite.dstArrayElement = 0;
    imageWrite.descriptorCount = 1;
    imageWrite.descriptorType = vk::DescriptorType::eStorageImage;
    imageWrite.pImageInfo = &descriptorImageInfo;

    vk::WriteDescriptorSet& accelerationStructureWrite = descriptorWrites.at(1);
    accelerationStructureWrite.pNext = &descriptorAccelerationStructureInfo;
    accelerationStructureWrite.dstSet = _descriptorSet;
    accelerationStructureWrite.dstBinding = 1;
    accelerationStructureWrite.dstArrayElement = 0;
    accelerationStructureWrite.descriptorCount = 1;
    accelerationStructureWrite.descriptorType = vk::DescriptorType::eAccelerationStructureKHR;

    vk::WriteDescriptorSet& uniformBufferWrite = descriptorWrites.at(2);
    uniformBufferWrite.dstSet = _descriptorSet;
    uniformBufferWrite.dstBinding = 2;
    uniformBufferWrite.dstArrayElement = 0;
    uniformBufferWrite.descriptorCount = 1;
    uniformBufferWrite.descriptorType = vk::DescriptorType::eUniformBuffer;
    uniformBufferWrite.pBufferInfo = &descriptorBufferInfo;

    _vulkanContext->Device().updateDescriptorSets(static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
}

void Renderer::InitializePipeline()
{
    vk::ShaderModule raygenModule = Shader::CreateShaderModule("shaders/bin/ray_gen.rgen.spv", _vulkanContext->Device());
    vk::ShaderModule missModule = Shader::CreateShaderModule("shaders/bin/miss.rmiss.spv", _vulkanContext->Device());
    vk::ShaderModule chitModule = Shader::CreateShaderModule("shaders/bin/closest_hit.rchit.spv", _vulkanContext->Device());

    std::array<vk::PipelineShaderStageCreateInfo, 3> shaderStagesCreateInfo {};

    vk::PipelineShaderStageCreateInfo& raygenStage = shaderStagesCreateInfo.at(0);
    raygenStage.stage = vk::ShaderStageFlagBits::eRaygenKHR;
    raygenStage.module = raygenModule;
    raygenStage.pName = "main";

    vk::PipelineShaderStageCreateInfo& missStage = shaderStagesCreateInfo.at(1);
    missStage.stage = vk::ShaderStageFlagBits::eMissKHR;
    missStage.module = missModule;
    missStage.pName = "main";

    vk::PipelineShaderStageCreateInfo& chitStage = shaderStagesCreateInfo.at(2);
    chitStage.stage = vk::ShaderStageFlagBits::eClosestHitKHR;
    chitStage.module = chitModule;
    chitStage.pName = "main";

    std::array<vk::RayTracingShaderGroupCreateInfoKHR, 3> shaderGroupsCreateInfo {};

    vk::RayTracingShaderGroupCreateInfoKHR& group1 = shaderGroupsCreateInfo.at(0);
    group1.type = vk::RayTracingShaderGroupTypeKHR::eGeneral;
    group1.generalShader = 0;
    group1.closestHitShader = vk::ShaderUnusedKHR;
    group1.anyHitShader = vk::ShaderUnusedKHR;
    group1.intersectionShader = vk::ShaderUnusedKHR;

    vk::RayTracingShaderGroupCreateInfoKHR& group2 = shaderGroupsCreateInfo.at(1);
    group2.type = vk::RayTracingShaderGroupTypeKHR::eGeneral;
    group2.generalShader = 1;
    group2.closestHitShader = vk::ShaderUnusedKHR;
    group2.anyHitShader = vk::ShaderUnusedKHR;
    group2.intersectionShader = vk::ShaderUnusedKHR;

    vk::RayTracingShaderGroupCreateInfoKHR& group3 = shaderGroupsCreateInfo.at(2);
    group3.type = vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup;
    group3.generalShader = vk::ShaderUnusedKHR;
    group3.closestHitShader = 2;
    group3.anyHitShader = vk::ShaderUnusedKHR;
    group3.intersectionShader = vk::ShaderUnusedKHR;

    vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo {};
    pipelineLayoutCreateInfo.setLayoutCount = 1;
    pipelineLayoutCreateInfo.pSetLayouts = &_descriptorSetLayout;
    pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
    pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;
    _pipelineLayout = _vulkanContext->Device().createPipelineLayout(pipelineLayoutCreateInfo);

    vk::PipelineLibraryCreateInfoKHR libraryCreateInfo {};
    libraryCreateInfo.libraryCount = 0;

    vk::RayTracingPipelineCreateInfoKHR pipelineCreateInfo {};
    pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStagesCreateInfo.size());
    pipelineCreateInfo.pStages = shaderStagesCreateInfo.data();
    pipelineCreateInfo.groupCount = static_cast<uint32_t>(shaderGroupsCreateInfo.size());
    pipelineCreateInfo.pGroups = shaderGroupsCreateInfo.data();
    pipelineCreateInfo.maxPipelineRayRecursionDepth = _vulkanContext->RayTracingPipelineProperties().maxRayRecursionDepth;
    pipelineCreateInfo.pLibraryInfo = &libraryCreateInfo;
    pipelineCreateInfo.pLibraryInterface = nullptr;
    pipelineCreateInfo.layout = _pipelineLayout;
    pipelineCreateInfo.basePipelineHandle = nullptr;
    pipelineCreateInfo.basePipelineIndex = 0;

    _pipeline = _vulkanContext->Device().createRayTracingPipelineKHR(nullptr, nullptr, pipelineCreateInfo, nullptr, _vulkanContext->Dldi()).value;

    _vulkanContext->Device().destroyShaderModule(raygenModule);
    _vulkanContext->Device().destroyShaderModule(missModule);
    _vulkanContext->Device().destroyShaderModule(chitModule);
}

void Renderer::InitializeShaderBindingTable()
{
    vk::PhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingPipelineProperties = _vulkanContext->RayTracingPipelineProperties();
    const uint32_t baseAlignment = rayTracingPipelineProperties.shaderGroupBaseAlignment;
    const uint32_t handleSize = rayTracingPipelineProperties.shaderGroupHandleSize;
    const uint32_t shaderGroupCount = 3; // TODO: Get this from somewhere
    vk::DeviceSize sbtBufferSize = baseAlignment * shaderGroupCount;

    BufferCreation shaderBindingTableBufferCreation {};
    shaderBindingTableBufferCreation.SetName("Shader Binding Table")
        .SetSize(sbtBufferSize)
        .SetUsageFlags(vk::BufferUsageFlagBits::eShaderBindingTableKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress)
        .SetMemoryUsage(VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE)
        .SetIsMappable(true);
    _sbtBuffer = std::make_unique<Buffer>(shaderBindingTableBufferCreation, _vulkanContext);

    std::vector<uint8_t> handles = _vulkanContext->Device().getRayTracingShaderGroupHandlesKHR<uint8_t>(_pipeline, 0, shaderGroupCount, shaderGroupCount * handleSize, _vulkanContext->Dldi());

    vk::BufferDeviceAddressInfo sbtBufferDeviceAddressInfo {};
    sbtBufferDeviceAddressInfo.buffer = _sbtBuffer->buffer;
    vk::DeviceAddress sbtAddress = _vulkanContext->Device().getBufferAddress(sbtBufferDeviceAddressInfo);

    _raygenAddressRegion.deviceAddress = sbtAddress + baseAlignment * 0;
    _raygenAddressRegion.stride = baseAlignment;
    _raygenAddressRegion.size = baseAlignment;

    _missAddressRegion.deviceAddress = sbtAddress + baseAlignment * 1;
    _missAddressRegion.stride = baseAlignment;
    _missAddressRegion.size = baseAlignment;

    _hitAddressRegion.deviceAddress = sbtAddress + baseAlignment * 2;
    _hitAddressRegion.stride = baseAlignment;
    _hitAddressRegion.size = baseAlignment;

    uint8_t* sbtBufferData = static_cast<uint8_t*>(_sbtBuffer->mappedPtr);
    memcpy(sbtBufferData, handles.data(), handleSize);
    memcpy(sbtBufferData + baseAlignment, handles.data() + handleSize, handleSize);
    memcpy(sbtBufferData + baseAlignment * 2, handles.data() + handleSize * 2, handleSize);
}