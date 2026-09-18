#pragma once

#include "render_core/rhi/acceleration_structure.h"

#include "dx12_buffer_resource.h"
#include "dx12_core.h"

namespace Mizu::Dx12
{

class Dx12AccelerationStructure : public AccelerationStructure
{
  public:
    Dx12AccelerationStructure(AccelerationStructureDescription desc);
    ~Dx12AccelerationStructure() override;

    AccelerationStructureBuildSizes get_build_sizes() const override { return m_build_sizes; }
    AccelerationStructureType get_type() const override { return m_type; }

    const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& get_inputs() const { return m_inputs; }


    std::shared_ptr<Dx12BufferResource> get_as_buffer() const { return m_as_buffer; }

  private:
    std::shared_ptr<Dx12BufferResource> m_as_buffer{};

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS m_inputs{};
    D3D12_RAYTRACING_GEOMETRY_DESC m_geometry_desc{};

    AccelerationStructureDescription m_description{};
    AccelerationStructureBuildSizes m_build_sizes{};
    AccelerationStructureType m_type{};

    void build_tlas(
        const TopLevelAccelerationStructureDescription& desc,
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& out_inputs);
    void build_blas(
        const BottomLevelAccelerationStructureDescription& desc,
        D3D12_RAYTRACING_GEOMETRY_DESC& out_geometry_desc,
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& out_inputs);
};

} // namespace Mizu::Dx12