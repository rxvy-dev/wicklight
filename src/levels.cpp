// WICKLIGHT - the world: Lumen Hollow (hub), the Sunken Wharf, the Clockwood,
// the Ashen Vaults and the Lighthouse Spire.
#include "builder.h"

void addPuzzle(LB& b, const AABB& zone, const std::vector<V3>& pos, const std::vector<float>& yaw, int len, uint32_t seed, int group, const std::string& flag, int reward, V3 rewardPos);

static void dock(LB& b, float x0, float z0, float x1, float z1, float y, float postDepth = 3) {
    b.slab(x0, z0, x1, z1, y, T_PLANK, T_DARKWOOD, 0.4f);
    for (float x = std::min(x0, x1) + 0.5f; x <= std::max(x0, x1) - 0.4f; x += std::max(2.f, (std::fabs(x1 - x0) - 1) / 2))
        for (float z = std::min(z0, z1) + 0.5f; z <= std::max(z0, z1) - 0.4f; z += 3.f)
            b.post(V3(x, y - postDepth, z), postDepth - 0.4f, 0.18f);
}

static void boat(LB& b, V3 c, float yaw, float s = 1) {
    M4 m = M4::translate(c) * M4::rotY(yaw) * M4::scale(V3(s, s, s));
    b.L.decor.cube(m * M4::translate(V3(0, 0.3f, 0)) * M4::scale(V3(2.0f, 0.6f, 5.0f)), T_DARKWOOD);
    b.L.decor.cube(m * M4::translate(V3(0, 0.75f, 0)) * M4::scale(V3(2.2f, 0.3f, 5.2f)), T_PLANK);
    b.L.decor.cube(m * M4::translate(V3(0, 0.6f, 2.8f)) * M4::rotX(0.5f) * M4::scale(V3(1.4f, 0.6f, 1.0f)), T_DARKWOOD);
    b.L.decor.cylinder(m * M4::translate(V3(0, 2.4f, -0.5f)) * M4::scale(V3(0.15f, 3.6f, 0.15f)), T_DARKWOOD, 6);
}

static void stall(LB& b, V3 c, float yaw, int cloth) {
    M4 m = M4::translate(c) * M4::rotY(yaw);
    for (int i = 0; i < 4; i++) {
        float x = (i & 1) ? 1.3f : -1.3f, z = (i & 2) ? 0.9f : -0.9f;
        b.L.decor.cube(m * M4::translate(V3(x, 1.2f, z)) * M4::scale(V3(0.15f, 2.4f, 0.15f)), T_DARKWOOD);
    }
    b.L.decor.cube(m * M4::translate(V3(0, 0.5f, 0)) * M4::scale(V3(2.6f, 1.0f, 1.6f)), T_PLANK);
    b.L.decor.color = cloth == 0 ? hexColor(0xc04a3a) : hexColor(0x3a7ac0);
    b.L.decor.cube(m * M4::translate(V3(0, 2.5f, 0)) * M4::rotX(0.2f) * M4::scale(V3(3.0f, 0.1f, 2.2f)), T_CLOTH);
    b.L.decor.color = V3(1, 1, 1);
    b.solid(AABB(c + V3(-1.3f, 0, -0.8f), c + V3(1.3f, 1.0f, 0.8f)), T_PLANK, -1, V3(1, 1, 1), false);
}

