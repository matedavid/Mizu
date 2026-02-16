#pragma once

#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "base/containers/inplace_vector.h"
#include "base/debug/assert.h"
#include "base/utils/enum_utils.h"
#include "render_core/rhi/acceleration_structure.h"
#include "render_core/rhi/buffer_resource.h"
#include "render_core/rhi/image_resource.h"
#include "render_core/rhi/resource_view.h"

#include "mizu_render_module.h"
#include "render/render_graph/render_graph_types2.h"

namespace Mizu
{

// Forward declarations
class CommandBuffer;

enum class RenderGraphPassHint
{
    Raster,
    Compute,
    AsyncCompute,
    Transfer,
    RayTracing,
};

using RenderGraphResourceUsageBitsType = uint8_t;

// clang-format off
enum class RenderGraphResourceUsageBits : RenderGraphResourceUsageBitsType
{
    None       = 0,
    Read       = (1 << 0),
    Write      = (1 << 1),
    Attachment = (1 << 2),
    CopySrc    = (1 << 3),
    CopyDst    = (1 << 4),
};
// clang-format on

IMPLEMENT_ENUM_FLAGS_FUNCTIONS(RenderGraphResourceUsageBits, RenderGraphResourceUsageBitsType);

enum class RenderGraphResourceType
{
    Buffer,
    Texture,
    AccelerationStructure,
};

#if MIZU_DEBUG

inline std::string_view render_graph_resource_type_to_string(RenderGraphResourceType type)
{
    switch (type)
    {
    case RenderGraphResourceType::Buffer:
        return "Buffer";
    case RenderGraphResourceType::Texture:
        return "Texture";
    case RenderGraphResourceType::AccelerationStructure:
        return "AccelerationStructure";
    }
}

#endif

struct RenderGraphResourceDescription
{
    RenderGraphResource resource{};
    RenderGraphResourceType type{};
    RenderGraphResourceUsageBits usage{};

    static constexpr size_t EXTERNAL_RESOURCE_ID = std::numeric_limits<size_t>::max();
    size_t external_index = EXTERNAL_RESOURCE_ID;

    uint32_t first_pass_idx = std::numeric_limits<uint32_t>::max();
    uint32_t last_pass_idx = std::numeric_limits<uint32_t>::min();

    std::variant<BufferDescription, ImageDescription, AccelerationStructureDescription> desc{};

#define MIZU_IMPLEMENT_RENDER_GRAPH_RESOURCE_DESC_GETTER(_name, _type, _data)                  \
    const _data& _name() const                                                                 \
    {                                                                                          \
        MIZU_ASSERT(                                                                           \
            type == _type && std::holds_alternative<_data>(desc),                              \
            "Trying to get invalid resource description from RenderGraphResourceDescription"); \
        return std::get<_data>(desc);                                                          \
    }                                                                                          \
    _data& _name()                                                                             \
    {                                                                                          \
        MIZU_ASSERT(                                                                           \
            type == _type && std::holds_alternative<_data>(desc),                              \
            "Trying to get invalid resource description from RenderGraphResourceDescription"); \
        return std::get<_data>(desc);                                                          \
    }

    MIZU_IMPLEMENT_RENDER_GRAPH_RESOURCE_DESC_GETTER(buffer, RenderGraphResourceType::Buffer, BufferDescription);
    MIZU_IMPLEMENT_RENDER_GRAPH_RESOURCE_DESC_GETTER(image, RenderGraphResourceType::Texture, ImageDescription);
    MIZU_IMPLEMENT_RENDER_GRAPH_RESOURCE_DESC_GETTER(
        acceleration_structure,
        RenderGraphResourceType::AccelerationStructure,
        AccelerationStructureDescription);

#undef MIZU_RENDER_GRAPH_RESOURCE_DESC_GETTER

    inline bool is_external() const { return external_index != EXTERNAL_RESOURCE_ID; }
};

struct RenderGraphExternalResourceDescription
{
    using ExternalResourceValueT = std::variant<
        std::shared_ptr<BufferResource>,
        std::shared_ptr<ImageResource>,
        std::shared_ptr<AccelerationStructure>>;

    ExternalResourceValueT value{};
    std::variant<RenderGraphExternalBufferState, RenderGraphExternalImageState> state{};

#define MIZU_IMPLEMENT_RENDER_GRAPH_EXTERNAL_RESOURCE_DESC_GETTER(_name, _data, _state_type)                    \
    std::shared_ptr<_data> _name() const                                                                        \
    {                                                                                                           \
        MIZU_ASSERT(                                                                                            \
            std::holds_alternative<std::shared_ptr<_data>>(value),                                              \
            "Trying to get invalid external resource description from RenderGraphExternalResourceDescription"); \
        return std::get<std::shared_ptr<_data>>(value);                                                         \
    }                                                                                                           \
    _state_type _name##_state() const                                                                           \
    {                                                                                                           \
        MIZU_ASSERT(                                                                                            \
            std::holds_alternative<_state_type>(state),                                                         \
            "Trying to get invalid external resource state from RenderGraphExternalResourceDescription");       \
        return std::get<_state_type>(state);                                                                    \
    }

