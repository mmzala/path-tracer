#include "bottom_level_acceleration_structure.hpp"
#include "model_loader.hpp"
#include "resources/bindless_resources.hpp"
#include "single_time_commands.hpp"
#include "vulkan_context.hpp"
#include <glm/glm.hpp>

BottomLevelAccelerationStructure::BottomLevelAccelerationStructure(const std::shared_ptr<Model>& model, const std::shared_ptr<BindlessResources>& resources, const std::shared_ptr<VulkanContext>& vulkanContext, const glm::mat4& transform)
    : _transform(transform)
    , _model(model)
    , _vulkanContext(vulkanContext)
{
    InitializeTransformBuffer();
    InitializeStructure(resources);
}

BottomLevelAccelerationStructure::~BottomLevelAccelerationStructure()
{
    _vulkanContext->Device().destroyAccelerationStructureKHR(_vkStructure, nullptr, _vulkanContext->Dldi());
}

BottomLevelAccelerationStructure::BottomLevelAccelerationStructure(BottomLevelAccelerationStructure&& other) noexcept
    : _transform(other._transform)
    , _model(other._model)
    , _transformBuffer(std::move(other._transformBuffer))
    , _vulkanContext(other._vulkanContext)
{
    _vkStructure = other._vkStructure;
    _structureBuffer = std::move(other._structureBuffer);
    _scratchBuffer = std::move(other._scratchBuffer);
    _instancesBuffer = std::move(other._instancesBuffer);
}

void BottomLevelAccelerationStructure::InitializeTransformBuffer()
{
    std::vector<vk::TransformMatrixKHR> transformMatrices {};
    for (const auto& node : _model->nodes)
    {
        if (node.meshes.empty())
        {
            continue;
        }

        vk::TransformMatrixKHR& matrix = transformMatrices.emplace_back();
        glm::mat4 transform = glm::transpose(node.GetWorldMatrix()); // VkTransformMatrixKHR uses a row-major memory layout, while glm::mat4 uses a column-major memory layout
        memcpy(&matrix, &transform, sizeof(vk::TransformMatrixKHR));
    }

    // TODO: Upload to GPU friendly memory
    BufferCreation transformBufferCreation {};
    transformBufferCreation.SetName("Transforms Buffer")
        .SetUsageFlags(vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress)
        .SetMemoryUsage(VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE)
        .SetIsMappable(true)
        .SetSize(transformMatrices.size() * sizeof(VkTransformMatrixKHR));

    _transformBuffer = std::make_unique<Buffer>(transformBufferCreation, _vulkanContext);
    memcpy(_transformBuffer->mappedPtr, transformMatrices.data(), transformMatrices.size() * sizeof(VkTransformMatrixKHR));
}

