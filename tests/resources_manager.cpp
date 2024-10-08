#include "resources_manager.h"

#include "utility/assert.h"

#ifndef MIZU_TEST_RESOURCES_PATH
#define MIZU_TEST_RESOURCES_PATH "./"
#endif

std::filesystem::path ResourcesManager::get_resource_path(const std::string& name) {
    const auto resource_path = get_resources_base_path() / name;
    MIZU_ASSERT(
        std::filesystem::exists(resource_path), "Could not find test resource path: {}", resource_path.string());

    return resource_path;
}

std::filesystem::path ResourcesManager::get_resources_base_path() {
    return {MIZU_TEST_RESOURCES_PATH};
}
