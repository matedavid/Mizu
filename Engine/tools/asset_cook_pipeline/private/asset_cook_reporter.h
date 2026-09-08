#pragma once

#include <format>
#include <mutex>
#include <string>
#include <vector>

namespace Mizu
{

struct AssetCookReporterSettings
{
    bool log_info = true;
    bool log_warning = true;
    bool log_error = true;
};

class AssetCookReporter
{
  public:
    void set_settings(AssetCookReporterSettings settings);

    template <typename... Args>
    void info(std::format_string<Args...> message, Args&&... args)
    {
        report_info_internal(std::format(message, std::forward<Args>(args)...));
    }

    template <typename... Args>
    void warning(std::format_string<Args...> message, Args&&... args)
    {
        report_warning_internal(std::format(message, std::forward<Args>(args)...));
    }

    template <typename... Args>
    void error(std::format_string<Args...> message, Args&&... args)
    {
        report_error_internal(std::format(message, std::forward<Args>(args)...));
    }

    void print_reports() const;

  private:
    void report_info_internal(std::string info);
    void report_warning_internal(std::string warning);
    void report_error_internal(std::string error);

    AssetCookReporterSettings m_settings{};

    enum class ReportType
    {
        Info,
        Warning,
        Error,
    };

    std::vector<std::string> m_infos{};
    std::vector<std::string> m_warnings{};
    std::vector<std::string> m_errors{};

    std::mutex m_info_mutex{};
    std::mutex m_warning_mutex{};
    std::mutex m_error_mutex{};
};

} // namespace Mizu