// ================================================================== HUB
void buildHub(Level& L, Game& g) {
    LB b(L);
    bool dawn = g.has("game.finished");
    L.name = "Lumen Hollow";
    L.song = SONG_HUB;
    if (dawn) {
        L.env.skyTop = hexColor(0x2a3a6a); L.env.skyHorizon = hexColor(0xe08a6a);
        L.env.fogColor = hexColor(0x6a5a72); L.env.fogDensity = 0.012f;
        L.env.ambSky = V3(0.45f, 0.42f, 0.5f); L.env.ambGround = V3(0.2f, 0.16f, 0.18f);
        L.env.sunDir = norm(V3(-0.6f, -0.5f, 0.4f)); L.env.sunColor = V3(0.9f, 0.65f, 0.45f);
        L.env.stars = 0.2f; L.env.moonSize = 0.0f;
    } else {
        L.env.skyTop = hexColor(0x070a1c); L.env.skyHorizon = hexColor(0x2a2148);
        L.env.fogColor = hexColor(0x1b1730); L.env.fogDensity = 0.018f;
        L.env.ambSky = V3(0.16f, 0.17f, 0.28f); L.env.ambGround = V3(0.07f, 0.06f, 0.1f);
        L.env.sunColor = V3(0.22f, 0.24f, 0.4f);
    }
    L.waterY = -4; L.waterArea = AABB(V3(-160, -4, -160), V3(160, -4, 160)); L.killY = -8;
    L.waterColor = hexColor(0x142a44);

    // island ground
    b.slab(-36, -36, 36, 36, 0, T_COBBLE, T_STONEBRICK, 6);
    // west street and stairs down to the docks
    b.slab(-44, -5, -36, 5, 0, T_COBBLE, T_STONEBRICK, 6);
    for (int i = 0; i < 4; i++) b.slab(-44 - i, -4, -45 - i, 4, -0.5f * (i + 1), T_COBBLE, T_STONEBRICK, 4);
    dock(b, -60, -5, -48, 5, -2, 2.5f);
    b.lampPost(V3(-42, 0, 5.5f), true);
    b.lampPost(V3(-42, 0, -5.5f), true);
    boat(b, V3(-57, -3.6f, 9), 0.2f);
    boat(b, V3(-55, -3.6f, -10), -0.3f, 0.8f);
    b.exit(AABB(V3(-61, -3, -5), V3(-57, 2, 5)), LV_WHARF, 0, "", "", "To the Sunken Wharf");
    b.hint(AABB(V3(-48, -3, -6), V3(-36, 3, 6)), "The Sunken Wharf lies beyond these docks.");

    // the Lighthouse Spire (square base with a door alcove, round tower above)
    b.solid(AABB(V3(-7, 0, -7), V3(-2, 22, 7)), T_STONEBRICK);
    b.solid(AABB(V3(2, 0, -7), V3(7, 22, 7)), T_STONEBRICK);
    b.solid(AABB(V3(-2, 0, -3), V3(2, 22, 7)), T_STONEBRICK);
    b.solid(AABB(V3(-2, 5, -7), V3(2, 22, -3)), T_STONEBRICK);
    L.decor.cylinder(M4::translate(V3(0, 22 + 30, 0)) * M4::scale(V3(11, 60, 11)), T_STONEBRICK, 16, true, 30);
    for (int i = 0; i < 6; i++) L.decor.cylinder(M4::translate(V3(0, 26 + i * 10, 0)) * M4::scale(V3(11.6f, 0.6f, 11.6f)), T_MARBLE, 16, true);
    // dark lamp house at the top
    L.decor.cylinder(M4::translate(V3(0, 83, 0)) * M4::scale(V3(14, 1, 14)), T_METAL, 16);
    if (dawn) { L.decor.emis = 1.5f; L.decor.color = hexColor(0xffd070); }
    else L.decor.color = hexColor(0x202030);
    L.decor.cylinder(M4::translate(V3(0, 87, 0)) * M4::scale(V3(8, 7, 8)), T_WINDOW, 12);
    L.decor.emis = 0; L.decor.color = V3(1, 1, 1);
    L.decor.cone(M4::translate(V3(0, 93, 0)) * M4::scale(V3(10, 5, 10)), T_METAL, 12);
    if (dawn) b.light(V3(0, 87, 0), 60, hexColor(0xffc070), 1.5f);
    // windows up the tower
    for (int i = 0; i < 12; i++) {
        float a = i * 2.1f, y = 28 + i * 4.5f;
        V3 p(std::sin(a) * 5.55f, y, std::cos(a) * 5.55f);
        L.decor.color = V3(0.12f, 0.12f, 0.2f);
        L.decor.cube(M4::translate(p) * M4::rotY(a) * M4::scale(V3(1.0f, 1.6f, 0.2f)), T_WINDOW);
        L.decor.color = V3(1, 1, 1);
    }
    // the Spire door and its three pedestals
    int door = b.gate(AABB(V3(-2, 0, -7.2f), V3(2, 5, -6.6f)), 50, "hub.spiredoor", T_DOOR);
    (void)door;
    b.exit(AABB(V3(-1.8f, 0, -6.4f), V3(1.8f, 3, -3.2f)), LV_SPIRE, 0, "hub.spire", "The Spire door is sealed.", "Into the Spire");
    for (int i = 0; i < 4; i++) {
        V3 p(-5.25f + i * 3.5f, 0, (i == 1 || i == 2) ? -10.5f : -9.8f);
        b.pillar(p.x, p.z, 0, 1.4f, 0.5f, T_MARBLE);
    }
    b.talker(V3(0, 0, -10), PI, 1);
    b.spawn(V3(0, 0, -24.5f), 0);  // 0: workshop (game start / lantern)
    b.lantern(V3(4.5f, 0, -16.5f), 0);
    b.hint(AABB(V3(-5, 0, -13), V3(5, 3, -7.5f)), "Three pedestals ring the Spire door. [{INTERACT}] to inspect.");

    // Marrow's workshop (open to the north)
    b.wall(-7, -32, -6.4f, -19, 0, 5, T_PLASTER);
    b.wall(6.4f, -32, 7, -19, 0, 5, T_PLASTER);
    b.wall(-7, -32.6f, 7, -32, 0, 5, T_PLASTER);
    L.decor.box(AABB(V3(-7.6f, 5, -33), V3(7.6f, 5.5f, -18.4f)), T_ROOF, 1, T_ROOF);
    L.decor.box(AABB(V3(-7, 0.01f, -32), V3(7, 0.05f, -19)), T_PLANK);
    b.solid(AABB(V3(-6.3f, 0, -31.8f), V3(-4.5f, 3.5f, -27)), T_BOOKS);
    b.solid(AABB(V3(4.5f, 0, -31.8f), V3(6.3f, 3.5f, -27)), T_BOOKS);
    b.solid(AABB(V3(-3, 0, -31.5f), V3(3, 1.0f, -29.8f)), T_DARKWOOD, T_PLANK);  // workbench
    b.candles(V3(-1.5f, 1.0f, -30.6f), 5);
    b.candles(V3(2.0f, 1.0f, -30.4f), 3);
    L.decor.color = hexColor(0xd8c8a0);
    L.decor.cylinder(M4::translate(V3(0.5f, 1.4f, -30.6f)) * M4::scale(V3(0.3f, 0.8f, 0.3f)), T_WAX, 6);  // wax figures on the bench
    L.decor.cylinder(M4::translate(V3(-0.4f, 1.3f, -30.8f)) * M4::scale(V3(0.25f, 0.6f, 0.25f)), T_WAX, 6);
    L.decor.color = V3(1, 1, 1);
    b.crate(V3(5.0f, 0, -22), 1.0f);
    b.barrel(V3(-5.2f, 0, -21));
    b.spinner(V3(-6.3f, 3.2f, -24), 1.2f, 0.2f, 0, 0.3f);
    b.spinner(V3(-6.3f, 2.3f, -25.6f), 0.7f, 0.2f, 0, -0.5f);
    b.banner(V3(0, 4.6f, -32.3f), 3, 2.2f, 0, T_CLOTH);
    b.light(V3(0, 3.5f, -26), 9, hexColor(0xffb060), 0.9f);
    b.talker(V3(-1.8f, 0, -26.2f), 0.4f, 0);
    b.hint(AABB(V3(-6, 0, -31), V3(6, 4, -19)), "Talk to Marrow [{INTERACT}].", "", "talk.marrow0");

    // plaza dressing
    for (int i = 0; i < 8; i++) {
        float a = i * TAU / 8 + 0.4f;
        V3 p(std::sin(a) * 15, 0, std::cos(a) * 15);
        if (std::fabs(p.x) < 4 && p.z < 0) continue;
        b.lampPost(p, i % 2 == 0 || dawn, hexColor(0xffb060));
    }
    b.statue(V3(-12, 0, 10), 1.2f, 2.5f);
    b.statue(V3(12, 0, 10), 1.2f, -2.5f);
    // houses
    struct Hs { float x0, z0, x1, z1, h; } houses[] = {
        {-31, -31, -21, -23, 6}, {-34, -16, -26, -9, 5}, {-31, 14, -21, 24, 7}, {-14, 26, -7, 34, 5}, {7, 26, 14, 34, 6},
        {20, 14, 30, 24, 7}, {26, -16, 34, -8, 5}, {19, -31, 28, -22, 6}, {-33, 26, -24, 33, 5}, {24, 27, 33, 34, 5}};
    for (auto& h : houses) b.house(h.x0, h.z0, h.x1, h.z1, 0, h.h, T_PLASTER);
    b.tree(V3(-16, 0, -18), 1.0f); b.tree(V3(16, 0, -18), 1.1f); b.tree(V3(-20, 0, 2), 0.9f);
    b.tree(V3(22, 0, 3), 1.0f); b.tree(V3(-8, 0, 22), 0.8f); b.tree(V3(9, 0, 20), 0.9f);
    b.barrel(V3(-24, 0, -6)); b.barrel(V3(-23, 0, -7.2f)); b.crate(V3(25, 0, 8), 1.2f); b.crate(V3(25.5f, 1.2f, 8), 0.8f, true);
    b.emitter(AABB(V3(-30, 1, -30), V3(30, 8, 30)), 4, 6);
    b.urns({V3(-10, 0, -16), V3(10, 0, -15), V3(-20, 0, -5), V3(18, 0, 4), V3(-38, 0, 3), V3(-39, 0, -3.5f), V3(-50, -2, -3.5f), V3(-50, -2, 3.5f), V3(-4, 0, 30), V3(4, 0, 30), V3(38, 0, 3.5f)});
    b.emitter(AABB(V3(-60, -3.5f, -60), V3(60, -2, 60)), 2, 3);
    // island edges
    b.rock(V3(-37, -3, -30), 3); b.rock(V3(37, -3, 25), 3.5f); b.rock(V3(-30, -3, 37), 3); b.rock(V3(28, -3, -37), 3);

    // north: the broken bridge to the Clockwood
    b.slab(-3, 36, 3, 44, 0, T_PLANK, T_DARKWOOD, 0.5f);
    b.slab(-3, 56, 3, 68, 0, T_PLANK, T_DARKWOOD, 0.5f);
    for (float z = 38; z < 68; z += 6) { if (z > 44 && z < 56) continue; b.pillar(-2.5f, z, -6, 0, 0.4f, T_STONEBRICK); b.pillar(2.5f, z, -6, 0, 0.4f, T_STONEBRICK); }
    for (int i = 0; i < 4; i++) L.decor.cube(M4::translate(V3(-1.5f + i, -0.2f, 44.6f + (i % 2) * 0.5f)) * M4::rotX(0.5f) * M4::scale(V3(0.8f, 0.15f, 1.6f)), T_PLANK);
    b.rail(V3(-3, 0, 36), V3(-3, 0, 44), 1.0f); b.rail(V3(3, 0, 36), V3(3, 0, 44), 1.0f);
    b.rail(V3(-3, 0, 56), V3(-3, 0, 66), 1.0f); b.rail(V3(3, 0, 56), V3(3, 0, 66), 1.0f);
    b.anchor(V3(0, 2.2f, 57.5f), true);
    b.anchor(V3(0, 2.2f, 42.5f), true);
    b.lampPost(V3(2.5f, 0, 37), true);
    b.lampPost(V3(-2.5f, 0, 64), false);
    b.arch(V3(0, 0, 66.5f), 4, 4.5f, 1, T_STONEBRICK, true);
    b.exit(AABB(V3(-2.5f, 0, 67), V3(2.5f, 4, 70)), LV_CLOCK, 0, "", "", "To the Clockwood");
    b.hint(AABB(V3(-4, 0, 34), V3(4, 3, 44)), "The bridge is broken. Your flame can cross what your feet cannot...", "", "ab.warp");
    b.hint(AABB(V3(-4, 0, 34), V3(4, 3, 44)), "Throw your flame across [{THROW}], then WARP to it [{WARP}].", "ab.warp", "ab.grapple");
    b.hint(AABB(V3(-4, 0, 34), V3(4, 3, 44)), "Throw your flame onto the hook, then HOLD [{GRAPPLE}] to swing across on the Ember Grapple!", "ab.grapple");
    b.spawn(V3(0, 0, -11.5f), PI);  // 1 placeholder (overwritten below)
    L.spawns[1] = {V3(-52, -2, 0), PI / 2};   // 1: west docks (from the Wharf)
    b.spawn(V3(0, 0, 61), PI);                // 2: north bridge (from the Clockwood)
    b.spawn(V3(39, 0, 0), -PI / 2);           // 3: east road (from the Vaults)
    b.spawn(V3(0, 0, -12), PI);               // 4: Spire door (from the Spire)

    // east: the old mausoleum and its wax seal
    b.slab(36, -5, 52, 5, 0, T_COBBLE, T_STONEBRICK, 6);
    b.wall(44, 4, 53, 5, 0, 6, T_STONEBRICK);
    b.wall(44, -5, 53, -4, 0, 6, T_STONEBRICK);
    b.wall(52, -5, 53, 5, 0, 6, T_STONEBRICK);
    b.wall(44, -4, 45, -2.5f, 0, 6, T_STONEBRICK);
    b.wall(44, 2.5f, 45, 4, 0, 6, T_STONEBRICK);
    b.wall(44, -2.5f, 45, 2.5f, 4.5f, 6, T_STONEBRICK);
    L.decor.box(AABB(V3(43.5f, 6, -5.5f), V3(53.5f, 6.6f, 5.5f)), T_OBSIDIAN, 1);
    L.decor.cone(M4::translate(V3(48.5f, 8, 0)) * M4::scale(V3(12, 3, 12)), T_OBSIDIAN, 4);
    b.seal(AABB(V3(44.1f, 0, -2.5f), V3(44.9f, 4.5f, 2.5f)), "hub.vaultseal");
    b.exit(AABB(V3(46, 0, -3), V3(51, 3, 3)), LV_VAULTS, 0, "", "", "Down to the Ashen Vaults");
    b.hint(AABB(V3(38, 0, -5), V3(44, 3, 5)), "A seal of old red wax. Your flame alone cannot melt it.", "", "ab.bloom");
    b.hint(AABB(V3(38, 0, -5), V3(44, 3, 5)), "BLOOM [{BLOOM}] near the seal to melt it.", "ab.bloom", "hub.vaultseal");
    b.tomb(V3(40, 0, 4), PI); b.tomb(V3(40, 0, -4), 0);
    b.candles(V3(43, 0, 3.5f), 4);
    b.lampPost(V3(37, 0, -4.5f), true, hexColor(0xff6a50));

    // Sprocket's bot shop
    b.solid(AABB(V3(10.5f, 0, -15.2f), V3(14.5f, 1.1f, -14.2f)), T_DARKWOOD, T_METAL);
    b.banner(V3(12.5f, 3.6f, -15.6f), 3.2f, 1.4f, 0, T_CLOTH);
    for (int i = 0; i < 2; i++) b.post(V3(10.6f + i * 3.8f, 0, -15.6f), 3.7f, 0.12f, T_METAL);
    L.decor.emis = 2; L.decor.color = hexColor(0x40f0ff);
    L.decor.cube(M4::translate(V3(12.5f, 4.0f, -15.55f)) * M4::scale(V3(2.6f, 0.3f, 0.08f)), T_WHITE);
    L.decor.emis = 0; L.decor.color = V3(1, 1, 1);
    b.light(V3(12.5f, 3, -14), 7, hexColor(0x40f0ff), 0.9f);
    b.talker(V3(12.5f, 0, -16.2f), 0, 3);
    b.talker(V3(-16, 0, 4), PI / 2, 8);  // Pip
    // the Junkyard gate (south-west)
    b.slab(-18, -46, -10, -36, 0, T_ASH, T_STONEBRICK, 6);
    b.wall(-18.6f, -46, -18, -36, 0, 4, T_RUST); b.wall(-10, -46, -9.4f, -36, 0, 4, T_RUST);
    b.arch(V3(-14, 0, -44), 5, 4.5f, 1, T_RUST, true);
    L.decor.emis = 2; L.decor.color = hexColor(0xff50c0);
    L.decor.cube(M4::translate(V3(-14, 5.9f, -44)) * M4::scale(V3(5, 0.5f, 0.15f)), T_WHITE);
    L.decor.emis = 0; L.decor.color = V3(1, 1, 1);
    b.light(V3(-14, 5, -43), 9, hexColor(0xff50c0), 1.0f);
    b.exit(AABB(V3(-16.5f, 0, -46.5f), V3(-11.5f, 4, -44.6f)), LV_JUNK, 0, "heart.2", "The Junkyard gate is rusted shut. (Clear the Ashen Vaults first)", "To the Junkyard");
    b.hint(AABB(V3(-18, -1, -44), V3(-10, 4, -36)), "The Junkyard of Lost Machines. Something squeaks in there...");
    b.spawn(V3(-14, 0, -40), 0);  // 5: from the Junkyard
    // portal to the Endless Dark (east of the Spire) and to Wick's Island (west)
    b.arch(V3(14, 0, 0), 3.2f, 4.2f, 1, T_OBSIDIAN, false);
    b.exit(AABB(V3(13.6f, 0, -1.5f), V3(14.4f, 4, 1.5f)), LV_ENDLESS, 0, "talk.marrow0", "Talk to Marrow first.", "Into the Endless Dark");
    L.decor.emis = 2; L.decor.color = hexColor(0x9a5aff);
    L.decor.cube(M4::translate(V3(14, 5.4f, 0)) * M4::scale(V3(0.15f, 0.4f, 4.6f)), T_WHITE);
    L.decor.emis = 0; L.decor.color = V3(1, 1, 1);
    b.light(V3(14, 3, 0), 8, hexColor(0x9a5aff), 1.0f);
    b.hint(AABB(V3(10, -1, -3), V3(13.5f, 4, 3)), "THE ENDLESS DARK: levels that never end. Each depth is harder - and pays better.");
    b.arch(V3(-14, 0, 0), 3.2f, 4.2f, 1, T_MARBLE, false);
    b.exit(AABB(V3(-14.4f, 0, -1.5f), V3(-13.6f, 4, 1.5f)), LV_HOME, 0, "home.unlocked", "Wick's Island unlocks after you clear 10 depths of the Endless Dark.", "To Wick's Island");
    L.decor.emis = 2; L.decor.color = hexColor(0x60ff90);
    L.decor.cube(M4::translate(V3(-14, 5.4f, 0)) * M4::scale(V3(0.15f, 0.4f, 4.6f)), T_WHITE);
    L.decor.emis = 0; L.decor.color = V3(1, 1, 1);
    b.light(V3(-14, 3, 0), 8, hexColor(0x60ff90), 1.0f);
    b.spawn(V3(8.5f, 0, 0), 0);   // 6: back from the Endless Dark
    b.spawn(V3(-8.5f, 0, 0), 0);   // 7: back from Wick's Island

    // ---- east bridge to Lumen City (the FBI level)
    b.slab(36, 9, 56, 15, 0, T_PLANK, T_DARKWOOD, 0.5f);
    for (float x = 38; x < 56; x += 4.5f) { b.pillar(x, 9.4f, -6, 0, 0.35f, T_STONEBRICK); b.pillar(x, 14.6f, -6, 0, 0.35f, T_STONEBRICK); }
    b.rail(V3(36, 0, 9.1f), V3(51, 0, 9.1f), 1.0f); b.rail(V3(36, 0, 14.9f), V3(51, 0, 14.9f), 1.0f);
    b.arch(V3(52, 0, 12), 4.4f, 4.6f, 1, T_BRICK, false);
    L.decor.emis = 2.2f;
    L.decor.color = hexColor(0xff3030); L.decor.cube(M4::translate(V3(52, 5.9f, 10.2f)) * M4::scale(V3(0.5f, 0.4f, 1.2f)), T_WHITE);
    L.decor.color = hexColor(0x3060ff); L.decor.cube(M4::translate(V3(52, 5.9f, 13.8f)) * M4::scale(V3(0.5f, 0.4f, 1.2f)), T_WHITE);
    L.decor.emis = 0; L.decor.color = V3(1, 1, 1);
    b.light(V3(52, 5, 10.5f), 7, hexColor(0xff3030), 0.9f); b.light(V3(52, 5, 13.5f), 7, hexColor(0x3060ff), 0.9f);
    b.exit(AABB(V3(52.6f, 0, 10), V3(55, 4, 14)), LV_CITY, 0, "boss.1", "Police barricade. (Beat GL1TCH in the Clockwood first - then the FBI comes looking for YOU)", "To Lumen City");
    b.hint(AABB(V3(37, -1, 9), V3(52, 3, 15)), "LUMEN CITY. Ever since GL1TCH, the FBI has been hunting 'the hacker'... and the posters have YOUR face.", "boss.1", "fbi.done");
    b.hint(AABB(V3(37, -1, 9), V3(52, 3, 15)), "A barricade blocks the road to Lumen City.", "", "boss.1");
    b.spawn(V3(46, 0, 12), -PI / 2);   // 8: back from Lumen City
    if (g.has("boss.1")) {  // WANTED poster
        L.decor.color = hexColor(0xe8d8a8); L.decor.cube(M4::translate(V3(19.93f, 2.6f, 18)) * M4::scale(V3(0.06f, 1.6f, 1.2f)), T_WHITE);
        L.decor.color = hexColor(0x3a2010); L.decor.cube(M4::translate(V3(19.9f, 3.15f, 18)) * M4::scale(V3(0.06f, 0.18f, 0.9f)), T_WHITE);
        L.decor.color = hexColor(0xeadfc4); L.decor.cube(M4::translate(V3(19.9f, 2.4f, 18)) * M4::scale(V3(0.06f, 0.6f, 0.5f)), T_WAX);
        L.decor.emis = 1.5f; L.decor.color = hexColor(0xffa040); L.decor.cube(M4::translate(V3(19.9f, 2.85f, 18)) * M4::scale(V3(0.06f, 0.25f, 0.18f)), T_WHITE);
        L.decor.emis = 0; L.decor.color = V3(1, 1, 1);
    }

    // ---- side jobs around the Hollow
    // Mira lost her cat Whiskers - he's up in the old treehouse west of the plaza
    L.decor.cylinder(M4::translate(V3(-28, 4, 0)) * M4::scale(V3(0.9f, 8, 0.9f)), T_BARK, 8, true, 3);
    b.solid(AABB(V3(-28.4f, 0, -0.4f), V3(-27.6f, 6.6f, 0.4f)), T_BARK, -1, V3(1, 1, 1), false);
    b.solid(AABB(V3(-30, 6.6f, -2), V3(-26, 7, 2)), T_PLANK, T_PLANK);
    b.rail(V3(-30, 7, -2), V3(-30, 7, 2), 0.7f);
    L.decor.sphere(M4::translate(V3(-28, 11.5f, 0)) * M4::scale(V3(6.5f, 4.5f, 6.5f)), T_LEAVES, 8, 5);
    L.decor.sphere(M4::translate(V3(-29.5f, 10.5f, 2)) * M4::scale(V3(4, 3, 4)), T_LEAVES, 7, 5);
    b.anchor(V3(-26.3f, 8.6f, 0));
    b.talker(V3(-22, 0, -2), -PI / 2, 11);  // Mira
    if (g.has("quest.catfound")) b.talker(V3(-22.4f, 0, -0.6f), -PI / 2, 12);
    else b.talker(V3(-28.6f, 7, 0.6f), PI / 2, 12);  // Whiskers, stuck in the treehouse
    // Old Fenn wants the six street braziers relit
    b.talker(V3(-6, 0, 14), PI, 13);
    for (V3 p : {V3(31, 0, -2), V3(16, 0, -28), V3(-18, 0, -28), V3(-33, 0, 9), V3(-2, 0, 31), V3(18, 0, 31)}) b.brazier(p, 70);
    // Flick runs the candle race
    b.talker(V3(2, 0, 14), PI, 14);
    {
        struct RR { float x, y, z; bool high; } rr[] = {{10, 1.4f, 18, false}, {19, 1.4f, 9, false}, {20, 2.9f, -2, true}, {13, 1.4f, -11, false}, {7, 1.4f, -16.5f, false}, {-7, 1.4f, -16.5f, false},
                                                       {-17, 1.4f, -11, false}, {-21, 2.9f, -2, true}, {-19, 1.4f, 9, false}, {-10, 1.4f, 19, false}, {0, 1.4f, 25, false}, {5, 1.4f, 16, false}};
        for (int i = 0; i < 12; i++) {
            V3 p(rr[i].x, rr[i].y, rr[i].z);
            V3 nx = i + 1 < 12 ? V3(rr[i + 1].x, rr[i + 1].y, rr[i + 1].z) : V3(2, 0, 14);
            V3 pv = i > 0 ? V3(rr[i - 1].x, rr[i - 1].y, rr[i - 1].z) : V3(2, 0, 14);
            V3 d = flatXZ(nx - pv);
            L.race.push_back({p, std::atan2(d.x, d.z), rr[i].high});
        }
    }
    // the wardrobe mirror in Marrow's workshop (change hats)
    b.talker(V3(5.6f, 0, -25.5f), -PI / 2, 15);
    b.hint(AABB(V3(3.5f, 0, -27.5f), V3(6.4f, 3, -23.5f)), "A tall old mirror. [{INTERACT}] to try on the hats you've found.");


    L.boss.type = B_NONE;
}

