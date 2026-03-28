#include <catch2/catch_all.hpp>

#include <ranges>

#include "base/containers/inplace_vector.h"
#include "base/containers/typed_bitset.h"

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
    for (uint32_t value : vec | std::views::reverse)
    {
        REQUIRE(value == expected_value);
        expected_value += 1;
    }
}

TEST_CASE("inplace_vector empty works correctly", "[Base]")
{
    inplace_vector<uint32_t, 5> vec;
    REQUIRE(vec.empty());

    vec.push_back(5);
    REQUIRE(!vec.empty());
    REQUIRE(vec.size() == 1);
}

TEST_CASE("inplace_vector can fill entire vector", "[Base]")
{
    inplace_vector<uint32_t, 1> vec;
    vec.push_back(3);
    REQUIRE(!vec.empty());
    REQUIRE(vec.size() == 1);
}

TEST_CASE("inplace_vector emplace_back works correctly", "[Base]")
{
    inplace_vector<uint32_t, 5> vec;

    uint32_t& value0 = vec.emplace_back();
    value0 = 5;

    vec.emplace_back(88);

    REQUIRE(vec.size() == 2);
    REQUIRE(vec[0] == 5);
    REQUIRE(vec[1] == 88);
}

TEST_CASE("inplace_vector erase works correctly", "[Base]")
{
    inplace_vector<uint32_t, 5> vec{1u, 2u, 3u, 4u, 5u};

    const auto it = vec.erase(vec.begin(), vec.begin() + 2);
    REQUIRE(vec.size() == 3);

    REQUIRE(vec[0] == 3);
    REQUIRE(vec[1] == 4);
    REQUIRE(vec[2] == 5);
    REQUIRE(*it == 3);
}

TEST_CASE("inplace_vector erase all elements results in empty vector", "[Base]")
{
    inplace_vector<uint32_t, 5> vec{1u, 2u, 3u};

    vec.erase(vec.begin(), vec.end());
    REQUIRE(vec.empty());
    REQUIRE(vec.size() == 0);
}

TEST_CASE("inplace_vector erase on empty vector does nothing", "[Base]")
{
    inplace_vector<uint32_t, 5> vec;

    const auto it = vec.erase(vec.begin(), vec.end());
    REQUIRE(vec.empty());

    REQUIRE(vec.size() == 0);
    REQUIRE(it == vec.end());
}

TEST_CASE("inplace_vector erase with remove_if works correctly", "[Base]")
{
    inplace_vector<uint32_t, 5> vec{1u, 2u, 3u, 4u, 5u};

    const auto new_end = std::remove_if(vec.begin(), vec.end(), [](uint32_t v) { return v % 2 == 0; });
    vec.erase(new_end, vec.end());

    REQUIRE(vec.size() == 3);

    REQUIRE(vec[0] == 1);
    REQUIRE(vec[1] == 3);
    REQUIRE(vec[2] == 5);
}

TEST_CASE("inplace_vector insert single element at end", "[Base]")
{
    inplace_vector<uint32_t, 5> vec{1u, 2u, 3u};
    const auto it = vec.insert(vec.end(), 4u);

    REQUIRE(vec.size() == 4);
    REQUIRE(vec[3] == 4);
    REQUIRE(*it == 4);
}

TEST_CASE("inplace_vector insert single element at beginning", "[Base]")
{
    inplace_vector<uint32_t, 5> vec{2u, 3u, 4u};
    const auto it = vec.insert(vec.begin(), 1u);

    REQUIRE(vec.size() == 4);
    REQUIRE(vec[0] == 1);
    REQUIRE(vec[1] == 2);
    REQUIRE(vec[2] == 3);
    REQUIRE(vec[3] == 4);
    REQUIRE(*it == 1);
}

TEST_CASE("inplace_vector insert single element in the middle", "[Base]")
{
    inplace_vector<uint32_t, 5> vec{1u, 2u, 4u, 5u};
    const auto it = vec.insert(vec.begin() + 2, 3u);

    REQUIRE(vec.size() == 5);
    REQUIRE(vec[0] == 1);
    REQUIRE(vec[1] == 2);
    REQUIRE(vec[2] == 3);
    REQUIRE(vec[3] == 4);
    REQUIRE(vec[4] == 5);
    REQUIRE(*it == 3);
}

TEST_CASE("inplace_vector insert single element into empty vector", "[Base]")
{
    inplace_vector<uint32_t, 5> vec;
    const auto it = vec.insert(vec.begin(), 42u);

    REQUIRE(vec.size() == 1);
    REQUIRE(vec[0] == 42);
    REQUIRE(*it == 42);
}

