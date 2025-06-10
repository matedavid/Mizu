#pragma once

#include <memory>
#include <string>
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

struct LayoutResourceImageView
{
    std::shared_ptr<ImageResourceView> value;
    ShaderImageProperty::Type type;
};

struct LayoutResourceBuffer
{
    std::shared_ptr<BufferResource> value;
    ShaderBufferProperty::Type type;
};

struct LayoutResourceSamplerState
{
    std::shared_ptr<SamplerState> value;
};

struct LayoutResourceTopLevelAccelerationStructure
{
    std::shared_ptr<TopLevelAccelerationStructure> value;
};

using LayoutResourceValueT = std::variant<LayoutResourceImageView,
                                          LayoutResourceBuffer,
                                          LayoutResourceSamplerState,
                                          LayoutResourceTopLevelAccelerationStructure>;

struct LayoutResource
{
    uint32_t binding;
    LayoutResourceValueT value;
    ShaderType stage;

    template <typename T>
    bool is_type() const
    {
        return std::holds_alternative<T>(value);
    }

    template <typename T>
    const T& as_type() const
    {
        MIZU_ASSERT(is_type<T>(), "Item's value is not of type {}", typeid(T).name());
        return std::get<T>(value);
    }
};

class ResourceGroupLayout
{
  public:
    ResourceGroupLayout& add_resource(uint32_t binding,
                                      std::shared_ptr<ImageResourceView> resource,
                                      ShaderType stage,
                                      ShaderImageProperty::Type type);

    ResourceGroupLayout& add_resource(uint32_t binding,
                                      std::shared_ptr<BufferResource> resource,
                                      ShaderType stage,
                                      ShaderBufferProperty::Type type);

    ResourceGroupLayout& add_resource(uint32_t binding, std::shared_ptr<SamplerState> resource, ShaderType stage);

    ResourceGroupLayout& add_resource(uint32_t binding,
                                      std::shared_ptr<TopLevelAccelerationStructure> resource,
                                      ShaderType stage);

    size_t get_hash() const;

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

    virtual size_t get_hash() const = 0;
};

} // namespace Mizu
