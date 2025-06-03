#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "render_core/shader/shader_properties.h"

namespace Mizu
{

enum class ShaderType
{
    Vertex,
    Fragment,
    Compute,
};

class Shader
{
  public:
    struct Description
    {
        std::filesystem::path path;
        std::string entry_point;
        ShaderType type;
    };

    virtual ~Shader() = default;

    static std::shared_ptr<Shader> create(const Description& desc);

    virtual std::string get_entry_point() const = 0;
    virtual ShaderType get_type() const = 0;

    virtual const std::vector<ShaderProperty>& get_properties() const = 0;
    virtual const std::vector<ShaderConstant>& get_constants() const = 0;
    virtual const std::vector<ShaderInput>& get_inputs() const = 0;
    virtual const std::vector<ShaderOutput>& get_outputs() const = 0;
};

class IShader
{
  public:
    virtual ~IShader() = default;

    [[nodiscard]] virtual std::vector<ShaderProperty> get_properties() const = 0;
    [[nodiscard]] virtual std::optional<ShaderProperty> get_property(std::string_view name) const = 0;

    [[nodiscard]] virtual std::vector<ShaderConstant> get_constants() const = 0;
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

    [[nodiscard]] virtual std::vector<ShaderOutput> get_outputs() const = 0;
};

class ComputeShader : public virtual IShader
{
  public:
    [[nodiscard]] static std::shared_ptr<ComputeShader> create(const ShaderStageInfo& comp_info);
};

} // namespace Mizu