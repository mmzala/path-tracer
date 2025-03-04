#pragma once

#include "resource_manager.hpp"
#include "gpu_resources.hpp"
#include <vulkan/vulkan.hpp>

class VulkanContext;

class ImageResources : public ResourceManager<Image>
{
public:
    explicit ImageResources(const std::shared_ptr<VulkanContext>& vulkanContext);
    ResourceHandle<Image> Create(const ImageCreation& creation);

private:
    std::shared_ptr<VulkanContext> _vulkanContext;
};

class MaterialResources : public ResourceManager<Material>
{
public:
    explicit MaterialResources(const std::shared_ptr<VulkanContext>& vulkanContext);
    ResourceHandle<Material> Create(const MaterialCreation& creation);

private:
    std::shared_ptr<VulkanContext> _vulkanContext;
};

class GeometryNodeResources : public ResourceManager<GeometryNode>
{
public:
    GeometryNodeResources() = default;
    ResourceHandle<GeometryNode> Create(const GeometryNodeCreation& creation);
};

class BindlessResources
{
public:
    explicit BindlessResources(const std::shared_ptr<VulkanContext>& vulkanContext);
    ~BindlessResources();
    void UpdateDescriptorSet();
    [[nodiscard]] ImageResources& Images() const { return *_imageResources; }
    [[nodiscard]] MaterialResources& Materials() const { return *_materialResources; }
    [[nodiscard]] GeometryNodeResources& GeometryNodes() const { return *_geometryNodeResources; }
    [[nodiscard]] const vk::DescriptorSetLayout& DescriptorSetLayout() const { return _bindlessLayout; }
    [[nodiscard]] const vk::DescriptorSet& DescriptorSet() const { return _bindlessSet; }

private:
    enum class BindlessBinding : uint8_t
    {
        eImages,
        eMaterials,
        eGeometryNodes,
    };

    static constexpr uint32_t MAX_RESOURCES = 1024;

    std::shared_ptr<VulkanContext> _vulkanContext;
    // TODO: Why are these pointers?
    std::unique_ptr<ImageResources> _imageResources;
    std::unique_ptr<MaterialResources> _materialResources;
    std::unique_ptr<GeometryNodeResources> _geometryNodeResources;
    std::unique_ptr<Buffer> _materialBuffer;
    std::unique_ptr<Buffer> _geometryNodeBuffer;

    vk::DescriptorPool _bindlessPool;
    vk::DescriptorSetLayout _bindlessLayout;
    vk::DescriptorSet _bindlessSet;

    ResourceHandle<Image> _fallbackImage;
    std::unique_ptr<Sampler> _fallbackSampler;

    void UploadImages();
    void UploadMaterials();
    void UploadGeometryNodes();
    void InitializeSet();
    void InitializeMaterialBuffer();
    void InitializeGeometryNodeBuffer();
};
