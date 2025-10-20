#ifndef TILE_SIZE
#define TILE_SIZE 16
#endif

#ifndef MAX_LIGHTS_PER_TILE
#define MAX_LIGHTS_PER_TILE 128
#endif

struct LightCullingInfo
{
    uint2 numTiles;
}
