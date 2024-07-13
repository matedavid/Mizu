#pragma once

#include <filesystem>

class ResourcesManager {
  public:
    [[nodiscard]] static std::filesystem::path get_resource_path(const std::string& name);
    [[nodiscard]] static std::filesystem::path get_resources_base_path();
};