    MIZU_IMPLEMENT_RENDER_GRAPH_EXTERNAL_RESOURCE_DESC_GETTER(buffer, BufferResource, RenderGraphExternalBufferState);
    MIZU_IMPLEMENT_RENDER_GRAPH_EXTERNAL_RESOURCE_DESC_GETTER(image, ImageResource, RenderGraphExternalImageState);
    MIZU_IMPLEMENT_RENDER_GRAPH_EXTERNAL_RESOURCE_DESC_GETTER(
        acceleration_structure,
        AccelerationStructure,
        RenderGraphExternalBufferState);

#undef MIZU_IMPLEMENT_RENDER_GRAPH_EXTERNAL_RESOURCE_DESC_GETTER
};

class IPassDataWrapper
{
  public:
    virtual ~IPassDataWrapper() = default;
};

template <typename T>
class PassDataWrapper : public IPassDataWrapper
{
  public:
    PassDataWrapper() : value({}) {}
    T value;
};

struct RenderGraphAccessRecord
{
    RenderGraphResource resource{};
    RenderGraphResourceUsageBits usage{};
    size_t pass_idx;

    RenderGraphAccessRecord* prev = nullptr;
    RenderGraphAccessRecord* next = nullptr;
};

struct RenderGraphFramebufferDescription
{
    inplace_vector<RenderGraphResource, MAX_FRAMEBUFFER_COLOR_ATTACHMENTS> color_attachments{};
    std::optional<RenderGraphResource> depth_stencil_attachment;
};

class MIZU_RENDER_API RenderGraphPassResources2
{
  public:
    ResourceView get_resource(RenderGraphResource resource) const;

    std::shared_ptr<BufferResource> get_buffer(RenderGraphResource resource) const;
    std::shared_ptr<ImageResource> get_image(RenderGraphResource resource) const;

  private:
    void add_resource(
        RenderGraphResource resource,
        std::shared_ptr<BufferResource> buffer,
        RenderGraphResourceUsageBits usage);
    void add_resource(
        RenderGraphResource resource,
        std::shared_ptr<ImageResource> image,
        RenderGraphResourceUsageBits usage);

    friend class RenderGraphBuilder2;
};

class RenderGraphBuilder2;

class MIZU_RENDER_API RenderGraphPassBuilder2
{
  public:
    RenderGraphPassBuilder2(RenderGraphBuilder2& builder, std::string_view name, uint32_t pass_idx);

    void set_hint(RenderGraphPassHint hint);

    RenderGraphResource read(RenderGraphResource resource);
    RenderGraphResource write(RenderGraphResource resource);
    RenderGraphResource attachment(RenderGraphResource resource);
    RenderGraphResource copy_src(RenderGraphResource resource);
    RenderGraphResource copy_dst(RenderGraphResource resource);

    std::span<const RenderGraphAccessRecord> get_access_records() const;

  private:
    RenderGraphBuilder2& m_builder;
    RenderGraphPassHint m_hint;
    std::string_view m_name;
    uint32_t m_pass_idx;
    bool m_has_outputs;

    static constexpr size_t MAX_PASS_DEPENDENCIES = 20;
    inplace_vector<size_t, MAX_PASS_DEPENDENCIES> m_pass_dependencies;
    inplace_vector<size_t, MAX_PASS_DEPENDENCIES> m_pass_outputs;

    static constexpr size_t MAX_ACCESS_RECORDS_PER_PASS = 20;
    inplace_vector<RenderGraphAccessRecord, MAX_ACCESS_RECORDS_PER_PASS> m_accesses;

    std::unique_ptr<IPassDataWrapper> m_pass_data_wrapper;
    std::function<void(CommandBuffer&, const RenderGraphPassResources2&)> m_execute_func;

    RenderGraphResource add_resource_access(RenderGraphResource resource, RenderGraphResourceUsageBits usage);
    void populate_dependency_info(RenderGraphAccessRecord& record, const RenderGraphResourceDescription& desc);

    template <typename T>
    T& create_pass_data_wrapper()
    {
        m_pass_data_wrapper = std::make_unique<PassDataWrapper<T>>();
        return static_cast<PassDataWrapper<T>&>(*m_pass_data_wrapper).value;
    }

    void set_execute_func(std::function<void(CommandBuffer&, const RenderGraphPassResources2&)>&& func)
    {
        m_execute_func = func;
    }

