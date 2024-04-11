#pragma once

#include <filesystem>

namespace Mizu {

class Shader {
  public:
    virtual ~Shader() = default;

    [[nodiscard]] static std::shared_ptr<Shader> create(const std::filesystem::path& vertex_path,
                                                        const std::filesystem::path& fragment_path);
};

class ComputeShader {
  public:
    virtual ~ComputeShader() = default;

    [[nodiscard]] static std::shared_ptr<ComputeShader> create(const std::filesystem::path& path);
};

} // namespace Mizu