#ifndef SCENE_RENDERER_LIGHT_CULLING_COMMON
#define SCENE_RENDERER_LIGHT_CULLING_COMMON

#ifndef TILE_SIZE
#error "TILE_SIZE not defined"
#endif

#ifndef MAX_LIGHTS_PER_TILE
#error "MAX_LIGHTS_PER_TILE not defined"
#endif

struct LightCullingInfo
{
    uint2 numTiles;
};

static const uint16_t INVALID_TILE_VISIBLE_LIGHT = 0xFFFF;

// clang-format off
#define LIGHT_TYPE uint16_t

static const LIGHT_TYPE LIGHT_TYPE_POINT = (1 << 0);
static const LIGHT_TYPE LIGHT_TYPE_AREA  = (1 << 1);
// clang-format on

uint16_t encodeTileVisibleLight(LIGHT_TYPE type, uint16_t index)
{
    // [     2       ..       14  ] = 16 bits
    // [ light_type         index ]

    return (type << 14) | (index & 0x3FFF);
}

void decodeTileVisibleLight(uint16_t value, out LIGHT_TYPE type, out uint16_t index)
{
    type = (LIGHT_TYPE)(value >> 14);
    index = value & 0x3FFF;
}

#endif 