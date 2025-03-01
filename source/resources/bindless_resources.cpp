#include "resources/bindless_resources.hpp"
#include "vulkan_context.hpp"
#include "vk_common.hpp"

ImageResources::ImageResources(const std::shared_ptr<VulkanContext>& vulkanContext)
    : _vulkanContext(vulkanContext)
{
}

ResourceHandle<Image> ImageResources::Create(const ImageCreation& creation)
{
    return ResourceManager::Create(Image(creation, _vulkanContext));
}

MaterialResources::MaterialResources(const std::shared_ptr<VulkanContext>& vulkanContext)
    : _vulkanContext(vulkanContext)
{
}

ResourceHandle<Material> MaterialResources::Create(const MaterialCreation& creation)
{
    return ResourceManager::Create(Material(creation));
}

BindlessResources::BindlessResources(const std::shared_ptr<VulkanContext>& vulkanContext)
    : _vulkanContext(vulkanContext), _imageResources(std::make_unique<ImageResources>(vulkanContext)), _materialResources(std::make_unique<MaterialResources>(vulkanContext))
{
    InitializeSet();
    InitializeMaterialBuffer();
}

BindlessResources::~BindlessResources()
{
    _vulkanContext->Device().destroy(_bindlessLayout);
    _vulkanContext->Device().destroy(_bindlessPool);
}

void BindlessResources::UpdateDescriptorSet()
{
    UploadImages();
    UploadMaterials();
}

void BindlessResources::UploadImages()
{
}

void BindlessResources::UploadMaterials()
{
}

void BindlessResources::InitializeSet()
{
    std::array<vk::DescriptorPoolSize, 2> poolSizes
    {
        vk::DescriptorPoolSize { vk::DescriptorType::eCombinedImageSampler, MAX_RESOURCES },
        vk::DescriptorPoolSize { vk::DescriptorType::eUniformBuffer, 1 },
    };

    vk::DescriptorPoolCreateInfo poolCreateInfo {};
    poolCreateInfo.flags = vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind;
    poolCreateInfo.maxSets = MAX_RESOURCES * poolSizes.size();
    poolCreateInfo.poolSizeCount = poolSizes.size();
    poolCreateInfo.pPoolSizes = poolSizes.data();
    VkCheckResult(_vulkanContext->Device().createDescriptorPool(&poolCreateInfo, nullptr, &_bindlessPool), "Failed creating bindless pool");

    std::vector<vk::DescriptorSetLayoutBinding> bindings(2);

    vk::DescriptorSetLayoutBinding& combinedImageSampler = bindings[0];
    combinedImageSampler.descriptorType = vk::DescriptorType::eCombinedImageSampler;
    combinedImageSampler.descriptorCount = MAX_RESOURCES;
    combinedImageSampler.binding = static_cast<uint32_t>(BindlessBinding::eImages);
    combinedImageSampler.stageFlags = vk::ShaderStageFlagBits::eClosestHitKHR;

    vk::DescriptorSetLayoutBinding& materialBinding = bindings[1];
    materialBinding.descriptorType = vk::DescriptorType::eUniformBuffer;
    materialBinding.descriptorCount = 1;
    materialBinding.binding = static_cast<uint32_t>(BindlessBinding::eMaterials);
    materialBinding.stageFlags = vk::ShaderStageFlagBits::eClosestHitKHR;

    vk::StructureChain<vk::DescriptorSetLayoutCreateInfo, vk::DescriptorSetLayoutBindingFlagsCreateInfo> structureChain;

    auto& layoutCreateInfo = structureChain.get<vk::DescriptorSetLayoutCreateInfo>();
    layoutCreateInfo.bindingCount = bindings.size();
    layoutCreateInfo.pBindings = bindings.data();
    layoutCreateInfo.flags = vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool;

    std::array<vk::DescriptorBindingFlagsEXT, 2> bindingFlags = {
        vk::DescriptorBindingFlagBits::ePartiallyBound | vk::DescriptorBindingFlagBits::eUpdateAfterBind,
        vk::DescriptorBindingFlagBits::ePartiallyBound | vk::DescriptorBindingFlagBits::eUpdateAfterBind,
    };

    auto& extInfo = structureChain.get<vk::DescriptorSetLayoutBindingFlagsCreateInfoEXT>();
    extInfo.bindingCount = bindings.size();
    extInfo.pBindingFlags = bindingFlags.data();

    _bindlessLayout = _vulkanContext->Device().createDescriptorSetLayout(layoutCreateInfo);

    vk::DescriptorSetAllocateInfo allocInfo {};
    allocInfo.descriptorPool = _bindlessPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &_bindlessLayout;
    VkCheckResult(_vulkanContext->Device().allocateDescriptorSets(&allocInfo, &_bindlessSet), "Failed creating bindless descriptor set");

    VkNameObject(_bindlessSet, "Bindless Set", _vulkanContext);
}

void BindlessResources::InitializeMaterialBuffer()
{
    BufferCreation creation {};
    creation.SetSize(MAX_RESOURCES * sizeof(Material))
        .SetUsageFlags(vk::BufferUsageFlagBits::eUniformBuffer)
        .SetName("Material buffer");

    _materialBuffer = std::make_unique<Buffer>(creation, _vulkanContext);
}


