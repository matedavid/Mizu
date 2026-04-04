#include <catch2/catch_all.hpp>

#include <stdexcept>
#include <string_view>
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

    void sim_begin_tick(const TickUpdateState&) override { m_order.push_back(m_identifier); }

    void sim_end_tick() override {}
    void rend_apply_updates([[maybe_unused]] const FrameUpdateState&) override {}

    std::string_view get_identifier() const override { return m_identifier; }

  private:
    std::string_view m_identifier;

    std::vector<std::string_view>& m_order;
};

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

    StateManagerCoordinator2 coordinator{};

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

    StateManagerCoordinator2 coordinator{};

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

    StateManagerCoordinator2 coordinator{};

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

    StateManagerCoordinator2 coordinator{};

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

    StateManagerCoordinator2 coordinator{};

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

    StateManagerCoordinator2 coordinator{};

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

    StateManagerCoordinator2 coordinator{};

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
