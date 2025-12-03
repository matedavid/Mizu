#pragma once

#include <glm/glm.hpp>
#include <memory>

#include "base/containers/inplace_vector.h"
#include "base/utils/enum_utils.h"

#include "render_core/rhi/framebuffer.h"
#include "render_core/shader/shader_types.h"

namespace Mizu
{

// Forward declarations
class Shader;

struct RasterizationState
{
    enum class PolygonMode
    {
        Fill,
        Line,
        Point,
    };

    enum class CullMode
    {
        None,
        Front,
        Back,
        FrontAndBack,
    };

    enum class FrontFace
    {
        CounterClockwise,
        ClockWise,
    };

    struct DepthBias
    {
        bool enabled = false;
        float constant_factor = 1.0f;
        float clamp = 0.0f;
        float slope_factor = 1.0f;
    };

    bool depth_clamp = false;
    // Controls whether primitives are discarded immediately before the rasterization stage.
    bool rasterizer_discard = false;
    PolygonMode polygon_mode = PolygonMode::Fill;
    CullMode cull_mode = CullMode::Back;
    FrontFace front_face = FrontFace::CounterClockwise;
    DepthBias depth_bias{};
};

struct DepthStencilState
{
    enum class DepthCompareOp
    {
        Never,
        Less,
        Equal,
        LessEqual,
        Greater,
        NotEqual,
        GreaterEqual,
        Always,
    };

    bool depth_test = true;
    bool depth_write = true;
    DepthCompareOp depth_compare_op = DepthCompareOp::Less;

    bool depth_bounds_test = false;
    float min_depth_bounds = 0.0f;
    float max_depth_bounds = 1.0f;

    bool stencil_test = false;
    // TODO: Stencil stuff
};

struct ColorBlendState
{
    enum class Method
    {
        None,
        PerAttachment,
        LogicOperations,
    };

    enum class BlendFactor
    {
        // TODO: There are blend factors missing, will include them as they're needed
        Zero,
        One,
        SourceAlpha,
        OneMinusSourceAlpha
    };

    using ColorComponentBitsType = uint8_t;

    // clang-format off
    enum class ColorComponentBits : ColorComponentBitsType
    {
        None  = 0,
        Red   = 1 << 0,
        Green = 1 << 1,
        Blue  = 1 << 2,
        Alpha = 1 << 3,

        All   = Red | Green | Blue | Alpha
    };
    // clang-format on

    enum class BlendOperation
    {
        Add,
        Subtract,
        ReverseSubtract,
        Min,
        Max
    };

    enum class LogicOperation
    {
        // TODO: There are logic operations missing, will include them as they're needed
        Clear,
    };

    struct AttachmentState
    {
        bool blend_enabled = false;

        BlendFactor src_color_blend_factor = BlendFactor::SourceAlpha;
        BlendFactor dst_color_blend_factor = BlendFactor::OneMinusSourceAlpha;
        BlendOperation color_blend_op = BlendOperation::Add;

        BlendFactor src_alpha_blend_factor = BlendFactor::One;
        BlendFactor dst_alpha_blend_factor = BlendFactor::Zero;
        BlendOperation alpha_blend_op = BlendOperation::Add;

        ColorComponentBits color_write_mask = ColorComponentBits::All;
    };

    Method method = Method::None;
    LogicOperation logic_op = LogicOperation::Clear;
    inplace_vector<AttachmentState, MAX_FRAMEBUFFER_COLOR_ATTACHMENTS + 1> attachments{};
    glm::vec4 blend_constants = glm::vec4{0.0f};
};

IMPLEMENT_ENUM_FLAGS_FUNCTIONS(ColorBlendState::ColorComponentBits, ColorBlendState::ColorComponentBitsType);

//
// GraphicsPipelineDescription
//

struct GraphicsPipelineDescription
{
    std::shared_ptr<Shader> vertex_shader{};
    std::shared_ptr<Shader> fragment_shader{};

    std::shared_ptr<Framebuffer> target_framebuffer{};

    RasterizationState rasterization{};
    DepthStencilState depth_stencil{};
    ColorBlendState color_blend{};
};

//
// ComputePipelineDescription
//

struct ComputePipelineDescription
{
    std::shared_ptr<Shader> compute_shader{};
};

//
// RayTracingPipeline
//

struct RayTracingPipelineDescription
{
    static constexpr size_t MAX_VARIABLE_NUM_SHADERS = 10;

    std::shared_ptr<Shader> raygen_shader{};
    inplace_vector<std::shared_ptr<Shader>, MAX_VARIABLE_NUM_SHADERS> miss_shaders{};
    inplace_vector<std::shared_ptr<Shader>, MAX_VARIABLE_NUM_SHADERS> closest_hit_shaders{};

    uint32_t max_ray_recursion_depth = 1;
};

//
// Pipeline
//

enum class PipelineType
{
    Graphics,
    Compute,
    RayTracing,
};

class Pipeline
{
  public:
    virtual ~Pipeline() = default;

    static std::shared_ptr<Pipeline> create(const GraphicsPipelineDescription& desc);
    static std::shared_ptr<Pipeline> create(const ComputePipelineDescription& desc);
    static std::shared_ptr<Pipeline> create(const RayTracingPipelineDescription& desc);

    virtual PipelineType get_pipeline_type() const = 0;
};

} // namespace Mizu