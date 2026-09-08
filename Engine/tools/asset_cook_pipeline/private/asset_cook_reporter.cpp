#include "asset_cook_reporter.h"

#include "base/debug/logging.h"

namespace Mizu
{

void AssetCookReporter::set_settings(AssetCookReporterSettings settings)
{
    m_settings = settings;
}

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