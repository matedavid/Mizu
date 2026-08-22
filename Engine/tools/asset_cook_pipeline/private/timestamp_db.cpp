#include "timestamp_db.h"

namespace Mizu
{

void TimestampDb::record(size_t id, Timestamp ts)
{
    (void)id;
    (void)ts;
}

std::optional<Timestamp> TimestampDb::get_timestamp(size_t id) const
{
    (void)id;
    return std::optional<Timestamp>();
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

} // namespace Mizu
