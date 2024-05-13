#pragma once

#include <memory>
#include <optional>
#include <vector>

#include <vulkan/vulkan.h>

#include "renderer/abstraction/command_buffer.h"
#include "renderer/abstraction/renderer.h"

namespace Mizu::Vulkan {

// Forward declarations
class VulkanInstance;
class VulkanQueue;

class VulkanDevice {
  public:
    VulkanDevice(const VulkanInstance& instance,
                 const Requirements& reqs,
                 const std::vector<std::string>& instance_extensions);
    ~VulkanDevice();

    [[nodiscard]] std::shared_ptr<VulkanQueue> get_graphics_queue() const;
    [[nodiscard]] std::shared_ptr<VulkanQueue> get_compute_queue() const;
    [[nodiscard]] std::shared_ptr<VulkanQueue> get_transfer_queue() const;

    [[nodiscard]] std::vector<VkCommandBuffer> allocate_command_buffers(uint32_t count, CommandBufferType type) const;
    void free_command_buffers(const std::vector<VkCommandBuffer>& command_buffers, CommandBufferType type) const;

    [[nodiscard]] std::optional<uint32_t> find_memory_type(uint32_t filter, VkMemoryPropertyFlags properties) const;

    [[nodiscard]] VkDevice handle() const { return m_device; }
    [[nodiscard]] VkPhysicalDevice physical_device() const { return m_physical_device; }

  private:
    VkDevice m_device{VK_NULL_HANDLE};
    VkPhysicalDevice m_physical_device{VK_NULL_HANDLE};

    struct QueueFamilies {
        uint32_t graphics;
        uint32_t compute;
        uint32_t transfer;
    };
    QueueFamilies m_queue_families{};

    std::shared_ptr<VulkanQueue> m_graphics_queue = nullptr;
    std::shared_ptr<VulkanQueue> m_compute_queue = nullptr;
    std::shared_ptr<VulkanQueue> m_transfer_queue = nullptr;

    VkCommandPool m_graphics_command_pool{VK_NULL_HANDLE};
    VkCommandPool m_compute_command_pool{VK_NULL_HANDLE};
    VkCommandPool m_transfer_command_pool{VK_NULL_HANDLE};

    void select_physical_device(const VulkanInstance& instance, const Requirements& reqs);

    [[nodiscard]] static VkPhysicalDeviceProperties get_properties(VkPhysicalDevice physical_device);
    [[nodiscard]] static std::vector<VkQueueFamilyProperties> get_queue_family_properties(
        VkPhysicalDevice physical_device);
};

} // namespace Mizu::Vulkan