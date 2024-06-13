#include "vulkan_instance.h"

#include <algorithm>
#include <vector>

#include "renderer/abstraction/backend/vulkan/vk_core.h"

namespace Mizu::Vulkan {

VulkanInstance::VulkanInstance(const Description& desc) {
    VkApplicationInfo application_info{};
    application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    application_info.pApplicationName = desc.application_name.c_str();
    application_info.applicationVersion = desc.application_version;
    application_info.pEngineName = desc.application_name.c_str();
    application_info.engineVersion = desc.application_version;
    application_info.apiVersion = VK_API_VERSION_1_3;

    const std::vector<const char*> layers = {"VK_LAYER_KHRONOS_validation"};
    assert(validation_layers_available(layers) && "VulkanInstance layers not available");

    VkInstanceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &application_info;
    create_info.enabledLayerCount = static_cast<uint32_t>(layers.size());
    create_info.ppEnabledLayerNames = layers.data();
    create_info.enabledExtensionCount = static_cast<uint32_t>(desc.extensions.size());
    create_info.ppEnabledExtensionNames = desc.extensions.data();

    VK_CHECK(vkCreateInstance(&create_info, nullptr, &m_handle));
}

VulkanInstance::~VulkanInstance() {
    vkDestroyInstance(m_handle, nullptr);
}

std::vector<VkPhysicalDevice> VulkanInstance::get_physical_devices() const {
    uint32_t count;
    VK_CHECK(vkEnumeratePhysicalDevices(m_handle, &count, nullptr));

    std::vector<VkPhysicalDevice> physical_devices(count);
    VK_CHECK(vkEnumeratePhysicalDevices(m_handle, &count, physical_devices.data()));

    return physical_devices;
}

bool VulkanInstance::is_extension_available(const char* name) {
    const auto str_name = std::string(name);

    uint32_t count;
    vkEnumerateInstanceExtensionProperties(VK_NULL_HANDLE, &count, VK_NULL_HANDLE);

    std::vector<VkExtensionProperties> extensions(count);
    vkEnumerateInstanceExtensionProperties(VK_NULL_HANDLE, &count, extensions.data());

    for (const auto& ext : extensions) {
        if (std::string(ext.extensionName) == str_name) {
            return true;
        }
    }

    return false;
}

bool VulkanInstance::validation_layers_available(const std::vector<const char*>& validation_layers) {
    uint32_t layerCount;
    VK_CHECK(vkEnumerateInstanceLayerProperties(&layerCount, nullptr));

    std::vector<VkLayerProperties> layers(layerCount);
    VK_CHECK(vkEnumerateInstanceLayerProperties(&layerCount, layers.data()));

    return std::all_of(validation_layers.begin(), validation_layers.end(), [&](const std::string_view& validation) {
        for (const auto& property : layers) {
            const std::string layer_name = property.layerName;
            if (validation == layer_name)
                return true;
        }
        return false;
    });
}

} // namespace Mizu::Vulkan
