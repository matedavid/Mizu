#include "Mizu/Mizu.h"

#include <cassert>

int main() {
    const Mizu::Configuration config{
        .graphics_api = Mizu::GraphicsAPI::Vulkan,
        .application_name = "Pruebas",
        .application_version = Mizu::Version{0, 1, 0},
    };

    [[maybe_unused]] const bool ok = Mizu::initialize(config);
    assert(ok);

    Mizu::shutdown();
    return 0;
}