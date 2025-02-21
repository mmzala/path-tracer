#pragma once
#include <memory>
#include <vulkan/vulkan.hpp>

struct Buffer;

struct AccelerationStructure
{
protected:
    vk::AccelerationStructureKHR _vkStructure;
    std::unique_ptr<Buffer> _structureBuffer;
    std::unique_ptr<Buffer> _scratchBuffer;
    std::unique_ptr<Buffer> _instancesBuffer;
};
