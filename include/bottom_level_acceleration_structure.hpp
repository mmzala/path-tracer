#pragma once
#include "acceleration_structure.hpp"
#include "common.hpp"

class VulkanContext;
struct Model;
struct Buffer;

class BottomLevelAccelerationStructure : public AccelerationStructure
{
public:
    BottomLevelAccelerationStructure(const std::shared_ptr<Model>& model, const std::shared_ptr<VulkanContext>& vulkanContext);
    ~BottomLevelAccelerationStructure();
    NON_COPYABLE(BottomLevelAccelerationStructure);
    NON_MOVABLE(BottomLevelAccelerationStructure);

    [[nodiscard]] vk::AccelerationStructureKHR Structure() const { return _vkStructure; }

private:
    void InitializeTransformBuffer();
    void InitializeStructure();

    std::shared_ptr<Model> _model;
    std::unique_ptr<Buffer> _transformBuffer;

    std::shared_ptr<VulkanContext> _vulkanContext;
};
