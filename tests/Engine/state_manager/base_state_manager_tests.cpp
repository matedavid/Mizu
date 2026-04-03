#include <catch2/catch_all.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>
#include <vector>

#include <glm/glm.hpp>

#include "state_manager/base_state_manager2.h"
#include "state_manager/base_state_manager2.inl.cpp"

using namespace Mizu;

struct TestStaticState
{
    uint32_t value = 0;
};

struct TestDynamicState
{
    float value = 0.0f;
    int32_t count = 0;
    bool enabled = false;

    bool has_changed(const TestDynamicState& other) const
    {
        return value != other.value || count != other.count || enabled != other.enabled;
    }

    TestDynamicState interpolate(const TestDynamicState& other, double alpha) const
    {
        const float a = static_cast<float>(alpha);

        const float interpolated_value = glm::mix(value, other.value, a);
        const int32_t interpolated_count =
            static_cast<int32_t>(glm::mix(static_cast<float>(count), static_cast<float>(other.count), a));
        const bool interpolated_enabled = (a >= 1.0f) ? other.enabled : enabled;

        return TestDynamicState{
            interpolated_value,
            interpolated_count,
            interpolated_enabled,
        };
    }
};

MIZU_STATE_MANAGER_CREATE_HANDLE(TestHandle);

struct TestConfig
{
    static constexpr uint64_t MaxNumHandles = 8;
    static constexpr bool Interpolate = true;
};

struct TestConfigNoInterp
{
    static constexpr uint64_t MaxNumHandles = 8;
    static constexpr bool Interpolate = false;
};

struct RecordedRenderEvent
{
    enum class Kind
    {
        Create,
        Update,
        Destroy,
    };

    Kind kind;
    uint64_t handle_idx;
    float value;
    int32_t count;
    bool enabled;
    uint64_t render_time_us;
};

class TestStateManagerHarness : public BaseStateManager2<TestStaticState, TestDynamicState, TestHandle, TestConfig>
{
  public:
    void begin_tick(uint64_t sim_time_us) { sim_begin_tick(TickUpdateState{sim_time_us}); }

    void end_tick() { sim_end_tick(); }

    TestHandle create(uint32_t static_value, const TestDynamicState& dynamic_state)
    {
        return sim_create(TestStaticState{static_value}, dynamic_state);
    }

    void update(TestHandle handle, const TestDynamicState& dynamic_state) { sim_update(handle, dynamic_state); }

    void destroy(TestHandle handle) { sim_destroy(handle); }

    void apply_render(uint64_t render_time_us)
    {
        m_last_render_time_us = render_time_us;
        rend_apply_updates(FrameUpdateState{render_time_us});
    }

    void clear_events() { m_events.clear(); }

    std::string_view get_identifier() const override { return "TestStateManager"; }

    const std::vector<RecordedRenderEvent>& events() const { return m_events; }

  protected:
    void rend_on_create(TestHandle handle, TestStaticState, TestDynamicState dynamic_state) override
    {
        m_events.push_back(
            RecordedRenderEvent{
                RecordedRenderEvent::Kind::Create,
                handle.get_internal_id(),
                dynamic_state.value,
                dynamic_state.count,
                dynamic_state.enabled,
                m_last_render_time_us});
    }

    void rend_on_update(TestHandle handle, TestDynamicState dynamic_state) override
    {
        m_events.push_back(
            RecordedRenderEvent{
                RecordedRenderEvent::Kind::Update,
                handle.get_internal_id(),
                dynamic_state.value,
                dynamic_state.count,
                dynamic_state.enabled,
                m_last_render_time_us});
    }

    void rend_on_destroy(TestHandle handle) override
    {
        m_events.push_back(
            RecordedRenderEvent{
                RecordedRenderEvent::Kind::Destroy, handle.get_internal_id(), 0.0f, 0, false, m_last_render_time_us});
    }

