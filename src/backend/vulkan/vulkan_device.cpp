#include "vulkan_device.h"

#include <cassert>
#include <numeric>
#include <ranges>

#include <iostream>

#include "backend/vulkan/vulkan_instance.h"

namespace Mizu::Vulkan {

VulkanDevice::VulkanDevice(const VulkanInstance& instance, const Requirements& reqs) {
    // Select physical device
    select_physical_device(instance, reqs);
    assert(m_physical_device != VK_NULL_HANDLE && "No suitable VkPhysicalDevice found");

    std::cout << "Selected device: " << get_properties(m_physical_device).deviceName << "\n";

    // Create queues
    std::vector<VkDeviceQueueCreateInfo> queue_create_infos;

    VkDeviceQueueCreateInfo queue_create_info_base{};
    queue_create_info_base.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create_info_base.queueCount = 1;
    float priority = 1.0f;
    queue_create_info_base.pQueuePriorities = &priority;

    if (reqs.graphics) {
        queue_create_info_base.queueFamilyIndex = m_queue_families.graphics;
        queue_create_infos.push_back(queue_create_info_base);
    }

    if (reqs.compute) {
        queue_create_info_base.queueFamilyIndex = m_queue_families.compute;
        queue_create_infos.push_back(queue_create_info_base);
    }

    {
        queue_create_info_base.queueFamilyIndex = m_queue_families.transfer;
        queue_create_infos.push_back(queue_create_info_base);
    }

    // Create device
    VkDeviceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
    create_info.pQueueCreateInfos = queue_create_infos.data();

    vkCreateDevice(m_physical_device, &create_info, nullptr, &m_device);
}

VulkanDevice::~VulkanDevice() {
    vkDestroyDevice(m_device, nullptr);
}

void VulkanDevice::select_physical_device(const VulkanInstance& instance, const Requirements& reqs) {
    auto is_physical_device_suitable = [&](VkPhysicalDevice device) -> bool {
        const auto queue_family_properties = get_queue_family_properties(device);

        bool graphics = !reqs.graphics;
        bool compute = !reqs.compute;
        bool transfer = false; // Transfer queue is always a requirement

        for (const VkQueueFamilyProperties property : queue_family_properties) {
            if (property.queueFlags & VK_QUEUE_GRAPHICS_BIT)
                graphics = true;

            if (property.queueFlags & VK_QUEUE_COMPUTE_BIT)
                compute = true;

            if (property.queueFlags & VK_QUEUE_TRANSFER_BIT)
                transfer = true;
        }

        // If physical devices does not contain at least one queue for each of the
        // requirements, physical device is not suitable.
        return graphics && compute && transfer;
    };

    const auto devices = instance.get_physical_devices();

    uint32_t biggest_score = std::numeric_limits<uint32_t>::min();
    VkPhysicalDevice best_device = VK_NULL_HANDLE;
    QueueFamilies best_queue_families{};

    // Notes on Physical Device selection:
    // - Prefer discrete GPUs
    // - Prefer if specific queue types have their own queue (e.g. graphics queue different from compute queue)

    constexpr uint32_t DISCRETE_GPU_SCORE = 40;
    constexpr uint32_t SPECIFIC_QUEUE_TYPE_SCORE = 10;

    for (const VkPhysicalDevice& device : devices | std::views::filter(is_physical_device_suitable)) {
        uint32_t score = 0;

        QueueFamilies queue_familes{};
        bool specific_graphics_queue = false;
        bool specific_compute_queue = false;
        bool specific_transfer_queue = false;

        const auto properties = get_properties(device);
        if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
            score += DISCRETE_GPU_SCORE;

        const auto queue_properties = get_queue_family_properties(device);
        for (uint32_t idx = 0; idx < queue_properties.size(); ++idx) {
            const bool has_graphics = queue_properties[idx].queueFlags & VK_QUEUE_GRAPHICS_BIT;
            const bool has_compute = queue_properties[idx].queueFlags & VK_QUEUE_COMPUTE_BIT;
            const bool has_transfer = queue_properties[idx].queueFlags & VK_QUEUE_TRANSFER_BIT;

            // Has queue with only graphics operation
            if (reqs.graphics) {
                if (has_graphics && !has_compute && !specific_graphics_queue) {
                    score += SPECIFIC_QUEUE_TYPE_SCORE;

                    queue_familes.graphics = idx;
                    specific_graphics_queue = true;
                } else if (has_graphics && !specific_graphics_queue) {
                    queue_familes.graphics = idx;
                }
            }

            // Has queue with only compute operation
            if (reqs.compute) {
                if (has_compute && !has_graphics && !specific_compute_queue) {
                    score += SPECIFIC_QUEUE_TYPE_SCORE;

                    queue_familes.compute = idx;
                    specific_compute_queue = true;
                } else if (has_compute && !specific_compute_queue) {
                    queue_familes.compute = idx;
                }
            }

            // Has queue with only transfer operation
            if (has_transfer && !has_graphics && !has_compute && !specific_transfer_queue) {
                score += SPECIFIC_QUEUE_TYPE_SCORE;

                queue_familes.transfer = idx;
                specific_transfer_queue = true;
            } else if (has_transfer && !specific_transfer_queue) {
                queue_familes.transfer = idx;
            }
        }

        if (score > biggest_score) {
            best_device = device;
            biggest_score = score;
            best_queue_families = queue_familes;
        }
    }

    m_physical_device = best_device;
    m_queue_families = best_queue_families;
}

VkPhysicalDeviceProperties VulkanDevice::get_properties(VkPhysicalDevice physical_device) {
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(physical_device, &properties);

    return properties;
}

std::vector<VkQueueFamilyProperties> VulkanDevice::get_queue_family_properties(VkPhysicalDevice physical_device) {
    uint32_t count;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &count, nullptr);

    std::vector<VkQueueFamilyProperties> queue_family_properties{count};
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &count, queue_family_properties.data());

    return queue_family_properties;
}

} // namespace Mizu::Vulkan