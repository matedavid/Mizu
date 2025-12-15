#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "mizu_base_module.h"

namespace Mizu
{

namespace Filesystem
{

MIZU_BASE_API std::vector<char> read_file(const std::filesystem::path& path);
MIZU_BASE_API std::string read_file_string(const std::filesystem::path& path);

MIZU_BASE_API void write_file(const std::filesystem::path& path, const char* content, size_t size);
MIZU_BASE_API void write_file_string(const std::filesystem::path& path, std::string_view content);

}; // namespace Filesystem

} // namespace Mizu