  private:
    uint64_t m_last_render_time_us = 0;
    std::vector<RecordedRenderEvent> m_events;
};

class TestStateManagerNoInterpHarness
    : public BaseStateManager2<TestStaticState, TestDynamicState, TestHandle, TestConfigNoInterp>
{
  public:
    void begin_tick(uint64_t sim_time_us) { sim_begin_tick(TickUpdateState{sim_time_us}); }

    void end_tick() { sim_end_tick(); }

    TestHandle create(uint32_t static_value, const TestDynamicState& dynamic_state)
    {
        return sim_create(TestStaticState{static_value}, dynamic_state);
    }

    void update(TestHandle handle, const TestDynamicState& dynamic_state) { sim_update(handle, dynamic_state); }

    void destroy(TestHandle handle) { sim_destroy(handle); }

    void apply_render(uint64_t render_time_us)
    {
        m_last_render_time_us = render_time_us;
        rend_apply_updates(FrameUpdateState{render_time_us});
    }

    void clear_events() { m_events.clear(); }

    std::string_view get_identifier() const override { return "TestStateManagerNoInterp"; }

    const std::vector<RecordedRenderEvent>& events() const { return m_events; }

  protected:
    void rend_on_create(TestHandle handle, TestStaticState, TestDynamicState dynamic_state) override
    {
        m_events.push_back(
            RecordedRenderEvent{
                RecordedRenderEvent::Kind::Create,
                handle.get_internal_id(),
                dynamic_state.value,
                dynamic_state.count,
                dynamic_state.enabled,
                m_last_render_time_us});
    }

    void rend_on_update(TestHandle handle, TestDynamicState dynamic_state) override
    {
        m_events.push_back(
            RecordedRenderEvent{
                RecordedRenderEvent::Kind::Update,
                handle.get_internal_id(),
                dynamic_state.value,
                dynamic_state.count,
                dynamic_state.enabled,
                m_last_render_time_us});
    }

    void rend_on_destroy(TestHandle handle) override
    {
        m_events.push_back(
            RecordedRenderEvent{
                RecordedRenderEvent::Kind::Destroy, handle.get_internal_id(), 0.0f, 0, false, m_last_render_time_us});
    }

  private:
    uint64_t m_last_render_time_us = 0;
    std::vector<RecordedRenderEvent> m_events;
};

TEST_CASE("BaseStateManager2 has correct identifier", "[StateManager]")
{
    TestStateManagerHarness harness;
    REQUIRE(harness.get_identifier() == "TestStateManager");
}

TEST_CASE("BaseStateManager2 only publishes non-empty sim ticks", "[StateManager]")
{
    TestStateManagerHarness harness;

    harness.begin_tick(100);
    harness.end_tick();

    harness.apply_render(1000);
    REQUIRE(harness.events().empty());

    harness.begin_tick(200);
    const TestHandle handle = harness.create(1, TestDynamicState{5.0f, 2, true});
    harness.end_tick();

    harness.apply_render(199);
    REQUIRE(harness.events().empty());

    harness.apply_render(200);
    REQUIRE(harness.events().size() == 1);
    REQUIRE(harness.events()[0].kind == RecordedRenderEvent::Kind::Create);
    REQUIRE(harness.events()[0].handle_idx == handle.get_internal_id());
    REQUIRE(harness.events()[0].value == Catch::Approx(5.0f));
    REQUIRE(harness.events()[0].count == 2);
    REQUIRE(harness.events()[0].enabled == true);
}

TEST_CASE("BaseStateManager2 coalesces create then update into one create", "[StateManager]")
{
    TestStateManagerHarness harness;

    harness.begin_tick(100);
    const TestHandle handle = harness.create(7, TestDynamicState{1.0f, 10, false});
    harness.update(handle, TestDynamicState{9.0f, 42, true});
    harness.end_tick();

    harness.apply_render(100);
    REQUIRE(harness.events().size() == 1);
    REQUIRE(harness.events()[0].kind == RecordedRenderEvent::Kind::Create);
    REQUIRE(harness.events()[0].handle_idx == handle.get_internal_id());
    REQUIRE(harness.events()[0].value == Catch::Approx(9.0f));
    REQUIRE(harness.events()[0].count == 42);
    REQUIRE(harness.events()[0].enabled == true);
}

