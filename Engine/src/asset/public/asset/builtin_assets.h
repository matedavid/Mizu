#pragma once

#include "asset/asset.h"

namespace Mizu
{

enum class BuiltinTexture
{
    White,
    Black,
    Gray,
};

inline constexpr std::string_view get_builtin_texture_virtual_path(BuiltinTexture texture)
{
    switch (texture)
    {
    case BuiltinTexture::White:
        return "engine:texture/white";
    case BuiltinTexture::Black:
        return "engine:texture/black";
    case BuiltinTexture::Gray:
        return "engine:texture/gray";
    }
}

inline constexpr TextureAssetHandle get_builtin_texture_handle(BuiltinTexture texture)
{
    return TextureAssetHandle{get_texture_asset_id(get_builtin_texture_virtual_path(texture))};
}

} // namespace Mizu