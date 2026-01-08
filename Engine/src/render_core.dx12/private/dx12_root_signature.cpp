#include "dx12_root_signature.h"

#include "base/debug/logging.h"
#include "base/utils/hash.h"

#include "dx12_context.h"
#include "dx12_pipeline.h"
#include "dx12_shader.h"

namespace Mizu::Dx12
{

Dx12RootSignatureCache::~Dx12RootSignatureCache()
{
    for (const auto& [id, layout] : m_cache)
    {
        layout->Release();
    }
}

ID3D12RootSignature* Dx12RootSignatureCache::create(
    PipelineLayoutId id,
    const D3D12_VERSIONED_ROOT_SIGNATURE_DESC& create_info)
{
    MIZU_ASSERT(!contains(id), "Pipeline layout cache already contains entry with id: {}", id);

    ID3DBlob *signature = nullptr, *error = nullptr;
    D3D12SerializeVersionedRootSignature(&create_info, &signature, &error);
    if (error)
    {
        MIZU_LOG_ERROR("Error serializing RootSignature: {}", static_cast<const char*>(error->GetBufferPointer()));
        MIZU_UNREACHABLE("Failed to call D3D12SerializeVersionedRootSignature");
    }

    ID3D12RootSignature* root_signature;
    DX12_CHECK(Dx12Context.device->handle()->CreateRootSignature(
        0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&root_signature)));

    if (signature != nullptr)
        signature->Release();
    if (error != nullptr)
        error->Release();

    m_cache.insert({id, root_signature});

    return root_signature;
}

ID3D12RootSignature* Dx12RootSignatureCache::get(PipelineLayoutId id) const
{
    MIZU_ASSERT(contains(id), "Pipeline layout cache does not contain entry with id: {}", id);
    return m_cache.find(id)->second;
}

bool Dx12RootSignatureCache::contains(PipelineLayoutId id) const
{
    return m_cache.find(id) != m_cache.end();
}