TEST_CASE("BaseStateManager2 drops create then destroy in same tick", "[StateManager]")
{
    TestStateManagerHarness harness;

    harness.begin_tick(100);
    const TestHandle handle = harness.create(3, TestDynamicState{2.0f, 5, true});
    harness.destroy(handle);
    harness.end_tick();

    harness.apply_render(1000);
    REQUIRE(harness.events().empty());
}

TEST_CASE("BaseStateManager2 coalesces update then destroy into destroy", "[StateManager]")
{
    TestStateManagerHarness harness;

    harness.begin_tick(100);
    const TestHandle handle = harness.create(0, TestDynamicState{1.0f, 1, true});
    harness.end_tick();

    harness.apply_render(100);
    REQUIRE(harness.events().size() == 1);
    REQUIRE(harness.events()[0].kind == RecordedRenderEvent::Kind::Create);
    REQUIRE(harness.events()[0].value == Catch::Approx(1.0f));
    REQUIRE(harness.events()[0].count == 1);
    REQUIRE(harness.events()[0].enabled == true);

    harness.clear_events();

    harness.begin_tick(200);
    harness.update(handle, TestDynamicState{4.0f, 9, false});
    harness.destroy(handle);
    harness.end_tick();

    harness.apply_render(150);
    REQUIRE(harness.events().empty());

    harness.apply_render(200);
    REQUIRE(harness.events().size() == 1);
    REQUIRE(harness.events()[0].kind == RecordedRenderEvent::Kind::Destroy);
    REQUIRE(harness.events()[0].handle_idx == handle.get_internal_id());
}

TEST_CASE("BaseStateManager2 coalesces repeated updates in one tick", "[StateManager]")
{
    TestStateManagerHarness harness;

    harness.begin_tick(100);
    const TestHandle handle = harness.create(0, TestDynamicState{1.0f, 1, false});
    harness.end_tick();
    harness.apply_render(100);
    harness.clear_events();

    harness.begin_tick(200);
    harness.update(handle, TestDynamicState{3.0f, 3, false});
    harness.update(handle, TestDynamicState{7.0f, 7, true});
    harness.end_tick();

    harness.apply_render(200);
    REQUIRE(harness.events().size() == 1);
    REQUIRE(harness.events()[0].kind == RecordedRenderEvent::Kind::Update);
    REQUIRE(harness.events()[0].handle_idx == handle.get_internal_id());
    REQUIRE(harness.events()[0].value == Catch::Approx(7.0f));
    REQUIRE(harness.events()[0].count == 7);
    REQUIRE(harness.events()[0].enabled == true);
}

TEST_CASE("BaseStateManager2 only emits updates for handles touched by target tick", "[StateManager]")
{
    TestStateManagerHarness harness;

    harness.begin_tick(100);
    const TestHandle handle_a = harness.create(0, TestDynamicState{0.0f, 0, false});
    const TestHandle handle_b = harness.create(1, TestDynamicState{2.0f, 2, true});
    harness.end_tick();
    harness.apply_render(100);
    harness.clear_events();

    harness.begin_tick(200);
    harness.update(handle_a, TestDynamicState{5.0f, 5, true});
    harness.end_tick();

    harness.apply_render(200);
    REQUIRE(harness.events().size() == 1);
    REQUIRE(harness.events()[0].kind == RecordedRenderEvent::Kind::Update);
    REQUIRE(harness.events()[0].handle_idx == handle_a.get_internal_id());
    REQUIRE(harness.events()[0].value == Catch::Approx(5.0f));
    REQUIRE(harness.events()[0].count == 5);
    REQUIRE(harness.events()[0].enabled == true);

    REQUIRE(harness.events()[0].handle_idx != handle_b.get_internal_id());
}

