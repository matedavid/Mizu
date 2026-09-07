#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <shared_mutex>
#include <unordered_map>

namespace Mizu
{

struct Timestamp
{
    uint64_t ts;
    uint64_t version;
};

class TimestampDb
{
  public:
    TimestampDb() = default;

    TimestampDb(const TimestampDb&) = delete;
    TimestampDb& operator=(const TimestampDb&) = delete;

    bool load(const std::filesystem::path& path);
    bool save(const std::filesystem::path& path) const;

    void record(size_t id, Timestamp ts);
    std::optional<Timestamp> get_timestamp(size_t id) const;

    bool is_different(Timestamp a, Timestamp b) const;
    bool is_different(size_t id, Timestamp ts) const;

    size_t finalize_run(uint32_t max_unused_runs);

  private:
    struct FileEntry
    {
        uint64_t id;
        uint64_t ts;
        uint64_t version;
        uint32_t non_touched_runs;
    };

    struct Record
    {
        Timestamp timestamp{};
        mutable std::atomic<bool> touched_this_run{false};
        uint32_t non_touched_runs = 0;
    };

    struct Shard
    {
        std::unordered_map<size_t, Record> records;
        mutable std::shared_mutex mutex;
    };

    static constexpr size_t NUM_SHARDS = 64;
    std::array<Shard, NUM_SHARDS> m_shards{};

    static size_t shard_index(size_t id) { return id & (NUM_SHARDS - 1); }
};

bool timestamp_should_import(
    size_t id,
    const std::filesystem::path& path,
    uint32_t version,
    const TimestampDb& timestamp_db);

void timestamp_record(size_t id, const std::filesystem::path& path, uint32_t version, TimestampDb& timestamp_db);

} // namespace Mizu