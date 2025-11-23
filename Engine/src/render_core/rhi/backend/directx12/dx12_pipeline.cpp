#include "dx12_pipeline.h"

#include "base/debug/logging.h"

#include "render_core/shader/shader_group.h"

#include "render_core/rhi/backend/directx12/dx12_context.h"
#include "render_core/rhi/backend/directx12/dx12_shader.h"

namespace Mizu::Dx12
{

void create_root_signature(
    const ShaderGroup& shader_group,
    Dx12RootSignatureInfo& root_signature_info,
    ID3D12RootSignature*& root_signature)
{
    // TODO: Fix this mess and make the code more flexible.

    std::vector<D3D12_ROOT_PARAMETER1> root_parameters;

    std::vector<D3D12_DESCRIPTOR_RANGE1> ranges;
    std::vector<D3D12_DESCRIPTOR_RANGE1> sampler_ranges;

    for (uint32_t space = 0; space < shader_group.get_max_set(); ++space)
    {
        std::vector<ShaderResource> parameters = shader_group.get_parameters_in_set(space);

        // Current sorting it's SRV -> UAV -> CBV -> Sampler
        std::sort(parameters.begin(), parameters.end(), [](const ShaderResource& a, const ShaderResource& b) {
            const D3D12_DESCRIPTOR_RANGE_TYPE a_type = Dx12Shader::get_dx12_descriptor_type(a.value);
            const D3D12_DESCRIPTOR_RANGE_TYPE b_type = Dx12Shader::get_dx12_descriptor_type(b.value);

            return a_type < b_type;
        });

        ShaderType shader_stage_bits = static_cast<ShaderType>(0);

        for (const ShaderResource& parameter : parameters)
        {
            const D3D12_DESCRIPTOR_RANGE_TYPE range_type = Dx12Shader::get_dx12_descriptor_type(parameter.value);

            D3D12_DESCRIPTOR_RANGE1 range{};
            range.RangeType = range_type;
            range.NumDescriptors = 1;
            range.BaseShaderRegister = parameter.binding_info.binding;
            range.RegisterSpace = space;
            range.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;
            range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

            shader_stage_bits |= shader_group.get_resource_stage_bits(parameter.name);

            if (range_type == D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER)
            {
                sampler_ranges.push_back(range);
            }
            else
            {
                ranges.push_back(range);
            }
        }

        MIZU_ASSERT(static_cast<ShaderTypeBitsType>(shader_stage_bits) != 0, "Invalid shader type bits");

        D3D12_ROOT_PARAMETER1 root_parameter{};
        root_parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        root_parameter.ShaderVisibility = Dx12Shader::get_dx12_shader_stage_bits(shader_stage_bits);
        root_parameter.DescriptorTable.NumDescriptorRanges = static_cast<uint32_t>(ranges.size());
        root_parameter.DescriptorTable.pDescriptorRanges = ranges.data();

        root_parameters.push_back(root_parameter);

        if (!sampler_ranges.empty())
        {
            D3D12_ROOT_PARAMETER1 sampler_root_parameter{};
            sampler_root_parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            sampler_root_parameter.ShaderVisibility = Dx12Shader::get_dx12_shader_stage_bits(shader_stage_bits);
            sampler_root_parameter.DescriptorTable.NumDescriptorRanges = static_cast<uint32_t>(sampler_ranges.size());
            sampler_root_parameter.DescriptorTable.pDescriptorRanges = sampler_ranges.data();

            root_parameters.push_back(sampler_root_parameter);
        }
    }

    MIZU_ASSERT(
        shader_group.get_constants().size() <= 1,
        "Maximum of one root constant is supported, number of root constants in shader group = {}",
        shader_group.get_constants().size());

    for (const ShaderPushConstant& constant : shader_group.get_constants())
    {
        D3D12_ROOT_PARAMETER1 root_parameter{};
        root_parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        root_parameter.ShaderVisibility =
            Dx12Shader::get_dx12_shader_stage_bits(shader_group.get_resource_stage_bits(constant.name));
        root_parameter.Constants.ShaderRegister = constant.binding_info.binding;
        root_parameter.Constants.RegisterSpace = constant.binding_info.set;
        root_parameter.Constants.Num32BitValues = static_cast<uint32_t>(constant.size / 4u);

        root_parameters.push_back(root_parameter);
    }

    D3D12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_desc{};
    root_signature_desc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
    root_signature_desc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    root_signature_desc.Desc_1_1.NumParameters = static_cast<uint32_t>(root_parameters.size());
    root_signature_desc.Desc_1_1.pParameters = root_parameters.data();
    root_signature_desc.Desc_1_1.NumStaticSamplers = 0;
    root_signature_desc.Desc_1_1.pStaticSamplers = nullptr;

    ID3DBlob *signature = nullptr, *error = nullptr;

    // TODO: Should use root signature cache instead of creating it directly
    D3D12SerializeVersionedRootSignature(&root_signature_desc, &signature, &error);
    if (error)
    {
        MIZU_LOG_ERROR("Error serializing RootSignature: {}", static_cast<const char*>(error->GetBufferPointer()));
        MIZU_UNREACHABLE("Failed to call D3D12SerializeVersionedRootSignature");
    }

    DX12_CHECK(Dx12Context.device->handle()->CreateRootSignature(
        0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&root_signature)));

    // TODO: Should I print error if something happened????

    if (signature != nullptr)
        signature->Release();
    if (error != nullptr)
        error->Release();

    root_signature_info = Dx12RootSignatureInfo{};
    root_signature_info.num_parameters = root_signature_desc.Desc_1_1.NumParameters;
    if (shader_group.get_constants().size() == 1)
    {
        // Root constant is always the last parameter in the RootSignature
        root_signature_info.root_constant_index = root_signature_info.num_parameters - 1;
    }
}

} // namespace Mizu::Dx12
