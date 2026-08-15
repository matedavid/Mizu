#pragma once

#include <vector>

#include "base/reflection/enum_traits.h"
#include "shader/shader_registry.h"

#include "asset_cooker.h"

namespace Mizu
{

class ShaderDeclarationCookRequestSource : public ICookRequestSource
{
  public:
    virtual ~ShaderDeclarationCookRequestSource() override = default;

    void init(const CookContext& context) override;

    uint32_t enumerate_n(const CookContext& context, uint32_t number, std::vector<CookRequest>& outputs) override;

  private:
    ShaderRegistry m_registry;
    std::span<const ShaderDeclarationMetadata> m_shader_metadata;
    uint32_t m_shader_cursor = 0;

    std::vector<ShaderDeclarationCookInfo> m_pending_cook_infos;
    meta::enum_array<ShaderBytecodeTarget, bool> m_enabled_bytecode_targets{};

    void create_cook_requests_for_metadata(
        const ShaderDeclarationMetadata& metadata,
        std::vector<ShaderDeclarationCookInfo>& outputs);
};

} // namespace Mizu