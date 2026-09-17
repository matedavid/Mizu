#pragma once

#include "render_core/rhi/acceleration_structure.h"

#include "vulkan_core.h"
#include "vulkan_resource_view.h"

namespace Mizu::Vulkan
{

// Forward declarations
class VulkanBufferResource;

class VulkanAccelerationStructure : public AccelerationStructure
{
  public:
    VulkanAccelerationStructure(AccelerationStructureDescription desc);
    ~VulkanAccelerationStructure() override;

    AccelerationStructureBuildSizes get_build_sizes() const override { return m_build_sizes; }
    AccelerationStructureType get_type() const override { return m_type; }

    VulkanAccelerationStructureResourceView as_srv();

    const VkAccelerationStructureGeometryKHR& get_geometry() const { return m_geometry; }
    const VkAccelerationStructureBuildRangeInfoKHR& get_build_range_info() const { return m_build_range_info; }
    VkBuildAccelerationStructureFlagsKHR get_flags() const { return m_flags; }

    const VulkanBufferResource& get_instances_buffer() const { return *m_instances_buffer; }

    VkAccelerationStructureKHR handle() const { return m_handle; }
    std::shared_ptr<VulkanBufferResource> get_as_buffer() const { return m_as_buffer; }

  private:
    VkAccelerationStructureKHR m_handle{VK_NULL_HANDLE};
    std::shared_ptr<VulkanBufferResource> m_as_buffer{};

    VkAccelerationStructureGeometryKHR m_geometry{};
    VkAccelerationStructureBuildRangeInfoKHR m_build_range_info{};
    VkBuildAccelerationStructureFlagsKHR m_flags{0};

    // Only TLAS
    std::unique_ptr<VulkanBufferResource> m_instances_buffer{};

    AccelerationStructureDescription m_description{};
    AccelerationStructureBuildSizes m_build_sizes{};
    AccelerationStructureType m_type{};

    void create_tlas(
        const TopLevelAccelerationStructureDescription& desc,
        VkAccelerationStructureGeometryKHR& out_geometry,
        VkAccelerationStructureBuildRangeInfoKHR& out_range_info);
    void create_blas(
        const BottomLevelAccelerationStructureDescription& desc,
        VkAccelerationStructureGeometryKHR& out_geometry,
        VkAccelerationStructureBuildRangeInfoKHR& out_range_info);
};

} // namespace Mizu::Vulkan