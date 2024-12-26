#pragma once

/*

#include <variant>

#include "render_core/shader/shader_declaration.h"

namespace Mizu
{

using MaterialParameterT = std::variant<std::shared_ptr<Texture2D>>;

struct MaterialParameterInfo
{
    std::string param_name;
    ShaderDeclarationMemberType param_type;
    MaterialParameterT value;
};

using material_members_vec_t = std::vector<MaterialParameterInfo>;

#define BEGIN_MATERIAL_PARAMETERS()                                                                       \
    class MaterialParameters                                                                              \
    {                                                                                                     \
      public:                                                                                             \
        MaterialParameters() {}                                                                           \
                                                                                                          \
      private:                                                                                            \
        typedef void* _func;                                                                              \
        struct _first_member_id                                                                           \
        {                                                                                                 \
        };                                                                                                \
        typedef _func (*_member_func)(_first_member_id,                                                   \
                                      Mizu::material_members_vec_t&,                                      \
                                      const MaterialParameters& params);                                  \
        static _func _append_member_get_prev_func(_first_member_id,                                       \
                                                  [[maybe_unused]] Mizu::material_members_vec_t& members, \
                                                  [[maybe_unused]] const MaterialParameters& params)      \
        {                                                                                                 \
            return nullptr;                                                                               \
        }                                                                                                 \
        typedef _first_member_id

#define END_MATERIAL_PARAMETERS()                                                                      \
    _last_member_id;                                                                                   \
                                                                                                       \
  public:                                                                                              \
    static Mizu::material_members_vec_t get_members(const MaterialParameters& params = {})             \
    {                                                                                                  \
        Mizu::material_members_vec_t members;                                                          \
        _func (*last_func)(_last_member_id, Mizu::material_members_vec_t&, const MaterialParameters&); \
        last_func = _append_member_get_prev_func;                                                      \
        _func ptr = (_func)last_func;                                                                  \
        do                                                                                             \
        {                                                                                              \
            ptr = reinterpret_cast<_member_func>(ptr)(_first_member_id{}, members, params);            \
        } while (ptr);                                                                                 \
        return members;                                                                                \
    }                                                                                                  \
    }                                                                                                  \
    ;

#define MATERIAL_PARAMETER_IMPL(name, type, type_enum)                                                               \
    _member_##name;                                                                                                  \
                                                                                                                     \
  public:                                                                                                            \
    type name = nullptr;                                                                                             \
                                                                                                                     \
  private:                                                                                                           \
    struct _next_member_##name                                                                                       \
    {                                                                                                                \
    };                                                                                                               \
    static _func _append_member_get_prev_func(                                                                       \
        _next_member_##name, Mizu::material_members_vec_t& members, const MaterialParameters& params)                \
    {                                                                                                                \
        auto info = Mizu::MaterialParameterInfo{.param_name = #name, .param_type = type_enum, .value = params.name}; \
        members.push_back(info);                                                                                     \
        _func (*prev_func)(_member_##name, Mizu::material_members_vec_t&, const MaterialParameters&);                \
        prev_func = _append_member_get_prev_func;                                                                    \
        return (void*)prev_func;                                                                                     \
    }                                                                                                                \
    typedef _next_member_##name

#define MATERIAL_PARAMETER_TEXTURE2D(name) \
    MATERIAL_PARAMETER_IMPL(name, std::shared_ptr<Mizu::Texture2D>, Mizu::ShaderDeclarationMemberType::RGTexture2D)

template <typename T = BaseShaderDeclaration>
class MaterialShader : public ShaderDeclaration<T>
{
  public:
    using Parameters = T::Parameters;
    using Parent = T;
};

} // namespace Mizu

*/