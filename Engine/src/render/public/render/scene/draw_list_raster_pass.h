#pragma once

#include <optional>
#include <string_view>
#include <type_traits>

#include "shader/shader_asset.h"

#include "render/scene/draw_class.h"
#include "render/scene/draw_list_system_types.h"

namespace Mizu
{

struct ShaderInstance;

class DrawListRasterPass
{
  public:
    virtual ~DrawListRasterPass() = default;

    virtual std::optional<RasterShaders> select(const DrawClassDesc& draw_class) const = 0;
    virtual DrawFilter filter() const { return {}; }

    virtual std::string_view name() const { return "DrawList"; }
};

#define MIZU_IMPLEMENT_DRAW_LIST_RASTER_PASS(_name)                                                                   \
    _name* get_##_name()                                                                                              \
    {                                                                                                                 \
        static_assert(                                                                                                \
            std::is_base_of_v<DrawListRasterPass, _name>, "DrawListRasterPass must inherit from DrawListRasterPass"); \
                                                                                                                      \
        static _name raster_pass{};                                                                                   \
        return &raster_pass;                                                                                          \
    }

class FixedShaderRasterPass : public DrawListRasterPass
{
  public:
    FixedShaderRasterPass(const ShaderDeclaration& vertex, const ShaderDeclaration& fragment)
        : FixedShaderRasterPass(vertex.get_instance(), fragment.get_instance())
    {
    }

    FixedShaderRasterPass(ShaderInstance vertex, ShaderInstance fragment)
        : FixedShaderRasterPass(
              get_shader_declaration_asset_handle(vertex),
              get_shader_declaration_asset_handle(fragment))
    {
    }

    FixedShaderRasterPass(ShaderAssetHandle vertex, ShaderAssetHandle fragment)
        : m_shaders(RasterShaders{std::move(vertex), std::move(fragment)})
    {
    }

    std::optional<RasterShaders> select(const DrawClassDesc&) const override { return m_shaders; }

  private:
    RasterShaders m_shaders;
};

class MaterialShaderRasterPass : public DrawListRasterPass
{
  public:
    std::optional<RasterShaders> select(const DrawClassDesc& draw_class) const override
    {
        if (!draw_class.vertex.is_valid() || !draw_class.fragment.is_valid())
            return std::nullopt;

        return RasterShaders{draw_class.vertex, draw_class.fragment};
    }
};

} // namespace Mizu
