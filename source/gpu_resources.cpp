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

ImageCreation& ImageCreation::SetSize(uint32_t width, uint32_t height)
{
    this->width = width;
    this->height = height;
    return *this;
}

ImageCreation& ImageCreation::SetFormat(vk::Format format)
{
    this->format = format;
    return *this;
}

ImageCreation& ImageCreation::SetUsageFlags(vk::ImageUsageFlags usage)
{
    this->usage = usage;
    return *this;
}

ImageCreation& ImageCreation::SetName(std::string_view name)
{
    this->name = name;
    return *this;
}

Image::Image(const ImageCreation& creation, std::shared_ptr<VulkanContext> vulkanContext)
    : _vulkanContext(vulkanContext)
{
    format = creation.format;

    vk::ImageCreateInfo imageCreateInfo {};
    imageCreateInfo.imageType = vk::ImageType::e2D;
    imageCreateInfo.extent.width = creation.width;
    imageCreateInfo.extent.height = creation.height;
    imageCreateInfo.extent.depth = 1;
    imageCreateInfo.mipLevels = 1;
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.format = creation.format;
    imageCreateInfo.tiling = vk::ImageTiling::eOptimal;
    imageCreateInfo.initialLayout = vk::ImageLayout::eUndefined;
    imageCreateInfo.sharingMode = vk::SharingMode::eExclusive;
    imageCreateInfo.samples = vk::SampleCountFlagBits::e1;
    imageCreateInfo.usage = creation.usage;

    VmaAllocationCreateInfo allocCreateInfo {};
    allocCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    vmaCreateImage(_vulkanContext->MemoryAllocator(), reinterpret_cast<VkImageCreateInfo*>(&imageCreateInfo), &allocCreateInfo, reinterpret_cast<VkImage*>(&image), &allocation, nullptr);
    std::string allocName = creation.name + " texture allocation";
    vmaSetAllocationName(_vulkanContext->MemoryAllocator(), allocation, allocName.c_str());

    vk::ImageViewCreateInfo viewCreateInfo {};
    viewCreateInfo.image = image;
    viewCreateInfo.viewType = vk::ImageViewType::e2D;
    viewCreateInfo.format = creation.format;
    viewCreateInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    viewCreateInfo.subresourceRange.baseMipLevel = 0;
    viewCreateInfo.subresourceRange.levelCount = 1;
    viewCreateInfo.subresourceRange.baseArrayLayer = 0;
    viewCreateInfo.subresourceRange.layerCount = 1;
    view = _vulkanContext->Device().createImageView(viewCreateInfo);

    VkNameObject(image, creation.name, _vulkanContext);
}

Image::~Image()
{
    _vulkanContext->Device().destroy(view);
    vmaDestroyImage(_vulkanContext->MemoryAllocator(), image, allocation);
}

