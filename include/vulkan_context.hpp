#pragma once
#include <functional>
#include <optional>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>
#include "common.hpp"

struct VulkanInitInfo
{
    uint32_t extensionCount { 0 };
    const char* const* extensions { nullptr };
    uint32_t width {}, height {};

    std::function<vk::SurfaceKHR(vk::Instance)> retrieveSurface;
};

struct QueueFamilyIndices
{
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

   [[nodiscard]] bool IsComplete() const;

    static QueueFamilyIndices FindQueueFamilies(vk::PhysicalDevice device, vk::SurfaceKHR surface);
};

class VulkanContext
{
public:
    explicit VulkanContext(const VulkanInitInfo& initInfo);
    ~VulkanContext();
    NON_COPYABLE(VulkanContext);
    NON_MOVABLE(VulkanContext);

private:
    vk::Instance _instance;
    vk::DispatchLoaderDynamic _dldi;
    vk::PhysicalDevice _physicalDevice;
    vk::Device _device;
    vk::Queue _graphicsQueue;
    vk::Queue _presentQueue;
    QueueFamilyIndices _queueFamilyIndices;
    VmaAllocator _vmaAllocator;

    vk::SurfaceKHR _surface;

    vk::DebugUtilsMessengerEXT _debugMessenger;
    bool _validationLayersEnabled = false;

    const std::vector<const char*> _validationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };

    const std::vector<const char*> _deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };

    void InitializeInstance(const VulkanInitInfo& initInfo);
    void InizializeValidationLayers();
    void InitializePhysicalDevice();
    void InitializeDevice();
    void InitializeVMA();
    [[nodiscard]] bool AreValidationLayersSupported() const;
    [[nodiscard]] std::vector<const char*> GetRequiredExtensions(const VulkanInitInfo& initInfo) const;
    [[nodiscard]] uint32_t RateDeviceSuitability(const vk::PhysicalDevice& deviceToRate) const;
    [[nodiscard]] bool AreExtensionsSupported(const vk::PhysicalDevice& deviceToCheckSupport) const;
};