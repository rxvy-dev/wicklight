// WICKLIGHT - Lumen City: "Wick is wanted".
// After GL1TCH's hack the FBI is hunting "the hacker" - and every poster has Wick's face on it.
// A night chase through the city: searchlights and patrolling agents on the main street, a hacked
// police barricade, a climb up the side of a tower, a rooftop fight, and a last stand on the
// helipad until Sprocket's rescue chopper arrives (with the EMBER GRAPPLE on board).
#include "builder.h"

void addPuzzle(LB& b, const AABB& zone, const std::vector<V3>& pos, const std::vector<float>& yaw, int len, uint32_t seed, int group, const std::string& flag, int reward, V3 rewardPos);

static void tower(LB& b, float x0, float z0, float x1, float z1, float h, int tile) {
    b.solid(AABB(V3(x0, 0, z0), V3(x1, h, z1)), tile, T_TILEFLOOR, V3(1, 1, 1), true, 0.25f);
    // a cornice so the skyline has some shape
    b.L.decor.box(AABB(V3(x0 - 0.3f, h - 0.4f, z0 - 0.3f), V3(x1 + 0.3f, h, z1 + 0.3f)), T_STONEBRICK, 1);
}

static void cityCar(LB& b, V3 c, float yaw, V3 col, bool police) {
    M4 m = M4::translate(c) * M4::rotY(yaw);
    b.L.decor.color = col;
    b.L.decor.cube(m * M4::translate(V3(0, 0.55f, 0)) * M4::scale(V3(1.9f, 0.7f, 4.2f)), T_METAL);
    b.L.decor.cube(m * M4::translate(V3(0, 1.15f, -0.2f)) * M4::scale(V3(1.7f, 0.6f, 2.2f)), T_METAL);
    b.L.decor.color = hexColor(0x30405a);
    b.L.decor.cube(m * M4::translate(V3(0, 1.15f, -0.2f)) * M4::scale(V3(1.75f, 0.45f, 2.0f)), T_WINDOW);
    b.L.decor.color = hexColor(0x181818);
    for (int i = 0; i < 4; i++) b.L.decor.cylinder(m * M4::translate(V3(i & 1 ? 0.95f : -0.95f, 0.35f, i & 2 ? 1.3f : -1.3f)) * M4::rotZ(PI / 2) * M4::scale(V3(0.7f, 0.25f, 0.7f)), T_TAR, 8);
    if (police) {
        b.L.decor.emis = 2.5f;
        b.L.decor.color = hexColor(0xff3030); b.L.decor.cube(m * M4::translate(V3(-0.35f, 1.55f, -0.2f)) * M4::scale(V3(0.5f, 0.2f, 0.3f)), T_WHITE);
        b.L.decor.color = hexColor(0x3060ff); b.L.decor.cube(m * M4::translate(V3(0.35f, 1.55f, -0.2f)) * M4::scale(V3(0.5f, 0.2f, 0.3f)), T_WHITE);
        b.L.decor.emis = 0;
        b.light(c + V3(0, 2.2f, 0), 6, hexColor(0x8050ff), 0.8f);
    }
    b.L.decor.color = V3(1, 1, 1);
    // collision: a plain box the size of the body
    V3 ax(std::cos(yaw), 0, -std::sin(yaw)), az(std::sin(yaw), 0, std::cos(yaw));
    V3 ext = V3(std::fabs(ax.x) * 0.95f + std::fabs(az.x) * 2.1f, 0, std::fabs(ax.z) * 0.95f + std::fabs(az.z) * 2.1f);
    b.solid(AABB(c - V3(ext.x, 0, ext.z), c + V3(ext.x, 1.45f, ext.z)), T_METAL, -1, V3(1, 1, 1), false);
}

