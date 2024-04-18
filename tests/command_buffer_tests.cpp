#include <catch2/catch_all.hpp>
#include <Mizu/Mizu.h>

#include <future>
#include <thread>

TEST_CASE("Vulkan Command Buffer", "[CommandBuffer]") {
    Mizu::Configuration config{};
    config.graphics_api = Mizu::GraphicsAPI::Vulkan;
    config.requirements = Mizu::Requirements{.graphics = true, .compute = true};

    REQUIRE(Mizu::initialize(config));

    SECTION("Can create RenderCommandBuffer", "[CommandBuffer]") {
        const auto rcb = Mizu::RenderCommandBuffer::create();
        REQUIRE(rcb != nullptr);
    }

    SECTION("Can create ComputeCommandBuffer", "[CommandBuffer]") {
        const auto rcb = Mizu::ComputeCommandBuffer::create();
        REQUIRE(rcb != nullptr);
    }

    SECTION("RenderCommandBuffer signals fence correctly", "[CommandBuffer]") {
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

    Mizu::shutdown();
}