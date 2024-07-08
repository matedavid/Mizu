#pragma once

#include <functional>
#include <memory>
#include <variant>

#include "renderer/render_graph/render_graph_types.h"

namespace Mizu {

using Shader2MemberT = std::variant<RGTextureRef>;

struct Shader2MemberInfo {
    std::string mem_name;
    Shader2MemberT value;
};

using members_vec_t = std::vector<Shader2MemberInfo>;

#define BEGIN_SHADER_PARAMETERS()                                                                        \
    class Parameters : public Parent::Parameters {                                                       \
      private:                                                                                           \
        typedef void* _func;                                                                             \
        struct _first_member_id {};                                                                      \
        typedef _func (*_member_func)(_first_member_id, Mizu::members_vec_t&, const Parameters& params); \
        static _func _append_member_get_prev_func(_first_member_id,                                      \
                                                  [[maybe_unused]] Mizu::members_vec_t& members,         \
                                                  [[maybe_unused]] const Parameters& params) {           \
            return nullptr;                                                                              \
        }                                                                                                \
        typedef _first_member_id

#define SHADER_PARAMETER_RG_TEXTURE2D(name)                                            \
    _member_##name;                                                                    \
                                                                                       \
  public:                                                                              \
    Mizu::RGTextureRef name;                                                           \
                                                                                       \
  private:                                                                             \
    struct _next_member_##name {};                                                     \
    static _func _append_member_get_prev_func(                                         \
        _next_member_##name, Mizu::members_vec_t& members, const Parameters& params) { \
        auto info = Mizu::Shader2MemberInfo{.mem_name = #name, .value = params.name};  \
        members.push_back(info);                                                       \
        _func (*prev_func)(_member_##name, Mizu::members_vec_t&, const Parameters&);   \
        prev_func = _append_member_get_prev_func;                                      \
        return (void*)prev_func;                                                       \
    }                                                                                  \
    typedef _next_member_##name

#define END_SHADER_PARAMETERS()                                                             \
    _last_member_id;                                                                        \
                                                                                            \
  public:                                                                                   \
    static Mizu::members_vec_t get_members(const Parameters& params = {}) {                 \
        Mizu::members_vec_t members = Parent::Parameters::get_members(params);              \
        _func (*last_func)(_last_member_id, Mizu::members_vec_t&, const Parameters&);       \
        last_func = _append_member_get_prev_func;                                           \
        _func ptr = (_func)last_func;                                                       \
        do {                                                                                \
            ptr = reinterpret_cast<_member_func>(ptr)(_first_member_id{}, members, params); \
        } while (ptr);                                                                      \
        return members;                                                                     \
    }                                                                                       \
    }                                                                                       \
    ;

class BaseShader final {
  public:
    struct Parameters {
        static Mizu::members_vec_t get_members([[maybe_unused]] const Parameters& params = {}) { return {}; }
    };
};

template <typename T = BaseShader>
class Shader2 {
  protected:
    using Parent = T;
};

} // namespace Mizu

class ParentShader : public Mizu::Shader2<> {
  public:
    BEGIN_SHADER_PARAMETERS()

    SHADER_PARAMETER_RG_TEXTURE2D(Name)
    SHADER_PARAMETER_RG_TEXTURE2D(Name2)

    END_SHADER_PARAMETERS()
};

class ShaderPrueba : public Mizu::Shader2<ParentShader> {
  public:
    BEGIN_SHADER_PARAMETERS()

    SHADER_PARAMETER_RG_TEXTURE2D(Name3)

    END_SHADER_PARAMETERS()
};