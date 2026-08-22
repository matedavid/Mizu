#pragma once

#include <cstdint>
#include <optional>

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
    void record(size_t id, Timestamp ts);
    std::optional<Timestamp> get_timestamp(size_t id) const;

    bool is_different(Timestamp a, Timestamp b) const;
    bool is_different(size_t id, Timestamp ts) const;
};

} // namespace Mizu