#include "base/io/filesystem.h"

#include <fstream>
#include <limits>

#include "base/debug/assert.h"

namespace Mizu
{

std::vector<char> Filesystem::read_file(const std::filesystem::path& path)
{
    MIZU_VERIFY(std::filesystem::exists(path), "File: '{}' does not exist", path.string());

    std::ifstream file(path, std::ios::ate | std::ios::binary);
    MIZU_ASSERT(file.is_open(), "Failed to open file with path: {}", path.string());

    const std::streamoff end_pos = file.tellg();
    MIZU_ASSERT(end_pos >= 0, "Failed to get file size for: {}", path.string());
    MIZU_ASSERT(end_pos <= std::numeric_limits<std::streamoff>::max(), "File too large: {}", path.string());

    const size_t size = static_cast<size_t>(end_pos);
    std::vector<char> content(size);

    file.seekg(0);
    MIZU_ASSERT(
        size <= static_cast<size_t>(std::numeric_limits<std::streamsize>::max()),
        "File too large to read into streamsize: {}",
        path.string());
    file.read(content.data(), static_cast<std::streamsize>(size));

    file.close();

    return content;
}

std::string Filesystem::read_file_string(const std::filesystem::path& path)
{
    MIZU_VERIFY(std::filesystem::exists(path), "File: '{}' does not exist", path.string());

    std::ifstream file(path, std::ios::ate | std::ios::binary);
    MIZU_ASSERT(file.is_open(), "Failed to open file with path: {}", path.string());

    const std::streamoff end_pos = file.tellg();
    MIZU_ASSERT(end_pos >= 0, "Failed to get file size for: {}", path.string());
    MIZU_ASSERT(end_pos <= std::numeric_limits<std::streamoff>::max(), "File too large: {}", path.string());

    const size_t size = static_cast<size_t>(end_pos);

    std::string content;
    content.resize(size);

    file.seekg(0);
    MIZU_ASSERT(
        size <= static_cast<size_t>(std::numeric_limits<std::streamsize>::max()),
        "File too large to read into streamsize: {}",
        path.string());
    file.read(content.data(), static_cast<std::streamsize>(size));

    file.close();

    return content;
}

void Filesystem::write_file(const std::filesystem::path& path, const char* content, size_t size)
{
    std::ofstream file(path, std::ios::ate | std::ios::binary);
    MIZU_ASSERT(file.is_open(), "Failed to create/open file");

    MIZU_ASSERT(
        size <= static_cast<size_t>(std::numeric_limits<std::streamsize>::max()),
        "File too large to write into streamsize: {}",
        path.string());
    file.write(content, static_cast<std::streamsize>(size));
}

void Filesystem::write_file_string(const std::filesystem::path& path, std::string_view content)
{
    write_file(path, content.data(), content.size());
}

} // namespace Mizu