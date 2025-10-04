#pragma once

#include <functional>
#include <glm/glm.hpp>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

namespace Mizu
{

// Forward declarations
class Fence;
class Semaphore;
class GraphicsPipeline;
class ComputePipeline;
class RayTracingPipeline;
class RenderPass;
class VertexBuffer;
class IndexBuffer;
class ResourceGroup;
class BufferResource;
class ImageResource;
class ImageResourceViewRange;
class AccelerationStructure;
struct AccelerationStructureInstanceData;
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

    std::vector<std::shared_ptr<Semaphore>> wait_semaphores{};
    std::vector<std::shared_ptr<Semaphore>> signal_semaphores{};
};

class CommandBuffer
{
  public:
    virtual ~CommandBuffer() = default;

    static std::shared_ptr<CommandBuffer> create(CommandBufferType type);

    using SubmitSingleTimeFunc = std::function<void(CommandBuffer&)>;
    static void submit_single_time(CommandBufferType type, const SubmitSingleTimeFunc& func);

    virtual void begin() = 0;
    virtual void end() = 0;

    void submit() const;
    virtual void submit(const CommandBufferSubmitInfo& info) const = 0;

    virtual void bind_resource_group(std::shared_ptr<ResourceGroup> resource_group, uint32_t set) = 0;
    virtual void push_constant(std::string_view name, uint32_t size, const void* data) const = 0;

    template <typename T>
    void push_constant(std::string_view name, const T& data) const
    {
        push_constant(name, sizeof(T), &data);
    }

    virtual void begin_render_pass(std::shared_ptr<RenderPass> render_pass) = 0;
    virtual void end_render_pass() = 0;

    virtual void bind_pipeline(std::shared_ptr<GraphicsPipeline> pipeline) = 0;
    virtual void bind_pipeline(std::shared_ptr<ComputePipeline> pipeline) = 0;
    virtual void bind_pipeline(std::shared_ptr<RayTracingPipeline> pipeline) = 0;

    virtual void draw(const VertexBuffer& vertex) const = 0;
    virtual void draw_indexed(const VertexBuffer& vertex, const IndexBuffer& index) const = 0;

    virtual void draw_instanced(const VertexBuffer& vertex, uint32_t instance_count) const = 0;
    virtual void draw_indexed_instanced(const VertexBuffer& vertex, const IndexBuffer& index, uint32_t instance_count)
        const = 0;

    virtual void dispatch(glm::uvec3 group_count) const = 0;

    virtual void trace_rays(glm::uvec3 dimensions) const = 0;

    virtual void transition_resource(
        const ImageResource& image,
        ImageResourceState old_state,
        ImageResourceState new_state) const = 0;

    virtual void transition_resource(
        const ImageResource& image,
        ImageResourceState old_state,
        ImageResourceState new_state,
        ImageResourceViewRange range) const = 0;

    virtual void copy_buffer_to_buffer(const BufferResource& source, const BufferResource& dest) const = 0;
    virtual void copy_buffer_to_image(const BufferResource& buffer, const ImageResource& image) const = 0;

    virtual void build_blas(const AccelerationStructure& blas, const BufferResource& scratch_buffer) const = 0;
    virtual void build_tlas(
        const AccelerationStructure& blas,
        std::span<AccelerationStructureInstanceData> instances,
        const BufferResource& scratch_buffer) const = 0;
    virtual void update_tlas(
        const AccelerationStructure& tlas,
        std::span<AccelerationStructureInstanceData> instances,
        const BufferResource& scratch_buffer) const = 0;

    virtual void begin_gpu_marker(const std::string_view& label) const = 0;
    virtual void end_gpu_marker() const = 0;

    virtual std::shared_ptr<RenderPass> get_active_render_pass() const = 0;
};

#define DEFINE_SPECIFIC_COMMAND_BUFFER(_name, _type)                                    \
    class _name                                                                         \
    {                                                                                   \
      public:                                                                           \
        static std::shared_ptr<CommandBuffer> create()                                  \
        {                                                                               \
            return CommandBuffer::create(_type);                                        \
        }                                                                               \
        static void submit_single_time(const CommandBuffer::SubmitSingleTimeFunc& func) \
        {                                                                               \
            CommandBuffer::submit_single_time(_type, func);                             \
        }                                                                               \
    }

DEFINE_SPECIFIC_COMMAND_BUFFER(RenderCommandBuffer, CommandBufferType::Graphics);
DEFINE_SPECIFIC_COMMAND_BUFFER(ComputeCommandBuffer, CommandBufferType::Compute);
DEFINE_SPECIFIC_COMMAND_BUFFER(TransferCommandBuffer, CommandBufferType::Transfer);

#undef DEFINE_SPECIFIC_COMMAND_BUFFER

} // namespace Mizu
