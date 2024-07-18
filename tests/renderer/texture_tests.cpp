#include "renderer_tests_common.h"

static bool image_description_matches(Mizu::ImageDescription desc, const std::shared_ptr<Mizu::Texture2D>& tex) {
    return desc.width == tex->get_width() && desc.height == tex->get_height() && desc.format == tex->get_format();
}

TEST_CASE("Texture2D tests", "[Texture2D]") {
    const auto& [api, backend_config] = GENERATE_GRAPHICS_APIS();

    Mizu::RendererConfiguration config{};
    config.graphics_api = api;
    config.backend_specific_config = backend_config;
    config.requirements = Mizu::Requirements{.graphics = true, .compute = true};

    REQUIRE(Mizu::Renderer::initialize(config));

    SECTION("Can create texture") {
        Mizu::ImageDescription desc{};
        desc.width = 100;
        desc.height = 400;
        desc.format = Mizu::ImageFormat::RGBA8_SRGB;
        desc.usage = Mizu::ImageUsageBits::Sampled;
        desc.sampling_options = Mizu::SamplingOptions{};

        const auto texture = Mizu::Texture2D::create(desc);
        REQUIRE(texture != nullptr);

        REQUIRE(image_description_matches(desc, texture));
    }

    SECTION("Can create depth texture") {
        Mizu::ImageDescription desc{};
        desc.width = 300;
        desc.height = 100;
        desc.format = Mizu::ImageFormat::D32_SFLOAT;
        desc.usage = Mizu::ImageUsageBits::Sampled;
        desc.sampling_options = Mizu::SamplingOptions{};

        const auto texture = Mizu::Texture2D::create(desc);
        REQUIRE(texture != nullptr);

        REQUIRE(image_description_matches(desc, texture));
    }

    SECTION("Can create image with mips") {
        Mizu::ImageDescription desc{};
        desc.width = 1920;
        desc.height = 1080;
        desc.format = Mizu::ImageFormat::RGBA8_SRGB;
        desc.usage = Mizu::ImageUsageBits::Sampled;
        desc.sampling_options = Mizu::SamplingOptions{};
        desc.generate_mips = true;

        const auto texture = Mizu::Texture2D::create(desc);
        REQUIRE(texture != nullptr);

        REQUIRE(image_description_matches(desc, texture));
    }

    SECTION("Can create image for storage usage with mips") {
        Mizu::ImageDescription desc{};
        desc.width = 400;
        desc.height = 400;
        desc.format = Mizu::ImageFormat::RGBA16_SFLOAT;
        desc.usage = Mizu::ImageUsageBits::Storage;
        desc.sampling_options = Mizu::SamplingOptions{};
        desc.generate_mips = true;

        const auto texture = Mizu::Texture2D::create(desc);
        REQUIRE(texture != nullptr);

        REQUIRE(image_description_matches(desc, texture));
    }

    Mizu::Renderer::shutdown();
}
