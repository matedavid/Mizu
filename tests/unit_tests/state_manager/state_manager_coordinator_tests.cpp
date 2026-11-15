#include <catch2/catch_all.hpp>

#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

#include "state_manager/state_manager.h"
#include "state_manager/state_manager_coordinator.h"

using namespace Mizu;

class MockStateManager : public IStateManager
{
  public:
    MockStateManager(std::string_view identifier, std::vector<std::string_view>& order)
        : m_identifier(identifier)
        , m_order(order)
    {
    }

    void sim_begin_tick(const TickUpdateState& /*state*/) override { m_order.push_back(m_identifier); }

    void sim_end_tick() override {}

    void rend_begin_apply_updates() override {}
    uint64_t rend_get_next_tick_time_us() const override { return NO_PENDING_TICK_TIME; }
    void rend_consume_ticks_until(uint64_t /*sim_time_us*/) override {}
    void rend_interpolate(const FrameUpdateState& /*state*/) override {}

    std::string_view get_identifier() const override { return m_identifier; }

  private:
    std::string_view m_identifier;

    std::vector<std::string_view>& m_order;
};

struct RendCall
{
    std::string_view identifier;
    std::string_view kind;
    uint64_t time_us;

    bool operator==(const RendCall&) const = default;
};

class TickedMockStateManager : public IStateManager
{
  public:
    TickedMockStateManager(std::string_view identifier, std::vector<uint64_t> tick_times_us, std::vector<RendCall>& calls)
        : m_identifier(identifier)
        , m_tick_times_us(std::move(tick_times_us))
        , m_calls(calls)
    {
    }

    void sim_begin_tick(const TickUpdateState& /*state*/) override {}
    void sim_end_tick() override {}

    void rend_begin_apply_updates() override { m_calls.push_back({m_identifier, "begin", 0}); }

    uint64_t rend_get_next_tick_time_us() const override
    {
        if (m_next_tick >= m_tick_times_us.size())
            return NO_PENDING_TICK_TIME;

        return m_tick_times_us[m_next_tick];
    }

    void rend_consume_ticks_until(uint64_t sim_time_us) override
    {
        while (m_next_tick < m_tick_times_us.size())
        {
            const uint64_t tick_time_us = m_tick_times_us[m_next_tick];
            if (tick_time_us > sim_time_us)
                break;

            m_calls.push_back({m_identifier, "consume", tick_time_us});
            m_next_tick += 1;
        }
    }

    void rend_interpolate(const FrameUpdateState& state) override
    {
        m_calls.push_back({m_identifier, "interpolate", state.render_time_us});
    }

    std::string_view get_identifier() const override { return m_identifier; }

  private:
    std::string_view m_identifier;
    std::vector<uint64_t> m_tick_times_us;
    size_t m_next_tick = 0;

    std::vector<RendCall>& m_calls;
};

static std::vector<RendCall> filter_calls(const std::vector<RendCall>& calls, std::string_view kind)
{
    std::vector<RendCall> filtered;
    for (const RendCall& call : calls)
    {
        if (call.kind == kind)
            filtered.push_back(call);
    }

    return filtered;
}

static size_t index_of(const std::vector<std::string_view>& order, std::string_view id)
{
    for (size_t i = 0; i < order.size(); ++i)
    {
        if (order[i] == id)
            return i;
    }

    FAIL("identifier '" << id << "' not found in call order");
}

TEST_CASE("Registering without dependencies orders state managers in order of insertion", "[StateManagerCoordinator]")
{
    std::vector<std::string_view> order;

    MockStateManager manager_a("A", order);
    MockStateManager manager_b("B", order);
    MockStateManager manager_c("C", order);

    StateManagerCoordinator coordinator{};

    coordinator.register_state_manager(StateManagerRegistrationBuilder::begin(&manager_a));
    coordinator.register_state_manager(StateManagerRegistrationBuilder::begin(&manager_b));
    coordinator.register_state_manager(StateManagerRegistrationBuilder::begin(&manager_c));

    coordinator.sim_begin_tick(TickUpdateState{});

    REQUIRE(order.size() == 3);
    REQUIRE(order[0] == "A");
    REQUIRE(order[1] == "B");
    REQUIRE(order[2] == "C");
}

TEST_CASE("Linear dependency A -> B -> C is respected", "[StateManagerCoordinator]")
{
    std::vector<std::string_view> order;

    MockStateManager manager_a("A", order);
    MockStateManager manager_b("B", order);
    MockStateManager manager_c("C", order);

    StateManagerCoordinator coordinator{};

    coordinator.register_state_manager(StateManagerRegistrationBuilder::begin(&manager_a));

    auto builder_b = StateManagerRegistrationBuilder::begin(&manager_b);
    builder_b.depends_on(&manager_a);
    coordinator.register_state_manager(builder_b);

    auto builder_c = StateManagerRegistrationBuilder::begin(&manager_c);
    builder_c.depends_on(&manager_b);
    coordinator.register_state_manager(builder_c);

    coordinator.sim_begin_tick(TickUpdateState{});

    REQUIRE(order.size() == 3);
    REQUIRE(index_of(order, "A") < index_of(order, "B"));
    REQUIRE(index_of(order, "B") < index_of(order, "C"));
}

