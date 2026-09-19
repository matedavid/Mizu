#include "dx12_pipeline.h"

#include <string>
#include <vector>

#include "base/debug/logging.h"

#include "dx12_context.h"
#include "dx12_shader.h"
#include "dx12_types.h"

namespace Mizu::Dx12
{

// Rasterization helpers

static D3D12_PRIMITIVE_TOPOLOGY_TYPE get_polygon_mode(RasterizationState::PolygonMode mode)
{
    using PolygonMode = RasterizationState::PolygonMode;

    switch (mode)
    {
    case PolygonMode::Fill:
        return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    case PolygonMode::Line:
        return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
    case PolygonMode::Point:
        return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
    }
}

static D3D12_CULL_MODE get_cull_mode(RasterizationState::CullMode mode)
{
    using CullMode = RasterizationState::CullMode;

    switch (mode)
    {
    case CullMode::None:
        return D3D12_CULL_MODE_NONE;
    case CullMode::Front:
        return D3D12_CULL_MODE_FRONT;
    case CullMode::Back:
        return D3D12_CULL_MODE_BACK;
    case CullMode::FrontAndBack:
        MIZU_UNREACHABLE("Invalid value for Dx12");
        return D3D12_CULL_MODE_NONE;
    }
}

static BOOL get_front_face(RasterizationState::FrontFace mode)
{
    using FrontFace = RasterizationState::FrontFace;

    // https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_rasterizer_desc
    // Determines if a triangle is front- or back-facing. If this member is TRUE, a triangle will be considered
    // front-facing if its vertices are counter-clockwise on the render target and considered back-facing if they are
    // clockwise. If this parameter is FALSE, the opposite is true.

    switch (mode)
    {
    case FrontFace::CounterClockwise:
        return TRUE;
    case FrontFace::ClockWise:
        return FALSE;
    }
}

// Depth Stencil helpers

static D3D12_COMPARISON_FUNC get_depth_compare_op(DepthStencilState::DepthCompareOp op)
{
    using DepthCompareOp = DepthStencilState::DepthCompareOp;

    switch (op)
    {
    case DepthCompareOp::Never:
        return D3D12_COMPARISON_FUNC_NEVER;
    case DepthCompareOp::Less:
        return D3D12_COMPARISON_FUNC_LESS;
    case DepthCompareOp::Equal:
        return D3D12_COMPARISON_FUNC_EQUAL;
    case DepthCompareOp::LessEqual:
        return D3D12_COMPARISON_FUNC_LESS_EQUAL;
    case DepthCompareOp::Greater:
        return D3D12_COMPARISON_FUNC_GREATER;
    case DepthCompareOp::NotEqual:
        return D3D12_COMPARISON_FUNC_NOT_EQUAL;
    case DepthCompareOp::GreaterEqual:
        return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
    case DepthCompareOp::Always:
        return D3D12_COMPARISON_FUNC_ALWAYS;
    }
}

// Color Blend helpers

static D3D12_BLEND get_blend_factor(ColorBlendState::BlendFactor factor)
{
    using BlendFactor = ColorBlendState::BlendFactor;

    switch (factor)
    {
    case BlendFactor::Zero:
        return D3D12_BLEND_ZERO;
    case BlendFactor::One:
        return D3D12_BLEND_ONE;
    case BlendFactor::SourceAlpha:
        return D3D12_BLEND_SRC_ALPHA;
    case BlendFactor::OneMinusSourceAlpha:
        return D3D12_BLEND_INV_SRC_ALPHA;
    }

    MIZU_UNREACHABLE("Unimplemented or Invalid BlendFactor");

    return D3D12_BLEND_ZERO; // Default return to prevent compilation errors
}

static D3D12_BLEND_OP get_blend_operation(ColorBlendState::BlendOperation operation)
{
    using BlendOperation = ColorBlendState::BlendOperation;

    switch (operation)
    {
    case BlendOperation::Add:
        return D3D12_BLEND_OP_ADD;
    case BlendOperation::Subtract:
        return D3D12_BLEND_OP_SUBTRACT;
    case BlendOperation::ReverseSubtract:
        return D3D12_BLEND_OP_REV_SUBTRACT;
    case BlendOperation::Min:
        return D3D12_BLEND_OP_MIN;
    case BlendOperation::Max:
        return D3D12_BLEND_OP_MAX;
    }
}

static UINT8 get_color_component_flags(ColorBlendState::ColorComponentBits bits)
{
    using ColorComponentBits = ColorBlendState::ColorComponentBits;

    UINT8 flags = 0;
    if (bits & ColorComponentBits::Red)
        flags |= D3D12_COLOR_WRITE_ENABLE_RED;
    if (bits & ColorComponentBits::Green)
        flags |= D3D12_COLOR_WRITE_ENABLE_GREEN;
    if (bits & ColorComponentBits::Blue)
        flags |= D3D12_COLOR_WRITE_ENABLE_BLUE;
    if (bits & ColorComponentBits::Alpha)
        flags |= D3D12_COLOR_WRITE_ENABLE_ALPHA;

    return flags;
}

static D3D12_LOGIC_OP get_logic_operation(ColorBlendState::LogicOperation operation)
{
    using LogicOperation = ColorBlendState::LogicOperation;

    switch (operation)
    {
    case LogicOperation::Clear:
        return D3D12_LOGIC_OP_CLEAR;
    }

    MIZU_UNREACHABLE("Unimplemented or Invalid LogicOperation");

    return D3D12_LOGIC_OP_CLEAR; // Default return to prevent compilation errors
}

//
// GraphicsPipeline
//

Dx12Pipeline::Dx12Pipeline(const GraphicsPipelineDescription& desc) : m_pipeline_type(PipelineType::Graphics)
{
    // Shaders

    MIZU_ASSERT(
        desc.vertex_shader != nullptr && desc.vertex_shader->get_type() == ShaderType::Vertex,
        "No vertex shader provided in GraphicsPipeline");
    MIZU_ASSERT(
        desc.fragment_shader != nullptr && desc.fragment_shader->get_type() == ShaderType::Fragment,
        "No fragment shader provided in GraphicsPipeline");

    const Dx12Shader& native_vertex_shader = static_cast<const Dx12Shader&>(*desc.vertex_shader);
    const Dx12Shader& native_fragment_shader = static_cast<const Dx12Shader&>(*desc.fragment_shader);

    // Root signature
    m_root_signature = Dx12Context.pipeline_layout_cache->get(desc.layout);
    m_draw_indirect_command_signature =
        Dx12Context.pipeline_layout_cache->get_draw_indirect_command_signature(desc.layout);
    m_root_signature_info = Dx12Context.pipeline_layout_cache->get_root_signature_info(desc.layout);

    // Input layout

    const auto shader_primitive_type_to_dx12_format = [](ShaderPrimitiveType type) -> DXGI_FORMAT {
        switch (type)
        {
        case ShaderPrimitiveType::Float:
            return DXGI_FORMAT_R32_FLOAT;
        case ShaderPrimitiveType::Float2:
            return DXGI_FORMAT_R32G32_FLOAT;
        case ShaderPrimitiveType::Float3:
            return DXGI_FORMAT_R32G32B32_FLOAT;
        case ShaderPrimitiveType::Float4:
            return DXGI_FORMAT_R32G32B32A32_FLOAT;
        default:
            MIZU_UNREACHABLE("Not implemented shader primitive type");
            return DXGI_FORMAT_UNKNOWN; // Default return value to prevent compilation error
        }
    };

    constexpr size_t MAX_VERTEX_INPUT_ATTRIBUTE_DESCRIPTIONS = 20;
    inplace_vector<D3D12_INPUT_ELEMENT_DESC, MAX_VERTEX_INPUT_ATTRIBUTE_DESCRIPTIONS> input_layout{};
    MIZU_ASSERT(
        desc.vertex_inputs.size() < MAX_VERTEX_INPUT_ATTRIBUTE_DESCRIPTIONS,
        "Number of vertex inputs is greater than the maximum allowed vertex input attribute descriptions");

    uint32_t stride = 0;
    for (const ShaderInputOutput& input : desc.vertex_inputs)
    {
        D3D12_INPUT_ELEMENT_DESC input_element_desc{};
        input_element_desc.SemanticName = input.semantic_name.c_str();
        input_element_desc.SemanticIndex = input.semantic_index;
        input_element_desc.Format = shader_primitive_type_to_dx12_format(input.primitive.type);
        input_element_desc.InputSlot = 0;
        input_element_desc.AlignedByteOffset = stride;
        input_element_desc.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
        input_element_desc.InstanceDataStepRate = 0;

        input_layout.push_back(input_element_desc);

        stride += ShaderPrimitiveType::size(input.primitive.type);
    }

    // Rasterization

    INT depth_bias = D3D12_DEFAULT_DEPTH_BIAS;
    FLOAT depth_bias_clamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    FLOAT slope_scaled_depth_bias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;

    if (desc.rasterization.depth_bias.enabled)
    {
        depth_bias = static_cast<int32_t>(desc.rasterization.depth_bias.constant_factor);
        depth_bias_clamp = desc.rasterization.depth_bias.clamp;
        slope_scaled_depth_bias = desc.rasterization.depth_bias.slope_factor;
    }

    D3D12_RASTERIZER_DESC rasterizer_state{};
    rasterizer_state.FillMode = D3D12_FILL_MODE_SOLID;
    rasterizer_state.CullMode = get_cull_mode(desc.rasterization.cull_mode);
    rasterizer_state.FrontCounterClockwise = get_front_face(desc.rasterization.front_face);
    rasterizer_state.DepthBias = depth_bias;
    rasterizer_state.DepthBiasClamp = depth_bias_clamp;
    rasterizer_state.SlopeScaledDepthBias = slope_scaled_depth_bias;
    rasterizer_state.DepthClipEnable = !desc.rasterization.depth_clamp;
    rasterizer_state.MultisampleEnable = FALSE;     // TODO: Make configurable
    rasterizer_state.AntialiasedLineEnable = FALSE; // TODO: Make configurable
    rasterizer_state.ForcedSampleCount = 0;         // TODO: Make configurable
    rasterizer_state.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    // Depth Stencil

    D3D12_DEPTH_STENCIL_DESC depth_stencil{};
    depth_stencil.DepthEnable = desc.depth_stencil.depth_test;
    depth_stencil.DepthWriteMask =
        desc.depth_stencil.depth_write ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
    depth_stencil.DepthFunc = get_depth_compare_op(desc.depth_stencil.depth_compare_op);
    depth_stencil.StencilEnable = desc.depth_stencil.stencil_test;
    // TODO: depth_stencil.StencilReadMask;
    // TODO: depth_stencil.StencilWriteMask;
    // TODO: depth_stencil.FrontFace;
    // TODO: depth_stencil.BackFace;

    // Color blend

    D3D12_BLEND_DESC blend_desc{};
    blend_desc.AlphaToCoverageEnable = FALSE; // TODO: Configure and investigate what it does
    blend_desc.IndependentBlendEnable = FALSE;

    const size_t num_color_attachments = desc.framebuffer_info.color_attachments.size();
    for (size_t i = 0; i < num_color_attachments; ++i)
    {
        if (desc.color_blend.method == ColorBlendState::Method::None)
        {
            D3D12_RENDER_TARGET_BLEND_DESC state{};
            state.BlendEnable = FALSE;
            state.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

            blend_desc.RenderTarget[i] = state;

            continue;
        }

        MIZU_ASSERT(
            i < desc.color_blend.attachments.size(),
            "Attachment with idx {} does not have a corresponding attachments value");
        const ColorBlendState::AttachmentState& attachment_state = desc.color_blend.attachments[i];

        D3D12_RENDER_TARGET_BLEND_DESC target_blend_desc{};
        target_blend_desc.BlendEnable = attachment_state.blend_enabled;
        target_blend_desc.SrcBlend = get_blend_factor(attachment_state.src_color_blend_factor);
        target_blend_desc.DestBlend = get_blend_factor(attachment_state.dst_color_blend_factor);
        target_blend_desc.BlendOp = get_blend_operation(attachment_state.color_blend_op);
        target_blend_desc.SrcBlendAlpha = get_blend_factor(attachment_state.src_alpha_blend_factor);
        target_blend_desc.DestBlendAlpha = get_blend_factor(attachment_state.dst_alpha_blend_factor);
        target_blend_desc.BlendOpAlpha = get_blend_operation(attachment_state.alpha_blend_op);
        target_blend_desc.LogicOpEnable = desc.color_blend.method == ColorBlendState::Method::LogicOperations;
        target_blend_desc.LogicOp = get_logic_operation(desc.color_blend.logic_op);
        target_blend_desc.RenderTargetWriteMask = get_color_component_flags(attachment_state.color_write_mask);

        blend_desc.RenderTarget[i] = target_blend_desc;
    }

    // Sample desc

    DXGI_SAMPLE_DESC sample_desc{};
    sample_desc.Count = 1;

    // Render targets

    uint32_t num_color_targets = 0;

    static_assert(
        MAX_FRAMEBUFFER_COLOR_ATTACHMENTS == 8, "MAX_FRAMEBUFFER_COLOR_ATTACHMENTS has changed, revisit this code");

    DXGI_FORMAT rtv_formats[8] = {DXGI_FORMAT_UNKNOWN};
    DXGI_FORMAT dsv_format = DXGI_FORMAT_UNKNOWN;

    for (const ImageFormat format : desc.framebuffer_info.color_attachments)
    {
        MIZU_ASSERT(!is_depth_format(format), "Color attachment can't have depth format");
        rtv_formats[num_color_targets++] = get_dx12_image_format(format);
    }

    if (desc.framebuffer_info.depth_stencil_attachment.has_value())
    {
        const ImageFormat format = *desc.framebuffer_info.depth_stencil_attachment;
        MIZU_ASSERT(is_depth_format(format), "Depth stencil attachment must have depth format");
        dsv_format = get_dx12_image_format(format);
    }

    //
    // Create Pipeline
    //

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc{};
    pso_desc.VS = native_vertex_shader.get_shader_bytecode();
    pso_desc.PS = native_fragment_shader.get_shader_bytecode();
    pso_desc.pRootSignature = m_root_signature;
    pso_desc.InputLayout = {input_layout.data(), static_cast<uint32_t>(input_layout.size())};
    pso_desc.RasterizerState = rasterizer_state;
    pso_desc.PrimitiveTopologyType = get_polygon_mode(desc.rasterization.polygon_mode);
    pso_desc.DepthStencilState = depth_stencil;
    pso_desc.BlendState = blend_desc;
    pso_desc.SampleMask = UINT_MAX;
    pso_desc.SampleDesc = sample_desc;
    pso_desc.NumRenderTargets = num_color_targets;
    for (uint32_t i = 0; i < 8; ++i)
        pso_desc.RTVFormats[i] = rtv_formats[i];
    pso_desc.DSVFormat = dsv_format;

    DX12_CHECK(Dx12Context.device->handle()->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&m_pipeline_state)));
}

