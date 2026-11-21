#pragma once

#include <format>
#include <mutex>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "asset/asset_handle.h"
#include "base/reflection/enum_traits.h"

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

    void report_completion(AssetType type);

    std::span<const std::string> get_infos() const { return m_infos; }
    std::span<const std::string> get_warnings() const { return m_warnings; }
    std::span<const std::string> get_errors() const { return m_errors; }

    bool has_errors() const { return !m_errors.empty(); }

    std::vector<std::pair<AssetType, uint32_t>> get_asset_completions() const;

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

    meta::enum_array<AssetType, uint32_t> m_asset_completions{};
};

} // namespace Mizu