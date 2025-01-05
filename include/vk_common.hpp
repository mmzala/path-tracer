#pragma once
#include <vulkan/vulkan.hpp>

constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;

void VkCheckResult(vk::Result result, std::string_view message);
void VkCheckResult(VkResult result, std::string_view message);