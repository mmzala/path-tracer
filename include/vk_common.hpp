#pragma once
#include <vulkan/vulkan.hpp>
#include "vulkan_context.hpp"

constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;

void VkCheckResult(vk::Result result, std::string_view message);
void VkCheckResult(VkResult result, std::string_view message);

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