#pragma once

#include <filesystem>

#ifndef MIZU_TEST_RESOURCES_PATH
#define MIZU_TEST_RESOURCES_PATH "./"
#endif

class ResourcesManager {
  public:
    [[nodiscard]] static std::filesystem::path get_resource_path(const std::string& name);
    [[nodiscard]] static std::filesystem::path get_resources_base_path();
};
