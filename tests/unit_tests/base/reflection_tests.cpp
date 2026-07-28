#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <string_view>
#include <vector>

#include "base/containers/typed_bitset.h"
#include "base/reflection/enum_traits.h"

using namespace Mizu;

enum class Api
{
    Dx12,
    Vulkan,
};

enum SharingMode
{
    Exclusive,
    Concurrent,
};

enum class Keycode
{
    Space = 32,
    A = 65,
    RightControl = 345,
};

MIZU_META_ENUM_RANGE(Keycode, 32, 346);

enum class Usage : uint16_t
{
    None = 0,
    VertexBuffer = (1 << 0),
    IndexBuffer = (1 << 1),
    HostVisible = (1 << 10),
};

MIZU_META_ENUM_FLAGS(Usage);

TEST_CASE("enum_traits reflects a dense enum with no registration", "[Base][Reflection]")
{
    STATIC_REQUIRE(meta::enum_traits<Api>::count == 2);
    STATIC_REQUIRE(meta::enum_traits<Api>::values == std::array{Api::Dx12, Api::Vulkan});
    STATIC_REQUIRE(meta::enum_traits<Api>::names == std::array<std::string_view, 2>{"Dx12", "Vulkan"});
}

TEST_CASE("enum_traits reflects unscoped enums", "[Base][Reflection]")
{
    STATIC_REQUIRE(meta::enum_traits<SharingMode>::count == 2);
    STATIC_REQUIRE(meta::enum_name(Concurrent) == "Concurrent");
}

TEST_CASE("enum_name maps a value to its enumerator name", "[Base][Reflection]")
{
    STATIC_REQUIRE(meta::enum_name(Api::Dx12) == "Dx12");
    STATIC_REQUIRE(meta::enum_name(Api::Vulkan) == "Vulkan");
}

TEST_CASE("enum_name returns an empty view for an unnamed value", "[Base][Reflection]")
{
    STATIC_REQUIRE(meta::enum_name(static_cast<Api>(7)).empty());
}

TEST_CASE("enum_from_string round trips and rejects unknown names", "[Base][Reflection]")
{
    STATIC_REQUIRE(meta::enum_from_string<Api>("Vulkan") == Api::Vulkan);
    STATIC_REQUIRE(meta::enum_from_string<Api>("Dx12") == Api::Dx12);
    STATIC_REQUIRE(meta::enum_from_string<Api>("D3D12") == std::nullopt);
    STATIC_REQUIRE(meta::enum_from_string<Api>("") == std::nullopt);
}

TEST_CASE("index_of is a dense position and rejects unnamed values", "[Base][Reflection]")
{
    STATIC_REQUIRE(meta::enum_traits<Api>::index_of(Api::Dx12) == 0u);
    STATIC_REQUIRE(meta::enum_traits<Api>::index_of(Api::Vulkan) == 1u);
    STATIC_REQUIRE(meta::enum_traits<Api>::index_of(static_cast<Api>(7)) == std::nullopt);

    STATIC_REQUIRE(meta::enum_traits<Api>::contains(Api::Vulkan));
    STATIC_REQUIRE(!meta::enum_traits<Api>::contains(static_cast<Api>(7)));
}

TEST_CASE("a sparse enum reflects every enumerator once the range is widened", "[Base][Reflection]")
{
    STATIC_REQUIRE(meta::enum_traits<Keycode>::count == 3);
    STATIC_REQUIRE(meta::enum_name(Keycode::RightControl) == "RightControl");
    STATIC_REQUIRE(meta::enum_from_string<Keycode>("Space") == Keycode::Space);
}

TEST_CASE("a sparse enum has a dense index space", "[Base][Reflection]")
{
    // Indices, not the raw values 32 / 65 / 345.
    STATIC_REQUIRE(meta::enum_traits<Keycode>::index_of(Keycode::Space) == 0u);
    STATIC_REQUIRE(meta::enum_traits<Keycode>::index_of(Keycode::A) == 1u);
    STATIC_REQUIRE(meta::enum_traits<Keycode>::index_of(Keycode::RightControl) == 2u);
}

TEST_CASE("a flag enum reflects its bits, including beyond the scan window", "[Base][Reflection]")
{
    STATIC_REQUIRE(meta::enum_traits<Usage>::is_flags);
    STATIC_REQUIRE(meta::enum_traits<Usage>::count == 3);
    STATIC_REQUIRE(meta::enum_name(Usage::HostVisible) == "HostVisible");
}

TEST_CASE("a flag enum still resolves its named zero", "[Base][Reflection]")
{
    STATIC_REQUIRE(meta::enum_name(Usage::None) == "None");
    STATIC_REQUIRE(meta::enum_from_string<Usage>("None") == Usage::None);
}

TEST_CASE("enum_flags_name renders a combination", "[Base][Reflection]")
{
    const auto usage =
        static_cast<Usage>(static_cast<uint16_t>(Usage::VertexBuffer) | static_cast<uint16_t>(Usage::HostVisible));

    REQUIRE(meta::enum_flags_name(usage) == "VertexBuffer|HostVisible");
    REQUIRE(meta::enum_flags_name(Usage::None) == "None");
}

TEST_CASE("for_each_flag visits only the set bits", "[Base][Reflection]")
{
    const auto usage =
        static_cast<Usage>(static_cast<uint16_t>(Usage::VertexBuffer) | static_cast<uint16_t>(Usage::HostVisible));

    std::vector<Usage> visited;
    meta::for_each_flag(usage, [&](Usage bit) { visited.push_back(bit); });

    REQUIRE(visited == std::vector{Usage::VertexBuffer, Usage::HostVisible});

    visited.clear();
    meta::for_each_flag(Usage::None, [&](Usage bit) { visited.push_back(bit); });
    REQUIRE(visited.empty());
}

TEST_CASE("typed_bitset is sized by enumerator count, not by value span", "[Base][Reflection]")
{
    typed_bitset<Keycode> keys{};
    REQUIRE(keys.size() == 3);

    keys.set(Keycode::RightControl);
    REQUIRE(keys.test(Keycode::RightControl));
    REQUIRE(!keys.test(Keycode::Space));
    REQUIRE(keys.count() == 1);
}

TEST_CASE("enum_array is indexed by the enumerator", "[Base][Reflection]")
{
    meta::enum_array<Keycode, int> counts{};
    counts[Keycode::A] = 42;

    REQUIRE(counts.size() == 3);
    REQUIRE(counts[Keycode::A] == 42);
    REQUIRE(counts[Keycode::Space] == 0);
}
