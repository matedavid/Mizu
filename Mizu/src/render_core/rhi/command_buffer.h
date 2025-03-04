#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <string_view>

namespace Mizu
{

// Forward declarations
class Fence;
class Semaphore;
class GraphicsPipeline;
class ComputePipeline;
class RenderPass;
class VertexBuffer;
class IndexBuffer;
class ResourceGroup;
class ImageResource;
enum class ImageResourceState;

enum class CommandBufferType
{
    Graphics,
    Compute,
    Transfer,
};

struct CommandBufferSubmitInfo
{
    std::shared_ptr<Fence> signal_fence = nullptr;
    std::shared_ptr<Semaphore> wait_semaphore = nullptr;
    std::shared_ptr<Semaphore> signal_semaphore = nullptr;
};

class ICommandBuffer
{
  public:
    virtual ~ICommandBuffer() = default;

    virtual void begin() = 0;
    virtual void end() = 0;

    virtual void submit() const = 0;
    virtual void submit(const CommandBufferSubmitInfo& info) const = 0;

    virtual void bind_resource_group(std::shared_ptr<ResourceGroup> resource_group, uint32_t set) = 0;

    template <typename T>
    void push_constant(std::string_view name, const T& data) const
    {
        push_constant(name, sizeof(T), &data);
    }

    virtual void push_constant(std::string_view name, uint32_t size, const void* data) const = 0;

    virtual void transition_resource(ImageResource& image,
                                     ImageResourceState old_state,
                                     ImageResourceState new_state) const = 0;
    // TODO: Don't really like using std::pair for this, specially when we have Range in ImageResourceView, should think
    // a way to abstract mip and layer ranges that is used across the whole engine...
    virtual void transition_resource(ImageResource& image,
                                     ImageResourceState old_state,
                                     ImageResourceState new_state,
                                     std::pair<uint32_t, uint32_t> mip_range,
                                     std::pair<uint32_t, uint32_t> layer_range) const = 0;

    // DEBUG
    virtual void begin_debug_label(const std::string_view& label) const = 0;
    virtual void end_debug_label() const = 0;
};

class RenderCommandBuffer : public virtual ICommandBuffer
{
  public:
    [[nodiscard]] static std::shared_ptr<RenderCommandBuffer> create();

    virtual void begin_render_pass(std::shared_ptr<RenderPass> render_pass) = 0;
    virtual void end_render_pass() = 0;

    virtual void bind_pipeline(std::shared_ptr<GraphicsPipeline> pipeline) = 0;
    virtual void bind_pipeline(std::shared_ptr<ComputePipeline> pipeline) = 0;

    virtual void draw(const VertexBuffer& vertex) const = 0;
    virtual void draw_indexed(const VertexBuffer& vertex, const IndexBuffer& index) const = 0;

    virtual void draw_instanced(const VertexBuffer& vertex, uint32_t instance_count) const = 0;
    virtual void draw_indexed_instanced(const VertexBuffer& vertex,
                                        const IndexBuffer& index,
                                        uint32_t instance_count) const = 0;

    virtual void dispatch(glm::uvec3 group_count) const = 0;

    [[nodiscard]] virtual std::shared_ptr<RenderPass> get_current_render_pass() const = 0;
};

class ComputeCommandBuffer : public virtual ICommandBuffer
{
  public:
    [[nodiscard]] static std::shared_ptr<ComputeCommandBuffer> create();

    virtual void bind_pipeline(std::shared_ptr<ComputePipeline> pipeline) = 0;
    virtual void dispatch(glm::uvec3 group_count) const = 0;
};

} // namespace Mizu
