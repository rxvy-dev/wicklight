// WICKLIGHT - Wick's Island: a block world unlocked after clearing 10 depths of the Endless Dark.
// Mine blocks, collect them, craft, and place them - including things you can use:
// beds, doors, chests, ladders, jump pads, ember bombs, lamps, forges. Gloomlings come out at night.
#include "builder.h"
#include <algorithm>
#include <cstdio>
#include <sstream>

static V4 C4(uint32_t h, float a = 1) { V3 c = hexColor(h); return V4(c.x, c.y, c.z, a); }
static const float DAY_LEN = 600.f;  // seconds for a whole day + night
static const float REACH = 5.5f;
static const V3 PLAZA_C(0, 0, -21);
static Rng erng(9191);

// ------------------------------------------------------------------ crafting
struct Recipe { int out, n; int in[3][2]; int station; };  // station: 0 anywhere, 1 workbench, 2 furnace
static const Recipe RECIPES[] = {
    {BK_PLANKS, 4, {{BK_LOG, 1}}, 0},
    {BK_BENCH, 1, {{BK_PLANKS, 4}}, 0},
    {BK_CANDLE, 2, {{IT_WAX, 1}}, 0},
    {BK_WAX_WHITE, 2, {{IT_WAX, 2}}, 0},
    {BK_LADDER, 4, {{BK_PLANKS, 3}}, 1},
    {BK_DOOR, 1, {{BK_PLANKS, 6}}, 1},
    {BK_CHEST, 1, {{BK_PLANKS, 8}}, 1},
    {BK_BED, 1, {{BK_PLANKS, 3}, {BK_LEAVES, 3}}, 1},
    {BK_FURNACE, 1, {{BK_COBBLE, 8}}, 1},
    {BK_STONEBRICK, 4, {{BK_STONE, 4}}, 1},
    {BK_MARBLE, 2, {{BK_STONE, 2}, {BK_SAND, 1}}, 1},
    {BK_LAMP, 2, {{BK_GLASS, 2}, {IT_CRYSTAL, 1}}, 1},
    {BK_GLOWCRYSTAL, 1, {{IT_CRYSTAL, 3}}, 1},
    {BK_JUMPPAD, 1, {{IT_GEAR, 2}, {BK_PLANKS, 2}}, 1},
    {BK_BOMB, 2, {{BK_SAND, 2}, {IT_GEAR, 1}}, 1},
    {BK_METAL, 1, {{IT_GEAR, 4}}, 1},
    {BK_FORGE, 1, {{BK_COBBLE, 8}, {IT_CRYSTAL, 2}, {IT_GEAR, 2}}, 1},
    {BK_WAX_RED, 2, {{BK_WAX_WHITE, 2}, {BK_FLOWER_RED, 1}}, 1},
    {BK_WAX_YELLOW, 2, {{BK_WAX_WHITE, 2}, {BK_FLOWER_YELLOW, 1}}, 1},
    {BK_WAX_GREEN, 2, {{BK_WAX_WHITE, 2}, {BK_LEAVES, 1}}, 1},
    {BK_WAX_BLUE, 2, {{BK_WAX_WHITE, 2}, {IT_CRYSTAL, 1}}, 1},
    {BK_WAX_PURPLE, 2, {{BK_WAX_RED, 1}, {BK_WAX_BLUE, 1}}, 1},
    {BK_GLASS, 1, {{BK_SAND, 1}}, 2},
    {BK_STONE, 1, {{BK_COBBLE, 1}}, 2},
    {BK_ROOF, 2, {{BK_DIRT, 2}}, 2},
};
static const int NREC = (int)(sizeof(RECIPES) / sizeof(RECIPES[0]));
static const char* STATION_NAME[3] = {"", "Workbench", "Furnace"};

// Bramble's shop: things you can buy with embers
struct Offer { int item, n, price; };
static const Offer OFFERS[] = {
    {BK_LOG, 8, 30}, {BK_COBBLE, 16, 30}, {BK_SAND, 8, 25}, {BK_GLASS, 8, 45}, {IT_WAX, 6, 40}, {IT_CRYSTAL, 2, 50},
    {IT_GEAR, 2, 50}, {BK_LAMP, 4, 70}, {BK_BED, 1, 60}, {BK_CHEST, 1, 45}, {BK_DOOR, 1, 35}, {BK_LADDER, 8, 30},
    {BK_JUMPPAD, 1, 80}, {BK_BOMB, 3, 70}, {BK_SAPLING, 4, 25}, {BK_CANDLE, 6, 30}, {BK_MARBLE, 16, 60}, {BK_ROOF, 16, 40},
};
static const int NOFFER = (int)(sizeof(OFFERS) / sizeof(OFFERS[0]));

static int lookDir(float yaw) {  // 0 +z, 1 +x, 2 -z, 3 -x
    float s = std::sin(yaw), c = std::cos(yaw);
    if (std::fabs(c) >= std::fabs(s)) return c > 0 ? 0 : 2;
    return s > 0 ? 1 : 3;
}
static V3 blockColor(int b) {
    switch (b) {
        case BK_GRASS: return hexColor(0x4a7a44);
        case BK_DIRT: return hexColor(0x5a4230);
        case BK_SAND: return hexColor(0xb8a878);
        case BK_LOG: case BK_PLANKS: case BK_BENCH: case BK_CHEST: case BK_DOOR: case BK_DOOR_TOP: case BK_LADDER: return hexColor(0x9a7448);
        case BK_LEAVES: case BK_SAPLING: return hexColor(0x3c7a44);
        case BK_GLASS: return hexColor(0xc8e4ec);
        case BK_EMBER_ORE: return hexColor(0xff7a2a);
        case BK_CRYSTAL_ORE: case BK_GLOWCRYSTAL: return hexColor(0x7ae8ff);
        case BK_LAMP: case BK_CANDLE: return hexColor(0xffd890);
        case BK_BED: case BK_BOMB: case BK_FLOWER_RED: return hexColor(0xd04848);
        case BK_FLOWER_YELLOW: return hexColor(0xf0d040);
        default: if (b >= BK_WAX_WHITE && b <= BK_WAX_PURPLE) return hexColor(blockDef(b).tint) * 0.9f; return hexColor(0x7a7884);
    }
}

// ------------------------------------------------------------------ the level around the blocks
void buildHome(Level& L, Game& g) {
    LB b(L);
    L.name = "Wick's Island";
    L.song = SONG_ENDING;
    L.waterY = -1000;
    L.killY = VoxWorld::Y0 - 8;
    g.ensureVox();
    g.vox.active = true;
    g.updateDayNight(0);
    b.spawn(V3(0.5f, 0, -19.5f), 0);
    // portal home
    b.arch(V3(0.5f, 0, -27.f), 4, 4.2f, 1, T_MARBLE, true);
    b.exit(AABB(V3(-1.1f, 0, -27.8f), V3(2.1f, 4, -26.6f)), LV_HUB, 7, "", "", "Back to Lumen Hollow");
    // Bramble's supply stall
    b.solid(AABB(V3(2.6f, 0, -25.4f), V3(5.4f, 1.0f, -24.4f)), T_PLANK, T_PLANK);
    b.crate(V3(2.2f, 0, -26.0f), 0.8f);
    b.talker(V3(4.0f, 0, -26.1f), 0, 10);
    b.hint(AABB(V3(-7, -1, -27), V3(7, 4, -15)),
           "Hold [{THROW}] to MINE, [{WARP}] to PLACE or USE.  [1]-[9] or the wheel pick a block.  [{JOURNAL}] = inventory & crafting.");
    // invisible walls at the edge of the world
    float x0 = (float)VoxWorld::X0, x1 = (float)(VoxWorld::X0 + VoxWorld::NX), z0 = (float)VoxWorld::Z0, z1 = (float)(VoxWorld::Z0 + VoxWorld::NZ);
    float y0 = (float)VoxWorld::Y0 - 4, y1 = (float)(VoxWorld::Y0 + VoxWorld::NY) + 30;
    b.solid(AABB(V3(x0 - 1, y0, z0 - 1), V3(x0 + 0.5f, y1, z1 + 1)), T_WHITE, -1, V3(1, 1, 1), false);
    b.solid(AABB(V3(x1 - 0.5f, y0, z0 - 1), V3(x1 + 1, y1, z1 + 1)), T_WHITE, -1, V3(1, 1, 1), false);
    b.solid(AABB(V3(x0 - 1, y0, z0 - 1), V3(x1 + 1, y1, z0 + 0.5f)), T_WHITE, -1, V3(1, 1, 1), false);
    b.solid(AABB(V3(x0 - 1, y0, z1 - 0.5f), V3(x1 + 1, y1, z1 + 1)), T_WHITE, -1, V3(1, 1, 1), false);
    b.emitter(AABB(V3(-30, 1, -30), V3(30, 8, 30)), 0, 6);
}

