#pragma once
#include <vulkan/vulkan.hpp>

class SwapChain
{
public:
    struct SupportDetails
    {
        vk::SurfaceCapabilitiesKHR capabilities;
        std::vector<vk::SurfaceFormatKHR> formats;
        std::vector<vk::PresentModeKHR> presentModes;
    };

    static SupportDetails QuerySupport(vk::PhysicalDevice device, vk::SurfaceKHR surface);
};