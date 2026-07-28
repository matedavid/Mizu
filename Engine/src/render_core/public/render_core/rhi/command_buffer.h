#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <memory>
#include <optional>
#include <span>
#include <string_view>

#include "base/containers/inplace_vector.h"
#include "base/utils/enum_utils.h"

#include "mizu_render_core_module.h"
#include "render_core/definitions/resource.h"
#include "render_core/rhi/resource_view.h"

namespace Mizu
{

// Forward declarations
class AccelerationStructure;
class BufferResource;
class DescriptorSet;
class Fence;
class ImageResource;
class Pipeline;
class Semaphore;
enum class AccelerationStructureResourceState;
enum class BufferResourceState;
enum class ImageResourceState;
struct AccelerationStructureInstanceData;
struct RenderPassInfo;

enum class CommandBufferType
{
    Graphics,
    Compute,
    Transfer,
};

struct CommandBufferSubmitInfo
{
    std::shared_ptr<Fence> signal_fence = nullptr;

    static constexpr size_t MAX_SEMAPHORES = 6;
    inplace_vector<std::shared_ptr<Semaphore>, MAX_SEMAPHORES> wait_semaphores{};
    inplace_vector<std::shared_ptr<Semaphore>, MAX_SEMAPHORES> signal_semaphores{};
};

enum class IndexBufferFormat
{
    UInt16,
    UInt32,
};

struct ResourceTransitionInfo
{
    std::optional<CommandBufferType> src_queue_family;
    std::optional<CommandBufferType> dst_queue_family;
    ResourceTransitionMode transition_mode;

    ResourceTransitionInfo(
        std::optional<CommandBufferType> src_queue_family_,
        std::optional<CommandBufferType> dst_queue_family_,
        ResourceTransitionMode transition_mode_)
        : src_queue_family(src_queue_family_)
        , dst_queue_family(dst_queue_family_)
        , transition_mode(transition_mode_)
    {
    }
};

struct BufferTransitionInfo : public ResourceTransitionInfo
{
    BufferResourceState old_state;
    BufferResourceState new_state;
    size_t size;
    size_t offset;

    BufferTransitionInfo(
        BufferResourceState old_state_,
        BufferResourceState new_state_,
        size_t size_,
        size_t offset_,
        std::optional<CommandBufferType> src_queue_family_,
        std::optional<CommandBufferType> dst_queue_family_,
        ResourceTransitionMode transition_mode_)
        : ResourceTransitionInfo(src_queue_family_, dst_queue_family_, transition_mode_)
        , old_state(old_state_)
        , new_state(new_state_)
        , size(size_)
        , offset(offset_)
    {
    }
};

struct ImageTransitionInfo : public ResourceTransitionInfo
{
    ImageResourceState old_state;
    ImageResourceState new_state;
    ImageResourceViewDescription view_desc;

    ImageTransitionInfo(
        ImageResourceState old_state_,
        ImageResourceState new_state_,
        ImageResourceViewDescription view_desc_,
        std::optional<CommandBufferType> src_queue_family_,
        std::optional<CommandBufferType> dst_queue_family_,
        ResourceTransitionMode transition_mode_)
        : ResourceTransitionInfo(src_queue_family_, dst_queue_family_, transition_mode_)
        , old_state(old_state_)
        , new_state(new_state_)
        , view_desc(view_desc_)
    {
    }
};

struct AccelerationStructureTransitionInfo : public ResourceTransitionInfo
{
    AccelerationStructureResourceState old_state;
    AccelerationStructureResourceState new_state;

    AccelerationStructureTransitionInfo(
        AccelerationStructureResourceState old_state_,
        AccelerationStructureResourceState new_state_,
        std::optional<CommandBufferType> src_queue_family_,
        std::optional<CommandBufferType> dst_queue_family_,
        ResourceTransitionMode transition_mode_)
        : ResourceTransitionInfo(src_queue_family_, dst_queue_family_, transition_mode_)
        , old_state(old_state_)
        , new_state(new_state_)
    {
    }
};

struct ImageSubresourceLayers
{
    uint32_t mip_level = 0;
    uint32_t base_array_layer = 0;
    uint32_t layer_count = 1;
};

struct CopyBufferToBufferInfo
{
    uint64_t size = 0;
    uint64_t src_offset = 0;
    uint64_t dst_offset = 0;
};

struct CopyImageToImageInfo
{
    glm::uvec3 source_offset{};
    ImageSubresourceLayers source_subresource_layers{};

    glm::uvec3 dest_offset{};
    ImageSubresourceLayers dest_subresource_layers{};

    glm::uvec3 extent{};
};

struct CopyBufferToImageBase
{
    uint64_t buffer_offset = 0;
    uint32_t buffer_row_length = 0;
    uint32_t buffer_image_height = 0;

    ImageSubresourceLayers image_subresource_layers{};

