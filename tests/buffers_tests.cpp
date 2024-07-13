#include "tests_common.h"

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
    const auto [api, backend_config] = GENERATE_GRAPHICS_APIS();

    Mizu::RendererConfiguration config{};
    config.graphics_api = api;
    config.backend_specific_config = backend_config;
    config.requirements = Mizu::Requirements{.graphics = true, .compute = true};

    REQUIRE(Mizu::Renderer::initialize(config));

    SECTION("Can create Vertex Buffer") {
        const std::vector<Vertex> vertex_data = {
            {.pos = glm::vec3(0.0f), .normal = glm::vec3(1.0f), .uv = glm::vec2(2.0f)},
            {.pos = glm::vec3(-1.0f), .normal = glm::vec3(-2.0f), .uv = glm::vec2(0.5f, 0.4f)},
        };

        const std::vector<Mizu::VertexBuffer::Layout> layout = {
            {.type = Mizu::VertexBuffer::Layout::Type::Float, .count = 3, .normalized = false},
            {.type = Mizu::VertexBuffer::Layout::Type::Float, .count = 3, .normalized = false},
            {.type = Mizu::VertexBuffer::Layout::Type::Float, .count = 2, .normalized = false},
        };

        const auto vertex = Mizu::VertexBuffer::create(vertex_data, layout);

        REQUIRE(vertex != nullptr);
        REQUIRE(vertex->count() == 2);
    }

    //    SECTION("Cannot create empty Vertex Buffer", "[VertexBuffer]") {
    //        const std::vector<Vertex> vertex_data{};
    //        const auto vertex = Mizu::VertexBuffer::create(vertex_data);
    //
    //        REQUIRE(vertex == nullptr);
    //    }

    SECTION("Can create Index Buffer") {
        const auto index = Mizu::IndexBuffer::create({0, 1, 2, 3, 4, 5});

        REQUIRE(index != nullptr);
        REQUIRE(index->count() == 6);
    }

    //    SECTION("Cannot create empty Index Buffer", "[IndexBuffer]") {
    //        const auto index = Mizu::IndexBuffer::create({});
    //
    //        REQUIRE(index == nullptr);
    //    }

    SECTION("Can create Uniform Buffer") {
        const auto uniform = Mizu::UniformBuffer::create<UniformBufferData>();
        uniform->update(UniformBufferData{});

        REQUIRE(uniform != nullptr);
        REQUIRE(uniform->size() == sizeof(UniformBufferData));
    }

    Mizu::Renderer::shutdown();
}