#pragma once

#include <cstdint>
#include <functional>
#include <memory>

#include "renderer/abstraction/graphics_pipeline.h"

namespace Mizu {

// Forward declarations
class Texture2D;
class UniformBuffer;
class ICommandBuffer;

using RGFunction = std::function<void(std::shared_ptr<ICommandBuffer> command_buffer)>;

class RenderGraph {
  public:
    RenderGraph();
    ~RenderGraph();

    void register_texture(std::shared_ptr<Texture2D> texture);
    void register_uniform_buffer(std::shared_ptr<UniformBuffer> ub);

    void add_pass(std::string_view name, const GraphicsPipeline::Description& pipeline_description, RGFunction func);

    // TODO: Probably will need to add some sort of synchronization, like fences or semaphores
    void execute();

  private:
};

} // namespace Mizu
