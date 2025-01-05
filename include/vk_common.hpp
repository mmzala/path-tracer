#pragma once
#include <vulkan/vulkan.hpp>
#include "vulkan_context.hpp"

constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;

struct ImageLayoutTransitionState
{
    vk::PipelineStageFlags2 pipelineStage {};
    vk::AccessFlags2 accessFlags {};
};

void VkCheckResult(vk::Result result, std::string_view message);
void VkCheckResult(VkResult result, std::string_view message);

bool VkHasStencilComponent(vk::Format format);
ImageLayoutTransitionState VkGetImageLayoutTransitionSourceState(vk::ImageLayout sourceLayout);
ImageLayoutTransitionState VkGetImageLayoutTransitionDestinationState(vk::ImageLayout destinationLayout);
void VkInitializeImageMemoryBarrier(vk::ImageMemoryBarrier2& barrier, vk::Image image, vk::Format format, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, uint32_t numLayers = 1, uint32_t mipLevel = 0, uint32_t mipCount = 1, vk::ImageAspectFlagBits imageAspect = vk::ImageAspectFlagBits::eColor);
void VkTransitionImageLayout(vk::CommandBuffer commandBuffer, vk::Image image, vk::Format format, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, uint32_t numLayers = 1, uint32_t mipLevel = 0, uint32_t mipCount = 1, vk::ImageAspectFlagBits imageAspect = vk::ImageAspectFlagBits::eColor);

template <typename T>
static void VkNameObject(T object, std::string_view name, std::shared_ptr<VulkanContext> context)
{
#if defined(NDEBUG)
    return;
#endif

    vk::DebugUtilsObjectNameInfoEXT nameInfo {};
    nameInfo.pObjectName = name.data();
    nameInfo.objectType = object.objectType;
    nameInfo.objectHandle = reinterpret_cast<uint64_t>(static_cast<typename T::CType>(object));

    VkCheckResult(context->Device().setDebugUtilsObjectNameEXT(&nameInfo, context->Dldi()), "[VULKAN] Failed debug naming object!");
}