#pragma once

#include "base/containers/inplace_vector.h"
#include "render_core/rhi/acceleration_structure.h"

#include "vulkan_core.h"

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
    AccelerationStructureType get_type() const override { return m_description.type; }

    ResourceView as_srv() override;

    VkAccelerationStructureBuildGeometryInfoKHR get_build_geometry_info() const { return m_build_geometry_info; }
    VkAccelerationStructureBuildRangeInfoKHR get_build_range_info() const { return m_build_range_info; }

    const VulkanBufferResource& get_instances_buffer() const { return *m_instances_buffer; }

    VkAccelerationStructureKHR handle() const { return m_handle; }

  private:
    VkAccelerationStructureKHR m_handle{VK_NULL_HANDLE};

    // Considering 1 srv
    static constexpr size_t MAX_RESOURCE_VIEWS = 1;
    inplace_vector<ResourceView, MAX_RESOURCE_VIEWS> m_resource_views;

    std::shared_ptr<VulkanBufferResource> m_as_buffer;

    VkAccelerationStructureGeometryKHR m_geometry{};
    VkAccelerationStructureBuildGeometryInfoKHR m_build_geometry_info{};
    VkAccelerationStructureBuildRangeInfoKHR m_build_range_info{};

    // Only for TopLevel acceleration structures
    std::unique_ptr<VulkanBufferResource> m_instances_buffer;

    AccelerationStructureDescription m_description;

    AccelerationStructureBuildSizes m_build_sizes;
};

} // namespace Mizu::Vulkan