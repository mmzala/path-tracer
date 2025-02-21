#pragma once
#include "acceleration_structure.hpp"
#include "common.hpp"
#include <glm/mat4x4.hpp>

class VulkanContext;
struct Model;
struct Buffer;

class BottomLevelAccelerationStructure : public AccelerationStructure
{
public:
    BottomLevelAccelerationStructure(const std::shared_ptr<Model>& model, const std::shared_ptr<VulkanContext>& vulkanContext, const glm::mat4& transform = glm::mat4(1.0f));
    ~BottomLevelAccelerationStructure();
    BottomLevelAccelerationStructure(BottomLevelAccelerationStructure&& other) noexcept;
    BottomLevelAccelerationStructure& operator=(BottomLevelAccelerationStructure&& other) = delete;
    NON_COPYABLE(BottomLevelAccelerationStructure);

    [[nodiscard]] vk::AccelerationStructureKHR Structure() const { return _vkStructure; }
    [[nodiscard]] const glm::mat4& Transform() const { return _transform; }

private:
    void InitializeTransformBuffer();
    void InitializeStructure();

    glm::mat4 _transform {};
    std::shared_ptr<Model> _model;
    std::unique_ptr<Buffer> _transformBuffer;

    std::shared_ptr<VulkanContext> _vulkanContext;
};