// ================================================================== SUNKEN WHARF
void buildWharf(Level& L, Game&) {
    LB b(L);
    L.name = "The Sunken Wharf";
    L.song = SONG_WHARF;
    L.env.skyTop = hexColor(0x04101a); L.env.skyHorizon = hexColor(0x1a4048);
    L.env.fogColor = hexColor(0x123036); L.env.fogDensity = 0.028f; L.env.fogStart = 6;
    L.env.ambSky = V3(0.1f, 0.2f, 0.22f); L.env.ambGround = V3(0.04f, 0.07f, 0.08f);
    L.env.sunColor = V3(0.15f, 0.25f, 0.3f); L.env.sunDir = norm(V3(0.3f, -1, 0.5f));
    L.env.moonDir = norm(V3(-0.2f, 0.35f, 1)); L.env.moonColor = hexColor(0xc0f0f0);
    L.waterY = -0.8f; L.waterArea = AABB(V3(-80, -0.8f, -40), V3(80, -0.8f, 230)); L.killY = -6;
    L.waterColor = hexColor(0x1a5a60);

    // --- start dock & tutorial
    dock(b, -3, -8, 3, 10, 0);
    b.spawn(V3(0, 0, -1), 0);
    b.exit(AABB(V3(-3, -1, -9.5f), V3(3, 3, -7)), LV_HUB, 1, "", "", "Back to Lumen Hollow");
    b.talker(V3(-2.2f, 0, 2.5f), PI / 2, 4);  // Gil the fisher
    b.hint(AABB(V3(-3, -1, -7), V3(3, 3, 5)), "Mist on the water. Move with WASD, look with the mouse.");
    dock(b, -3, 12, 3, 20, 0);
    b.hint(AABB(V3(-3, -1, 5), V3(3, 3, 12)), "Jump the gap [{JUMP}].");
    dock(b, -7, 20, 7, 30, 0);
    b.lampPost(V3(-6, 0, 21), false); b.lampPost(V3(6, 0, 21), false);
    boat(b, V3(-8, -0.7f, 6), 0.3f); boat(b, V3(9, -0.7f, 14), -0.2f, 0.9f);
    // first brazier on a rock in the water -> gate
    b.slab(6.5f, 24.5f, 9.5f, 27.5f, 0, T_ROCK, T_ROCK, 3);
    b.brazier(V3(8, 0, 26), 0);
    b.gate(AABB(V3(-7, 0, 30), V3(7, 4.5f, 30.6f)), 0, "wharf.g1");
    b.wall(-14, 30, -7, 30.6f, 0, 4.5f, T_STONEBRICK); b.wall(7, 30, 14, 30.6f, 0, 4.5f, T_STONEBRICK);
    b.hint(AABB(V3(-7, -1, 20), V3(7, 4, 30)), "Aim at the brazier and THROW your flame [{THROW}]. Press [{THROW}] again to call it back.", "", "wharf.g1");

    // --- fish-market square
    b.slab(-12, 30.6f, 12, 58, 0, T_COBBLE, T_STONEBRICK, 3);
    b.lantern(V3(9, 0, 35), b.spawn(V3(7.5f, 0, 36.5f), 0));
    stall(b, V3(-8, 0, 40), 0.1f, 0); stall(b, V3(8, 0, 46), -0.1f, 1); stall(b, V3(-7, 0, 50), 0, 1);
    b.barrel(V3(10, 0, 52)); b.barrel(V3(10.6f, 0, 53.2f)); b.crate(V3(-10.5f, 0, 34), 1.2f);
    b.enemy(E_GLOOMLING, V3(-3, 0.2f, 43)); b.enemy(E_GLOOMLING, V3(4, 0.2f, 47)); b.enemy(E_GLOOMLING, V3(0, 0.2f, 53));
    b.hint(AABB(V3(-12, -1, 31), V3(12, 4, 38)), "Gloomlings! Your flame burns them on the way OUT and on the way BACK.");
    b.house(-20, 32, -13, 44, 0, 6, T_PLASTER); b.house(13, 36, 20, 50, 0, 7, T_PLASTER);
    // secret ledge with a memory candle
    b.crate(V3(-8.5f, 0, 55.6f), 1.1f);
    b.solid(AABB(V3(-12, 0, 54), V3(-9.2f, 2.2f, 58)), T_STONEBRICK, T_COBBLE);
    b.pickup(PK_CANDLE, V3(-10.6f, 2.9f, 56), 0, "candle.0");
    b.wall(-12, 57.4f, -3, 58, 0, 1.0f, T_STONEBRICK); b.wall(3, 57.4f, 12, 58, 0, 1.0f, T_STONEBRICK);

    // --- stepping stones & tarbells
    struct St { float x, z; } stones[] = {{0, 61}, {3, 65}, {-1, 69}, {2.5f, 73}, {0, 77}, {-3, 81}, {0, 85}, {0, 88.4f}};
    for (auto& s : stones) { b.slab(s.x - 1.4f, s.z - 1.4f, s.x + 1.4f, s.z + 1.4f, 0.3f, T_BARNACLE, T_ROCK, 4); }
    b.slab(-11, 67, -7, 72, 0.6f, T_ROCK, T_ROCK, 4); b.enemy(E_TARBELL, V3(-9, 0.6f, 69.5f));
    b.slab(7, 77, 11, 82, 0.6f, T_ROCK, T_ROCK, 4); b.enemy(E_TARBELL, V3(9, 0.6f, 79.5f));
    b.hint(AABB(V3(-4, -1, 58), V3(4, 4, 63)), "Tarbells spit tar from afar. DASH [{DASH}] - nothing can touch you mid-dash.");
    // warehouse with a shard on the roof (needs Warp)
    b.solid(AABB(V3(14, -1, 60), V3(22, 7, 72)), T_PLANK, T_ROOF);
    for (int i = 0; i < 3; i++) L.decor.cube(M4::translate(V3(13.95f, 3.5f, 63 + i * 3)) * M4::scale(V3(0.06f, 1.2f, 1.0f)), T_WINDOW);
    b.anchor(V3(16.5f, 8.4f, 66));
    b.pickup(PK_SHARD, V3(19.5f, 7.8f, 66), 0, "shard.wharf0");
    b.rock(V3(-14, -0.8f, 78), 2); b.rock(V3(12, -0.8f, 92), 2.5f);

    // --- the net yard (ambush arena)
    b.slab(-12, 89.4f, 12, 118.6f, 0, T_PLANK, T_DARKWOOD, 1);
    b.wall(-13, 89.4f, -12, 118.6f, 0, 3, T_STONEBRICK); b.wall(12, 89.4f, 13, 118.6f, 0, 3, T_STONEBRICK);
    b.wall(-12, 89.4f, -3, 90, 0, 3, T_STONEBRICK); b.wall(3, 89.4f, 12, 90, 0, 3, T_STONEBRICK);
    b.wall(-12, 118, -3, 118.6f, 0, 3, T_STONEBRICK); b.wall(3, 118, 12, 118.6f, 0, 3, T_STONEBRICK);
    int ga = b.gate(AABB(V3(-3, 0, 89.4f), V3(3, 4, 90)), -1, "", T_GRATE);
    int gb = b.gate(AABB(V3(-3, 0, 118), V3(3, 4, 118.6f)), -1, "", T_GRATE);
    int ar = b.arena(AABB(V3(-11, -1, 92), V3(11, 5, 116)), {ga, gb}, "wharf.arena");
    b.enemy(E_GLOOMLING, V3(-6, 0.2f, 104), ar); b.enemy(E_GLOOMLING, V3(6, 0.2f, 104), ar);
    b.enemy(E_GLOOMLING, V3(-4, 0.2f, 110), ar); b.enemy(E_GLOOMLING, V3(4, 0.2f, 110), ar);
    b.enemy(E_TARBELL, V3(0, 0, 113), ar);
    b.enemy(E_GLOOMKING, V3(0, 0.2f, 106), ar);
    for (int i = 0; i < 4; i++) { b.post(V3(-11 + i * 7.3f, 0, 100), 3.5f, 0.15f); }
    b.chain(V3(-7, 6, 96), 3); b.chain(V3(7, 6, 112), 3);
    b.pickup(PK_CANDLE, V3(-10.3f, 0.7f, 116.3f), 1, "candle.1");
    b.crate(V3(10, 0, 92.5f), 1.2f); b.barrel(V3(-10, 0, 93));

    // --- the long pier, timed braziers
    dock(b, -4, 118.6f, 4, 154, 0);
    dock(b, -11, 130, -4, 134, 0); dock(b, 4, 130, 11, 134, 0);
    b.brazier(V3(-9.5f, 0, 132), 1, false, 5.0f);
    b.brazier(V3(9.5f, 0, 132), 1, false, 5.0f);
    b.gate(AABB(V3(-4, 0, 140), V3(4, 4.5f, 140.6f)), 1, "wharf.g2");
    b.wall(-9, 137, -4, 142, 0, 4.5f, T_STONEBRICK); b.wall(4, 137, 9, 142, 0, 4.5f, T_STONEBRICK);
    b.hint(AABB(V3(-4, -1, 120), V3(4, 4, 139)), "Brass-ringed braziers burn out fast. Light both before either fades.", "", "wharf.g2");
    dock(b, 4, 120, 11, 124, 0);
    b.pickup(PK_CANDLE, V3(9.8f, 0.7f, 122), 2, "candle.2");
    b.lantern(V3(3, 0, 146), b.spawn(V3(1.5f, 0, 147.5f), 0));
    // boathouse with a sealed shard (needs Bloom)
    dock(b, -14, 140, -4, 148, 0);
    b.wall(-14.5f, 140, -14, 148, 0, 4, T_PLANK); b.wall(-14.5f, 147.6f, -8, 148, 0, 4, T_PLANK); b.wall(-14.5f, 140, -8, 140.4f, 0, 4, T_PLANK);
    b.wall(-8.5f, 140, -8, 142.5f, 0, 4, T_PLANK); b.wall(-8.5f, 145.5f, -8, 148, 0, 4, T_PLANK); b.wall(-8.5f, 142.5f, -8, 145.5f, 3.4f, 4, T_PLANK);
    L.decor.box(AABB(V3(-15, 4, 139.5f), V3(-7.5f, 4.4f, 148.5f)), T_ROOF, 1, T_ROOF);
    b.seal(AABB(V3(-8.45f, 0, 142.5f), V3(-8.05f, 3.4f, 145.5f)), "wharf.seal");
    b.pickup(PK_SHARD, V3(-11.5f, 0.9f, 144), 1, "shard.wharf1");
    b.candles(V3(-13, 0, 146.5f), 3);

    // --- the drowned plaza (boss)
    b.slab(-16, 154, 16, 186, 0, T_COBBLE, T_STONEBRICK, 3);
    b.wall(-9, 150, -4, 154, 0, 4.5f, T_STONEBRICK); b.wall(4, 150, 9, 154, 0, 4.5f, T_STONEBRICK);
    int bg = b.gate(AABB(V3(-4, 0, 151), V3(4, 4.5f, 151.6f)), -1, "", T_GRATE);
    for (int i = 0; i < 8; i++) {
        float a = i * TAU / 8 + PI / 8;
        V3 p(std::sin(a) * 14.5f, 0, 170 + std::cos(a) * 14.5f);
        b.pillar(p.x, p.z, 0, 5, 0.6f, T_BARNACLE);
    }
    L.boss.type = B_BELLWETHER; L.boss.center = V3(0, 0, 170); L.boss.radius = 16;
    L.boss.trigger = AABB(V3(-13, -1, 157), V3(13, 6, 183)); L.boss.gates = {bg}; L.boss.flag = "boss.0";
    b.arch(V3(0, 0, 187), 5, 5, 1, T_STONEBRICK, true);
    b.slab(-4, 186, 4, 192, 0, T_COBBLE, T_STONEBRICK, 3);
    b.exit(AABB(V3(-2.5f, 0, 188), V3(2.5f, 4, 192)), LV_HUB, 1, "heart.0", "Take the Beacon-Heart first - it holds your next power.", "Return to Lumen Hollow");
    b.emitter(AABB(V3(-40, -0.6f, -10), V3(40, 1.5f, 200)), 2, 10);
    b.urns({V3(2.2f, 0, 6), V3(-2.2f, 0, 8), V3(-5, 0, 24), V3(5, 0, 28), V3(-10, 0, 38), V3(10, 0, 42), V3(-3, 0, 56), V3(4, 0, 56), V3(-10, 0, 100), V3(10, 0, 108), V3(-10, 0, 114), V3(3.2f, 0, 125), V3(-3.2f, 0, 128), V3(-3, 0, 144), V3(3, 0, 150), V3(-12, 0, 147)});
    b.enemy(E_GLOOMLING, V3(-8, 0.2f, 48)); b.enemy(E_GLOOMLING, V3(8, 0.2f, 53)); b.enemy(E_GLOOMLING, V3(0, 0.2f, 123)); b.enemy(E_GLOOMLING, V3(2, 0.2f, 136));
    b.emitter(AABB(V3(-20, 0, 0), V3(20, 6, 190)), 4, 4);
    // fishing spots (Gil's Golden Carp)
    b.talker(V3(2.5f, 0, 7.5f), PI / 2, 16);
    b.talker(V3(-10.4f, 0, 133.6f), -PI / 2, 16);
    b.talker(V3(10.6f, 0, 120.6f), PI / 2, 16);
    for (V3 p : {V3(2.75f, 0, 7.5f), V3(-10.7f, 0, 133.6f), V3(10.85f, 0, 120.6f)}) b.post(p, 1.2f, 0.06f, T_DARKWOOD);
    // a tiny island with a secret
    b.slab(-16.5f, 76.5f, -11.5f, 80.5f, 0.3f, T_ROCK, T_ROCK, 4);
    b.pickup(PK_HAT, V3(-14, 1.3f, 78.5f), 5, "hat.5");
}

