#include "render_core/rhi/queue.h"

#include <algorithm>

#include "base/debug/assert.h"

namespace Mizu
{

namespace
{

void validate_bind_memory(const MemoryPlacement& memory, uint64_t page_size, std::string_view resource_name)
{
    MIZU_ASSERT(memory.heap != nullptr, "Sparse bind of resource '{}' requires a heap", resource_name);
    MIZU_ASSERT(
        memory.offset % page_size == 0,
        "Sparse bind heap offset of resource '{}' must be page aligned (page size = {})",
        resource_name,
        page_size);
}

void validate_buffer_range(const BufferResource& buffer, uint64_t resource_offset, uint64_t size)
{
    MIZU_ASSERT(buffer.is_sparse(), "Buffer '{}' is not sparse", buffer.get_name());
    MIZU_ASSERT(size != 0, "Sparse buffer range can't be empty");
    MIZU_ASSERT(
        resource_offset + size <= buffer.get_size(),
        "Sparse buffer range [{}, {}) exceeds buffer '{}' size ({})",
        resource_offset,
        resource_offset + size,
        buffer.get_name(),
        buffer.get_size());

    const uint64_t page_size = buffer.get_memory_requirements().alignment;
    MIZU_ASSERT(
        resource_offset % page_size == 0,
        "Sparse buffer resource offset must be page aligned (page size = {})",
        page_size);
    // The last page can be partial if it reaches the end of the buffer
    MIZU_ASSERT(
        size % page_size == 0 || resource_offset + size == buffer.get_size(),
        "Sparse buffer range size must be a multiple of the page size unless it reaches the end of the buffer");
}

void validate_image_box(
    const ImageResource& image,
    uint32_t mip_level,
    uint32_t array_layer,
    glm::uvec3 offset,
    glm::uvec3 extent)
{
    MIZU_ASSERT(image.is_sparse(), "Image '{}' is not sparse", image.get_name());

    const SparseImageMemoryRequirements& reqs = image.get_sparse_memory_requirements();
    const ImageDescription& desc = image.get_description();

    MIZU_ASSERT(array_layer < desc.num_layers, "Sparse image array layer out of range");
    MIZU_ASSERT(
        mip_level < reqs.mip_tail_first_mip,
        "Sparse image mip {} is part of the mip tail (first = {}), use add_mip_tail_bind",
        mip_level,
        reqs.mip_tail_first_mip);

    const glm::uvec2 mip_size = compute_mip_size(desc.width, desc.height, mip_level);
    const uint32_t mip_depth = desc.type == ImageType::Image3D ? glm::max(1u, desc.depth >> mip_level) : 1u;
    const glm::uvec3 mip_extent{mip_size, mip_depth};

    const glm::uvec3 end = offset + extent;

    MIZU_ASSERT(extent.x != 0 && extent.y != 0 && extent.z != 0, "Sparse image extent can't be zero");
    MIZU_ASSERT(glm::all(glm::lessThanEqual(end, mip_extent)), "Sparse image box exceeds mip {} size", mip_level);
    MIZU_ASSERT(
        glm::all(glm::equal(offset % reqs.tile_extent, glm::uvec3(0))), "Sparse image offset must be tile aligned");

    for (glm::length_t i = 0; i < 3; ++i)
    {
        MIZU_ASSERT(
            extent[i] % reqs.tile_extent[i] == 0 || end[i] == mip_extent[i],
            "Sparse image extent must be tile aligned unless it reaches the edge of the mip");
    }
}

void validate_mip_tail(const ImageResource& image, uint32_t array_layer)
{
    MIZU_ASSERT(image.is_sparse(), "Image '{}' is not sparse", image.get_name());

    const SparseImageMemoryRequirements& reqs = image.get_sparse_memory_requirements();

    MIZU_ASSERT(reqs.mip_tail_size != 0, "Image '{}' has no mip tail", image.get_name());
    MIZU_ASSERT(
        reqs.single_mip_tail ? array_layer == 0 : array_layer < image.get_num_layers(),
        "Invalid mip tail array layer {}",
        array_layer);
}

void validate_metadata(const ImageResource& image)
{
    MIZU_ASSERT(image.is_sparse(), "Image '{}' is not sparse", image.get_name());
    MIZU_ASSERT(
        image.get_sparse_memory_requirements().metadata_size != 0,
        "Image '{}' does not require sparse metadata",
        image.get_name());
}

} // namespace

SparseBindBatch& SparseBindBatch::add_buffer_bind(const SparseBufferBind& bind)
{
    MIZU_ASSERT(bind.buffer != nullptr, "Sparse buffer bind requires a buffer");
    validate_buffer_range(*bind.buffer, bind.resource_offset, bind.size);
    validate_bind_memory(bind.memory, bind.buffer->get_memory_requirements().alignment, bind.buffer->get_name());

    MIZU_ASSERT(
        !overlaps_buffer_range(bind.buffer, bind.resource_offset, bind.size),
        "Sparse buffer range is already bound or unbound in this batch");

    m_buffer_binds.push_back(bind);
    return *this;
}

SparseBindBatch& SparseBindBatch::add_buffer_unbind(const SparseBufferUnbind& unbind)
{
    MIZU_ASSERT(unbind.buffer != nullptr, "Sparse buffer unbind requires a buffer");
    validate_buffer_range(*unbind.buffer, unbind.resource_offset, unbind.size);

    MIZU_ASSERT(
        !overlaps_buffer_range(unbind.buffer, unbind.resource_offset, unbind.size),
        "Sparse buffer range is already bound or unbound in this batch");

    m_buffer_unbinds.push_back(unbind);
    return *this;
}

SparseBindBatch& SparseBindBatch::add_image_bind(const SparseImageBind& bind)
{
    MIZU_ASSERT(bind.image != nullptr, "Sparse image bind requires an image");
    validate_image_box(*bind.image, bind.mip_level, bind.array_layer, bind.offset, bind.extent);
    validate_bind_memory(bind.memory, bind.image->get_sparse_memory_requirements().page_size, bind.image->get_name());

    MIZU_ASSERT(
        !overlaps_image_box(bind.image, bind.mip_level, bind.array_layer, bind.offset, bind.extent),
        "Sparse image box is already bound or unbound in this batch");

    m_image_binds.push_back(bind);
    return *this;
}

SparseBindBatch& SparseBindBatch::add_image_unbind(const SparseImageUnbind& unbind)
{
    MIZU_ASSERT(unbind.image != nullptr, "Sparse image unbind requires an image");
    validate_image_box(*unbind.image, unbind.mip_level, unbind.array_layer, unbind.offset, unbind.extent);

    MIZU_ASSERT(
        !overlaps_image_box(unbind.image, unbind.mip_level, unbind.array_layer, unbind.offset, unbind.extent),
        "Sparse image box is already bound or unbound in this batch");

    m_image_unbinds.push_back(unbind);
    return *this;
}

SparseBindBatch& SparseBindBatch::add_mip_tail_bind(const SparseMipTailBind& bind)
{
    MIZU_ASSERT(bind.image != nullptr, "Sparse mip tail bind requires an image");
    validate_mip_tail(*bind.image, bind.array_layer);
    validate_bind_memory(bind.memory, bind.image->get_sparse_memory_requirements().page_size, bind.image->get_name());

    MIZU_ASSERT(
        !overlaps_mip_tail(bind.image, bind.array_layer), "Sparse mip tail is already bound or unbound in this batch");

    m_mip_tail_binds.push_back(bind);
    return *this;
}

SparseBindBatch& SparseBindBatch::add_mip_tail_unbind(const SparseMipTailUnbind& unbind)
{
    MIZU_ASSERT(unbind.image != nullptr, "Sparse mip tail unbind requires an image");
    validate_mip_tail(*unbind.image, unbind.array_layer);

    MIZU_ASSERT(
        !overlaps_mip_tail(unbind.image, unbind.array_layer),
        "Sparse mip tail is already bound or unbound in this batch");

    m_mip_tail_unbinds.push_back(unbind);
    return *this;
}

SparseBindBatch& SparseBindBatch::add_metadata_bind(const SparseMetadataBind& bind)
{
    MIZU_ASSERT(bind.image != nullptr, "Sparse metadata bind requires an image");
    validate_metadata(*bind.image);
    validate_bind_memory(bind.memory, bind.image->get_sparse_memory_requirements().page_size, bind.image->get_name());

    MIZU_ASSERT(!overlaps_metadata(bind.image), "Sparse metadata is already bound or unbound in this batch");

    m_metadata_binds.push_back(bind);
    return *this;
}

SparseBindBatch& SparseBindBatch::add_metadata_unbind(const SparseMetadataUnbind& unbind)
{
    MIZU_ASSERT(unbind.image != nullptr, "Sparse metadata unbind requires an image");
    validate_metadata(*unbind.image);

    MIZU_ASSERT(!overlaps_metadata(unbind.image), "Sparse metadata is already bound or unbound in this batch");

    m_metadata_unbinds.push_back(unbind);
    return *this;
}

bool SparseBindBatch::overlaps_buffer_range(const BufferResource* buffer, uint64_t resource_offset, uint64_t size) const
{
    const auto overlaps = [&](const auto& op) {
        return op.buffer == buffer && resource_offset < op.resource_offset + op.size
               && op.resource_offset < resource_offset + size;
    };

    return std::ranges::any_of(m_buffer_binds, overlaps) || std::ranges::any_of(m_buffer_unbinds, overlaps);
}

bool SparseBindBatch::overlaps_image_box(
    const ImageResource* image,
    uint32_t mip_level,
    uint32_t array_layer,
    glm::uvec3 offset,
    glm::uvec3 extent) const
{
    const auto overlaps = [&](const auto& op) {
        return op.image == image && op.mip_level == mip_level && op.array_layer == array_layer
               && glm::all(glm::lessThan(offset, op.offset + op.extent))
               && glm::all(glm::lessThan(op.offset, offset + extent));
    };

    return std::ranges::any_of(m_image_binds, overlaps) || std::ranges::any_of(m_image_unbinds, overlaps);
}

bool SparseBindBatch::overlaps_mip_tail(const ImageResource* image, uint32_t array_layer) const
{
    const auto overlaps = [&](const auto& op) {
        return op.image == image && op.array_layer == array_layer;
    };

    return std::ranges::any_of(m_mip_tail_binds, overlaps) || std::ranges::any_of(m_mip_tail_unbinds, overlaps);
}

bool SparseBindBatch::overlaps_metadata(const ImageResource* image) const
{
    const auto overlaps = [&](const auto& op) {
        return op.image == image;
    };

    return std::ranges::any_of(m_metadata_binds, overlaps) || std::ranges::any_of(m_metadata_unbinds, overlaps);
}

} // namespace Mizu
