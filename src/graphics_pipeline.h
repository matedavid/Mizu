#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <string_view>

namespace Mizu {

// Forward declarations
class Shader;
class Framebuffer;
class ICommandBuffer;
class Texture2D;
class UniformBuffer;

struct RasterizationState {
    enum class PolygonMode {
        Fill,
        Line,
        Point,
    };

    enum class CullMode {
        None,
        Front,
        Back,
        FrontAndBack,
    };

    enum class FrontFace {
        CounterClockwise,
        ClockWise,
    };

    struct DepthBias {
        bool enabled = false;
        float constant_factor = 1.0f;
        float clamp = 0.0f;
        float slope_factor = 1.0f;
    };

    // Controls whether primitives are discarded immediately before the rasterization stage.
    bool rasterizer_discard = false;
    PolygonMode polygon_mode = PolygonMode::Fill;
    CullMode cull_mode = CullMode::Back;
    FrontFace front_face = FrontFace::CounterClockwise;
    DepthBias depth_bias{};
};

struct DepthStencilState {
    enum class DepthCompareOp {
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

struct ColorBlendState {
    bool logic_op_enable = false;
    // TODO: LogicOp logic_op;

    glm::vec4 blend_constants = glm::vec4{0.0f};
};

class GraphicsPipeline {
  public:
    struct Description {
        std::shared_ptr<Shader> shader{};
        std::shared_ptr<Framebuffer> target_framebuffer{};

        RasterizationState rasterization{};
        DepthStencilState depth_stencil{};
        ColorBlendState color_blend{};
    };

    virtual ~GraphicsPipeline() = default;

    [[nodiscard]] static std::shared_ptr<GraphicsPipeline> create(const Description& desc);

    virtual void bind(const std::shared_ptr<ICommandBuffer>& command_buffer) const = 0;
    [[nodiscard]] virtual bool bake() = 0;

    virtual void add_input(std::string_view name, const std::shared_ptr<Texture2D>& texture) = 0;
    virtual void add_input(std::string_view name, const std::shared_ptr<UniformBuffer>& ub) = 0;

    template <typename T>
    [[nodiscard]] bool push_constant(const std::shared_ptr<ICommandBuffer>& command_buffer,
                                    std::string_view name,
                                    const T& data) {
        return push_constant(command_buffer, name, sizeof(T), &data);
    }

    [[nodiscard]] virtual bool push_constant(const std::shared_ptr<ICommandBuffer>& command_buffer,
                                            std::string_view name,
                                            uint32_t size,
                                            const void* data) = 0;
};

} // namespace Mizu