TEST_CASE("Diamond dependency A -> B,C -> D is respected", "[StateManagerCoordinator]")
{
    std::vector<std::string_view> order;

    MockStateManager manager_a("A", order);
    MockStateManager manager_b("B", order);
    MockStateManager manager_c("C", order);
    MockStateManager manager_d("D", order);

    StateManagerCoordinator coordinator{};

    coordinator.register_state_manager(StateManagerRegistrationBuilder::begin(&manager_a));

    auto builder_b = StateManagerRegistrationBuilder::begin(&manager_b);
    builder_b.depends_on(&manager_a);
    coordinator.register_state_manager(builder_b);

    auto builder_c = StateManagerRegistrationBuilder::begin(&manager_c);
    builder_c.depends_on(&manager_a);
    coordinator.register_state_manager(builder_c);

    auto builder_d = StateManagerRegistrationBuilder::begin(&manager_d);
    builder_d.depends_on(&manager_b);
    builder_d.depends_on(&manager_c);
    coordinator.register_state_manager(builder_d);

    coordinator.sim_begin_tick(TickUpdateState{});

    REQUIRE(order.size() == 4);
    REQUIRE(index_of(order, "A") < index_of(order, "B"));
    REQUIRE(index_of(order, "A") < index_of(order, "C"));
    REQUIRE(index_of(order, "B") < index_of(order, "D"));
    REQUIRE(index_of(order, "C") < index_of(order, "D"));
}

TEST_CASE("Fan-out dependency A -> B,C,D is respected", "[StateManagerCoordinator]")
{
    std::vector<std::string_view> order;

    MockStateManager manager_a("A", order);
    MockStateManager manager_b("B", order);
    MockStateManager manager_c("C", order);
    MockStateManager manager_d("D", order);

    StateManagerCoordinator coordinator{};

    coordinator.register_state_manager(StateManagerRegistrationBuilder::begin(&manager_a));

    auto builder_b = StateManagerRegistrationBuilder::begin(&manager_b);
    builder_b.depends_on(&manager_a);
    coordinator.register_state_manager(builder_b);

    auto builder_c = StateManagerRegistrationBuilder::begin(&manager_c);
    builder_c.depends_on(&manager_a);
    coordinator.register_state_manager(builder_c);

    auto builder_d = StateManagerRegistrationBuilder::begin(&manager_d);
    builder_d.depends_on(&manager_a);
    coordinator.register_state_manager(builder_d);

    coordinator.sim_begin_tick(TickUpdateState{});

    REQUIRE(order.size() == 4);
    const size_t idx_a = index_of(order, "A");
    REQUIRE(idx_a < index_of(order, "B"));
    REQUIRE(idx_a < index_of(order, "C"));
    REQUIRE(idx_a < index_of(order, "D"));
}

TEST_CASE("Fan-in dependency A,B,C -> D is respected", "[StateManagerCoordinator]")
{
    std::vector<std::string_view> order;

    MockStateManager manager_a("A", order);
    MockStateManager manager_b("B", order);
    MockStateManager manager_c("C", order);
    MockStateManager manager_d("D", order);

    StateManagerCoordinator coordinator{};

    coordinator.register_state_manager(StateManagerRegistrationBuilder::begin(&manager_a));
    coordinator.register_state_manager(StateManagerRegistrationBuilder::begin(&manager_b));
    coordinator.register_state_manager(StateManagerRegistrationBuilder::begin(&manager_c));

    auto builder_d = StateManagerRegistrationBuilder::begin(&manager_d);
    builder_d.depends_on(&manager_a);
    builder_d.depends_on(&manager_b);
    builder_d.depends_on(&manager_c);
    coordinator.register_state_manager(builder_d);

    coordinator.sim_begin_tick(TickUpdateState{});

    REQUIRE(order.size() == 4);
    const size_t idx_d = index_of(order, "D");
    REQUIRE(index_of(order, "A") < idx_d);
    REQUIRE(index_of(order, "B") < idx_d);
    REQUIRE(index_of(order, "C") < idx_d);
}

TEST_CASE("Duplicate registration is skipped", "[StateManagerCoordinator]")
{
    std::vector<std::string_view> order;

    MockStateManager manager_a("A", order);

    StateManagerCoordinator coordinator{};

    coordinator.register_state_manager(StateManagerRegistrationBuilder::begin(&manager_a));
    coordinator.register_state_manager(StateManagerRegistrationBuilder::begin(&manager_a));

    coordinator.sim_begin_tick(TickUpdateState{});

    REQUIRE(order.size() == 1);
    REQUIRE(order[0] == "A");
}

