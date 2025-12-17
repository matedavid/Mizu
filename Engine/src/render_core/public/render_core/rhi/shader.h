#pragma once

#include <filesystem>
#include <string>

#include "base/utils/enum_utils.h"

#include "mizu_render_core_module.h"

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

struct ShaderDescription
{
    std::filesystem::path path;
    std::string entry_point;
    ShaderType type;
};

class MIZU_RENDER_CORE_API Shader
{
  public:
    virtual ~Shader() = default;

    static std::shared_ptr<Shader> create(const ShaderDescription& desc);

    virtual const std::string& get_entry_point() const = 0;
    virtual ShaderType get_type() const = 0;
};

} // namespace Mizu