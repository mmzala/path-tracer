#pragma once
#include <vulkan/vulkan.hpp>

void VkCheckResult(vk::Result result, std::string_view message);
void VkCheckResult(VkResult result, std::string_view message);