static void neonSign(LB& b, V3 p, float yaw, float w, V3 col) {
    M4 m = M4::translate(p) * M4::rotY(yaw);
    b.L.decor.cube(m * M4::scale(V3(w + 0.2f, 1.0f, 0.15f)), T_METAL);
    b.L.decor.emis = 2.2f; b.L.decor.color = col;
    b.L.decor.cube(m * M4::translate(V3(0, 0, 0.09f)) * M4::scale(V3(w, 0.7f, 0.05f)), T_WHITE);
    b.L.decor.emis = 0; b.L.decor.color = V3(1, 1, 1);
    b.light(p + V3(std::sin(yaw), 0, std::cos(yaw)) * 1.2f, 6, col, 0.9f);
}

static void wantedPoster(LB& b, V3 p, float yaw) {
    M4 m = M4::translate(p) * M4::rotY(yaw);
    b.L.decor.color = hexColor(0xe8d8a8); b.L.decor.cube(m * M4::scale(V3(1.0f, 1.4f, 0.05f)), T_WHITE);
    b.L.decor.color = hexColor(0x3a2010); b.L.decor.cube(m * M4::translate(V3(0, 0.5f, 0.03f)) * M4::scale(V3(0.8f, 0.16f, 0.03f)), T_WHITE);
    b.L.decor.color = hexColor(0xeadfc4); b.L.decor.cube(m * M4::translate(V3(0, -0.15f, 0.03f)) * M4::scale(V3(0.4f, 0.5f, 0.03f)), T_WAX);
    b.L.decor.emis = 1.5f; b.L.decor.color = hexColor(0xffa040); b.L.decor.cube(m * M4::translate(V3(0, 0.2f, 0.03f)) * M4::scale(V3(0.16f, 0.22f, 0.03f)), T_WHITE);
    b.L.decor.emis = 0; b.L.decor.color = V3(1, 1, 1);
}

static void acUnit(LB& b, V3 c) {
    b.solid(AABB(c + V3(-0.9f, 0, -0.7f), c + V3(0.9f, 1.2f, 0.7f)), T_METAL, T_GRATE);
    b.L.decor.cylinder(M4::translate(c + V3(0, 1.25f, 0)) * M4::scale(V3(1.0f, 0.1f, 1.0f)), T_GRATE, 10);
}

