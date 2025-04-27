#pragma once

#include <glm/glm.hpp>
#include <string>
#include <variant>
#include <vector>

#include "render_core/rhi/image_resource.h"

#include "utility/assert.h"

namespace Mizu
{

class ShaderType
{
  public:
    enum Type
    {
        Float,
        Float2,
        Float3,
        Float4,

        Float3x3,
        Float4x4,

        Double,

        UInt64,
    };

    ShaderType() : m_type(Float) {}
    ShaderType(Type type) : m_type(type) {}

    operator Type() const { return m_type; }

    static uint32_t num_components(ShaderType type)
    {
        switch (type)
        {
        case Float:
            return 1;
        case Float2:
            return 2;
        case Float3:
            return 3;
        case Float4:
            return 4;
        case Float3x3:
            return 3 * 3;
        case Float4x4:
            return 4 * 4;

        case Double:
            return 1;

        case UInt64:
            return 1;
        }

        MIZU_UNREACHABLE("Invalid ShaderType");
    }

    static uint32_t size(ShaderType type)
    {
        switch (type)
        {
        case Float:
            return sizeof(float);
        case Float2:
            return sizeof(glm::vec2);
        case Float3:
            return sizeof(glm::vec3);
        case Float4:
            return sizeof(glm::vec4);
        case Float3x3:
            return sizeof(glm::mat3);
        case Float4x4:
            return sizeof(glm::mat4);

        case Double:
            return sizeof(double);

        case UInt64:
            return sizeof(uint64_t);
        }

        MIZU_UNREACHABLE("Invalid ShaderType");
    }

    static uint32_t padded_size(ShaderType type)
    {
        switch (type)
        {
        case Float:
            return sizeof(float);
        case Float2:
        case Float3:
        case Float4:
            return sizeof(glm::vec4);
        case Float3x3:
        case Float4x4:
            return sizeof(glm::mat4);

        case Double:
            return sizeof(double);

        case UInt64:
            return sizeof(uint64_t);
        }

        MIZU_UNREACHABLE("Invalid ShaderType");
    }

    static bool is_scalar(ShaderType type)
    {
        switch (type.m_type)
        {
        case Float:
        case Double:
        case UInt64:
            return true;
        case Float2:
        case Float3:
        case Float4:
        case Float3x3:
        case Float4x4:
            return false;
        }

        MIZU_UNREACHABLE("Invalid ShaderType");
    }

    explicit operator std::string() const
    {
        switch (m_type)
        {
        case Float:
            return "Float";
        case Float2:
            return "Float2";
        case Float3:
            return "Float3";
        case Float4:
            return "Float4";
        case Float3x3:
            return "Float3x3";
        case Float4x4:
            return "Float4x4";
        case Double:
            return "Double";
        case UInt64:
            return "UInt64";
        }

        MIZU_UNREACHABLE("Invalid ShaderType");
    }

    [[nodiscard]] std::string as_string() const { return std::string(*this); }

  private:
    Type m_type;
};

struct ShaderInput
{
    std::string name;
    ShaderType type;
    uint32_t location;
};

struct ShaderOutput
{
    std::string name;
    ShaderType type;
};

struct ShaderMemberProperty
{
    std::string name;
    ShaderType type;
};

struct ShaderImageProperty
{
    enum class Type
    {
        Sampled,
        Separate,
        Storage,
    };

    Type type;
};

struct ShaderBufferProperty
{
    enum class Type
    {
        Uniform,
        Storage,
    };

    Type type = Type::Uniform;
    uint32_t total_size = 0;
    std::vector<ShaderMemberProperty> members{};
};

struct ShaderSamplerProperty
{
};

using ShaderPropertyT = std::variant<ShaderImageProperty, ShaderBufferProperty, ShaderSamplerProperty>;

struct ShaderProperty
{
    std::string name;
    ShaderPropertyT value;

    struct
    {
        uint32_t set;
        uint32_t binding;
    } binding_info;
};

struct ShaderConstant
{
    std::string name;
    uint32_t size;
};

} // namespace Mizu