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
    : _vulkanContext(vulkanContext), _imageResources(std::make_unique<ImageResources>(vulkanContext))
{
    std::array<vk::DescriptorPoolSize, 1> poolSizes = {
        vk::DescriptorPoolSize { vk::DescriptorType::eCombinedImageSampler, _maxResources },
    };

    vk::DescriptorPoolCreateInfo poolCreateInfo {};
    poolCreateInfo.flags = vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind;
    poolCreateInfo.maxSets = _maxResources * poolSizes.size();
    poolCreateInfo.poolSizeCount = poolSizes.size();
    poolCreateInfo.pPoolSizes = poolSizes.data();
    VkCheckResult(_vulkanContext->Device().createDescriptorPool(&poolCreateInfo, nullptr, &_bindlessPool), "Failed creating bindless pool");

    std::vector<vk::DescriptorSetLayoutBinding> bindings(1);
    vk::DescriptorSetLayoutBinding& combinedImageSampler = bindings[0];
    combinedImageSampler.descriptorType = vk::DescriptorType::eCombinedImageSampler;
    combinedImageSampler.descriptorCount = _maxResources;
    combinedImageSampler.binding = static_cast<uint32_t>(BindlessBinding::eImage);
    combinedImageSampler.stageFlags = vk::ShaderStageFlagBits::eClosestHitKHR;

    vk::StructureChain<vk::DescriptorSetLayoutCreateInfo, vk::DescriptorSetLayoutBindingFlagsCreateInfo> structureChain;

    auto& layoutCreateInfo = structureChain.get<vk::DescriptorSetLayoutCreateInfo>();
    layoutCreateInfo.bindingCount = bindings.size();
    layoutCreateInfo.pBindings = bindings.data();
    layoutCreateInfo.flags = vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool;

    std::array<vk::DescriptorBindingFlagsEXT, 1> bindingFlags = {
        vk::DescriptorBindingFlagBits::ePartiallyBound,
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

BindlessResources::~BindlessResources()
{
    _vulkanContext->Device().destroy(_bindlessLayout);
    _vulkanContext->Device().destroy(_bindlessPool);
}

void BindlessResources::UpdateDescriptorSet()
{

}