void buildCity(Level& L, Game& g) {
    LB b(L);
    L.name = "Lumen City";
    L.song = SONG_CITY;
    L.env.skyTop = hexColor(0x050916); L.env.skyHorizon = hexColor(0x1f2a4e);
    L.env.fogColor = hexColor(0x111a30); L.env.fogDensity = 0.016f; L.env.fogStart = 10;
    L.env.ambSky = V3(0.16f, 0.18f, 0.28f); L.env.ambGround = V3(0.07f, 0.07f, 0.1f);
    L.env.sunColor = V3(0.2f, 0.24f, 0.4f); L.env.sunDir = norm(V3(0.3f, -1, 0.4f));
    L.env.stars = 0.5f; L.env.moonSize = 0.07f; L.env.moonDir = norm(V3(-0.3f, 0.45f, 1)); L.env.moonColor = hexColor(0xd0e0ff);
    L.killY = -10;
    V3 pink = hexColor(0xff50c0), cyan = hexColor(0x40f0ff), amber = hexColor(0xffb050);

    // --- the back alley (start)
    b.slab(-4, -8, 4, 22, 0, T_ASPHALT, T_STONEBRICK, 3);
    tower(b, -16, -8, -4, 22, 16, T_BRICK);
    tower(b, 4, -8, 16, 22, 12, T_OFFICE);
    b.wall(-4, -8.6f, 4, -8, 0, 8, T_BRICK);
    b.spawn(V3(0, 0, -2), 0);
    b.exit(AABB(V3(-3, -1, -8), V3(3, 3, -6)), LV_CLOCK, 3, "", "", "Back through GL1TCH's backdoor (the Clockwood)");
    b.hint(AABB(V3(-4, -1, -6), V3(4, 4, 8)), "WANTED: 'THE HACKER'. The FBI thinks it's YOU! Searchlights sweep the streets - if one finds you, agents come running.");
    b.hint(AABB(V3(-4, -1, 8), V3(4, 4, 22)), "Sprocket radioed: 'BEEP. Get to the HELIPAD on the tower at the north end. I am bringing the chopper.'");
    for (int i = 0; i < 3; i++) wantedPoster(b, V3(-3.97f, 2.4f, 2 + i * 6), PI / 2);
    for (int i = 0; i < 2; i++) wantedPoster(b, V3(3.97f, 2.4f, 5 + i * 8), -PI / 2);
    L.decor.color = hexColor(0x2a5a3a);
    L.decor.cube(M4::translate(V3(-3, 0.7f, 10)) * M4::scale(V3(1.6f, 1.4f, 2.6f)), T_METAL);  // dumpsters
    L.decor.cube(M4::translate(V3(3, 0.7f, 15)) * M4::scale(V3(1.6f, 1.4f, 2.6f)), T_METAL);
    L.decor.color = V3(1, 1, 1);
    b.solid(AABB(V3(-3.8f, 0, 8.7f), V3(-2.2f, 1.4f, 11.3f)), T_METAL, -1, V3(1, 1, 1), false);
    b.solid(AABB(V3(2.2f, 0, 13.7f), V3(3.8f, 1.4f, 16.3f)), T_METAL, -1, V3(1, 1, 1), false);
    neonSign(b, V3(-3.9f, 5, 16), PI / 2, 3, pink);
    b.light(V3(0, 4, 4), 7, amber, 0.8f);
    b.enemy(E_AGENT, V3(1, 0, 19));
    b.urns({V3(-3, 0, 2), V3(3, 0, 4), V3(-3, 0, 19)}, 1);

    // --- the main street: searchlights, patrols, parked cars for cover
    b.slab(-10, 22, 10, 70, 0, T_ASPHALT, T_STONEBRICK, 3);
    b.slab(-16, 22, -10, 70, 0, T_COBBLE, T_STONEBRICK, 3);
    b.slab(10, 22, 16, 70, 0, T_COBBLE, T_STONEBRICK, 3);
    for (float z = 26; z < 70; z += 6) L.decor.box(AABB(V3(-0.15f, 0.01f, z), V3(0.15f, 0.03f, z + 3)), T_WHITE, 1);  // lane dashes
    tower(b, -30, 22, -16, 70, 18, T_OFFICE);
    tower(b, 16, 22, 30, 38, 22, T_BRICK);
    tower(b, 16, 52, 30, 70, 22, T_OFFICE);
    tower(b, 20, 38, 30, 52, 7, T_BRICK);   // a low shop: its roof hides a secret
    b.wall(16, 38, 20, 38.6f, 0, 3, T_BRICK); b.wall(16, 51.4f, 20, 52, 0, 3, T_BRICK);
    b.anchor(V3(20.6f, 8.8f, 45));
    b.pickup(PK_HAT, V3(26, 8.0f, 45), 10, "hat.10");
    neonSign(b, V3(19.9f, 5.2f, 45), -PI / 2, 5, cyan);
    neonSign(b, V3(-15.9f, 6, 46), PI / 2, 4, amber);
    neonSign(b, V3(15.9f, 7, 60), -PI / 2, 3.5f, pink);
    for (float z = 26; z < 70; z += 11) { b.lampPost(V3(-11, 0, z), true, hexColor(0xffd0a0)); b.lampPost(V3(11, 0, z + 5), true, hexColor(0xffd0a0)); }
    cityCar(b, V3(-7.5f, 0, 30), 0, hexColor(0x8a2a2a), false);
    cityCar(b, V3(7.5f, 0, 37), 0, hexColor(0x2a4a8a), false);
    cityCar(b, V3(-7.5f, 0, 49), 0, hexColor(0xd0d0d8), true);
    cityCar(b, V3(7.5f, 0, 56), PI, hexColor(0x3a7a4a), false);
    for (int i = 0; i < 4; i++) wantedPoster(b, V3(-15.97f, 2.5f, 28 + i * 11), PI / 2);
    b.lantern(V3(-13, 0, 26), b.spawn(V3(-11.5f, 0, 27), 0));  // spawn 1
    L.searchlights.push_back({V3(-8, 0, 33), V3(8, 0, 41), 7.0f, 0.0f, 3.2f, V3()});
    L.searchlights.push_back({V3(8, 0, 50), V3(-8, 0, 58), 6.5f, 0.5f, 3.2f, V3()});
    b.enemy(E_AGENT, V3(-5, 0, 40)); b.enemy(E_AGENT, V3(5, 0, 58));
    b.enemy(E_DRONE, V3(0, 0, 47));
    for (V3 p : {V3(-13, 0, 23), V3(13, 0, 23), V3(-13, 0, 68), V3(13, 0, 68), V3(-14, 0, 46), V3(14, 0, 33)}) L.reinforce.push_back(p);
    b.hint(AABB(V3(-16, -1, 22), V3(16, 4, 32)), "Stay out of the searchlights! Duck behind cars. Agents with flashlights see in front of them - burn them from behind.");
    b.urns({V3(-12, 0, 35), V3(12, 0, 41), V3(-12, 0, 57), V3(12, 0, 64)}, 1);

    // --- the barricade: GL1TCH's hack locked the city's traffic system too
    b.gate(AABB(V3(-16, 0, 70), V3(16, 4.2f, 70.6f)), 1, "city.g1", T_GRATE);
    {
        std::vector<V3> tp = {V3(-9, 0, 63), V3(9, 0, 63), V3(0, 0, 67)};
        std::vector<float> ty = {PI * 0.75f, -PI * 0.75f, PI};
        addPuzzle(b, AABB(V3(-16, -1, 59), V3(16, 5, 70)), tp, ty, 3, 33, 1, "city.g1", -1, V3());
    }
    b.hint(AABB(V3(-16, -1, 59), V3(16, 4, 70)), "Police barricade - hacked by GL1TCH. WATCH the traffic boxes flash, then BURN them in order [{THROW}].", "", "city.g1");
    cityCar(b, V3(-12, 0, 68), PI / 2, hexColor(0xd0d0d8), true);
    cityCar(b, V3(12, 0, 68), -PI / 2, hexColor(0xd0d0d8), true);

    // --- the plaza below the tower
    b.slab(-20, 70.6f, 20, 100, 0, T_TILEFLOOR, T_STONEBRICK, 3);
    tower(b, -34, 70, -20, 100, 20, T_BRICK);
    tower(b, 20, 70, 34, 100, 20, T_OFFICE);
    tower(b, -20, 100, -12, 130, 22, T_OFFICE);
    tower(b, 12, 100, 20, 130, 22, T_BRICK);
    b.lantern(V3(-16, 0, 76), b.spawn(V3(-14.5f, 0, 77), 0));  // spawn 2
    L.decor.cylinder(M4::translate(V3(-9, 0.5f, 86)) * M4::scale(V3(5, 1, 5)), T_MARBLE, 14);  // fountain
    L.decor.color = hexColor(0x3a6a9a); L.decor.emis = 0.4f;
    L.decor.cylinder(M4::translate(V3(-9, 0.95f, 86)) * M4::scale(V3(4.4f, 0.1f, 4.4f)), T_WHITE, 14);
    L.decor.color = V3(1, 1, 1); L.decor.emis = 0;
    b.statue(V3(-9, 1, 86), 0.8f, 0.4f);
    b.solid(AABB(V3(-11.4f, 0, 83.6f), V3(-6.6f, 1, 88.4f)), T_MARBLE, -1, V3(1, 1, 1), false);
    L.searchlights.push_back({V3(-14, 0, 80), V3(14, 0, 93), 8.0f, 0.25f, 3.6f, V3()});
    b.enemy(E_AGENT, V3(10, 0, 80)); b.enemy(E_AGENT, V3(-4, 0, 95)); b.enemy(E_DRONE, V3(6, 0, 88));
    for (V3 p : {V3(-18, 0, 74), V3(18, 0, 74), V3(-18, 0, 97), V3(18, 0, 97)}) L.reinforce.push_back(p);
    neonSign(b, V3(-19.9f, 7, 85), PI / 2, 6, cyan);
    neonSign(b, V3(19.9f, 7, 90), -PI / 2, 5, pink);
    b.urns({V3(15, 0, 76), V3(-15, 0, 96), V3(16, 0, 96)}, 1);

    // --- the tower: climb its face hook by hook
    tower(b, -12, 100, 12, 128, 14, T_OFFICE);
    b.solid(AABB(V3(3, 4.5f, 97.5f), V3(9, 5, 100)), T_METAL, T_GRATE);
    b.anchor(V3(6, 6.6f, 98.6f));
    b.solid(AABB(V3(-9, 12, 97.5f), V3(-4, 12.5f, 100)), T_METAL, T_GRATE);
    b.anchor(V3(-6.5f, 14.6f, 98.6f));
    b.anchor(V3(0, 16.4f, 101.2f));
    for (float y : {5.f, 12.5f}) for (float x : {3.f, 9.f, -9.f, -4.f}) {
        if ((y < 6) != (x > 0)) continue;
        L.decor.cube(M4::translate(V3(x, y + 0.5f, 97.6f)) * M4::scale(V3(0.08f, 1.0f, 0.08f)), T_METAL);
    }
    b.hint(AABB(V3(-12, -1, 90), V3(12, 4, 100)), "Up the fire escapes! Throw to each hook and WARP [{WARP}].");

    // --- the rooftop (ambush)
    b.wall(-12.6f, 100, -12, 128, 14, 15.2f, T_STONEBRICK); b.wall(12, 100, 12.6f, 128, 14, 15.2f, T_STONEBRICK);
    b.wall(-12, 127.4f, -3, 128, 14, 15.2f, T_STONEBRICK); b.wall(3, 127.4f, 12, 128, 14, 15.2f, T_STONEBRICK);
    b.rail(V3(-12, 14, 100.2f), V3(-3, 14, 100.2f), 0.8f); b.rail(V3(3, 14, 100.2f), V3(12, 14, 100.2f), 0.8f);
    int rg = b.gate(AABB(V3(-3, 14, 127.4f), V3(3, 18, 128)), -1, "", T_GRATE);
    int ra = b.arena(AABB(V3(-11.5f, 13, 104), V3(11.5f, 20, 127)), {rg}, "city.arena");
    b.enemy(E_AGENT, V3(-6, 14, 118), ra); b.enemy(E_AGENT, V3(6, 14, 120), ra);
    b.enemy(E_DRONE, V3(-4, 14, 124), ra); b.enemy(E_DRONE, V3(4, 14, 124), ra);
    b.enemy(E_VIRUS, V3(0, 14.2f, 114), ra);
    for (V3 p : {V3(-8, 14, 108), V3(8, 14, 110), V3(-7, 14, 122), V3(9, 14, 124)}) acUnit(b, p);
    L.decor.cylinder(M4::translate(V3(8, 18, 116)) * M4::scale(V3(3.4f, 3.5f, 3.4f)), T_PLANK, 10);  // water tower
    L.decor.cone(M4::translate(V3(8, 20.4f, 116)) * M4::scale(V3(3.8f, 1.4f, 3.8f)), T_ROOF, 10);
    for (int i = 0; i < 4; i++) b.post(V3(8 + (i & 1 ? 1.2f : -1.2f), 14, 116 + (i & 2 ? 1.2f : -1.2f)), 2.4f, 0.12f, T_METAL);
    b.solid(AABB(V3(6.6f, 16.2f, 114.6f), V3(9.4f, 19.8f, 117.4f)), T_PLANK, -1, V3(1, 1, 1), false);
    b.lantern(V3(-10, 14, 103), b.spawn(V3(-8.5f, 14, 104), 0));  // spawn 3
    neonSign(b, V3(0, 15.6f, 127.2f), PI, 6, hexColor(0x60ff90));

    // --- the bridge and the helipad: hold out until Sprocket's chopper lands
    b.slab(-2.2f, 128, 2.2f, 140, 14, T_GRATE, T_METAL, 0.5f);
    b.rail(V3(-2.2f, 14, 128), V3(-2.2f, 14, 140), 1.0f); b.rail(V3(2.2f, 14, 128), V3(2.2f, 14, 140), 1.0f);
    b.invisibleWall(-2.8f, 128, -2.2f, 140, 14, 16); b.invisibleWall(2.2f, 128, 2.8f, 140, 14, 16);
    tower(b, -12, 140, 12, 168, 14, T_BRICK);
    b.wall(-12.6f, 140, -12, 168, 14, 15.2f, T_STONEBRICK); b.wall(12, 140, 12.6f, 168, 14, 15.2f, T_STONEBRICK);
    b.wall(-12, 167.4f, 12, 168, 14, 15.2f, T_STONEBRICK);
    b.wall(-12, 140, -2.2f, 140.6f, 14, 15.2f, T_STONEBRICK); b.wall(2.2f, 140, 12, 140.6f, 14, 15.2f, T_STONEBRICK);
    L.decor.color = hexColor(0xd0c040);
    L.decor.cylinder(M4::translate(V3(0, 14.03f, 155)) * M4::scale(V3(10, 0.04f, 10)), T_WHITE, 20, true);
    L.decor.color = hexColor(0x2a2a30);
    L.decor.cylinder(M4::translate(V3(0, 14.05f, 155)) * M4::scale(V3(9, 0.04f, 9)), T_WHITE, 20, true);
    L.decor.color = hexColor(0xf0f0f0);  // the big H
    L.decor.cube(M4::translate(V3(-1.6f, 14.07f, 155)) * M4::scale(V3(0.7f, 0.04f, 4.4f)), T_WHITE);
    L.decor.cube(M4::translate(V3(1.6f, 14.07f, 155)) * M4::scale(V3(0.7f, 0.04f, 4.4f)), T_WHITE);
    L.decor.cube(M4::translate(V3(0, 14.07f, 155)) * M4::scale(V3(2.6f, 0.04f, 0.7f)), T_WHITE);
    L.decor.color = V3(1, 1, 1);
    for (int i = 0; i < 8; i++) {
        float a = i * TAU / 8;
        L.decor.emis = 2; L.decor.color = hexColor(0x60ff60);
        L.decor.cube(M4::translate(V3(std::sin(a) * 10.4f, 14.15f, 155 + std::cos(a) * 10.4f)) * M4::scale(V3(0.3f, 0.2f, 0.3f)), T_WHITE);
        L.decor.emis = 0; L.decor.color = V3(1, 1, 1);
    }
    b.lantern(V3(-10, 14, 143), b.spawn(V3(-8.5f, 14, 144), 0));  // spawn 4
    L.hasHoldout = true;
    L.holdout.zone = AABB(V3(-11, 13, 144), V3(11, 20, 167));
    L.holdout.dur = 30; L.holdout.flag = "city.holdout";
    L.holdout.pad = V3(0, 14, 155);
    L.holdout.spawns = {V3(-10, 14, 146), V3(10, 14, 146), V3(-10, 14, 165), V3(10, 14, 165), V3(0, 14, 166)};
    b.hint(AABB(V3(-11, 13, 141), V3(11, 20, 167)), "HOLD OUT on the helipad until Sprocket's chopper arrives!", "", "city.holdout");
    b.exit(AABB(V3(-2.5f, 14, 158), V3(2.5f, 18, 162)), LV_HUB, 4, "pw.grapple", "Grab the package Sprocket dropped first!", "Fly home with Sprocket");
    if (g.has("city.holdout") && !g.has("pw.grapple")) b.pickup(PK_POWER, V3(0, 15.2f, 151), 2, "pw.grapple");

    b.emitter(AABB(V3(-20, 0, -8), V3(20, 30, 170)), 6, 40);
    b.emitter(AABB(V3(-16, 0, 20), V3(16, 3, 100)), 2, 3);
    b.light(V3(0, 18, 155), 14, hexColor(0xfff0d0), 0.6f);
}
