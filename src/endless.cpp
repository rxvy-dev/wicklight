// WICKLIGHT - The Endless Dark: infinite procedurally generated levels.
// Every level is built from a seed (its depth), so a level is the same each time you retry it.
// The generator also records a route (used by the autoplay test to prove every level is beatable).
#include "builder.h"
#include <cstdio>

struct Theme { uint32_t skyTop, skyHor, fog; V3 amb; int floorTop, floorSide, wallTile; uint32_t lightCol; Song song; const char* name; };
static const Theme THEMES[5] = {
    {0x04101a, 0x1a4048, 0x123036, V3(0.1f, 0.2f, 0.22f), T_PLANK, T_DARKWOOD, T_STONEBRICK, 0x60e0e0, SONG_WHARF, "Drowned"},
    {0x081410, 0x2a4430, 0x18281e, V3(0.14f, 0.2f, 0.14f), T_GRASS, T_DIRT, T_BRASS, 0xc0ff80, SONG_CLOCK, "Ticking"},
    {0x080406, 0x2a0a14, 0x180810, V3(0.14f, 0.07f, 0.09f), T_TILEFLOOR, T_STONEBRICK, T_STONEBRICK, 0xff6040, SONG_VAULTS, "Ashen"},
    {0x100620, 0x5a2050, 0x241230, V3(0.2f, 0.12f, 0.26f), T_DIRT, T_RUST, T_RUST, 0xff50c0, SONG_JUNK, "Rusted"},
    {0x05031a, 0x2a1a4a, 0x150f2a, V3(0.15f, 0.13f, 0.28f), T_MARBLE, T_STONEBRICK, T_MARBLE, 0xa080ff, SONG_SPIRE, "Starlit"},
};

