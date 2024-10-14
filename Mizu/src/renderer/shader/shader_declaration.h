#pragma once

#include <functional>
#include <memory>
#include <variant>

#include "managers/shader_manager.h"
#include "renderer/abstraction/shader.h"
#include "renderer/render_graph/render_graph_types.h"

namespace Mizu {

using ShaderDeclarationMemberT = std::variant<RGTextureRef, RGCubemapRef, RGUniformBufferRef>;

enum class ShaderDeclarationMemberType {
    RGTexture2D,
    RGCubemap,
    RGUniformBuffer,
};

struct ShaderDeclarationMemberInfo {
    std::string mem_name;
    ShaderDeclarationMemberType mem_type;
    ShaderDeclarationMemberT value;
};

using members_vec_t = std::vector<ShaderDeclarationMemberInfo>;

#define BEGIN_SHADER_PARAMETERS()                                                                        \
    class Parameters : public Parent::Parameters {                                                       \
      public:                                                                                            \
        Parameters() {}                                                                                  \
                                                                                                         \
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

#define SHADER_PARAMETER_IMPL(name, type, default_value, type_enum)                                                    \
    _member_##name;                                                                                                    \
                                                                                                                       \
  public:                                                                                                              \
    type name = default_value;                                                                                         \
                                                                                                                       \
  private:                                                                                                             \
    struct _next_member_##name {};                                                                                     \
    static _func _append_member_get_prev_func(                                                                         \
        _next_member_##name, Mizu::members_vec_t& members, const Parameters& params) {                                 \
        auto info = Mizu::ShaderDeclarationMemberInfo{.mem_name = #name, .mem_type = type_enum, .value = params.name}; \
        members.push_back(info);                                                                                       \
        _func (*prev_func)(_member_##name, Mizu::members_vec_t&, const Parameters&);                                   \
        prev_func = _append_member_get_prev_func;                                                                      \
        return (void*)prev_func;                                                                                       \
    }                                                                                                                  \
    typedef _next_member_##name

#define SHADER_PARAMETER_RG_TEXTURE2D(name) \
    SHADER_PARAMETER_IMPL(                  \
        name, Mizu::RGTextureRef, Mizu::RGTextureRef::invalid(), Mizu::ShaderDeclarationMemberType::RGTexture2D)

#define SHADER_PARAMETER_RG_CUBEMAP(name) \
    SHADER_PARAMETER_IMPL(                \
        name, Mizu::RGCubemapRef, Mizu::RGCubemapRef::invalid(), Mizu::ShaderDeclarationMemberType::RGCubemap)

#define SHADER_PARAMETER_RG_UNIFORM_BUFFER(name)               \
    SHADER_PARAMETER_IMPL(name,                                \
                          Mizu::RGUniformBufferRef,            \
                          Mizu::RGUniformBufferRef::invalid(), \
                          Mizu::ShaderDeclarationMemberType::RGUniformBuffer)

#define IMPLEMENT_GRAPHICS_SHADER(vertex_path, fragment_path)               \
    static std::shared_ptr<Mizu::IShader> get_shader() {                    \
        return Mizu::ShaderManager::get_shader(vertex_path, fragment_path); \
    }

#define IMPLEMENT_COMPUTE_SHADER(compute_path)                \
    static std::shared_ptr<Mizu::IShader> get_shader() {      \
        return Mizu::ShaderManager::get_shader(compute_path); \
    }

class BaseShaderDeclaration final {
  public:
    struct Parameters {
        static Mizu::members_vec_t get_members([[maybe_unused]] const Parameters& params = {}) { return {}; }
    };
};

template <typename T = BaseShaderDeclaration>
class ShaderDeclaration {
  public:
    static std::shared_ptr<IShader> get_shader() { return nullptr; }

    using Parent = T;
};

} // namespace Mizu