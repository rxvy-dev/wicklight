// WICKLIGHT - Wick's Island block world: generation, meshing, collision, ray casts and save diffs.
#include "voxel.h"
#include <cmath>
#include <cstdio>
#include <sstream>
#include <algorithm>

// name, side, top, bottom, front, hard, shape, solid, opaque, emis, tint, light, drop
static const BlockDef DEFS[BK_COUNT] = {
    {"Air", 0, 0, 0, -1, -1, SH_NONE, false, false, 0, 0xffffff, 0, -1},
    {"Grass", T_GRASS_SIDE, T_GRASS, T_DIRT, -1, 0.45f, SH_CUBE, true, true, 0, 0xffffff, 0, BK_DIRT},
    {"Dirt", T_DIRT, T_DIRT, T_DIRT, -1, 0.4f, SH_CUBE, true, true, 0, 0xffffff, 0, 0},
    {"Stone", T_STONE, T_STONE, T_STONE, -1, 1.0f, SH_CUBE, true, true, 0, 0xffffff, 0, BK_COBBLE},
    {"Cobblestone", T_COBBLE, T_COBBLE, T_COBBLE, -1, 1.1f, SH_CUBE, true, true, 0, 0xffffff, 0, 0},
    {"Sand", T_SAND, T_SAND, T_SAND, -1, 0.35f, SH_CUBE, true, true, 0, 0xffffff, 0, 0},
    {"Log", T_BARK, T_LOG_TOP, T_LOG_TOP, -1, 0.8f, SH_CUBE, true, true, 0, 0xffffff, 0, 0},
    {"Planks", T_PLANK, T_PLANK, T_PLANK, -1, 0.7f, SH_CUBE, true, true, 0, 0xffffff, 0, 0},
    {"Leaves", T_LEAVES, T_LEAVES, T_LEAVES, -1, 0.15f, SH_CUBE, true, true, 0, 0xffffff, 0, -1},
    {"Glass", T_GLASS, T_GLASS, T_GLASS, -1, 0.3f, SH_CUBE, true, false, 0, 0xffffff, 0, 0},
    {"Stone Bricks", T_STONEBRICK, T_STONEBRICK, T_STONEBRICK, -1, 1.1f, SH_CUBE, true, true, 0, 0xffffff, 0, 0},
    {"Ember Ore", T_ORE_EMBER, T_ORE_EMBER, T_ORE_EMBER, -1, 1.3f, SH_CUBE, true, true, 0, 0xffffff, 0, -1},
    {"Crystal Ore", T_ORE_CRYSTAL, T_ORE_CRYSTAL, T_ORE_CRYSTAL, -1, 1.5f, SH_CUBE, true, true, 0, 0xffffff, 0, IT_CRYSTAL},
    {"Scrap Ore", T_ORE_SCRAP, T_ORE_SCRAP, T_ORE_SCRAP, -1, 1.2f, SH_CUBE, true, true, 0, 0xffffff, 0, IT_GEAR},
    {"Bedrock", T_OBSIDIAN, T_OBSIDIAN, T_OBSIDIAN, -1, -1, SH_CUBE, true, true, 0, 0x8070a0, 0, -1},
    {"Lamp", T_LAMPBLOCK, T_LAMPBLOCK, T_LAMPBLOCK, -1, 0.4f, SH_CUBE, true, true, 0, 0xffffff, 0xffd890, 0},
    {"Marble", T_MARBLE, T_MARBLE, T_MARBLE, -1, 1.1f, SH_CUBE, true, true, 0, 0xffffff, 0, 0},
    {"Roof Tiles", T_ROOF, T_ROOF, T_ROOF, -1, 0.8f, SH_CUBE, true, true, 0, 0xffffff, 0, 0},
    {"Metal Block", T_METAL, T_METAL, T_METAL, -1, 1.2f, SH_CUBE, true, true, 0, 0xffffff, 0, 0},
    {"Glow Crystal", T_CRYSTAL, T_CRYSTAL, T_CRYSTAL, -1, 0.6f, SH_CUBE, true, true, 0.3f, 0xffffff, 0x7ae8ff, 0},
    {"White Wax", T_WAX, T_WAX, T_WAX, -1, 0.5f, SH_CUBE, true, true, 0, 0xffffff, 0, 0},
    {"Red Wax", T_WAX, T_WAX, T_WAX, -1, 0.5f, SH_CUBE, true, true, 0, 0xff6a6a, 0, 0},
    {"Yellow Wax", T_WAX, T_WAX, T_WAX, -1, 0.5f, SH_CUBE, true, true, 0, 0xffe070, 0, 0},
    {"Green Wax", T_WAX, T_WAX, T_WAX, -1, 0.5f, SH_CUBE, true, true, 0, 0x88e088, 0, 0},
    {"Blue Wax", T_WAX, T_WAX, T_WAX, -1, 0.5f, SH_CUBE, true, true, 0, 0x7aa8ff, 0, 0},
    {"Purple Wax", T_WAX, T_WAX, T_WAX, -1, 0.5f, SH_CUBE, true, true, 0, 0xc08aff, 0, 0},
    {"Water", 0, 0, 0, -1, -1, SH_WATER, false, false, 0, 0xffffff, 0, -1},
    {"Bed", T_CLOTH, T_CLOTH, T_PLANK, -1, 0.4f, SH_BED, true, false, 0, 0xffffff, 0, 0},
    {"Chest", T_CHEST, T_CHEST, T_CHEST, -1, 0.6f, SH_CHEST, true, false, 0, 0xffffff, 0, 0},
    {"Door", T_DOOR, T_DOOR, T_DOOR, -1, 0.5f, SH_DOOR, true, false, 0, 0xffffff, 0, BK_DOOR},
    {"Door", T_DOOR, T_DOOR, T_DOOR, -1, 0.5f, SH_DOOR, true, false, 0, 0xffffff, 0, BK_DOOR},
    {"Ladder", T_LADDER, T_LADDER, T_LADDER, -1, 0.3f, SH_LADDER, false, false, 0, 0xffffff, 0, 0},
    {"Workbench", T_BENCH_SIDE, T_BENCH_TOP, T_PLANK, -1, 0.7f, SH_CUBE, true, true, 0, 0xffffff, 0, 0},
    {"Furnace", T_COBBLE, T_COBBLE, T_COBBLE, T_FURNACE, 1.1f, SH_CUBE, true, true, 0, 0xffffff, 0, 0},
    {"Jump Pad", T_METAL, T_JUMPPAD, T_METAL, -1, 0.6f, SH_SLAB, true, false, 0, 0xffffff, 0, 0},
    {"Ember Bomb", T_BOMB, T_BOMB, T_BOMB, -1, 0.2f, SH_CUBE, true, true, 0, 0xffffff, 0, 0},
    {"Sapling", T_SAPLING, T_SAPLING, T_SAPLING, -1, 0.05f, SH_CROSS, false, false, 0, 0xffffff, 0, 0},
    {"Red Flower", T_FLOWER_RED, T_FLOWER_RED, T_FLOWER_RED, -1, 0.05f, SH_CROSS, false, false, 0, 0xffffff, 0, 0},
    {"Yellow Flower", T_FLOWER_YELLOW, T_FLOWER_YELLOW, T_FLOWER_YELLOW, -1, 0.05f, SH_CROSS, false, false, 0, 0xffffff, 0, 0},
    {"Ember Forge", T_STONEBRICK, T_STONEBRICK, T_STONEBRICK, T_FORGE, 1.2f, SH_CUBE, true, true, 0, 0xffffff, 0xff8030, 0},
    {"Candle", T_WAX, T_WAX, T_WAX, -1, 0.1f, SH_CANDLE, false, false, 0, 0xffffff, 0xffb040, 0},
};