void Game::ensureVox() {
    if (save.voxSeed == 0) save.voxSeed = (hash32((uint32_t)(save.playTime * 1000) + 977u) | 1u);
    if (!vox.blk.empty() && vox.seed == save.voxSeed) return;
    vox.destroyMeshes();
    vox.generate(save.voxSeed);
    for (auto& l : save.voxLines) {
        std::istringstream ls(l);
        std::string k; int x, y, z, b, m;
        if (ls >> k >> x >> y >> z >> b >> m) vox.applyDiffLine(x, y, z, b, m);
    }
    for (auto& d : vox.dirty) d = true;
    vox.anyDirty = true;
    voxLightsDirty = true;
}

// ------------------------------------------------------------------ day and night
bool Game::isNight() const { float t = save.dayT; return t > 0.6f && t < 0.96f; }

void Game::updateDayNight(float dt) {
    save.dayT += dt / DAY_LEN;
    if (save.dayT >= 1) save.dayT -= 1;
    float t = save.dayT;
    float day = t < 0.55f ? 1.f : (t < 0.63f ? 1 - (t - 0.55f) / 0.08f : (t < 0.92f ? 0.f : (t - 0.92f) / 0.08f));
    day = smoothstep(0, 1, day);
    float dusk = std::max(0.f, 1 - std::fabs(day - 0.5f) * 2.5f);
    Environment& e = lv.env;
    e.skyTop = lerp(hexColor(0x060a1e), hexColor(0x3a6aaa), day);
    e.skyHorizon = lerp(hexColor(0x1a2048), hexColor(0xa8c8e0), day);
    e.skyHorizon = lerp(e.skyHorizon, hexColor(0xe08a5a), dusk * 0.8f);
    e.fogColor = lerp(hexColor(0x0c1022), hexColor(0x9ab4c8), day);
    e.fogColor = lerp(e.fogColor, hexColor(0x9a6a6a), dusk * 0.6f);
    e.fogDensity = lerpf(0.022f, 0.009f, day);
    e.fogStart = 10;
    e.ambSky = lerp(V3(0.13f, 0.14f, 0.24f), V3(0.55f, 0.58f, 0.62f), day);
    e.ambGround = lerp(V3(0.05f, 0.05f, 0.09f), V3(0.26f, 0.24f, 0.22f), day);
    e.sunDir = norm(V3(-0.5f, -0.75f, 0.35f));
    e.sunColor = lerp(V3(0.14f, 0.16f, 0.3f), V3(0.95f, 0.85f, 0.7f), day);
    e.sunColor = lerp(e.sunColor, V3(0.9f, 0.5f, 0.3f), dusk * 0.6f);
    e.stars = 1 - day;
    e.moonSize = day < 0.5f ? 0.06f : 0.f;
}

// ------------------------------------------------------------------ inventory
void Game::addItem(int item, int n, bool announce) {
    if (n <= 0) return;
    save.inv[item] += n;
    bool onBar = false;
    for (int i = 1; i < 9; i++) if (save.hot[i] == item) onBar = true;
    if (!onBar)
        for (int i = 1; i < 9; i++) if (save.hot[i] == 0) { save.hot[i] = item; break; }
    if (announce) floats.push_back({pl.pos + V3(0, 2.0f, 0), "+" + std::to_string(n) + " " + itemName(item), V3(0.9f, 0.95f, 1.0f)});
}

bool Game::takeItem(int item, int n) {
    auto it = save.inv.find(item);
    if (it == save.inv.end() || it->second < n) return false;
    it->second -= n;
    if (it->second <= 0) {
        save.inv.erase(it);
        for (int i = 1; i < 9; i++) if (save.hot[i] == item) save.hot[i] = 0;
    }
    return true;
}

// old saves: blueprint buildings and raw resources turn into blocks and items
void Game::migrateOldIsland() {
    static const int OLD_COST[15][4] = {{0, 1, 0, 0}, {2, 0, 0, 0}, {1, 0, 0, 0}, {2, 0, 0, 0}, {0, 1, 1, 0}, {3, 0, 0, 0}, {0, 0, 3, 0}, {0, 10, 4, 0},
                                        {10, 6, 2, 0}, {8, 0, 0, 2}, {0, 8, 3, 0}, {4, 12, 0, 0}, {0, 10, 0, 4}, {0, 6, 0, 6}, {0, 20, 8, 4}};
    int r[4] = {save.res[0], save.res[1], save.res[2], save.res[3]};
    for (auto& p : save.builds) if (p.type >= 0 && p.type < 15) for (int k = 0; k < 4; k++) r[k] += OLD_COST[p.type][k];
    save.builds.clear();
    for (int& v : save.res) v = 0;
    static const int MAP[4] = {BK_LOG, BK_COBBLE, IT_CRYSTAL, IT_GEAR};
    for (int k = 0; k < 4; k++) if (r[k] > 0) addItem(MAP[k], r[k], false);
}

// ------------------------------------------------------------------ placing, mining, using
bool Game::placeBlock(int item, int x, int y, int z, int facing, int ladderSide) {
    if (!isPlaceable(item) || itemCount(item) <= 0 || !VoxWorld::inside(x, y, z)) return false;
    auto freeCell = [&](int cx, int cy, int cz) { uint8_t c = vox.get(cx, cy, cz); return VoxWorld::inside(cx, cy, cz) && (c == BK_AIR || c == BK_WATER); };
    if (!freeCell(x, y, z)) return false;
    auto solidBelow = [&]() { uint8_t c = vox.get(x, y - 1, z); return c != BK_AIR && c != BK_WATER && blockDef(c).solid; };
    int m = facing & 3;
    if ((item == BK_SAPLING || item == BK_FLOWER_RED || item == BK_FLOWER_YELLOW)) {
        uint8_t c = vox.get(x, y - 1, z);
        if (c != BK_GRASS && c != BK_DIRT) { statusMsg = "Plants need grass or dirt underneath"; statusT = 1.5f; return false; }
    }
    if (item == BK_CANDLE && !solidBelow()) return false;
    if (item == BK_DOOR) {
        if (!freeCell(x, y + 1, z) || !solidBelow()) { statusMsg = "A door needs 2 free blocks of space and floor below"; statusT = 1.5f; return false; }
    }
    if (item == BK_LADDER) {
        static const int D[4][2] = {{0, 1}, {1, 0}, {0, -1}, {-1, 0}};
        uint8_t w = vox.get(x + D[ladderSide][0], y, z + D[ladderSide][1]);
        if (w == BK_AIR || w == BK_WATER || !blockDef(w).solid) { statusMsg = "Ladders go against a wall"; statusT = 1.5f; return false; }
        m = ladderSide;
    }
    uint8_t prev = vox.get(x, y, z), prevM = vox.getMeta(x, y, z);
    vox.set(x, y, z, (uint8_t)item, (uint8_t)m);
    // don't build inside yourself or a gloomling
    if (blockDef(item).solid) {
        AABB bx[2];
        int n = vox.boxes(x, y, z, bx, false);
        AABB me(pl.pos - V3(PLAYER_R, 0, PLAYER_R), pl.pos + V3(PLAYER_R, PLAYER_H, PLAYER_R));
        bool blocked = false;
        for (int k = 0; k < n; k++) {
            if (bx[k].overlaps(me)) blocked = true;
            for (auto& e : enemies) if (e.alive && bx[k].overlaps(AABB(e.pos - V3(e.radius, 0, e.radius), e.pos + V3(e.radius, e.height, e.radius)))) blocked = true;
        }
        if (blocked) { vox.set(x, y, z, prev, prevM); return false; }
    }
    if (item == BK_DOOR) vox.set(x, y + 1, z, BK_DOOR_TOP, (uint8_t)m);
    takeItem(item, 1);
    if (item == BK_CHEST) save.chests[VoxWorld::idx(x, y, z)];
    if (item == BK_FORGE && save.forgeTime <= 0) save.forgeTime = save.playTime;
    voxLightsDirty = true;
    audio::play(SFX_STEP, 0.9f, 0.7f);
    burst(V3(x + 0.5f, y + 0.5f, z + 0.5f), blockColor(item), 6, 2, 0.06f, 0, 0.35f, 6);
    if (!has("isle.placed")) setFlag("isle.placed");
    return true;
}

