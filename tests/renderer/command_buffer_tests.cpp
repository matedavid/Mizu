#include "renderer_tests_common.h"

#include <future>
#include <thread>

TEST_CASE("CommandBuffer Tests", "[CommandBuffer]") {
    const auto& [api, backend_config] = GENERATE_GRAPHICS_APIS();

    Mizu::RendererConfiguration config{};
    config.graphics_api = api;
    config.backend_specific_config = backend_config;
    config.requirements = Mizu::Requirements{.graphics = true, .compute = true};

    REQUIRE(Mizu::Renderer::initialize(config));

    SECTION("Can create RenderCommandBuffer") {
        const auto cb = Mizu::RenderCommandBuffer::create();
        REQUIRE(cb != nullptr);
    }

    // SECTION("Can create ComputeCommandBuffer", "[CommandBuffer]") {
    //     const auto cb = Mizu::ComputeCommandBuffer::create();
    //     REQUIRE(cb != nullptr);
    // }

    SECTION("RenderCommandBuffer signals fence correctly") {
        std::promise<bool> promise;
        std::future<bool> future = promise.get_future();
        auto t = std::thread([&]() {
            auto cb = Mizu::RenderCommandBuffer::create();

            auto signal_fence = Mizu::Fence::create();
            signal_fence->wait_for();

            cb->begin();
            cb->end();

            cb->submit({.signal_fence = signal_fence});

            signal_fence->wait_for();
            promise.set_value(true);
        });

        REQUIRE(future.wait_for(std::chrono::seconds(2)) == std::future_status::ready);
        t.join();
    }

    /*
    SECTION("ComputeCommandBuffer signals fence correctly", "[CommandBuffer]") {
        std::promise<bool> promise;
        std::future<bool> future = promise.get_future();
        auto t = std::thread([&]() {
            auto cb = Mizu::ComputeCommandBuffer::create();

            auto signal_fence = Mizu::Fence::create();
            signal_fence->wait_for();

            cb->begin();
            cb->end();

            cb->submit({.signal_fence = signal_fence});

            signal_fence->wait_for();
            promise.set_value(true);
        });

        REQUIRE(future.wait_for(std::chrono::seconds(2)) == std::future_status::ready);
        t.join();
    }
    */

    Mizu::Renderer::shutdown();
}