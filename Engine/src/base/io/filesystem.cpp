#include "filesystem.h"

#include <fstream>

#include "base/debug/assert.h"

namespace Mizu
{

std::vector<char> Filesystem::read_file(const std::filesystem::path& path)
{
    MIZU_VERIFY(std::filesystem::exists(path), "File: '{}' does not exist", path.string());

    std::ifstream file(path, std::ios::ate | std::ios::binary);
    MIZU_ASSERT(file.is_open(), "Failed to open file");

    const size_t size = file.tellg();
    std::vector<char> content(size);

    file.seekg(0);
    file.read(content.data(), size);

    file.close();

    return content;
}

std::string Filesystem::read_file_string(const std::filesystem::path& path)
{
    MIZU_VERIFY(std::filesystem::exists(path), "File: '{}' does not exist", path.string());

    std::ifstream file(path, std::ios::ate | std::ios::binary);
    MIZU_ASSERT(file.is_open(), "Failed to open file");

    const size_t size = file.tellg();

    std::string content;
    content.resize(size);

    file.seekg(0);
    file.read(content.data(), size);

    file.close();

    return content;
}

void Filesystem::write_file_string(const std::filesystem::path& path, std::string_view content)
{
    std::ofstream file(path, std::ios::ate | std::ios::binary);
    MIZU_ASSERT(file.is_open(), "Failed to create/open file");

    file.write(content.data(), content.size());
}

} // namespace Mizu