TEST_CASE("BaseStateManager2 consumes updates in tick order", "[StateManager]")
{
    TestStateManagerHarness harness;

    harness.begin_tick(100);
    const TestHandle handle = harness.create(0, TestDynamicState{0.0f, 0, false});
    harness.end_tick();
    harness.apply_render(100);
    harness.clear_events();

    harness.begin_tick(200);
    harness.update(handle, TestDynamicState{10.0f, 10, false});
    harness.end_tick();

    harness.begin_tick(300);
    harness.update(handle, TestDynamicState{20.0f, 20, true});
    harness.end_tick();

    harness.apply_render(300);
    REQUIRE(harness.events().size() == 1);
    REQUIRE(harness.events()[0].kind == RecordedRenderEvent::Kind::Update);
    REQUIRE(harness.events()[0].value == Catch::Approx(10.0f));
    REQUIRE(harness.events()[0].count == 10);
    REQUIRE(harness.events()[0].enabled == false);

    harness.clear_events();

    harness.apply_render(300);
    REQUIRE(harness.events().size() == 1);
    REQUIRE(harness.events()[0].kind == RecordedRenderEvent::Kind::Update);
    REQUIRE(harness.events()[0].value == Catch::Approx(20.0f));
    REQUIRE(harness.events()[0].count == 20);
    REQUIRE(harness.events()[0].enabled == true);
}

TEST_CASE("BaseStateManager2 interpolates update before full consume", "[StateManager]")
{
    TestStateManagerHarness harness;

    harness.begin_tick(100);
    const TestHandle handle = harness.create(0, TestDynamicState{0.0f, 4, false});
    harness.end_tick();

    harness.apply_render(100);
    harness.clear_events();

    harness.begin_tick(200);
    harness.update(handle, TestDynamicState{10.0f, 14, true});
    harness.end_tick();

    harness.apply_render(150);
    REQUIRE(harness.events().size() == 1);
    REQUIRE(harness.events()[0].kind == RecordedRenderEvent::Kind::Update);
    REQUIRE(harness.events()[0].handle_idx == handle.get_internal_id());
    REQUIRE(harness.events()[0].value == Catch::Approx(5.0f));
    REQUIRE(harness.events()[0].count == 9);
    REQUIRE(harness.events()[0].enabled == false);

    harness.clear_events();

    harness.apply_render(200);
    REQUIRE(harness.events().size() == 1);
    REQUIRE(harness.events()[0].kind == RecordedRenderEvent::Kind::Update);
    REQUIRE(harness.events()[0].value == Catch::Approx(10.0f));
    REQUIRE(harness.events()[0].count == 14);
    REQUIRE(harness.events()[0].enabled == true);
}

TEST_CASE("BaseStateManager2 no-interpolate mode does not emit midpoint update", "[StateManager]")
{
    TestStateManagerNoInterpHarness harness;

    harness.begin_tick(100);
    const TestHandle handle = harness.create(0, TestDynamicState{0.0f, 4, false});
    harness.end_tick();

    harness.apply_render(100);
    harness.clear_events();

    harness.begin_tick(200);
    harness.update(handle, TestDynamicState{10.0f, 14, true});
    harness.end_tick();

    harness.apply_render(150);
    REQUIRE(harness.events().empty());

    harness.apply_render(200);
    REQUIRE(harness.events().size() == 1);
    REQUIRE(harness.events()[0].kind == RecordedRenderEvent::Kind::Update);
    REQUIRE(harness.events()[0].handle_idx == handle.get_internal_id());
    REQUIRE(harness.events()[0].value == Catch::Approx(10.0f));
    REQUIRE(harness.events()[0].count == 14);
    REQUIRE(harness.events()[0].enabled == true);
}

