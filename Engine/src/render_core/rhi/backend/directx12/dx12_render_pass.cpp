#include "dx12_render_pass.h"

namespace Mizu::Dx12
{

Dx12RenderPass::Dx12RenderPass(Description desc) : m_description(std::move(desc)) {}

} // namespace Mizu::Dx12