bool Game::mineBlock(int x, int y, int z, bool drops) {
    uint8_t b = vox.get(x, y, z);
    if (b == BK_AIR || b == BK_WATER || blockDef(b).hard < 0) return false;
    // doors are two blocks tall
    if (b == BK_DOOR && vox.get(x, y + 1, z) == BK_DOOR_TOP) vox.set(x, y + 1, z, BK_AIR);
    if (b == BK_DOOR_TOP) { if (vox.get(x, y - 1, z) == BK_DOOR) vox.set(x, y - 1, z, BK_AIR); b = BK_DOOR; }
    int key = VoxWorld::idx(x, y, z);
    if (b == BK_CHEST) {
        auto it = save.chests.find(key);
        if (it != save.chests.end()) { for (auto& kv : it->second) addItem(kv.first, kv.second, false); save.chests.erase(it); }
    }
    if (b == BK_BED && save.hasBed && save.bedX == x && save.bedY == y && save.bedZ == z) save.hasBed = false;
    bombs.erase(std::remove_if(bombs.begin(), bombs.end(), [&](const LitBomb& lb) { return lb.x == x && lb.y == y && lb.z == z; }), bombs.end());
    // water next to the hole flows in (just one step - it's a calm sea)
    uint8_t fill = BK_AIR;
    if (y <= VoxWorld::SEA - 1) {
        static const int N[5][3] = {{0, 1, 0}, {1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}};
        for (auto& n : N) if (vox.get(x + n[0], y + n[1], z + n[2]) == BK_WATER) fill = BK_WATER;
    }
    vox.set(x, y, z, fill);
    voxLightsDirty = true;
    V3 c(x + 0.5f, y + 0.5f, z + 0.5f);
    burst(c, blockColor(b), 14, 3.5f, 0.09f, 0, 0.6f, 12);
    audio::play(SFX_BREAK, 0.7f, 0.9f + 0.3f * erng.f());
    if (drops) {
        if (b == BK_EMBER_ORE) {
            int n = 3 + (int)(erng.f() * 5);
            save.embers += n;
            floats.push_back({c + V3(0, 0.6f, 0), "+" + std::to_string(n) + " embers", V3(1, 0.7f, 0.3f)});
            audio::play(SFX_CATCH, 0.8f, 1.4f);
        } else if (b == BK_LEAVES) {
            addItem(BK_LEAVES, 1);
            if (erng.f() < 0.12f) addItem(BK_SAPLING, 1);
        } else {
            int d = blockDef(b).drop;
            if (d == 0) addItem(b, 1);
            else if (d > 0) addItem(d, 1);
        }
    }
    // things sitting on top fall off with it
    uint8_t up = vox.get(x, y + 1, z);
    if (up == BK_SAPLING || up == BK_FLOWER_RED || up == BK_FLOWER_YELLOW || up == BK_CANDLE) mineBlock(x, y + 1, z, drops);
    if (!has("isle.mined")) setFlag("isle.mined");
    return true;
}

bool Game::useBlock(int x, int y, int z) {
    uint8_t b = vox.get(x, y, z);
    switch (b) {
        case BK_BED: {
            save.hasBed = true; save.bedX = x; save.bedY = y; save.bedZ = z;
            if (!isNight()) { statusMsg = "Spawn point set! You can only sleep at night."; statusT = 2.5f; audio::play(SFX_CHECKPOINT, 0.6f); writeSave(); return true; }
            for (auto& e : enemies) if (e.alive && len(e.pos - pl.pos) < 9) { statusMsg = "You can't sleep - gloomlings are nearby!"; statusT = 2.5f; audio::play(SFX_MENU_BACK); return true; }
            sleepT = 2.4f;
            audio::play(SFX_CHECKPOINT, 0.8f, 0.7f);
            return true;
        }
        case BK_DOOR: case BK_DOOR_TOP: {
            int by = b == BK_DOOR ? y : y - 1;
            uint8_t m = vox.getMeta(x, by, z) ^ 4;
            vox.set(x, by, z, BK_DOOR, m);
            if (vox.get(x, by + 1, z) == BK_DOOR_TOP) vox.set(x, by + 1, z, BK_DOOR_TOP, m);
            audio::play(SFX_GATE, 0.5f, (m & 4) ? 1.6f : 1.3f);
            return true;
        }
        case BK_CHEST:
            chestKey = VoxWorld::idx(x, y, z);
            save.chests[chestKey];
            craftCol = 0; craftSel[0] = craftSel[1] = 0; craftScroll[0] = craftScroll[1] = 0;
            screen = SC_CHEST; audio::play(SFX_MENU_SELECT);
            return true;
        case BK_BENCH: case BK_FURNACE:
            craftCol = 1; craftSel[1] = 0; craftScroll[1] = 0;
            screen = SC_CRAFT; audio::play(SFX_MENU_SELECT);
            return true;
        case BK_BOMB:
            for (auto& lb : bombs) if (lb.x == x && lb.y == y && lb.z == z) return true;
            bombs.push_back({x, y, z, 3.0f});
            audio::play(SFX_CHARGE, 0.8f, 1.4f);
            statusMsg = "The ember bomb is lit - RUN!"; statusT = 2;
            return true;
        case BK_FORGE:
            statusMsg = "The Ember Forge makes 1 ember every 15 s while you're on the island."; statusT = 3;
            return true;
        default: return false;
    }
}

void Game::explodeBomb(int x, int y, int z) {
    if (vox.get(x, y, z) == BK_BOMB) vox.set(x, y, z, BK_AIR);
    V3 c(x + 0.5f, y + 0.5f, z + 0.5f);
    const int R0 = 3;
    for (int dy = -R0; dy <= R0; dy++)
        for (int dz = -R0; dz <= R0; dz++)
            for (int dx = -R0; dx <= R0; dx++) {
                if (dx * dx + dy * dy + dz * dz > 8) continue;
                int bx = x + dx, by = y + dy, bz = z + dz;
                uint8_t b = vox.get(bx, by, bz);
                if (b == BK_BOMB) { bool lit = false; for (auto& lb : bombs) if (lb.x == bx && lb.y == by && lb.z == bz) lit = true; if (!lit) bombs.push_back({bx, by, bz, 0.25f + 0.1f * erng.f()}); continue; }
                if (b == BK_AIR || b == BK_WATER || blockDef(b).hard < 0 || b == BK_CHEST) continue;
                mineBlock(bx, by, bz, erng.f() < 0.45f);
            }
    burst(c, hexColor(0xffa040), 60, 9, 0.16f, 3, 0.9f, 4);
    burst(c, hexColor(0x5a5050), 30, 5, 0.3f, 2, 1.4f, -1);
    audio::play(SFX_SLAM, 1.0f, 0.8f);
    shake = std::max(shake, 0.9f * clampf(1 - len(pl.pos - c) / 20, 0, 1));
    if (len(pl.pos + V3(0, 0.6f, 0) - c) < 4.2f) hurtPlayer(2, c);
    for (auto& e : enemies) if (e.alive && len(e.pos - c) < 4.5f) damageEnemy(e, 10, c, true);
}

void Game::collectForges() { save.forgeTime = save.playTime; }