    friend class RenderGraphBuilder2;
};

template <typename DataT>
using RenderGraphSetupFunc = std::function<void(RenderGraphPassBuilder2&, DataT& data)>;
template <typename DataT>
using RenderGraphExecuteFunc = std::function<void(CommandBuffer&, const DataT&, const RenderGraphPassResources2&)>;

struct RenderGraphBuilder2Config
{
    bool async_compute_enabled = true;
    bool async_copy_enabled = false;
};

class MIZU_RENDER_API RenderGraphBuilder2
{
  public:
    RenderGraphBuilder2(RenderGraphBuilder2Config config);

    RenderGraphBuilder2(const RenderGraphBuilder2& other) = delete;
    RenderGraphBuilder2& operator=(const RenderGraphBuilder2& other) = delete;

    RenderGraphResource create_buffer(BufferDescription desc);
    RenderGraphResource create_constant_buffer(uint64_t size, std::string name);
    RenderGraphResource create_structured_buffer(uint64_t size, uint64_t stride, std::string name);

    template <typename T>
    RenderGraphResource create_constant_buffer(std::string name)
    {
        return create_constant_buffer(sizeof(T), name);
    }

    template <typename T>
    RenderGraphResource create_structured_buffer(uint64_t count, std::string name)
    {
        const uint64_t size = sizeof(T) * count;
        return create_structured_buffer(size, sizeof(T), name);
    }

    RenderGraphResource create_texture(ImageDescription desc);
    RenderGraphResource create_texture2d(uint32_t width, uint32_t height, ImageFormat format, std::string name);

    RenderGraphResource register_external_buffer(
        std::shared_ptr<BufferResource> buffer,
        RenderGraphExternalBufferState state);
    RenderGraphResource register_external_texture(
        std::shared_ptr<ImageResource> image,
        RenderGraphExternalImageState state);
    RenderGraphResource register_external_acceleration_structure(
        std::shared_ptr<AccelerationStructure> acceleration_structure);

    template <typename DataT>
    void add_pass(
        std::string_view name,
        const RenderGraphSetupFunc<DataT>& setup_func,
        RenderGraphExecuteFunc<DataT> execute_func)
    {
        RenderGraphPassBuilder2& pass_builder =
            m_passes.emplace_back(*this, name, static_cast<uint32_t>(m_passes.size()));

        DataT& pass_data = pass_builder.create_pass_data_wrapper<DataT>();
        setup_func(pass_builder, pass_data);

        pass_builder.set_execute_func(
            [pass_data, execute_func](CommandBuffer& command, const RenderGraphPassResources2& resources) {
                execute_func(command, pass_data, resources);
            });
    }

    void compile();

  private:
    RenderGraphBuilder2Config m_config;
    std::vector<RenderGraphResourceDescription> m_resources;
    std::vector<RenderGraphPassBuilder2> m_passes;

    std::vector<RenderGraphExternalResourceDescription> m_external_resources;

    void enqueue_buffer_resource_state_transition(
        const BufferResource& buffer,
        const RenderGraphAccessRecord& access,
        const RenderGraphResourceDescription& resource_desc,
        const RenderGraphPassBuilder2& pass_info);
    void enqueue_image_resource_state_transition(
        const ImageResource& image,
        const RenderGraphAccessRecord& access,
        const RenderGraphResourceDescription& resource_desc,
        const RenderGraphPassBuilder2& pass_info);

    static bool validate_render_pass_builder(const RenderGraphPassBuilder2& pass);

    static BufferUsageBits get_buffer_usage_bits(RenderGraphResourceUsageBits usage);
    static ImageUsageBits get_image_usage_bits(RenderGraphResourceUsageBits usage);

    inline const RenderGraphResourceDescription& get_resource_desc(RenderGraphResource resource) const
    {
        MIZU_ASSERT(
            resource.id != INVALID_RENDER_GRAPH_RESOURCE && resource.id < m_resources.size(),
            "Invalid RenderGraphResource with id {}",
            resource.id);
        return m_resources[resource.id];
    }

    inline RenderGraphResourceDescription& get_resource_desc(RenderGraphResource resource)
    {
        MIZU_ASSERT(
            resource.id != INVALID_RENDER_GRAPH_RESOURCE && resource.id < m_resources.size(),
            "Invalid RenderGraphResource with id {}",
            resource.id);
        return m_resources[resource.id];
    }

    inline const RenderGraphExternalResourceDescription& get_external_resource_desc(RenderGraphResource resource)
    {
        const RenderGraphResourceDescription& resource_desc = get_resource_desc(resource);
        MIZU_ASSERT(
            resource_desc.is_external() && resource_desc.external_index < m_external_resources.size(),
            "Invalid external RenderGraphResource with id {}",
            resource.id);
        return m_external_resources[resource_desc.external_index];
    }

    friend class RenderGraphPassBuilder2;
};

} // namespace Mizu