#pragma once

#include <memory>
#include <optional>
#include <variant>
#include <vector>

#include "render_core/resources/buffers.h"

namespace Mizu
{

// Forward declarations
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
        std::shared_ptr<VertexBuffer> vertex_buffer;
        ImageFormat vertex_format;
        uint32_t vertex_stride;

        // Index is not mandatory, can only be vertex buffer information
        std::shared_ptr<IndexBuffer> index_buffer = nullptr;
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

    static AccelerationStructureGeometry triangles(std::shared_ptr<VertexBuffer> vertex_buffer,
                                                   ImageFormat vertex_format,
                                                   uint32_t vertex_stride,
                                                   std::shared_ptr<IndexBuffer> index_buffer = nullptr)
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

class AccelerationStructure
{
  public:
    enum class Type
    {
        TopLevel,
        BottomLevel,
    };

    struct Description
    {
        Type type;
        AccelerationStructureGeometry geometry;

        std::string name;
    };

    virtual ~AccelerationStructure() = default;

    static std::shared_ptr<AccelerationStructure> create(const Description& desc,
                                                         std::weak_ptr<IDeviceMemoryAllocator> allocator);

    virtual AccelerationStructureBuildSizes get_build_sizes() const = 0;
    virtual Type get_type() const = 0;
};

struct AccelerationStructureInstanceData
{
    std::shared_ptr<AccelerationStructure> blas;
    glm::mat4 transform;
};

#define DEFINE_ACCELERATION_STRUCTURE_TYPE(_name, _type)                                                      \
    class _name                                                                                               \
    {                                                                                                         \
      public:                                                                                                 \
        static std::shared_ptr<AccelerationStructure> create(const AccelerationStructureGeometry& geometry,   \
                                                             std::string name,                                \
                                                             std::weak_ptr<IDeviceMemoryAllocator> allocator) \
        {                                                                                                     \
            AccelerationStructure::Description desc{};                                                        \
            desc.type = _type;                                                                                \
            desc.geometry = geometry;                                                                         \
            desc.name = name;                                                                                 \
                                                                                                              \
            return AccelerationStructure::create(desc, allocator);                                            \
        }                                                                                                     \
    }

DEFINE_ACCELERATION_STRUCTURE_TYPE(BottomLevelAccelerationStructure, AccelerationStructure::Type::BottomLevel);
DEFINE_ACCELERATION_STRUCTURE_TYPE(TopLevelAccelerationStructure, AccelerationStructure::Type::TopLevel);

#undef DEFINE_ACCELERATION_STRUCTURE_TYPE

} // namespace Mizu
