#pragma once

#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "base/debug/assert.h"

#include "render_core/rhi/shader.h"

namespace Mizu
{

// Forward declarations
class ImageResourceView;
class BufferResource;
class SamplerState;
class AccelerationStructure;
class ShaderResourceView;
class UnorderedAccessView;
class ConstantBufferView;

struct ResourceGroupItem
{
#define RESOURCE_GROUP_ITEMS_LIST                                       \
    X(SampledImage, std::shared_ptr<ImageResourceView>)                 \
    X(StorageImage, std::shared_ptr<ImageResourceView>)                 \
    X(UniformBuffer, std::shared_ptr<BufferResource>)                   \
    X(StorageBuffer, std::shared_ptr<BufferResource>)                   \
    X(Sampler, std::shared_ptr<SamplerState>)                           \
    X(RtxAccelerationStructure, std::shared_ptr<AccelerationStructure>) \
    X(BufferSrv, std::shared_ptr<ShaderResourceView>)                   \
    X(TextureSrv, std::shared_ptr<ShaderResourceView>)                  \
    X(BufferUav, std::shared_ptr<UnorderedAccessView>)                  \
    X(TextureUav, std::shared_ptr<UnorderedAccessView>)                 \
    X(ConstantBuffer, std::shared_ptr<ConstantBufferView>)

#define X(_name, _type)                                                                                  \
    struct _name##T                                                                                      \
    {                                                                                                    \
        _type value;                                                                                     \
        size_t hash() const { return std::hash<_type>()(value); }                                        \
    };                                                                                                   \
    static ResourceGroupItem _name(uint32_t binding, _type value, ShaderType stage)                      \
    {                                                                                                    \
        return ResourceGroupItem{.binding = binding, .value = _name##T{.value = value}, .stage = stage}; \
    }

    RESOURCE_GROUP_ITEMS_LIST

#undef X

    using ResourceGroupItemT = std::variant<
#define X(_name, _type) _name##T,
        RESOURCE_GROUP_ITEMS_LIST
#undef X
        int>; // int because trailing commas are not allowed

    uint32_t binding;
    ResourceGroupItemT value;
    ShaderType stage;

    template <typename T>
    bool is_type() const
    {
        return std::holds_alternative<T>(value);
    }

    template <typename T>
    T as_type() const
    {
        MIZU_ASSERT(is_type<T>(), "ResourceGroupItem does not contain item of type {}", typeid(T).name());
        return std::get<T>(value);
    }

    size_t hash() const
    {
        const size_t value_hash = std::visit(
            [&](auto&& v) -> size_t {
                using T = std::decay_t<decltype(v)>;

                // Special case because int is needed in the variant
                if constexpr (std::is_same_v<T, int>)
                    return 0;
                else
                    return v.hash();
            },
            value);
        return std::hash<uint32_t>()(binding) ^ value_hash ^ std::hash<ShaderType>()(stage);
    }
};

class ResourceGroupBuilder
{
  public:
    ResourceGroupBuilder& add_resource(ResourceGroupItem item);

    size_t get_hash() const;

    const std::vector<ResourceGroupItem>& get_resources() const { return m_resources; }

  private:
    std::vector<ResourceGroupItem> m_resources;
};

class ResourceGroup
{
  public:
    virtual ~ResourceGroup() = default;

    static std::shared_ptr<ResourceGroup> create(const ResourceGroupBuilder& builder);

    virtual size_t get_hash() const = 0;
};

} // namespace Mizu