const BlockDef& blockDef(int b) { return DEFS[(b >= 0 && b < BK_COUNT) ? b : 0]; }

const char* itemName(int item) {
    if (item == 0) return "Flame";
    if (item == IT_CRYSTAL) return "Crystal";
    if (item == IT_GEAR) return "Gear";
    if (item == IT_WAX) return "Wax Lump";
    return blockDef(item).name;
}
int itemIconTile(int item) {
    switch (item) {
        case 0: return T_FLAME_ICON;
        case IT_CRYSTAL: return T_CRYSTAL_ICON;
        case IT_GEAR: return T_GEAR_ICON;
        case IT_WAX: return T_WAXLUMP_ICON;
        case BK_BED: return T_BED_ICON;
        case BK_DOOR: return T_DOOR_ICON;
        case BK_CANDLE: return T_CANDLE_ICON;
        case BK_GRASS: return T_GRASS_SIDE;
        case BK_FURNACE: return T_FURNACE;
        case BK_FORGE: return T_FORGE;
        case BK_JUMPPAD: return T_JUMPPAD;
        default: return blockDef(item).side;
    }
}
V3 itemIconTint(int item) { return (item > 0 && item < BK_COUNT) ? hexColor(blockDef(item).tint) : V3(1, 1, 1); }
bool isPlaceable(int item) { return item > 0 && item < BK_COUNT && item != BK_WATER && item != BK_DOOR_TOP && item != BK_BEDROCK; }

