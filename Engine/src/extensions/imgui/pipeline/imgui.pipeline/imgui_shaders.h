#pragma once

#include "render_core/rhi/shader.h"
#include "shader/shader_declaration.h"

namespace Mizu
{

class ImGuiShaderVS : public ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION("imgui:imgui.slang", ShaderType::Vertex, "vs_main");
};

class ImGuiShaderFS : public ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION("imgui:imgui.slang", ShaderType::Fragment, "fs_main");
};

} // namespace Mizu