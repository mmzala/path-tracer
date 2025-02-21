#pragma once
#include "acceleration_structure.hpp"
#include "common.hpp"

class VulkanContext;
class BottomLevelAccelerationStructure;

class TopLevelAccelerationStructure : public AccelerationStructure
{
public:
    TopLevelAccelerationStructure(const std::vector<BottomLevelAccelerationStructure>& blases, const std::shared_ptr<VulkanContext>& vulkanContext);
    ~TopLevelAccelerationStructure();
    NON_COPYABLE(TopLevelAccelerationStructure);
    NON_MOVABLE(TopLevelAccelerationStructure);

    [[nodiscard]] vk::AccelerationStructureKHR Structure() const { return _vkStructure; }

private:
    void InitializeStructure(const std::vector<BottomLevelAccelerationStructure>& blases);

    std::shared_ptr<VulkanContext> _vulkanContext;
};