// ------------------------------------------------------------------ noise
static float h3(int x, int y, int z, uint32_t s) {
    return (hash32((uint32_t)x * 73856093u ^ (uint32_t)y * 19349663u ^ (uint32_t)z * 83492791u ^ s * 2654435761u) & 0xFFFF) / 65535.f;
}
static float smooth(float t) { return t * t * (3 - 2 * t); }
static float vnoise2(float x, float z, uint32_t s) {
    int xi = (int)std::floor(x), zi = (int)std::floor(z);
    float fx = smooth(x - xi), fz = smooth(z - zi);
    float a = h3(xi, 0, zi, s), b = h3(xi + 1, 0, zi, s), c = h3(xi, 0, zi + 1, s), d = h3(xi + 1, 0, zi + 1, s);
    return lerpf(lerpf(a, b, fx), lerpf(c, d, fx), fz);
}
static float fbm2(float x, float z, uint32_t s) {
    float v = 0, amp = 0.5f, tot = 0;
    for (int o = 0; o < 4; o++) { v += vnoise2(x, z, s + o * 101) * amp; tot += amp; x *= 2.03f; z *= 2.03f; amp *= 0.5f; }
    return v / tot;
}
static float vnoise3(float x, float y, float z, uint32_t s) {
    int xi = (int)std::floor(x), yi = (int)std::floor(y), zi = (int)std::floor(z);
    float fx = smooth(x - xi), fy = smooth(y - yi), fz = smooth(z - zi);
    float v[2][2][2];
    for (int a = 0; a < 2; a++) for (int b = 0; b < 2; b++) for (int c = 0; c < 2; c++) v[a][b][c] = h3(xi + a, yi + b, zi + c, s);
    float x00 = lerpf(v[0][0][0], v[1][0][0], fx), x10 = lerpf(v[0][1][0], v[1][1][0], fx);
    float x01 = lerpf(v[0][0][1], v[1][0][1], fx), x11 = lerpf(v[0][1][1], v[1][1][1], fx);
    return lerpf(lerpf(x00, x10, fy), lerpf(x01, x11, fy), fz);
}

// ------------------------------------------------------------------ world
void VoxWorld::set(int x, int y, int z, uint8_t b, uint8_t m) {
    if (!inside(x, y, z)) return;
    int i = idx(x, y, z);
    blk[i] = b; meta[i] = m;
    int cx = (x - X0) / CS, cz = (z - Z0) / CS;
    // mark this chunk and any neighbour whose faces / ambient occlusion touch this block
    for (int dz = -1; dz <= 1; dz++)
        for (int dx = -1; dx <= 1; dx++) {
            int nx = (x + dx - X0) / CS, nz = (z + dz - Z0) / CS;
            if (x + dx < X0 || z + dz < Z0 || nx >= NCX || nz >= NCZ) continue;
            if (nx != cx || nz != cz || (dx == 0 && dz == 0)) dirty[nz * NCX + nx] = true;
        }
    anyDirty = true;
}

int VoxWorld::groundHeight(int x, int z) const {
    if (x < X0 || z < Z0 || x >= X0 + NX || z >= Z0 + NZ) return -8;
    return height[(z - Z0) * NX + (x - X0)];
}

static const V3 PLAZA(0, 0, -21);

