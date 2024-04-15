#include <catch2/catch_all.hpp>
#include <Mizu/Mizu.h>

#include "resources_manager.h"

TEST_CASE("Vulkan Graphics Pipeline", "[GraphicsPipeline]") {
    Mizu::Configuration config{};
    config.graphics_api = Mizu::GraphicsAPI::Vulkan;
    config.requirements = Mizu::Requirements{.graphics = true, .compute = false};

    REQUIRE(Mizu::initialize(config));

    SECTION("Can create GraphicsPipeline") {
        const auto vertex_path = ResourcesManager::get_resource_path("GraphicsShader_1.Vert.spv");
        const auto fragment_path = ResourcesManager::get_resource_path("GraphicsShader_1.Frag.spv");

        const auto shader = Mizu::Shader::create(vertex_path, fragment_path);
        REQUIRE(shader != nullptr);

        Mizu::ImageDescription texture_desc{};
        texture_desc.width = 100;
        texture_desc.height = 100;
        texture_desc.format = Mizu::ImageFormat::RGBA8_SRGB;
        texture_desc.attachment = true;

        const auto color_texture = Mizu::Texture2D::create(texture_desc);
        REQUIRE(color_texture != nullptr);

        Mizu::Framebuffer::Attachment color_attachment{};
        color_attachment.image = color_texture;

        Mizu::Framebuffer::Description framebuffer_desc{};
        framebuffer_desc.width = 100;
        framebuffer_desc.height = 100;
        framebuffer_desc.attachments = {color_attachment};

        const auto framebuffer = Mizu::Framebuffer::create(framebuffer_desc);
        REQUIRE(framebuffer != nullptr);

        Mizu::GraphicsPipeline::Description pipeline_desc{};
        pipeline_desc.shader = shader;
        pipeline_desc.target_framebuffer = framebuffer;

        const auto pipeline = Mizu::GraphicsPipeline::create(pipeline_desc);
        REQUIRE(pipeline != nullptr);
    }

    Mizu::shutdown();
}
