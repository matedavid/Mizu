#pragma once

#include <cassert>
#include <optional>
#include <unordered_map>
#include <vulkan/vulkan.h>

#include "renderer/abstraction/resource_group.h"
#include "renderer/shader/shader_properties.h"

namespace Mizu::Vulkan {

// Forward declarations
class VulkanImageResource;
class VulkanBufferResource;
class VulkanDescriptorPool;
class VulkanShaderBase;
struct VulkanDescriptorInfo;

class VulkanResourceGroup : public ResourceGroup {
  public:
    VulkanResourceGroup() = default;
    ~VulkanResourceGroup() override = default;

    void add_resource(std::string_view name, std::shared_ptr<ImageResource> image_resource) override;
    void add_resource(std::string_view name, std::shared_ptr<BufferResource> buffer_resource) override;

    [[nodiscard]] bool bake(const std::shared_ptr<IShader>& shader, uint32_t set) override;
    [[nodiscard]] uint32_t currently_baked_set() const override { return m_currently_baked_set_num; }
    [[nodiscard]] bool is_baked() const { return m_set != VK_NULL_HANDLE && m_layout != VK_NULL_HANDLE; }

    [[nodiscard]] VkDescriptorSet get_descriptor_set() const {
        assert(m_set != VK_NULL_HANDLE);
        return m_set;
    }

    [[nodiscard]] VkDescriptorSetLayout get_descriptor_set_layout() const {
        assert(m_layout != VK_NULL_HANDLE);
        return m_layout;
    }

  private:
    VkDescriptorSet m_set{VK_NULL_HANDLE};
    VkDescriptorSetLayout m_layout{VK_NULL_HANDLE};

    uint32_t m_currently_baked_set_num{};

    std::shared_ptr<VulkanDescriptorPool> m_descriptor_pool{};

    std::unordered_map<std::string, std::shared_ptr<VulkanImageResource>> m_image_resource_info;
    std::unordered_map<std::string, std::shared_ptr<VulkanBufferResource>> m_buffer_info;

    [[nodiscard]] static std::optional<ShaderProperty> get_descriptor_info(
        const std::string& name,
        uint32_t set,
        VkDescriptorType type,
        const std::shared_ptr<VulkanShaderBase>& shader);
};

} // namespace Mizu::Vulkan