TEST_CASE("BaseStateManager2 no-interpolate mode commits ordered exact snapshots", "[StateManager]")
{
    TestStateManagerNoInterpHarness harness;

    harness.begin_tick(100);
    const TestHandle handle = harness.create(0, TestDynamicState{0.0f, 0, false});
    harness.end_tick();
    harness.apply_render(100);
    harness.clear_events();

    harness.begin_tick(200);
    harness.update(handle, TestDynamicState{10.0f, 10, false});
    harness.end_tick();

    harness.begin_tick(300);
    harness.update(handle, TestDynamicState{20.0f, 20, true});
    harness.end_tick();

    harness.apply_render(250);
    REQUIRE(harness.events().size() == 1);
    REQUIRE(harness.events()[0].kind == RecordedRenderEvent::Kind::Update);
    REQUIRE(harness.events()[0].value == Catch::Approx(10.0f));
    REQUIRE(harness.events()[0].count == 10);
    REQUIRE(harness.events()[0].enabled == false);

    harness.clear_events();

    harness.apply_render(300);
    REQUIRE(harness.events().size() == 1);
    REQUIRE(harness.events()[0].kind == RecordedRenderEvent::Kind::Update);
    REQUIRE(harness.events()[0].value == Catch::Approx(20.0f));
    REQUIRE(harness.events()[0].count == 20);
    REQUIRE(harness.events()[0].enabled == true);
}

TEST_CASE("BaseStateManager2 no-interpolate mode keeps create and destroy full-consume boundaries", "[StateManager]")
{
    TestStateManagerNoInterpHarness harness;

    harness.begin_tick(100);
    const TestHandle handle = harness.create(0, TestDynamicState{1.0f, 1, true});
    harness.end_tick();

    harness.apply_render(99);
    REQUIRE(harness.events().empty());

    harness.apply_render(100);
    REQUIRE(harness.events().size() == 1);
    REQUIRE(harness.events()[0].kind == RecordedRenderEvent::Kind::Create);
    REQUIRE(harness.events()[0].handle_idx == handle.get_internal_id());
    REQUIRE(harness.events()[0].value == Catch::Approx(1.0f));
    REQUIRE(harness.events()[0].count == 1);
    REQUIRE(harness.events()[0].enabled == true);

    harness.clear_events();

    harness.begin_tick(200);
    harness.destroy(handle);
    harness.end_tick();

    harness.apply_render(150);
    REQUIRE(harness.events().empty());

    harness.apply_render(200);
    REQUIRE(harness.events().size() == 1);
    REQUIRE(harness.events()[0].kind == RecordedRenderEvent::Kind::Destroy);
    REQUIRE(harness.events()[0].handle_idx == handle.get_internal_id());
}

TEST_CASE("BaseStateManager2 rend_get_dynamic_state tracks interpolated applied state", "[StateManager]")
{
    TestStateManagerHarness harness;

    harness.begin_tick(100);
    const TestHandle handle = harness.create(1, TestDynamicState{0.0f, 4, false});
    harness.end_tick();
    harness.apply_render(100);

    harness.begin_tick(200);
    harness.update(handle, TestDynamicState{10.0f, 14, true});
    harness.end_tick();

    harness.apply_render(150);
    const TestDynamicState& mid_state = harness.rend_get_dynamic_state(handle);
    REQUIRE(mid_state.value == Catch::Approx(5.0f));
    REQUIRE(mid_state.count == 9);
    REQUIRE(mid_state.enabled == false);

    harness.apply_render(200);
    const TestDynamicState& end_state = harness.rend_get_dynamic_state(handle);
    REQUIRE(end_state.value == Catch::Approx(10.0f));
    REQUIRE(end_state.count == 14);
    REQUIRE(end_state.enabled == true);
}

