#include "dx12_root_signature.h"

#include "base/debug/logging.h"
#include "base/utils/hash.h"
#include "render_core/rhi/descriptors.h"

#include "dx12_context.h"
#include "dx12_shader.h"

namespace Mizu::Dx12
{

//
// Dx12DescriptorSetLayoutCache
//

DescriptorSetLayoutHandle Dx12DescriptorSetLayoutCache::create(const DescriptorSetLayoutDescription& desc)
{
    size_t hash = 0;
    Dx12DescriptorSetLayoutInfo descriptor_layout_info{};

    for (const DescriptorItem& item : desc.layout)
    {
        if (item.type == ShaderResourceType::SamplerState)
        {
            descriptor_layout_info.sampler_bindings.push_back(item);
        }
        else
        {
            descriptor_layout_info.resource_bindings.push_back(item);
        }

        hash_combine(hash, item.hash());
    }

    const DescriptorSetLayoutHandle handle = DescriptorSetLayoutHandle{hash};

    if (contains(handle))
    {
        return handle;
    }

    std::sort(
        descriptor_layout_info.resource_bindings.begin(),
        descriptor_layout_info.resource_bindings.end(),
        [](const DescriptorItem& a, const DescriptorItem& b) {
            if (a.binding != b.binding)
            {
                return a.binding < b.binding;
            }

            // Order is: SRV -> UAV -> CBV
            const D3D12_DESCRIPTOR_RANGE_TYPE a_type = Dx12Shader::get_dx12_descriptor_type(a.type);
            const D3D12_DESCRIPTOR_RANGE_TYPE b_type = Dx12Shader::get_dx12_descriptor_type(b.type);

            return a_type < b_type;
        });

    std::sort(
        descriptor_layout_info.sampler_bindings.begin(),
        descriptor_layout_info.sampler_bindings.end(),
        [](const DescriptorItem& a, const DescriptorItem& b) { return a.binding < b.binding; });

    m_cache.insert({handle, descriptor_layout_info});

    return handle;
}

const Dx12DescriptorSetLayoutInfo& Dx12DescriptorSetLayoutCache::get(DescriptorSetLayoutHandle handle) const
{
    MIZU_ASSERT(contains(handle), "Dx12DescriptorSetLayoutCache does not contain handle {}", handle);
    return m_cache.find(handle)->second;
}

bool Dx12DescriptorSetLayoutCache::contains(DescriptorSetLayoutHandle handle) const
{
    return m_cache.find(handle) != m_cache.end();
}

//
// Dx12PipelineLayoutCache
//

Dx12PipelineLayoutCache::~Dx12PipelineLayoutCache()
{
    for (const auto& [_, layout] : m_cache)
    {
        layout->Release();
    }
}

PipelineLayoutHandle Dx12PipelineLayoutCache::create(const PipelineLayoutDescription& desc)
{
    size_t hash = 0;

    for (const DescriptorSetLayoutHandle& handle : desc.set_layouts)
    {
        hash_combine(hash, handle);
    }

    const PipelineLayoutHandle handle = PipelineLayoutHandle{hash};

    if (contains(handle))
    {
        MIZU_LOG_WARNING("Pipeline layout with handle '{}' has already been created", hash);
        return handle;
    }

    std::unordered_map<uint32_t, std::vector<DescriptorItem>> space_to_resource_bindings;
    std::unordered_map<uint32_t, std::vector<DescriptorItem>> space_to_sampler_bindings;
    std::vector<PushConstantItem> push_constant_items;

    for (uint32_t space = 0; space < desc.set_layouts.size(); ++space)
    {
        const DescriptorSetLayoutHandle set_layout_handle = desc.set_layouts[space];
        if (set_layout_handle == 0)
            continue;

        std::vector<DescriptorItem>& resource_bindings = space_to_resource_bindings.insert({space, {}}).first->second;
        std::vector<DescriptorItem>& sampler_bindings = space_to_sampler_bindings.insert({space, {}}).first->second;

        const Dx12DescriptorSetLayoutInfo& info = Dx12Context.descriptor_set_layout_cache->get(set_layout_handle);

        for (const DescriptorItem& item : info.resource_bindings)
        {
            resource_bindings.push_back(item);
        }

        for (const DescriptorItem& item : info.sampler_bindings)
        {
            sampler_bindings.push_back(item);
        }
    }

    if (desc.push_constant.has_value())
    {
        push_constant_items.push_back(*desc.push_constant);
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

    uint32_t num_resource_parameters = 0;
    uint32_t num_sampler_parameters = 0;

    const auto add_descriptor_range = [&](std::unordered_map<uint32_t, std::vector<DescriptorItem>>& binding_map) {
        for (auto& [space, bindings] : binding_map)
        {
            if (bindings.empty())
                continue;

            ShaderType shader_stage_bits = static_cast<ShaderType>(0);
            RangesVec& ranges = descriptor_ranges_vec[ranges_vec_index++];

            bool is_bindless = false;
            for (const DescriptorItem& item : bindings)
            {
                uint32_t descriptor_count = item.count;
                shader_stage_bits |= item.stage;
                const D3D12_DESCRIPTOR_RANGE_TYPE range_type = Dx12Shader::get_dx12_descriptor_type(item.type);

                const auto is_array_resource_type = [](ShaderResourceType type) -> bool {
                    return type == ShaderResourceType::TextureSrv;
                };

                if (descriptor_count == BINDLESS_DESCRIPTOR_COUNT && is_array_resource_type(item.type))
                {
                    descriptor_count = UINT32_MAX;
                    is_bindless = true;
                }

                D3D12_DESCRIPTOR_RANGE1 range{};
                range.RangeType = range_type;
                range.NumDescriptors = descriptor_count;
                range.BaseShaderRegister = item.binding;
                range.RegisterSpace = space;
                range.Flags =
                    is_bindless ? D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE : D3D12_DESCRIPTOR_RANGE_FLAG_NONE;
                range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

                ranges.push_back(range);
            }

            // is_bindless -> layout_bindings.size() == 1
            MIZU_ASSERT(
                !is_bindless || bindings.size() == 1,
                "Currently only supporting one descriptor binding for bindless layouts");

            MIZU_ASSERT(static_cast<ShaderTypeBitsType>(shader_stage_bits) != 0, "Invalid shader type bits");

            D3D12_ROOT_PARAMETER1 root_parameter{};
            root_parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            root_parameter.ShaderVisibility = Dx12Shader::get_dx12_shader_stage_bits(shader_stage_bits);
            root_parameter.DescriptorTable.NumDescriptorRanges = static_cast<uint32_t>(ranges.size());
            root_parameter.DescriptorTable.pDescriptorRanges = ranges.data();

            root_parameters.push_back(root_parameter);
        }
    };

    add_descriptor_range(space_to_resource_bindings);
    num_resource_parameters = static_cast<uint32_t>(root_parameters.size());

    add_descriptor_range(space_to_sampler_bindings);
    num_sampler_parameters = static_cast<uint32_t>(root_parameters.size()) - num_resource_parameters;

    MIZU_ASSERT(push_constant_items.size() <= 1, "Currently only supporting one push constant per root signature");

    for (const PushConstantItem& item : push_constant_items)
    {
        MIZU_ASSERT(item.size % 4 == 0, "Push constant size must be a multiple of 4");

        // TODO: Setting the ShaderRegister to +1 the biggest constant buffer in space0, as by default Slang will do it
        // that way. Don't like this solution as it is coupling how slang works with render_core but for the moment it
        // works.
        uint32_t shader_register = 0;

        const auto it = space_to_resource_bindings.find(0);
        if (it != space_to_resource_bindings.end())
        {
            const std::vector<DescriptorItem>& bindings_in_set = it->second;
            for (const DescriptorItem& info : bindings_in_set)
            {
                if (info.type == ShaderResourceType::ConstantBuffer && info.binding >= shader_register)
                {
                    shader_register = info.binding + 1;
                }
            }
        }

        D3D12_ROOT_PARAMETER1 root_parameter{};
        root_parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        root_parameter.ShaderVisibility = Dx12Shader::get_dx12_shader_stage_bits(item.stage);
        root_parameter.Constants.ShaderRegister = shader_register;
        root_parameter.Constants.RegisterSpace = 0;
        root_parameter.Constants.Num32BitValues = static_cast<uint32_t>(item.size / 4u);

        root_parameters.push_back(root_parameter);
    }

    D3D12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_desc{};
    root_signature_desc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
    root_signature_desc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    root_signature_desc.Desc_1_1.NumParameters = static_cast<uint32_t>(root_parameters.size());
    root_signature_desc.Desc_1_1.pParameters = root_parameters.data();
    root_signature_desc.Desc_1_1.NumStaticSamplers = 0;
    root_signature_desc.Desc_1_1.pStaticSamplers = nullptr;

    const Dx12RootSignatureInfo root_signature_info = {
        .num_parameters = static_cast<uint32_t>(root_parameters.size()),

        .num_resource_parameters = num_resource_parameters,
        .num_sampler_parameters = num_sampler_parameters,
        .num_root_constants = static_cast<uint32_t>(push_constant_items.size()),

        .sampler_parameters_offset = num_resource_parameters,
        .root_constant_offset = num_resource_parameters + num_sampler_parameters,
    };

    ID3DBlob *signature = nullptr, *error = nullptr;
    D3D12SerializeVersionedRootSignature(&root_signature_desc, &signature, &error);
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

    m_cache.insert({handle, root_signature});
    m_root_signature_info_cache.insert({handle, root_signature_info});

    return handle;
}

ID3D12RootSignature* Dx12PipelineLayoutCache::get(PipelineLayoutHandle handle) const
{
    MIZU_ASSERT(contains(handle), "Dx12PipelineLayoutCache does not contain handle {}", handle);
    return m_cache.find(handle)->second;
}

bool Dx12PipelineLayoutCache::contains(PipelineLayoutHandle handle) const
{
    return m_cache.find(handle) != m_cache.end();
}

const Dx12RootSignatureInfo& Dx12PipelineLayoutCache::get_root_signature_info(PipelineLayoutHandle handle) const
{
    const auto it = m_root_signature_info_cache.find(handle);
    MIZU_ASSERT(it != m_root_signature_info_cache.end(), "Dx12PipelineLayoutCache does not contain handle {}", handle);
    return it->second;
}

} // namespace Mizu::Dx12