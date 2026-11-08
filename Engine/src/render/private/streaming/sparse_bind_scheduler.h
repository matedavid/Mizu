#pragma once

#include "render_core/rhi/queue.h"

namespace Mizu
{

class SparseBindScheduler
{
  public:
    SparseBindScheduler(CommandBufferType type);

    void enqueue_buffer_bind(const SparseBufferBind& bind);
    void enqueue_buffer_unbind(const SparseBufferUnbind& unbind);

    void enqueue_image_bind(const SparseImageBind& bind);
    void enqueue_image_unbind(const SparseImageUnbind& unbind);

    void enqueue_mip_tail_bind(const SparseMipTailBind& bind);
    void add_mip_tail_unbind(const SparseMipTailUnbind& unbind);

    void add_metadata_bind(const SparseMetadataBind& bind);
    void add_metadata_unbind(const SparseMetadataUnbind& unbind);

    SyncPoint flush(const SubmitInfo& submit_info);

  private:
    CommandBufferType m_queue_type{};
    SparseBindBatch m_current_batch{};

    Queue& get_queue() const;
};

} // namespace Mizu