void VoxWorld::generate(uint32_t sd) {
    seed = sd;
    blk.assign((size_t)NX * NY * NZ, BK_AIR);
    meta.assign(blk.size(), 0);
    height.assign((size_t)NX * NZ, 0);
    // 1. terrain columns
    for (int z = Z0; z < Z0 + NZ; z++)
        for (int x = X0; x < X0 + NX; x++) {
            float dx = x + 0.5f, dz = z + 0.5f;
            float d = std::sqrt(dx * dx + dz * dz) / 52.f;
            float fall = clampf(1 - d * d, 0, 1);
            float n = fbm2(x * 0.05f, z * 0.05f, seed), m = fbm2(x * 0.11f + 50, z * 0.11f, seed + 1);
            float h = -7 + fall * (8.5f + n * 10 + (m - 0.5f) * 3);
            float pd = std::sqrt((x + 0.5f - PLAZA.x) * (x + 0.5f - PLAZA.x) + (z + 0.5f - PLAZA.z) * (z + 0.5f - PLAZA.z));
            if (pd < 12) h = pd < 7 ? 0.f : lerpf(0.f, h, (pd - 7) / 5.f);
            int H = std::max(Y0 + 4, std::min((int)std::floor(h + 0.5f), Y0 + NY - 14));
            height[(z - Z0) * NX + (x - X0)] = (int8_t)H;
            bool beach = H <= SEA + 1;
            for (int y = Y0; y < Y0 + NY; y++) {
                uint8_t b = BK_AIR;
                if (y == Y0) b = BK_BEDROCK;
                else if (y < H) {
                    int depth = H - 1 - y;
                    if (depth == 0) b = beach ? BK_SAND : BK_GRASS;
                    else if (depth <= 3) b = beach ? BK_SAND : BK_DIRT;
                    else b = BK_STONE;
                    if (depth == 0 && pd < 6.2f) b = BK_STONEBRICK;
                } else if (y <= SEA - 1) b = BK_WATER;
                blk[idx(x, y, z)] = b;
            }
        }
    // 2. caves and ore veins
    for (int z = Z0; z < Z0 + NZ; z++)
        for (int x = X0; x < X0 + NX; x++) {
            int H = height[(z - Z0) * NX + (x - X0)];
            float pd = std::sqrt((x - PLAZA.x) * (x - PLAZA.x) + (z - PLAZA.z) * (z - PLAZA.z));
            for (int y = Y0 + 2; y < H - 3; y++) {
                int i = idx(x, y, z);
                if (blk[i] != BK_STONE && blk[i] != BK_DIRT) continue;
                bool underSea = H <= SEA;
                float c = vnoise3(x * 0.085f, y * 0.13f, z * 0.085f, seed + 7);
                float c2 = vnoise3(x * 0.04f, y * 0.06f, z * 0.04f, seed + 9);
                bool nearTop = y > H - 6;
                if (!(underSea && y > H - 6) && !(pd < 10 && y > -7) && c > 0.7f - 0.12f * c2 + (nearTop ? 0.06f : 0)) { blk[i] = BK_AIR; continue; }
                if (blk[i] != BK_STONE) continue;
                float v = vnoise3(x * 0.3f, y * 0.3f, z * 0.3f, seed + 11);
                if (v > 0.74f) {
                    float r = h3(x, y, z, seed + 13);
                    if (y < -7 && r < 0.25f) blk[i] = BK_CRYSTAL_ORE;
                    else if (r < 0.55f) blk[i] = BK_EMBER_ORE;
                    else if (r < 0.75f && y < 1) blk[i] = BK_SCRAP_ORE;
                }
            }
        }
    // 3. trees and flowers
    for (int gz = Z0; gz < Z0 + NZ; gz += 6)
        for (int gx = X0; gx < X0 + NX; gx += 6) {
            if (h3(gx, 1, gz, seed + 17) > 0.62f) continue;
            int x = gx + 1 + (int)(h3(gx, 2, gz, seed) * 4), z = gz + 1 + (int)(h3(gx, 3, gz, seed) * 4);
            if (x < X0 + 3 || z < Z0 + 3 || x >= X0 + NX - 3 || z >= Z0 + NZ - 3) continue;
            int H = height[(z - Z0) * NX + (x - X0)];
            if (get(x, H - 1, z) != BK_GRASS) continue;
            float pd = std::sqrt((x - PLAZA.x) * (x - PLAZA.x) + (z - PLAZA.z) * (z - PLAZA.z));
            if (pd < 10) continue;
            int th = 4 + (int)(h3(x, 4, z, seed) * 3);
            for (int k = 0; k < th; k++) blk[idx(x, H + k, z)] = BK_LOG;
            int top = H + th - 1;
            for (int dy = -2; dy <= 1; dy++) {
                int r = dy >= 0 ? 1 : 2;
                for (int dz = -r; dz <= r; dz++)
                    for (int dx = -r; dx <= r; dx++) {
                        if (std::abs(dx) == r && std::abs(dz) == r && (dy == 1 || h3(x + dx, top + dy, z + dz, seed) < 0.5f)) continue;
                        if (!inside(x + dx, top + dy, z + dz)) continue;
                        int i = idx(x + dx, top + dy, z + dz);
                        if (blk[i] == BK_AIR) blk[i] = BK_LEAVES;
                    }
            }
            if (inside(x, top + 2, z) && blk[idx(x, top + 2, z)] == BK_AIR) blk[idx(x, top + 2, z)] = BK_LEAVES;
        }
    for (int z = Z0; z < Z0 + NZ; z++)
        for (int x = X0; x < X0 + NX; x++) {
            int H = height[(z - Z0) * NX + (x - X0)];
            if (get(x, H - 1, z) != BK_GRASS || get(x, H, z) != BK_AIR) continue;
            float r = h3(x, 5, z, seed + 19);
            if (r < 0.035f) blk[idx(x, H, z)] = r < 0.018f ? BK_FLOWER_RED : BK_FLOWER_YELLOW;
        }
    // 4. the landing plaza: workbench, furnace and four lamp pillars
    auto put = [&](int x, int y, int z, uint8_t b, uint8_t m) { if (inside(x, y, z)) { blk[idx(x, y, z)] = b; meta[idx(x, y, z)] = m; } };
    put(-4, 0, -25, BK_BENCH, 0);
    put(-5, 0, -25, BK_FURNACE, 0);
    for (int i = 0; i < 4; i++) {
        int px = (i & 1) ? 5 : -6, pz = (i & 2) ? -16 : -26;
        put(px, 0, pz, BK_STONEBRICK, 0); put(px, 1, pz, BK_STONEBRICK, 0); put(px, 2, pz, BK_LAMP, 0);
    }
    genBlk = blk; genMeta = meta;
    for (auto& d : dirty) d = true;
    anyDirty = true;
}