// ================================================================== CLOCKWOOD
static void bigTree(LB& b, V3 base, float r, float h) {
    b.L.decor.cylinder(M4::translate(base + V3(0, h / 2, 0)) * M4::scale(V3(2 * r, h, 2 * r)), T_BARK, 10, true, h / 3);
    for (int i = 0; i < 5; i++) {
        float a = i * 1.3f;
        V3 o(std::cos(a) * r * 1.6f, h + std::sin(i * 2.1f) * 2, std::sin(a) * r * 1.6f);
        b.L.decor.sphere(M4::translate(base + o) * M4::scale(V3(r * 4, r * 2.5f, r * 4)), T_LEAVES, 8, 5);
    }
    b.solid(AABB(base + V3(-r * 0.8f, 0, -r * 0.8f), base + V3(r * 0.8f, h * 0.6f, r * 0.8f)), T_BARK, -1, V3(1, 1, 1), false);
}

void buildClock(Level& L, Game&) {
    LB b(L);
    L.name = "The Clockwood";
    L.song = SONG_CLOCK;
    L.env.skyTop = hexColor(0x081410); L.env.skyHorizon = hexColor(0x2a4430);
    L.env.fogColor = hexColor(0x18281e); L.env.fogDensity = 0.024f;
    L.env.ambSky = V3(0.14f, 0.2f, 0.14f); L.env.ambGround = V3(0.06f, 0.08f, 0.05f);
    L.env.sunColor = V3(0.25f, 0.3f, 0.2f); L.env.sunDir = norm(V3(-0.5f, -1, 0.2f));
    L.env.moonColor = hexColor(0xf0e8c0);
    L.killY = -12;

    // --- arrival & the first chasm (warp tutorial)
    b.slab(-8, -8, 8, 14, 0, T_GRASS, T_DIRT, 6);
    b.spawn(V3(0, 0, 0), 0);
    b.exit(AABB(V3(-3, -1, -9.5f), V3(3, 3, -7)), LV_HUB, 2, "", "", "Back to Lumen Hollow");
    b.talker(V3(4, 0, 2), -PI / 2, 5);  // Tilda the clockmaker
    b.arch(V3(0, 0, -6.5f), 4, 4.5f, 1, T_STONEBRICK, true);
    b.tree(V3(-6, 0, 4), 1.0f); b.tree(V3(6.5f, 0, 9), 0.9f);
    b.mushroom(V3(-5, 0, 11), 0.8f);
    b.hint(AABB(V3(-8, -1, 5), V3(8, 4, 14)), "Throw your flame across the chasm [{THROW}], then WARP [{WARP}] to it.");
    b.slab(-8, 26, 8, 36, 0, T_GRASS, T_DIRT, 6);
    b.anchor(V3(0, 2.0f, 29), true);
    // cliff
    b.solid(AABB(V3(-8, -6, 36), V3(8, 6, 60)), T_ROCK, T_GRASS);
    b.anchor(V3(0, 7.6f, 36.9f));
    b.hint(AABB(V3(-8, -1, 26), V3(8, 4, 36)), "Warping works upward too. Throw to the brass hook above the cliff.");
    b.pickup(PK_CANDLE, V3(6.8f, 0.7f, 33.5f), 3, "candle.3");
    b.tree(V3(5.5f, 0, 31), 0.8f);
    b.rock(V3(-6, 0, 33), 1);

    // --- clockwork lifts over the gorge
    b.mover(AABB(V3(-1.5f, 5.4f, 62), V3(1.5f, 6, 65)), V3(-6, 0, 0), V3(6, 0, 0), 6, 0, T_BRASS, T_METAL);
    b.mover(AABB(V3(-1.5f, 5.4f, 67), V3(1.5f, 6, 70)), V3(-6, 0, 0), V3(6, 0, 0), 6, 0, T_BRASS, T_METAL);
    b.spinner(V3(-9, 4, 66), 3, 0.6f, 2, 0.8f); b.spinner(V3(9, 4, 66), 3, 0.6f, 2, -0.8f);
    b.solid(AABB(V3(-10, -6, 72), V3(10, 6, 96)), T_ROCK, T_GRASS);
    b.lantern(V3(5, 6, 82), b.spawn(V3(3.5f, 6, 83.5f), 0));
    b.enemy(E_MOTH, V3(-4, 6, 86)); b.enemy(E_MOTH, V3(3, 6, 90)); b.enemy(E_MOTH, V3(-1, 6, 93));
    b.hint(AABB(V3(-10, 5, 72), V3(10, 9, 80)), "Moths smother a thrown flame. RECALL [{THROW}] it through them!");
    b.tree(V3(-8, 6, 76), 1.0f); b.tree(V3(8, 6, 92), 0.9f);
    b.mushroom(V3(8, 6, 75), 1.0f); b.mushroom(V3(-8.5f, 6, 90), 0.7f);
    L.decor.color = V3(1, 1, 1);
    L.decor.cube(M4::translate(V3(0, 7, 60.2f)) * M4::scale(V3(3, 3, 0.2f)), T_CLOCKFACE);

    // --- the great tree climb
    b.solid(AABB(V3(-10, -6, 96), V3(10, 6, 104)), T_ROCK, T_GRASS);
    bigTree(b, V3(0, 6, 112), 4.5f, 30);
    b.solid(AABB(V3(4, 8.5f, 104), V3(9, 9, 110)), T_PLANK, T_PLANK);
    b.anchor(V3(6.5f, 10.3f, 107));
    b.solid(AABB(V3(4, 11.5f, 114), V3(9, 12, 120)), T_PLANK, T_PLANK);
    b.anchor(V3(6.5f, 13.3f, 117));
    b.solid(AABB(V3(-9, 14.5f, 114), V3(-4, 15, 120)), T_PLANK, T_PLANK);
    b.anchor(V3(-6.5f, 16.3f, 117));
    b.solid(AABB(V3(-9, 17.5f, 104), V3(-4, 18, 110)), T_PLANK, T_PLANK);
    b.anchor(V3(-6.5f, 19.3f, 107));
    b.solid(AABB(V3(-14, 17.5f, 104), V3(-9, 18, 134)), T_PLANK, T_PLANK);
    b.rail(V3(-14, 18, 106), V3(-14, 18, 132), 1.0f);
    b.pickup(PK_CANDLE, V3(8.4f, 12.7f, 119.3f), 4, "candle.4");
    b.enemy(E_TARBELL, V3(-7, 6, 100.5f));
    b.hint(AABB(V3(-10, 5, 96), V3(10, 9, 104)), "Climb the great tree from hook to hook.");
    // crown with a shard (secret)
    b.solid(AABB(V3(-3, 25.5f, 109), V3(3, 26, 115)), T_PLANK, T_PLANK);
    b.anchor(V3(0, 27.3f, 112));
    b.pickup(PK_SHARD, V3(1.5f, 26.8f, 112), 0, "shard.clock0");
    b.spinner(V3(0, 22, 107.2f), 2.5f, 0.4f, 2, 0.4f);
    L.decor.cube(M4::translate(V3(0, 14, 107.4f)) * M4::scale(V3(4, 4, 0.2f)), T_CLOCKFACE);

    // --- gearwork clearing (ambush arena)
    b.solid(AABB(V3(-14, 12, 134), V3(14, 18, 166)), T_ROCK, T_GRASS);
    int ga = b.gate(AABB(V3(-14, 18, 134), V3(-9, 22, 134.6f)), -1, "", T_GRATE);
    int gb = b.gate(AABB(V3(-3, 18, 165.4f), V3(3, 22, 166)), -1, "", T_GRATE);
    b.wall(-14, 165.4f, -3, 166, 18, 21, T_STONEBRICK); b.wall(3, 165.4f, 14, 166, 18, 21, T_STONEBRICK);
    b.wall(-9, 134, 14, 134.6f, 18, 21, T_STONEBRICK);
    b.wall(-14.6f, 134, -14, 166, 18, 21, T_STONEBRICK); b.wall(14, 134, 14.6f, 166, 18, 21, T_STONEBRICK);
    int ar = b.arena(AABB(V3(-13, 17, 137), V3(13, 23, 164)), {ga, gb}, "clock.arena");
    b.enemy(E_GLOOMLING, V3(-5, 18.2f, 150), ar); b.enemy(E_GLOOMLING, V3(5, 18.2f, 150), ar); b.enemy(E_GLOOMLING, V3(0, 18.2f, 156), ar);
    b.enemy(E_MOTH, V3(-6, 18, 158), ar); b.enemy(E_MOTH, V3(6, 18, 158), ar);
    b.enemy(E_TARBELL, V3(10, 18, 160), ar);
    b.spinner(V3(-14.8f, 22, 150), 4, 0.6f, 0, 0.5f); b.spinner(V3(14.8f, 22, 150), 4, 0.6f, 0, -0.5f);
    b.pickup(PK_CANDLE, V3(12.2f, 18.7f, 163.5f), 5, "candle.5");
    b.tree(V3(-11, 18, 140), 0.9f); b.tree(V3(11, 18, 141), 0.8f);

    // --- hacked terminals on pillars across the void (GL1TCH locked the gate)
    b.solid(AABB(V3(-3, 12, 166), V3(3, 18, 200)), T_ROCK, T_GRASS);
    {
        std::vector<V3> tp; std::vector<float> ty;
        for (V3 p : {V3(-9, 22, 173), V3(9, 22, 173), V3(-9, 22, 182), V3(9, 22, 182)}) {
            b.pillar(p.x, p.z, 6, 22, 1.2f, T_STONEBRICK);
            tp.push_back(p); ty.push_back(p.x < 0 ? PI / 2 : -PI / 2);
        }
        addPuzzle(b, AABB(V3(-3, 17, 167), V3(3, 23, 186)), tp, ty, 4, 7, 1, "clock.g2", -1, V3());
    }
    b.gate(AABB(V3(-3, 18, 186), V3(3, 22.5f, 186.6f)), 1, "clock.g2");
    b.wall(-10, 186, -3, 186.6f, 18, 22.5f, T_STONEBRICK); b.wall(3, 186, 10, 186.6f, 18, 22.5f, T_STONEBRICK);
    b.hint(AABB(V3(-3, 17, 167), V3(3, 22, 186)), "GL1TCH hacked the gate! WATCH the order the terminals flash, then BURN them in that order [{THROW}].", "", "clock.g2");
    b.enemy(E_DRONE, V3(0, 18, 170));
    // GL1TCH's server room (optional): crack it for the FIREWALL power
    b.solid(AABB(V3(-15, 12, 186.6f), V3(-3, 18, 198.5f)), T_TILEFLOOR, T_TILEFLOOR);
    b.wall(-15.6f, 186, -10, 186.6f, 18, 22.5f, T_STONEBRICK); b.wall(-15.6f, 186.6f, -15, 198.5f, 18, 22.5f, T_STONEBRICK);
    b.wall(-15.6f, 198.5f, -10, 199.1f, 18, 22.5f, T_STONEBRICK);
    {
        V3 c(-8, 18, 192.5f);
        std::vector<V3> tp; std::vector<float> ty;
        for (int k = 0; k < 5; k++) {
            float a = (120 + k * 30) * PI / 180;
            V3 p = c + V3(std::cos(a) * 5, 0, std::sin(a) * 5);
            tp.push_back(p); ty.push_back(std::atan2(c.x - p.x, c.z - p.z));
        }
        addPuzzle(b, AABB(V3(-15, 17, 187), V3(-3.2f, 23, 198)), tp, ty, 5, 21, -1, "clock.servers", 1, c + V3(0, 0.9f, 0));
        for (int k = 0; k < 4; k++) {  // racks of humming servers along the walls
            V3 r(-14.4f, 18, 188.5f + k * 2.8f);
            b.solid(AABB(r + V3(-0.5f, 0, -1), r + V3(0.5f, 3.6f, 1)), T_METAL, T_METAL);
            L.decor.emis = 1.3f; L.decor.color = hexColor(0x60ff90);
            L.decor.cube(M4::translate(r + V3(0.52f, 2.0f, 0)) * M4::scale(V3(0.04f, 2.6f, 1.6f)), T_CODE);
            L.decor.emis = 0; L.decor.color = V3(1, 1, 1);
        }
        b.light(c + V3(0, 3, 0), 10, hexColor(0x40ff80), 0.9f);
        b.emitter(AABB(V3(-15, 18, 187), V3(-3, 23, 198)), 5, 10);
        b.hint(AABB(V3(-15, 17, 187), V3(-3.2f, 23, 198)), "GL1TCH's server room. Crack all five terminals in order - wrong moves call his FIREWALL DRONES.", "", "clock.servers");
    }
    b.lantern(V3(2, 18, 190), b.spawn(V3(0.5f, 18, 191), 0));
    // sealed alcove with a shard (needs Bloom)
    b.solid(AABB(V3(3, 12, 191), V3(10, 18, 197)), T_ROCK, T_GRASS);
    b.wall(3, 196.6f, 10, 197, 18, 21, T_STONEBRICK); b.wall(9.6f, 191, 10, 197, 18, 21, T_STONEBRICK); b.wall(3.6f, 191, 10, 191.4f, 18, 21, T_STONEBRICK);
    L.decor.box(AABB(V3(3.4f, 21, 190.8f), V3(10.2f, 21.4f, 197.2f)), T_STONEBRICK, 1);
    b.seal(AABB(V3(3.1f, 18, 191.4f), V3(3.5f, 21, 196.6f)), "clock.seal");
    b.pickup(PK_SHARD, V3(7, 18.9f, 194), 1, "shard.clock1");

    // --- the Warden's arena
    b.solid(AABB(V3(-18, 12, 200), V3(18, 18, 236)), T_TILEFLOOR, T_TILEFLOOR);
    int bg = b.gate(AABB(V3(-3, 18, 198.5f), V3(3, 22.5f, 199.1f)), -1, "", T_GRATE);
    b.wall(-10, 198.5f, -3, 199.1f, 18, 22.5f, T_STONEBRICK); b.wall(3, 198.5f, 10, 199.1f, 18, 22.5f, T_STONEBRICK);
    V3 C(0, 18, 218);
    L.boss.type = B_WARDEN; L.boss.center = C; L.boss.radius = 18;
    L.boss.trigger = AABB(V3(-15, 17, 203), V3(15, 24, 233)); L.boss.gates = {bg}; L.boss.flag = "boss.1";
        b.spinner(C + V3(0, 19, 3), 4, 0.8f, 2, -0.2f);
    b.arch(V3(0, 18, 237), 5, 5, 1, T_STONEBRICK, true);
    b.solid(AABB(V3(-4, 12, 236), V3(4, 18, 242)), T_TILEFLOOR, T_TILEFLOOR);
    b.exit(AABB(V3(-2.5f, 18, 238), V3(2.5f, 22, 242)), LV_HUB, 2, "heart.1", "Take the Beacon-Heart first - it holds your next power.", "Return to Lumen Hollow");
    b.hint(AABB(V3(-18, 17, 200), V3(18, 26, 236)), "BURN the 3 glowing SERVERS to crash GL1TCH's firewall, then BURN GL1TCH!", "", "boss.1");

    b.emitter(AABB(V3(-15, 1, 0), V3(15, 12, 200)), 0, 14);
    // GL1TCH's hack has crept into the wood: screens of scrolling code bolted to rocks and trunks, and falling data
    struct Scr { V3 p; float yaw, w, h; } scr[] = {{V3(-4.5f, 2.6f, 35.9f), 0, 3, 2}, {V3(5.5f, 3, 71.9f), 0, 2.5f, 2}, {V3(-6, 9, 103.9f), 0, 2.2f, 1.6f}, {V3(4.6f, 12, 112), PI / 2, 2.4f, 2},
                                                   {V3(-13.9f, 20.5f, 145), PI / 2, 3, 2}, {V3(13.9f, 20.5f, 155), PI / 2, 3, 2}, {V3(-17.9f, 21.5f, 210), PI / 2, 4, 2.5f}, {V3(17.9f, 21.5f, 226), PI / 2, 4, 2.5f}};
    for (auto& sc : scr) {
        M4 m = M4::translate(sc.p) * M4::rotY(sc.yaw);
        L.decor.cube(m * M4::scale(V3(sc.w + 0.3f, sc.h + 0.3f, 0.12f)), T_METAL);
        L.decor.emis = 1.4f; L.decor.color = hexColor(0x60ff90);
        L.decor.cube(m * M4::translate(V3(0, 0, -0.05f)) * M4::scale(V3(sc.w, sc.h, 0.06f)), T_CODE, 2);
        L.decor.cube(m * M4::translate(V3(0, 0, 0.05f)) * M4::scale(V3(sc.w, sc.h, 0.06f)), T_CODE, 2);
        L.decor.emis = 0; L.decor.color = V3(1, 1, 1);
        b.light(sc.p, 5, hexColor(0x40ff80), 0.6f);
    }
    b.emitter(AABB(V3(-18, 18, 200), V3(18, 34, 236)), 5, 14);
    b.emitter(AABB(V3(-10, 18, 166), V3(10, 30, 200)), 5, 6);
    b.enemy(E_VIRUS, V3(-6, 18.2f, 143)); b.enemy(E_VIRUS, V3(6, 18.2f, 143));
    // a hidden branch off the crown (secret)
    b.solid(AABB(V3(-13, 25.5f, 104), V3(-9, 26, 108)), T_PLANK, T_PLANK);
    b.anchor(V3(-10.5f, 27.4f, 106.5f));
    b.pickup(PK_HAT, V3(-11.6f, 26.9f, 105.4f), 6, "hat.6");
    b.urns({V3(5, 0, -4), V3(-5, 0, 10), V3(-6, 0, 30), V3(-5, 6, 45), V3(5, 6, 52), V3(-6, 6, 80), V3(7, 6, 88), V3(-8, 6, 101), V3(-11.5f, 18, 115), V3(-11.5f, 18, 125), V3(-10, 18, 140), V3(10, 18, 150), V3(2, 18, 172), V3(-2, 18, 195)}, 1);
    b.enemy(E_GLOOMLING, V3(0, 6.2f, 46)); b.enemy(E_GLOOMLING, V3(3, 6.2f, 50)); b.enemy(E_GLOOMLING, V3(-6, 6.2f, 98)); b.enemy(E_MOTH, V3(-11, 18, 122));
    b.emitter(AABB(V3(-20, -10, 0), V3(20, -2, 240)), 2, 4);
}

