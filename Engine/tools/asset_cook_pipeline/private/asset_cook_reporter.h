#pragma once

#include <format>
#include <mutex>
#include <string>
#include <vector>

namespace Mizu
{

class AssetCookReporter
{
  public:
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