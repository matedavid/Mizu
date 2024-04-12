#include "resources_manager.h"

#include <cassert>

std::filesystem::path ResourcesManager::get_resource_path(const std::string& name) {
    const auto resource_path = get_resources_base_path() / name;
    assert(std::filesystem::exists(resource_path) && "Could not find test resource path");

    return resource_path;
}

std::filesystem::path ResourcesManager::get_resources_base_path() {
    return {MIZU_TEST_RESOURCES_PATH};
}
