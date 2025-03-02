#include "resources/bindless_resources.hpp"
#include <spdlog/spdlog.h>
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

    constexpr uint32_t size = 2;
    std::vector<std::byte> data {};
    data.assign(size * size * 4, std::byte {});
    ImageCreation fallbackImageCreation {};
    fallbackImageCreation.SetName("Fallback texture")
        .SetSize(size, size)
        .SetUsageFlags(vk::ImageUsageFlagBits::eSampled)
        .SetFormat(vk::Format::eR8G8B8A8Unorm)
        .SetData(data);
    _fallbackImage = _imageResources->Create(fallbackImageCreation);
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
    if (_imageResources->GetAll().empty())
    {
        return;
    }

    if (_imageResources->GetAll().size() < MAX_RESOURCES)
    {
        spdlog::error("[RESOURCES] Too many images to fit into the bindless set");
        return;
    }

    std::array<vk::DescriptorImageInfo, MAX_RESOURCES> imageInfos {};
    std::array<vk::WriteDescriptorSet, MAX_RESOURCES> descriptorWrites {};

    for (uint32_t i = 0; i < MAX_RESOURCES; ++i)
    {
        // TODO: Default image and sampler, then fill image infos
    }

    _vulkanContext->Device().updateDescriptorSets(MAX_RESOURCES, descriptorWrites.data(), 0, nullptr);
}

void BindlessResources::UploadMaterials()
{
    if (_materialResources->GetAll().empty())
    {
        return;
    }

    if (_materialResources->GetAll().size() < MAX_RESOURCES)
    {
        spdlog::error("[RESOURCES] Material buffer is too small to fit all of the available materials");
        return;
    }

    std::memcpy(_materialBuffer->mappedPtr, _materialResources->GetAll().data(), _materialResources->GetAll().size() * sizeof(Material));

    vk::DescriptorBufferInfo bufferInfo {};
    bufferInfo.buffer = _materialBuffer->buffer;
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(Material) * _materialResources->GetAll().size();

    vk::WriteDescriptorSet descriptorWrite {};
    descriptorWrite.dstSet = _bindlessSet;
    descriptorWrite.dstBinding = static_cast<uint32_t>(BindlessBinding::eMaterials);
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = vk::DescriptorType::eUniformBuffer;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo = &bufferInfo;

    _vulkanContext->Device().updateDescriptorSets(1, &descriptorWrite, 0, nullptr);
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
        .SetIsMappable(true)
        .SetName("Material buffer");

    _materialBuffer = std::make_unique<Buffer>(creation, _vulkanContext);
}


