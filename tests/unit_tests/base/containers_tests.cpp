#include <catch2/catch_all.hpp>

#include <memory>
#include <ranges>
#include <string>

#include "base/containers/inplace_any.h"
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

TEST_CASE("inplace_vector clear on filled vector results in empty vector", "[Base]")
{
    inplace_vector<uint32_t, 5> vec{1u, 2u, 3u};

    vec.clear();

    REQUIRE(vec.empty());
    REQUIRE(vec.size() == 0);
}

TEST_CASE("inplace_vector clear on empty vector does nothing", "[Base]")
{
    inplace_vector<uint32_t, 5> vec;

    vec.clear();

    REQUIRE(vec.empty());
    REQUIRE(vec.size() == 0);
}

TEST_CASE("inplace_vector clear allows reuse of vector", "[Base]")
{
    inplace_vector<uint32_t, 5> vec{10u, 20u, 30u};

    vec.clear();
    vec.push_back(99u);
    vec.push_back(100u);

    REQUIRE(vec.size() == 2);
    REQUIRE(vec[0] == 99);
    REQUIRE(vec[1] == 100);
}

TEST_CASE("inplace_vector clear makes begin equal to end", "[Base]")
{
    inplace_vector<uint32_t, 5> vec{1u, 2u, 3u};

    vec.clear();

    REQUIRE(vec.begin() == vec.end());
}

TEST_CASE("inplace_vector clear releases resources for non-trivial types", "[Base]")
{
    auto destroyed = std::make_shared<int>(0);
    std::weak_ptr<int> weak = destroyed;

    {
        inplace_vector<std::shared_ptr<int>, 3> vec;
        vec.push_back(destroyed);
        destroyed.reset();

        REQUIRE(!weak.expired());
        vec.clear();
        REQUIRE(weak.expired());
    }
}

enum class Color
{
    Red,
    Green,
    Blue,
};

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

struct InplaceAnyTracked
{
    static inline int32_t alive = 0;

    InplaceAnyTracked() { alive += 1; }
    InplaceAnyTracked(int32_t value_) : value(value_) { alive += 1; }
    InplaceAnyTracked(const InplaceAnyTracked& other) : value(other.value) { alive += 1; }
    InplaceAnyTracked(InplaceAnyTracked&& other) noexcept : value(other.value) { alive += 1; }
    ~InplaceAnyTracked() { alive -= 1; }

    int32_t value = 0;
};

struct InplaceAnyOther
{
    float value = 0.0f;
};

TEST_CASE("inplace_any is empty by default", "[Base]")
{
    const inplace_any<64> any{};

    REQUIRE(!any.has_value());
    REQUIRE(!any.is_type<InplaceAnyTracked>());
    REQUIRE(any.get_if<InplaceAnyTracked>() == nullptr);
    REQUIRE(any.capacity() == 64);
}

TEST_CASE("inplace_any emplace stores and returns the value", "[Base]")
{
    inplace_any<64> any{};

    const InplaceAnyTracked& value = any.emplace<InplaceAnyTracked>(42);

    REQUIRE(any.has_value());
    REQUIRE(value.value == 42);
    REQUIRE(any.get<InplaceAnyTracked>().value == 42);
}

TEST_CASE("inplace_any emplace returns a reference that aliases the stored value", "[Base]")
{
    inplace_any<64> any{};

    InplaceAnyTracked& value = any.emplace<InplaceAnyTracked>(1);
    value.value = 7;

    REQUIRE(any.get<InplaceAnyTracked>().value == 7);
}

TEST_CASE("inplace_any is_type only matches the stored type", "[Base]")
{
    inplace_any<64> any{};
    any.emplace<InplaceAnyTracked>(1);

    REQUIRE(any.is_type<InplaceAnyTracked>());
    REQUIRE(!any.is_type<InplaceAnyOther>());
}

