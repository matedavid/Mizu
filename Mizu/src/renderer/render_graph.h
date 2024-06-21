#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>

#include "renderer/abstraction/graphics_pipeline.h"

namespace Mizu {

// Forward declarations
class Texture2D;
class UniformBuffer;
class RenderCommandBuffer;
class RenderPass;
class Framebuffer;
class Fence;
struct CommandBufferSubmitInfo;

using RGFunction = std::function<void(std::shared_ptr<RenderCommandBuffer> command_buffer)>;

class RenderGraph {
  public:
    RenderGraph();
    ~RenderGraph();

    void register_texture(std::shared_ptr<Texture2D> texture);
    void register_uniform_buffer(std::shared_ptr<UniformBuffer> ub);

    void add_pass(std::string_view name,
                  const GraphicsPipeline::Description& pipeline_description,
                  std::shared_ptr<Framebuffer> framebuffer,
                  RGFunction func);

    // TODO: Probably will need to add some sort of synchronization, like fences or semaphores
    void execute(const CommandBufferSubmitInfo& submit_info);

  private:
    std::shared_ptr<RenderCommandBuffer> m_command_buffer;

    std::unordered_map<size_t, std::shared_ptr<GraphicsPipeline>> m_graphic_pipelines;

    struct RGRenderPassDescription {
        std::shared_ptr<RenderPass> render_pass;
        std::shared_ptr<GraphicsPipeline> pipeline;

        RGFunction function;
    };
    std::vector<RGRenderPassDescription> m_render_passes;

    [[nodiscard]] static size_t get_graphics_pipeline_checksum(const GraphicsPipeline::Description& desc);
};

} // namespace Mizu
