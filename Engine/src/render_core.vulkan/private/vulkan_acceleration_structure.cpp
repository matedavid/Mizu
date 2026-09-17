#include "vulkan_acceleration_structure.h"

#include <format>

#include "base/debug/assert.h"
#include "base/debug/logging.h"

#include "vulkan_buffer_resource.h"
#include "vulkan_context.h"
#include "vulkan_resource_view.h"
#include "vulkan_types.h"

namespace Mizu::Vulkan
{

static VkAccelerationStructureTypeKHR get_vulkan_acceleration_structure_type(AccelerationStructureType type)
{
    switch (type)
    {
    case AccelerationStructureType::TopLevel:
        return VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    case AccelerationStructureType::BottomLevel:
        return VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    }
}

static VkBuildAccelerationStructureFlagsKHR get_vulkan_acceleration_structure_flags(AccelerationStructureFlagBits bits)
{
    VkBuildAccelerationStructureFlagsKHR flags = 0;

    if (bits & AccelerationStructureFlagBits::AllowUpdate)
        flags |= VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;

    if (bits & AccelerationStructureFlagBits::PreferFastTrace)
        flags |= VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;

    return flags;
}

static void validate_acceleration_structure_flags(AccelerationStructureFlagBits bits)
{
    const bool has_allow_update = (bits & AccelerationStructureFlagBits::AllowUpdate);
    const bool has_prefer_fast_trace = (bits & AccelerationStructureFlagBits::PreferFastTrace);

    if (has_allow_update && has_prefer_fast_trace)
    {
        MIZU_LOG_WARNING(
            "Acceleration structure requests both AllowUpdate and PreferFastTrace. These flags represent conflicting "
            "optimization priorities.");
    }
}

VulkanAccelerationStructure::VulkanAccelerationStructure(AccelerationStructureDescription desc)
    : m_description(std::move(desc))
{
    m_type = std::holds_alternative<TopLevelAccelerationStructureDescription>(m_description.description)
                 ? AccelerationStructureType::TopLevel
                 : AccelerationStructureType::BottomLevel;

    m_flags = get_vulkan_acceleration_structure_flags(m_description.flags);

    validate_acceleration_structure_flags(m_description.flags);

    if (const auto* tlas_desc = std::get_if<TopLevelAccelerationStructureDescription>(&m_description.description))
    {
        create_tlas(*tlas_desc, m_geometry, m_build_range_info);
    }
    else if (
        const auto* blas_desc = std::get_if<BottomLevelAccelerationStructureDescription>(&m_description.description))
    {
        create_blas(*blas_desc, m_geometry, m_build_range_info);
    }
    else
    {
        MIZU_UNREACHABLE("Invalid description");
    }

    VkAccelerationStructureBuildGeometryInfoKHR build_info{};
    build_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    build_info.flags = m_flags;
    build_info.type = get_vulkan_acceleration_structure_type(m_type);
    build_info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    build_info.geometryCount = 1;
    build_info.pGeometries = &m_geometry;

    VkAccelerationStructureBuildSizesInfoKHR build_sizes_info{};
    build_sizes_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

    vkGetAccelerationStructureBuildSizesKHR(
        VulkanContext.device->handle(),
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &build_info,
        &m_build_range_info.primitiveCount,
        &build_sizes_info);

    m_build_sizes = AccelerationStructureBuildSizes{
        .acceleration_structure_size = build_sizes_info.accelerationStructureSize,
        .build_scratch_size = build_sizes_info.buildScratchSize,
        .update_scratch_size = build_sizes_info.updateScratchSize,
    };

    BufferDescription as_buffer_desc{};
    as_buffer_desc.size = build_sizes_info.accelerationStructureSize;
    as_buffer_desc.usage = BufferUsageBits::RtxAccelerationStructureStorage;
    if (!m_description.name.empty())
        as_buffer_desc.name = std::format("{}_ASBuffer", m_description.name);

    m_as_buffer = std::make_shared<VulkanBufferResource>(as_buffer_desc);

    VkAccelerationStructureCreateInfoKHR create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    create_info.buffer = m_as_buffer->handle();
    create_info.offset = 0;
    create_info.size = m_as_buffer->get_size();
    create_info.type = get_vulkan_acceleration_structure_type(m_type);

    VK_CHECK(vkCreateAccelerationStructureKHR(VulkanContext.device->handle(), &create_info, nullptr, &m_handle));

    if (!m_description.name.empty())
    {
        VK_DEBUG_SET_OBJECT_NAME(m_handle, m_description.name);
    }
}

VulkanAccelerationStructure::~VulkanAccelerationStructure()
{
    vkDestroyAccelerationStructureKHR(VulkanContext.device->handle(), m_handle, nullptr);
}

VulkanAccelerationStructureResourceView VulkanAccelerationStructure::as_srv()
{
    VulkanAccelerationStructureResourceView resource_view{};
    resource_view.handle = m_handle;

    return resource_view;
}

void VulkanAccelerationStructure::create_tlas(
    const TopLevelAccelerationStructureDescription& desc,
    VkAccelerationStructureGeometryKHR& out_geometry,
    VkAccelerationStructureBuildRangeInfoKHR& out_range_info)
{
    MIZU_ASSERT(desc.max_instances != 0, "Can't create instances with 0 max_instances");

    BufferDescription instances_buffer_desc{};
    instances_buffer_desc.size = sizeof(VkAccelerationStructureInstanceKHR) * desc.max_instances;
    instances_buffer_desc.usage = BufferUsageBits::RtxAccelerationStructureInputReadOnly | BufferUsageBits::HostVisible;
    if (!m_description.name.empty())
        instances_buffer_desc.name = std::format("{}_InstancesBuffer", m_description.name);

    m_instances_buffer = std::make_unique<VulkanBufferResource>(instances_buffer_desc);

    VkAccelerationStructureGeometryInstancesDataKHR geometry_instances_data{};
    geometry_instances_data.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    geometry_instances_data.arrayOfPointers = VK_FALSE;
    geometry_instances_data.data.deviceAddress = get_device_address(m_instances_buffer->handle());

    VkAccelerationStructureGeometryKHR geometry{};
    geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    geometry.geometry.instances = geometry_instances_data;

    VkAccelerationStructureBuildRangeInfoKHR range_info{};
    range_info.primitiveCount = desc.max_instances;

    out_geometry = geometry;
    out_range_info = range_info;
}

void VulkanAccelerationStructure::create_blas(
    const BottomLevelAccelerationStructureDescription& desc,
    VkAccelerationStructureGeometryKHR& out_geometry,
    VkAccelerationStructureBuildRangeInfoKHR& out_range_info)
{
    VkAccelerationStructureGeometryKHR geometry{};
    geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;

    VkAccelerationStructureBuildRangeInfoKHR range_info{};

    if (const auto* triangles_desc = desc.geometry.get_if<AccelerationStructureGeometry::TrianglesDescription>())
    {
        MIZU_ASSERT(triangles_desc->vertex_buffer != nullptr, "vertex buffer is nullptr");

        const VulkanBufferResource& native_vertex =
            static_cast<const VulkanBufferResource&>(*triangles_desc->vertex_buffer);

        VkAccelerationStructureGeometryTrianglesDataKHR geometry_triangles_data{};
        geometry_triangles_data.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
        geometry_triangles_data.vertexFormat = get_vulkan_image_format(triangles_desc->vertex_format);
        geometry_triangles_data.vertexData.deviceAddress =
            get_device_address(native_vertex.handle()) + triangles_desc->vertex_offset;
        geometry_triangles_data.vertexStride = triangles_desc->vertex_stride;
        geometry_triangles_data.maxVertex = triangles_desc->vertex_count - 1;

        if (triangles_desc->index_buffer != nullptr)
        {
            const VulkanBufferResource& native_index =
                static_cast<const VulkanBufferResource&>(*triangles_desc->index_buffer);

            geometry_triangles_data.indexType = get_vulkan_index_type(triangles_desc->index_format);
            geometry_triangles_data.indexData.deviceAddress =
                get_device_address(native_index.handle()) + triangles_desc->index_offset;
        }

        geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
        geometry.geometry.triangles = geometry_triangles_data;
        geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;

        range_info.primitiveOffset = 0;
        range_info.firstVertex = 0;
        range_info.transformOffset = 0;

        if (triangles_desc->index_buffer != nullptr)
            range_info.primitiveCount = triangles_desc->index_count / 3;
        else
            range_info.primitiveCount = triangles_desc->vertex_count / 3;
    }
    else
    {
        MIZU_UNREACHABLE("Invalid blas geometry");
    }

    out_geometry = geometry;
    out_range_info = range_info;
}

} // namespace Mizu::Vulkan