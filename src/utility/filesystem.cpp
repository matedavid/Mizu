#include "filesystem.h"

#include "utility/logging.h"

namespace Mizu {

std::vector<char> Filesystem::read_file(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        MIZU_LOG_ERROR("File: '{}' does not exist", path.string());
        assert(false);
    }

    std::ifstream file(path, std::ios::ate | std::ios::binary);
    assert(file.is_open() && "Failed to open file");

    const auto size = static_cast<uint32_t>(file.tellg());
    std::vector<char> content(size);

    file.seekg(0);
    file.read(content.data(), size);

    file.close();

    return content;
}

} // namespace Mizu