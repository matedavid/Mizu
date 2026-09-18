#include "dx12_acceleration_structure.h"

#include "dx12_buffer_resource.h"
#include "dx12_context.h"
#include "dx12_types.h"

namespace Mizu::Dx12
{

static D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE get_dx12_acceleration_structure_type(AccelerationStructureType type)
{
    switch (type)
    {
    case AccelerationStructureType::TopLevel:
        return D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    case AccelerationStructureType::BottomLevel:
        return D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    }
}

static D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS get_dx12_acceleration_structure_flags(
    AccelerationStructureFlagBits bits)
{
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;

    if (bits & AccelerationStructureFlagBits::AllowUpdate)
        flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;

    if (bits & AccelerationStructureFlagBits::PreferFastTrace)
        flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

    return flags;
}

Dx12AccelerationStructure::Dx12AccelerationStructure(AccelerationStructureDescription desc)
    : m_description(std::move(desc))
{
    MIZU_VERIFY(
        Dx12Context.device->get_properties().ray_tracing_hardware, "Rtx hardware is not supported on current device");

    m_type = std::holds_alternative<TopLevelAccelerationStructureDescription>(m_description.description)
                 ? AccelerationStructureType::TopLevel
                 : AccelerationStructureType::BottomLevel;

    if (const auto* tlas_desc = std::get_if<TopLevelAccelerationStructureDescription>(&m_description.description))
    {
        build_tlas(*tlas_desc, m_inputs);
    }
    else if (
        const auto* blas_desc = std::get_if<BottomLevelAccelerationStructureDescription>(&m_description.description))
    {
        build_blas(*blas_desc, m_geometry_desc, m_inputs);
    }

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuild_info{};
    Dx12Context.device->handle()->GetRaytracingAccelerationStructurePrebuildInfo(&m_inputs, &prebuild_info);

    m_build_sizes = AccelerationStructureBuildSizes{
        .acceleration_structure_size = prebuild_info.ResultDataMaxSizeInBytes,
        .build_scratch_size = prebuild_info.ScratchDataSizeInBytes,
        .update_scratch_size = prebuild_info.UpdateScratchDataSizeInBytes,
    };

    BufferDescription as_buffer_desc{};
    as_buffer_desc.size = prebuild_info.ResultDataMaxSizeInBytes;
    as_buffer_desc.usage = BufferUsageBits::RtxAccelerationStructureStorage;
    if (!m_description.name.empty())
        as_buffer_desc.name = std::format("{}_ASBuffer", m_description.name);

    m_as_buffer = std::make_shared<Dx12BufferResource>(as_buffer_desc);
}

Dx12AccelerationStructure::~Dx12AccelerationStructure() {}

void Dx12AccelerationStructure::build_tlas(
    const TopLevelAccelerationStructureDescription& desc,
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& out_inputs)
{
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs{};
    inputs.Type = get_dx12_acceleration_structure_type(m_type);
    inputs.Flags = get_dx12_acceleration_structure_flags(m_description.flags);
    inputs.NumDescs = desc.max_instances;

    out_inputs = inputs;
}

void Dx12AccelerationStructure::build_blas(
    const BottomLevelAccelerationStructureDescription& desc,
    D3D12_RAYTRACING_GEOMETRY_DESC& out_geometry_desc,
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& out_inputs)
{
    D3D12_RAYTRACING_GEOMETRY_DESC geometry_desc{};

    if (const auto* triangles_desc = desc.geometry.get_if<AccelerationStructureGeometry::TrianglesDescription>())
    {
        MIZU_ASSERT(triangles_desc->vertex_buffer != nullptr, "vertex buffer is nullptr");

        const Dx12BufferResource& native_vertex =
            static_cast<const Dx12BufferResource&>(*triangles_desc->vertex_buffer);

        D3D12_RAYTRACING_GEOMETRY_TRIANGLES_DESC geometry_triangles_desc{};
        geometry_triangles_desc.Transform3x4 = NULL;
        geometry_triangles_desc.VertexFormat = get_dx12_image_format(triangles_desc->vertex_format);
        geometry_triangles_desc.VertexCount = triangles_desc->vertex_count;
        geometry_triangles_desc.VertexBuffer = {
            .StartAddress = native_vertex.handle()->GetGPUVirtualAddress() + triangles_desc->vertex_offset,
            .StrideInBytes = triangles_desc->vertex_stride,
        };

        geometry_triangles_desc.IndexFormat = DXGI_FORMAT_UNKNOWN;
        geometry_triangles_desc.IndexCount = 0;
        geometry_triangles_desc.IndexBuffer = 0;

        if (triangles_desc->index_buffer != nullptr)
        {
            const Dx12BufferResource& native_index =
                static_cast<const Dx12BufferResource&>(*triangles_desc->index_buffer);

            geometry_triangles_desc.IndexFormat = get_dx12_index_format(triangles_desc->index_format);
            geometry_triangles_desc.IndexCount = triangles_desc->index_count;
            geometry_triangles_desc.IndexBuffer =
                native_index.handle()->GetGPUVirtualAddress() + triangles_desc->index_offset;
        }

        geometry_desc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
        geometry_desc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
        geometry_desc.Triangles = geometry_triangles_desc;
    }
    else
    {
        MIZU_UNREACHABLE("Invalid blas geometry");
    }

    out_geometry_desc = geometry_desc;

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs{};
    inputs.Type = get_dx12_acceleration_structure_type(m_type);
    inputs.Flags = get_dx12_acceleration_structure_flags(m_description.flags);
    inputs.NumDescs = 1;
    inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    inputs.pGeometryDescs = &out_geometry_desc;

    out_inputs = inputs;
}

} // namespace Mizu::Dx12
