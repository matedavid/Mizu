#pragma once

#include <string>
#include <vector>

namespace Mizu {

class ShaderTranspiler {
  public:
    enum class Translation {
        Vulkan_2_OpenGL46,
    };

    explicit ShaderTranspiler(const std::vector<char>& content,
                              Translation translation = Translation::Vulkan_2_OpenGL46);
    ~ShaderTranspiler() = default;

    [[nodiscard]] std::string compile() const { return m_compilation; }

  private:
    std::string m_compilation;
};

} // namespace Mizu