// ================================================================== ASHEN VAULTS
void buildVaults(Level& L, Game&) {
    LB b(L);
    L.name = "The Ashen Vaults";
    L.song = SONG_VAULTS;
    L.env.skyTop = hexColor(0x080406); L.env.skyHorizon = hexColor(0x1a0a10);
    L.env.fogColor = hexColor(0x140810); L.env.fogDensity = 0.04f; L.env.fogStart = 5;
    L.env.ambSky = V3(0.1f, 0.06f, 0.08f); L.env.ambGround = V3(0.06f, 0.03f, 0.04f);
    L.env.sunColor = V3(0.03f, 0.02f, 0.03f); L.env.stars = 0; L.env.moonSize = 0;
    L.killY = -6;
    auto ceil = [&](float x0, float z0, float x1, float z1, float y) { b.solid(AABB(V3(x0, y, z0), V3(x1, y + 1, z1)), T_STONEBRICK); };

    // --- entrance stair hall (bloom tutorial)
    b.slab(-4, -8, 4, 30, 0, T_TILEFLOOR, T_STONEBRICK, 3);
    b.wall(-5, -8, -4, 30, 0, 8, T_STONEBRICK); b.wall(4, -8, 5, 6, 0, 8, T_STONEBRICK); b.wall(4, 12, 5, 30, 0, 8, T_STONEBRICK);
    b.wall(4, 6, 5, 7, 0, 8, T_STONEBRICK); b.wall(4, 11, 5, 12, 0, 8, T_STONEBRICK); b.wall(4, 7, 5, 11, 4, 8, T_STONEBRICK);
    ceil(-5, -8, 5, 30, 8);
    b.spawn(V3(0, 0, 0), 0);
    b.exit(AABB(V3(-3, -1, -9), V3(3, 3, -6.5f)), LV_HUB, 3, "", "", "Back to Lumen Hollow");
    b.talker(V3(-3, 0, 4), PI / 2, 6);  // Mort the gravekeeper
    b.brazier(V3(0, 0, 13), 0, true);
    b.gate(AABB(V3(-4, 0, 20), V3(4, 5, 20.6f)), 0, "vaults.g1");
    b.hint(AABB(V3(-4, -1, 4), V3(4, 4, 20)), "This brazier is sealed in wax. Get close and BLOOM [{BLOOM}] - or throw your flame beside it and bloom from afar.", "", "vaults.g1");
    for (int i = 0; i < 4; i++) { b.candles(V3(i % 2 ? 3.3f : -3.3f, 0, 2 + i * 6), 3); }
    // side room behind a wax seal (memory candle)
    b.slab(5, 5, 11, 13, 0, T_TILEFLOOR, T_STONEBRICK, 3);
    b.wall(11, 5, 11.6f, 13, 0, 4, T_STONEBRICK); b.wall(5, 4.4f, 11.6f, 5, 0, 4, T_STONEBRICK); b.wall(5, 13, 11.6f, 13.6f, 0, 4, T_STONEBRICK);
    ceil(5, 4.4f, 11.6f, 13.6f, 4);
    b.seal(AABB(V3(4.1f, 0, 7), V3(4.9f, 4, 11)), "vaults.seal1");
    b.pickup(PK_CANDLE, V3(9, 0.8f, 9), 6, "candle.6");
    b.bones(V3(7, 0, 12)); b.tomb(V3(10.3f, 0, 7), -PI / 2);

    // --- the knight hall
    b.slab(-10, 30, 10, 70, 0, T_TILEFLOOR, T_STONEBRICK, 3);
    b.wall(-11, 30, -10, 44.5f, 0, 10, T_STONEBRICK); b.wall(-11, 48.5f, -10, 70, 0, 10, T_STONEBRICK); b.wall(10, 30, 11, 70, 0, 10, T_STONEBRICK);
    b.wall(-11, 44.5f, -10, 48.5f, 4, 10, T_STONEBRICK);
    b.seal(AABB(V3(-10.9f, 0, 44.5f), V3(-10.1f, 4, 48.5f)), "vaults.hatseal");
    b.slab(-16, 43.9f, -11, 49.1f, 0, T_TILEFLOOR, T_STONEBRICK, 3);
    b.wall(-16.6f, 43.9f, -16, 49.1f, 0, 4, T_STONEBRICK); b.wall(-16.6f, 43.3f, -11, 43.9f, 0, 4, T_STONEBRICK); b.wall(-16.6f, 49.1f, -11, 49.7f, 0, 4, T_STONEBRICK);
    ceil(-16.6f, 43.3f, -10, 49.7f, 4);
    b.pickup(PK_HAT, V3(-13.5f, 0.9f, 46.5f), 7, "hat.7");
    b.candles(V3(-15.3f, 0, 44.6f), 3); b.bones(V3(-12, 0, 48.4f));
    b.wall(-10, 29.4f, -4, 30, 0, 10, T_STONEBRICK); b.wall(4, 29.4f, 10, 30, 0, 10, T_STONEBRICK);
    ceil(-11, 29.4f, 11, 70, 10);
    for (int z = 38; z <= 62; z += 8) { b.pillar(-6, (float)z, 0, 10, 0.8f, T_MARBLE); b.pillar(6, (float)z, 0, 10, 0.8f, T_MARBLE); }
    b.lantern(V3(-8, 0, 33), b.spawn(V3(-6.5f, 0, 34), 0));
    b.enemy(E_KNIGHT, V3(0, 0, 47)); b.enemy(E_KNIGHT, V3(0, 0, 61));
    b.hint(AABB(V3(-10, -1, 30), V3(10, 4, 40)), "Hushknights block with their shields. Throw PAST them and RECALL [{THROW}] the flame into their backs.");
    for (int z = 34; z <= 66; z += 8) { b.banner(V3(-9.9f, 8, (float)z), 1.6f, 3.5f, PI / 2, T_CLOTH); b.banner(V3(9.9f, 8, (float)z), 1.6f, 3.5f, -PI / 2, T_CLOTH); }
    b.light(V3(0, 7, 50), 14, hexColor(0xff6040), 0.5f);
    b.wall(-10, 70, -4, 70.6f, 0, 10, T_STONEBRICK); b.wall(4, 70, 10, 70.6f, 0, 10, T_STONEBRICK);

    // --- lava catacomb with moths
    auto lavaStrip = [&](float z0, float z1) {
        L.decor.emis = 1.0f;
        L.decor.box(AABB(V3(-6, -3.4f, z0), V3(6, -3, z1)), T_LAVA, 1);
        L.decor.emis = 0;
        b.light(V3(0, -1.5f, (z0 + z1) / 2), 9, hexColor(0xff5020), 1.2f);
        b.emitter(AABB(V3(-5, -3, z0), V3(5, 0, z1)), 1, 6);
    };
    b.slab(-6, 70, 6, 80, 0, T_ASH, T_STONEBRICK, 3); lavaStrip(80, 83);
    b.slab(-6, 83, 6, 92, 0, T_ASH, T_STONEBRICK, 3); lavaStrip(92, 95);
    b.slab(-6, 95, 6, 110, 0, T_ASH, T_STONEBRICK, 3);
    b.wall(-7, 70, -6, 110, -4, 8, T_STONEBRICK); b.wall(6, 70, 7, 94, -4, 8, T_STONEBRICK); b.wall(6, 101, 7, 110, -4, 8, T_STONEBRICK);
    ceil(-7, 70, 7, 110, 8);
    b.enemy(E_MOTH, V3(-3, 0, 86)); b.enemy(E_MOTH, V3(3, 0, 88)); b.enemy(E_MOTH, V3(0, 0, 98)); b.enemy(E_MOTH, V3(-2, 0, 102)); b.enemy(E_MOTH, V3(2, 0, 104));
    b.hint(AABB(V3(-6, -1, 70), V3(6, 4, 80)), "A swarm ahead. One BLOOM [{BLOOM}] burns a whole cloud of moths.");
    // ledge with a memory candle (warp)
    b.slab(6, 94, 11, 101, 3, T_ASH, T_STONEBRICK, 1);
    b.wall(11, 94, 11.6f, 101, 0, 8, T_STONEBRICK); b.wall(6, 93.4f, 11.6f, 94, 3, 8, T_STONEBRICK); b.wall(6, 101, 11.6f, 101.6f, 3, 8, T_STONEBRICK);
    b.anchor(V3(8.5f, 4.4f, 97.5f));
    b.pickup(PK_CANDLE, V3(9.5f, 3.8f, 99.5f), 7, "candle.7");
    b.bones(V3(9.5f, 3, 96));

    // --- the ossuary (ambush arena)
    b.slab(-14, 110, 14, 146.6f, 0, T_TILEFLOOR, T_STONEBRICK, 3);
    b.wall(-15, 110, -14, 146.6f, 0, 10, T_STONEBRICK); b.wall(14, 110, 15, 146.6f, 0, 10, T_STONEBRICK);
    b.wall(-14, 110, -3, 110.6f, 0, 10, T_STONEBRICK); b.wall(3, 110, 14, 110.6f, 0, 10, T_STONEBRICK);
    b.wall(-14, 146, -3, 146.6f, 0, 10, T_STONEBRICK); b.wall(3, 146, 14, 146.6f, 0, 10, T_STONEBRICK);
    ceil(-15, 110, 15, 146.6f, 10);
    int ga = b.gate(AABB(V3(-3, 0, 110), V3(3, 5, 110.6f)), -1, "", T_GRATE);
    int gb = b.gate(AABB(V3(-3, 0, 146), V3(3, 5, 146.6f)), -1, "", T_GRATE);
    int ar = b.arena(AABB(V3(-13, -1, 113), V3(13, 6, 144)), {ga, gb}, "vaults.arena");
    b.enemy(E_KNIGHT, V3(-5, 0, 130), ar); b.enemy(E_KNIGHT, V3(5, 0, 136), ar);
    b.enemy(E_MOTH, V3(-6, 0, 140), ar); b.enemy(E_MOTH, V3(6, 0, 140), ar); b.enemy(E_MOTH, V3(0, 0, 142), ar);
    b.enemy(E_GLOOMLING, V3(-8, 0.2f, 122), ar); b.enemy(E_GLOOMLING, V3(8, 0.2f, 122), ar);
    for (int i = 0; i < 6; i++) b.tomb(V3(i < 3 ? -12.3f : 12.3f, 0, 116 + (i % 3) * 9.f), i < 3 ? PI / 2 : -PI / 2);
    b.bones(V3(-4, 0, 118)); b.bones(V3(6, 0, 128)); b.bones(V3(-7, 0, 140));
    // high gallery with a shard (warp)
    b.solid(AABB(V3(-14, 4.5f, 126), V3(-10, 5, 134)), T_STONEBRICK, T_TILEFLOOR);
    b.anchor(V3(-11.5f, 6.4f, 130));
    b.pickup(PK_SHARD, V3(-13, 5.8f, 130), 0, "shard.vaults0");
    b.light(V3(0, 8, 128), 16, hexColor(0x8040ff), 0.4f);

    // --- sealed-wick bridge
    b.slab(-8, 146.6f, 8, 176, 0, T_TILEFLOOR, T_STONEBRICK, 3);
    b.wall(-9, 146.6f, -8, 176, 0, 9, T_STONEBRICK);
    ceil(-9, 146.6f, 9, 176, 9);
    b.brazier(V3(-5.5f, 0, 154), 1, true); b.brazier(V3(-5.5f, 0, 159), 1, true); b.brazier(V3(5.5f, 0, 166), 1, true);
    b.gate(AABB(V3(-8, 0, 169.5f), V3(8, 5, 170.1f)), 1, "vaults.g2");
    b.hint(AABB(V3(-8, -1, 147), V3(8, 4, 169)), "Three sealed wicks. A single Bloom between two of them lights both.", "", "vaults.g2");
    // crystal-locked alcove (needs Sunflare)
    b.slab(8, 150, 13, 156, 0, T_TILEFLOOR, T_STONEBRICK, 3);
    b.wall(13, 150, 13.6f, 156, 0, 5, T_STONEBRICK); b.wall(8, 149.4f, 13.6f, 150, 0, 5, T_STONEBRICK); b.wall(8, 156, 13.6f, 156.6f, 0, 5, T_STONEBRICK);
    ceil(8, 149.4f, 13.6f, 156.6f, 5);
    b.wall(8, 146.6f, 9, 150, 0, 9, T_STONEBRICK); b.wall(8, 156, 9, 176, 0, 9, T_STONEBRICK); b.wall(8, 150, 9, 156, 4, 9, T_STONEBRICK);
    b.crystal(AABB(V3(8.1f, 0, 150), V3(8.9f, 4, 156)), "vaults.crystal");
    b.pickup(PK_SHARD, V3(11.5f, 0.9f, 153), 1, "shard.vaults1");
    b.crystalCluster(V3(12, 0, 151), 0.6f, hexColor(0x80e0ff));
    b.lantern(V3(-5, 0, 173), b.spawn(V3(-3.5f, 0, 173.5f), 0));
    b.wall(-8, 176, -3, 176.6f, 0, 9, T_STONEBRICK); b.wall(3, 176, 8, 176.6f, 0, 9, T_STONEBRICK);

    // --- the Matron's crypt
    b.slab(-16, 176, 16, 212, 0, T_TILEFLOOR, T_STONEBRICK, 3);
    b.wall(-17, 176.6f, -16, 212, 0, 14, T_STONEBRICK); b.wall(16, 176.6f, 17, 212, 0, 14, T_STONEBRICK);
    b.wall(-16, 176.6f, -3, 177.2f, 0, 14, T_STONEBRICK); b.wall(3, 176.6f, 16, 177.2f, 0, 14, T_STONEBRICK);
    b.wall(-16, 212, -3, 212.6f, 0, 14, T_STONEBRICK); b.wall(3, 212, 16, 212.6f, 0, 14, T_STONEBRICK);
    b.wall(-3, 212, 3, 212.6f, 5, 14, T_STONEBRICK);
    ceil(-17, 176, 17, 212.6f, 14);
    int bg = b.gate(AABB(V3(-3, 0, 176.6f), V3(3, 5, 177.2f)), -1, "", T_GRATE);
    V3 C(0, 0, 194);
    b.brazier(C + V3(-8, 0, -2.5f), -1, false, 0, true); b.brazier(C + V3(-8, 0, 2.5f), -1, false, 0, true);
    b.brazier(C + V3(8, 0, -2.5f), -1, false, 0, true); b.brazier(C + V3(8, 0, 2.5f), -1, false, 0, true);
    L.boss.type = B_MATRON; L.boss.center = C; L.boss.radius = 15;
    L.boss.trigger = AABB(V3(-14, -1, 180), V3(14, 8, 209)); L.boss.gates = {bg}; L.boss.flag = "boss.2";
    for (int i = 0; i < 6; i++) { float a = i * TAU / 6 + PI / 6; b.pillar(C.x + std::sin(a) * 13.5f, C.z + std::cos(a) * 13.5f, 0, 14, 0.9f, T_MARBLE); }
    b.hint(AABB(V3(-15, -1, 178), V3(15, 8, 210)), "She shuns the dark... and cannot resist a blaze. Light all four braziers!", "", "boss.2");
    b.slab(-3, 212, 3, 218, 0, T_TILEFLOOR, T_STONEBRICK, 3);
    b.wall(-4, 212.6f, -3, 218, 0, 6, T_STONEBRICK); b.wall(3, 212.6f, 4, 218, 0, 6, T_STONEBRICK);
    b.exit(AABB(V3(-2.5f, 0, 213.5f), V3(2.5f, 4, 218)), LV_HUB, 3, "heart.2", "Take the Beacon-Heart first - it holds your next power.", "Return to Lumen Hollow");
    b.emitter(AABB(V3(-14, 0, 0), V3(14, 9, 210)), 3, 10);
    b.urns({V3(3, 0, 3), V3(-3, 0, 15), V3(-8, 0, 40), V3(8, 0, 52), V3(-8, 0, 66), V3(3, 0, 75), V3(-4, 0, 100), V3(10, 0, 120), V3(-10, 0, 140), V3(6, 0, 149), V3(-6, 0, 168), V3(10, 0, 210), V3(-10, 0, 210)}, 0);
    b.enemy(E_GLOOMLING, V3(2, 0.2f, 76)); b.enemy(E_GLOOMLING, V3(-3, 0.2f, 104)); b.enemy(E_GLOOMLING, V3(3, 0.2f, 106));
}

