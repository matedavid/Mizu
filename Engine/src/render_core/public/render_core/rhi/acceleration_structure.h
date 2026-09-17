#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

#include "base/debug/assert.h"
#include "base/utils/enum_utils.h"

#include "mizu_render_core_module.h"
#include "render_core/rhi/image_resource.h"
#include "render_core/rhi/resource_view.h"

namespace Mizu
{

// Forward declarations
class BufferResource;

enum class AccelerationStructureResourceState
{
    Undefined,
    AccelStructRead,
    AccelStructWrite,
};

struct AccelerationStructureBuildSizes
{
    uint64_t acceleration_structure_size;
    uint64_t build_scratch_size;
    uint64_t update_scratch_size;
};

struct AccelerationStructureInstanceData
{
    std::shared_ptr<AccelerationStructure> blas;
    glm::mat4 transform;
};

using AccelerationStructureFlagBitsType = uint8_t;

// clang-format off
enum class AccelerationStructureFlagBits
{
    None            = 0,
    AllowUpdate     = (1 << 0),
    PreferFastTrace = (1 << 1),
};
// clang-format on

IMPLEMENT_ENUM_FLAGS_FUNCTIONS(AccelerationStructureFlagBits, AccelerationStructureFlagBitsType);
MIZU_META_ENUM_FLAGS(AccelerationStructureFlagBits);

class AccelerationStructureGeometry
{
  public:
    struct TrianglesDescription
    {
        std::shared_ptr<BufferResource> vertex_buffer = nullptr;
        uint64_t vertex_offset = 0;
        uint32_t vertex_count = 0;
        ImageFormat vertex_format = ImageFormat::R32G32B32_SFLOAT;
        uint32_t vertex_stride = 0;

        std::shared_ptr<BufferResource> index_buffer = nullptr;
        uint64_t index_offset = 0;
        uint32_t index_count = 0;
        IndexBufferFormat index_format = IndexBufferFormat::UInt32;
    };

    AccelerationStructureGeometry() = default;

    static AccelerationStructureGeometry triangles(const TrianglesDescription& desc)
    {
        return AccelerationStructureGeometry(desc);
    }

    static AccelerationStructureGeometry triangles(
        std::shared_ptr<BufferResource> vertex_buffer,
        uint64_t vertex_offset,
        uint32_t vertex_count,
        ImageFormat vertex_format,
        uint32_t vertex_stride,
        std::shared_ptr<BufferResource> index_buffer,
        uint64_t index_offset,
        uint32_t index_count,
        IndexBufferFormat index_format)
    {
        return AccelerationStructureGeometry(
            TrianglesDescription{
                .vertex_buffer = vertex_buffer,
                .vertex_offset = vertex_offset,
                .vertex_count = vertex_count,
                .vertex_format = vertex_format,
                .vertex_stride = vertex_stride,
                .index_buffer = index_buffer,
                .index_offset = index_offset,
                .index_count = index_count,
                .index_format = index_format,
            });
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

    template <typename T>
    const T* get_if() const
    {
        if (m_value.has_value())
            return std::get_if<T>(&m_value.value());

        return nullptr;
    }

    bool has_value() const { return m_value.has_value(); }

  private:
    using GeometryT = std::variant<TrianglesDescription>;
    std::optional<GeometryT> m_value;

    AccelerationStructureGeometry(TrianglesDescription desc) : m_value(std::move(desc)) {}
};

enum class AccelerationStructureType
{
    TopLevel,
    BottomLevel,
};

struct TopLevelAccelerationStructureDescription
{
    uint32_t max_instances = 0;
};

struct BottomLevelAccelerationStructureDescription
{
    AccelerationStructureGeometry geometry{};
};

struct AccelerationStructureDescription
{
    using AccelerationStructureDescT =
        std::variant<TopLevelAccelerationStructureDescription, BottomLevelAccelerationStructureDescription>;

    AccelerationStructureDescT description{};
    AccelerationStructureFlagBits flags = AccelerationStructureFlagBits::None;

    std::string name{};
};

class AccelerationStructure
{
  public:
    virtual ~AccelerationStructure() = default;

    virtual AccelerationStructureBuildSizes get_build_sizes() const = 0;
    virtual AccelerationStructureType get_type() const = 0;
};

} // namespace Mizu
