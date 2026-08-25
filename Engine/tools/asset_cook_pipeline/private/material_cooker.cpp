#include "material_cooker.h"

namespace Mizu
{

bool MaterialCooker::should_cook(const CookRequest& request, const TimestampDb& timestamp_db) const
{
    (void)request;
    (void)timestamp_db;

    return true;
}

void MaterialCooker::cook(const CookRequest& request, const CookContext& context, std::vector<SinkRequest>& outputs)
{
    (void)request;
    (void)context;
    (void)outputs;
}

} // namespace Mizu
