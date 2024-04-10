#include "vulkan_instance.h"

#include <cassert>

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
    create_info.enabledExtensionCount = 0;
    create_info.ppEnabledExtensionNames = nullptr;

    vkCreateInstance(&create_info, nullptr, &m_instance);
}

VulkanInstance::~VulkanInstance() {
    vkDestroyInstance(m_instance, nullptr);
}

bool VulkanInstance::validation_layers_available(const std::vector<const char*>& validation_layers) {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> layers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, layers.data());

    return std::all_of(validation_layers.begin(), validation_layers.end(), [&](const std::string_view& validation) {
        for (const auto& property : layers) {
            const std::string layer_name = property.layerName;
            if (validation == layer_name)
                return true;
        }
        return false;
    });
}

} // namespace Mizu