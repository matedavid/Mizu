#pragma once

#include <functional>
#include <glm/glm.hpp>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

#include "base/utils/enum_utils.h"

#include "mizu_render_core_module.h"

namespace Mizu
{

// Forward declarations
class AccelerationStructure;
class BufferResource;
class DescriptorSet;
class Fence;
class Framebuffer;
class ImageResource;
class Pipeline;
class ResourceGroup;
class Semaphore;
enum class BufferResourceState;
enum class ImageResourceState;
struct AccelerationStructureInstanceData;
struct ImageResourceViewDescription;

enum class CommandBufferType
{
    Graphics,
    Compute,
    Transfer,
};

MIZU_CREATE_ENUM_METADATA(CommandBufferType, static_cast<size_t>(CommandBufferType::Transfer) + 1);

struct CommandBufferSubmitInfo
{
    std::shared_ptr<Fence> signal_fence = nullptr;

    std::vector<std::shared_ptr<Semaphore>> wait_semaphores{};
    std::vector<std::shared_ptr<Semaphore>> signal_semaphores{};
};

class MIZU_RENDER_CORE_API CommandBuffer
{
  public:
    virtual ~CommandBuffer() = default;

    virtual void begin() = 0;
    virtual void end() = 0;

    void submit() const;
    virtual void submit(const CommandBufferSubmitInfo& info) const = 0;

    virtual void bind_resource_group(std::shared_ptr<ResourceGroup> resource_group, uint32_t set) = 0;
    virtual void bind_descriptor_set(std::shared_ptr<DescriptorSet> descriptor_set, uint32_t set) = 0;
    virtual void push_constant(uint32_t size, const void* data) const = 0;

    template <typename T>
    void push_constant(const T& data) const
    {
        push_constant(sizeof(T), &data);
    }

    virtual void begin_render_pass(std::shared_ptr<Framebuffer> framebuffer) = 0;
    virtual void end_render_pass() = 0;

    virtual void bind_pipeline(std::shared_ptr<Pipeline> pipeline) = 0;

    virtual void draw(const BufferResource& vertex) const = 0;
    virtual void draw_indexed(const BufferResource& vertex, const BufferResource& index) const = 0;

    virtual void draw_instanced(const BufferResource& vertex, uint32_t instance_count) const = 0;
    virtual void draw_indexed_instanced(
        const BufferResource& vertex,
        const BufferResource& index,
        uint32_t instance_count) const = 0;

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
        ImageResourceViewDescription range) const = 0;

    virtual void transition_resource(
        const BufferResource& buffer,
        BufferResourceState old_state,
        BufferResourceState new_state) const = 0;

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

    virtual std::shared_ptr<Framebuffer> get_active_framebuffer() const = 0;
};

} // namespace Mizu