// ------------------------------------------------------------------ per-frame island logic
void Game::voxPlayerPhysics(float dt, const InputState& in) {
    Player& p = pl;
    int fx = (int)std::floor(p.pos.x), fz = (int)std::floor(p.pos.z);
    uint8_t feet = vox.get(fx, (int)std::floor(p.pos.y + 0.1f), fz), mid = vox.get(fx, (int)std::floor(p.pos.y + 0.6f), fz);
    bool inWater = feet == BK_WATER || mid == BK_WATER;
    if (inWater) {
        p.vel.y += GRAVITY * dt * 0.8f;  // floaty
        p.vel.y = std::max(p.vel.y, -3.5f);
        if (in.down[A_JUMP]) p.vel.y = std::min(p.vel.y + 34 * dt, 4.2f);
        if (in.pressed[A_JUMP] && mid != BK_WATER) p.vel.y = 9.5f;  // hop out at the shore
        float damp = 1 - std::min(1.f, 2.5f * dt);
        p.vel.x *= damp; p.vel.z *= damp;
        p.airJumps = 0;
        static float splashT = 0; splashT -= dt;
        if (splashT <= 0 && lenXZ(p.vel) > 1.5f) { splashT = 0.25f; burst(V3(p.pos.x, VoxWorld::SEA - 0.1f, p.pos.z), hexColor(0x9ad8e8), 4, 2, 0.06f, 0, 0.4f, 8); }
    }
    // ladders: push into one and climb
    bool ladder = false;
    for (float h : {0.1f, 0.9f}) if (vox.get(fx, (int)std::floor(p.pos.y + h), fz) == BK_LADDER) ladder = true;
    if (ladder) {
        if (in.moveY > 0.2f || in.down[A_JUMP]) p.vel.y = 4.2f;
        else if (in.moveY < -0.2f) p.vel.y = -3.5f;
        else p.vel.y = std::max(p.vel.y, -0.6f) * 0.5f;
    }
    // jump pads
    if (p.grounded && vox.get(fx, (int)std::floor(p.pos.y - 0.05f), fz) == BK_JUMPPAD && p.vel.y <= 0.5f) {
        p.vel.y = 21; p.grounded = false;
        audio::play(SFX_JUMP, 1, 0.6f);
        burst(p.pos, hexColor(0x60ff90), 18, 5, 0.08f, 3, 0.5f, 2);
    }
}

