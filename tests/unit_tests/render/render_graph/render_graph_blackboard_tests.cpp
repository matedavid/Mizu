#include <catch2/catch_all.hpp>

#include <string>

#include "render/render_graph/render_graph_blackboard.h"

using namespace Mizu;

struct ResourceA
{
    int32_t value = 0;
};

struct ResourceB
{
    std::string name = "default";
};

struct ResourceC
{
    float x = 0.0f;
    float y = 0.0f;
};

TEST_CASE("RenderGraphBlackboard add default-constructs the resource", "[Render]")
{
    RenderGraphBlackboard blackboard;

    ResourceA& resource = blackboard.add<ResourceA>();
    REQUIRE(resource.value == 0);
}

TEST_CASE("RenderGraphBlackboard add stores and returns the moved-in value", "[Render]")
{
    RenderGraphBlackboard blackboard;

    ResourceA& resource = blackboard.add<ResourceA>(ResourceA{42});
    REQUIRE(resource.value == 42);

    REQUIRE(blackboard.get<ResourceA>().value == 42);
}

TEST_CASE("RenderGraphBlackboard add returns a reference that aliases the stored resource", "[Render]")
{
    RenderGraphBlackboard blackboard;

    ResourceA& resource = blackboard.add<ResourceA>(ResourceA{1});
    resource.value = 7;

    REQUIRE(blackboard.get<ResourceA>().value == 7);
}

TEST_CASE("RenderGraphBlackboard get reflects mutations made through a prior reference", "[Render]")
{
    RenderGraphBlackboard blackboard;

    blackboard.add<ResourceB>(ResourceB{"initial"});
    blackboard.get<ResourceB>().name = "mutated";

    REQUIRE(blackboard.get<ResourceB>().name == "mutated");
}

TEST_CASE("RenderGraphBlackboard contains is false before add and true after", "[Render]")
{
    RenderGraphBlackboard blackboard;

    REQUIRE_FALSE(blackboard.contains<ResourceA>());

    blackboard.add<ResourceA>();
    REQUIRE(blackboard.contains<ResourceA>());
}

TEST_CASE("RenderGraphBlackboard stores distinct types independently", "[Render]")
{
    RenderGraphBlackboard blackboard;

    blackboard.add<ResourceA>(ResourceA{5});
    blackboard.add<ResourceB>(ResourceB{"hello"});
    blackboard.add<ResourceC>(ResourceC{1.0f, 2.0f});

    REQUIRE(blackboard.contains<ResourceA>());
    REQUIRE(blackboard.contains<ResourceB>());
    REQUIRE(blackboard.contains<ResourceC>());

    REQUIRE(blackboard.get<ResourceA>().value == 5);
    REQUIRE(blackboard.get<ResourceB>().name == "hello");
    REQUIRE(blackboard.get<ResourceC>().x == Catch::Approx(1.0f));
    REQUIRE(blackboard.get<ResourceC>().y == Catch::Approx(2.0f));
}

TEST_CASE("RenderGraphBlackboard duplicate add keeps the original value", "[Render]")
{
    RenderGraphBlackboard blackboard;

    blackboard.add<ResourceA>(ResourceA{10});
    ResourceA& second = blackboard.add<ResourceA>(ResourceA{20});

    REQUIRE(second.value == 10);
    REQUIRE(blackboard.get<ResourceA>().value == 10);
}

TEST_CASE("RenderGraphBlackboard remove erases a resource", "[Render]")
{
    RenderGraphBlackboard blackboard;

    blackboard.add<ResourceA>(ResourceA{3});
    REQUIRE(blackboard.contains<ResourceA>());

    blackboard.remove<ResourceA>();
    REQUIRE_FALSE(blackboard.contains<ResourceA>());
}

TEST_CASE("RenderGraphBlackboard remove of a missing resource is a no-op", "[Render]")
{
    RenderGraphBlackboard blackboard;

    REQUIRE_FALSE(blackboard.contains<ResourceA>());
    blackboard.remove<ResourceA>();
    REQUIRE_FALSE(blackboard.contains<ResourceA>());
}

TEST_CASE("RenderGraphBlackboard can re-add a resource after removal", "[Render]")
{
    RenderGraphBlackboard blackboard;

    blackboard.add<ResourceA>(ResourceA{1});
    blackboard.remove<ResourceA>();

    ResourceA& re_added = blackboard.add<ResourceA>(ResourceA{99});
    REQUIRE(re_added.value == 99);
    REQUIRE(blackboard.get<ResourceA>().value == 99);
}

TEST_CASE("RenderGraphBlackboard child reads resources from its parent", "[Render]")
{
    RenderGraphBlackboard parent;
    parent.add<ResourceA>(ResourceA{123});

    const RenderGraphBlackboard child(parent);

    REQUIRE(child.contains<ResourceA>());
    REQUIRE(child.get<ResourceA>().value == 123);
}

TEST_CASE("RenderGraphBlackboard child contains is false for a resource absent from both scopes", "[Render]")
{
    RenderGraphBlackboard parent;
    parent.add<ResourceA>(ResourceA{1});

    const RenderGraphBlackboard child(parent);

    REQUIRE(child.contains<ResourceA>());
    REQUIRE_FALSE(child.contains<ResourceB>());
}

TEST_CASE("RenderGraphBlackboard child add shadows the parent resource", "[Render]")
{
    RenderGraphBlackboard parent;
    parent.add<ResourceA>(ResourceA{1});

    RenderGraphBlackboard child(parent);
    child.add<ResourceA>(ResourceA{2});

    REQUIRE(child.get<ResourceA>().value == 2);
    REQUIRE(parent.get<ResourceA>().value == 1);
}

TEST_CASE("RenderGraphBlackboard child resources do not leak into the parent", "[Render]")
{
    RenderGraphBlackboard parent;

    RenderGraphBlackboard child(parent);
    child.add<ResourceB>(ResourceB{"child-only"});

    REQUIRE(child.contains<ResourceB>());
    REQUIRE_FALSE(parent.contains<ResourceB>());
}

TEST_CASE("RenderGraphBlackboard child sees parent mutations through the shared resource", "[Render]")
{
    RenderGraphBlackboard parent;
    parent.add<ResourceA>(ResourceA{1});

    const RenderGraphBlackboard child(parent);

    parent.get<ResourceA>().value = 55;
    REQUIRE(child.get<ResourceA>().value == 55);
}

TEST_CASE("RenderGraphBlackboard resolves resources across multiple parent levels", "[Render]")
{
    RenderGraphBlackboard grandparent;
    grandparent.add<ResourceA>(ResourceA{7});

    const RenderGraphBlackboard parent(grandparent);
    const RenderGraphBlackboard child(parent);

    REQUIRE(child.contains<ResourceA>());
    REQUIRE(child.get<ResourceA>().value == 7);
}
