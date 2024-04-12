#include <catch2/catch_all.hpp>
#include <Mizu/Mizu.h>

TEST_CASE("Vulkan initializes Graphics", "[Initialization]") {
    Mizu::Configuration config{};
    config.graphics_api = Mizu::GraphicsAPI::Vulkan;
    config.requirements = Mizu::Requirements{.graphics = true, .compute = false};

    REQUIRE(Mizu::initialize(config));

    Mizu::shutdown();
}

TEST_CASE("Vulkan initializes Compute", "[Initialization]") {
    Mizu::Configuration config{};
    config.graphics_api = Mizu::GraphicsAPI::Vulkan;
    config.requirements = Mizu::Requirements{.graphics = false, .compute = true};

    REQUIRE(Mizu::initialize(config));

    Mizu::shutdown();
}

TEST_CASE("Vulkan initializes Graphics and Compute", "[Initialization]") {
    Mizu::Configuration config{};
    config.graphics_api = Mizu::GraphicsAPI::Vulkan;
    config.requirements = Mizu::Requirements{.graphics = true, .compute = true};

    REQUIRE(Mizu::initialize(config));

    Mizu::shutdown();
}