void Game::updateVoxel(float dt, const InputState& inRaw, InputState& masked) {
    static const InputState none;
    const InputState& in = (dialog.empty() && !pl.dead) ? inRaw : none;
    useCd = std::max(0.f, useCd - dt);
    hotNameT = std::max(0.f, hotNameT - dt);
    updateDayNight(dt);
    vox.remesh(headless);
    // hotbar selection (also handled per frame in Game::frame so fast monitors never drop a key)
    if (in.numKey) { hotSel = in.numKey - 1; hotNameT = 1.6f; audio::play(SFX_MENU_MOVE, 0.4f); }
    if (in.wheel) { hotSel = ((hotSel + in.wheel) % 9 + 9) % 9; hotNameT = 1.6f; audio::play(SFX_MENU_MOVE, 0.4f); }
    int held = hotSel == 0 ? 0 : save.hot[hotSel];
    // sleeping: fade out, skip the night
    if (sleepT > 0) {
        sleepT -= dt;
        masked = InputState();
        if (sleepT <= 0) {
            save.dayT = 0.0f;
            for (auto& e : enemies) e.alive = false;
            pl.hp = maxHp();
            showToast("GOOD MORNING", "You slept through the night. Spawn point set at your bed.");
            writeSave();
        }
        return;
    }
    // what is the crosshair pointing at?
    V3 fwd = norm(camTarget - camEye);
    V3 head = pl.pos + V3(0, 0.9f, 0);
    float camToHead = len(head - camEye);
    int hx = 0, hy = 0, hz = 0; V3 n;
    float t = vox.raycast(camEye, fwd, camToHead + REACH + 1, &n, &hx, &hy, &hz, false);
    tgtValid = false;
    if (t >= 0 && !pl.dead) {
        V3 hp = camEye + fwd * t;
        if (len(hp - head) <= REACH && t > camToHead - 2.5f) { tgtValid = true; tgtX = hx; tgtY = hy; tgtZ = hz; tgtN = n; }
    }
    // left button: mine (or throw the flame when holding it and aiming at nothing)
    static bool lmbLatched = false, rmbLatched = false;
    if (in.pressed[A_THROW] && (tgtValid || held != 0)) lmbLatched = true;
    if (!in.down[A_THROW]) lmbLatched = false;
    if (lmbLatched) {
        masked.pressed[A_THROW] = masked.down[A_THROW] = masked.released[A_THROW] = false;
        if (tgtValid && !pl.dead) {
            if (!mining || mineX != tgtX || mineY != tgtY || mineZ != tgtZ) { mining = true; mineX = tgtX; mineY = tgtY; mineZ = tgtZ; mineT = 0; }
            uint8_t b = vox.get(tgtX, tgtY, tgtZ);
            float hard = blockDef(b).hard;
            if (hard < 0) { if (statusT <= 0) { statusMsg = "That's too tough to break."; statusT = 1.2f; } }
            else if (useCd <= 0) {
                mineT += dt * (1 + 0.4f * upLevel("heat"));
                static float chip = 0; chip -= dt;
                if (chip <= 0) {
                    chip = 0.14f;
                    V3 c(tgtX + 0.5f, tgtY + 0.5f, tgtZ + 0.5f);
                    burst(c + tgtN * 0.52f, blockColor(b), 3, 1.5f, 0.05f, 0, 0.3f, 10);
                    audio::play(SFX_STEP, 0.6f, 1.4f + 0.3f * erng.f());
                }
                if (mineT >= hard) { mineBlock(tgtX, tgtY, tgtZ, true); mining = false; mineT = 0; useCd = 0.12f; }
            }
        } else mining = false;
    } else mining = false;
    // right button: use a block (bed, door, chest...) or place the held block
    auto act = [&]() -> bool {
        if (!tgtValid) return false;
        if (useBlock(tgtX, tgtY, tgtZ)) return true;
        if (held == 0) return false;
        int px = tgtX + (int)std::round(tgtN.x), py = tgtY + (int)std::round(tgtN.y), pz = tgtZ + (int)std::round(tgtN.z);
        uint8_t tb = vox.get(tgtX, tgtY, tgtZ);
        if (tb == BK_FLOWER_RED || tb == BK_FLOWER_YELLOW || tb == BK_SAPLING) { px = tgtX; py = tgtY; pz = tgtZ; mineBlock(tgtX, tgtY, tgtZ, true); }
        int ladderSide = lookDir(camYaw);
        if (std::fabs(tgtN.y) < 0.5f) {
            V3 w = tgtN * -1.f;
            ladderSide = std::fabs(w.x) > 0.5f ? (w.x > 0 ? 1 : 3) : (w.z > 0 ? 0 : 2);
        }
        if (!placeBlock(held, px, py, pz, lookDir(camYaw), ladderSide)) { audio::play(SFX_MENU_BACK, 0.4f); return true; }
        return true;
    };
    if (in.pressed[A_WARP] && (tgtValid || held != 0)) {
        rmbLatched = true;
        act();
        useCd = 0.3f;
    }
    if (!in.down[A_WARP]) rmbLatched = false;
    if (rmbLatched) {
        masked.pressed[A_WARP] = masked.down[A_WARP] = masked.released[A_WARP] = false;
        if (in.down[A_WARP] && useCd <= 0 && held != 0 && tgtValid && isPlaceable(held)) {
            uint8_t tb = vox.get(tgtX, tgtY, tgtZ);
            if (tb != BK_BED && tb != BK_DOOR && tb != BK_DOOR_TOP && tb != BK_CHEST && tb != BK_BENCH && tb != BK_FURNACE && tb != BK_BOMB) { act(); useCd = 0.22f; }
        }
    }
    // E also uses blocks (when nobody's around to talk to)
    if (in.pressed[A_INTERACT] && tgtValid) {
        bool talker = false;
        for (auto& tk : lv.talkers) if (len(tk.pos - pl.pos) < 3.0f) talker = true;
        if (!talker && useBlock(tgtX, tgtY, tgtZ)) masked.pressed[A_INTERACT] = false;
    }
    // inventory / crafting
    if (in.pressed[A_JOURNAL]) { craftCol = 0; screen = SC_CRAFT; audio::play(SFX_MENU_SELECT); masked.pressed[A_JOURNAL] = false; }

    // lit bombs
    for (size_t i = 0; i < bombs.size(); i++) {
        bombs[i].t -= dt;
        if (bombs[i].t <= 0) { LitBomb lb = bombs[i]; bombs.erase(bombs.begin() + i); explodeBomb(lb.x, lb.y, lb.z); i = (size_t)-1; }
    }
    // light sources, saplings and forges (rescanned when blocks change)
    static std::vector<V3> saplings, forges;
    if (voxLightsDirty) {
        voxLightsDirty = false;
        voxLights.clear(); saplings.clear(); forges.clear();
        for (int y = VoxWorld::Y0; y < VoxWorld::Y0 + VoxWorld::NY; y++)
            for (int z = VoxWorld::Z0; z < VoxWorld::Z0 + VoxWorld::NZ; z++)
                for (int x = VoxWorld::X0; x < VoxWorld::X0 + VoxWorld::NX; x++) {
                    uint8_t b = vox.blk[VoxWorld::idx(x, y, z)];
                    if (b == BK_AIR || b == BK_STONE || b == BK_DIRT || b == BK_WATER) continue;
                    if (blockDef(b).light) voxLights.push_back(V3(x + 0.5f, y + 0.5f, z + 0.5f));
                    if (b == BK_SAPLING) saplings.push_back(V3((float)x, (float)y, (float)z));
                    if (b == BK_FORGE) forges.push_back(V3(x + 0.5f, y + 1.2f, z + 0.5f));
                }
    }
    // saplings grow into trees
    growT -= dt;
    if (growT <= 0) {
        growT = 2;
        for (auto& s : saplings) {
            if (erng.f() > 2.f / 75.f) continue;
            int x = (int)s.x, y = (int)s.y, z = (int)s.z;
            int th = 4 + (int)(erng.f() * 2);
            bool room = true;
            for (int k = 1; k <= th + 1; k++) if (vox.get(x, y + k, z) != BK_AIR) room = false;
            if (!room) continue;
            for (int k = 0; k < th; k++) vox.set(x, y + k, z, BK_LOG);
            int top = y + th - 1;
            for (int dy = -2; dy <= 1; dy++) {
                int r = dy >= 0 ? 1 : 2;
                for (int dz = -r; dz <= r; dz++)
                    for (int dx = -r; dx <= r; dx++) {
                        if (std::abs(dx) == r && std::abs(dz) == r && dy == 1) continue;
                        if (vox.get(x + dx, top + dy, z + dz) == BK_AIR) vox.set(x + dx, top + dy, z + dz, BK_LEAVES);
                    }
            }
            if (vox.get(x, top + 2, z) == BK_AIR) vox.set(x, top + 2, z, BK_LEAVES);
            burst(V3(x + 0.5f, y + 1.5f, z + 0.5f), hexColor(0x80e070), 20, 3, 0.08f, 3, 0.6f, 2);
            voxLightsDirty = true;
            break;
        }
    }
    // ember forges
    if (!forges.empty() && save.playTime - save.forgeTime >= 15) {
        save.forgeTime = save.playTime;
        save.embers += (int)forges.size();
        for (auto& f : forges) if (len(f - pl.pos) < 30) floats.push_back({f, "+1 ember", V3(1, 0.7f, 0.3f)});
    } else if (forges.empty()) save.forgeTime = save.playTime;
    // night: gloomlings creep out of the dark (lamps and the plaza keep them away)
    int alive = 0;
    for (auto& e : enemies) if (e.alive) alive++;
    if (isNight()) {
        mobT -= dt;
        int cap = std::min(8, 4 + save.endlessBest / 10);
        if (mobT <= 0 && alive < cap) {
            mobT = 3.5f + erng.f() * 3;
            for (int tries = 0; tries < 12; tries++) {
                float a = erng.f() * TAU, d = 12 + erng.f() * 12;
                int x = (int)std::floor(pl.pos.x + std::sin(a) * d), z = (int)std::floor(pl.pos.z + std::cos(a) * d);
                if (x < VoxWorld::X0 + 2 || z < VoxWorld::Z0 + 2 || x >= VoxWorld::X0 + VoxWorld::NX - 2 || z >= VoxWorld::Z0 + VoxWorld::NZ - 2) continue;
                int top = -999;
                for (int y = VoxWorld::Y0 + VoxWorld::NY - 3; y > VoxWorld::Y0; y--) {
                    uint8_t b = vox.get(x, y, z);
                    if (b == BK_AIR) continue;
                    if (b != BK_WATER && blockDef(b).solid && blockDef(b).shape == SH_CUBE && vox.get(x, y + 1, z) == BK_AIR && vox.get(x, y + 2, z) == BK_AIR) top = y + 1;
                    break;
                }
                if (top == -999 || top <= VoxWorld::SEA) continue;
                V3 p(x + 0.5f, (float)top, z + 0.5f);
                if (distXZ(p, PLAZA_C) < 10) continue;
                bool lit = false;
                for (auto& l : voxLights) if (len(l - p) < 7) lit = true;
                if (lit) continue;
                EnemyType et = erng.f() < 0.7f ? E_GLOOMLING : (erng.f() < 0.5f ? E_RAT : E_MOTH);
                spawnEnemy(et, p + V3(0, 0.2f, 0), -1);
                burst(p + V3(0, 0.6f, 0), hexColor(0x6040a0), 12, 2, 0.1f, 2, 0.8f, -1);
                break;
            }
        }
        if (!has("isle.night")) { setFlag("isle.night"); showToast("NIGHT FALLS", "Gloomlings creep out of the dark. Light lamps, build walls - or sleep in a bed."); }
    } else {
        // sunrise burns them away
        for (auto& e : enemies)
            if (e.alive && erng.f() < dt * 0.6f) { e.alive = false; burst(e.pos + V3(0, 0.6f, 0), hexColor(0xffd890), 16, 3, 0.1f, 3, 0.6f, 1); }
    }
    // autosave every minute
    static float autosave = 0;
    autosave += dt;
    if (autosave > 60) { autosave = 0; writeSave(); }
}

// ------------------------------------------------------------------ crafting / inventory screen
static bool stationNear(Game& g, int st) {
    if (st == 0) return true;
    uint8_t want = st == 1 ? BK_BENCH : BK_FURNACE;
    int px = (int)std::floor(g.pl.pos.x), py = (int)std::floor(g.pl.pos.y), pz = (int)std::floor(g.pl.pos.z);
    for (int y = py - 3; y <= py + 3; y++)
        for (int z = pz - 4; z <= pz + 4; z++)
            for (int x = px - 4; x <= px + 4; x++) if (g.vox.get(x, y, z) == want) return true;
    return false;
}

bool Game::craft(int r) {
    const Recipe& rc = RECIPES[r];
    if (!stationNear(*this, rc.station)) { statusMsg = std::string("You need to be near a ") + STATION_NAME[rc.station]; statusT = 2; return false; }
    for (auto& in : rc.in) if (in[1] > 0 && itemCount(in[0]) < in[1]) { statusMsg = "Missing ingredients"; statusT = 1.5f; return false; }
    for (auto& in : rc.in) if (in[1] > 0) takeItem(in[0], in[1]);
    addItem(rc.out, rc.n, false);
    if (!has("isle.crafted")) setFlag("isle.crafted");
    return true;
}

static std::vector<std::pair<int, int>> invList(const Game& g) {
    std::vector<std::pair<int, int>> v;
    for (auto& kv : g.save.inv) if (kv.second > 0) v.push_back(kv);
    return v;
}

static void listNav(int& sel, int n, const InputState& in) {
    if (n <= 0) { sel = 0; return; }
    if (in.mUp) { sel = (sel + n - 1) % n; audio::play(SFX_MENU_MOVE); }
    if (in.mDown) { sel = (sel + 1) % n; audio::play(SFX_MENU_MOVE); }
    sel = clampi(sel, 0, n - 1);
}

