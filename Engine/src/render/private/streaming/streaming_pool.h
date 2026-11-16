#pragma once

#include "base/debug/assert.h"

namespace Mizu
{

enum class ResourceBacking
{
    Placed,
    Sparse,
};

template <typename Pool, typename Handle>
class PendingResidencyUpdate
{
  public:
    ~PendingResidencyUpdate()
    {
        if (m_pool == nullptr)
            return;

        MIZU_UNREACHABLE("PendingResidencyUpdate for asset {} was neither committed nor aborted", m_handle.get_id());
        abort();
    }

    PendingResidencyUpdate(const PendingResidencyUpdate&) = delete;
    PendingResidencyUpdate& operator=(const PendingResidencyUpdate&) = delete;
    PendingResidencyUpdate(PendingResidencyUpdate&&) = delete;
    PendingResidencyUpdate& operator=(PendingResidencyUpdate&&) = delete;

    void commit()
    {
        MIZU_ASSERT(m_pool != nullptr, "Committing a residency update that is not pending");
        m_pool->commit_residency_update(m_handle);
        m_pool = nullptr;
    }

    void abort()
    {
        MIZU_ASSERT(m_pool != nullptr, "Aborting a residency update that is not pending");
        m_pool->abort_residency_update(m_handle);
        m_pool = nullptr;
    }

    bool is_pending() const { return m_pool != nullptr; }
    explicit operator bool() const { return is_pending(); }

  private:
    PendingResidencyUpdate() = default;
    PendingResidencyUpdate(Pool* pool, Handle handle) : m_pool(pool), m_handle(handle) {}

    Pool* m_pool = nullptr;
    Handle m_handle{};

    friend Pool;
};

} // namespace Mizu
