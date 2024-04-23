#pragma once

#include <cassert>
#include <filesystem>
#include <fstream>
#include <vector>

namespace Mizu {

class Filesystem {
  public:
    [[nodiscard]] static std::vector<char> read_file(const std::filesystem::path& path);
};

} // namespace Mizu