// ------------------------------------------------------------------ shapes
static const float DOOR_T = 0.15f, LADDER_T = 0.0625f;
static AABB doorBox(int m) {
    int f = m & 3; bool open = m & 4;
    float T = DOOR_T;
    if (!open) {
        if (f == 0) return AABB(V3(0, 0, 0), V3(1, 1, T));
        if (f == 2) return AABB(V3(0, 0, 1 - T), V3(1, 1, 1));
        if (f == 1) return AABB(V3(0, 0, 0), V3(T, 1, 1));
        return AABB(V3(1 - T, 0, 0), V3(1, 1, 1));
    }
    if (f == 0) return AABB(V3(0, 0, 0), V3(T, 1, 1));
    if (f == 2) return AABB(V3(1 - T, 0, 0), V3(1, 1, 1));
    if (f == 1) return AABB(V3(0, 0, 1 - T), V3(1, 1, 1));
    return AABB(V3(0, 0, 0), V3(1, 1, T));
}
static AABB ladderBox(int m, float t) {
    switch (m & 3) {
        case 0: return AABB(V3(0, 0, 1 - t), V3(1, 1, 1));
        case 1: return AABB(V3(1 - t, 0, 0), V3(1, 1, 1));
        case 2: return AABB(V3(0, 0, 0), V3(1, 1, t));
        default: return AABB(V3(0, 0, 0), V3(t, 1, 1));
    }
}

int VoxWorld::boxes(int x, int y, int z, AABB* out, bool forSelect) const {
    uint8_t b = get(x, y, z);
    if (b == BK_AIR || b == BK_WATER) return 0;
    const BlockDef& d = DEFS[b];
    if (!d.solid && !forSelect) return 0;
    V3 o((float)x, (float)y, (float)z);
    AABB a(V3(0, 0, 0), V3(1, 1, 1));
    switch (d.shape) {
        case SH_BED: a.hi.y = 0.5625f; break;
        case SH_CHEST: a = AABB(V3(0.0625f, 0, 0.0625f), V3(0.9375f, 0.875f, 0.9375f)); break;
        case SH_SLAB: a.hi.y = 0.375f; break;
        case SH_DOOR: a = doorBox(getMeta(x, y, z)); break;
        case SH_LADDER: a = ladderBox(getMeta(x, y, z), 0.2f); break;
        case SH_CROSS: a = AABB(V3(0.2f, 0, 0.2f), V3(0.8f, 0.8f, 0.8f)); break;
        case SH_CANDLE: a = AABB(V3(0.3f, 0, 0.3f), V3(0.7f, 0.7f, 0.7f)); break;
        default: break;
    }
    out[0] = AABB(o + a.lo, o + a.hi);
    return 1;
}

void VoxWorld::query(const AABB& q, std::vector<AABB>& out) const {
    if (!active) return;
    int x0 = (int)std::floor(q.lo.x), x1 = (int)std::floor(q.hi.x);
    int y0 = (int)std::floor(q.lo.y), y1 = (int)std::floor(q.hi.y);
    int z0 = (int)std::floor(q.lo.z), z1 = (int)std::floor(q.hi.z);
    AABB bx[2];
    for (int y = y0; y <= y1; y++)
        for (int z = z0; z <= z1; z++)
            for (int x = x0; x <= x1; x++) {
                int n = boxes(x, y, z, bx, false);
                for (int k = 0; k < n; k++) if (bx[k].overlaps(q)) out.push_back(bx[k]);
            }
}

