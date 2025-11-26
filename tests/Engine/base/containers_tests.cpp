#include <catch2/catch_all.hpp>

#include "base/containers/inplace_vector.h"

using namespace Mizu;

TEST_CASE("inplace_vector initializes correctly", "[Base]")
{
    const inplace_vector<uint32_t, 5> vec{1u, 2u, 3u};
    REQUIRE(vec.size() == 3);
    REQUIRE(vec.capacity() == 5);

    REQUIRE(vec[0] == 1);
    REQUIRE(vec[1] == 2);
    REQUIRE(vec[2] == 3);
}

TEST_CASE("inplace_vector push_back works correctly", "[Base]")
{
    inplace_vector<uint32_t, 5> vec{3u, 2u};

    vec.push_back(1);
    REQUIRE(vec.size() == 3);

    vec.push_back(0);
    REQUIRE(vec.size() == 4);

    REQUIRE(vec[0] == 3);
    REQUIRE(vec[1] == 2);
    REQUIRE(vec[2] == 1);
    REQUIRE(vec[3] == 0);
}

TEST_CASE("inplace_vector reference [] overload works correctly", "[Base]")
{
    struct Item
    {
        uint32_t value;
    };

    inplace_vector<Item, 3> vec{Item{1u}, Item{2u}};

    REQUIRE(vec[0].value == 1);
    REQUIRE(vec[1].value == 2);

    Item& item = vec[0];
    item.value = 42;

    REQUIRE(vec[0].value == 42);
    REQUIRE(vec.size() == 2);
}

TEST_CASE("inplace_vector can iterate over elements", "[Base]")
{
    const inplace_vector<uint32_t, 5> vec{3u, 2u, 1u, 0u};

    uint32_t expected_value = 3;
    for (const uint32_t value : vec)
    {
        REQUIRE(value == expected_value);
        expected_value -= 1;
    }

    expected_value = 0;
    for (int64_t i = vec.size() - 1; i >= 0; --i)
    {
        REQUIRE(vec[i] == expected_value);
        expected_value += 1;
    }
}

TEST_CASE("inplace_vector is_empty works correctly", "[Base]")
{
    inplace_vector<uint32_t, 5> vec;
    REQUIRE(vec.is_empty());

    vec.push_back(5);
    REQUIRE(!vec.is_empty());
    REQUIRE(vec.size() == 1);
}
