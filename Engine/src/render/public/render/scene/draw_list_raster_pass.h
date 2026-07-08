#pragma once

#include <type_traits>

#include "render/scene/draw_list_system_types.h"

namespace Mizu
{

struct DrawItem;
struct ShaderInstance;

class DrawListRasterPass
{
  public:
    virtual ShaderInstance get_vertex_shader(const DrawItem& element) const = 0;
    virtual ShaderInstance get_fragment_shader(const DrawItem& element) const = 0;
    virtual size_t get_pipeline_hash(const DrawItem& element) const = 0;

    // TODO: This shouldn't exist, keeping for the moment to know what push constant to use
    virtual bool get_is_material_raster_pass() const = 0;
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
        : m_vertex_shader(std::move(vertex))
        , m_fragment_shader(std::move(fragment))
    {
        m_pipeline_hash = hash_compute(m_vertex_shader.get_hash(), m_fragment_shader.get_hash());
    }

    ShaderInstance get_vertex_shader(const DrawItem&) const override { return m_vertex_shader; }
    ShaderInstance get_fragment_shader(const DrawItem&) const override { return m_fragment_shader; }
    size_t get_pipeline_hash(const DrawItem&) const override { return m_pipeline_hash; }

    bool get_is_material_raster_pass() const override { return false; }

  private:
    ShaderInstance m_vertex_shader;
    ShaderInstance m_fragment_shader;
    size_t m_pipeline_hash;
};

class MaterialShaderRasterPass : public DrawListRasterPass
{
  public:
    ShaderInstance get_vertex_shader(const DrawItem& element) const override { return element.vertex_instance; }
    ShaderInstance get_fragment_shader(const DrawItem& element) const override { return element.fragment_instance; }
    size_t get_pipeline_hash(const DrawItem& element) const override { return element.pipeline_hash; }

    bool get_is_material_raster_pass() const override { return true; }
};

} // namespace Mizu
