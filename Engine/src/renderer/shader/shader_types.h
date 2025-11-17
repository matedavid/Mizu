#pragma once

#include <glm/glm.hpp>
#include <string>
#include <variant>

#include "base/debug/assert.h"

namespace Mizu
{

class ShaderPrimitiveType
{
  public:
    enum Type
    {
        Bool,

        Float,
        Float2,
        Float3,
        Float4,

        Float3x3,
        Float4x4,

        Double,

        UInt,
        UInt2,
        UInt3,
        UInt4,

        ULong,
    };

    ShaderPrimitiveType() : m_type(Float) {}
    ShaderPrimitiveType(Type type) : m_type(type) {}

    operator Type() const { return m_type; }

    static uint32_t num_components(ShaderPrimitiveType type)
    {
        switch (type)
        {
        case Bool:
            return 1;

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

        case UInt:
            return 1;
        case UInt2:
            return 2;
        case UInt3:
            return 3;
        case UInt4:
            return 4;

        case ULong:
            return 1;
        }

        MIZU_UNREACHABLE("Invalid ShaderPrimitiveType");
    }

    uint32_t num_components() const { return num_components(*this); }

    static uint32_t size(ShaderPrimitiveType type)
    {
        switch (type)
        {
        case Bool:
            return sizeof(bool);

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

        case UInt:
            return sizeof(uint32_t);
        case UInt2:
            return sizeof(glm::uvec2);
        case UInt3:
            return sizeof(glm::uvec3);
        case UInt4:
            return sizeof(glm::uvec4);

        case ULong:
            return sizeof(uint64_t);
        }

        MIZU_UNREACHABLE("Invalid ShaderPrimitiveType");
    }

    uint32_t size() const { return size(*this); }

    static uint32_t padded_size(ShaderPrimitiveType type)
    {
        switch (type)
        {
        case Bool:
            return 4;

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

        case UInt:
            return sizeof(uint32_t);
        case UInt2:
        case UInt3:
        case UInt4:
            return sizeof(glm::uvec4);

        case ULong:
            return sizeof(uint64_t);
        }

        MIZU_UNREACHABLE("Invalid ShaderPrimitiveType");
    }

    uint32_t padded_size() const { return padded_size(*this); }

    static bool is_scalar(ShaderPrimitiveType type)
    {
        switch (type.m_type)
        {
        case Bool:
        case Float:
        case Double:
        case UInt:
        case ULong:
            return true;
        case Float2:
        case Float3:
        case Float4:
        case Float3x3:
        case Float4x4:
        case UInt2:
        case UInt3:
        case UInt4:
            return false;
        }

        MIZU_UNREACHABLE("Invalid ShaderPrimitiveType");
    }

    bool is_scalar() const { return is_scalar(*this); }

    explicit operator std::string() const
    {
        switch (m_type)
        {
        case Bool:
            return "Bool";
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
        case UInt:
            return "UInt";
        case UInt2:
            return "UInt2";
        case UInt3:
            return "UInt3";
        case UInt4:
            return "UInt4";
        case ULong:
            return "ULong";
        }

        MIZU_UNREACHABLE("Invalid ShaderPrimitiveType");
    }

    std::string as_string() const { return std::string(*this); }

  private:
    Type m_type;
};

struct ShaderPrimitive
{
    std::string name;
    ShaderPrimitiveType type;
};

struct ShaderInputOutput
{
    ShaderPrimitive primitive;
    uint32_t location;
    std::string semantic_name;
    uint32_t semantic_index;
};

struct ShaderBindingInfo
{
    uint32_t set;
    uint32_t binding;
};

enum class ShaderResourceAccessType
{
    ReadOnly,
    ReadWrite,
};

struct ShaderResourceTexture
{
    ShaderResourceAccessType access;
};

struct ShaderResourceTextureCube
{
};

struct ShaderResourceConstantBuffer
{
    std::vector<ShaderPrimitive> members;
    size_t total_size;
};

struct ShaderResourceStructuredBuffer
{
    ShaderResourceAccessType access;
};

struct ShaderResourceByteAddressBuffer
{
    ShaderResourceAccessType access;
};

struct ShaderResourceSamplerState
{
};

struct ShaderResourceAccelerationStructure
{
};

using ShaderResourceT = std::variant<
    ShaderResourceTexture,
    ShaderResourceTextureCube,
    ShaderResourceConstantBuffer,
    ShaderResourceStructuredBuffer,
    ShaderResourceByteAddressBuffer,
    ShaderResourceSamplerState,
    ShaderResourceAccelerationStructure>;

struct ShaderResource
{
    std::string name;
    ShaderBindingInfo binding_info;
    ShaderResourceT value;
};

struct ShaderPushConstant
{
    std::string name;
    size_t size;
    ShaderBindingInfo binding_info;
};

} // namespace Mizu