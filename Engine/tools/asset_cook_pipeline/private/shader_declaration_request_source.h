#pragma once

#include <vector>

#include "shader/shader_registry.h"

#include "asset_cooker.h"

namespace Mizu
{

class ShaderDeclarationRequestSource : public IRequestSource
{
  public:
    bool init(const CookContext& context) override;

    uint32_t enumerate_n(uint32_t number, std::vector<ImportRequest>& outputs) override;

  private:
    ShaderRegistry m_registry{};
    std::span<const ShaderDeclarationMetadata> m_shader_metadata{};
    uint32_t m_shader_cursor = 0;

    std::vector<std::string> m_include_paths{};
};

} // namespace Mizu
