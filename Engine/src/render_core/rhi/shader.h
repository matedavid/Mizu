#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "base/utils/enum_utils.h"

#include "render_core/shader/shader_properties.h"

namespace Mizu
{

// Forward declarations
class ShaderReflection;
class SlangReflection;

using ShaderTypeBitsType = uint8_t;

// clang-format off
enum class ShaderType : ShaderTypeBitsType
{
    Vertex   = (1 << 0),
    Fragment = (1 << 1),
    Compute  = (1 << 2),

    RtxRaygen        = (1 << 3),
    RtxAnyHit        = (1 << 4),
    RtxClosestHit    = (1 << 5),
    RtxMiss          = (1 << 6),
    RtxIntersection  = (1 << 7),
};
// clang-format on

IMPLEMENT_ENUM_FLAGS_FUNCTIONS(ShaderType, ShaderTypeBitsType)

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

    virtual const std::string& get_entry_point() const = 0;
    virtual ShaderType get_type() const = 0;

    virtual const ShaderReflection& get_reflection() const = 0;
    virtual const SlangReflection& get_reflection2() const = 0;

};

} // namespace Mizu