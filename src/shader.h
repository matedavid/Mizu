#pragma once

#include <filesystem>
#include <optional>
#include <vector>

namespace Mizu {

struct ShaderProperty {
    enum class Type {
        Float,
        Vec2,
        Vec3,
        Vec4,
        Mat3,
        Mat4,
        Texture,
        Custom,
    };

    Type type;
    uint32_t size;
    std::string name;
};

class Shader {
  public:
    virtual ~Shader() = default;

    [[nodiscard]] static std::shared_ptr<Shader> create(const std::filesystem::path& vertex_path,
                                                        const std::filesystem::path& fragment_path);

    [[nodiscard]] virtual std::vector<ShaderProperty> get_properties() const = 0;
};

class ComputeShader {
  public:
    virtual ~ComputeShader() = default;

    [[nodiscard]] static std::shared_ptr<ComputeShader> create(const std::filesystem::path& path);

    [[nodiscard]] virtual std::vector<ShaderProperty> get_properties() const = 0;
};

} // namespace Mizu