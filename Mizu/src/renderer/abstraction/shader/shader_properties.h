#pragma once

#include <string>
#include <variant>
#include <vector>

#include <glm/glm.hpp>

namespace Mizu {

/*
struct ShaderValueProperty {
    enum class Type {
        Float,
        Vec2,
        Vec3,
        Vec4,
        Mat3,
        Mat4,
        Custom,
    };

    std::string name;
    Type type;
    uint32_t size;
};

struct ShaderTextureProperty {};

struct ShaderUniformBufferProperty {
    uint32_t total_size;
    std::vector<ShaderValueProperty> members;
};

using ShaderPropertyT = std::variant<ShaderValueProperty, ShaderTextureProperty, ShaderUniformBufferProperty>;

struct ShaderProperty {
    std::string name;
    uint32_t set;
    ShaderPropertyT value;
};

struct ShaderConstant {
    std::string name;
    uint32_t size;
};
*/

/*
enum class ShaderType {
    Float,
    Vec2,
    Vec3,
    Vec4,
    Mat3,
    Mat4,
};
*/

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

  private:
    Type m_type;
};

struct ShaderInput {
    std::string name;
    ShaderType type;
};

} // namespace Mizu