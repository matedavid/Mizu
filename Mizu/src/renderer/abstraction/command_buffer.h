#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <optional>
#include <string_view>

namespace Mizu {

// Forward declarations
class Fence;
class Semaphore;
class GraphicsPipeline;
class ComputePipeline;
class RenderPass;
class VertexBuffer;
class IndexBuffer;
class ResourceGroup;

enum class CommandBufferType {
    Graphics,
    Compute,
    Transfer,
};

struct CommandBufferSubmitInfo {
    std::shared_ptr<Fence> signal_fence = nullptr;
    std::shared_ptr<Semaphore> wait_semaphore = nullptr;
    std::shared_ptr<Semaphore> signal_semaphore = nullptr;
};

class ICommandBuffer {
  public:
    virtual ~ICommandBuffer() = default;

    virtual void begin() = 0;
    virtual void end() = 0;

    virtual void submit() const = 0;
    virtual void submit(const CommandBufferSubmitInfo& info) const = 0;

    virtual void bind_resource_group(const std::shared_ptr<ResourceGroup>& resource_group, uint32_t set) = 0;

    template <typename T>
    void push_constant(std::string_view name, const T& data) {
        push_constant(name, sizeof(T), &data);
    }

    virtual void push_constant(std::string_view name, uint32_t size, const void* data) = 0;
};

class RenderCommandBuffer : public virtual ICommandBuffer {
  public:
    [[nodiscard]] static std::shared_ptr<RenderCommandBuffer> create();

    virtual void bind_pipeline(const std::shared_ptr<GraphicsPipeline>& pipeline) = 0;
    virtual void bind_pipeline(const std::shared_ptr<ComputePipeline>& pipeline) = 0;

    virtual void begin_render_pass(const std::shared_ptr<RenderPass>& render_pass) = 0;
    virtual void end_render_pass(const std::shared_ptr<RenderPass>& render_pass) = 0;

    virtual void draw(const std::shared_ptr<VertexBuffer>& vertex) = 0;
    virtual void draw_indexed(const std::shared_ptr<VertexBuffer>& vertex,
                              const std::shared_ptr<IndexBuffer>& index) = 0;

    virtual void dispatch(glm::uvec3 group_count) = 0;
};

class ComputeCommandBuffer : public virtual ICommandBuffer {
  public:
    [[nodiscard]] static std::shared_ptr<ComputeCommandBuffer> create();

    virtual void bind_pipeline(const std::shared_ptr<ComputePipeline>& pipeline) = 0;
    virtual void dispatch(glm::uvec3 group_count) = 0;
};

} // namespace Mizu