void BottomLevelAccelerationStructure::InitializeStructure(const std::shared_ptr<BindlessResources>& resources)
{
    uint32_t nodeCount = 0;
    uint32_t maxPrimitiveCount = 0;
    std::vector<uint32_t> maxPrimitiveCounts {};
    std::vector<vk::AccelerationStructureGeometryKHR> geometries {};
    std::vector<vk::AccelerationStructureBuildRangeInfoKHR> buildRangeInfos {};

    for (const auto& node : _model->nodes)
    {
        for (const auto meshIndex : node.meshes)
        {
            const Mesh& mesh = _model->meshes[meshIndex];

            vk::DeviceOrHostAddressConstKHR vertexBufferDeviceAddress {};
            vk::DeviceOrHostAddressConstKHR indexBufferDeviceAddress {};
            vk::DeviceOrHostAddressConstKHR transformBufferDeviceAddress {};

            vertexBufferDeviceAddress.deviceAddress = _vulkanContext->GetBufferDeviceAddress(_model->vertexBuffer->buffer);
            indexBufferDeviceAddress.deviceAddress = _vulkanContext->GetBufferDeviceAddress(_model->indexBuffer->buffer) + mesh.firstIndex * sizeof(uint32_t);
            transformBufferDeviceAddress.deviceAddress = _vulkanContext->GetBufferDeviceAddress(_transformBuffer->buffer) + nodeCount * sizeof(vk::TransformMatrixKHR);

            vk::AccelerationStructureGeometryTrianglesDataKHR trianglesData {};
            trianglesData.vertexFormat = vk::Format::eR32G32B32Sfloat;
            trianglesData.vertexData = vertexBufferDeviceAddress;
            trianglesData.maxVertex = _model->verticesCount;
            trianglesData.vertexStride = sizeof(Model::Vertex);
            trianglesData.indexType = vk::IndexType::eUint32;
            trianglesData.indexData = indexBufferDeviceAddress;
            trianglesData.transformData = transformBufferDeviceAddress;

            vk::AccelerationStructureGeometryKHR& accelerationStructureGeometry = geometries.emplace_back();
            accelerationStructureGeometry.flags = vk::GeometryFlagBitsKHR::eOpaque;
            accelerationStructureGeometry.geometryType = vk::GeometryTypeKHR::eTriangles;
            accelerationStructureGeometry.geometry.triangles = trianglesData;

            uint32_t primitiveCount = mesh.indexCount / 3;
            maxPrimitiveCounts.push_back(primitiveCount);
            maxPrimitiveCount += primitiveCount;

            vk::AccelerationStructureBuildRangeInfoKHR& buildRangeInfo = buildRangeInfos.emplace_back();
            buildRangeInfo.primitiveCount = primitiveCount;
            buildRangeInfo.primitiveOffset = 0;
            buildRangeInfo.firstVertex = 0;
            buildRangeInfo.transformOffset = 0;

            GeometryNodeCreation geometryNodeCreation {};
            geometryNodeCreation.vertexBufferDeviceAddress = vertexBufferDeviceAddress.deviceAddress;
            geometryNodeCreation.indexBufferDeviceAddress = indexBufferDeviceAddress.deviceAddress;
            geometryNodeCreation.material = mesh.material;
            resources->GeometryNodes().Create(geometryNodeCreation);
        }

        if (!node.meshes.empty())
        {
            nodeCount++;
        }
    }

    _geometryCount = geometries.size();

    vk::AccelerationStructureBuildGeometryInfoKHR buildGeometryInfo {};
    buildGeometryInfo.type = vk::AccelerationStructureTypeKHR::eBottomLevel;
    buildGeometryInfo.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
    buildGeometryInfo.mode = vk::BuildAccelerationStructureModeKHR::eBuild;
    buildGeometryInfo.geometryCount = static_cast<uint32_t>(geometries.size());
    buildGeometryInfo.pGeometries = geometries.data();

    vk::AccelerationStructureBuildSizesInfoKHR buildSizesInfo = _vulkanContext->Device().getAccelerationStructureBuildSizesKHR(
        vk::AccelerationStructureBuildTypeKHR::eDevice, buildGeometryInfo, maxPrimitiveCounts, _vulkanContext->Dldi());

    BufferCreation structureBufferCreation {};
    structureBufferCreation.SetName("BLAS Structure Buffer")
        .SetUsageFlags(vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress)
        .SetMemoryUsage(VMA_MEMORY_USAGE_GPU_ONLY)
        .SetIsMappable(false)
        .SetSize(buildSizesInfo.accelerationStructureSize);
    _structureBuffer = std::make_unique<Buffer>(structureBufferCreation, _vulkanContext);

    vk::AccelerationStructureCreateInfoKHR createInfo {};
    createInfo.type = vk::AccelerationStructureTypeKHR::eBottomLevel;
    createInfo.buffer = _structureBuffer->buffer;
    createInfo.size = buildSizesInfo.accelerationStructureSize;
    _vkStructure = _vulkanContext->Device().createAccelerationStructureKHR(createInfo, nullptr, _vulkanContext->Dldi());

    BufferCreation scratchBufferCreation {};
    scratchBufferCreation.SetName("BLAS Scratch Buffer")
        .SetUsageFlags(vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress)
        .SetMemoryUsage(VMA_MEMORY_USAGE_GPU_ONLY)
        .SetIsMappable(false)
        .SetSize(buildSizesInfo.buildScratchSize);
    _scratchBuffer = std::make_unique<Buffer>(scratchBufferCreation, _vulkanContext);

    // Fill in remaining data
    buildGeometryInfo.dstAccelerationStructure = _vkStructure;
    buildGeometryInfo.scratchData.deviceAddress = _vulkanContext->GetBufferDeviceAddress(_scratchBuffer->buffer);

    std::vector<vk::AccelerationStructureBuildRangeInfoKHR*> pBuildRangeInfos(buildRangeInfos.size());
    for (uint32_t i = 0; i < buildRangeInfos.size(); i++)
    {
        pBuildRangeInfos[i] = &buildRangeInfos[i];
    }

    SingleTimeCommands singleTimeCommands { _vulkanContext };
    singleTimeCommands.Record([&](vk::CommandBuffer commandBuffer)
        { commandBuffer.buildAccelerationStructuresKHR(1, &buildGeometryInfo, pBuildRangeInfos.data(), _vulkanContext->Dldi()); });
    singleTimeCommands.Submit();
}
