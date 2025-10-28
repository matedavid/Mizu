#include "dx12_shader.h"

#include "base/debug/assert.h"
#include "base/debug/logging.h"
#include "base/io/filesystem.h"

#include "renderer/shader/shader_reflection.h"

#include "render_core/shader/shader_reflection.h"
#include "render_core/shader/shader_transpiler.h"

namespace Mizu::Dx12
{

Dx12Shader::Dx12Shader(Description desc) : m_description(std::move(desc))
{
    m_source_code = Filesystem::read_file(m_description.path);

    m_shader_bytecode = {};
    m_shader_bytecode.pShaderBytecode = m_source_code.data();
    m_shader_bytecode.BytecodeLength = m_source_code.size();

    const std::filesystem::path reflection_path = m_description.path.string() + ".json";
    MIZU_ASSERT(std::filesystem::exists(reflection_path), "Reflection path does not exist");

    const std::string reflection_content = Filesystem::read_file_string(reflection_path);
    m_reflection = std::make_unique<SlangReflection>(reflection_content);
}

Dx12Shader::~Dx12Shader()
{
    // Destructor here to destruct m_reflection correctly
}

const SlangReflection& Dx12Shader::get_reflection() const
{
    return *m_reflection;
}

} // namespace Mizu::Dx12