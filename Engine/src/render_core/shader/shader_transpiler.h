#pragma once

#include <string>
#include <vector>

namespace Mizu
{

class ShaderTranspiler
{
  public:
    enum class Translation
    {
        Spirv_2_OpenGL46,
    };

    explicit ShaderTranspiler(const std::vector<char>& content,
                              Translation translation = Translation::Spirv_2_OpenGL46);
    ~ShaderTranspiler() = default;

    [[nodiscard]] std::string compile() const { return m_compilation; }

  private:
    std::string m_compilation;

    void compile_spirv_2_opengl46(const std::vector<char>& content);
};

} // namespace Mizu
