#include "swap_chain.hpp"
#include "vk_common.hpp"

SwapChain::SupportDetails SwapChain::QuerySupport(vk::PhysicalDevice device, vk::SurfaceKHR surface)
{
    SupportDetails details {};

    VkCheckResult(device.getSurfaceCapabilitiesKHR(surface, &details.capabilities), "[VULKAN] Failed getting surface capabilities from physical device!");

    details.formats = device.getSurfaceFormatsKHR(surface);
    details.presentModes = device.getSurfacePresentModesKHR(surface);

    return details;
}