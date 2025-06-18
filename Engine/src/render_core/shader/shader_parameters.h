#pragma once

#include <string>
#include <variant>
#include <vector>

namespace Mizu
{

// Forward declarations
class SamplerState;
struct RGFramebufferAttachments;

using ShaderParameterMemberT = std::variant<RGImageViewRef,
                                            RGUniformBufferRef,
                                            RGStorageBufferRef,
                                            std::shared_ptr<SamplerState>,
                                            RGAccelerationStructureRef,
                                            RGFramebufferAttachments>;

enum class ShaderParameterMemberType
{
    RGSampledImageView,
    RGStorageImageView,
    SamplerState,
    RGUniformBuffer,
    RGStorageBuffer,
    RGAccelerationStructure,
    RGFramebufferAttachments,
};

struct ShaderParameterMemberInfo
{
    std::string mem_name;
    ShaderParameterMemberType mem_type;
    ShaderParameterMemberT value;
};

using members_vec_t = std::vector<ShaderParameterMemberInfo>;

class _BaseParameters
{
  public:
    static Mizu::members_vec_t get_members([[maybe_unused]] const _BaseParameters& params = {}) { return {}; }
};

#define BEGIN_SHADER_PARAMETERS_INHERIT(name, inherit)                                                        \
    class name : public inherit                                                                               \
    {                                                                                                         \
      private:                                                                                                \
        using _ParametersName = name;                                                                         \
        using _ParentParametersName = inherit;                                                                \
                                                                                                              \
      public:                                                                                                 \
        name() {}                                                                                             \
                                                                                                              \
      private:                                                                                                \
        typedef void* _func;                                                                                  \
        struct _first_member_id                                                                               \
        {                                                                                                     \
        };                                                                                                    \
        typedef _func (*_member_func)(_first_member_id, Mizu::members_vec_t&, const _ParametersName& params); \
        static _func _append_member_get_prev_func(_first_member_id,                                           \
                                                  [[maybe_unused]] Mizu::members_vec_t& members,              \
                                                  [[maybe_unused]] const _ParametersName& params)             \
        {                                                                                                     \
            return nullptr;                                                                                   \
        }                                                                                                     \
        typedef _first_member_id

#define BEGIN_SHADER_PARAMETERS(name) BEGIN_SHADER_PARAMETERS_INHERIT(name, Mizu::_BaseParameters)

#define END_SHADER_PARAMETERS()                                                             \
    _last_member_id;                                                                        \
                                                                                            \
  public:                                                                                   \
    static Mizu::members_vec_t get_members(const _ParametersName& params = {})              \
    {                                                                                       \
        Mizu::members_vec_t members = _ParentParametersName::get_members(params);           \
        _func (*last_func)(_last_member_id, Mizu::members_vec_t&, const _ParametersName&);  \
        last_func = _append_member_get_prev_func;                                           \
        _func ptr = (_func)last_func;                                                       \
        do                                                                                  \
        {                                                                                   \
            ptr = reinterpret_cast<_member_func>(ptr)(_first_member_id{}, members, params); \
        } while (ptr);                                                                      \
        return members;                                                                     \
    }                                                                                       \
    }                                                                                       \
    ;

#define SHADER_PARAMETER_IMPL(name, type, default_value, type_enum)                                                  \
    _member_##name;                                                                                                  \
                                                                                                                     \
  public:                                                                                                            \
    type name = default_value;                                                                                       \
                                                                                                                     \
  private:                                                                                                           \
    struct _next_member_##name                                                                                       \
    {                                                                                                                \
    };                                                                                                               \
    static _func _append_member_get_prev_func(                                                                       \
        _next_member_##name, Mizu::members_vec_t& members, const _ParametersName& params)                            \
    {                                                                                                                \
        auto info = Mizu::ShaderParameterMemberInfo{.mem_name = #name, .mem_type = type_enum, .value = params.name}; \
        members.push_back(info);                                                                                     \
        _func (*prev_func)(_member_##name, Mizu::members_vec_t&, const _ParametersName&);                            \
        prev_func = _append_member_get_prev_func;                                                                    \
        return (void*)prev_func;                                                                                     \
    }                                                                                                                \
    typedef _next_member_##name

#define SHADER_PARAMETER_RG_SAMPLED_IMAGE_VIEW(name)       \
    SHADER_PARAMETER_IMPL(name,                            \
                          Mizu::RGImageViewRef,            \
                          Mizu::RGImageViewRef::invalid(), \
                          Mizu::ShaderParameterMemberType::RGSampledImageView)

#define SHADER_PARAMETER_RG_STORAGE_IMAGE_VIEW(name)       \
    SHADER_PARAMETER_IMPL(name,                            \
                          Mizu::RGImageViewRef,            \
                          Mizu::RGImageViewRef::invalid(), \
                          Mizu::ShaderParameterMemberType::RGStorageImageView)

#define SHADER_PARAMETER_RG_UNIFORM_BUFFER(name)               \
    SHADER_PARAMETER_IMPL(name,                                \
                          Mizu::RGUniformBufferRef,            \
                          Mizu::RGUniformBufferRef::invalid(), \
                          Mizu::ShaderParameterMemberType::RGUniformBuffer)

#define SHADER_PARAMETER_RG_STORAGE_BUFFER(name)               \
    SHADER_PARAMETER_IMPL(name,                                \
                          Mizu::RGStorageBufferRef,            \
                          Mizu::RGStorageBufferRef::invalid(), \
                          Mizu::ShaderParameterMemberType::RGStorageBuffer)

#define SHADER_PARAMETER_SAMPLER_STATE(name) \
    SHADER_PARAMETER_IMPL(                   \
        name, std::shared_ptr<Mizu::SamplerState>, nullptr, Mizu::ShaderParameterMemberType::SamplerState)

#define SHADER_PARAMETER_RG_ACCELERATION_STRUCTURE(name)               \
    SHADER_PARAMETER_IMPL(name,                                        \
                          Mizu::RGAccelerationStructureRef,            \
                          Mizu::RGAccelerationStructureRef::invalid(), \
                          Mizu::ShaderParameterMemberType::RGAccelerationStructure)

#define SHADER_PARAMETER_RG_FRAMEBUFFER_ATTACHMENTS()       \
    SHADER_PARAMETER_IMPL(framebuffer,                      \
                          Mizu::RGFramebufferAttachments,   \
                          Mizu::RGFramebufferAttachments{}, \
                          Mizu::ShaderParameterMemberType::RGFramebufferAttachments)

} // namespace Mizu