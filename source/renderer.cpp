#include "renderer.hpp"
#include "swap_chain.hpp"
#include "vulkan_context.hpp"
#include "gpu_resources.hpp"

Renderer::Renderer(const VulkanInitInfo& initInfo, std::shared_ptr<VulkanContext> vulkanContext)
    : _vulkanContext(vulkanContext)
{
    _swapChain = std::make_unique<SwapChain>(vulkanContext, glm::uvec2 { initInfo.width, initInfo.height });
    InitializeCommandBuffers();
    InitializeSynchronizationObjects();

    InitializeTriangle();
}

Renderer::~Renderer()
{
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
    VkTransitionImageLayout(commandBuffer, _swapChain->GetImage(swapChainImageIndex), _swapChain->GetFormat(),
        vk::ImageLayout::eUndefined, vk::ImageLayout::ePresentSrcKHR);
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

void Renderer::InitializeTriangle()
{
    const std::vector<Vertex> vertices =
    {
        { { 1.0f, 1.0f, 0.0f } },
        { { -1.0f, 1.0f, 0.0f } },
        { { 0.0f, -1.0f, 0.0f } }
    };

    const std::vector<uint32_t> indices = { 0, 1, 2 };

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
}