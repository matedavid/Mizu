#include "tests_common.h"

TEST_CASE("Framebuffer tests", "[Framebuffer]") {
    const auto& [api, backend_config] = GENERATE_GRAPHICS_APIS();

    Mizu::RendererConfiguration config{};
    config.graphics_api = api;
    config.backend_specific_config = backend_config;
    config.requirements = Mizu::Requirements{.graphics = true, .compute = true};

    REQUIRE(Mizu::Renderer::initialize(config));

    SECTION("Can create color and depth framebuffer") {
        Mizu::ImageDescription color_desc{};
        color_desc.width = 400;
        color_desc.height = 400;
        color_desc.format = Mizu::ImageFormat::RGBA8_SRGB;
        color_desc.attachment = true;

        const auto color_texture = Mizu::Texture2D::create(color_desc);

        Mizu::ImageDescription depth_desc{};
        depth_desc.width = 400;
        depth_desc.height = 400;
        depth_desc.format = Mizu::ImageFormat::D32_SFLOAT;
        depth_desc.attachment = true;

        const auto depth_texture = Mizu::Texture2D::create(depth_desc);

        Mizu::Framebuffer::Attachment color_attachment{};
        color_attachment.image = color_texture;
        color_attachment.load_operation = Mizu::LoadOperation::Clear;
        color_attachment.store_operation = Mizu::StoreOperation::Store;
        color_attachment.clear_value = glm::vec3(0.0f);

        Mizu::Framebuffer::Attachment depth_attachment{};
        depth_attachment.image = depth_texture;
        depth_attachment.load_operation = Mizu::LoadOperation::Clear;
        depth_attachment.store_operation = Mizu::StoreOperation::DontCare;

        Mizu::Framebuffer::Description framebuffer_desc{};
        framebuffer_desc.width = 400;
        framebuffer_desc.height = 400;
        framebuffer_desc.attachments = {color_attachment, depth_attachment};

        const auto framebuffer = Mizu::Framebuffer::create(framebuffer_desc);
        REQUIRE(framebuffer != nullptr);
    }

    SECTION("Can create depth only framebuffer") {
        Mizu::ImageDescription depth_desc{};
        depth_desc.width = 400;
        depth_desc.height = 400;
        depth_desc.format = Mizu::ImageFormat::D32_SFLOAT;
        depth_desc.attachment = true;

        const auto depth_texture = Mizu::Texture2D::create(depth_desc);

        Mizu::Framebuffer::Attachment depth_attachment{};
        depth_attachment.image = depth_texture;
        depth_attachment.load_operation = Mizu::LoadOperation::Clear;
        depth_attachment.store_operation = Mizu::StoreOperation::DontCare;

        Mizu::Framebuffer::Description framebuffer_desc{};
        framebuffer_desc.width = 400;
        framebuffer_desc.height = 400;
        framebuffer_desc.attachments = {depth_attachment};

        const auto framebuffer = Mizu::Framebuffer::create(framebuffer_desc);
        REQUIRE(framebuffer != nullptr);
    }

    SECTION("Can create two framebuffers with same depth texture") {
        Mizu::ImageDescription depth_desc{};
        depth_desc.width = 1920;
        depth_desc.height = 1080;
        depth_desc.format = Mizu::ImageFormat::D32_SFLOAT;
        depth_desc.attachment = true;

        const auto depth_texture = Mizu::Texture2D::create(depth_desc);

        // Framebuffer 1
        Mizu::Framebuffer::Attachment depth_attachment_1{};
        depth_attachment_1.image = depth_texture;
        depth_attachment_1.load_operation = Mizu::LoadOperation::Clear;
        depth_attachment_1.store_operation = Mizu::StoreOperation::Store;

        Mizu::Framebuffer::Description framebuffer_desc_1{};
        framebuffer_desc_1.width = 1920;
        framebuffer_desc_1.height = 1080;
        framebuffer_desc_1.attachments = {depth_attachment_1};

        const auto framebuffer_1 = Mizu::Framebuffer::create(framebuffer_desc_1);
        REQUIRE(framebuffer_1 != nullptr);

        // Framebuffer 2
        Mizu::Framebuffer::Attachment depth_attachment_2{};
        depth_attachment_2.image = depth_texture;
        depth_attachment_2.load_operation = Mizu::LoadOperation::Load;
        depth_attachment_2.store_operation = Mizu::StoreOperation::DontCare;

        Mizu::Framebuffer::Description framebuffer_desc_2{};
        framebuffer_desc_2.width = 1920;
        framebuffer_desc_2.height = 1080;
        framebuffer_desc_2.attachments = {depth_attachment_2};

        const auto framebuffer_2 = Mizu::Framebuffer::create(framebuffer_desc_2);
        REQUIRE(framebuffer_2 != nullptr);
    }

    Mizu::Renderer::shutdown();
}
