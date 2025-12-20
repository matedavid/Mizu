#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "base/debug/assert.h"

namespace Mizu
{

// Forward declarations
class BufferResource;
enum class ImageFormat;

struct AccelerationStructureBuildSizes
{
    uint64_t acceleration_structure_size;
    uint64_t build_scratch_size;
    uint64_t update_scratch_size;
};

class AccelerationStructureGeometry
{
  public:
    struct TrianglesDescription
    {
        std::shared_ptr<BufferResource> vertex_buffer;
        ImageFormat vertex_format;
        uint32_t vertex_stride;

        // Index is not mandatory, can only be vertex buffer information
        std::shared_ptr<BufferResource> index_buffer = nullptr;
    };

    struct InstancesDescription
    {
        uint32_t max_instances;
        bool allow_updates = true;
    };

    AccelerationStructureGeometry() = default;

    static AccelerationStructureGeometry triangles(const TrianglesDescription& desc)
    {
        return AccelerationStructureGeometry(desc);
    }

    static AccelerationStructureGeometry triangles(
        std::shared_ptr<BufferResource> vertex_buffer,
        ImageFormat vertex_format,
        uint32_t vertex_stride,
        std::shared_ptr<BufferResource> index_buffer = nullptr)
    {
        TrianglesDescription desc{};
        desc.vertex_buffer = vertex_buffer;
        desc.vertex_format = vertex_format;
        desc.vertex_stride = vertex_stride;
        desc.index_buffer = index_buffer;

        return AccelerationStructureGeometry(desc);
    }

    static AccelerationStructureGeometry instances(const InstancesDescription& desc)
    {
        return AccelerationStructureGeometry(desc);
    }

    static AccelerationStructureGeometry instances(uint32_t max_instances, bool allow_updates = true)
    {
        InstancesDescription desc{};
        desc.max_instances = max_instances;
        desc.allow_updates = allow_updates;

        return AccelerationStructureGeometry(desc);
    }

    template <typename T>
    bool is_type() const
    {
        MIZU_ASSERT(has_value(), "AccelerationStructureGeometry doesn't have a value");
        return std::holds_alternative<T>(*m_value);
    }

    template <typename T>
    const T& as_type() const
    {
        MIZU_ASSERT(is_type<T>(), "Variant is not of type {}", typeid(T).name());
        return std::get<T>(*m_value);
    }

    bool has_value() const { return m_value.has_value(); }

  private:
    using GeometryT = std::variant<TrianglesDescription, InstancesDescription>;
    std::optional<GeometryT> m_value;

    AccelerationStructureGeometry(TrianglesDescription desc) : m_value(std::move(desc)) {}
    AccelerationStructureGeometry(InstancesDescription desc) : m_value(std::move(desc)) {}
};

enum class AccelerationStructureType
{
    TopLevel,
    BottomLevel,
};

struct AccelerationStructureDescription
{
    AccelerationStructureType type;
    AccelerationStructureGeometry geometry;

    std::string name;
};

class AccelerationStructure
{
  public:
    virtual ~AccelerationStructure() = default;

    virtual AccelerationStructureBuildSizes get_build_sizes() const = 0;
    virtual AccelerationStructureType get_type() const = 0;
};

struct AccelerationStructureInstanceData
{
    std::shared_ptr<AccelerationStructure> blas;
    glm::mat4 transform;
};

#define DEFINE_ACCELERATION_STRUCTURE_TYPE(_name, _type)      \
    class _name                                               \
    {                                                         \
      public:                                                 \
        static std::shared_ptr<AccelerationStructure> create( \
            const AccelerationStructureGeometry& geometry,    \
            std::string name)                                 \
        {                                                     \
            AccelerationStructureDescription desc{};          \
            desc.type = _type;                                \
            desc.geometry = geometry;                         \
            desc.name = name;                                 \
                                                              \
            (void)desc;                                       \
            return nullptr;                                   \
        }                                                     \
    }

DEFINE_ACCELERATION_STRUCTURE_TYPE(BottomLevelAccelerationStructure, AccelerationStructureType::BottomLevel);
DEFINE_ACCELERATION_STRUCTURE_TYPE(TopLevelAccelerationStructure, AccelerationStructureType::TopLevel);

#undef DEFINE_ACCELERATION_STRUCTURE_TYPE

} // namespace Mizu
