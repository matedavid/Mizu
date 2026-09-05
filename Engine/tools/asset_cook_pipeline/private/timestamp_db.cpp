#include "timestamp_db.h"

#include <fstream>
#include <vector>

#include "base/debug/logging.h"

namespace Mizu
{

void TimestampDb::record(size_t id, Timestamp ts)
{
    Shard& shard = m_shards[shard_index(id)];
    std::unique_lock lock(shard.mutex);

    Record& record = shard.records[id];
    record.timestamp = ts;
    record.touched_this_run.store(true, std::memory_order_relaxed);
}

std::optional<Timestamp> TimestampDb::get_timestamp(size_t id) const
{
    const Shard& shard = m_shards[shard_index(id)];
    std::shared_lock lock(shard.mutex);

    const auto it = shard.records.find(id);
    if (it == shard.records.end())
        return std::nullopt;

    it->second.touched_this_run.store(true, std::memory_order_relaxed);
    return it->second.timestamp;
}

bool TimestampDb::is_different(Timestamp left, Timestamp right) const
{
    return left.version != right.version || left.ts != right.ts;
}

bool TimestampDb::is_different(size_t id, Timestamp ts) const
{
    const std::optional<Timestamp> record = get_timestamp(id);
    if (!record.has_value())
        return true;

    return is_different(*record, ts);
}

size_t TimestampDb::finalize_run(uint32_t max_unused_runs)
{
    size_t removed = 0;

    for (Shard& shard : m_shards)
    {
        std::unique_lock lock(shard.mutex);

        for (auto it = shard.records.begin(); it != shard.records.end();)
        {
            Record& record = it->second;

            if (record.touched_this_run.exchange(false, std::memory_order_relaxed))
            {
                record.non_touched_runs = 0;
                ++it;
            }
            else
            {
                record.non_touched_runs += 1;
                if (record.non_touched_runs > max_unused_runs)
                {
                    it = shard.records.erase(it);
                    ++removed;
                }
                else
                {
                    ++it;
                }
            }
        }
    }

    return removed;
}

bool TimestampDb::load(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open())
    {
        MIZU_LOG_INFO("TimestampDb: no existing db at {}, starting cold", path.string());
        return true;
    }

    uint64_t entry_count = 0;
    file.read(reinterpret_cast<char*>(&entry_count), sizeof(entry_count));

    if (!file)
    {
        MIZU_LOG_WARNING("TimestampDb: {} is truncated, starting cold", path.string());
        return true;
    }

    std::vector<FileEntry> entries(entry_count);
    file.read(reinterpret_cast<char*>(entries.data()), static_cast<std::streamsize>(entry_count * sizeof(FileEntry)));

    if (!file)
    {
        MIZU_LOG_WARNING("TimestampDb: {} is truncated, starting cold", path.string());
        return true;
    }

    for (const FileEntry& entry : entries)
    {
        Shard& shard = m_shards[shard_index(entry.id)];
        Record& record = shard.records[entry.id];
        record.timestamp = Timestamp{.ts = entry.ts, .version = entry.version};
        record.non_touched_runs = entry.non_touched_runs;
        // touched_this_run stays false until this run's cook actually looks it up
    }

    return true;
}

bool TimestampDb::save(const std::filesystem::path& path) const
{
    std::vector<FileEntry> entries;

    for (const Shard& shard : m_shards)
    {
        std::shared_lock lock(shard.mutex);
        entries.reserve(entries.size() + shard.records.size());

        for (const auto& [id, record] : shard.records)
        {
            entries.push_back(
                FileEntry{
                    .id = id,
                    .ts = record.timestamp.ts,
                    .version = record.timestamp.version,
                    .non_touched_runs = record.non_touched_runs,
                });
        }
    }

    const std::filesystem::path tmp_path = path.string() + ".tmp";

    {
        std::ofstream file(tmp_path, std::ios::binary | std::ios::trunc);
        if (!file.is_open())
        {
            MIZU_LOG_ERROR("TimestampDb: failed to open {} for writing", tmp_path.string());
            return false;
        }

        const uint64_t entry_count = entries.size();

        file.write(reinterpret_cast<const char*>(&entry_count), sizeof(entry_count));
        file.write(
            reinterpret_cast<const char*>(entries.data()),
            static_cast<std::streamsize>(entries.size() * sizeof(FileEntry)));
    }

    std::error_code ec;
    std::filesystem::rename(tmp_path, path, ec);
    if (ec)
    {
        MIZU_LOG_ERROR("TimestampDb: failed to finalize {}: {}", path.string(), ec.message());
        return false;
    }

    return true;
}

} // namespace Mizu