void buildEndless(Level& L, Game& g) {
    LB b(L);
    int depth = std::max(1, g.save.endlessDepth);
    Rng r(hash32((uint32_t)depth * 7919u + 13u));
    // now and then (rarely) a depth opens onto Lumen City in the middle of an FBI raid
    bool city = depth >= 4 && depth % 5 != 0 && hash32((uint32_t)depth * 2654435761u + 99u) % 7 == 0;
    static const Theme CITY = {0x050916, 0x1f2a4e, 0x111a30, V3(0.16f, 0.18f, 0.28f), T_ASPHALT, T_BRICK, T_BRICK, 0xffd0a0, SONG_CITY, "Lumen City"};
    const Theme& th = city ? CITY : THEMES[(depth - 1) % 5];
    char nm[64]; snprintf(nm, sizeof(nm), city ? "The Endless Dark - Depth %d: FBI RAID" : "The Endless Dark - Depth %d", depth);
    L.name = nm;
    L.song = th.song;
    L.env.skyTop = hexColor(th.skyTop); L.env.skyHorizon = hexColor(th.skyHor);
    L.env.fogColor = hexColor(th.fog); L.env.fogDensity = 0.024f;
    L.env.ambSky = th.amb; L.env.ambGround = th.amb * 0.45f;
    L.env.sunColor = th.amb * 1.4f;
    L.killY = -14;
    g.enemyHpMul = std::min(1.f + 0.06f * (depth - 1), 3.f);  // tougher with depth, but capped so it stays fair
    bool warp = g.ability("warp"), bloom = g.ability("bloom");
    V3 lightCol = hexColor(th.lightCol);
    auto R = [&](int op, V3 p) { L.route.push_back({op, p}); };

    // enemy pool grows with depth
    auto pickEnemy = [&]() -> EnemyType {
        std::vector<EnemyType> pool = {E_GLOOMLING, E_GLOOMLING, E_RAT, E_MOTH};
        if (depth >= 2) pool.push_back(E_TARBELL);
        if (depth >= 3) pool.push_back(E_RAT);
        if (depth >= 4) pool.push_back(E_KNIGHT);
        if (depth >= 6) pool.push_back(E_WRAITH);
        if (depth >= 8) pool.push_back(E_KNIGHT);
        if (city) pool = {E_AGENT, E_AGENT, E_DRONE, E_VIRUS, E_RAT};
        return pool[r.irange(0, (int)pool.size() - 1)];
    };

    // --- start platform
    b.slab(-6, -7, 6, 8, 0, th.floorTop, th.floorSide, 3);
    b.spawn(V3(0, 0, -1), 0);
    b.exit(AABB(V3(-3, -1, -7.5f), V3(3, 3, -5.5f)), LV_HUB, 6, "", "", "Retreat to Lumen Hollow");
    b.lantern(V3(4, 0, 4), 0);
    char hint[96]; snprintf(hint, sizeof(hint), city ? "DEPTH %d: an FBI RAID in Lumen City! Dodge the searchlights. Best so far: %d" : "DEPTH %d.  Reach the portal at the end. Best so far: %d", depth, g.save.endlessBest);
    b.hint(AABB(V3(-6, -1, -5), V3(6, 4, 4)), hint);
    float z = 8, y = 0;
    R(0, V3(0, 0, 6));

    int segs = std::min(3 + depth / 2, 10);
    bool bossLevel = depth % 5 == 0;
    if (bossLevel) segs = std::min(segs, 4);
    int arenaCount = 0;
    for (int s = 0; s < segs; s++) {
        // choose a segment type the player can actually do
        std::vector<int> types = {0, 0, 1, 3, 4};
        if (warp) { types.push_back(2); types.push_back(5); }
        int t = types[r.irange(0, (int)types.size() - 1)];
        if (s == 0 && t == 2) t = 1;
        switch (t) {
            case 0: {  // combat room (an ambush arena)
                float len = 18;
                b.slab(-9, z, 9, z + len, y, th.floorTop, th.floorSide, 3);
                b.wall(-10, z, -9, z + len, y, y + 3.5f, th.wallTile); b.wall(9, z, 10, z + len, y, y + 3.5f, th.wallTile);
                b.wall(-9, z, -2.5f, z + 0.6f, y, y + 3.5f, th.wallTile); b.wall(2.5f, z, 9, z + 0.6f, y, y + 3.5f, th.wallTile);
                b.wall(-9, z + len - 0.6f, -2.5f, z + len, y, y + 3.5f, th.wallTile); b.wall(2.5f, z + len - 0.6f, 9, z + len, y, y + 3.5f, th.wallTile);
                int ga = b.gate(AABB(V3(-2.5f, y, z), V3(2.5f, y + 4, z + 0.6f)), -1, "", T_GRATE);
                int gb = b.gate(AABB(V3(-2.5f, y, z + len - 0.6f), V3(2.5f, y + 4, z + len)), -1, "", T_GRATE);
                int ar = b.arena(AABB(V3(-8.5f, y - 1, z + 2), V3(8.5f, y + 6, z + len - 2)), {ga, gb}, "");
                int k = std::min(2 + depth / 2, 8);
                for (int i = 0; i < k; i++) {
                    EnemyType et = pickEnemy();
                    V3 p(r.range(-6, 6), y + 0.2f, z + r.range(8, len - 3));
                    b.enemy(et, p, ar);
                }
                if (depth >= 5 && r.f() < 0.25f + depth * 0.02f) b.enemy(r.f() < 0.5f ? E_GLOOMKING : E_MEGABOT, V3(0, y + 0.2f, z + len - 5), ar);
                b.urns({V3(-7.5f, y, z + 3), V3(7.5f, y, z + len - 3)}, (depth % 2));
                b.light(V3(0, y + 4, z + len / 2), 12, lightCol, 0.7f);
                R(0, V3(0, y, z + 1.2f)); R(0, V3(0, y, z + 5)); R(3, V3(0, y, z + len / 2)); R(0, V3(0, y, z + len - 1.2f));
                z += len; arenaCount++;
            } break;
            case 1: {  // stepping stones
                int n = r.irange(3, 5);
                float x = 0;
                for (int i = 0; i < n; i++) {
                    float gap = r.range(1.0f, 1.8f);
                    z += gap;
                    x = clampf(x + r.range(-1.2f, 1.2f), -4, 4);
                    float ny = clampf(y + r.range(-0.6f, 0.6f), -2, 6);
                    b.slab(x - 1.7f, z, x + 1.7f, z + 4.2f, ny, th.floorTop, th.floorSide, 1.5f);
                    if (r.f() < 0.3f) b.urns({V3(x + 1.0f, ny, z + 2.1f)}, 1);
                    R(0, V3(x, ny, z + 2.1f));
                    z += 4.2f; y = ny;
                }
                z += 1.6f;
                b.slab(-4, z, 4, z + 5, y, th.floorTop, th.floorSide, 2);
                if (depth >= 3 && r.f() < 0.5f) b.enemy(E_TARBELL, V3(r.f() < 0.5f ? -3.2f : 3.2f, y, z + 4));
                R(0, V3(0, y, z + 2.5f)); R(3, V3(0, y, z + 2.5f));
                z += 5;
            } break;
            case 2: {  // warp gap
                float gap = r.range(9, 12);
                b.slab(-4, z, 4, z + 3, y, th.floorTop, th.floorSide, 2);
                R(0, V3(0, y, z + 2.2f));
                z += 3 + gap;
                float ny = clampf(y + r.range(-1, 3), -2, 8);
                b.slab(-4, z, 4, z + 6, ny, th.floorTop, th.floorSide, 2);
                V3 an(0, ny + 2.2f, z + 1.6f);
                b.anchor(an, true);
                b.hint(AABB(V3(-4, y - 1, z - gap - 3), V3(4, y + 4, z - gap)), "Throw to the hook [{THROW}], then WARP [{WARP}]");
                R(1, an); R(0, V3(0, ny, z + 4));
                z += 6; y = ny;
            } break;
            case 3: {  // brazier gate
                float len = 14;
                b.slab(-5, z, 5, z + len, y, th.floorTop, th.floorSide, 3);
                bool timed = depth >= 6;
                bool sealed = bloom && depth >= 9 && r.f() < 0.4f;
                V3 b1(-8.5f, y, z + 6), b2(8.5f, y, z + 6);
                b.slab(b1.x - 1.3f, b1.z - 1.3f, b1.x + 1.3f, b1.z + 1.3f, y, th.floorSide, th.floorSide, 3);
                b.slab(b2.x - 1.3f, b2.z - 1.3f, b2.x + 1.3f, b2.z + 1.3f, y, th.floorSide, th.floorSide, 3);
                int grp = 10 + s;
                b.brazier(b1, grp, false, timed ? 7.f : 0.f);
                b.brazier(b2, grp, false, timed ? 7.f : 0.f);
                b.gate(AABB(V3(-5, y, z + len - 1), V3(5, y + 4.5f, z + len - 0.4f)), grp, "");
                b.wall(-14, z + len - 1, -5, z + len - 0.4f, y, y + 4.5f, th.wallTile); b.wall(5, z + len - 1, 14, z + len - 0.4f, y, y + 4.5f, th.wallTile);
                b.hint(AABB(V3(-5, y - 1, z), V3(5, y + 4, z + len - 1)), timed ? "Light both braziers before they fade!" : "Light both braziers to open the gate.");
                R(0, V3(0, y, z + 6)); R(2, b1 + V3(0, 1.35f, 0)); R(2, b2 + V3(0, 1.35f, 0)); R(0, V3(0, y, z + len + 0.5f));
                (void)sealed;
                z += len;
            } break;
            case 4: {  // treasure room
                float len = 10;
                b.slab(-6, z, 6, z + len, y, th.floorTop, th.floorSide, 3);
                std::vector<V3> pots;
                for (int i = 0; i < 4 + depth / 4 && i < 8; i++) { Level::Breakable u; u.pos = V3(r.range(-5, 5), y, z + r.range(1, len - 1)); u.kind = i % 2; L.breakables.push_back(u); }
                b.crystalCluster(V3(-5, y, z + len / 2), 0.7f, lightCol);
                b.crystalCluster(V3(5, y, z + len / 2), 0.7f, lightCol);
                if (r.f() < 0.5f) b.enemy(pickEnemy(), V3(0, y + 0.2f, z + len - 2));
                R(0, V3(0, y, z + len / 2)); R(3, V3(0, y, z + len / 2)); R(0, V3(0, y, z + len - 1));
                z += len;
            } break;
            case 5: {  // climb (warp up a ledge)
                b.slab(-5, z, 5, z + 6, y, th.floorTop, th.floorSide, 3);
                float ny = y + 4.5f;
                b.solid(AABB(V3(-5, y - 3, z + 6), V3(5, ny, z + 14)), th.floorSide, th.floorTop);
                V3 an(0, ny + 2.6f, z + 7.6f);
                b.anchor(an);
                b.hint(AABB(V3(-5, y - 1, z), V3(5, y + 4, z + 6)), "Warp up: throw to the hook above the ledge.");
                R(0, V3(0, y, z + 3)); R(1, an); R(0, V3(0, ny, z + 12));
                z += 14; y = ny;
            } break;
        }
        // city depths: office towers along the route, and a searchlight sweeping some stretches
        if (city) {
            for (int sd = -1; sd <= 1; sd += 2) {
                float h = 14 + 10 * hashf(s, sd, depth);
                L.decor.box(AABB(V3(sd * 15.f - 4, y - 12, z - 14), V3(sd * 15.f + 4, y + h, z)), T_OFFICE, 0.25f, T_TILEFLOOR);
            }
            if (s % 2 == 1) L.searchlights.push_back({V3(-5, y, z - 10), V3(5, y, z - 3), 6.5f + s * 0.4f, 0.3f * s, 3.0f, V3()});
            if (s > 0) L.reinforce.push_back(V3(0, y, z - 2));
        }
        // decoration between segments
        if (r.f() < 0.5f) b.light(V3(r.f() < 0.5f ? -4.f : 4.f, y + 3, z - 2), 9, lightCol, 0.6f);
    }

    // --- boss arena or exit platform
    if (bossLevel) {
        float zc = z + 18;
        b.slab(-16, z, 16, z + 36, y, th.floorTop, th.floorSide, 3);
        b.wall(-17, z, -16, z + 36, y, y + 6, th.wallTile); b.wall(16, z, 17, z + 36, y, y + 6, th.wallTile);
        b.wall(-16, z, -4, z + 0.6f, y, y + 6, th.wallTile); b.wall(4, z, 16, z + 0.6f, y, y + 6, th.wallTile);
        int bg = b.gate(AABB(V3(-4, y, z), V3(4, y + 5, z + 0.6f)), -1, "", T_GRATE);
        V3 C(0, y, zc);
        static const BossType ROT[3] = {B_BELLWETHER, B_WARDEN, B_RATKING};
        BossType bt = ROT[(depth / 5 - 1) % 3];
        if (bt == B_RATKING) b.solid(AABB(C + V3(-2.6f, 0, -2.6f), C + V3(2.6f, 3, 2.6f)), T_RUST, -1, V3(1, 1, 1), false);
        char flag[48]; snprintf(flag, sizeof(flag), "endless.boss.%d", depth);
        L.boss.type = bt; L.boss.center = C; L.boss.radius = 15;
        L.boss.trigger = AABB(V3(-14, y - 1, z + 4), V3(14, y + 8, z + 32)); L.boss.gates = {bg}; L.boss.flag = flag;
        for (int i = 0; i < 4; i++) b.light(C + V3(i < 2 ? -13.f : 13.f, 5, i % 2 ? -12.f : 12.f), 12, lightCol, 0.8f);
        b.slab(-3, z + 36, 3, z + 42, y, th.floorTop, th.floorSide, 3);
        b.exit(AABB(V3(-2.5f, y, z + 38), V3(2.5f, y + 4, z + 42)), LV_ENDLESS, 99, flag, "Defeat the guardian first!", "Deeper");
        R(0, V3(0, y, z + 3)); R(4, C); R(0, V3(-6, y, zc + 8)); R(0, V3(0, y, z + 37)); R(0, V3(0, y, z + 41));
    } else {
        b.slab(-6, z, 6, z + 10, y, th.floorTop, th.floorSide, 3);
        b.arch(V3(0, y, z + 7), 4, 4.5f, 1, th.wallTile, true);
        b.exit(AABB(V3(-1.8f, y, z + 6.4f), V3(1.8f, y + 4, z + 8.5f)), LV_ENDLESS, 99, "", "", "Deeper");
        b.light(V3(0, y + 3, z + 7), 10, lightCol, 1.2f);
        R(0, V3(0, y, z + 4)); R(0, V3(0, y, z + 8.2f));
    }
    b.emitter(AABB(V3(-12, y - 2, 0), V3(12, y + 8, z + 40)), city ? 6 : 4, city ? 30 : 6);
}

