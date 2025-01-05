#pragma once
#include "common.hpp"
#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>

class VulkanContext;

class SwapChain
{
public:
    struct SupportDetails
    {
        vk::SurfaceCapabilitiesKHR capabilities;
        std::vector<vk::SurfaceFormatKHR> formats;
        std::vector<vk::PresentModeKHR> presentModes;
    };

    SwapChain(std::shared_ptr<VulkanContext> vulkanContext, glm::uvec2 screenSize);
    ~SwapChain();
    NON_COPYABLE(SwapChain);
    NON_MOVABLE(SwapChain);

    static SupportDetails QuerySupport(vk::PhysicalDevice device, vk::SurfaceKHR surface);

private:
    void InitializeSwapChain(glm::uvec2 screenSize);
    void CleanUp();
    void InitializeImageViews();
    vk::SurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats) const;
    vk::PresentModeKHR ChoosePresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes) const;
    vk::Extent2D ChooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities, glm::uvec2 screenSize) const;

    std::shared_ptr<VulkanContext> _vulkanContext;
    vk::SwapchainKHR _swapChain;
    std::vector<vk::Image> _images;
    std::vector<vk::ImageView> _imageViews;
    vk::Format _format;
    vk::Extent2D _extent;
};