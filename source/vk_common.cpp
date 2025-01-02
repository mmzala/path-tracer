#include "vk_common.hpp"
#include <spdlog/spdlog.h>

void VkCheckResult(vk::Result result, std::string_view message)
{
    if (result == vk::Result::eSuccess)
    {
        return;
    }

    spdlog::error("[VULKAN] {}", message);

    abort();
}