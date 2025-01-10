#pragma once

#include <functional>
#include <vulkan/vulkan.hpp>
#include "common.hpp"

class VulkanContext;

class SingleTimeCommands
{
public:
    SingleTimeCommands(std::shared_ptr<VulkanContext> context);
    ~SingleTimeCommands();
    NON_MOVABLE(SingleTimeCommands);
    NON_COPYABLE(SingleTimeCommands);

    void Record(const std::function<void(vk::CommandBuffer, std::shared_ptr<VulkanContext>)>& commands) const;
    void Submit();

private:
    std::shared_ptr<VulkanContext> _vulkanContext;
    vk::CommandBuffer _commandBuffer;
    vk::Fence _fence;
    bool _submitted = false;
};