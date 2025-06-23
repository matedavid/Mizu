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

    const uint32_t size = static_cast<uint32_t>(file.tellg());
    std::vector<char> content(size);

    file.seekg(0);
    file.read(content.data(), size);

    file.close();

    return content;
}

} // namespace Mizu