TEST_CASE("inplace_any get_if returns nullptr for a different type", "[Base]")
{
    inplace_any<64> any{};
    any.emplace<InplaceAnyTracked>(1);

    REQUIRE(any.get_if<InplaceAnyTracked>() != nullptr);
    REQUIRE(any.get_if<InplaceAnyOther>() == nullptr);
}

TEST_CASE("inplace_any emplace replaces the previously stored value", "[Base]")
{
    InplaceAnyTracked::alive = 0;

    inplace_any<64> any{};
    any.emplace<InplaceAnyTracked>(1);
    REQUIRE(InplaceAnyTracked::alive == 1);

    any.emplace<InplaceAnyOther>(2.0f);

    REQUIRE(InplaceAnyTracked::alive == 0);
    REQUIRE(any.is_type<InplaceAnyOther>());
    REQUIRE(any.get<InplaceAnyOther>().value == Catch::Approx(2.0f));
}

TEST_CASE("inplace_any reset destroys the stored value", "[Base]")
{
    InplaceAnyTracked::alive = 0;

    inplace_any<64> any{};
    any.emplace<InplaceAnyTracked>(1);
    REQUIRE(InplaceAnyTracked::alive == 1);

    any.reset();

    REQUIRE(InplaceAnyTracked::alive == 0);
    REQUIRE(!any.has_value());
}

TEST_CASE("inplace_any reset on an empty container does nothing", "[Base]")
{
    inplace_any<64> any{};

    any.reset();

    REQUIRE(!any.has_value());
}

TEST_CASE("inplace_any destructor destroys the stored value", "[Base]")
{
    InplaceAnyTracked::alive = 0;

    {
        inplace_any<64> any{};
        any.emplace<InplaceAnyTracked>(1);
        REQUIRE(InplaceAnyTracked::alive == 1);
    }

    REQUIRE(InplaceAnyTracked::alive == 0);
}

TEST_CASE("inplace_any releases resources for non-trivial types", "[Base]")
{
    auto destroyed = std::make_shared<int>(0);
    std::weak_ptr<int> weak = destroyed;

    {
        inplace_any<64> any{};
        any.emplace<std::shared_ptr<int>>(destroyed);
        destroyed.reset();

        REQUIRE(!weak.expired());
        any.reset();
        REQUIRE(weak.expired());
    }
}

TEST_CASE("inplace_any stores non-trivially copyable types", "[Base]")
{
    inplace_any<64> any{};

    any.emplace<std::string>("hello");
    REQUIRE(any.get<std::string>() == "hello");

    any.get<std::string>() += " world";
    REQUIRE(any.get<std::string>() == "hello world");
}

TEST_CASE("inplace_any copy constructor copies the stored value", "[Base]")
{
    InplaceAnyTracked::alive = 0;

    inplace_any<64> any{};
    any.emplace<InplaceAnyTracked>(7);

    inplace_any<64> copy{any};

    REQUIRE(InplaceAnyTracked::alive == 2);
    REQUIRE(copy.get<InplaceAnyTracked>().value == 7);

    copy.get<InplaceAnyTracked>().value = 9;
    REQUIRE(any.get<InplaceAnyTracked>().value == 7);
}

TEST_CASE("inplace_any copy of an empty container stays empty", "[Base]")
{
    const inplace_any<64> any{};
    const inplace_any<64> copy{any};

    REQUIRE(!copy.has_value());
}

TEST_CASE("inplace_any copy assignment replaces the stored value", "[Base]")
{
    InplaceAnyTracked::alive = 0;

    inplace_any<64> any{};
    any.emplace<InplaceAnyTracked>(7);

    inplace_any<64> other{};
    other.emplace<InplaceAnyTracked>(3);

    other = any;

    REQUIRE(InplaceAnyTracked::alive == 2);
    REQUIRE(other.get<InplaceAnyTracked>().value == 7);
    REQUIRE(any.get<InplaceAnyTracked>().value == 7);
}

