#pragma once

#include <string>
#include <variant>
#include <vector>

#include <glm/glm.hpp>

namespace Mizu {

class ShaderType {
  public:
    enum Type {
        Float,
        Vec2,
        Vec3,
        Vec4,
        Mat3,
        Mat4,
    };

    ShaderType() : m_type(Float) {}
    ShaderType(Type type) : m_type(type) {}

    operator Type() const { return m_type; }

    static uint32_t size(ShaderType type) {
        switch (type) {
        case Float:
            return sizeof(float);
        case Vec2:
            return sizeof(glm::vec2);
        case Vec3:
            return sizeof(glm::vec3);
        case Vec4:
            return sizeof(glm::vec4);
        case Mat3:
            return sizeof(glm::mat3);
        case Mat4:
            return sizeof(glm::mat4);
        default:
            return 0;
        }
    }

    static uint32_t padded_size(ShaderType type) {
        switch (type) {
        case Float:
            return sizeof(float);
        case Vec2:
        case Vec3:
        case Vec4:
            return sizeof(glm::vec4);
        case Mat3:
        case Mat4:
            return sizeof(glm::mat4);
        default:
            return 0;
        }
    }

    explicit operator std::string() const {
        switch (m_type) {
        case Float:
            return "Float";
        case Vec2:
            return "Vec2";
        case Vec3:
            return "Vec3";
        case Vec4:
            return "Vec4";
        case Mat3:
            return "Mat3";
        case Mat4:
            return "Mat4";
        default:
            return "";
        }
    }

    [[nodiscard]] std::string as_string() const { return std::string(*this); }

  private:
    Type m_type;
};

struct ShaderInput {
    std::string name;
    ShaderType type;
    uint32_t location;
};

struct ShaderMemberProperty {
    std::string name;
    ShaderType type;
};

struct ShaderTextureProperty {
    enum class Type {
        Sampled,
        Separate,
        Storage,
    };

    Type type;
};

struct ShaderBufferProperty {
    enum class Type {
        Uniform,
        Storage,
    };

    Type type;
    uint32_t total_size;
    std::vector<ShaderMemberProperty> members;
};

using ShaderPropertyT = std::variant<ShaderTextureProperty, ShaderBufferProperty>;

struct ShaderProperty {
    std::string name;
    ShaderPropertyT value;

    struct {
        uint32_t set;
        uint32_t binding;
    } binding_info;
};

struct ShaderConstant {
    std::string name;
    uint32_t size;
};

} // namespace Mizu