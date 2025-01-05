#include "vk_common.hpp"
#include <spdlog/spdlog.h>
#include <unordered_map>

void VkCheckResult(vk::Result result, std::string_view message)
{
    if (result == vk::Result::eSuccess)
    {
        return;
    }

    spdlog::error("[VULKAN] {}", message);

    abort();
}

void VkCheckResult(VkResult result, std::string_view message)
{
    VkCheckResult(static_cast<vk::Result>(result), message);
}

bool VkHasStencilComponent(vk::Format format)
{
    return format == vk::Format::eD32SfloatS8Uint || format == vk::Format::eD24UnormS8Uint;
}

ImageLayoutTransitionState VkGetImageLayoutTransitionSourceState(vk::ImageLayout sourceLayout)
{
    static const std::unordered_map<vk::ImageLayout, ImageLayoutTransitionState> sourceStateMap = {
        { vk::ImageLayout::eUndefined,
            { .pipelineStage = vk::PipelineStageFlagBits2::eTopOfPipe,
                .accessFlags = vk::AccessFlags2 { 0 } } },
        { vk::ImageLayout::eTransferDstOptimal,
            { .pipelineStage = vk::PipelineStageFlagBits2::eTransfer,
                .accessFlags = vk::AccessFlagBits2::eTransferWrite } },
        { vk::ImageLayout::eTransferSrcOptimal,
            { .pipelineStage = vk::PipelineStageFlagBits2::eTransfer,
                .accessFlags = vk::AccessFlagBits2::eTransferWrite } },
        { vk::ImageLayout::eShaderReadOnlyOptimal,
            { .pipelineStage = vk::PipelineStageFlagBits2::eFragmentShader,
                .accessFlags = vk::AccessFlagBits2::eShaderRead } },
        { vk::ImageLayout::eColorAttachmentOptimal,
            { .pipelineStage = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                .accessFlags = vk::AccessFlagBits2::eColorAttachmentWrite } },
        { vk::ImageLayout::eDepthStencilAttachmentOptimal,
            { .pipelineStage = vk::PipelineStageFlagBits2::eLateFragmentTests,
                .accessFlags = vk::AccessFlagBits2::eDepthStencilAttachmentWrite } },
    };

    auto it = sourceStateMap.find(sourceLayout);
    if (it == sourceStateMap.end())
    {
        spdlog::error("[VULKAN] Unsupported source state for image layout transition!");
        return ImageLayoutTransitionState {};
    }

    return it->second;
}

ImageLayoutTransitionState VkGetImageLayoutTransitionDestinationState(vk::ImageLayout destinationLayout)
{
    static const std::unordered_map<vk::ImageLayout, ImageLayoutTransitionState> destinationStateMap = {
        { vk::ImageLayout::eTransferDstOptimal,
            { .pipelineStage = vk::PipelineStageFlagBits2::eTransfer,
                .accessFlags = vk::AccessFlagBits2::eTransferWrite } },
        { vk::ImageLayout::eTransferSrcOptimal,
            { .pipelineStage = vk::PipelineStageFlagBits2::eTransfer,
                .accessFlags = vk::AccessFlagBits2::eTransferRead } },
        { vk::ImageLayout::eShaderReadOnlyOptimal,
            { .pipelineStage = vk::PipelineStageFlagBits2::eFragmentShader,
                .accessFlags = vk::AccessFlagBits2::eShaderRead } },
        { vk::ImageLayout::eColorAttachmentOptimal,
            { .pipelineStage = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                .accessFlags = vk::AccessFlagBits2::eColorAttachmentWrite } },
        { vk::ImageLayout::eDepthStencilAttachmentOptimal,
            { .pipelineStage = vk::PipelineStageFlagBits2::eEarlyFragmentTests,
                .accessFlags = vk::AccessFlagBits2::eDepthStencilAttachmentRead | vk::AccessFlagBits2::eDepthStencilAttachmentWrite } },
        { vk::ImageLayout::ePresentSrcKHR,
            { .pipelineStage = vk::PipelineStageFlagBits2::eBottomOfPipe,
                .accessFlags = vk::AccessFlags2 { 0 } } },
        { vk::ImageLayout::eDepthStencilReadOnlyOptimal,
            { .pipelineStage = vk::PipelineStageFlagBits2::eEarlyFragmentTests,
                .accessFlags = vk::AccessFlagBits2::eDepthStencilAttachmentRead } }
    };

    auto it = destinationStateMap.find(destinationLayout);
    if (it == destinationStateMap.end())
    {
        spdlog::error("[VULKAN] Unsupported destination state for image layout transition!");
        return ImageLayoutTransitionState {};
    }

    return it->second;
}

void VkInitializeImageMemoryBarrier(vk::ImageMemoryBarrier2& barrier, vk::Image image, vk::Format format, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, uint32_t numLayers, uint32_t mipLevel, uint32_t mipCount, vk::ImageAspectFlagBits imageAspect)
{
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = vk::QueueFamilyIgnored;
    barrier.dstQueueFamilyIndex = vk::QueueFamilyIgnored;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = imageAspect;
    barrier.subresourceRange.baseMipLevel = mipLevel;
    barrier.subresourceRange.levelCount = mipCount;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = numLayers;

    if (newLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal)
    {
        barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
        if (VkHasStencilComponent(format))
        {
            barrier.subresourceRange.aspectMask |= vk::ImageAspectFlagBits::eStencil;
        }
    }

    const ImageLayoutTransitionState sourceState = VkGetImageLayoutTransitionSourceState(oldLayout);
    const ImageLayoutTransitionState destinationState = VkGetImageLayoutTransitionDestinationState(newLayout);

    barrier.srcStageMask = sourceState.pipelineStage;
    barrier.srcAccessMask = sourceState.accessFlags;
    barrier.dstStageMask = destinationState.pipelineStage;
    barrier.dstAccessMask = destinationState.accessFlags;
}

void VkTransitionImageLayout(vk::CommandBuffer commandBuffer, vk::Image image, vk::Format format, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, uint32_t numLayers, uint32_t mipLevel, uint32_t mipCount, vk::ImageAspectFlagBits imageAspect)
{
    vk::ImageMemoryBarrier2 barrier {};
    VkInitializeImageMemoryBarrier(barrier, image, format, oldLayout, newLayout, numLayers, mipLevel, mipCount, imageAspect);

    vk::DependencyInfo dependencyInfo {};
    dependencyInfo.setImageMemoryBarrierCount(1)
        .setPImageMemoryBarriers(&barrier);

    commandBuffer.pipelineBarrier2(dependencyInfo);
}