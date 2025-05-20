#pragma once

#include "render_core/rhi/rtx/acceleration_structure.h"

#include "render_core/rhi/backend/vulkan/vk_core.h"

namespace Mizu::Vulkan
{

// Forward declarations
class VulkanBufferResource;

class VulkanBottomLevelAccelerationStructure : public BottomLevelAccelerationStructure
{
  public:
    VulkanBottomLevelAccelerationStructure(Description desc);
    ~VulkanBottomLevelAccelerationStructure() override;

    void build(VkCommandBuffer command) const;

    const std::unique_ptr<VulkanBufferResource>& get_buffer() const { return m_blas_buffer; }

    VkAccelerationStructureKHR handle() const { return m_handle; }

  private:
    VkAccelerationStructureKHR m_handle{VK_NULL_HANDLE};
    std::unique_ptr<VulkanBufferResource> m_blas_buffer;
    std::unique_ptr<VulkanBufferResource> m_blas_scratch_buffer;

    VkAccelerationStructureBuildGeometryInfoKHR m_build_geometry_info{};
    VkAccelerationStructureGeometryKHR m_geometry_info{};
    VkAccelerationStructureBuildRangeInfoKHR m_build_range_info{};
    VkAccelerationStructureBuildSizesInfoKHR m_build_sizes_info{};

    Description m_description{};
};

class VulkanTopLevelAccelerationStructure : public TopLevelAccelerationStructure
{
  public:
    VulkanTopLevelAccelerationStructure(Description desc);
    ~VulkanTopLevelAccelerationStructure() override;

    void build(VkCommandBuffer command) const;

    VkAccelerationStructureKHR handle() const { return m_handle; }

  private:
    VkAccelerationStructureKHR m_handle{VK_NULL_HANDLE};
    std::unique_ptr<VulkanBufferResource> m_instances_buffer;
    std::unique_ptr<VulkanBufferResource> m_tlas_buffer;
    std::unique_ptr<VulkanBufferResource> m_tlas_scratch_buffer;

    VkAccelerationStructureBuildGeometryInfoKHR m_build_geometry_info{};
    VkAccelerationStructureGeometryKHR m_geometry_info{};
    VkAccelerationStructureBuildRangeInfoKHR m_build_range_info{};
    VkAccelerationStructureBuildSizesInfoKHR m_build_sizes_info{};

    Description m_description{};
};

} // namespace Mizu::Vulkan