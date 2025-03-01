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
        eImages,
        eMaterials,
    };

    static constexpr uint32_t MAX_RESOURCES = 1024;

    std::shared_ptr<VulkanContext> _vulkanContext;
    std::unique_ptr<ImageResources> _imageResources;
    std::unique_ptr<MaterialResources> _materialResources;
    std::unique_ptr<Buffer> _materialBuffer;

    vk::DescriptorPool _bindlessPool;
    vk::DescriptorSetLayout _bindlessLayout;
    vk::DescriptorSet _bindlessSet;

    void UploadImages();
    void UploadMaterials();
    void InitializeSet();
    void InitializeMaterialBuffer();
};
