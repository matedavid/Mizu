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

    VkAccelerationStructureKHR get_handle() const { return m_handle; }

  private:
    VkAccelerationStructureKHR m_handle;
    std::unique_ptr<VulkanBufferResource> m_blas_buffer;
    std::unique_ptr<VulkanBufferResource> m_blas_scratch_buffer;

    VkAccelerationStructureGeometryKHR m_geometry_info{};
    VkAccelerationStructureBuildRangeInfoKHR m_build_range_info{};
    VkAccelerationStructureBuildSizesInfoKHR m_build_sizes_info{};

    Description m_description{};
};

} // namespace Mizu::Vulkan