void Game::updateCraft(const InputState& in) {
    auto inv = invList(*this);
    int nInv = (int)inv.size();
    if (in.mLeft) { craftCol = 0; audio::play(SFX_MENU_MOVE); }
    if (in.mRight) { craftCol = 1; audio::play(SFX_MENU_MOVE); }
    bool clicked = false;
    for (auto& r : menuRects)
        if (mouseX >= r.x && mouseX <= r.x + r.w && mouseY >= r.y && mouseY <= r.y + r.h) {
            if (mouseMoved || mouseClick) { craftCol = r.idx / 1000; craftSel[craftCol] = r.idx % 1000; }
            if (mouseClick) clicked = true;
        }
    listNav(craftSel[craftCol], craftCol == 0 ? nInv : NREC, in);
    if (in.mBack || in.pressed[A_PAUSE] || in.pressed[A_JOURNAL] || in.pressed[A_INTERACT]) { screen = SC_PLAY; audio::play(SFX_MENU_BACK); return; }
    if (craftCol == 0 && nInv > 0) {
        int item = inv[craftSel[0]].first;
        if (in.numKey >= 2) {
            for (int i = 1; i < 9; i++) if (save.hot[i] == item) save.hot[i] = 0;
            save.hot[in.numKey - 1] = item;
            audio::play(SFX_TICK, 1, 1.3f);
        }
        if (in.mOk || clicked) {
            // put it in the hand
            int slot = -1;
            for (int i = 1; i < 9; i++) if (save.hot[i] == item) slot = i;
            if (slot < 0) { slot = hotSel >= 1 ? hotSel : 1; save.hot[slot] = item; }
            hotSel = slot; hotNameT = 1.6f;
            screen = SC_PLAY; audio::play(SFX_MENU_SELECT);
        }
    } else if (craftCol == 1) {
        int times = (in.mOk || clicked) ? 1 : (in.pressed[A_BLOOM] ? 5 : 0);
        int made = 0;
        for (int k = 0; k < times; k++) if (craft(craftSel[1])) made++; else break;
        if (made) audio::play(SFX_TICK, 1, 1.1f);
        else if (times) audio::play(SFX_MENU_BACK);
    }
}

static void drawList(Game& g, Renderer* R, float x, float y, float w, float h, int col, int count, int& scroll,
                     const std::function<void(int, float, float, bool)>& row, float rowH) {
    int vis = std::max(1, (int)(h / rowH));
    int sel = g.craftSel[col];
    if (sel < scroll) scroll = sel;
    if (sel >= scroll + vis) scroll = sel - vis + 1;
    scroll = clampi(scroll, 0, std::max(0, count - vis));
    for (int i = scroll; i < std::min(count, scroll + vis); i++) {
        float ry = y + (i - scroll) * rowH;
        bool s = i == sel && g.craftCol == col;
        if (s) R->uiRect(x, ry, w, rowH - 1, C4(0x2a3a5a, 0.9f));
        g.menuRects.push_back({x, ry, w, rowH, col * 1000 + i});
        row(i, x, ry, s);
    }
    if (scroll > 0) R->uiTextCentered(x + w / 2, y - 8 * (float)R->uiScale(), "^", (float)R->uiScale(), C4(0x8a8098));
    if (scroll + vis < count) R->uiTextCentered(x + w / 2, y + vis * rowH, "v", (float)R->uiScale(), C4(0x8a8098));
}

void Game::drawCraft() {
    float s = (float)R->uiScale();
    float W = (float)R->winW, H = (float)R->winH;
    float pw = std::min(W - 16 * s, 520 * s), px = W / 2 - pw / 2, py = 10 * s, ph = H - 54 * s;
    R->uiFrame(px, py, pw, ph, C4(0x0e1018, 0.95f), C4(0x6a8ac0));
    menuRects.clear();
    float colW = pw / 2 - 12 * s, rowH = 13 * s;
    float lx = px + 8 * s, rx = px + pw / 2 + 4 * s, ty = py + 6 * s;
    R->uiText(lx, ty, "INVENTORY", s * 1.3f, craftCol == 0 ? C4(0xffc070) : C4(0x8a8098));
    R->uiText(rx, ty, "CRAFTING", s * 1.3f, craftCol == 1 ? C4(0xffc070) : C4(0x8a8098));
    bool bench = stationNear(*this, 1), furnace = stationNear(*this, 2);
    std::string st = std::string(bench ? "Workbench: yes" : "Workbench: no") + "   " + (furnace ? "Furnace: yes" : "Furnace: no");
    R->uiText(rx, ty + 13 * s, st, s, C4(0x8a8098));
    R->uiText(lx, ty + 13 * s, "Embers " + std::to_string(save.embers), s, C4(0xffc070));
    float ly = ty + 28 * s, lh = ph - 50 * s;
    auto inv = invList(*this);
    if (inv.empty()) R->uiText(lx, ly, "Nothing yet - go mine something!", s, C4(0x8a8098));
    drawList(*this, R, lx, ly, colW, lh, 0, (int)inv.size(), craftScroll[0], [&](int i, float x, float y, bool sel) {
        int item = inv[i].first;
        R->uiIcon(x + 2 * s, y + 1 * s, 11 * s, itemIconTile(item), V4(itemIconTint(item).x, itemIconTint(item).y, itemIconTint(item).z, 1));
        R->uiText(x + 16 * s, y + 3 * s, itemName(item), s, sel ? C4(0xffffff) : C4(0xe0d8c8));
        std::string c = "x" + std::to_string(inv[i].second);
        int slot = -1;
        for (int k = 1; k < 9; k++) if (save.hot[k] == item) slot = k;
        if (slot > 0) c = "[" + std::to_string(slot + 1) + "] " + c;
        R->uiText(x + colW - R->uiTextWidth(c, s) - 4 * s, y + 3 * s, c, s, C4(0xffc070));
    }, rowH);
    drawList(*this, R, rx, ly, colW, lh, 1, NREC, craftScroll[1], [&](int i, float x, float y, bool sel) {
        const Recipe& rc = RECIPES[i];
        bool stOk = rc.station == 0 || (rc.station == 1 ? bench : furnace);
        bool have = true;
        for (auto& in : rc.in) if (in[1] > 0 && itemCount(in[0]) < in[1]) have = false;
        bool ok = stOk && have;
        V3 t = itemIconTint(rc.out);
        R->uiIcon(x + 2 * s, y + 1 * s, 11 * s, itemIconTile(rc.out), V4(t.x, t.y, t.z, ok ? 1.f : 0.45f));
        std::string name = std::string(itemName(rc.out)) + (rc.n > 1 ? " x" + std::to_string(rc.n) : "");
        R->uiText(x + 16 * s, y + 3 * s, name, s, ok ? (sel ? C4(0xffffff) : C4(0xe0d8c8)) : C4(0x6a6474));
        std::string need;
        for (auto& in : rc.in) if (in[1] > 0) { if (!need.empty()) need += "+"; need += std::to_string(in[1]) + " " + itemName(in[0]); }
        if (!stOk) need = STATION_NAME[rc.station];
        float nw = R->uiTextWidth(need, s);
        R->uiText(x + colW - nw - 4 * s, y + 3 * s, need, s, ok ? C4(0x90e090) : (stOk ? C4(0xc07060) : C4(0x8a8098)));
    }, rowH);
    std::string help = craftCol == 0 ? "Enter/click: hold it   [2]-[9]: put it on that hotbar slot   Left/Right: switch side"
                                     : "Enter/click: craft 1   [" + bindingName(settings.keys[A_BLOOM]) + "]: craft 5   Left/Right: switch side";
    R->uiTextCentered(W / 2, py + ph + 4 * s, help, s, C4(0xb0a8c0));
    R->uiTextCentered(W / 2, py + ph + 14 * s, "[" + bindingName(settings.keys[A_JOURNAL]) + "] / Esc: close", s, C4(0x8a8098));
}

