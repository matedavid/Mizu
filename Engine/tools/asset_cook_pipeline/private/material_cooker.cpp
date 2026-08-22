#include "material_cooker.h"

namespace Mizu
{

bool MaterialCooker::should_cook(const CookRequest& request, const TimestampDb& timestamp_db) const
{
    (void)request;
    (void)timestamp_db;

    return true;
}

void MaterialCooker::cook(const CookRequest& request, std::vector<SinkRequest>& outputs)
{
    (void)request;
    (void)outputs;
}

} // namespace Mizu
