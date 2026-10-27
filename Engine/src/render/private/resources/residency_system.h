#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "asset/asset_handle.h"

namespace Mizu
{

enum class ResidencyStatus
{
    Unloaded,
    Loading,
    GpuResident,
    Evicting,
};

template <AssetHandleType AssetHandleT, typename RecordPayload>
class ResidencySystemBase
{
  public:
    virtual ~ResidencySystemBase() = default;

    bool increment_reference_count(AssetHandleT handle)
    {
        Record* record = get_or_create_record(handle);
        if (record == nullptr)
            return false;

        record->references.fetch_add(1, std::memory_order_acq_rel);

        return true;
    }

    bool decrement_reference_count(AssetHandleT handle)
    {
        Record* record = get_record(handle);
        if (record == nullptr)
            return false;

        // Can't decrement a record with 0 references
        if (record->references.load(std::memory_order_acquire) == 0)
            return false;

        record->references.fetch_sub(1, std::memory_order_acq_rel);

        return true;
    }

    bool transition_status(AssetHandleT handle, ResidencyStatus expected, ResidencyStatus desired)
    {
        Record* record = get_record(handle);
        if (record == nullptr)
            return false;

        ResidencyStatus current = expected;
        while (!record->status.compare_exchange_weak(current, desired, std::memory_order_acq_rel))
        {
            if (current != expected)
                return false;

            current = expected;
        }

        return true;
    }

    ResidencyStatus get_status(AssetHandleT handle) const
    {
        const Record* record = get_record(handle);
        if (record == nullptr)
            return ResidencyStatus::Unloaded;

        return record->status.load(std::memory_order_acquire);
    }

    uint32_t get_references(AssetHandleT handle) const
    {
        const Record* record = get_record(handle);
        if (record == nullptr)
            return 0;

        return record->references.load(std::memory_order_acquire);
    }

    RecordPayload* get_payload(AssetHandleT handle)
    {
        Record* record = get_record(handle);
        if (record == nullptr)
            return nullptr;

        return &record->payload;
    }

    const RecordPayload* get_payload(AssetHandleT handle) const
    {
        const Record* record = get_record(handle);
        if (record == nullptr)
            return nullptr;

        return &record->payload;
    }

  protected:
    struct Record
    {
        AssetHandleT handle{};
        std::atomic<ResidencyStatus> status{ResidencyStatus::Unloaded};
        std::atomic<uint32_t> references{0};
        std::atomic<uint64_t> eviction_requested_frame{0};

        RecordPayload payload{};
    };

    static constexpr size_t NUM_SHARDS = 16;

    struct Shard
    {
        std::unordered_map<size_t, Record> records;
        mutable std::mutex mutex;
    };

    std::array<Shard, NUM_SHARDS> m_shards{};

    Record* get_record(AssetHandleT handle)
    {
        Shard& shard = get_shard(handle);

        std::lock_guard lock{shard.mutex};

        auto it = shard.records.find(handle.get_id());
        if (it == shard.records.end())
            return nullptr;

        return &it->second;
    }

    const Record* get_record(AssetHandleT handle) const
    {
        const Shard& shard = get_shard(handle);

        std::lock_guard lock{shard.mutex};

        const auto it = shard.records.find(handle.get_id());
        if (it == shard.records.end())
            return nullptr;

        return &it->second;
    }

    Record* get_or_create_record(AssetHandleT handle)
    {
        Shard& shard = get_shard(handle);

        std::lock_guard lock{shard.mutex};

        const auto it = shard.records.find(handle.get_id());
        if (it != shard.records.end())
            return &it->second;

        auto [insert_it, inserted] = shard.records.try_emplace(handle.get_id());
        if (!inserted)
            return nullptr;

        Record& record = insert_it->second;
        record.handle = handle;
        record.status.store(ResidencyStatus::Unloaded, std::memory_order_release);
        record.references.store(0, std::memory_order_release);
        record.eviction_requested_frame.store(0, std::memory_order_release);

        return &record;
    }

    void remove_record(AssetHandleT handle)
    {
        Shard& shard = get_shard(handle);

        std::lock_guard lock{shard.mutex};

        const auto it = shard.records.find(handle.get_id());
        if (it == shard.records.end())
            return;

        shard.records.erase(it);
    }

    Shard& get_shard(AssetHandleT handle)
    {
        const size_t shard_idx = handle.get_id() % NUM_SHARDS;
        return m_shards[shard_idx];
    }

    const Shard& get_shard(AssetHandleT handle) const
    {
        const size_t shard_idx = handle.get_id() % NUM_SHARDS;
        return m_shards[shard_idx];
    }
};

} // namespace Mizu
