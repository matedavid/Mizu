#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "render_core/rhi/shader.h"

#include "utility/assert.h"
#include "utility/enum_utils.h"

namespace Mizu
{

// Forward declarations
class ImageResourceView;
class BufferResource;
class SamplerState;
class TopLevelAccelerationStructure;

template <typename T, typename Variant>
struct is_in_variant;

template <typename T, typename... Ts>
struct is_in_variant<T, std::variant<Ts...>> : std::disjunction<std::is_same<T, Ts>...>
{
};

using LayoutResourceT = std::variant<std::shared_ptr<ImageResourceView>,
                                     std::shared_ptr<BufferResource>,
                                     std::shared_ptr<SamplerState>,
                                     std::shared_ptr<TopLevelAccelerationStructure>>;

struct LayoutResource
{
    uint32_t binding;
    LayoutResourceT value;
    ShaderType stage;

    template <typename T>
    bool is_type() const
    {
        return std::holds_alternative<std::shared_ptr<T>>(value);
    }

    template <typename T>
    std::shared_ptr<T> as_type() const
    {
        MIZU_ASSERT(is_type<T>(), "Item's value is not of type {}", typeid(T).name());
        return std::get<std::shared_ptr<T>>(value);
    }
};

class ResourceGroupLayout
{
  public:
    template <typename T>
    ResourceGroupLayout& add_resource(uint32_t binding, std::shared_ptr<T> resource, ShaderType stage)
    {
        static_assert(is_in_variant<std::shared_ptr<T>, LayoutResourceT>::value, "Resource type is not allowed");

        LayoutResource item{};
        item.binding = binding;
        item.value = resource;
        item.stage = stage;

        m_resources.push_back(item);

        return *this;
    }

    const std::vector<LayoutResource>& get_resources() const { return m_resources; }

  private:
    std::vector<LayoutResource> m_resources;

    friend class ResourceGroup;
};

class ResourceGroup
{
  public:
    virtual ~ResourceGroup() = default;

    static std::shared_ptr<ResourceGroup> create(const ResourceGroupLayout& layout);
};

} // namespace Mizu
