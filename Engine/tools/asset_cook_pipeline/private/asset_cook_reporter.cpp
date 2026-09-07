#include "asset_cook_reporter.h"

#include "base/debug/logging.h"

namespace Mizu
{

static constexpr bool IMMEDIATE_LOG_INFO = true;
static constexpr bool IMMEDIATE_LOG_WARNING = true;
static constexpr bool IMMEDIATE_LOG_ERROR = true;

void AssetCookReporter::print_reports() const
{
#if MIZU_DEBUG
    for (const std::string& info : m_infos)
    {
        MIZU_LOG_INFO("{}", info);
    }

    for (const std::string& warning : m_warnings)
    {
        MIZU_LOG_WARNING("{}", warning);
    }

    for (const std::string& error : m_errors)
    {
        MIZU_LOG_ERROR("{}", error);
    }
#endif
}

void AssetCookReporter::report_info_internal(std::string info)
{
    if constexpr (IMMEDIATE_LOG_INFO)
    {
        MIZU_LOG_INFO(info);
    }

    std::lock_guard lock(m_info_mutex);
    m_infos.push_back(info);
}

void AssetCookReporter::report_warning_internal(std::string warning)
{
    if constexpr (IMMEDIATE_LOG_WARNING)
    {
        MIZU_LOG_WARNING(warning);
    }

    std::lock_guard lock(m_warning_mutex);
    m_warnings.push_back(warning);
}

void AssetCookReporter::report_error_internal(std::string error)
{
    if constexpr (IMMEDIATE_LOG_ERROR)
    {
        MIZU_LOG_ERROR(error);
    }

    std::lock_guard lock(m_error_mutex);
    m_errors.push_back(error);
}

} // namespace Mizu