TEST_CASE("inplace_vector insert count copies at end", "[Base]")
{
    inplace_vector<uint32_t, 7> vec{1u, 2u, 3u};
    const auto it = vec.insert(vec.end(), 3u, 99u);

    REQUIRE(vec.size() == 6);
    REQUIRE(vec[3] == 99);
    REQUIRE(vec[4] == 99);
    REQUIRE(vec[5] == 99);
    REQUIRE(*it == 99);
}

TEST_CASE("inplace_vector insert count copies in the middle", "[Base]")
{
    inplace_vector<uint32_t, 7> vec{1u, 2u, 5u, 6u};
    const auto it = vec.insert(vec.begin() + 2, 2u, 99u);

    REQUIRE(vec.size() == 6);
    REQUIRE(vec[0] == 1);
    REQUIRE(vec[1] == 2);
    REQUIRE(vec[2] == 99);
    REQUIRE(vec[3] == 99);
    REQUIRE(vec[4] == 5);
    REQUIRE(vec[5] == 6);
    REQUIRE(*it == 99);
}

TEST_CASE("inplace_vector insert count of zero does nothing", "[Base]")
{
    inplace_vector<uint32_t, 5> vec{1u, 2u, 3u};
    const auto it = vec.insert(vec.begin() + 1, 0u, 99u);

    REQUIRE(vec.size() == 3);
    REQUIRE(it == vec.begin() + 1);
}

TEST_CASE("inplace_vector insert iterator range at end", "[Base]")
{
    inplace_vector<uint32_t, 6> vec{1u, 2u, 3u};
    const std::array<uint32_t, 3> source{4u, 5u, 6u};

    const auto it = vec.insert(vec.end(), source.begin(), source.end());

    REQUIRE(vec.size() == 6);
    REQUIRE(vec[3] == 4);
    REQUIRE(vec[4] == 5);
    REQUIRE(vec[5] == 6);
    REQUIRE(*it == 4);
}

TEST_CASE("inplace_vector insert iterator range at beginning", "[Base]")
{
    inplace_vector<uint32_t, 6> vec{4u, 5u, 6u};
    const std::array<uint32_t, 3> source{1u, 2u, 3u};

    const auto it = vec.insert(vec.begin(), source.begin(), source.end());

    REQUIRE(vec.size() == 6);
    REQUIRE(vec[0] == 1);
    REQUIRE(vec[1] == 2);
    REQUIRE(vec[2] == 3);
    REQUIRE(vec[3] == 4);
    REQUIRE(vec[4] == 5);
    REQUIRE(vec[5] == 6);
    REQUIRE(*it == 1);
}

TEST_CASE("inplace_vector insert iterator range in the middle", "[Base]")
{
    inplace_vector<uint32_t, 6> vec{1u, 2u, 5u, 6u};
    const std::array<uint32_t, 2> source{3u, 4u};

    const auto it = vec.insert(vec.begin() + 2, source.begin(), source.end());

    REQUIRE(vec.size() == 6);
    REQUIRE(vec[0] == 1);
    REQUIRE(vec[1] == 2);
    REQUIRE(vec[2] == 3);
    REQUIRE(vec[3] == 4);
    REQUIRE(vec[4] == 5);
    REQUIRE(vec[5] == 6);
    REQUIRE(*it == 3);
}

TEST_CASE("inplace_vector insert empty iterator range does nothing", "[Base]")
{
    inplace_vector<uint32_t, 5> vec{1u, 2u, 3u};
    const std::array<uint32_t, 0> source{};

    const auto it = vec.insert(vec.begin() + 1, source.begin(), source.end());

    REQUIRE(vec.size() == 3);
    REQUIRE(it == vec.begin() + 1);
}

TEST_CASE("inplace_vector insert iterator range from another inplace_vector", "[Base]")
{
    inplace_vector<uint32_t, 6> vec{1u, 4u};
    const inplace_vector<uint32_t, 3> source{2u, 3u};

    vec.insert(vec.begin() + 1, source.begin(), source.end());

    REQUIRE(vec.size() == 4);
    REQUIRE(vec[0] == 1);
    REQUIRE(vec[1] == 2);
    REQUIRE(vec[2] == 3);
    REQUIRE(vec[3] == 4);
}

enum class Color
{
    Red,
    Green,
    Blue,
};

MIZU_CREATE_ENUM_METADATA(Color, 3);

TEST_CASE("typed_bitset can set values", "[Base]")
{
    typed_bitset<Color> bitset{};
    bitset.set(Color::Red, true);
    bitset.set(Color::Blue, false);

    REQUIRE(bitset.test(Color::Red));
    REQUIRE(!bitset.test(Color::Blue));
}

TEST_CASE("typed_bitset by default all values are false", "[Base]")
{
    typed_bitset<Color> bitset{};

    REQUIRE(!bitset.test(Color::Red));
    REQUIRE(!bitset.test(Color::Green));
    REQUIRE(!bitset.test(Color::Blue));
}