float VoxWorld::raycast(const V3& o, const V3& d, float maxT, V3* nrm, int* hx, int* hy, int* hz, bool solidOnly) const {
    if (!active) return -1;
    int x = (int)std::floor(o.x), y = (int)std::floor(o.y), z = (int)std::floor(o.z);
    int sx = d.x > 0 ? 1 : -1, sy = d.y > 0 ? 1 : -1, sz = d.z > 0 ? 1 : -1;
    auto first = [](float p, float dd, int c) { if (std::fabs(dd) < 1e-9f) return 1e30f; float b = dd > 0 ? c + 1 - p : p - c; return b / std::fabs(dd); };
    float tx = first(o.x, d.x, x), ty = first(o.y, d.y, y), tz = first(o.z, d.z, z);
    float dx = std::fabs(d.x) < 1e-9f ? 1e30f : 1 / std::fabs(d.x), dy = std::fabs(d.y) < 1e-9f ? 1e30f : 1 / std::fabs(d.y), dz = std::fabs(d.z) < 1e-9f ? 1e30f : 1 / std::fabs(d.z);
    AABB bx[2];
    for (int it = 0; it < 400; it++) {
        int n = boxes(x, y, z, bx, !solidOnly);
        float best = -1; V3 bn;
        for (int k = 0; k < n; k++) {
            V3 nn;
            float t = rayAABB(o, d, bx[k], maxT, &nn);
            if (t >= 0 && (best < 0 || t < best)) { best = t; bn = nn; }
        }
        if (best >= 0) {
            if (nrm) *nrm = bn;
            if (hx) { *hx = x; *hy = y; *hz = z; }
            return best;
        }
        float tm = std::min(tx, std::min(ty, tz));
        if (tm > maxT) break;
        if (tx <= ty && tx <= tz) { x += sx; tx += dx; }
        else if (ty <= tz) { y += sy; ty += dy; }
        else { z += sz; tz += dz; }
        if (y < Y0 - 1 || y > Y0 + NY + 1) break;
    }
    return -1;
}

