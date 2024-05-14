#include <catch2/catch_all.hpp>
#include <Mizu/Mizu.h>

TEST_CASE("Vulkan initializes Graphics", "[Initialization]") {
    Mizu::RendererConfiguration config{};
    config.graphics_api = Mizu::GraphicsAPI::Vulkan;
    config.backend_specific_config = Mizu::VulkanSpecificConfiguration{};
    config.requirements = Mizu::Requirements{.graphics = true, .compute = false};

    REQUIRE(Mizu::Renderer::initialize(config));

    Mizu::Renderer::shutdown();
}

TEST_CASE("Vulkan initializes Compute", "[Initialization]") {
    Mizu::RendererConfiguration config{};
    config.graphics_api = Mizu::GraphicsAPI::Vulkan;
    config.backend_specific_config = Mizu::VulkanSpecificConfiguration{};
    config.requirements = Mizu::Requirements{.graphics = false, .compute = true};

    REQUIRE(Mizu::Renderer::initialize(config));

    Mizu::Renderer::shutdown();
}

TEST_CASE("Vulkan initializes Graphics and Compute", "[Initialization]") {
    Mizu::RendererConfiguration config{};
    config.graphics_api = Mizu::GraphicsAPI::Vulkan;
    config.backend_specific_config = Mizu::VulkanSpecificConfiguration{};
    config.requirements = Mizu::Requirements{.graphics = true, .compute = true};

    REQUIRE(Mizu::Renderer::initialize(config));

    Mizu::Renderer::shutdown();
}

TEST_CASE("OpenGL initializes Graphics", "[Initialization]") {
    Mizu::RendererConfiguration config{};
    config.graphics_api = Mizu::GraphicsAPI::OpenGL;
    config.backend_specific_config = Mizu::OpenGLSpecificConfiguration{.create_context = true};
    config.requirements = Mizu::Requirements{.graphics = true, .compute = false};

    REQUIRE(Mizu::Renderer::initialize(config));

    Mizu::Renderer::shutdown();
}

TEST_CASE("OpenGL initializes Compute", "[Initialization]") {
    Mizu::RendererConfiguration config{};
    config.graphics_api = Mizu::GraphicsAPI::OpenGL;
    config.backend_specific_config = Mizu::OpenGLSpecificConfiguration{.create_context = true};
    config.requirements = Mizu::Requirements{.graphics = false, .compute = true};

    REQUIRE(Mizu::Renderer::initialize(config));

    Mizu::Renderer::shutdown();
}

TEST_CASE("OpenGL initializes Graphics and Compute", "[Initialization]") {
    Mizu::RendererConfiguration config{};
    config.graphics_api = Mizu::GraphicsAPI::OpenGL;
    config.backend_specific_config = Mizu::OpenGLSpecificConfiguration{.create_context = true};
    config.requirements = Mizu::Requirements{.graphics = true, .compute = true};

    REQUIRE(Mizu::Renderer::initialize(config));

    Mizu::Renderer::shutdown();
}