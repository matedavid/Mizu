#include <catch2/catch_all.hpp>
#include <Mizu/Mizu.h>

struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 uv;
};

// clang-format off
struct UniformBufferData {
    glm::vec4 a1;
    glm::vec3 a2; float _padding1;
    glm::vec4 a3;
};
// clang-format on

TEST_CASE("Vulkan Buffers", "[Buffers]") {
    Mizu::Configuration config{};
    config.graphics_api = Mizu::GraphicsAPI::Vulkan;
    config.requirements = Mizu::Requirements{.graphics = true, .compute = true};

    REQUIRE(Mizu::initialize(config));

    SECTION("Can create Vertex Buffer", "[VertexBuffer]") {
        const std::vector<Vertex> vertex_data = {
            {.pos = glm::vec3(0.0f), .normal = glm::vec3(1.0f), .uv = glm::vec2(2.0f)},
            {.pos = glm::vec3(-1.0f), .normal = glm::vec3(-2.0f), .uv = glm::vec2(0.5f, 0.4f)},
        };
        const auto vertex = Mizu::VertexBuffer::create(vertex_data);

        REQUIRE(vertex != nullptr);
        REQUIRE(vertex->count() == 2);
    }

    //    SECTION("Cannot create empty Vertex Buffer", "[VertexBuffer]") {
    //        const std::vector<Vertex> vertex_data{};
    //        const auto vertex = Mizu::VertexBuffer::create(vertex_data);
    //
    //        REQUIRE(vertex == nullptr);
    //    }

    SECTION("Can create Index Buffer", "[IndexBuffer]") {
        const auto index = Mizu::IndexBuffer::create({0, 1, 2, 3, 4, 5});

        REQUIRE(index != nullptr);
        REQUIRE(index->count() == 6);
    }

    //    SECTION("Cannot create empty Index Buffer", "[IndexBuffer]") {
    //        const auto index = Mizu::IndexBuffer::create({});
    //
    //        REQUIRE(index == nullptr);
    //    }

    SECTION("Can create Uniform Buffer", "[UniformBuffer]") {
        const auto uniform = Mizu::UniformBuffer::create<UniformBufferData>();
        uniform->update(UniformBufferData{});

        REQUIRE(uniform != nullptr);
        REQUIRE(uniform->size() == sizeof(UniformBufferData));
    }

    Mizu::shutdown();
}