// ================================================================== THE LIGHTHOUSE SPIRE
void buildSpire(Level& L, Game&) {
    LB b(L);
    L.name = "The Lighthouse Spire";
    L.song = SONG_SPIRE;
    L.env.skyTop = hexColor(0x05031a); L.env.skyHorizon = hexColor(0x2a1a4a);
    L.env.fogColor = hexColor(0x150f2a); L.env.fogDensity = 0.022f;
    L.env.ambSky = V3(0.15f, 0.13f, 0.28f); L.env.ambGround = V3(0.06f, 0.05f, 0.1f);
    L.env.sunColor = V3(0.25f, 0.22f, 0.45f); L.env.sunDir = norm(V3(0.4f, -1, -0.3f));
    L.env.stars = 1.4f; L.env.moonSize = 0.09f; L.env.moonDir = norm(V3(0.2f, 0.5f, 1));
    L.killY = -14;

    // --- ground hall (wraiths)
    b.slab(-10, -8, 10, 14, 0, T_MARBLE, T_STONEBRICK, 3);
    b.wall(-11, -8, -10, 14, 0, 10, T_STONEBRICK); b.wall(10, -8, 11, 14, 0, 10, T_STONEBRICK);
    b.wall(-10, -8.6f, 10, -8, 0, 10, T_STONEBRICK);
    b.spawn(V3(0, 0, -4), 0);
    b.exit(AABB(V3(-2.5f, 0, -8), V3(2.5f, 3, -6.5f)), LV_HUB, 4, "", "", "Back to Lumen Hollow");
    b.enemy(E_WRAITH, V3(-4, 0, 8)); b.enemy(E_WRAITH, V3(5, 0, 10));
    b.hint(AABB(V3(-10, -1, -6), V3(10, 4, 4)), "Something moves in the dark. Wraiths can't be burned unseen - let your flame HOVER near them.");
    b.candles(V3(-8, 0, -6), 4); b.candles(V3(8, 0, -6), 4);
    b.statue(V3(-8, 0, 12), 1.0f, PI); b.statue(V3(8, 0, 12), 1.0f, PI);
    b.stairs(V3(0, 0, 14), V3(0, 0, 1), 6, 12, 0.5f, 1.0f, T_MARBLE);
    b.wall(-10, 14, -3.6f, 14.6f, 0, 10, T_STONEBRICK); b.wall(3.6f, 14, 10, 14.6f, 0, 10, T_STONEBRICK);
    b.wall(-3.6f, 14, -3, 26, 0, 7, T_STONEBRICK); b.wall(3, 14, 3.6f, 26, 0, 7, T_STONEBRICK);

    // --- first landing (knights, crystal)
    b.slab(-10, 26, 10, 50, 6, T_MARBLE, T_STONEBRICK, 3);
    b.lantern(V3(8, 6, 28), b.spawn(V3(6.5f, 6, 29), 0));
    b.enemy(E_KNIGHT, V3(0, 6, 38));
    b.hint(AABB(V3(-10, 5, 26), V3(10, 9, 34)), "A SUNFLARE shatters shields. Hold [{THROW}] until your flame flashes white.");
    b.wall(-10, 44, -3, 44.6f, 6, 12, T_STONEBRICK); b.wall(3, 44, 10, 44.6f, 6, 12, T_STONEBRICK);
    b.crystal(AABB(V3(-3, 6, 44), V3(3, 11, 44.6f)), "spire.c1");
    b.wall(-3, 44, 3, 44.6f, 11, 12, T_STONEBRICK);
    b.hint(AABB(V3(-10, 5, 38), V3(10, 9, 44)), "Crystal. Only a Sunflare [hold {THROW}] can shatter it.", "", "spire.c1");
    for (int i = 0; i < 4; i++) b.crystalCluster(V3(i < 2 ? -8.5f : 8.5f, 6, 32 + (i % 2) * 8.f), 0.7f, hexColor(0x9a7aff));

    // --- outer climb (hooks over the void)
    b.solid(AABB(V3(-3, 9.5f, 52), V3(3, 10, 57)), T_MARBLE, T_MARBLE);
    b.anchor(V3(0, 12.2f, 54.5f));
    b.solid(AABB(V3(6, 13.5f, 52), V3(10, 14, 58)), T_MARBLE, T_MARBLE);
    b.anchor(V3(8, 16.2f, 55));
    b.solid(AABB(V3(6, 17.5f, 62), V3(10, 18, 68)), T_MARBLE, T_MARBLE);
    b.anchor(V3(8, 20.2f, 65));
    b.solid(AABB(V3(-3, 21.5f, 64), V3(3, 22, 70)), T_MARBLE, T_MARBLE);
    b.anchor(V3(0, 24.2f, 67));
    b.anchor(V3(0, 28.2f, 74.5f));
    b.solid(AABB(V3(-8, 17.5f, 55), V3(-4.5f, 18, 59)), T_MARBLE, T_MARBLE);  // a lonely ledge (secret)
    b.anchor(V3(-6.2f, 19.8f, 57));
    b.pickup(PK_HAT, V3(-7.2f, 18.9f, 57.6f), 9, "hat.9");
    b.enemy(E_MOTH, V3(4, 14, 58)); b.enemy(E_MOTH, V3(4, 18, 64)); b.enemy(E_MOTH, V3(-3, 22, 70));
    b.hint(AABB(V3(-10, 5, 44), V3(10, 9, 50)), "Up the outside of the Spire, hook by hook. Don't look down.");
    L.decor.cylinder(M4::translate(V3(-14, 30, 60)) * M4::scale(V3(10, 80, 10)), T_STONEBRICK, 12, true, 20);  // tower core beside the climb

    // --- the bell gallery (ambush arena)
    b.slab(-12, 72, 12, 100.6f, 26, T_MARBLE, T_STONEBRICK, 3);
    b.wall(-12.6f, 72, -12, 100.6f, 26, 30, T_STONEBRICK); b.wall(12, 72, 12.6f, 100.6f, 26, 30, T_STONEBRICK);
    b.wall(-12, 100, -3, 100.6f, 26, 32, T_STONEBRICK); b.wall(3, 100, 12, 100.6f, 26, 32, T_STONEBRICK);
    int gb = b.gate(AABB(V3(-3, 26, 100), V3(3, 31, 100.6f)), -1, "", T_GRATE);
    int ar = b.arena(AABB(V3(-11, 25, 78), V3(11, 31, 99)), {gb}, "spire.arena");
    b.enemy(E_KNIGHT, V3(-5, 26, 92), ar); b.enemy(E_KNIGHT, V3(5, 26, 92), ar);
    b.enemy(E_WRAITH, V3(0, 26, 96), ar); b.enemy(E_WRAITH, V3(-8, 26, 86), ar);
    b.enemy(E_MOTH, V3(6, 26, 84), ar); b.enemy(E_MOTH, V3(-6, 26, 84), ar);
    b.spinner(V3(0, 33, 86), 3, 0.5f, 1, 0.4f);
    L.decor.cone(M4::translate(V3(0, 31, 86)) * M4::scale(V3(5, 4, 5)), T_BRASS, 12);
    b.light(V3(0, 30, 86), 10, hexColor(0xa080ff), 0.6f);

    // --- the three sealed wicks
    b.slab(-8, 100.6f, 8, 126, 26, T_MARBLE, T_STONEBRICK, 3);
    b.wall(-8.6f, 100.6f, -8, 126, 26, 31, T_STONEBRICK); b.wall(8, 100.6f, 8.6f, 126, 26, 31, T_STONEBRICK);
    b.brazier(V3(-5.5f, 26, 105.5f), 2, true, 7); b.brazier(V3(-5.5f, 26, 110.5f), 2, true, 7);
    b.brazier(V3(5.5f, 26, 108), 2, true, 7);
    b.gate(AABB(V3(-8, 26, 117), V3(8, 31, 117.6f)), 2, "spire.g2");
    b.hint(AABB(V3(-8, 25, 101), V3(8, 30, 116)), "Timed and sealed. Throw your flame to the far wick and BLOOM [{BLOOM}] it remotely.", "", "spire.g2");
    b.lantern(V3(5, 26, 121), b.spawn(V3(3.5f, 26, 122), 0));
    b.pickup(PK_CANDLE, V3(-6.5f, 26.7f, 124), 8, "candle.8");

    // --- the lift to the summit
    b.mover(AABB(V3(-2, 25.5f, 126), V3(2, 26, 130)), V3(0, 0, 0), V3(0, 34, 0), 16, 0, T_BRASS, T_METAL);
    b.spinner(V3(-3, 28, 128), 1.5f, 0.3f, 0, 1);
    b.hint(AABB(V3(-4, 25, 122), V3(4, 30, 130)), "The old lamp-lift still runs. Ride it to the summit.");
    for (int i = 0; i < 8; i++) b.chain(V3(i < 4 ? -2.1f : 2.1f, 62, 126.2f + (i % 4) * 1.2f), 4);
    b.slab(-4, 130, 4, 140, 60, T_MARBLE, T_STONEBRICK, 2);

    // --- the summit and the dark Great Lamp
    b.slab(-18, 140, 18, 176, 60, T_MARBLE, T_STONEBRICK, 3);
    int bg = b.gate(AABB(V3(-4, 60, 139), V3(4, 65, 139.6f)), -1, "", T_GRATE);
    V3 C(0, 60, 158);
    b.solid(AABB(C + V3(-2.5f, 0, -2.5f), C + V3(2.5f, 1.2f, 2.5f)), T_BRASS, T_METAL);
    for (int i = 0; i < 3; i++) {
        float a = PI + i * TAU / 3;
        b.brazier(C + V3(std::sin(a) * 7, 0, std::cos(a) * 7), -1, true, 0, true);
    }
    for (int i = 0; i < 10; i++) {
        float a = i * TAU / 10;
        b.post(C + V3(std::sin(a) * 17.5f, 0, std::cos(a) * 17.5f), 1.4f, 0.2f, T_METAL);
    }
    L.boss.type = B_HUSH; L.boss.center = C; L.boss.radius = 17;
    L.boss.trigger = AABB(V3(-14, 59, 144), V3(14, 68, 172)); L.boss.gates = {bg}; L.boss.flag = "boss.3";
    b.emitter(AABB(V3(-20, 0, -10), V3(20, 70, 180)), 4, 10);
    b.urns({V3(-8, 0, 0), V3(8, 0, 6), V3(-8, 6, 30), V3(8, 6, 42), V3(-6, 26, 103), V3(6, 26, 115), V3(-10, 26, 75), V3(10, 26, 75)}, 1);
    b.enemy(E_GLOOMLING, V3(-5, 6.2f, 34)); b.enemy(E_GLOOMLING, V3(5, 6.2f, 34));
}

