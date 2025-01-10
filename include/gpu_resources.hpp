#pragma once

#include <memory>
#include <vulkan/vulkan.hpp>
#include <vk_mem_alloc.h>
#include "common.hpp"

class VulkanContext;

struct BufferCreation
{
    vk::DeviceSize size {};
    vk::BufferUsageFlags usage {};
    bool isMappable = true;
    VmaMemoryUsage memoryUsage = VMA_MEMORY_USAGE_CPU_ONLY;
    std::string name {};

    BufferCreation& SetSize(vk::DeviceSize size);
    BufferCreation& SetUsageFlags(vk::BufferUsageFlags usage);
    BufferCreation& SetIsMappable(bool isMappable);
    BufferCreation& SetMemoryUsage(VmaMemoryUsage memoryUsage);
    BufferCreation& SetName(std::string_view name);
};

struct Buffer
{
    Buffer(const BufferCreation& creation, std::shared_ptr<VulkanContext> vulkanContext);
    ~Buffer();
    NON_COPYABLE(Buffer);
    NON_MOVABLE(Buffer);

    vk::Buffer buffer {};
    VmaAllocation allocation {};
    void* mappedPtr = nullptr;

private:
    std::shared_ptr<VulkanContext> _vulkanContext;
};