//
// ComputePipeline
//

Dx12Pipeline::Dx12Pipeline(const ComputePipelineDescription& desc) : m_pipeline_type(PipelineType::Compute)
{
    // Shader

    MIZU_ASSERT(
        desc.compute_shader != nullptr && desc.compute_shader->get_type() == ShaderType::Compute,
        "No compute shader provided in ComputePipeline");

    const Dx12Shader& native_compute_shader = static_cast<const Dx12Shader&>(*desc.compute_shader);

    // Root signature
    m_root_signature = Dx12Context.pipeline_layout_cache->get(desc.layout);
    m_root_signature_info = Dx12Context.pipeline_layout_cache->get_root_signature_info(desc.layout);

    //
    // Create Pipeline
    //

    D3D12_COMPUTE_PIPELINE_STATE_DESC pso_desc{};
    pso_desc.CS = native_compute_shader.get_shader_bytecode();
    pso_desc.pRootSignature = m_root_signature;
    pso_desc.NodeMask = 0;

    DX12_CHECK(Dx12Context.device->handle()->CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(&m_pipeline_state)));
}

//
// RayTracingPipeline
//

Dx12Pipeline::Dx12Pipeline(const RayTracingPipelineDescription& desc) : m_pipeline_type(PipelineType::RayTracing)
{
    MIZU_ASSERT(
        Dx12Context.device->get_properties().ray_tracing_hardware,
        "Can't create RayTracingPipeline because ray tracing hardware is not supported");

    MIZU_ASSERT(desc.raygen_shader != nullptr, "Raygen shader is required in RayTracingPipeline");

    // Root signature
    m_root_signature = Dx12Context.pipeline_layout_cache->get(desc.layout);
    m_root_signature_info = Dx12Context.pipeline_layout_cache->get_root_signature_info(desc.layout);

    //
    // Shaders
    //

    const Dx12Shader& raygen_shader = static_cast<const Dx12Shader&>(*desc.raygen_shader);

    for ([[maybe_unused]] const auto& shader : desc.miss_shaders)
    {
        MIZU_ASSERT(shader != nullptr, "Null miss shader in RayTracingPipeline");
    }

    for ([[maybe_unused]] const auto& hit_group : desc.hit_groups)
    {
        MIZU_ASSERT(
            hit_group.closest_hit_shader != nullptr || hit_group.any_hit_shader != nullptr
                || hit_group.intersection_shader != nullptr,
            "RayTracing hit group must contain at least one shader");
    }

    const uint32_t miss_count = static_cast<uint32_t>(desc.miss_shaders.size());
    const uint32_t hit_group_count = static_cast<uint32_t>(desc.hit_groups.size());
    const uint32_t shader_count = 1 + miss_count + hit_group_count * 3;

    std::vector<std::wstring> export_names;
    export_names.reserve(shader_count);

    std::vector<D3D12_EXPORT_DESC> export_descs;
    export_descs.reserve(shader_count);

    std::vector<D3D12_DXIL_LIBRARY_DESC> libraries;
    libraries.reserve(shader_count);

    // Hit-group names

    std::vector<std::wstring> hit_group_names(hit_group_count);
    std::vector<std::wstring> closest_hit_names(hit_group_count);
    std::vector<std::wstring> any_hit_names(hit_group_count);
    std::vector<std::wstring> intersection_names(hit_group_count);

    // Entry points are ASCII identifiers, so widening char by char is sufficient.
    std::vector<std::wstring> entry_point_names;
    entry_point_names.reserve(shader_count);

    const auto add_shader = [&](const Dx12Shader& shader, std::wstring export_name) {
        const std::string& entry_point = shader.get_entry_point();
        entry_point_names.emplace_back(entry_point.begin(), entry_point.end());

        export_names.push_back(std::move(export_name));

        D3D12_EXPORT_DESC export_desc{};
        export_desc.Name = export_names.back().c_str();

        //
        // Example:
        //
        //     [shader("closesthit")]
        //     void main(...)
        //
        // gets exported as:
        //
        //     ClosestHit_0
        //

        export_desc.ExportToRename = entry_point_names.back().c_str();
        export_desc.Flags = D3D12_EXPORT_FLAG_NONE;
        export_descs.push_back(export_desc);

        D3D12_DXIL_LIBRARY_DESC library{};
        library.DXILLibrary = shader.get_shader_bytecode();
        library.NumExports = 1;
        library.pExports = &export_descs.back();
        libraries.push_back(library);
    };

    // Raygen

    add_shader(raygen_shader, L"RayGen");

    // Miss shaders

    for (uint32_t i = 0; i < miss_count; ++i)
    {
        const Dx12Shader& shader = static_cast<const Dx12Shader&>(*desc.miss_shaders[i]);
        add_shader(shader, L"Miss_" + std::to_wstring(i));
    }

    // Hit-group shaders

    for (uint32_t i = 0; i < hit_group_count; ++i)
    {
        const auto& src = desc.hit_groups[i];

        hit_group_names[i] = L"HitGroup_" + std::to_wstring(i);

        if (src.closest_hit_shader)
        {
            const Dx12Shader& shader = static_cast<const Dx12Shader&>(*src.closest_hit_shader);

            closest_hit_names[i] = L"ClosestHit_" + std::to_wstring(i);

            add_shader(shader, closest_hit_names[i]);
        }

        if (src.any_hit_shader)
        {
            const Dx12Shader& shader = static_cast<const Dx12Shader&>(*src.any_hit_shader);

            any_hit_names[i] = L"AnyHit_" + std::to_wstring(i);

            add_shader(shader, any_hit_names[i]);
        }

        if (src.intersection_shader)
        {
            const Dx12Shader& shader = static_cast<const Dx12Shader&>(*src.intersection_shader);

            intersection_names[i] = L"Intersection_" + std::to_wstring(i);

            add_shader(shader, intersection_names[i]);
        }
    }

    // Hit-group descriptors

    std::vector<D3D12_HIT_GROUP_DESC> native_hit_groups(hit_group_count);

    for (uint32_t i = 0; i < hit_group_count; ++i)
    {
        const auto& src = desc.hit_groups[i];

        D3D12_HIT_GROUP_DESC& dst = native_hit_groups[i];
        dst = {};
        dst.HitGroupExport = hit_group_names[i].c_str();
        dst.Type = src.intersection_shader ? D3D12_HIT_GROUP_TYPE_PROCEDURAL_PRIMITIVE : D3D12_HIT_GROUP_TYPE_TRIANGLES;

        if (src.closest_hit_shader)
        {
            dst.ClosestHitShaderImport = closest_hit_names[i].c_str();
        }

        if (src.any_hit_shader)
        {
            dst.AnyHitShaderImport = any_hit_names[i].c_str();
        }

        if (src.intersection_shader)
        {
            dst.IntersectionShaderImport = intersection_names[i].c_str();
        }
    }

    static constexpr uint32_t MAX_PAYLOAD_SIZE_BYTES = 32;
    static constexpr uint32_t MAX_ATTRIBUTE_SIZE_BYTES = 8; // sizeof(BuiltInTriangleIntersectionAttributes);

    static_assert(
        MAX_ATTRIBUTE_SIZE_BYTES <= D3D12_RAYTRACING_MAX_ATTRIBUTE_SIZE_IN_BYTES,
        "Ray tracing attribute size exceeds D3D12 limit");

    D3D12_RAYTRACING_SHADER_CONFIG shader_config{};
    shader_config.MaxPayloadSizeInBytes = MAX_PAYLOAD_SIZE_BYTES;
    shader_config.MaxAttributeSizeInBytes = MAX_ATTRIBUTE_SIZE_BYTES;

    MIZU_ASSERT(
        desc.max_ray_recursion_depth <= D3D12_RAYTRACING_MAX_DECLARABLE_TRACE_RECURSION_DEPTH,
        "Ray tracing recursion depth exceeds D3D12 limit");

    D3D12_RAYTRACING_PIPELINE_CONFIG pipeline_config{};
    pipeline_config.MaxTraceRecursionDepth = desc.max_ray_recursion_depth;

    D3D12_GLOBAL_ROOT_SIGNATURE global_root_signature{};
    global_root_signature.pGlobalRootSignature = m_root_signature;

    const uint32_t subobject_count = static_cast<uint32_t>(libraries.size() + native_hit_groups.size() + 3);
    std::vector<D3D12_STATE_SUBOBJECT> subobjects(subobject_count);

    uint32_t subobject_index = 0;

    for (uint32_t i = 0; i < libraries.size(); ++i)
    {
        D3D12_STATE_SUBOBJECT& subobject = subobjects[subobject_index++];
        subobject.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
        subobject.pDesc = &libraries[i];
    }

    for (uint32_t i = 0; i < native_hit_groups.size(); ++i)
    {
        D3D12_STATE_SUBOBJECT& subobject = subobjects[subobject_index++];
        subobject.Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP;
        subobject.pDesc = &native_hit_groups[i];
    }

    {
        D3D12_STATE_SUBOBJECT& subobject = subobjects[subobject_index++];
        subobject.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG;
        subobject.pDesc = &shader_config;
    }

    {
        D3D12_STATE_SUBOBJECT& subobject = subobjects[subobject_index++];
        subobject.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG;
        subobject.pDesc = &pipeline_config;
    }

    {
        D3D12_STATE_SUBOBJECT& subobject = subobjects[subobject_index++];
        subobject.Type = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE;
        subobject.pDesc = &global_root_signature;
    }

    MIZU_ASSERT(subobject_index == subobjects.size(), "DX12 state-object subobject count mismatch");

    D3D12_STATE_OBJECT_DESC state_object_desc{};
    state_object_desc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
    state_object_desc.NumSubobjects = static_cast<UINT>(subobjects.size());
    state_object_desc.pSubobjects = subobjects.data();

    //
    // Create Pipeline
    //

    DX12_CHECK(
        Dx12Context.device->handle()->CreateStateObject(&state_object_desc, IID_PPV_ARGS(&m_ray_tracing_state_object)));

    ID3D12StateObjectProperties* state_object_properties = nullptr;
    DX12_CHECK(m_ray_tracing_state_object->QueryInterface(IID_PPV_ARGS(&state_object_properties)));

    // Raygen

    const void* raygen_identifier = state_object_properties->GetShaderIdentifier(L"RayGen");

    MIZU_VERIFY(raygen_identifier != nullptr, "Failed to get RayGen shader identifier");

    std::vector<const void*> miss_identifiers(miss_count);
    std::vector<std::wstring> miss_names(miss_count);

    for (uint32_t i = 0; i < miss_count; ++i)
    {
        miss_names[i] = L"Miss_" + std::to_wstring(i);
        miss_identifiers[i] = state_object_properties->GetShaderIdentifier(miss_names[i].c_str());
        MIZU_VERIFY(miss_identifiers[i] != nullptr, "Failed to get miss shader identifier");
    }

    // Hit groups

    std::vector<const void*> hit_group_identifiers(hit_group_count);

    for (uint32_t i = 0; i < hit_group_count; ++i)
    {
        hit_group_identifiers[i] = state_object_properties->GetShaderIdentifier(hit_group_names[i].c_str());
        MIZU_VERIFY(hit_group_identifiers[i] != nullptr, "Failed to get hit-group shader identifier");
    }

    //
    // Create SBT
    //

    constexpr uint32_t shader_identifier_size = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
    constexpr uint32_t record_alignment = D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT;
    constexpr uint32_t table_alignment = D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT;

    const auto align_up = [](uint32_t value, uint32_t alignment) -> uint32_t {
        MIZU_ASSERT(alignment != 0, "Invalid alignment");

        return (value + alignment - 1) & ~(alignment - 1);
    };

    const uint32_t raygen_stride = align_up(shader_identifier_size, record_alignment);
    const uint32_t miss_stride = align_up(shader_identifier_size, record_alignment);
    const uint32_t hit_stride = align_up(shader_identifier_size, record_alignment);

    const uint32_t raygen_size = raygen_stride;
    const uint32_t miss_size = miss_count * miss_stride;
    const uint32_t hit_size = hit_group_count * hit_stride;

    const uint32_t raygen_offset = 0;
    const uint32_t miss_offset = align_up(raygen_offset + raygen_size, table_alignment);
    const uint32_t hit_offset = align_up(miss_offset + miss_size, table_alignment);
    const uint32_t sbt_size = hit_offset + hit_size;

    MIZU_ASSERT(raygen_stride <= D3D12_RAYTRACING_MAX_SHADER_RECORD_STRIDE, "Raygen record stride exceeds D3D12 limit");
    MIZU_ASSERT(miss_stride <= D3D12_RAYTRACING_MAX_SHADER_RECORD_STRIDE, "Miss record stride exceeds D3D12 limit");
    MIZU_ASSERT(hit_stride <= D3D12_RAYTRACING_MAX_SHADER_RECORD_STRIDE, "Hit record stride exceeds D3D12 limit");

    BufferDescription sbt_desc{};
    sbt_desc.size = sbt_size;
    sbt_desc.usage = BufferUsageBits::RtxShaderBindingTable | BufferUsageBits::HostVisible;
    sbt_desc.name = "SBT_buffer";
    m_sbt_buffer = std::make_unique<Dx12BufferResource>(sbt_desc);

    const D3D12_GPU_VIRTUAL_ADDRESS sbt_address = m_sbt_buffer->handle()->GetGPUVirtualAddress();

    m_ray_generation_region.StartAddress = sbt_address + raygen_offset;
    m_ray_generation_region.SizeInBytes = raygen_size;

    if (miss_count != 0)
    {
        m_miss_region.StartAddress = sbt_address + miss_offset;
        m_miss_region.SizeInBytes = miss_size;
        m_miss_region.StrideInBytes = miss_stride;
    }
    else
    {
        m_miss_region = {};
    }

    if (hit_group_count != 0)
    {
        m_hit_region.StartAddress = sbt_address + hit_offset;
        m_hit_region.SizeInBytes = hit_size;
        m_hit_region.StrideInBytes = hit_stride;
    }
    else
    {
        m_hit_region = {};
    }

    std::vector<uint8_t> sbt_data(sbt_size, 0);

    // Raygen

    memcpy(sbt_data.data() + raygen_offset, raygen_identifier, shader_identifier_size);

    // Miss

    uint8_t* miss_dst = sbt_data.data() + miss_offset;
    for (uint32_t i = 0; i < miss_count; ++i)
    {
        memcpy(miss_dst, miss_identifiers[i], shader_identifier_size);

        miss_dst += miss_stride;
    }

    uint8_t* hit_dst = sbt_data.data() + hit_offset;
    for (uint32_t i = 0; i < hit_group_count; ++i)
    {
        memcpy(hit_dst, hit_group_identifiers[i], shader_identifier_size);

        hit_dst += hit_stride;
    }

    m_sbt_buffer->set_data(sbt_data.data());

    // The state object keeps its own reference to everything the identifiers point into.
    state_object_properties->Release();
}

//
// Other
//

Dx12Pipeline::~Dx12Pipeline()
{
    if (m_pipeline_state != nullptr)
        m_pipeline_state->Release();

    if (m_ray_tracing_state_object != nullptr)
        m_ray_tracing_state_object->Release();
}

} // namespace Mizu::Dx12
