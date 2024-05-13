#pragma once

#include <cassert>
#include <optional>
#include <unordered_map>
#include <vulkan/vulkan.h>

#include "renderer/abstraction/resource_group.h"

namespace Mizu::Vulkan {

// Forward declarations
class VulkanDescriptorPool;
class VulkanTexture2D;
class VulkanUniformBuffer;
class VulkanShader;
struct VulkanDescriptorInfo;

class VulkanResourceGroup : public ResourceGroup {
  public:
    VulkanResourceGroup() = default;
    ~VulkanResourceGroup() override = default;

    void add_resource(std::string_view name, std::shared_ptr<Texture2D> texture) override;
    void add_resource(std::string_view name, std::shared_ptr<UniformBuffer> ubo) override;

    [[nodiscard]] bool bake(const std::shared_ptr<Shader>& shader, uint32_t set) override;
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

    std::unordered_map<std::string, std::shared_ptr<VulkanTexture2D>> m_image_info;
    std::unordered_map<std::string, std::shared_ptr<VulkanUniformBuffer>> m_buffer_info;

    [[nodiscard]] static std::optional<VulkanDescriptorInfo> get_descriptor_info(
        std::string name,
        uint32_t set,
        VkDescriptorType type,
        const std::shared_ptr<VulkanShader>& shader);
};

} // namespace Mizu::Vulkan