TEST_CASE("Build is stable across multiple rebuilds", "[StateManagerCoordinator]")
{
    std::vector<std::string_view> order;

    MockStateManager manager_a("A", order);
    MockStateManager manager_b("B", order);
    MockStateManager manager_c("C", order);
    MockStateManager manager_d("D", order);

    StateManagerCoordinator coordinator{};

    coordinator.register_state_manager(StateManagerRegistrationBuilder::begin(&manager_a));

    auto builder_b = StateManagerRegistrationBuilder::begin(&manager_b);
    builder_b.depends_on(&manager_a);
    coordinator.register_state_manager(builder_b);

    auto builder_c = StateManagerRegistrationBuilder::begin(&manager_c);
    builder_c.depends_on(&manager_a);
    coordinator.register_state_manager(builder_c);

    auto builder_d = StateManagerRegistrationBuilder::begin(&manager_d);
    builder_d.depends_on(&manager_b);
    builder_d.depends_on(&manager_c);
    coordinator.register_state_manager(builder_d);

    coordinator.sim_begin_tick(TickUpdateState{});

    REQUIRE(order.size() == 4);
    REQUIRE(index_of(order, "A") < index_of(order, "B"));
    REQUIRE(index_of(order, "A") < index_of(order, "C"));
    REQUIRE(index_of(order, "B") < index_of(order, "D"));
    REQUIRE(index_of(order, "C") < index_of(order, "D"));

    order.clear();

    coordinator.sim_begin_tick(TickUpdateState{});

    REQUIRE(order.size() == 4);
    REQUIRE(index_of(order, "A") < index_of(order, "B"));
    REQUIRE(index_of(order, "A") < index_of(order, "C"));
    REQUIRE(index_of(order, "B") < index_of(order, "D"));
    REQUIRE(index_of(order, "C") < index_of(order, "D"));
}

TEST_CASE("rend_apply_updates steps dependent state managers together by sim time", "[StateManagerCoordinator]")
{
    std::vector<RendCall> calls;

    TickedMockStateManager manager_a("A", {100, 300}, calls);
    TickedMockStateManager manager_b("B", {100, 200, 300}, calls);

    StateManagerCoordinator coordinator{};

    coordinator.register_state_manager(StateManagerRegistrationBuilder::begin(&manager_a));

    auto builder_b = StateManagerRegistrationBuilder::begin(&manager_b);
    builder_b.depends_on(&manager_a);
    coordinator.register_state_manager(builder_b);

    coordinator.rend_apply_updates(FrameUpdateState{300});

    const std::vector<RendCall> expected = {
        {"A", "consume", 100},
        {"B", "consume", 100},
        {"B", "consume", 200},
        {"A", "consume", 300},
        {"B", "consume", 300},
    };
    REQUIRE(filter_calls(calls, "consume") == expected);
}

TEST_CASE("rend_apply_updates leaves ticks after render time for interpolation", "[StateManagerCoordinator]")
{
    std::vector<RendCall> calls;

    TickedMockStateManager manager_a("A", {100, 300}, calls);
    TickedMockStateManager manager_b("B", {100, 200, 300}, calls);

    StateManagerCoordinator coordinator{};

    coordinator.register_state_manager(StateManagerRegistrationBuilder::begin(&manager_a));

    auto builder_b = StateManagerRegistrationBuilder::begin(&manager_b);
    builder_b.depends_on(&manager_a);
    coordinator.register_state_manager(builder_b);

    coordinator.rend_apply_updates(FrameUpdateState{250});

    const std::vector<RendCall> expected_consumes = {
        {"A", "consume", 100},
        {"B", "consume", 100},
        {"B", "consume", 200},
    };
    REQUIRE(filter_calls(calls, "consume") == expected_consumes);

    const std::vector<RendCall> expected_interpolates = {
        {"A", "interpolate", 250},
        {"B", "interpolate", 250},
    };
    REQUIRE(filter_calls(calls, "interpolate") == expected_interpolates);

    REQUIRE(manager_a.rend_get_next_tick_time_us() == 300);
    REQUIRE(manager_b.rend_get_next_tick_time_us() == 300);
}

TEST_CASE("rend_apply_updates snapshots dependents before their dependencies", "[StateManagerCoordinator]")
{
    std::vector<RendCall> calls;

    TickedMockStateManager manager_a("A", {}, calls);
    TickedMockStateManager manager_b("B", {}, calls);

    StateManagerCoordinator coordinator{};

    coordinator.register_state_manager(StateManagerRegistrationBuilder::begin(&manager_a));

    auto builder_b = StateManagerRegistrationBuilder::begin(&manager_b);
    builder_b.depends_on(&manager_a);
    coordinator.register_state_manager(builder_b);

    coordinator.rend_apply_updates(FrameUpdateState{100});

    const std::vector<RendCall> expected = {
        {"B", "begin", 0},
        {"A", "begin", 0},
    };
    REQUIRE(filter_calls(calls, "begin") == expected);
}
