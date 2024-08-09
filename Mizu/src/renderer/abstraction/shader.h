#pragma once

#include <filesystem>
#include <optional>
#include <variant>
#include <vector>

#include "renderer/abstraction/shader/shader_properties.h"

namespace Mizu {

class GraphicsShader {
  public:
    virtual ~GraphicsShader() = default;

    [[nodiscard]] static std::shared_ptr<GraphicsShader> create(const std::filesystem::path& vertex_path,
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