// ================================================================== JUNKYARD OF LOST MACHINES
static void junkPile(LB& b, V3 c, float s, uint32_t seed) {
    Rng r(seed);
    for (int i = 0; i < 6; i++) {
        V3 o(r.range(-1.2f, 1.2f) * s, r.range(0, 1.2f) * s, r.range(-1.2f, 1.2f) * s);
        int kind = r.irange(0, 3);
        M4 m = M4::translate(c + o) * M4::rotY(r.range(0, TAU)) * M4::rotZ(r.range(-0.5f, 0.5f));
        if (kind == 0) b.L.decor.cube(m * M4::scale(V3(1.4f, 0.9f, 1.0f) * s), T_RUST);
        else if (kind == 1) b.L.decor.cylinder(m * M4::rotX(PI / 2) * M4::scale(V3(1.0f, 0.4f, 1.0f) * s), T_TAR, 10);   // tyre
        else if (kind == 2) b.L.decor.cube(m * M4::scale(V3(1.6f, 0.2f, 1.2f) * s), T_METAL);
        else { b.L.decor.cube(m * M4::scale(V3(0.9f, 0.7f, 0.7f) * s), T_METAL); b.L.decor.emis = 1.2f; b.L.decor.color = r.f() < 0.5f ? hexColor(0x40ffd0) : hexColor(0xff60c0);
               b.L.decor.cube(m * M4::translate(V3(0, 0, 0.36f * s)) * M4::scale(V3(0.7f, 0.5f, 0.02f) * s), T_WHITE); b.L.decor.emis = 0; b.L.decor.color = V3(1, 1, 1); }  // old TV
    }
}
static void car(LB& b, V3 c, float yaw, V3 col) {
    M4 m = M4::translate(c) * M4::rotY(yaw);
    b.L.decor.color = col;
    b.L.decor.cube(m * M4::translate(V3(0, 0.7f, 0)) * M4::scale(V3(2.0f, 0.8f, 4.2f)), T_RUST);
    b.L.decor.cube(m * M4::translate(V3(0, 1.4f, -0.3f)) * M4::scale(V3(1.8f, 0.7f, 2.2f)), T_RUST);
    b.L.decor.color = V3(1, 1, 1);
    for (int i = 0; i < 4; i++) b.L.decor.cylinder(m * M4::translate(V3(i & 1 ? 1.0f : -1.0f, 0.4f, i & 2 ? 1.4f : -1.4f)) * M4::rotZ(PI / 2) * M4::scale(V3(0.8f, 0.3f, 0.8f)), T_TAR, 8);
    b.solid(AABB(c + V3(-1.6f, 0, -1.6f), c + V3(1.6f, 1.8f, 1.6f)), T_RUST, -1, V3(1, 1, 1), false);
}
static void neon(LB& b, V3 p, V3 col) {
    b.L.decor.emis = 2; b.L.decor.color = col;
    b.L.decor.cube(M4::translate(p) * M4::scale(V3(2.2f, 0.25f, 0.1f)), T_WHITE);
    b.L.decor.emis = 0; b.L.decor.color = V3(1, 1, 1);
    b.post(p - V3(0, p.y, 0), p.y - 0.1f, 0.1f, T_METAL);
    b.light(p, 7, col, 0.9f);
}

