#include "streaming/mesh_streaming_pool.h"

#include <algorithm>
#include <numeric>

#include "asset/asset.h"
#include "asset/asset_metadata.h"
#include "base/debug/assert.h"
#include "base/debug/logging.h"
#include "base/math/math.h"
#include "render_core/rhi/command_buffer.h"
#include "render_core/rhi/device.h"

#include "render/runtime/renderer.h"
#include "render/systems/frame_linear_allocator.h"
#include "streaming/sparse_bind_scheduler.h"

namespace Mizu
{

//
// BufferRangeAllocator
//

bool BufferRangeAllocator::init(uint64_t size)
{
    std::lock_guard lock{m_mutex};

    m_capacity = size;
    m_free_ranges.clear();

    if (size != 0)
        m_free_ranges.push_back(FreeRange{0, size});

    return size != 0;
}

std::optional<uint64_t> BufferRangeAllocator::allocate(uint64_t size, uint64_t alignment)
{
    if (size == 0 || alignment == 0)
        return std::nullopt;

    std::lock_guard lock{m_mutex};

    for (size_t index = 0; index < m_free_ranges.size(); ++index)
    {
        FreeRange& range = m_free_ranges[index];
        const uint64_t aligned_offset = math::align_up(range.offset, alignment);
        const uint64_t range_end = range.offset + range.size;

        if (aligned_offset > range_end)
            continue;

        const uint64_t aligned_padding = aligned_offset - range.offset;
        if (aligned_padding > range.size)
            continue;

        const uint64_t available_size = range.size - aligned_padding;
        if (available_size < size)
            continue;

        const uint64_t allocation_end = aligned_offset + size;
        const uint64_t trailing_size = range_end - allocation_end;

        if (aligned_padding != 0 && trailing_size != 0)
        {
            range.size = aligned_padding;
            m_free_ranges.insert(
                m_free_ranges.begin() + static_cast<std::ptrdiff_t>(index + 1),
                FreeRange{allocation_end, trailing_size});
        }
        else if (aligned_padding != 0)
        {
            range.size = aligned_padding;
        }
        else if (trailing_size != 0)
        {
            range.offset = allocation_end;
            range.size = trailing_size;
        }
        else
        {
            m_free_ranges.erase(m_free_ranges.begin() + static_cast<std::ptrdiff_t>(index));
        }

        return aligned_offset;
    }

    return std::nullopt;
}

bool BufferRangeAllocator::try_grow(uint64_t offset, uint64_t size, uint64_t new_size)
{
    if (new_size <= size)
        return true;

    std::lock_guard lock{m_mutex};

    const uint64_t range_end = offset + size;
    auto it = std::ranges::find_if(m_free_ranges, [&](const FreeRange& range) { return range.offset >= range_end; });

    if (it == m_free_ranges.end() || it->offset != range_end)
        return false;

    const uint64_t extra_size = new_size - size;
    if (it->size < extra_size)
        return false;

    if (it->size == extra_size)
    {
        m_free_ranges.erase(it);
    }
    else
    {
        it->offset += extra_size;
        it->size -= extra_size;
    }

    return true;
}

void BufferRangeAllocator::free(uint64_t offset, uint64_t size)
{
    if (size == 0)
        return;

    std::lock_guard lock{m_mutex};

    MIZU_ASSERT(offset + size <= m_capacity, "Trying to free range out of bounds on BufferRangeAllocator");

    FreeRange free_range{offset, size};
    auto free_ranges_it = std::lower_bound(
        m_free_ranges.begin(), m_free_ranges.end(), free_range, [](const FreeRange& a, const FreeRange& b) {
            return a.offset < b.offset;
        });

    if (free_ranges_it != m_free_ranges.end() && free_range.offset + free_range.size == free_ranges_it->offset)
    {
        free_range.size += free_ranges_it->size;
        free_ranges_it = m_free_ranges.erase(free_ranges_it);
    }

    if (free_ranges_it != m_free_ranges.begin())
    {
        auto prev_it = std::prev(free_ranges_it);
        if (prev_it->offset + prev_it->size == free_range.offset)
        {
            prev_it->size += free_range.size;

            if (free_ranges_it != m_free_ranges.end() && prev_it->offset + prev_it->size == free_ranges_it->offset)
            {
                prev_it->size += free_ranges_it->size;
                m_free_ranges.erase(free_ranges_it);
            }

            return;
        }
    }

    m_free_ranges.insert(free_ranges_it, free_range);
}

//
// MeshStreamingPool
//

// TODO: Get this from the device, it's the global toggle if sparse resources are supported
static bool s_mesh_sparse_buffers_supported = true;

static uint64_t get_mesh_resident_vertex_size(const MeshAssetMetadata& metadata, uint8_t first_lod)
{
    if (first_lod >= metadata.get_num_lods())
        return 0;

    return metadata.get_vertex_data_size_bytes(first_lod);
}

static uint64_t get_mesh_resident_index_size(const MeshAssetMetadata& metadata, uint8_t first_lod)
{
    if (first_lod >= metadata.get_num_lods())
        return 0;

    return metadata.get_index_data_size_bytes(first_lod);
}

static bool are_mesh_pages_contiguous(const GpuPage& first, const GpuPage& second, uint64_t page_size)
{
    return first.placement.heap == second.placement.heap
           && second.placement.offset == first.placement.offset + page_size;
}

MeshStreamingPool::MeshStreamingPool(
    MeshStreamingPoolDescription desc,
    GpuPageAllocator& page_allocator,
    SparseBindScheduler& sparse_bind_scheduler)
    : m_description(std::move(desc))
    , m_page_allocator(page_allocator)
    , m_sparse_bind_scheduler(sparse_bind_scheduler)
{
    const uint64_t page_size = get_page_size();

    const uint64_t vertex_buffer_alignment = std::lcm(page_size, sizeof(MeshAssetVertex));
    const uint64_t vertex_buffer_size = math::align_up(m_description.vertex_buffer_size, vertex_buffer_alignment);

    const uint64_t index_buffer_size = math::align_up(m_description.index_buffer_size, page_size);

    BufferDescription vertex_buffer_desc =
        create_vertex_buffer_desc(vertex_buffer_size, sizeof(MeshAssetVertex), "MeshStreamingPool::VertexBuffer");
    vertex_buffer_desc.usage |= BufferUsageBits::TransferSrc | BufferUsageBits::TransferDst;

    BufferDescription index_buffer_desc = create_index_buffer_desc(index_buffer_size, "MeshStreamingPool::IndexBuffer");
    index_buffer_desc.usage |= BufferUsageBits::TransferSrc | BufferUsageBits::TransferDst;

    m_vertex_allocator.init(vertex_buffer_size);
    m_index_allocator.init(index_buffer_size);

    const auto is_page_compatible = [page_size](const BufferResource& buffer) {
        const uint64_t alignment = buffer.get_memory_requirements().alignment;
        return alignment != 0 && page_size % alignment == 0;
    };

    if (s_mesh_sparse_buffers_supported)
    {
        m_vertex_buffer = g_render_device->create_sparse_buffer(vertex_buffer_desc);
        m_index_buffer = g_render_device->create_sparse_buffer(index_buffer_desc);

        const bool vertex_compatible = is_page_compatible(*m_vertex_buffer);
        const bool index_compatible = is_page_compatible(*m_index_buffer);

        if (vertex_compatible && index_compatible)
        {
            m_backing = ResourceBacking::Sparse;

            m_vertex_page_table.buffer = m_vertex_buffer.get();
            m_vertex_page_table.slots.resize(vertex_buffer_size / page_size);

            m_index_page_table.buffer = m_index_buffer.get();
            m_index_page_table.slots.resize(index_buffer_size / page_size);

            return;
        }

        MIZU_LOG_WARNING("Sparse mesh buffers are not compatible with the page allocator, using placed buffers");
    }

    m_backing = ResourceBacking::Placed;

    m_vertex_buffer = g_render_device->create_buffer(vertex_buffer_desc);
    m_index_buffer = g_render_device->create_buffer(index_buffer_desc);
}

MeshStreamingPool::~MeshStreamingPool()
{
    for (PageSlot& slot : m_vertex_page_table.slots)
    {
        if (slot.bound)
        {
            m_page_allocator.free_page(slot.page);
        }
    }

    for (PageSlot& slot : m_index_page_table.slots)
    {
        if (slot.bound)
        {
            m_page_allocator.free_page(slot.page);
        }
    }
}

void MeshStreamingPool::update()
{
    process_pending_range_releases();
    process_pending_page_releases();
}

bool MeshStreamingPool::register_mesh(MeshAssetHandle handle, const MeshAssetMetadata& metadata)
{
    if (m_mesh_entry_map.contains(handle))
    {
        MIZU_UNREACHABLE("Trying to register mesh that is already registered: {}", handle.get_id());
        return false;
    }

    MeshEntry entry{};

    if (m_backing == ResourceBacking::Sparse)
    {
        const uint64_t vertex_size = get_mesh_resident_vertex_size(metadata, 0);
        const uint64_t index_size = get_mesh_resident_index_size(metadata, 0);

        if (vertex_size == 0 || index_size == 0)
        {
            MIZU_LOG_ERROR(
                "Failed to get resident size for mesh {}, vertex_size={}, index_size={}",
                handle.get_id(),
                vertex_size,
                index_size);
            return false;
        }

        const std::optional<uint64_t> vertex_offset =
            m_vertex_allocator.allocate(vertex_size, metadata.get_vertex_stride_bytes());
        if (!vertex_offset.has_value())
        {
            MIZU_LOG_ERROR("Failed to reserve {} vertex bytes for mesh {}", vertex_size, handle.get_id());
            return false;
        }

        const std::optional<uint64_t> index_offset =
            m_index_allocator.allocate(index_size, metadata.get_index_stride_bytes());
        if (!index_offset.has_value())
        {
            MIZU_LOG_ERROR("Failed to reserve {} index bytes for mesh {}", index_size, handle.get_id());
            m_vertex_allocator.free(*vertex_offset, vertex_size);
            return false;
        }

        entry.vertex_range = BufferRange{*vertex_offset, vertex_size};
        entry.index_range = BufferRange{*index_offset, index_size};
    }

    m_mesh_entry_map.emplace(handle, entry);

    return true;
}

void MeshStreamingPool::unregister_mesh(MeshAssetHandle handle)
{
    const auto entry_it = m_mesh_entry_map.find(handle);
    if (entry_it == m_mesh_entry_map.end())
    {
        MIZU_UNREACHABLE("Trying to unregister mesh that was not registered: {}", handle.get_id());
        return;
    }

    MeshEntry& entry = entry_it->second;

    if (m_backing == ResourceBacking::Sparse)
    {
        resize_resident_pages(m_vertex_page_table, entry.vertex_range, entry.resident_vertex_size, 0);
        resize_resident_pages(m_index_page_table, entry.index_range, entry.resident_index_size, 0);
    }

    release_range_delayed(m_vertex_allocator, entry.vertex_range);
    release_range_delayed(m_index_allocator, entry.index_range);

    if (entry.pending_copy_vertex_range.has_value())
    {
        release_range_delayed(m_vertex_allocator, *entry.pending_copy_vertex_range);
    }

    if (entry.pending_copy_index_range.has_value())
    {
        release_range_delayed(m_index_allocator, *entry.pending_copy_index_range);
    }

    m_mesh_entry_map.erase(entry_it);
}

bool MeshStreamingPool::update_residency_state(
    MeshAssetHandle handle,
    const MeshAssetMetadata& metadata,
    uint8_t first_lod)
{
    MeshEntry* entry = get_entry(handle);
    MIZU_ASSERT(entry != nullptr, "Failed to find entry for handle: {}", handle.get_id());
    MIZU_ASSERT(
        first_lod <= metadata.get_num_lods(),
        "Invalid first lod {}, mesh has {} lods",
        first_lod,
        metadata.get_num_lods());

    switch (m_backing)
    {
    case ResourceBacking::Placed:
        return update_placed_residency_state(*entry, metadata, first_lod);
    case ResourceBacking::Sparse:
        return update_sparse_residency_state(*entry, metadata, first_lod);
    }

    MIZU_UNREACHABLE("Invalid resource backing");
    return false;
}

static void record_pending_copy(
    CommandBuffer& command,
    const BufferResource& buffer,
    uint64_t src_offset,
    uint64_t dst_offset,
    uint64_t size)
{
    command.transition_resource(buffer, BufferResourceState::TransferDst, BufferResourceState::TransferSrc);

    command.copy_buffer_to_buffer(
        buffer,
        buffer,
        CopyBufferToBufferInfo{
            .size = size,
            .src_offset = src_offset,
            .dst_offset = dst_offset,
        });

    command.transition_resource(buffer, BufferResourceState::TransferSrc, BufferResourceState::TransferDst);
}

static void record_lods_upload(
    const MeshAssetMetadata& metadata,
    const MeshUploadData& upload,
    const BufferResource& vertex_buffer,
    uint64_t vertex_offset,
    const BufferResource& index_buffer,
    uint64_t index_offset,
    CommandBuffer& command,
    FrameLinearAllocator& frame_allocator)
{
    MIZU_ASSERT(!upload.data.empty(), "Upgrade without staged data");

    const uint8_t lod_count = upload.old_first_lod - upload.new_first_lod;
    const uint64_t range_file_offset = metadata.get_lod_range_file_offset(upload.new_first_lod, lod_count);
    const uint64_t range_file_size = metadata.get_lod_range_file_size_bytes(upload.new_first_lod, lod_count);

    MIZU_ASSERT(
        upload.data.size() >= range_file_size,
        "Staged data is smaller than the lod range ({} < {})",
        upload.data.size(),
        range_file_size);

    const uint64_t alignment = g_render_device->get_properties().min_raw_buffer_offset_alignment;
    const FrameAllocation allocation = frame_allocator.allocate(range_file_size, alignment, sizeof(uint8_t));
    allocation.upload(upload.data.first(range_file_size));

    const BufferResource& staging_buffer = *frame_allocator.get_buffer();
    const uint64_t staging_offset = allocation.view.desc.offset;

    for (uint8_t lod = upload.new_first_lod; lod < upload.old_first_lod; ++lod)
    {
        const uint64_t vertex_size = metadata.get_lod_vertex_size_bytes(lod);
        if (vertex_size != 0)
        {
            command.copy_buffer_to_buffer(
                staging_buffer,
                vertex_buffer,
                CopyBufferToBufferInfo{
                    .size = vertex_size,
                    .src_offset = staging_offset + metadata.get_lod_vertex_file_offset(lod) - range_file_offset,
                    .dst_offset = vertex_offset + metadata.get_lod_vertex_offset_bytes(lod),
                });
        }

        const uint64_t index_size = metadata.get_lod_index_size_bytes(lod);
        if (index_size != 0)
        {
            command.copy_buffer_to_buffer(
                staging_buffer,
                index_buffer,
                CopyBufferToBufferInfo{
                    .size = index_size,
                    .src_offset = staging_offset + metadata.get_lod_index_file_offset(lod) - range_file_offset,
                    .dst_offset = index_offset + metadata.get_lod_index_offset_bytes(lod),
                });
        }
    }
}

void MeshStreamingPool::record_gpu_upload(
    MeshAssetHandle handle,
    const MeshAssetMetadata& metadata,
    const MeshUploadData& upload,
    CommandBuffer& command,
    FrameLinearAllocator& frame_allocator)
{
    MeshEntry* entry = get_entry(handle);
    MIZU_ASSERT(entry != nullptr, "Failed to find entry for handle {}", handle.get_id());

    if (upload.new_first_lod > upload.old_first_lod)
    {
        shrink_resident_ranges(
            *entry,
            get_mesh_resident_vertex_size(metadata, upload.new_first_lod),
            get_mesh_resident_index_size(metadata, upload.new_first_lod));

        return;
    }

    if (upload.new_first_lod == upload.old_first_lod)
        return;

    if (entry->pending_copy_vertex_range.has_value())
    {
        const BufferRange& pending_copy_range = *entry->pending_copy_vertex_range;
        record_pending_copy(
            command, *m_vertex_buffer, pending_copy_range.offset, entry->vertex_range.offset, pending_copy_range.size);

        release_range_delayed(m_vertex_allocator, pending_copy_range);
        entry->pending_copy_vertex_range.reset();
    }

    if (entry->pending_copy_index_range.has_value())
    {
        const BufferRange& pending_copy_range = *entry->pending_copy_index_range;
        record_pending_copy(
            command, *m_index_buffer, pending_copy_range.offset, entry->index_range.offset, pending_copy_range.size);

        release_range_delayed(m_index_allocator, pending_copy_range);
        entry->pending_copy_index_range.reset();
    }

    record_lods_upload(
        metadata,
        upload,
        *m_vertex_buffer,
        entry->vertex_range.offset,
        *m_index_buffer,
        entry->index_range.offset,
        command,
        frame_allocator);
}

std::optional<GpuMeshAllocationHandle> MeshStreamingPool::get_allocation(MeshAssetHandle handle) const
{
    const MeshEntry* entry = get_entry(handle);
    if (entry == nullptr)
        return std::nullopt;

    return GpuMeshAllocationHandle{
        .handle = handle,
        .vertex_offset = entry->vertex_range.offset,
        .vertex_size = entry->resident_vertex_size,
        .index_offset = entry->index_range.offset,
        .index_size = entry->resident_index_size,
    };
}

bool MeshStreamingPool::update_placed_residency_state(
    MeshEntry& entry,
    const MeshAssetMetadata& metadata,
    uint8_t first_lod)
{
    MIZU_ASSERT(
        !entry.pending_copy_vertex_range.has_value() && !entry.pending_copy_index_range.has_value(),
        "Updating residency of a mesh with a pending copy");

    const uint8_t num_lods = metadata.get_num_lods();
    const uint8_t old_first_lod = num_lods - entry.resident_lods;

    if (first_lod == old_first_lod)
        return true;

    if (first_lod == num_lods)
    {
        release_range_delayed(m_vertex_allocator, entry.vertex_range);
        release_range_delayed(m_index_allocator, entry.index_range);

        entry = MeshEntry{};
        return true;
    }

    if (first_lod > old_first_lod)
    {
        entry.resident_lods = num_lods - first_lod;
        return true;
    }

    const uint64_t old_vertex_size = entry.vertex_range.size;

    const uint64_t new_vertex_size = get_mesh_resident_vertex_size(metadata, first_lod);
    const uint64_t new_index_size = get_mesh_resident_index_size(metadata, first_lod);

    if (!grow_placed_range(
            m_vertex_allocator,
            entry.vertex_range,
            entry.pending_copy_vertex_range,
            new_vertex_size,
            metadata.get_vertex_stride_bytes()))
    {
        MIZU_LOG_WARNING("Failed to allocate {} vertex bytes for mesh streaming", new_vertex_size);
        return false;
    }

    if (!grow_placed_range(
            m_index_allocator,
            entry.index_range,
            entry.pending_copy_index_range,
            new_index_size,
            metadata.get_index_stride_bytes()))
    {
        MIZU_LOG_WARNING("Failed to allocate {} index bytes for mesh streaming", new_index_size);
        rollback_placed_grow(m_vertex_allocator, entry.vertex_range, entry.pending_copy_vertex_range, old_vertex_size);
        return false;
    }

    entry.resident_vertex_size = new_vertex_size;
    entry.resident_index_size = new_index_size;
    entry.resident_lods = num_lods - first_lod;

    return true;
}

bool MeshStreamingPool::update_sparse_residency_state(
    MeshEntry& entry,
    const MeshAssetMetadata& metadata,
    uint8_t first_lod)
{
    const uint8_t num_lods = metadata.get_num_lods();
    const uint8_t old_first_lod = num_lods - entry.resident_lods;

    if (first_lod == old_first_lod)
        return true;

    if (first_lod == num_lods)
    {
        resize_resident_pages(m_vertex_page_table, entry.vertex_range, entry.resident_vertex_size, 0);
        resize_resident_pages(m_index_page_table, entry.index_range, entry.resident_index_size, 0);

        entry.resident_vertex_size = 0;
        entry.resident_index_size = 0;
        entry.resident_lods = 0;
        return true;
    }

    if (first_lod > old_first_lod)
    {
        entry.resident_lods = num_lods - first_lod;
        return true;
    }

    const uint64_t new_vertex_size = get_mesh_resident_vertex_size(metadata, first_lod);
    const uint64_t new_index_size = get_mesh_resident_index_size(metadata, first_lod);

    if (!resize_resident_pages(m_vertex_page_table, entry.vertex_range, entry.resident_vertex_size, new_vertex_size))
        return false;

    if (!resize_resident_pages(m_index_page_table, entry.index_range, entry.resident_index_size, new_index_size))
    {
        resize_resident_pages(m_vertex_page_table, entry.vertex_range, new_vertex_size, entry.resident_vertex_size);
        return false;
    }

    entry.resident_vertex_size = new_vertex_size;
    entry.resident_index_size = new_index_size;
    entry.resident_lods = num_lods - first_lod;

    return true;
}

bool MeshStreamingPool::grow_placed_range(
    BufferRangeAllocator& allocator,
    BufferRange& range,
    std::optional<BufferRange>& pending_copy_range,
    uint64_t new_size,
    uint64_t alignment)
{
    if (new_size <= range.size)
        return true;

    if (range.size != 0)
    {
        if (allocator.try_grow(range.offset, range.size, new_size))
        {
            range.size = new_size;
            return true;
        }
    }

    const std::optional<uint64_t> offset = allocator.allocate(new_size, alignment);
    if (!offset.has_value())
        return false;

    if (range.size != 0)
    {
        pending_copy_range = range;
    }

    range = BufferRange{*offset, new_size};
    return true;
}

void MeshStreamingPool::rollback_placed_grow(
    BufferRangeAllocator& allocator,
    BufferRange& range,
    std::optional<BufferRange>& pending_copy_range,
    uint64_t old_size)
{
    if (pending_copy_range.has_value())
    {
        allocator.free(range.offset, range.size);
        range = *pending_copy_range;
        pending_copy_range.reset();
        return;
    }

    if (range.size > old_size)
    {
        allocator.free(range.offset + old_size, range.size - old_size);
    }

    range.size = old_size;
    if (old_size == 0)
    {
        range.offset = 0;
    }
}

void MeshStreamingPool::shrink_resident_ranges(MeshEntry& entry, uint64_t new_vertex_size, uint64_t new_index_size)
{
    if (m_backing == ResourceBacking::Sparse)
    {
        resize_resident_pages(m_vertex_page_table, entry.vertex_range, entry.resident_vertex_size, new_vertex_size);
        resize_resident_pages(m_index_page_table, entry.index_range, entry.resident_index_size, new_index_size);
    }
    else
    {
        if (new_vertex_size < entry.vertex_range.size)
        {
            release_range_delayed(
                m_vertex_allocator,
                BufferRange{entry.vertex_range.offset + new_vertex_size, entry.vertex_range.size - new_vertex_size});
            entry.vertex_range.size = new_vertex_size;
        }

        if (new_index_size < entry.index_range.size)
        {
            release_range_delayed(
                m_index_allocator,
                BufferRange{entry.index_range.offset + new_index_size, entry.index_range.size - new_index_size});
            entry.index_range.size = new_index_size;
        }
    }

    entry.resident_vertex_size = new_vertex_size;
    entry.resident_index_size = new_index_size;
}

std::pair<uint64_t, uint64_t> MeshStreamingPool::get_page_slot_span(const BufferRange& range, uint64_t size) const
{
    const uint64_t page_size = get_page_size();
    const uint64_t first_slot = range.offset / page_size;

    if (size == 0)
        return {first_slot, first_slot};

    return {first_slot, math::ceil_div(range.offset + size, page_size)};
}

bool MeshStreamingPool::acquire_pages(SparsePageTable& table, uint64_t first_slot, uint64_t last_slot)
{
    MIZU_ASSERT(last_slot <= table.slots.size(), "Page slot range out of bounds");

    uint32_t num_new_pages = 0;
    for (uint64_t slot = first_slot; slot < last_slot; ++slot)
    {
        if (!table.slots[slot].bound)
        {
            num_new_pages += 1;
        }
    }

    std::vector<GpuPage> new_pages{};
    if (num_new_pages != 0)
    {
        if (!m_page_allocator.allocate_pages(num_new_pages, new_pages))
        {
            MIZU_LOG_WARNING("Failed to allocate {} sparse pages for mesh streaming", num_new_pages);
            return false;
        }
    }

    std::vector<uint64_t> newly_bound_slots{};
    newly_bound_slots.reserve(num_new_pages);

    size_t next_page = 0;
    for (uint64_t slot_idx = first_slot; slot_idx < last_slot; ++slot_idx)
    {
        PageSlot& slot = table.slots[slot_idx];
        slot.references += 1;

        if (slot.bound)
            continue;

        slot.page = new_pages[next_page];
        slot.bound = true;
        next_page += 1;

        newly_bound_slots.push_back(slot_idx);
    }

    const uint64_t page_size = get_page_size();

    size_t first = 0;
    while (first < newly_bound_slots.size())
    {
        size_t run = 1;
        while (first + run < newly_bound_slots.size())
        {
            const uint64_t prev_slot = newly_bound_slots[first + run - 1];
            const uint64_t slot = newly_bound_slots[first + run];

            if (slot != prev_slot + 1)
                break;

            if (!are_mesh_pages_contiguous(table.slots[prev_slot].page, table.slots[slot].page, page_size))
                break;

            run += 1;
        }

        const uint64_t first_slot_idx = newly_bound_slots[first];

        m_sparse_bind_scheduler.enqueue_buffer_bind(
            SparseBufferBind{
                .buffer = table.buffer,
                .resource_offset = first_slot_idx * page_size,
                .size = run * page_size,
                .memory = table.slots[first_slot_idx].page.placement,
            });

        first += run;
    }

    return true;
}

void MeshStreamingPool::release_pages(SparsePageTable& table, uint64_t first_slot, uint64_t last_slot)
{
    MIZU_ASSERT(last_slot <= table.slots.size(), "Page slot range out of bounds");

    for (uint64_t slot_idx = first_slot; slot_idx < last_slot; ++slot_idx)
    {
        PageSlot& slot = table.slots[slot_idx];
        MIZU_ASSERT(slot.references > 0, "Releasing sparse page slot {} with no references", slot_idx);

        slot.references -= 1;
        if (slot.references != 0)
            continue;

        slot.release_serial += 1;

        m_pending_page_releases.push_back({
            .table = &table,
            .slot = slot_idx,
            .serial = slot.release_serial,
            .frames = m_description.release_delay_frames,
        });
    }
}

bool MeshStreamingPool::resize_resident_pages(
    SparsePageTable& table,
    const BufferRange& range,
    uint64_t old_size,
    uint64_t new_size)
{
    const uint64_t old_last_slot = get_page_slot_span(range, old_size).second;
    const uint64_t new_last_slot = get_page_slot_span(range, new_size).second;

    if (new_last_slot > old_last_slot)
    {
        return acquire_pages(table, old_last_slot, new_last_slot);
    }

    if (new_last_slot < old_last_slot)
    {
        release_pages(table, new_last_slot, old_last_slot);
    }

    return true;
}

void MeshStreamingPool::release_range_delayed(BufferRangeAllocator& allocator, const BufferRange& range)
{
    if (range.size == 0)
        return;

    m_pending_range_releases.push_back({
        .allocator = &allocator,
        .range = range,
        .frames = m_description.release_delay_frames,
    });
}

void MeshStreamingPool::process_pending_range_releases()
{
    auto it = m_pending_range_releases.begin();
    while (it != m_pending_range_releases.end())
    {
        it->frames -= 1;

        if (it->frames != 0)
        {
            it = std::next(it);
            continue;
        }

        it->allocator->free(it->range.offset, it->range.size);
        it = m_pending_range_releases.erase(it);
    }
}

void MeshStreamingPool::process_pending_page_releases()
{
    const uint64_t page_size = get_page_size();

    auto it = m_pending_page_releases.begin();
    while (it != m_pending_page_releases.end())
    {
        it->frames -= 1;

        if (it->frames != 0)
        {
            it = std::next(it);
            continue;
        }

        PageSlot& slot = it->table->slots[it->slot];

        const bool still_released = slot.bound && slot.references == 0 && slot.release_serial == it->serial;
        if (still_released)
        {
            m_sparse_bind_scheduler.enqueue_buffer_unbind(
                SparseBufferUnbind{
                    .buffer = it->table->buffer,
                    .resource_offset = it->slot * page_size,
                    .size = page_size,
                });

            m_page_allocator.free_page(slot.page);
            slot.page = GpuPage{};
            slot.bound = false;
        }

        it = m_pending_page_releases.erase(it);
    }
}

auto MeshStreamingPool::get_entry(MeshAssetHandle handle) -> MeshEntry*
{
    const auto it = m_mesh_entry_map.find(handle);
    if (it == m_mesh_entry_map.end())
        return nullptr;

    return &it->second;
}

auto MeshStreamingPool::get_entry(MeshAssetHandle handle) const -> const MeshEntry*
{
    const auto it = m_mesh_entry_map.find(handle);
    if (it == m_mesh_entry_map.end())
        return nullptr;

    return &it->second;
}

} // namespace Mizu