TEST_CASE("BaseStateManager2 rend_get_dynamic_state in no-interpolate mode updates only on full consume", "[StateManager]")
{
    TestStateManagerNoInterpHarness harness;

    harness.begin_tick(100);
    const TestHandle handle = harness.create(1, TestDynamicState{0.0f, 4, false});
    harness.end_tick();
    harness.apply_render(100);

    harness.begin_tick(200);
    harness.update(handle, TestDynamicState{10.0f, 14, true});
    harness.end_tick();

    harness.apply_render(150);
    const TestDynamicState& mid_state = harness.rend_get_dynamic_state(handle);
    REQUIRE(mid_state.value == Catch::Approx(0.0f));
    REQUIRE(mid_state.count == 4);
    REQUIRE(mid_state.enabled == false);

    harness.apply_render(200);
    const TestDynamicState& end_state = harness.rend_get_dynamic_state(handle);
    REQUIRE(end_state.value == Catch::Approx(10.0f));
    REQUIRE(end_state.count == 14);
    REQUIRE(end_state.enabled == true);
}

TEST_CASE("BaseStateManager2 get_static_state returns created static state", "[StateManager]")
{
    TestStateManagerHarness harness;

    harness.begin_tick(100);
    const TestHandle handle = harness.create(77, TestDynamicState{1.0f, 1, true});
    harness.end_tick();

    REQUIRE(harness.get_static_state(handle).value == 77);
}

TEST_CASE("BaseStateManager2 get_static_state resets after destroy reclaim pass", "[StateManager]")
{
    TestStateManagerHarness harness;

    harness.begin_tick(100);
    const TestHandle handle = harness.create(77, TestDynamicState{1.0f, 1, true});
    harness.end_tick();
    harness.apply_render(100);

    harness.begin_tick(200);
    harness.destroy(handle);
    harness.end_tick();
    harness.apply_render(200);

    harness.begin_tick(300);
    harness.end_tick();

    REQUIRE(harness.get_static_state(handle).value == 0);
}

TEST_CASE("BaseStateManager2 static state persists until next sim_begin_tick reclaim", "[StateManager]")
{
    TestStateManagerHarness harness;

    harness.begin_tick(100);
    const TestHandle handle = harness.create(77, TestDynamicState{1.0f, 1, true});
    harness.end_tick();
    harness.apply_render(100);

    harness.begin_tick(200);
    harness.destroy(handle);
    harness.end_tick();
    harness.apply_render(200);

    REQUIRE(harness.get_static_state(handle).value == 77);

    harness.begin_tick(300);
    harness.end_tick();

    REQUIRE(harness.get_static_state(handle).value == 0);
}

TEST_CASE("BaseStateManager2 reuses ring slots without stale handle ticks", "[StateManager]")
{
    TestStateManagerHarness harness;

    harness.begin_tick(100);
    const TestHandle handle = harness.create(1, TestDynamicState{0.0f, 0, false});
    harness.end_tick();
    harness.apply_render(100);
    harness.clear_events();

    constexpr uint64_t TickStepUs = 10;
    const uint64_t total_ticks = MaxTicksAhead + 15;

    for (uint64_t i = 1; i <= total_ticks; ++i)
    {
        harness.begin_tick(100 + i * TickStepUs);
        harness.update(handle, TestDynamicState{static_cast<float>(i), static_cast<int32_t>(i), (i % 2u) == 0u});
        harness.end_tick();

        harness.apply_render(100 + i * TickStepUs);

        REQUIRE(harness.events().size() == 1);
        REQUIRE(harness.events()[0].kind == RecordedRenderEvent::Kind::Update);
        REQUIRE(harness.events()[0].value == Catch::Approx(static_cast<float>(i)));
        REQUIRE(harness.events()[0].count == static_cast<int32_t>(i));
        REQUIRE(harness.events()[0].enabled == ((i % 2u) == 0u));

        harness.clear_events();
    }
}