ID3D12RootSignature* create_pipeline_layout(
    std::span<DescriptorBindingInfo> binding_info,
    Dx12RootSignatureInfo& root_signature_info)
{
    size_t hash = 0;
    for (const DescriptorBindingInfo& info : binding_info)
    {
        hash_combine(hash, info.hash());
    }

    if (Dx12Context.root_signature_cache->contains(hash))
    {
        return Dx12Context.root_signature_cache->get(hash);
    }

    std::unordered_map<uint32_t, std::vector<DescriptorBindingInfo>> space_to_resource_binding_infos;
    std::unordered_map<uint32_t, std::vector<DescriptorBindingInfo>> space_to_sampler_binding_infos;
    std::vector<DescriptorBindingInfo> push_constant_binding_infos;

    for (const DescriptorBindingInfo& binding : binding_info)
    {
        if (binding.type == ShaderResourceType::PushConstant)
        {
            push_constant_binding_infos.push_back(binding);
        }
        else if (binding.type == ShaderResourceType::SamplerState)
        {
            auto it = space_to_sampler_binding_infos.find(binding.binding_info.set);
            if (it == space_to_sampler_binding_infos.end())
            {
                it = space_to_sampler_binding_infos
                         .insert({binding.binding_info.set, std::vector<DescriptorBindingInfo>{}})
                         .first;
            }

            it->second.push_back(binding);
        }
        else
        {
            auto it = space_to_resource_binding_infos.find(binding.binding_info.set);
            if (it == space_to_resource_binding_infos.end())
            {
                it = space_to_resource_binding_infos
                         .insert({binding.binding_info.set, std::vector<DescriptorBindingInfo>{}})
                         .first;
            }

            it->second.push_back(binding);
        }
    }

    // Order of D3D12_ROOT_PARAMETER:
    // - Set 0 SRV_UAV_CBV DescriptorTable
    // - Set 1 SRV_UAV_CBV DescriptorTable
    // - ... Rest of SRV_UAV_CBV DescriptorTable sets
    // - Set 0 Sampler DescriptorTable
    // - Set 1 Sampler DescriptorTable
    // - ... Rest of Sampler DescriptorTable sets
    // - RootConstant

    constexpr size_t MAX_NUM_ROOT_PARAMETERS = 10;
    inplace_vector<D3D12_ROOT_PARAMETER1, MAX_NUM_ROOT_PARAMETERS> root_parameters;

    constexpr size_t MAX_DESCRIPTOR_RANGES = 10;
    using RangesVec = inplace_vector<D3D12_DESCRIPTOR_RANGE1, MAX_DESCRIPTOR_RANGES>;
    std::array<RangesVec, MAX_DESCRIPTOR_RANGES> descriptor_ranges_vec;
    uint32_t ranges_vec_index = 0;

    const auto add_descriptor_range =
        [&](std::unordered_map<uint32_t, std::vector<DescriptorBindingInfo>>& binding_map) {
            for (auto& [space, bindings] : binding_map)
            {
                // Current sorting it's SRV -> UAV -> CBV
                std::sort(
                    bindings.begin(),
                    bindings.end(),
                    [](const DescriptorBindingInfo& a, const DescriptorBindingInfo& b) {
                        const D3D12_DESCRIPTOR_RANGE_TYPE a_type = Dx12Shader::get_dx12_descriptor_type(a.type);
                        const D3D12_DESCRIPTOR_RANGE_TYPE b_type = Dx12Shader::get_dx12_descriptor_type(b.type);

                        return a_type < b_type;
                    });

                ShaderType shader_stage_bits = static_cast<ShaderType>(0);
                RangesVec& ranges = descriptor_ranges_vec[ranges_vec_index++];

                for (const DescriptorBindingInfo& binding : bindings)
                {
                    MIZU_ASSERT(binding.type != ShaderResourceType::PushConstant, "Invalid resource type");

                    shader_stage_bits |= binding.stage;

                    const D3D12_DESCRIPTOR_RANGE_TYPE range_type = Dx12Shader::get_dx12_descriptor_type(binding.type);

                    uint32_t num_descriptors = binding.size;
                    if (num_descriptors == 0)
                    {
                        // TODO: Harcoded, should unify with vulkan and other uses
                        num_descriptors = 1024;
                    }

                    D3D12_DESCRIPTOR_RANGE1 range{};
                    range.RangeType = range_type;
                    range.NumDescriptors = num_descriptors;
                    range.BaseShaderRegister = binding.binding_info.binding;
                    range.RegisterSpace = space;
                    range.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;
                    range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

                    ranges.push_back(range);
                }

                MIZU_ASSERT(static_cast<ShaderTypeBitsType>(shader_stage_bits) != 0, "Invalid shader type bits");

                D3D12_ROOT_PARAMETER1 root_parameter{};
                root_parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
                root_parameter.ShaderVisibility = Dx12Shader::get_dx12_shader_stage_bits(shader_stage_bits);
                root_parameter.DescriptorTable.NumDescriptorRanges = static_cast<uint32_t>(ranges.size());
                root_parameter.DescriptorTable.pDescriptorRanges = ranges.data();

                root_parameters.push_back(root_parameter);
            }
        };

    add_descriptor_range(space_to_resource_binding_infos);
    add_descriptor_range(space_to_sampler_binding_infos);

    MIZU_ASSERT(
        push_constant_binding_infos.size() <= 1, "Currently only supporting one push constant per root signature");

    for (const DescriptorBindingInfo& binding : push_constant_binding_infos)
    {
        MIZU_ASSERT(
            binding.type == ShaderResourceType::PushConstant,
            "Push constant binding infos must be of type PushConstant");

        MIZU_ASSERT(binding.size % 4 == 0, "Push constant size must be a multiple of 4");

        // TODO: Setting the ShaderRegister to +1 the biggest constant buffer in space0, as by default Slang will do it
        // that way. Don't like this solution as it is coupling how slang works with render_core but for the moment it
        // works.
        uint32_t shader_register = 0;

        const auto it = space_to_resource_binding_infos.find(0);
        if (it != space_to_resource_binding_infos.end())
        {
            const std::vector<DescriptorBindingInfo>& bindings_in_set = it->second;
            for (const DescriptorBindingInfo& info : bindings_in_set)
            {
                if (info.type == ShaderResourceType::ConstantBuffer && info.binding_info.binding >= shader_register)
                {
                    shader_register = info.binding_info.binding + 1;
                }
            }
        }

        D3D12_ROOT_PARAMETER1 root_parameter{};
        root_parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        root_parameter.ShaderVisibility = Dx12Shader::get_dx12_shader_stage_bits(binding.stage);
        root_parameter.Constants.ShaderRegister = shader_register;
        root_parameter.Constants.RegisterSpace = 0;
        root_parameter.Constants.Num32BitValues = static_cast<uint32_t>(binding.size / 4u);

        root_parameters.push_back(root_parameter);
    }

    D3D12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_desc{};
    root_signature_desc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
    root_signature_desc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    root_signature_desc.Desc_1_1.NumParameters = static_cast<uint32_t>(root_parameters.size());
    root_signature_desc.Desc_1_1.pParameters = root_parameters.data();
    root_signature_desc.Desc_1_1.NumStaticSamplers = 0;
    root_signature_desc.Desc_1_1.pStaticSamplers = nullptr;

    (void)root_signature_info;

    return Dx12Context.root_signature_cache->create(hash, root_signature_desc);
}

} // namespace Mizu::Dx12