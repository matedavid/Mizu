#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace Mizu
{

namespace Filesystem
{

std::vector<char> read_file(const std::filesystem::path& path);
std::string read_file_string(const std::filesystem::path& path);

void write_file_string(const std::filesystem::path& path, std::string_view content);

}; // namespace Filesystem

} // namespace Mizu