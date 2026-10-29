#include "asset_cook_reporter.h"

#include "base/debug/logging.h"

namespace Mizu
{

void AssetCookReporter::set_settings(AssetCookReporterSettings settings)
{
    m_settings = settings;
}

void AssetCookReporter::report_completion(AssetType type)
{
    m_asset_completions[type] += 1;
}

std::vector<std::pair<AssetType, uint32_t>> AssetCookReporter::get_asset_completions() const
{
    std::vector<std::pair<AssetType, uint32_t>> results{};

    for (uint32_t i = 0; i < meta::enum_count_v<AssetType>; ++i)
    {
        const AssetType type = static_cast<AssetType>(i);
        const uint32_t completions = m_asset_completions[type];

        if (completions == 0)
            continue;

        results.emplace_back(type, completions);
    }

    return results;
}

void AssetCookReporter::report_info_internal(std::string info)
{
    if (m_settings.log_info)
    {
        MIZU_LOG_INFO(info);
    }

    std::lock_guard lock(m_info_mutex);
    m_infos.push_back(std::move(info));
}

void AssetCookReporter::report_warning_internal(std::string warning)
{
    if (m_settings.log_warning)
    {
        MIZU_LOG_WARNING(warning);
    }

    std::lock_guard lock(m_warning_mutex);
    m_warnings.push_back(std::move(warning));
}

void AssetCookReporter::report_error_internal(std::string error)
{
    if (m_settings.log_error)
    {
        MIZU_LOG_ERROR(error);
    }

    std::lock_guard lock(m_error_mutex);
    m_errors.push_back(std::move(error));
}

} // namespace Mizu