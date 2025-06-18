#pragma once

#include "render_core/rhi/rtx/acceleration_structure.h"

#include "render_core/rhi/backend/vulkan/vk_core.h"

namespace Mizu::Vulkan
{

// Forward declarations
class VulkanBufferResource;

class VulkanAccelerationStructure : public AccelerationStructure
{
  public:
    VulkanAccelerationStructure(Description desc, std::weak_ptr<IDeviceMemoryAllocator> allocator);
    ~VulkanAccelerationStructure() override;

    AccelerationStructureBuildSizes get_build_sizes() const override { return m_build_sizes; }
    Type get_type() const override { return m_description.type; }

    VkAccelerationStructureBuildGeometryInfoKHR get_build_geometry_info() const { return m_build_geometry_info; }
    VkAccelerationStructureBuildRangeInfoKHR get_build_range_info() const { return m_build_range_info; }

    const VulkanBufferResource& get_instances_buffer() const { return *m_instances_buffer; }

    VkAccelerationStructureKHR handle() const { return m_handle; }

  private:
    VkAccelerationStructureKHR m_handle{VK_NULL_HANDLE};

    std::shared_ptr<VulkanBufferResource> m_as_buffer;

    VkAccelerationStructureGeometryKHR m_geometry{};
    VkAccelerationStructureBuildGeometryInfoKHR m_build_geometry_info{};
    VkAccelerationStructureBuildRangeInfoKHR m_build_range_info{};

    // Only for TopLevel acceleration structures
    std::unique_ptr<VulkanBufferResource> m_instances_buffer;

    Description m_description;
    std::weak_ptr<IDeviceMemoryAllocator> m_allocator;

    AccelerationStructureBuildSizes m_build_sizes;
};

} // namespace Mizu::Vulkan