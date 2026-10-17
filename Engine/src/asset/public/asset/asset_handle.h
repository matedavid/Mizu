#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>

namespace Mizu
{

enum class AssetType
{
    Mesh,
    Texture,
    Material,
    Prefab,
    ShaderDeclaration,
    ShaderGraph,
};

using AssetHandleId = uint64_t;

template <typename Tag>
struct AssetHandle
{
  public:
    static constexpr uint64_t INVALID_VALUE = std::numeric_limits<AssetHandleId>::max();

    constexpr AssetHandle() : m_id(INVALID_VALUE) {}
    constexpr AssetHandle(AssetHandleId id) : m_id(id) {}

    constexpr AssetHandleId get_id() const { return m_id; }
    constexpr bool is_valid() const { return m_id != INVALID_VALUE; }

    constexpr bool operator==(const AssetHandle&) const = default;

  private:
    AssetHandleId m_id;
};

template <typename T>
struct is_asset_handle : std::false_type
{
};

template <typename Tag>
struct is_asset_handle<AssetHandle<Tag>> : std::true_type
{
};

template <typename T>
constexpr bool is_asset_handle_v = is_asset_handle<T>::value;

template <typename T>
concept AssetHandleType = requires() { is_asset_handle_v<T>; };

struct AssetKey
{
    AssetType type;
    AssetHandleId id;

    bool operator==(const AssetKey& other) const = default;
};

template <AssetHandleType T>
struct asset_type_for_handle
{
};

#define MIZU_CREATE_ASSET_HANDLE(HandleTypeName)                    \
    using HandleTypeName = AssetHandle<struct HandleTypeName##Tag>; \
    static_assert(is_asset_handle_v<MeshAssetHandle>, #HandleTypeName " should satisfy is_asset_handle")

#define MIZU_CREATE_ASSET_HANDLE_KEYED(HandleTypeName, AssetTypeValue) \
    MIZU_CREATE_ASSET_HANDLE(HandleTypeName);                          \
    template <>                                                        \
    struct asset_type_for_handle<HandleTypeName>                       \
    {                                                                  \
        static constexpr AssetType value = AssetTypeValue;             \
    }

MIZU_CREATE_ASSET_HANDLE_KEYED(MeshAssetHandle, AssetType::Mesh);
MIZU_CREATE_ASSET_HANDLE_KEYED(TextureAssetHandle, AssetType::Texture);
MIZU_CREATE_ASSET_HANDLE(MaterialAssetHandle);
MIZU_CREATE_ASSET_HANDLE(PrefabAssetHandle);
MIZU_CREATE_ASSET_HANDLE(ShaderAssetHandle);

#undef MIZU_CREATE_ASSET_HANDLE_KEYED
#undef MIZU_CREATE_ASSET_HANDLE

template <AssetHandleType T>
constexpr AssetKey to_asset_key(T handle)
{
    return AssetKey{
        .type = asset_type_for_handle<T>::value,
        .id = handle.get_id(),
    };
}

template <AssetHandleType T>
constexpr T to_asset_handle(AssetKey key)
{
    return T{key.id};
}

} // namespace Mizu

template <typename Tag>
struct std::hash<Mizu::AssetHandle<Tag>>
{
    size_t operator()(const Mizu::AssetHandle<Tag>& handle) const { return handle.get_id(); }
};
