#include <catch2/catch_all.hpp>
#include <Mizu/Mizu.h>

static bool image_description_matches(Mizu::ImageDescription desc, const std::shared_ptr<Mizu::Texture2D>& tex) {
    return desc.width == tex->get_width() && desc.height == tex->get_height() && desc.format == tex->get_format();
}

TEST_CASE("Vulkan Texture2D", "[Texture]") {
    Mizu::Configuration config{};
    config.graphics_api = Mizu::GraphicsAPI::Vulkan;
    config.requirements = Mizu::Requirements{.graphics = true, .compute = true};

    REQUIRE(Mizu::initialize(config));

    SECTION("Can create texture", "[Texture2D]") {
        Mizu::ImageDescription desc{};
        desc.width = 100;
        desc.height = 400;
        desc.format = Mizu::ImageFormat::RGBA8_SRGB;
        desc.sampling_options = Mizu::SamplingOptions{};

        const auto texture = Mizu::Texture2D::create(desc);
        REQUIRE(texture != nullptr);

        REQUIRE(image_description_matches(desc, texture));
    }

    SECTION("Can create depth texture", "[Texture2D]") {
        Mizu::ImageDescription desc{};
        desc.width = 300;
        desc.height = 100;
        desc.format = Mizu::ImageFormat::D32_SFLOAT;
        desc.sampling_options = Mizu::SamplingOptions{};

        const auto texture = Mizu::Texture2D::create(desc);
        REQUIRE(texture != nullptr);

        REQUIRE(image_description_matches(desc, texture));
    }

    SECTION("Can create image with mips", "[Texture2D]") {
        Mizu::ImageDescription desc{};
        desc.width = 1920;
        desc.height = 1080;
        desc.format = Mizu::ImageFormat::RGBA8_SRGB;
        desc.sampling_options = Mizu::SamplingOptions{};
        desc.generate_mips = true;

        const auto texture = Mizu::Texture2D::create(desc);
        REQUIRE(texture != nullptr);

        REQUIRE(image_description_matches(desc, texture));
    }

    SECTION("Can create image for storage usage with mips", "[Texture2D]") {
        Mizu::ImageDescription desc{};
        desc.width = 400;
        desc.height = 400;
        desc.format = Mizu::ImageFormat::RGBA16_SFLOAT;
        desc.sampling_options = Mizu::SamplingOptions{};
        desc.generate_mips = true;
        desc.storage = true;

        const auto texture = Mizu::Texture2D::create(desc);
        REQUIRE(texture != nullptr);

        REQUIRE(image_description_matches(desc, texture));
    }

    Mizu::shutdown();
}