TEST_CASE("BaseStateManager2 ignores stale entries beyond tick valid prefix on slot reuse", "[StateManager]")
{
    TestStateManagerHarness harness;

    harness.begin_tick(100);
    const TestHandle handle_a = harness.create(1, TestDynamicState{0.0f, 0, false});
    const TestHandle handle_b = harness.create(2, TestDynamicState{10.0f, 10, true});
    harness.end_tick();
    harness.apply_render(100);
    harness.clear_events();

    // Tick 2: write two updates into this slot lineage.
    harness.begin_tick(200);
    harness.update(handle_a, TestDynamicState{1.0f, 1, false});
    harness.update(handle_b, TestDynamicState{11.0f, 11, true});
    harness.end_tick();
    harness.apply_render(200);
    harness.clear_events();

    // Advance to tick 11 so the same ring slot as tick 2 gets reused with only one entry.
    for (uint64_t tick = 3; tick <= 10; ++tick)
    {
        const uint64_t sim_time = tick * 100;

        harness.begin_tick(sim_time);
        harness.update(
            handle_a, TestDynamicState{static_cast<float>(tick), static_cast<int32_t>(tick), (tick % 2u) == 0u});
        harness.end_tick();

        harness.apply_render(sim_time);
        harness.clear_events();
    }

    harness.begin_tick(1100);
    harness.update(handle_a, TestDynamicState{11.0f, 11, false});
    harness.end_tick();

    harness.apply_render(1100);
    REQUIRE(harness.events().size() == 1);
    REQUIRE(harness.events()[0].kind == RecordedRenderEvent::Kind::Update);
    REQUIRE(harness.events()[0].handle_idx == handle_a.get_internal_id());
    REQUIRE(harness.events()[0].value == Catch::Approx(11.0f));
    REQUIRE(harness.events()[0].count == 11);
    REQUIRE(harness.events()[0].enabled == false);
}

/*
TEST_CASE("BaseStateManager2 catches up multiple ticks in a single apply_render call", "[StateManager]")
{
    TestStateManagerHarness harness;

    harness.begin_tick(100);
    const TestHandle handle = harness.create(0, TestDynamicState{0.0f, 0, false});
    harness.end_tick();
    harness.apply_render(100);
    harness.clear_events();

    harness.begin_tick(200);
    harness.update(handle, TestDynamicState{10.0f, 10, false});
    harness.end_tick();

    harness.begin_tick(300);
    harness.update(handle, TestDynamicState{20.0f, 20, true});
    harness.end_tick();

    // Single apply_render at t=300 should fully consume both tick 2 (t=200) and tick 3 (t=300).
    harness.apply_render(300);

    REQUIRE(harness.events().size() == 2);
    REQUIRE(harness.events()[0].kind == RecordedRenderEvent::Kind::Update);
    REQUIRE(harness.events()[0].value == Catch::Approx(10.0f));
    REQUIRE(harness.events()[0].count == 10);
    REQUIRE(harness.events()[0].enabled == false);
    REQUIRE(harness.events()[1].kind == RecordedRenderEvent::Kind::Update);
    REQUIRE(harness.events()[1].value == Catch::Approx(20.0f));
    REQUIRE(harness.events()[1].count == 20);
    REQUIRE(harness.events()[1].enabled == true);
}
*/

