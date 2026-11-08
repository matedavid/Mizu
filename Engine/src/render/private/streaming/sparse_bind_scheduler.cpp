#include "streaming/sparse_bind_scheduler.h"

#include "render/runtime/renderer.h"

namespace Mizu
{

SparseBindScheduler::SparseBindScheduler(CommandBufferType type) : m_queue_type(type) {}

void SparseBindScheduler::enqueue_buffer_bind(const SparseBufferBind& bind)
{
    m_current_batch.add_buffer_bind(bind);
}

void SparseBindScheduler::enqueue_buffer_unbind(const SparseBufferUnbind& unbind)
{
    m_current_batch.add_buffer_unbind(unbind);
}

void SparseBindScheduler::enqueue_image_bind(const SparseImageBind& bind)
{
    m_current_batch.add_image_bind(bind);
}

void SparseBindScheduler::enqueue_image_unbind(const SparseImageUnbind& unbind)
{
    m_current_batch.add_image_unbind(unbind);
}

void SparseBindScheduler::enqueue_mip_tail_bind(const SparseMipTailBind& bind)
{
    m_current_batch.add_mip_tail_bind(bind);
}

void SparseBindScheduler::add_mip_tail_unbind(const SparseMipTailUnbind& unbind)
{
    m_current_batch.add_mip_tail_unbind(unbind);
}

void SparseBindScheduler::add_metadata_bind(const SparseMetadataBind& bind)
{
    m_current_batch.add_metadata_bind(bind);
}

void SparseBindScheduler::add_metadata_unbind(const SparseMetadataUnbind& unbind)
{
    m_current_batch.add_metadata_unbind(unbind);
}

SyncPoint SparseBindScheduler::flush(const SubmitInfo& submit_info)
{
    if (m_current_batch.is_empty())
        return SyncPoint{};

    Queue& queue = get_queue();

    const SyncPoint sync = queue.bind_sparse(m_current_batch, submit_info);
    m_current_batch = SparseBindBatch{};

    return sync;
}

Queue& SparseBindScheduler::get_queue() const
{
    return g_render_device->get_queue(m_queue_type);
}

} // namespace Mizu
