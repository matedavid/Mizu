#include "shader_compiler.h"

namespace Mizu
{

size_t ShaderCompilationEnvironment::get_hash() const
{
    std::hash<const char*> string_hasher;
    std::hash<uint32_t> uint32_hasher;

    size_t hash = 0;
    for (const ShaderCompilationPermutation& permutation : m_permutation_values)
    {
        hash ^= string_hasher(permutation.define.data()) ^ uint32_hasher(permutation.value);
    }

    return hash;
}

} // namespace Mizu