#pragma once

#include "resource_manager.hpp"
#include "gpu_resources.hpp"
#include <vulkan/vulkan.hpp>

class VulkanContext;

class ImageResources : public ResourceManager<Image>
{
public:
    ImageResources(const std::shared_ptr<VulkanContext>& vulkanContext);
    ResourceHandle<Image> Create(const ImageCreation& creation);

private:
    std::shared_ptr<VulkanContext> _vulkanContext;
};

class MaterialResources : public ResourceManager<Material>
{
public:
    MaterialResources(const std::shared_ptr<VulkanContext>& vulkanContext);
    ResourceHandle<Material> Create(const MaterialCreation& creation);

private:
    std::shared_ptr<VulkanContext> _vulkanContext;
};

class BindlessResources
{
public:
    BindlessResources(const std::shared_ptr<VulkanContext>& vulkanContext);
    ~BindlessResources();
    void UpdateDescriptorSet();
    const ImageResources& Images() const { return *_imageResources; }

private:
    enum class BindlessBinding : uint8_t
    {
        eImage
    };

    const uint32_t _maxResources = 1024;

    std::shared_ptr<VulkanContext> _vulkanContext;
    std::unique_ptr<ImageResources> _imageResources;

    vk::DescriptorPool _bindlessPool;
    vk::DescriptorSetLayout _bindlessLayout;
    vk::DescriptorSet _bindlessSet;
};