void buildJunk(Level& L, Game&) {
    LB b(L);
    L.name = "The Junkyard of Lost Machines";
    L.song = SONG_JUNK;
    L.env.skyTop = hexColor(0x100620); L.env.skyHorizon = hexColor(0x5a2050);
    L.env.fogColor = hexColor(0x241230); L.env.fogDensity = 0.022f;
    L.env.ambSky = V3(0.2f, 0.12f, 0.26f); L.env.ambGround = V3(0.08f, 0.05f, 0.1f);
    L.env.sunColor = V3(0.25f, 0.18f, 0.35f); L.env.moonColor = hexColor(0xff9ad8);
    L.waterY = -1.0f; L.waterArea = AABB(V3(-60, -1, -30), V3(60, -1, 230)); L.killY = -6;
    L.waterColor = hexColor(0x4ab02a);  // toxic sludge
    V3 pink = hexColor(0xff50c0), cyan = hexColor(0x40f0ff);

    // --- the gate street
    b.slab(-8, -8, 8, 40, 0, T_DIRT, T_RUST, 3);
    b.wall(-9, -8, -8, 40, 0, 3, T_RUST); b.wall(8, -8, 9, 40, 0, 3, T_RUST);
    b.spawn(V3(0, 0, -2), 0);
    b.exit(AABB(V3(-3, -1, -9), V3(3, 3, -6.5f)), LV_HUB, 5, "", "", "Back to Lumen Hollow");
    b.talker(V3(5, 0, 6), -PI / 2, 7);  // Rusty
    junkPile(b, V3(-6.5f, 0, 12), 1.1f, 1); junkPile(b, V3(6.5f, 0, 22), 1.2f, 2); junkPile(b, V3(-6.5f, 0, 32), 1.0f, 3);
    car(b, V3(-5, 0, 24), 0.3f, hexColor(0xc04040)); car(b, V3(5, 0, 34), -0.2f, hexColor(0x4080c0));
    neon(b, V3(-7.5f, 3.5f, 4), pink); neon(b, V3(7.5f, 3.5f, 16), cyan); neon(b, V3(-7.5f, 3.5f, 28), cyan);
    b.enemy(E_RAT, V3(-2, 0.2f, 18)); b.enemy(E_RAT, V3(2, 0.2f, 20)); b.enemy(E_RAT, V3(0, 0.2f, 28)); b.enemy(E_RAT, V3(-3, 0.2f, 34)); b.enemy(E_RAT, V3(3, 0.2f, 36));
    b.enemy(E_TARBELL, V3(6.5f, 0, 38));
    b.hint(AABB(V3(-8, -1, 8), V3(8, 4, 16)), "Rats! Small and fast - your flame burns a whole line of them.");

    // --- the sludge pit and its crane platforms
    b.slab(-10, 40, 10, 56, 0, T_DIRT, T_RUST, 3);
    b.wall(-11, 40, -10, 56, 0, 3, T_RUST); b.wall(10, 40, 11, 56, 0, 3, T_RUST);
    b.mover(AABB(V3(-1.5f, -0.6f, 58), V3(1.5f, 0, 61)), V3(-6, 0, 0), V3(6, 0, 0), 5.5f, 0, T_METAL, T_GRATE);
    b.mover(AABB(V3(-1.5f, -0.6f, 64), V3(1.5f, 0, 67)), V3(-6, 0, 0), V3(6, 0, 0), 5.5f, 0, T_METAL, T_GRATE);
    b.slab(-10, 70, 10, 84, 0, T_DIRT, T_RUST, 3);
    b.anchor(V3(0, 2.2f, 72), true);
    b.hint(AABB(V3(-10, -1, 46), V3(10, 4, 56)), "Ride the crane platforms over the sludge - or throw to the hook and WARP across.");
    b.lantern(V3(6, 0, 76), b.spawn(V3(4.5f, 0, 77), 0));
    junkPile(b, V3(-8, 0, 50), 1.3f, 4); junkPile(b, V3(8, 0, 80), 1.0f, 5);
    neon(b, V3(9.5f, 3.5f, 44), pink); neon(b, V3(-9.5f, 3.5f, 78), pink);
    b.enemy(E_MOTH, V3(-3, 0, 76)); b.enemy(E_MOTH, V3(3, 0, 80));
    b.pickup(PK_SHARD, V3(-8.5f, 0.9f, 82), 2, "shard.junk0");  // reward for exploring the corner
    b.solid(AABB(V3(7, 0, 71.5f), V3(9.6f, 4.6f, 74.5f)), T_RUST, T_METAL);  // scrap tower (secret on top)
    b.anchor(V3(8.3f, 6.6f, 73));
    b.pickup(PK_HAT, V3(8.3f, 5.5f, 73.6f), 8, "hat.8");

    // --- the crusher arena: MEGA-BOT 3000
    b.slab(-14, 84, 14, 120, 0, T_TILEFLOOR, T_RUST, 3);
    b.wall(-15, 84, -14, 120, 0, 4, T_RUST); b.wall(14, 84, 15, 120, 0, 4, T_RUST);
    b.wall(-14, 84, -3, 84.6f, 0, 4, T_RUST); b.wall(3, 84, 14, 84.6f, 0, 4, T_RUST);
    b.wall(-14, 119.4f, -3, 120, 0, 4, T_RUST); b.wall(3, 119.4f, 14, 120, 0, 4, T_RUST);
    int ga = b.gate(AABB(V3(-3, 0, 84), V3(3, 4.5f, 84.6f)), -1, "", T_GRATE);
    int gb = b.gate(AABB(V3(-3, 0, 119.4f), V3(3, 4.5f, 120)), -1, "", T_GRATE);
    int ar = b.arena(AABB(V3(-13, -1, 87), V3(13, 6, 117)), {ga, gb}, "junk.arena");
    b.enemy(E_MEGABOT, V3(0, 0, 108), ar);
    b.enemy(E_RAT, V3(-8, 0.2f, 100), ar); b.enemy(E_RAT, V3(8, 0.2f, 100), ar); b.enemy(E_RAT, V3(0, 0.2f, 96), ar);
    b.hint(AABB(V3(-13, -1, 87), V3(13, 6, 117)), "MEGA-BOT turns slowly. Run around it and burn its BACK - or throw past it and RECALL.", "", "junk.arena");
    junkPile(b, V3(-11, 0, 90), 1.2f, 6); junkPile(b, V3(11, 0, 114), 1.2f, 7);
    neon(b, V3(-13.5f, 4.5f, 102), cyan); neon(b, V3(13.5f, 4.5f, 102), pink);
    b.pickup(PK_CANDLE, V3(12, 0.7f, 88), 9, "candle.9");

    // --- the furnace row (timed braziers)
    b.slab(-8, 120, 8, 150, 0, T_DIRT, T_RUST, 3);
    b.wall(-9, 120, -8, 150, 0, 4, T_RUST); b.wall(8, 120, 9, 150, 0, 4, T_RUST);
    b.brazier(V3(-6, 0, 127), 1, false, 6); b.brazier(V3(6, 0, 127), 1, false, 6); b.brazier(V3(0, 0, 136), 1, false, 6);
    b.gate(AABB(V3(-8, 0, 141), V3(8, 4.5f, 141.6f)), 1, "junk.g2");
    b.hint(AABB(V3(-8, -1, 121), V3(8, 4, 140)), "Three furnaces, six seconds. Throw, recall, throw - or Bloom between them.", "", "junk.g2");
    b.lantern(V3(5, 0, 147), b.spawn(V3(3.5f, 0, 147.5f), 0));
    car(b, V3(-5.5f, 0, 145), 1.4f, hexColor(0xd0a020));

    // --- KING RATTLETRAP's throne of scrap
    b.slab(-16, 150, 16, 194, 0, T_TILEFLOOR, T_RUST, 3);
    b.wall(-17, 150, -16, 194, 0, 6, T_RUST); b.wall(16, 150, 17, 194, 0, 6, T_RUST);
    b.wall(-16, 150, -4, 150.6f, 0, 6, T_RUST); b.wall(4, 150, 16, 150.6f, 0, 6, T_RUST);
    b.wall(-16, 193.4f, -3, 194, 0, 6, T_RUST); b.wall(3, 193.4f, 16, 194, 0, 6, T_RUST);
    int bg = b.gate(AABB(V3(-4, 0, 150), V3(4, 5, 150.6f)), -1, "", T_GRATE);
    V3 C(0, 0, 172);
    b.solid(AABB(C + V3(-2.6f, 0, -2.6f), C + V3(2.6f, 3, 2.6f)), T_RUST, -1, V3(1, 1, 1), false);
    L.boss.type = B_RATKING; L.boss.center = C; L.boss.radius = 15;
    L.boss.trigger = AABB(V3(-14, -1, 155), V3(14, 8, 190)); L.boss.gates = {bg}; L.boss.flag = "boss.4";
    for (int i = 0; i < 6; i++) { float a = i * TAU / 6 + 0.3f; junkPile(b, C + V3(std::sin(a) * 14, 0, std::cos(a) * 14), 1.3f, 20 + i); }
    neon(b, V3(-15.5f, 5, 160), pink); neon(b, V3(15.5f, 5, 160), cyan); neon(b, V3(-15.5f, 5, 184), cyan); neon(b, V3(15.5f, 5, 184), pink);
    b.hint(AABB(V3(-15, -1, 152), V3(15, 8, 192)), "Step out of the claw's shadow! When the claw is STUCK, the cockpit opens - burn the rat!", "", "boss.4");
    b.slab(-3, 194, 3, 200, 0, T_DIRT, T_RUST, 3);
    b.wall(-4, 194, -3, 200, 0, 5, T_RUST); b.wall(3, 194, 4, 200, 0, 5, T_RUST);
    b.exit(AABB(V3(-2.5f, 0, 195), V3(2.5f, 4, 200)), LV_HUB, 5, "heart.4", "Take the Beacon-Heart first - it holds your next power.", "Return to Lumen Hollow");

    b.urns({V3(-5, 0, 3), V3(5, 0, 14), V3(-5, 0, 38), V3(6, 0, 48), V3(-6, 0, 74), V3(-11, 0, 118), V3(11, 0, 86), V3(-6, 0, 132), V3(6, 0, 132), V3(-13, 0, 192), V3(13, 0, 192)}, 1);
    b.emitter(AABB(V3(-12, 0, -5), V3(12, 8, 195)), 1, 6);
    b.emitter(AABB(V3(-40, -1, -10), V3(40, 0.5f, 200)), 2, 6);
}
