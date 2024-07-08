#include "shader_2.h"

#include "utility/logging.h"

void prueba() {
    auto x = ShaderPrueba();

    auto params = ShaderPrueba::Parameters{};
    params.Name = 1;
    params.Name2 = 2;
    params.Name3 = 3;

    const auto members = ShaderPrueba::Parameters::get_members();
    for (const auto& mem : members) {
        auto t = std::get<Mizu::RGTextureRef>(mem.value);
        MIZU_LOG_INFO("{} -> {}", mem.mem_name, (Mizu::UUID::Type)t);
    }
}