    glm::uvec3 image_offset{0, 0, 0};
    glm::uvec3 image_extent{0, 0, 0};
};

using CopyBufferToImageInfo = CopyBufferToImageBase;
using CopyImageToBufferInfo = CopyBufferToImageBase;

class MIZU_RENDER_CORE_API CommandBuffer
{
  public:
    virtual ~CommandBuffer() = default;

    virtual void begin() = 0;
    virtual void end() = 0;

    void submit() const;
    virtual void submit(const CommandBufferSubmitInfo& info) const = 0;

    virtual void bind_descriptor_set(std::shared_ptr<DescriptorSet> descriptor_set, uint32_t set) = 0;
    virtual void push_constant(uint32_t size, const void* data) const = 0;

    template <typename T>
    void push_constant(const T& data) const
    {
        push_constant(sizeof(T), &data);
    }

    virtual void begin_render_pass(const RenderPassInfo& info) = 0;
    virtual void end_render_pass() = 0;
    virtual bool is_render_pass_active() const = 0;

    virtual void bind_pipeline(std::shared_ptr<Pipeline> pipeline) = 0;

    virtual void bind_vertex_buffer(const BufferResource& vertex_buffer, uint64_t offset = 0) = 0;
    virtual void bind_index_buffer(
        const BufferResource& index_buffer,
        IndexBufferFormat format,
        uint64_t offset = 0) = 0;

    void bind_index_buffer(const BufferResource& index_buffer, uint64_t offset = 0)
    {
        bind_index_buffer(index_buffer, IndexBufferFormat::UInt32, offset);
    }

    virtual void draw(
        uint32_t vertex_count,
        uint32_t first_vertex,
        uint32_t instance_count = 1,
        uint32_t first_instance = 0) = 0;
    virtual void draw_indexed(
        uint32_t index_count,
        uint32_t first_index,
        uint32_t first_vertex,
        uint32_t instance_count = 1,
        uint32_t first_instance = 0) = 0;

    virtual void draw(const BufferResource& vertex) const = 0;
    virtual void draw_indexed(const BufferResource& vertex, const BufferResource& index) const = 0;

    virtual void draw_instanced(const BufferResource& vertex, uint32_t instance_count) const = 0;
    virtual void draw_indexed_instanced(
        const BufferResource& vertex,
        const BufferResource& index,
        uint32_t instance_count) const = 0;

    virtual void dispatch(glm::uvec3 group_count) const = 0;

    virtual void trace_rays(glm::uvec3 dimensions) const = 0;

    virtual void transition_resource(const BufferResource& buffer, const BufferTransitionInfo& info) const = 0;
    virtual void transition_resource(const ImageResource& image, const ImageTransitionInfo& info) const = 0;
    virtual void transition_resource(
        const AccelerationStructure& accel_struct,
        const AccelerationStructureTransitionInfo& info) const = 0;

    void transition_resource(const BufferResource& buffer, BufferResourceState old_state, BufferResourceState new_state)
        const;
    void transition_resource(
        const BufferResource& buffer,
        BufferResourceState old_state,
        BufferResourceState new_state,
        size_t size,
        size_t offset) const;

    void transition_resource(const ImageResource& image, ImageResourceState old_state, ImageResourceState new_state)
        const;
    void transition_resource(
        const ImageResource& image,
        ImageResourceState old_state,
        ImageResourceState new_state,
        ImageResourceViewDescription view_desc) const;

    virtual void copy_buffer_to_buffer(
        const BufferResource& source,
        const BufferResource& dest,
        const CopyBufferToBufferInfo& info) const = 0;
    virtual void copy_image_to_image(
        const ImageResource& source,
        const ImageResource& dest,
        const CopyImageToImageInfo& info) const = 0;
    virtual void copy_buffer_to_image(
        const BufferResource& buffer,
        const ImageResource& image,
        const CopyBufferToImageInfo& info) const = 0;
    virtual void copy_image_to_buffer(
        const ImageResource& image,
        const BufferResource& buffer,
        const CopyImageToBufferInfo& info) const = 0;

    void copy_buffer_to_buffer(const BufferResource& source, const BufferResource& dest) const;
    void copy_image_to_image(const ImageResource& source, const ImageResource& dest) const;
    void copy_buffer_to_image(const BufferResource& buffer, const ImageResource& image) const;
    void copy_image_to_buffer(const ImageResource& image, const BufferResource& buffer) const;

    virtual void build_blas(const AccelerationStructure& blas, const BufferResource& scratch_buffer) const = 0;
    virtual void build_tlas(
        const AccelerationStructure& blas,
        std::span<AccelerationStructureInstanceData> instances,
        const BufferResource& scratch_buffer) const = 0;
    virtual void update_tlas(
        const AccelerationStructure& tlas,
        std::span<AccelerationStructureInstanceData> instances,
        const BufferResource& scratch_buffer) const = 0;

    virtual void begin_gpu_marker(std::string_view label) const = 0;
    virtual void end_gpu_marker() const = 0;
};

} // namespace Mizu
