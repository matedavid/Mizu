#pragma once

#include <filesystem>
#include <optional>
#include <variant>
#include <vector>

namespace Mizu {

struct ShaderValueProperty {
    enum class Type {
        Float,
        Vec2,
        Vec3,
        Vec4,
        Mat3,
        Mat4,
        Custom,
    };

    Type type;
    uint32_t size;
    std::string name;
};

struct ShaderTextureProperty {
    std::string name;
};

struct ShaderUniformBufferProperty {
    std::string name;
    uint32_t total_size;
    std::vector<ShaderValueProperty> members;
};

using ShaderProperty = std::variant<ShaderValueProperty, ShaderTextureProperty, ShaderUniformBufferProperty>;

struct ShaderConstant {
    std::string name;
    uint32_t size;
};

class Shader {
  public:
    virtual ~Shader() = default;

    [[nodiscard]] static std::shared_ptr<Shader> create(const std::filesystem::path& vertex_path,
                                                        const std::filesystem::path& fragment_path);

    [[nodiscard]] virtual std::vector<ShaderProperty> get_properties() const = 0;
    [[nodiscard]] virtual std::optional<ShaderProperty> get_property(std::string_view name) const = 0;

    [[nodiscard]] virtual std::optional<ShaderConstant> get_constant(std::string_view name) const = 0;
};

class ComputeShader {
  public:
    virtual ~ComputeShader() = default;

    [[nodiscard]] static std::shared_ptr<ComputeShader> create(const std::filesystem::path& path);

    [[nodiscard]] virtual std::vector<ShaderProperty> get_properties() const = 0;
    [[nodiscard]] virtual std::optional<ShaderProperty> get_property(std::string_view name) const = 0;

    [[nodiscard]] virtual std::optional<ShaderConstant> get_constant(std::string_view name) const = 0;
};

} // namespace Mizu