TEST_CASE("inplace_any copy assignment to itself keeps the value", "[Base]")
{
    InplaceAnyTracked::alive = 0;

    inplace_any<64> any{};
    any.emplace<InplaceAnyTracked>(7);

    inplace_any<64>& alias = any;
    any = alias;

    REQUIRE(InplaceAnyTracked::alive == 1);
    REQUIRE(any.get<InplaceAnyTracked>().value == 7);
}

TEST_CASE("inplace_any move constructor leaves the source empty", "[Base]")
{
    InplaceAnyTracked::alive = 0;

    inplace_any<64> any{};
    any.emplace<InplaceAnyTracked>(7);

    const inplace_any<64> moved{std::move(any)};

    REQUIRE(InplaceAnyTracked::alive == 1);
    REQUIRE(moved.get<InplaceAnyTracked>().value == 7);
    REQUIRE(!any.has_value());
}

TEST_CASE("inplace_any move assignment leaves the source empty", "[Base]")
{
    InplaceAnyTracked::alive = 0;

    inplace_any<64> any{};
    any.emplace<InplaceAnyTracked>(7);

    inplace_any<64> other{};
    other.emplace<InplaceAnyTracked>(3);

    other = std::move(any);

    REQUIRE(InplaceAnyTracked::alive == 1);
    REQUIRE(other.get<InplaceAnyTracked>().value == 7);
    REQUIRE(!any.has_value());
}

TEST_CASE("inplace_any move transfers ownership of non-trivial types", "[Base]")
{
    auto destroyed = std::make_shared<int>(0);
    std::weak_ptr<int> weak = destroyed;

    {
        inplace_any<64> any{};
        any.emplace<std::shared_ptr<int>>(destroyed);
        destroyed.reset();

        inplace_any<64> moved{std::move(any)};
        REQUIRE(!any.has_value());
        REQUIRE(!weak.expired());
    }

    REQUIRE(weak.expired());
}

TEST_CASE("inplace_any can be re-used after reset", "[Base]")
{
    inplace_any<64> any{};

    any.emplace<InplaceAnyTracked>(1);
    any.reset();

    any.emplace<InplaceAnyOther>(5.0f);

    REQUIRE(any.is_type<InplaceAnyOther>());
    REQUIRE(any.get<InplaceAnyOther>().value == Catch::Approx(5.0f));
}

struct InplaceAnyTooBig
{
    std::byte data[128];
};

struct InplaceAnyNoCopy
{
    InplaceAnyNoCopy() = default;
    InplaceAnyNoCopy(const InplaceAnyNoCopy&) = delete;
};

TEST_CASE("inplace_any concept accepts storable types", "[Base]")
{
    STATIC_REQUIRE(InplaceAnyStorable<InplaceAnyTracked, 64>);
    STATIC_REQUIRE(InplaceAnyStorable<InplaceAnyOther, 64>);
    STATIC_REQUIRE(InplaceAnyStorable<std::string, 64>);
}

TEST_CASE("inplace_any concept rejects types that do not fit in the capacity", "[Base]")
{
    STATIC_REQUIRE(sizeof(InplaceAnyTooBig) > 64);
    STATIC_REQUIRE_FALSE(InplaceAnyStorable<InplaceAnyTooBig, 64>);
    STATIC_REQUIRE(InplaceAnyStorable<InplaceAnyTooBig, 128>);
}

TEST_CASE("inplace_any concept rejects non copy constructible types", "[Base]")
{
    STATIC_REQUIRE_FALSE(InplaceAnyStorable<InplaceAnyNoCopy, 64>);
}

TEST_CASE("inplace_any concept rejects non decayed types", "[Base]")
{
    STATIC_REQUIRE_FALSE(InplaceAnyStorable<const InplaceAnyTracked, 64>);
    STATIC_REQUIRE_FALSE(InplaceAnyStorable<InplaceAnyTracked&, 64>);
}
