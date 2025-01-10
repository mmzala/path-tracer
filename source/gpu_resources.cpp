#include "gpu_resources.hpp"
#include "vk_common.hpp"

BufferCreation& BufferCreation::SetSize(vk::DeviceSize size)
{
    this->size = size;
    return *this;
}

BufferCreation& BufferCreation::SetUsageFlags(vk::BufferUsageFlags usage)
{
    this->usage = usage;
    return *this;
}

BufferCreation& BufferCreation::SetIsMappable(bool isMappable)
{
    this->isMappable = isMappable;
    return *this;
}

BufferCreation& BufferCreation::SetMemoryUsage(VmaMemoryUsage memoryUsage)
{
    this->memoryUsage = memoryUsage;
    return *this;
}

BufferCreation& BufferCreation::SetName(std::string_view name)
{
    this->name = name;
    return *this;
}

Buffer::Buffer(const BufferCreation& creation, std::shared_ptr<VulkanContext> vulkanContext)
    : _vulkanContext(vulkanContext)
{
    vk::BufferCreateInfo bufferInfo {};
    bufferInfo.size = creation.size;
    bufferInfo.usage = creation.usage;
    bufferInfo.sharingMode = vk::SharingMode::eExclusive;
    bufferInfo.queueFamilyIndexCount = 1;
    bufferInfo.pQueueFamilyIndices = &_vulkanContext->QueueFamilies().graphicsFamily.value();

    VmaAllocationCreateInfo allocationInfo {};
    allocationInfo.usage = creation.memoryUsage;
    if (creation.isMappable)
    {
        allocationInfo.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    }

    VkCheckResult(vmaCreateBuffer(_vulkanContext->MemoryAllocator(), reinterpret_cast<VkBufferCreateInfo*>(&bufferInfo), &allocationInfo, reinterpret_cast<VkBuffer*>(&buffer), &allocation, nullptr), "[VULKAN] Failed creating buffer!");
    vmaSetAllocationName(_vulkanContext->MemoryAllocator(), allocation, creation.name.data());
    VkNameObject(buffer, creation.name, _vulkanContext);

    if (creation.isMappable)
    {
        VkCheckResult(vmaMapMemory(_vulkanContext->MemoryAllocator(), allocation, &mappedPtr),
            "[VULKAN] Failed mapping memory for buffer: " + creation.name);
    }
}

Buffer::~Buffer()
{
    if (!_vulkanContext)
    {
        return;
    }

    if (mappedPtr)
    {
        vmaUnmapMemory(_vulkanContext->MemoryAllocator(), allocation);
    }

    vmaDestroyBuffer(_vulkanContext->MemoryAllocator(), buffer, allocation);
}