void Game::completeEndlessLevel() {
    int d = save.endlessDepth;
    save.endlessBest = std::max(save.endlessBest, d);
    int emb = 15 + 5 * d;
    Rng r(hash32((uint32_t)d * 31u + 7u));
    int wood = r.irange(2, 4), stone = r.irange(1, 3), crystal = d % 3 == 0 ? 1 + d / 10 : 0, gears = d % 5 == 0 ? 2 : 0;
    save.embers += emb;
    addItem(BK_LOG, wood, false); addItem(BK_COBBLE, stone, false); addItem(IT_CRYSTAL, crystal, false); addItem(IT_GEAR, gears, false);
    save.endlessDepth = d + 1;
    char sub[128];
    snprintf(sub, sizeof(sub), "+%d embers  +%d logs  +%d stone%s%s", emb, wood, stone, crystal ? "  +crystal" : "", gears ? "  +gears" : "");
    showToast("DEPTH " + std::to_string(d) + " CLEARED", sub);
    audio::play(SFX_HEART, 0.8f);
    if (save.endlessBest >= 10 && !has("home.unlocked")) {
        setFlag("home.unlocked");
        startDialog({{"", "Ten depths deep! Somewhere far above, a little island lights up."},
                     {"WICK'S ISLAND", "UNLOCKED! A portal in Lumen Hollow (west of the Spire) now leads to your own island."},
                     {"WICK'S ISLAND", "A whole island of blocks: mine it, build on it, craft beds and doors and lamps - and survive the nights."}});
    }
    writeSave();
    requestTransition(LV_ENDLESS, 0);
}
