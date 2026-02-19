#include "vulkan_acceleration_structure.h"

#include <format>

#include "base/debug/assert.h"

#include "vulkan_buffer_resource.h"
#include "vulkan_context.h"
#include "vulkan_image_resource.h"
#include "vulkan_resource_view.h"

namespace Mizu::Vulkan
{

VulkanAccelerationStructure::VulkanAccelerationStructure(AccelerationStructureDescription desc)
    : m_description(std::move(desc))
{
    MIZU_VERIFY(
        VulkanContext.device->get_properties().ray_tracing_hardware, "RTX hardware is not supported on current device");

    m_geometry = VkAccelerationStructureGeometryKHR{};
    m_geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;

    m_build_range_info = VkAccelerationStructureBuildRangeInfoKHR{};

    VkBuildAccelerationStructureFlagsKHR build_geometry_info_flags =
        VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;

    const AccelerationStructureGeometry& geom = m_description.geometry;
    if (geom.is_type<AccelerationStructureGeometry::TrianglesDescription>())
    {
        MIZU_ASSERT(m_description.type == AccelerationStructureType::BottomLevel, "Invalid Geometry type for BLAS");

        const auto& triangles = geom.as_type<AccelerationStructureGeometry::TrianglesDescription>();

        MIZU_ASSERT(triangles.vertex_buffer != nullptr, "Vertex buffer is required when defining Triangle Geometry");
        const VulkanBufferResource& vk_vertex = static_cast<VulkanBufferResource&>(*triangles.vertex_buffer);

        VkAccelerationStructureGeometryTrianglesDataKHR geometry_triangles_data{};
        geometry_triangles_data.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
        geometry_triangles_data.vertexFormat = VulkanImageResource::get_vulkan_image_format(triangles.vertex_format);
        geometry_triangles_data.vertexData.deviceAddress = get_device_address(vk_vertex.handle());
        geometry_triangles_data.vertexStride = triangles.vertex_stride;
        geometry_triangles_data.maxVertex =
            static_cast<uint32_t>(triangles.vertex_buffer->get_size() / triangles.vertex_stride) - 1;

        geometry_triangles_data.indexType = VK_INDEX_TYPE_NONE_KHR;

        if (triangles.index_buffer != nullptr)
        {
            const VulkanBufferResource& vk_index = static_cast<const VulkanBufferResource&>(*triangles.index_buffer);

            geometry_triangles_data.indexType = VK_INDEX_TYPE_UINT32;
            geometry_triangles_data.indexData.deviceAddress = get_device_address(vk_index.handle());
        }

        m_geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
        m_geometry.geometry.triangles = geometry_triangles_data;
        m_geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;

        if (triangles.index_buffer != nullptr)
        {
            m_build_range_info.primitiveCount = static_cast<uint32_t>(triangles.index_buffer->get_size()) / 3;
        }
        else
        {
            m_build_range_info.primitiveCount =
                static_cast<uint32_t>(triangles.vertex_buffer->get_size() / sizeof(uint32_t)) / 3;
        }
    }
    else if (geom.is_type<AccelerationStructureGeometry::InstancesDescription>())
    {
        MIZU_ASSERT(m_description.type == AccelerationStructureType::TopLevel, "Invalid Geometry type for TLAS");

        const auto& instances = geom.as_type<AccelerationStructureGeometry::InstancesDescription>();

        BufferDescription instances_buffer_desc{};
        instances_buffer_desc.size = sizeof(VkAccelerationStructureInstanceKHR) * instances.max_instances;
        instances_buffer_desc.usage =
            BufferUsageBits::RtxAccelerationStructureInputReadOnly | BufferUsageBits::HostVisible;
        instances_buffer_desc.name = std::format("{}_InstancesBuffer", m_description.name);

        m_instances_buffer = std::make_unique<VulkanBufferResource>(instances_buffer_desc);

        VkAccelerationStructureGeometryInstancesDataKHR geometry_instances_data{};
        geometry_instances_data.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
        geometry_instances_data.data.deviceAddress = get_device_address(m_instances_buffer->handle());

        m_geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
        m_geometry.geometry.instances = geometry_instances_data;

        m_build_range_info.primitiveCount = instances.max_instances;

        if (instances.allow_updates)
        {
            build_geometry_info_flags |= VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
        }
    }
    else
    {
        MIZU_UNREACHABLE("Invalid geometry type");
    }

    const VkAccelerationStructureTypeKHR vk_type = m_description.type == AccelerationStructureType::BottomLevel
                                                       ? VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR
                                                       : VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;

    m_build_geometry_info = VkAccelerationStructureBuildGeometryInfoKHR{};
    m_build_geometry_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    m_build_geometry_info.flags = build_geometry_info_flags;
    m_build_geometry_info.type = vk_type;
    m_build_geometry_info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    m_build_geometry_info.geometryCount = 1;
    m_build_geometry_info.pGeometries = &m_geometry;

    VkAccelerationStructureBuildSizesInfoKHR build_sizes_info{};
    build_sizes_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

    vkGetAccelerationStructureBuildSizesKHR(
        VulkanContext.device->handle(),
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &m_build_geometry_info,
        &m_build_range_info.primitiveCount,
        &build_sizes_info);

    m_build_sizes.acceleration_structure_size = build_sizes_info.accelerationStructureSize;
    m_build_sizes.build_scratch_size = build_sizes_info.buildScratchSize;
    m_build_sizes.update_scratch_size = build_sizes_info.updateScratchSize;

    BufferDescription as_buffer_desc{};
    as_buffer_desc.size = build_sizes_info.accelerationStructureSize;
    as_buffer_desc.usage = BufferUsageBits::RtxAccelerationStructureStorage;
    as_buffer_desc.name = std::format("{}_ASBuffer", m_description.name);

    m_as_buffer = std::make_shared<VulkanBufferResource>(as_buffer_desc);

    VkAccelerationStructureCreateInfoKHR create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    create_info.createFlags = 0;
    create_info.buffer = m_as_buffer->handle();
    create_info.offset = 0;
    create_info.size = m_as_buffer->get_size();
    create_info.type = vk_type;

    VK_CHECK(vkCreateAccelerationStructureKHR(VulkanContext.device->handle(), &create_info, nullptr, &m_handle));

    m_build_geometry_info.dstAccelerationStructure = m_handle;

    if (!m_description.name.empty())
    {
        VK_DEBUG_SET_OBJECT_NAME(m_handle, m_description.name);
    }
}

VulkanAccelerationStructure::~VulkanAccelerationStructure()
{
    vkDestroyAccelerationStructureKHR(VulkanContext.device->handle(), m_handle, nullptr);
}

ResourceView VulkanAccelerationStructure::as_srv()
{
    if (!m_resource_views.empty())
    {
        return m_resource_views[0];
    }

    // Create new view
    VulkanAccelerationStructureResourceView* internal = new VulkanAccelerationStructureResourceView{};
    internal->handle = m_handle;

    ResourceView view{};
    view.view_type = ResourceViewType::ShaderResourceView;
    view.internal = internal;

    m_resource_views.push_back(view);

    return view;
}

} // namespace Mizu::Vulkan