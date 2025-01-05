#pragma once
#include <memory>
#include <vulkan/vulkan.hpp>
#include "vk_common.hpp"
#include "common.hpp"

class VulkanContext;

class Renderer
{
public:
    Renderer(std::shared_ptr<VulkanContext> vulkanContext);
    ~Renderer();
    NON_COPYABLE(Renderer);
    NON_MOVABLE(Renderer);

private:
    void InitializeCommandBuffers();

    std::shared_ptr<VulkanContext> _vulkanContext;
    std::array<vk::CommandBuffer, MAX_FRAMES_IN_FLIGHT> _commandBuffers;
};