// WICKLIGHT - Wick's Island block world: a grid of 1-unit blocks you can mine and place.
// Generated from a seed (hills, beaches, caves, ores, trees); only changes are saved.
#pragma once
#include "render.h"
#include <cstdint>
#include <map>
#include <string>
#include <vector>

enum BlockId : uint8_t {
    BK_AIR = 0, BK_GRASS, BK_DIRT, BK_STONE, BK_COBBLE, BK_SAND, BK_LOG, BK_PLANKS, BK_LEAVES, BK_GLASS,
    BK_STONEBRICK, BK_EMBER_ORE, BK_CRYSTAL_ORE, BK_SCRAP_ORE, BK_BEDROCK, BK_LAMP, BK_MARBLE, BK_ROOF, BK_METAL, BK_GLOWCRYSTAL,
    BK_WAX_WHITE, BK_WAX_RED, BK_WAX_YELLOW, BK_WAX_GREEN, BK_WAX_BLUE, BK_WAX_PURPLE,
    BK_WATER,
    BK_BED, BK_CHEST, BK_DOOR, BK_DOOR_TOP, BK_LADDER, BK_BENCH, BK_FURNACE, BK_JUMPPAD, BK_BOMB, BK_SAPLING,
    BK_FLOWER_RED, BK_FLOWER_YELLOW, BK_FORGE, BK_CANDLE,
    BK_COUNT
};
// inventory items: block ids are items too; these are the extra non-block items
enum ItemId { IT_CRYSTAL = 100, IT_GEAR, IT_WAX, IT_END };

enum BlockShape : uint8_t { SH_NONE, SH_CUBE, SH_CROSS, SH_WATER, SH_BED, SH_CHEST, SH_DOOR, SH_LADDER, SH_SLAB, SH_CANDLE };

struct BlockDef {
    const char* name;
    int side, top, bottom, front;  // tiles (front = -1: same as side)
    float hard;                     // seconds to mine (bedrock: <0, unbreakable)
    BlockShape shape;
    bool solid;                     // collides
    bool opaque;                    // hides the neighbour's face
    float emis;
    uint32_t tint;                  // vertex colour
    uint32_t light;                 // 0 = gives no light
    int drop;                       // item dropped when mined: 0 = itself, -1 = nothing, else item id
};
const BlockDef& blockDef(int b);
const char* itemName(int item);
int itemIconTile(int item);
V3 itemIconTint(int item);
bool isPlaceable(int item);

struct VoxWorld {
    static const int X0 = -48, NX = 96, Y0 = -16, NY = 44, Z0 = -48, NZ = 96, CS = 16;
    static const int NCX = NX / CS, NCZ = NZ / CS;
    static const int SEA = -1;  // water surface (water blocks fill y <= SEA-1)
    std::vector<uint8_t> blk, meta, genBlk, genMeta;
    std::vector<int8_t> height;  // generated ground height per column (top face y)
    uint32_t seed = 0;
    bool active = false;
    Mesh solid[NCX * NCZ], water[NCX * NCZ];
    bool dirty[NCX * NCZ] = {};
    bool anyDirty = false;

    static bool inside(int x, int y, int z) { return x >= X0 && x < X0 + NX && y >= Y0 && y < Y0 + NY && z >= Z0 && z < Z0 + NZ; }
    static int idx(int x, int y, int z) { return ((y - Y0) * NZ + (z - Z0)) * NX + (x - X0); }
    uint8_t get(int x, int y, int z) const {
        if (!inside(x, y, z)) return y < Y0 ? BK_BEDROCK : BK_AIR;
        return blk[idx(x, y, z)];
    }
    uint8_t getMeta(int x, int y, int z) const { return inside(x, y, z) ? meta[idx(x, y, z)] : 0; }
    void set(int x, int y, int z, uint8_t b, uint8_t m = 0);
    int groundHeight(int x, int z) const;  // generated terrain height (top face)

    void generate(uint32_t seed);
    void remesh(bool headless);         // rebuild the dirty chunks
    void destroyMeshes();
    // collision boxes of one block (forSelect: the box you aim at, even for non-solid blocks)
    int boxes(int x, int y, int z, AABB* out, bool forSelect) const;
    void query(const AABB& q, std::vector<AABB>& out) const;
    // DDA ray cast. solidOnly: only collidable blocks. Returns t or -1, with the hit cell and face normal.
    float raycast(const V3& o, const V3& d, float maxT, V3* nrm, int* hx, int* hy, int* hz, bool solidOnly) const;
    std::string saveDiff() const;
    void applyDiffLine(int x, int y, int z, int b, int m);
};
