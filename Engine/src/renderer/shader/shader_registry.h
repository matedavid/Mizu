#pragma once

#include <span>
#include <vector>

namespace Mizu
{

// Forward declarations
struct ShaderDeclarationMetadata;

class ShaderRegistry
{
  public:
    static ShaderRegistry& get();

    void register_shader(const ShaderDeclarationMetadata& metadata);
    std::span<const ShaderDeclarationMetadata> get_shader_metadata_list() const;

  private:
    std::vector<ShaderDeclarationMetadata> m_shader_metadata_list;
};

struct ShaderRegistryCallback
{
    ShaderRegistryCallback(const ShaderDeclarationMetadata& metadata);
};

} // namespace Mizu