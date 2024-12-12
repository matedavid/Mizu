#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "render_core/shader/shader_properties.h"

namespace Mizu
{

class IShader
{
  public:
    virtual ~IShader() = default;

    [[nodiscard]] virtual std::vector<ShaderProperty> get_properties() const = 0;
    [[nodiscard]] virtual std::optional<ShaderProperty> get_property(std::string_view name) const = 0;

    [[nodiscard]] virtual std::optional<ShaderConstant> get_constant(std::string_view name) const = 0;
};

struct ShaderStageInfo
{
    std::filesystem::path path;
    std::string entry_point;
};

class GraphicsShader : public virtual IShader
{
  public:
    [[nodiscard]] static std::shared_ptr<GraphicsShader> create(const ShaderStageInfo& vert_info,
                                                                const ShaderStageInfo& frag_info);
};

class ComputeShader : public virtual IShader
{
  public:
    [[nodiscard]] static std::shared_ptr<ComputeShader> create(const ShaderStageInfo& comp_info);
};

} // namespace Mizu