// ------------------------------------------------------------------ meshing
static const int FACE_N[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
// corners: bottom-left, bottom-right, top-right, top-left seen from outside (CCW)
static const int FACE_C[6][4][3] = {
    {{1, 0, 1}, {1, 0, 0}, {1, 1, 0}, {1, 1, 1}},
    {{0, 0, 0}, {0, 0, 1}, {0, 1, 1}, {0, 1, 0}},
    {{0, 1, 1}, {1, 1, 1}, {1, 1, 0}, {0, 1, 0}},
    {{0, 0, 0}, {1, 0, 0}, {1, 0, 1}, {0, 0, 1}},
    {{0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}},
    {{1, 0, 0}, {0, 0, 0}, {0, 1, 0}, {1, 1, 0}},
};
static const float FACE_UV[4][2] = {{0, 1}, {1, 1}, {1, 0}, {0, 0}};

static bool occluder(const VoxWorld& w, int x, int y, int z) {
    uint8_t b = w.get(x, y, z);
    return b != BK_AIR && DEFS[b].opaque && DEFS[b].shape == SH_CUBE;
}

static void pushV(std::vector<Vertex>& v, V3 p, const int* n, float u, float vv, int tile, V3 c, float e) {
    Vertex q; q.px = p.x; q.py = p.y; q.pz = p.z; q.nx = (float)n[0]; q.ny = (float)n[1]; q.nz = (float)n[2];
    q.u = u; q.v = vv; q.tile = (float)tile; q.r = c.x; q.g = c.y; q.b = c.z; q.emis = e;
    v.push_back(q);
}

static void cubeFaces(const VoxWorld& w, std::vector<Vertex>& out, int x, int y, int z, uint8_t b) {
    const BlockDef& d = DEFS[b];
    V3 tint = hexColor(d.tint);
    int f = w.getMeta(x, y, z) & 3;
    static const int FRONT_FACE[4] = {5, 1, 4, 0};  // facing +z -> front looks -z, etc.
    for (int fi = 0; fi < 6; fi++) {
        const int* n = FACE_N[fi];
        int nx = x + n[0], ny = y + n[1], nz = z + n[2];
        uint8_t nb = w.get(nx, ny, nz);
        if (nb != BK_AIR && nb != BK_WATER) {
            const BlockDef& nd = DEFS[nb];
            if (nd.shape == SH_CUBE && (nd.opaque || nb == b)) continue;
        }
        int tile = fi == 2 ? d.top : (fi == 3 ? d.bottom : d.side);
        if (d.front >= 0 && fi == FRONT_FACE[f]) tile = d.front;
        // ambient occlusion
        float ao[4];
        int axis = n[0] ? 0 : (n[1] ? 1 : 2);
        for (int c = 0; c < 4; c++) {
            int cc[3] = {FACE_C[fi][c][0], FACE_C[fi][c][1], FACE_C[fi][c][2]};
            int s1[3] = {nx, ny, nz}, s2[3] = {nx, ny, nz}, co[3] = {nx, ny, nz};
            int a1 = (axis + 1) % 3, a2 = (axis + 2) % 3;
            s1[a1] += cc[a1] ? 1 : -1;
            s2[a2] += cc[a2] ? 1 : -1;
            co[a1] += cc[a1] ? 1 : -1; co[a2] += cc[a2] ? 1 : -1;
            int o1 = occluder(w, s1[0], s1[1], s1[2]), o2 = occluder(w, s2[0], s2[1], s2[2]), o3 = occluder(w, co[0], co[1], co[2]);
            int a = (o1 && o2) ? 3 : o1 + o2 + o3;
            ao[c] = 1.f - a * 0.16f;
        }
        V3 P[4];
        for (int c = 0; c < 4; c++) P[c] = V3((float)(x + FACE_C[fi][c][0]), (float)(y + FACE_C[fi][c][1]), (float)(z + FACE_C[fi][c][2]));
        static const int A[6] = {0, 1, 2, 0, 2, 3}, B[6] = {1, 2, 3, 1, 3, 0};
        const int* ord = (ao[0] + ao[2] < ao[1] + ao[3]) ? B : A;
        for (int k = 0; k < 6; k++) { int c = ord[k]; pushV(out, P[c], n, FACE_UV[c][0], FACE_UV[c][1], tile, tint * ao[c], d.emis); }
    }
}

static void specialShape(const VoxWorld& w, MeshBuilder& mb, int x, int y, int z, uint8_t b) {
    const BlockDef& d = DEFS[b];
    V3 o((float)x, (float)y, (float)z);
    int m = w.getMeta(x, y, z);
    mb.color = hexColor(d.tint); mb.emis = 0;
    switch (d.shape) {
        case SH_BED: {
            int f = m & 3;
            mb.box(AABB(o + V3(0, 0, 0), o + V3(1, 0.3f, 1)), T_PLANK);
            mb.color = hexColor(0xd04848);
            mb.box(AABB(o + V3(0.03f, 0.3f, 0.03f), o + V3(0.97f, 0.5f, 0.97f)), T_CLOTH, 1, T_CLOTH, false);
            // pillow at the head (the end away from the person who placed it)
            V3 head = o + V3(0.5f, 0, 0.5f) + V3(f == 1 ? 0.3f : (f == 3 ? -0.3f : 0), 0, f == 0 ? 0.3f : (f == 2 ? -0.3f : 0));
            mb.color = hexColor(0xf4f0e4);
            bool alongZ = f == 0 || f == 2;
            V3 hs = alongZ ? V3(0.38f, 0.07f, 0.16f) : V3(0.16f, 0.07f, 0.38f);
            mb.box(AABB(head + V3(-hs.x, 0.5f, -hs.z), head + V3(hs.x, 0.5f + hs.y * 2, hs.z)), T_WHITE, 1, -1, false);
        } break;
        case SH_CHEST:
            mb.box(AABB(o + V3(0.0625f, 0, 0.0625f), o + V3(0.9375f, 0.875f, 0.9375f)), T_CHEST, 1, T_PLANK);
            break;
        case SH_DOOR: {
            AABB a = doorBox(m);
            mb.box(AABB(o + a.lo, o + a.hi), T_DOOR, 1, T_DARKWOOD);
            if (b == BK_DOOR) {  // handle
                mb.color = hexColor(0xd8c070);
                V3 c = (a.lo + a.hi) * 0.5f;
                V3 ext = a.hi - a.lo;
                V3 hp = o + c + V3(ext.x > 0.5f ? 0.3f : 0, 0.45f, ext.z > 0.5f ? 0.3f : 0);
                mb.box(AABB(hp - V3(0.06f, 0.06f, 0.06f) - V3(ext.x < 0.5f ? 0.08f : 0, 0, ext.z < 0.5f ? 0.08f : 0),
                            hp + V3(0.06f, 0.06f, 0.06f) + V3(ext.x < 0.5f ? 0.08f : 0, 0, ext.z < 0.5f ? 0.08f : 0)), T_WHITE);
            }
        } break;
        case SH_LADDER: {
            AABB a = ladderBox(m, LADDER_T);
            mb.box(AABB(o + a.lo, o + a.hi), T_LADDER, 1, T_PLANK, true);
        } break;
        case SH_SLAB:
            mb.box(AABB(o, o + V3(1, 0.375f, 1)), d.side, 1, d.top);
            break;
        case SH_CROSS: {
            // two crossed double-sided quads
            V3 a0 = o + V3(0.1f, 0, 0.1f), a1 = o + V3(0.9f, 0, 0.9f), b0 = o + V3(0.9f, 0, 0.1f), b1 = o + V3(0.1f, 0, 0.9f);
            V3 up(0, 0.9f, 0);
            V3 n1 = norm(V3(1, 0, -1)), n2 = norm(V3(1, 0, 1));
            mb.quad(a0, a1, a1 + up, a0 + up, n1, 1, 1, d.side);
            mb.quad(a1, a0, a0 + up, a1 + up, n1 * -1.f, 1, 1, d.side);
            mb.quad(b0, b1, b1 + up, b0 + up, n2, 1, 1, d.side);
            mb.quad(b1, b0, b0 + up, b1 + up, n2 * -1.f, 1, 1, d.side);
        } break;
        case SH_CANDLE:
            mb.box(AABB(o + V3(0.36f, 0, 0.36f), o + V3(0.64f, 0.5f, 0.64f)), T_WAX, 1);
            mb.color = hexColor(0xffc050); mb.emis = 2;
            mb.box(AABB(o + V3(0.45f, 0.5f, 0.45f), o + V3(0.55f, 0.7f, 0.55f)), T_WHITE, 1);
            mb.emis = 0;
            break;
        default: break;
    }
    mb.color = V3(1, 1, 1);
}

void VoxWorld::remesh(bool headless) {
    if (!anyDirty) return;
    anyDirty = false;
    for (int cz = 0; cz < NCZ; cz++)
        for (int cx = 0; cx < NCX; cx++) {
            int ci = cz * NCX + cx;
            if (!dirty[ci]) continue;
            dirty[ci] = false;
            if (headless) continue;
            std::vector<Vertex> v;
            MeshBuilder mb, wb;
            for (int y = Y0; y < Y0 + NY; y++)
                for (int z = Z0 + cz * CS; z < Z0 + (cz + 1) * CS; z++)
                    for (int x = X0 + cx * CS; x < X0 + (cx + 1) * CS; x++) {
                        uint8_t b = blk[idx(x, y, z)];
                        if (b == BK_AIR) continue;
                        if (b == BK_WATER) {
                            uint8_t up = get(x, y + 1, z);
                            float top = up == BK_WATER ? y + 1.f : y + 0.88f;
                            V3 o((float)x, (float)y, (float)z);
                            if (up != BK_WATER && !(up != BK_AIR && DEFS[up].opaque && DEFS[up].shape == SH_CUBE))
                                wb.quad(o + V3(0, 0.88f, 1), o + V3(1, 0.88f, 1), o + V3(1, 0.88f, 0), o + V3(0, 0.88f, 0), V3(0, 1, 0), 1, 1, 0);
                            for (int fi : {0, 1, 4, 5}) {  // walls of water next to dug-out air
                                const int* n = FACE_N[fi];
                                uint8_t nb = get(x + n[0], y, z + n[2]);
                                if (nb != BK_AIR && !(DEFS[nb].shape != SH_CUBE && nb != BK_WATER)) continue;
                                if (!inside(x + n[0], y, z + n[2])) continue;
                                V3 P[4];
                                for (int c = 0; c < 4; c++) P[c] = V3((float)(x + FACE_C[fi][c][0]), FACE_C[fi][c][1] ? top : (float)y, (float)(z + FACE_C[fi][c][2]));
                                wb.quad(P[0], P[1], P[2], P[3], V3((float)n[0], 0, (float)n[2]), 1, 1, 0);
                            }
                            continue;
                        }
                        if (DEFS[b].shape == SH_CUBE) cubeFaces(*this, v, x, y, z, b);
                        else specialShape(*this, mb, x, y, z, b);
                    }
            v.insert(v.end(), mb.v.begin(), mb.v.end());
            solid[ci].destroy(); water[ci].destroy();
            if (!v.empty()) solid[ci].upload(v);
            if (!wb.v.empty()) water[ci].upload(wb.v);
        }
}

void VoxWorld::destroyMeshes() {
    for (int i = 0; i < NCX * NCZ; i++) { solid[i].destroy(); water[i].destroy(); }
}

// ------------------------------------------------------------------ saves: only what differs from the generated island
std::string VoxWorld::saveDiff() const {
    std::ostringstream o;
    if (genBlk.size() != blk.size()) return "";
    for (size_t i = 0; i < blk.size(); i++) {
        if (blk[i] == genBlk[i] && meta[i] == genMeta[i]) continue;
        int x = (int)(i % NX) + X0, z = (int)((i / NX) % NZ) + Z0, y = (int)(i / ((size_t)NX * NZ)) + Y0;
        o << "vx " << x << " " << y << " " << z << " " << (int)blk[i] << " " << (int)meta[i] << "\n";
    }
    return o.str();
}

void VoxWorld::applyDiffLine(int x, int y, int z, int b, int m) {
    if (!inside(x, y, z) || b < 0 || b >= BK_COUNT) return;
    blk[idx(x, y, z)] = (uint8_t)b; meta[idx(x, y, z)] = (uint8_t)m;
}
