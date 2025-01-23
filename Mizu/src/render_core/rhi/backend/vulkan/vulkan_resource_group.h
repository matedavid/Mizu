#pragma once

#include <cassert>
#include <optional>
#include <unordered_map>
#include <vulkan/vulkan.h>

#include "render_core/rhi/resource_group.h"
#include "render_core/shader/shader_properties.h"

#include "utility/assert.h"

namespace Mizu::Vulkan
{

// Forward declarations
class VulkanImageResource;
class VulkanBufferResource;
class VulkanDescriptorPool;
class VulkanShaderBase;
struct VulkanDescriptorInfo;

class VulkanResourceGroup : public ResourceGroup
{
  public:
    VulkanResourceGroup() = default;
    ~VulkanResourceGroup() override = default;

    void add_resource(std::string_view name, std::shared_ptr<ImageResource> image_resource) override;
    void add_resource(std::string_view name, std::shared_ptr<BufferResource> buffer_resource) override;

    [[nodiscard]] size_t get_hash() const override;

    [[nodiscard]] bool bake(const IShader& shader, uint32_t set) override;
    [[nodiscard]] uint32_t currently_baked_set() const override { return m_currently_baked_set_num; }
    [[nodiscard]] bool is_baked() const { return m_set != VK_NULL_HANDLE && m_layout != VK_NULL_HANDLE; }

    [[nodiscard]] VkDescriptorSet get_descriptor_set() const
    {
        MIZU_ASSERT(m_set != VK_NULL_HANDLE, "ResourceGroup has not been baked");
        return m_set;
    }

    [[nodiscard]] VkDescriptorSetLayout get_descriptor_set_layout() const
    {
        MIZU_ASSERT(m_layout != VK_NULL_HANDLE, "ResourceGroup has not been baked");
        return m_layout;
    }

  private:
    VkDescriptorSet m_set{VK_NULL_HANDLE};
    VkDescriptorSetLayout m_layout{VK_NULL_HANDLE};

    uint32_t m_currently_baked_set_num{};

    std::shared_ptr<VulkanDescriptorPool> m_descriptor_pool{};

    std::unordered_map<std::string, std::shared_ptr<VulkanImageResource>> m_image_resource_info;
    std::unordered_map<std::string, std::shared_ptr<VulkanBufferResource>> m_buffer_resource_info;

    [[nodiscard]] static std::optional<ShaderProperty> get_descriptor_info(const std::string& name,
                                                                           uint32_t set,
                                                                           VkDescriptorType type,
                                                                           const VulkanShaderBase& shader);
};

} // namespace Mizu::Vulkan