TEST_CASE("BaseStateManager2 reclaims handle IDs after destroy tick is fully consumed", "[StateManager]")
{
    TestStateManagerHarness harness;

    for (uint32_t cycle = 0; cycle < TestConfig::MaxNumHandles; ++cycle)
    {
        const uint64_t t_create = static_cast<uint64_t>(cycle * 2 + 1) * 100;
        const uint64_t t_destroy = static_cast<uint64_t>(cycle * 2 + 2) * 100;

        harness.begin_tick(t_create);
        const TestHandle h =
            harness.create(cycle, TestDynamicState{static_cast<float>(cycle), static_cast<int32_t>(cycle), false});
        harness.end_tick();
        harness.apply_render(t_create);
        harness.clear_events();

        harness.begin_tick(t_destroy);
        harness.destroy(h);
        harness.end_tick();
        harness.apply_render(t_destroy);
        harness.clear_events();
    }

    // After all MaxNumHandles cycles are done, at least one ID must be available for reuse.
    const uint64_t t_new = static_cast<uint64_t>(TestConfig::MaxNumHandles * 2 + 1) * 100;
    harness.begin_tick(t_new);
    const TestHandle reused = harness.create(99, TestDynamicState{99.0f, 99, true});
    harness.end_tick();

    REQUIRE(reused.is_valid());

    harness.apply_render(t_new);
    REQUIRE(harness.events().size() == 1);
    REQUIRE(harness.events()[0].kind == RecordedRenderEvent::Kind::Create);
    REQUIRE(harness.events()[0].value == Catch::Approx(99.0f));
    REQUIRE(harness.events()[0].count == 99);
    REQUIRE(harness.events()[0].enabled == true);
}

TEST_CASE("BaseStateManager2 waits when sim gets MaxTicksAhead of render", "[StateManager]")
{
    using namespace std::chrono_literals;

    TestStateManagerHarness harness;

    std::atomic<uint32_t> published_ticks = 0;
    std::atomic<bool> overflow_tick_started = false;
    std::atomic<bool> overflow_tick_finished = false;

    std::thread sim_thread([&]() {
        harness.begin_tick(100);
        const TestHandle handle = harness.create(0, TestDynamicState{0.0f, 0, false});
        harness.end_tick();
        published_ticks.store(1, std::memory_order_release);

        for (uint64_t tick = 2; tick <= MaxTicksAhead; ++tick)
        {
            harness.begin_tick(tick * 100);
            harness.update(handle, TestDynamicState{static_cast<float>(tick), static_cast<int32_t>(tick), false});
            harness.end_tick();
            published_ticks.store(static_cast<uint32_t>(tick), std::memory_order_release);
        }

        overflow_tick_started.store(true, std::memory_order_release);

        harness.begin_tick((MaxTicksAhead + 1) * 100);
        harness.update(
            handle,
            TestDynamicState{static_cast<float>(MaxTicksAhead + 1), static_cast<int32_t>(MaxTicksAhead + 1), true});
        harness.end_tick();

        overflow_tick_finished.store(true, std::memory_order_release);
    });

    const auto produced_deadline = std::chrono::steady_clock::now() + 1s;
    while (published_ticks.load(std::memory_order_acquire) != MaxTicksAhead)
    {
        if (std::chrono::steady_clock::now() >= produced_deadline)
        {
            sim_thread.join();
            FAIL("Sim did not publish MaxTicksAhead ticks in time");
        }

        std::this_thread::yield();
    }

    const auto started_deadline = std::chrono::steady_clock::now() + 1s;
    while (!overflow_tick_started.load(std::memory_order_acquire))
    {
        if (std::chrono::steady_clock::now() >= started_deadline)
        {
            sim_thread.join();
            FAIL("Sim did not reach the overflow tick in time");
        }

        std::this_thread::yield();
    }

    std::this_thread::sleep_for(10ms);
    REQUIRE(overflow_tick_finished.load(std::memory_order_acquire) == false);

    harness.apply_render(100);

    const auto resumed_deadline = std::chrono::steady_clock::now() + 1s;
    while (!overflow_tick_finished.load(std::memory_order_acquire))
    {
        if (std::chrono::steady_clock::now() >= resumed_deadline)
        {
            sim_thread.join();
            FAIL("Sim did not resume after render consumed one tick");
        }

        std::this_thread::yield();
    }

    sim_thread.join();
    REQUIRE(published_ticks.load(std::memory_order_acquire) == MaxTicksAhead);
    REQUIRE(overflow_tick_started.load(std::memory_order_acquire) == true);
    REQUIRE(overflow_tick_finished.load(std::memory_order_acquire) == true);
}