// ------------------------------------------------------------------ chests
void Game::updateChest(const InputState& in) {
    auto& box = save.chests[chestKey];
    auto inv = invList(*this);
    std::vector<std::pair<int, int>> cv(box.begin(), box.end());
    if (in.mLeft) { craftCol = 0; audio::play(SFX_MENU_MOVE); }
    if (in.mRight) { craftCol = 1; audio::play(SFX_MENU_MOVE); }
    bool clicked = false;
    for (auto& r : menuRects)
        if (mouseX >= r.x && mouseX <= r.x + r.w && mouseY >= r.y && mouseY <= r.y + r.h) {
            if (mouseMoved || mouseClick) { craftCol = r.idx / 1000; craftSel[craftCol] = r.idx % 1000; }
            if (mouseClick) clicked = true;
        }
    listNav(craftSel[craftCol], craftCol == 0 ? (int)inv.size() : (int)cv.size(), in);
    if (in.mBack || in.pressed[A_PAUSE] || in.pressed[A_JOURNAL] || in.pressed[A_INTERACT]) { screen = SC_PLAY; audio::play(SFX_MENU_BACK); writeSave(); return; }
    bool all = in.mOk || clicked, one = in.pressed[A_BLOOM];
    if (!all && !one) return;
    if (craftCol == 0 && !inv.empty()) {
        int item = inv[craftSel[0]].first, n = all ? inv[craftSel[0]].second : 1;
        if (takeItem(item, n)) { box[item] += n; audio::play(SFX_TICK); }
    } else if (craftCol == 1 && !cv.empty()) {
        int item = cv[craftSel[1]].first, n = all ? cv[craftSel[1]].second : 1;
        box[item] -= n;
        if (box[item] <= 0) box.erase(item);
        addItem(item, n, false);
        audio::play(SFX_TICK, 1, 1.2f);
    }
}

void Game::drawChest() {
    float s = (float)R->uiScale();
    float W = (float)R->winW, H = (float)R->winH;
    float pw = std::min(W - 16 * s, 440 * s), px = W / 2 - pw / 2, py = 16 * s, ph = H - 64 * s;
    R->uiFrame(px, py, pw, ph, C4(0x18120a, 0.95f), C4(0xc08040));
    menuRects.clear();
    float colW = pw / 2 - 12 * s, rowH = 13 * s;
    float lx = px + 8 * s, rx = px + pw / 2 + 4 * s, ty = py + 6 * s;
    R->uiText(lx, ty, "YOU", s * 1.3f, craftCol == 0 ? C4(0xffc070) : C4(0x8a8098));
    R->uiText(rx, ty, "CHEST", s * 1.3f, craftCol == 1 ? C4(0xffc070) : C4(0x8a8098));
    auto inv = invList(*this);
    auto& box = save.chests[chestKey];
    std::vector<std::pair<int, int>> cv(box.begin(), box.end());
    auto rowFn = [&](std::vector<std::pair<int, int>>& v) {
        return [&, colW](int i, float x, float y, bool sel) {
            int item = v[i].first;
            V3 t = itemIconTint(item);
            R->uiIcon(x + 2 * s, y + 1 * s, 11 * s, itemIconTile(item), V4(t.x, t.y, t.z, 1));
            R->uiText(x + 16 * s, y + 3 * s, itemName(item), s, sel ? C4(0xffffff) : C4(0xe0d8c8));
            std::string c = "x" + std::to_string(v[i].second);
            R->uiText(x + colW - R->uiTextWidth(c, s) - 4 * s, y + 3 * s, c, s, C4(0xffc070));
        };
    };
    float ly = ty + 18 * s, lh = ph - 40 * s;
    drawList(*this, R, lx, ly, colW, lh, 0, (int)inv.size(), craftScroll[0], rowFn(inv), rowH);
    drawList(*this, R, rx, ly, colW, lh, 1, (int)cv.size(), craftScroll[1], rowFn(cv), rowH);
    if (cv.empty()) R->uiText(rx, ly, "(empty)", s, C4(0x8a8098));
    R->uiTextCentered(W / 2, py + ph + 4 * s, "Enter/click: move the stack   [" + bindingName(settings.keys[A_BLOOM]) + "]: move one   Left/Right: switch side   Esc: close", s, C4(0xb0a8c0));
}

// ------------------------------------------------------------------ Bramble's shop
void Game::updateSupply(const InputState& in) {
    int n = NOFFER + 1;
    if (in.mUp) { shopSel = (shopSel + n - 1) % n; audio::play(SFX_MENU_MOVE); }
    if (in.mDown) { shopSel = (shopSel + 1) % n; audio::play(SFX_MENU_MOVE); }
    if (mouseMoved) for (auto& r : menuRects) if (mouseX >= r.x && mouseX <= r.x + r.w && mouseY >= r.y && mouseY <= r.y + r.h) shopSel = r.idx;
    bool clicked = false;
    if (mouseClick) for (auto& r : menuRects) if (mouseX >= r.x && mouseX <= r.x + r.w && mouseY >= r.y && mouseY <= r.y + r.h) { shopSel = r.idx; clicked = true; }
    if (in.mBack || in.pressed[A_PAUSE] || in.pressed[A_INTERACT]) { screen = SC_PLAY; audio::play(SFX_MENU_BACK); return; }
    if (!(in.mOk || clicked)) return;
    if (shopSel == NOFFER) { screen = SC_PLAY; audio::play(SFX_MENU_BACK); return; }
    const Offer& o = OFFERS[shopSel];
    if (save.embers < o.price) { statusMsg = "Not enough embers - the Endless Dark pays well!"; statusT = 1.5f; audio::play(SFX_MENU_BACK); return; }
    save.embers -= o.price;
    addItem(o.item, o.n, false);
    audio::play(SFX_TICK, 1, 1.2f);
    writeSave();
}

void Game::drawSupply() {
    float s = (float)R->uiScale();
    float W = (float)R->winW, H = (float)R->winH;
    float rowH = 12 * s;
    float pw = std::min(W - 20 * s, 320 * s), px = W / 2 - pw / 2;
    float ph = 34 * s + (NOFFER + 1) * rowH + 4 * s;
    float py = std::max(6 * s, (H - ph) / 2 - 10 * s);
    R->uiFrame(px, py, pw, ph, C4(0x1a120a, 0.94f), C4(0xc08040));
    R->uiTextCentered(W / 2, py + 5 * s, "BRAMBLE'S BUILDING SUPPLIES", s * 1.3f, C4(0xffc070));
    R->uiTextCentered(W / 2, py + 18 * s, "Embers: " + std::to_string(save.embers), s, C4(0xffc070));
    menuRects.clear();
    float y = py + 32 * s;
    for (int i = 0; i <= NOFFER; i++) {
        bool sel = i == shopSel;
        if (sel) R->uiRect(px + 3 * s, y - 1 * s, pw - 6 * s, rowH, C4(0x40280e, 0.85f));
        menuRects.push_back({px, y - 1 * s, pw, rowH, i});
        if (i == NOFFER) { R->uiText(px + 10 * s, y + 2 * s, "Leave", s, sel ? C4(0xffc070) : C4(0xf0e6d0)); break; }
        const Offer& o = OFFERS[i];
        V3 t = itemIconTint(o.item);
        R->uiIcon(px + 8 * s, y, 10 * s, itemIconTile(o.item), V4(t.x, t.y, t.z, 1));
        std::string label = std::to_string(o.n) + " " + itemName(o.item) + "   (have " + std::to_string(itemCount(o.item)) + ")";
        R->uiText(px + 22 * s, y + 2 * s, label, s, sel ? C4(0xffc070) : C4(0xf0e6d0));
        std::string price = std::to_string(o.price);
        R->uiText(px + pw - R->uiTextWidth(price, s) - 10 * s, y + 2 * s, price, s, save.embers >= o.price ? C4(0xffc070) : C4(0x706878));
        y += rowH;
    }
    R->uiTextCentered(W / 2, py + ph + 4 * s, "Or mine it yourself! Endless Dark levels also pay out logs, stone, crystals and gears.", s, C4(0x8a8098));
}

