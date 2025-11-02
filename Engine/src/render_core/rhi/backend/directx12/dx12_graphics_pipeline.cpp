#include "dx12_graphics_pipeline.h"

#include "renderer/shader/shader_reflection.h"

#include "render_core/shader/shader_group.h"

// TODO: remove this, use the dx12 version include instead when implemented
#include "render_core/rhi/resource_view.h"

#include "render_core/rhi/backend/directx12/dx12_context.h"
#include "render_core/rhi/backend/directx12/dx12_framebuffer.h"
#include "render_core/rhi/backend/directx12/dx12_image_resource.h"
#include "render_core/rhi/backend/directx12/dx12_shader.h"

namespace Mizu::Dx12
{

Dx12GraphicsPipeline::Dx12GraphicsPipeline(Description desc)
{
    // Shaders

    MIZU_ASSERT(
        desc.vertex_shader != nullptr && desc.vertex_shader->get_type() == ShaderType::Vertex,
        "No vertex shader provided in GraphicsPipeline");
    MIZU_ASSERT(
        desc.fragment_shader != nullptr && desc.fragment_shader->get_type() == ShaderType::Fragment,
        "No fragment shader provided in GraphicsPipeline");

    const Dx12Shader& native_vertex_shader = dynamic_cast<const Dx12Shader&>(*desc.vertex_shader);
    const Dx12Shader& native_fragment_shader = dynamic_cast<const Dx12Shader&>(*desc.fragment_shader);

    // Root signature
    m_shader_group = ShaderGroup{};
    m_shader_group.add_shader(native_vertex_shader);
    m_shader_group.add_shader(native_fragment_shader);

    create_root_signature(m_shader_group);

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

    std::vector<D3D12_INPUT_ELEMENT_DESC> input_layout;
    input_layout.reserve(native_vertex_shader.get_reflection().get_inputs().size());

    uint32_t stride = 0;
    for (const ShaderInputOutput& input : native_vertex_shader.get_reflection().get_inputs())
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
    rasterizer_state.DepthClipEnable = desc.rasterization.depth_clamp;
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

    const std::span<const Framebuffer::Attachment> attachments = desc.target_framebuffer->get_attachments();
    for (uint32_t i = 0; i < attachments.size(); ++i)
    {
        const Framebuffer::Attachment& attachment = attachments[i];
        if (ImageUtils::is_depth_format(attachment.rtv->get_format()))
            continue;

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

    DXGI_FORMAT rtv_formats[8] = {DXGI_FORMAT_UNKNOWN};
    DXGI_FORMAT dsv_format = DXGI_FORMAT_UNKNOWN;

    for (const Framebuffer::Attachment& attachment : attachments)
    {
        MIZU_ASSERT(num_color_targets < 8, "Max number of renter targets reached");

        const ImageFormat format = attachment.rtv->get_format();
        if (ImageUtils::is_depth_format(format))
        {
            MIZU_ASSERT(dsv_format == DXGI_FORMAT_UNKNOWN, "Framebuffer should only have one depth stencil attachment");
            dsv_format = Dx12ImageResource::get_dx12_image_format(format);
        }

        rtv_formats[num_color_targets++] = Dx12ImageResource::get_dx12_image_format(format);
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
    for (uint32_t i = 0; i < 8; ++i)
        pso_desc.RTVFormats[i] = rtv_formats[i];
    pso_desc.DSVFormat = dsv_format;
    pso_desc.NumRenderTargets = num_color_targets + (pso_desc.DSVFormat != DXGI_FORMAT_UNKNOWN);

    DX12_CHECK(Dx12Context.device->handle()->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&m_pipeline_state)));
}

Dx12GraphicsPipeline::~Dx12GraphicsPipeline()
{
    m_pipeline_state->Release();
}

void Dx12GraphicsPipeline::create_root_signature(const ShaderGroup& shader_group)
{
    std::vector<D3D12_ROOT_PARAMETER1> root_parameters;

    for (uint32_t space = 0; space < shader_group.get_max_set(); ++space)
    {
        const std::vector<ShaderResource>& parameters = shader_group.get_parameters_in_set(space);

        std::vector<D3D12_DESCRIPTOR_RANGE1> ranges;

        ShaderType shader_stage_bits = static_cast<ShaderType>(0);

        for (const ShaderResource& parameter : parameters)
        {
            D3D12_DESCRIPTOR_RANGE1 range{};
            range.RangeType = Dx12Shader::get_dx12_descriptor_type(parameter.value);
            range.NumDescriptors = 1;
            range.BaseShaderRegister = parameter.binding_info.binding;
            range.RegisterSpace = space;
            range.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;
            range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

            shader_stage_bits |= shader_group.get_resource_stage_bits(parameter.name);

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

    ID3DBlob *signature, *error;

    // TODO: Should use root signature cache instead of creating it directly
    DX12_CHECK(D3D12SerializeVersionedRootSignature(&root_signature_desc, &signature, &error));
    DX12_CHECK(Dx12Context.device->handle()->CreateRootSignature(
        0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_root_signature)));

    // TODO: Should I print error if something happened????

    if (signature != nullptr)
        signature->Release();
}

D3D12_PRIMITIVE_TOPOLOGY_TYPE Dx12GraphicsPipeline::get_polygon_mode(RasterizationState::PolygonMode mode)
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

D3D12_CULL_MODE Dx12GraphicsPipeline::get_cull_mode(RasterizationState::CullMode mode)
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

BOOL Dx12GraphicsPipeline::get_front_face(RasterizationState::FrontFace mode)
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

D3D12_COMPARISON_FUNC Dx12GraphicsPipeline::get_depth_compare_op(DepthStencilState::DepthCompareOp op)
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

D3D12_BLEND Dx12GraphicsPipeline::get_blend_factor(ColorBlendState::BlendFactor factor)
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

D3D12_BLEND_OP Dx12GraphicsPipeline::get_blend_operation(ColorBlendState::BlendOperation operation)
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

UINT8 Dx12GraphicsPipeline::get_color_component_flags(ColorBlendState::ColorComponentBits bits)
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

D3D12_LOGIC_OP Dx12GraphicsPipeline::get_logic_operation(ColorBlendState::LogicOperation operation)
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

} // namespace Mizu::Dx12