#pragma once

#include <memory>
#include <optional>
#include <vulkan/vulkan.hpp>
#include <vk_mem_alloc.h>
#include <glm/glm.hpp>
#include "common.hpp"
#include "resource_manager.hpp"

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
    Buffer(const BufferCreation& creation, const std::shared_ptr<VulkanContext>& vulkanContext);
    ~Buffer();
    NON_COPYABLE(Buffer);
    Buffer(Buffer&& other) noexcept;
    Buffer& operator=(Buffer&& other) noexcept;

    vk::Buffer buffer {};
    VmaAllocation allocation {};
    void* mappedPtr = nullptr;

private:
    std::shared_ptr<VulkanContext> _vulkanContext;
};

struct ImageCreation
{
    std::vector<std::byte> data {};
    uint32_t width {};
    uint32_t height {};
    vk::Format format = vk::Format::eUndefined;
    vk::ImageUsageFlags usage { 0 };
    std::string name {};

    ImageCreation& SetData(const std::vector<std::byte>& data);
    ImageCreation& SetSize(uint32_t width, uint32_t height);
    ImageCreation& SetFormat(vk::Format format);
    ImageCreation& SetUsageFlags(vk::ImageUsageFlags usage);
    ImageCreation& SetName(std::string_view name);
};

struct Image
{
    Image(const ImageCreation& creation, const std::shared_ptr<VulkanContext>& vulkanContext);
    ~Image();
    NON_COPYABLE(Image);
    Image(Image&& other) noexcept;
    Image& operator=(Image&& other) noexcept;

    vk::Image image {};
    vk::ImageView view {};
    VmaAllocation allocation {};
    vk::Format format {};

private:
    std::shared_ptr<VulkanContext> _vulkanContext;
};

struct MaterialCreation
{
    ResourceHandle<Image> albedoMap = ResourceHandle<Image>::Null();
    glm::vec4 albedoFactor { 0.0f };
    uint32_t albedoUVChannel = 0;

    ResourceHandle<Image> metallicRoughnessMap = ResourceHandle<Image>::Null();
    float metallicFactor = 0.0f;
    float roughnessFactor = 0.0f;
    std::optional<uint32_t> metallicRoughnessUVChannel {};

    ResourceHandle<Image> normalMap = ResourceHandle<Image>::Null();
    float normalScale = 0.0f;
    uint32_t normalUVChannel = 0;

    ResourceHandle<Image> occlusionMap = ResourceHandle<Image>::Null();
    float occlusionStrength = 0.0f;
    uint32_t occlusionUVChannel = 0;

    ResourceHandle<Image> emissiveMap = ResourceHandle<Image>::Null();
    glm::vec3 emissiveFactor { 0.0f };
    uint32_t emissiveUVChannel = 0;
};

struct Material
{
    Material(const MaterialCreation& creation);

    glm::vec4 albedoFactor { 0.0f };

    float metallicFactor = 0.0f;
    float roughnessFactor = 0.0f;
    float normalScale = 1.0f;
    float occlusionStrength = 0.0f;

    glm::vec3 emissiveFactor { 0.0f };
    int32_t useEmissiveMap = false;

    int32_t useAlbedoMap = false;
    int32_t useMetallicRoughnessMap = false;
    int32_t useNormalMap = false;
    int32_t useOcclusionMap = false;

    uint32_t albedoMapIndex = NULL_RESOURCE_INDEX_VALUE;
    uint32_t metallicRoughnessMapIndex = NULL_RESOURCE_INDEX_VALUE;
    uint32_t normalMapIndex = NULL_RESOURCE_INDEX_VALUE;
    uint32_t occlusionMapIndex = NULL_RESOURCE_INDEX_VALUE;

    uint32_t emissiveMapIndex = NULL_RESOURCE_INDEX_VALUE;
};