// ------------------------------------------------------------------ HUD: hotbar
void Game::drawHotbar() {
    float s = (float)R->uiScale();
    float W = (float)R->winW, H = (float)R->winH;
    float slot = 20 * s, gap = 2 * s, total = 9 * slot + 8 * gap;
    float x0 = W / 2 - total / 2, y0 = H - slot - 6 * s;
    for (int i = 0; i < 9; i++) {
        float x = x0 + i * (slot + gap);
        bool sel = i == hotSel;
        R->uiFrame(x, y0, slot, slot, C4(0x0c0a14, 0.75f), sel ? C4(0xffe090) : C4(0x5a5068));
        if (sel) R->uiFrame(x - s, y0 - s, slot + 2 * s, slot + 2 * s, C4(0, 0), C4(0xffe090));
        int item = i == 0 ? 0 : save.hot[i];
        if (i == 0 || item) {
            V3 t = itemIconTint(item);
            R->uiIcon(x + 2 * s, y0 + 2 * s, 16 * s, itemIconTile(item), V4(t.x, t.y, t.z, 1));
            if (i > 0) {
                std::string c = std::to_string(itemCount(item));
                R->uiText(x + slot - R->uiTextWidth(c, s) - 1 * s, y0 + slot - 8 * s, c, s, C4(0xffffff));
            }
        }
        R->uiText(x + 2 * s, y0 + 1 * s, std::to_string(i + 1), s * 0.75f < 1 ? 1 : s * 0.75f, C4(0x8a8098, 0.8f), false);
    }
    int held = hotSel == 0 ? 0 : save.hot[hotSel];
    if (hotNameT > 0) {
        std::string n = hotSel == 0 ? "Flame (throw, warp, bloom - and fight)" : (held ? itemName(held) : "(empty slot - press [" + bindingName(settings.keys[A_JOURNAL]) + "] to fill it)");
        R->uiTextCentered(W / 2, y0 - 12 * s, n, s, C4(0xf0e6d0, std::min(1.f, hotNameT * 2)));
    }
    // block under the crosshair
    if (tgtValid && screen == SC_PLAY) {
        uint8_t b = vox.get(tgtX, tgtY, tgtZ);
        std::string tn = blockDef(b).name;
        if (b == BK_BED || b == BK_DOOR || b == BK_DOOR_TOP || b == BK_CHEST || b == BK_BENCH || b == BK_FURNACE || b == BK_BOMB)
            tn += "   [" + bindingName(settings.keys[A_WARP]) + "] use";
        R->uiTextCentered(W / 2, H / 2 + 12 * s, tn, s, C4(0xe0d8c8, 0.85f));
        if (mining && blockDef(b).hard > 0) {
            float k = clampf(mineT / blockDef(b).hard, 0, 1);
            R->uiRect(W / 2 - 15 * s, H / 2 + 22 * s, 30 * s, 2 * s, C4(0x000000, 0.6f));
            R->uiRect(W / 2 - 15 * s, H / 2 + 22 * s, 30 * s * k, 2 * s, C4(0xffc070));
        }
    }
    // time of day
    float t = save.dayT;
    bool night = isNight();
    float left = night ? (t < 0.96f ? (0.96f - t) : 0) : (t < 0.6f ? 0.6f - t : 1.6f - t);
    int secs = (int)(left * DAY_LEN);
    char buf[96];
    snprintf(buf, sizeof(buf), "%s  -  %s in %d:%02d", night ? "NIGHT" : "DAY", night ? "sunrise" : "nightfall", secs / 60, secs % 60);
    R->uiText(8 * s, 34 * s, buf, s, night ? C4(0x9ab0ff) : C4(0xffe090));
    if (sleepT > 0) {
        float a = clampf(1 - sleepT / 2.4f, 0, 1) * 1.6f;
        R->uiRect(0, 0, W, H, C4(0x000000, std::min(1.f, a)));
        R->uiTextCentered(W / 2, H / 2, "Zzz...", s * 2, C4(0xf0e6d0, std::min(1.f, a)));
    }
}

// ------------------------------------------------------------------ 3D extras: crosshair block, cracks, held block, lamps
void drawHomeExtras(Game& g, Renderer* R) {
    if (g.lv.id != LV_HOME || !g.vox.active) return;
    // block meshes (drawn in the shadow and colour passes)
    for (int i = 0; i < VoxWorld::NCX * VoxWorld::NCZ; i++) R->drawMesh(g.vox.solid[i], M4());
    if (R->shadowPass) return;
    if (g.tgtValid && g.screen == SC_PLAY) {
        AABB bx[2];
        int n = g.vox.boxes(g.tgtX, g.tgtY, g.tgtZ, bx, true);
        for (int k = 0; k < n; k++) {
            V3 lo = bx[k].lo - V3(0.01f, 0.01f, 0.01f), hi = bx[k].hi + V3(0.01f, 0.01f, 0.01f);
            V3 sz = hi - lo, c = (lo + hi) * 0.5f;
            const float w = 0.035f;
            V3 col = hexColor(0x101018);
            for (int a = 0; a < 3; a++)
                for (int i = 0; i < 4; i++) {
                    V3 p = c, sc(w, w, w);
                    sc[a] = sz[a] + w;
                    int b1 = (a + 1) % 3, b2 = (a + 2) % 3;
                    p[b1] = (i & 1) ? hi[b1] : lo[b1];
                    p[b2] = (i & 2) ? hi[b2] : lo[b2];
                    R->drawPart(PM_CUBE, M4::translate(p) * M4::scale(sc), T_WHITE, col);
                }
            if (g.mining && blockDef(g.vox.get(g.tgtX, g.tgtY, g.tgtZ)).hard > 0) {
                float k2 = clampf(g.mineT / blockDef(g.vox.get(g.tgtX, g.tgtY, g.tgtZ)).hard, 0, 1);
                R->drawPart(PM_CUBE, M4::translate(c) * M4::scale(sz + V3(0.02f, 0.02f, 0.02f)), T_CRACK, V3(1, 1, 1), 0, 0, 0.25f + 0.75f * k2);
            }
        }
    }
    // the block Wick is holding
    int held = g.hotSel == 0 ? 0 : g.save.hot[g.hotSel];
    if (held && !g.pl.dead) {
        float yaw = g.pl.yaw;
        V3 side(std::cos(yaw), 0, -std::sin(yaw)), fwd(std::sin(yaw), 0, std::cos(yaw));
        V3 p = g.pl.pos + V3(0, 0.55f + 0.03f * std::sin(g.time * 4), 0) + side * -0.42f + fwd * 0.2f;
        R->drawPart(PM_CUBE, M4::translate(p) * M4::rotY(yaw + 0.5f) * M4::scale(V3(0.3f, 0.3f, 0.3f)), itemIconTile(held), itemIconTint(held));
    }
    // lit bombs flash faster and faster
    for (auto& b : g.bombs) {
        float f = std::sin(g.time * (12 + (3 - b.t) * 10)) > 0 ? 1.f : 0.f;
        R->drawPart(PM_CUBE, M4::translate(V3(b.x + 0.5f, b.y + 0.5f, b.z + 0.5f)) * M4::scale(V3(1.04f, 1.04f, 1.04f)), T_BOMB, V3(1, 1, 1), f * 0.8f, f * 0.5f);
    }
}

// lights and water are added by Game::draw
void homeLights(Game& g, Renderer* R) {
    if (g.lv.id != LV_HOME) return;
    std::vector<std::pair<float, V3>> near;
    for (auto& l : g.voxLights) { float d = len(l - g.pl.pos); if (d < 30) near.push_back({d, l}); }
    std::sort(near.begin(), near.end(), [](const std::pair<float, V3>& a, const std::pair<float, V3>& b) { return a.first < b.first; });
    for (size_t i = 0; i < near.size() && i < 10; i++) {
        V3 p = near[i].second;
        uint8_t b = g.vox.get((int)std::floor(p.x), (int)std::floor(p.y), (int)std::floor(p.z));
        R->lights.push_back({p, b == BK_CANDLE ? 6.f : 9.f, hexColor(blockDef(b).light), b == BK_CANDLE ? 1.0f : 1.4f});
    }
}
void homeWater(Game& g, Renderer* R) {
    if (g.lv.id != LV_HOME || !g.vox.active) return;
    for (int i = 0; i < VoxWorld::NCX * VoxWorld::NCZ; i++) R->drawWater(g.vox.water[i], hexColor(0x3a7aa0), 1);
}
