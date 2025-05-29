#include "vulkan_acceleration_structure.h"

#include <glm/gtc/matrix_transform.hpp>

#include "render_core/rhi/renderer.h"

#include "render_core/rhi/backend/vulkan/vulkan_buffer_resource.h"
#include "render_core/rhi/backend/vulkan/vulkan_context.h"

#include "render_core/rhi/backend/vulkan/rtx/vulkan_rtx_core.h"

#include "utility/assert.h"

namespace Mizu::Vulkan
{

//
// VulkanBottomLevelAccelerationStructure
//

VulkanBottomLevelAccelerationStructure::VulkanBottomLevelAccelerationStructure(Description desc)
    : m_description(std::move(desc))
{
    MIZU_VERIFY(Renderer::get_capabilities().ray_tracing_hardware, "RTX hardware is not supported on current device");

    MIZU_ASSERT(m_description.vertex_buffer != nullptr, "Vertex Buffer is nullptr");
    MIZU_ASSERT(m_description.index_buffer != nullptr, "Index Buffer is nullptr");

    const VulkanBufferResource& native_vertex_buffer =
        *std::dynamic_pointer_cast<VulkanBufferResource>(m_description.vertex_buffer->get_resource());
    const VulkanBufferResource& native_index_buffer =
        *std::dynamic_pointer_cast<VulkanBufferResource>(m_description.index_buffer->get_resource());

    VkBufferDeviceAddressInfo vertex_address_info{};
    vertex_address_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    vertex_address_info.buffer = native_vertex_buffer.handle();
    const VkDeviceAddress vertex_address =
        vkGetBufferDeviceAddressKHR(VulkanContext.device->handle(), &vertex_address_info);

    VkBufferDeviceAddressInfo index_address_info{};
    index_address_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    index_address_info.buffer = native_index_buffer.handle();
    const VkDeviceAddress index_address =
        vkGetBufferDeviceAddressKHR(VulkanContext.device->handle(), &index_address_info);

    VkAccelerationStructureGeometryTrianglesDataKHR geometry_triangles_data{};
    geometry_triangles_data.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
    geometry_triangles_data.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    geometry_triangles_data.vertexData.deviceAddress = vertex_address;
    geometry_triangles_data.vertexStride = sizeof(glm::vec3);
    geometry_triangles_data.maxVertex = m_description.vertex_buffer->get_count() - 1;
    geometry_triangles_data.indexType = VK_INDEX_TYPE_UINT32;
    geometry_triangles_data.indexData.deviceAddress = index_address;

    VkAccelerationStructureGeometryDataKHR geometry_data{};
    geometry_data.triangles = geometry_triangles_data;

    m_geometry_info = {};
    m_geometry_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    m_geometry_info.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    m_geometry_info.geometry = geometry_data;
    m_geometry_info.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;

    m_build_range_info = {};
    m_build_range_info.primitiveCount = m_description.index_buffer->get_count() / 3;
    m_build_range_info.primitiveOffset = 0;
    m_build_range_info.firstVertex = 0;
    m_build_range_info.transformOffset = 0;

    m_build_geometry_info = {};
    m_build_geometry_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    m_build_geometry_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    m_build_geometry_info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    m_build_geometry_info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    m_build_geometry_info.geometryCount = 1;
    m_build_geometry_info.pGeometries = &m_geometry_info;
    m_build_geometry_info.srcAccelerationStructure = VK_NULL_HANDLE;

    m_build_sizes_info = {};
    m_build_sizes_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    vkGetAccelerationStructureBuildSizesKHR(VulkanContext.device->handle(),
                                            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                            &m_build_geometry_info,
                                            &m_build_range_info.primitiveCount,
                                            &m_build_sizes_info);

    BufferDescription blas_buffer_desc{};
    blas_buffer_desc.size = m_build_sizes_info.accelerationStructureSize;
    blas_buffer_desc.usage = BufferUsageBits::RtxAccelerationStructureStorage;

    // TODO: Don't use Renderer::get_allocator()
    m_blas_buffer = std::make_unique<VulkanBufferResource>(blas_buffer_desc, Renderer::get_allocator());

    BufferDescription scratch_buffer_desc{};
    scratch_buffer_desc.size = m_build_sizes_info.buildScratchSize;
    scratch_buffer_desc.usage = BufferUsageBits::RtxAccelerationStructureStorage | BufferUsageBits::StorageBuffer;

    // TODO: Don't use Renderer::get_allocator()
    m_blas_scratch_buffer = std::make_unique<VulkanBufferResource>(scratch_buffer_desc, Renderer::get_allocator());

    VkAccelerationStructureCreateInfoKHR create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    create_info.createFlags = 0;
    create_info.buffer = m_blas_buffer->handle();
    create_info.offset = 0;
    create_info.size = m_blas_buffer->get_size();
    create_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

    vkCreateAccelerationStructureKHR(VulkanContext.device->handle(), &create_info, nullptr, &m_handle);

    VkBufferDeviceAddressInfo scratch_address_info{};
    scratch_address_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    scratch_address_info.buffer = m_blas_scratch_buffer->handle();
    const VkDeviceAddress scratch_address =
        vkGetBufferDeviceAddressKHR(VulkanContext.device->handle(), &scratch_address_info);

    m_build_geometry_info.dstAccelerationStructure = m_handle;
    m_build_geometry_info.scratchData.deviceAddress = scratch_address;
}

VulkanBottomLevelAccelerationStructure::~VulkanBottomLevelAccelerationStructure()
{
    vkDestroyAccelerationStructureKHR(VulkanContext.device->handle(), m_handle, nullptr);
}

void VulkanBottomLevelAccelerationStructure::build(VkCommandBuffer command) const
{
    const VkAccelerationStructureBuildRangeInfoKHR* build_range = &m_build_range_info;
    vkCmdBuildAccelerationStructuresKHR(command, 1, &m_build_geometry_info, &build_range);
}

//
// VulkanTopLevelAccelerationStructure
//

VulkanTopLevelAccelerationStructure::VulkanTopLevelAccelerationStructure(Description desc)
    : m_description(std::move(desc))
{
    MIZU_VERIFY(Renderer::get_capabilities().ray_tracing_hardware, "RTX hardware is not supported on current device");

    MIZU_ASSERT(!m_description.instances.empty(), "List of instances is empty");

    std::vector<VkAccelerationStructureInstanceKHR> instances_data;

    for (uint32_t i = 0; i < m_description.instances.size(); ++i)
    {
        const InstanceData& data = m_description.instances[i];
        MIZU_ASSERT(data.blas != nullptr, "Blas at index {} is nullptr", i);

        const glm::mat4 transform = glm::translate(glm::mat4(1.0f), data.position);

        VkTransformMatrixKHR vk_transform{};
        for (int32_t r = 0; r < 3; ++r)
        {
            for (int32_t c = 0; c < 4; ++c)
            {
                vk_transform.matrix[r][c] = transform[c][r];
            }
        }

        VkBufferDeviceAddressInfo blas_address_info{};
        blas_address_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        blas_address_info.buffer =
            std::dynamic_pointer_cast<VulkanBottomLevelAccelerationStructure>(data.blas)->get_buffer()->handle();
        const VkDeviceAddress blas_address =
            vkGetBufferDeviceAddressKHR(VulkanContext.device->handle(), &blas_address_info);

        VkAccelerationStructureInstanceKHR instance_data{};
        instance_data.transform = vk_transform;
        instance_data.instanceCustomIndex = i;
        instance_data.mask = 0xff;
        instance_data.instanceShaderBindingTableRecordOffset = 0;
        instance_data.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        instance_data.accelerationStructureReference = blas_address;

        instances_data.push_back(instance_data);
    }

    const uint32_t number_instances = static_cast<uint32_t>(instances_data.size());

    BufferDescription instances_buffer_desc{};
    instances_buffer_desc.size = sizeof(VkTransformMatrixKHR) * instances_data.size();
    instances_buffer_desc.usage = BufferUsageBits::RtxAccelerationStructureInputReadOnly | BufferUsageBits::TransferDst;

    const uint8_t* instances_data_ptr = reinterpret_cast<const uint8_t*>(instances_data.data());

    // TODO: Don't use Renderer::get_allocator()
    m_instances_buffer =
        std::make_unique<VulkanBufferResource>(instances_buffer_desc, instances_data_ptr, Renderer::get_allocator());

    VkBufferDeviceAddressInfo instances_buffer_info{};
    instances_buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    instances_buffer_info.buffer = m_instances_buffer->handle();

    const VkDeviceAddress instances_buffer_address =
        vkGetBufferDeviceAddress(VulkanContext.device->handle(), &instances_buffer_info);

    VkAccelerationStructureGeometryInstancesDataKHR geometry_instances_data{};
    geometry_instances_data.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    geometry_instances_data.data.deviceAddress = instances_buffer_address;

    m_geometry_info = {};
    m_geometry_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    m_geometry_info.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    m_geometry_info.geometry.instances = geometry_instances_data;

    m_build_range_info = {};
    m_build_range_info.primitiveCount = number_instances;
    m_build_range_info.primitiveOffset = 0;
    m_build_range_info.firstVertex = 0;
    m_build_range_info.transformOffset = 0;

    m_build_geometry_info = {};
    m_build_geometry_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    m_build_geometry_info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    m_build_geometry_info.geometryCount = 1;
    m_build_geometry_info.pGeometries = &m_geometry_info;
    // if update; build_geometry_info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR
    m_build_geometry_info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    m_build_geometry_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    m_build_geometry_info.srcAccelerationStructure = VK_NULL_HANDLE;

    m_build_sizes_info = {};
    m_build_sizes_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

    vkGetAccelerationStructureBuildSizesKHR(VulkanContext.device->handle(),
                                            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                            &m_build_geometry_info,
                                            &m_build_range_info.primitiveCount,
                                            &m_build_sizes_info);

    BufferDescription tlas_buffer_desc{};
    tlas_buffer_desc.size = m_build_sizes_info.accelerationStructureSize;
    tlas_buffer_desc.usage = BufferUsageBits::RtxAccelerationStructureStorage;

    // TODO: Don't use Renderer::get_allocator()
    m_tlas_buffer = std::make_unique<VulkanBufferResource>(tlas_buffer_desc, Renderer::get_allocator());

    BufferDescription scratch_buffer_desc{};
    scratch_buffer_desc.size = m_build_sizes_info.buildScratchSize;
    scratch_buffer_desc.usage = BufferUsageBits::RtxAccelerationStructureStorage | BufferUsageBits::StorageBuffer;

    // TODO: Don't use Renderer::get_allocator()
    m_tlas_scratch_buffer = std::make_unique<VulkanBufferResource>(scratch_buffer_desc, Renderer::get_allocator());

    VkAccelerationStructureCreateInfoKHR create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    create_info.createFlags = 0;
    create_info.buffer = m_tlas_buffer->handle();
    create_info.offset = 0;
    create_info.size = m_tlas_buffer->get_size();
    create_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;

    vkCreateAccelerationStructureKHR(VulkanContext.device->handle(), &create_info, nullptr, &m_handle);

    VkBufferDeviceAddressInfo scratch_address_info{};
    scratch_address_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    scratch_address_info.buffer = m_tlas_scratch_buffer->handle();
    const VkDeviceAddress scratch_address =
        vkGetBufferDeviceAddressKHR(VulkanContext.device->handle(), &scratch_address_info);

    m_build_geometry_info.dstAccelerationStructure = m_handle;
    m_build_geometry_info.scratchData.deviceAddress = scratch_address;
}

VulkanTopLevelAccelerationStructure::~VulkanTopLevelAccelerationStructure()
{
    vkDestroyAccelerationStructureKHR(VulkanContext.device->handle(), m_handle, nullptr);
}

void VulkanTopLevelAccelerationStructure::build(VkCommandBuffer command) const
{
    const VkAccelerationStructureBuildRangeInfoKHR* build_range = &m_build_range_info;
    vkCmdBuildAccelerationStructuresKHR(command, 1, &m_build_geometry_info, &build_range);
}

} // namespace Mizu::Vulkan