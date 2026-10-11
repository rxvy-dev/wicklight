// WICKLIGHT - fun forever:
//  THE TOY BOX   silly modes you can flip on any time (pause menu): big head, tiny Wick, moon gravity, party mode,
//                chain-reaction explosions, turbo, endless bloom, giant flame, jelly enemies, slow-mo kills
//  THE GAUNTLET  an arena of endless waves, each one stranger than the last (swarms, moth storms, FBI raids,
//                low gravity, everything-explodes...), with mini-bosses every 5 waves
//  FLAME GOLF    endless generated holes on floating islands: your flame is the ball - throw, warp, repeat,
//                and burn the brazier on the green in as few throws as you can
#include "builder.h"
#include <cstdio>
#include <algorithm>

extern int g_dmgKind;
static V4 C4(uint32_t h, float a = 1) { V3 c = hexColor(h); return V4(c.x, c.y, c.z, a); }
static const V4 COL_TEXT = C4(0xf0e6d0), COL_DIM = C4(0x8a8098), COL_HI = C4(0xffc060);
static Rng frng(2468);

// ------------------------------------------------------------------ the Toy Box
struct Toy { const char* name; const char* desc; };
static const Toy TOYS[] = {
    {"Big Head Wick", "a head three sizes too big (it still fits a hat)"},
    {"Tiny Wick", "a pocket-sized candle"},
    {"Moon Gravity", "floaty, enormous jumps"},
    {"Party Mode", "confetti on every hit, disco lights, party horns"},
    {"Kaboom", "burned enemies EXPLODE - chain reactions!"},
    {"Turbo Wick", "run and dash way faster"},
    {"Endless Bloom", "no cooldown on Bloom. Go wild"},
    {"Giant Flame", "a thrown flame twice as big that hits everything"},
    {"Jelly Enemies", "everything wobbles like jelly"},
    {"Slow-Mo Kills", "every kill gets a dramatic slow-motion moment"},
};
static const int NUM_TOYS = 10;

float Game::gravityMul() const {
    float g = toy(2) ? 0.35f : 1.f;
    if (lv.id == LV_ARENA && gState >= 1 && gMod == 6) g = std::min(g, 0.4f);
    return g;
}
float Game::enemySpeedMul() const { return (lv.id == LV_ARENA && gState == 1 && gMod == 1) ? 1.5f : 1.f; }

void Game::updateToys(const InputState& in) {
    int n = NUM_TOYS + 1;
    if (in.mUp) { shopSel = (shopSel + n - 1) % n; audio::play(SFX_MENU_MOVE); }
    if (in.mDown) { shopSel = (shopSel + 1) % n; audio::play(SFX_MENU_MOVE); }
    if (mouseMoved) for (auto& r : menuRects) if (mouseX >= r.x && mouseX <= r.x + r.w && mouseY >= r.y && mouseY <= r.y + r.h) shopSel = r.idx;
    bool clicked = false;
    if (mouseClick) for (auto& r : menuRects) if (mouseX >= r.x && mouseX <= r.x + r.w && mouseY >= r.y && mouseY <= r.y + r.h) { shopSel = r.idx; clicked = true; }
    if (in.mBack || in.pressed[A_PAUSE]) { screen = SC_PAUSE; menuSel = 5; audio::play(SFX_MENU_BACK); return; }
    if (!(in.mOk || clicked || in.mLeft || in.mRight)) return;
    if (shopSel == NUM_TOYS) { screen = SC_PAUSE; menuSel = 5; audio::play(SFX_MENU_BACK); return; }
    std::string f = "toy." + std::to_string(shopSel);
    if (has(f)) save.flags.erase(f); else setFlag(f);
    audio::play(has(f) ? SFX_UNLOCK : SFX_MENU_BACK, 0.8f, has(f) ? 1.3f : 1.f);
    writeSave();
}

void Game::drawToys() {
    float s = (float)R->uiScale();
    float W = (float)R->winW, H = (float)R->winH;
    R->uiRect(0, 0, W, H, V4(0, 0, 0, 0.55f));
    float pw = std::min(W - 20 * s, 340 * s), px = W / 2 - pw / 2, py = H * 0.08f;
    float ph = 30 * s + (NUM_TOYS + 1) * 17 * s;
    R->uiFrame(px, py, pw, ph, C4(0x0c0a14, 0.93f), COL_HI);
    R->uiTextCentered(W / 2, py + 6 * s, "THE TOY BOX", s * 1.6f, COL_HI);
    menuRects.clear();
    float y = py + 24 * s;
    for (int i = 0; i <= NUM_TOYS; i++) {
        bool sel = i == shopSel;
        if (sel) R->uiRect(px + 3 * s, y - 2 * s, pw - 6 * s, 16 * s, C4(0x3a2418, 0.8f));
        menuRects.push_back({px, y - 2 * s, pw, 16 * s, i});
        if (i == NUM_TOYS) { R->uiText(px + 10 * s, y + 3 * s, "Back", s * 1.2f, sel ? COL_HI : COL_TEXT); break; }
        bool on = toy(i);
        R->uiText(px + 10 * s, y, TOYS[i].name, s * 1.2f, sel ? COL_HI : COL_TEXT);
        R->uiText(px + 10 * s, y + 8 * s, TOYS[i].desc, s, COL_DIM);
        std::string st = on ? "ON" : "off";
        R->uiText(px + pw - R->uiTextWidth(st, s * 1.4f) - 10 * s, y + 2 * s, st, s * 1.4f, on ? C4(0x70ff80) : COL_DIM);
        y += 17 * s;
    }
    R->uiTextCentered(W / 2, py + ph + 6 * s, "Enter: switch on/off   Esc: back.   Mix them! Toys work everywhere, any time.", s, COL_DIM);
}

// party confetti, fireworks
static void confetti(Game& g, V3 p, int n) {
    if (g.headless || !g.R) return;
    static const uint32_t C[6] = {0xff4060, 0xffd040, 0x40e0ff, 0x60ff70, 0xc060ff, 0xff9030};
    for (int i = 0; i < n; i++) {
        Particle q; V3 d = norm(V3(frng.range(-1, 1), frng.range(0.2f, 1.5f), frng.range(-1, 1)));
        q.pos = p; q.vel = d * frng.range(3, 8); q.color = hexColor(C[i % 6]); q.alpha = 1; q.size = 0.09f;
        q.life = q.maxLife = frng.range(0.8f, 1.6f); q.gravity = 9; q.drag = 1.5f; q.kind = 0;
        g.R->spawn(q);
    }
}
static void fireworks(Game& g, V3 c, int n) {
    for (int k = 0; k < n; k++) {
        V3 p = c + V3(frng.range(-10, 10), frng.range(8, 14), frng.range(-10, 10));
        static const uint32_t C[5] = {0xff5060, 0xffd050, 0x50d0ff, 0x80ff80, 0xd080ff};
        g.burst(p, hexColor(C[k % 5]), 40, 9, 0.12f, 3, 1.2f, 3);
        g.burst(p, V3(1, 1, 1), 10, 3, 0.2f, 1, 0.4f, 0);
    }
    audio::play(SFX_BLOOM, 0.6f, 1.6f);
    audio::play(SFX_BELL, 0.5f, 1.8f);
}

// every enemy death goes through here (toys and the Gauntlet's modifiers)
void Game::enemyDied(Enemy& e) {
    V3 c = e.pos + V3(0, e.height * 0.5f, 0);
    bool kaboom = toy(4) || (lv.id == LV_ARENA && gState == 1 && gMod == 7);
    if (kaboom) pendingBlasts.push_back(c);
    if (toy(3)) { confetti(*this, c, 40); audio::playAt(SFX_BLIP, c, 0.8f, 2.4f); }
    if (toy(9)) killSlowT = 0.45f;
    if (lv.id == LV_ARENA && gMod == 8 && gState == 1) dropEmbers(e.pos, 6);
}

// ------------------------------------------------------------------ the Gauntlet
static const char* MODS[10] = {"SWARM", "SPEED DEMONS", "MOTH STORM", "ROBOT RUMBLE", "FBI RAID", "GLITCH STORM", "LOW GRAVITY", "KABOOM", "GOLD RUSH", "TANKS"};
static const char* MODDESC[10] = {"so many of them", "the enemies are FAST", "the sky is full of wings", "Mega-Bot brought friends", "the agents found you", "GL1TCH sends his regards",
                                  "jump to the moon", "everything explodes", "double embers!", "shields up"};
static V3 arenaGate(int k) { float a = k * TAU / 6 + 0.3f; return V3(std::sin(a) * 18.5f, 0.3f, std::cos(a) * 18.5f); }

void buildArena(Level& L, Game& g) {
    LB b(L);
    L.name = "The Gauntlet";
    L.song = SONG_JUNK;
    L.env.skyTop = hexColor(0x140818); L.env.skyHorizon = hexColor(0x6a2a3a);
    L.env.fogColor = hexColor(0x2a1424); L.env.fogDensity = 0.014f;
    L.env.ambSky = V3(0.3f, 0.2f, 0.26f); L.env.ambGround = V3(0.14f, 0.08f, 0.1f);
    L.env.sunColor = V3(0.5f, 0.3f, 0.3f); L.env.moonSize = 0.1f; L.env.moonColor = hexColor(0xffb090);
    L.killY = -10;
    g.gState = 0; g.gWave = 0; g.gMod = -1; g.gQueue = 0;
    b.slab(-22, -22, 22, 22, 0, T_TILEFLOOR, T_STONEBRICK, 3);
    L.decor.color = hexColor(0xd8c090);
    L.decor.cylinder(M4::translate(V3(0, 0.02f, 0)) * M4::scale(V3(26, 0.04f, 26)), T_SAND, 24, true, 6);
    L.decor.color = V3(1, 1, 1);
    L.decor.emis = 1.5f; L.decor.color = hexColor(0xff7040);
    for (int i = 0; i < 24; i++) { float a = i * TAU / 24; L.decor.cube(M4::translate(V3(std::sin(a) * 3, 0.06f, std::cos(a) * 3)) * M4::rotY(a) * M4::scale(V3(0.3f, 0.06f, 0.75f)), T_WHITE); }
    L.decor.emis = 0; L.decor.color = V3(1, 1, 1);
    b.wall(-23, -22, -22, 22, 0, 4, T_STONEBRICK); b.wall(22, -22, 23, 22, 0, 4, T_STONEBRICK);
    b.wall(-22, 22, 22, 23, 0, 4, T_STONEBRICK); b.wall(-22, -23, 22, -22, 0, 4, T_STONEBRICK);
    // the stands, packed with a cheering crowd of little lights
    Rng r(77);
    for (int side = 0; side < 4; side++)
        for (int tier = 0; tier < 4; tier++) {
            float d0 = 23 + tier * 2.0f, d1 = d0 + 2.0f, h = 4 + tier * 1.6f;
            AABB box = side == 0 ? AABB(V3(-d1, 0, -d1), V3(-d0, h, d1)) : side == 1 ? AABB(V3(d0, 0, -d1), V3(d1, h, d1)) : side == 2 ? AABB(V3(-d1, 0, d0), V3(d1, h, d1)) : AABB(V3(-d1, 0, -d1), V3(d1, h, -d0));
            L.decor.box(box, T_STONEBRICK, 1, T_PLANK);
            for (int k = 0; k < 14; k++) {
                float u = r.range(-d0 + 1, d0 - 1);
                V3 p = side == 0 ? V3(-(d0 + 1), h, u) : side == 1 ? V3(d0 + 1, h, u) : side == 2 ? V3(u, h, d0 + 1) : V3(u, h, -(d0 + 1));
                static const uint32_t C[6] = {0xff6060, 0x60a0ff, 0xffd060, 0x80ff90, 0xd080ff, 0xffffff};
                L.decor.color = hexColor(C[r.irange(0, 5)]); L.decor.emis = 0.5f;
                L.decor.cube(M4::translate(p + V3(0, 0.5f, 0)) * M4::scale(V3(0.5f, 1.0f, 0.5f)), T_CLOTH);
                L.decor.color = hexColor(0xd8a888); L.decor.emis = 0.2f;
                L.decor.cube(M4::translate(p + V3(0, 1.2f, 0)) * M4::scale(V3(0.4f, 0.4f, 0.4f)), T_WHITE);
                L.decor.emis = 0; L.decor.color = V3(1, 1, 1);
            }
        }
    for (int k = 0; k < 6; k++) {  // the six monster gates
        V3 p = arenaGate(k) * (21.9f / 18.5f);
        float yaw = std::atan2(p.x, p.z);
        L.decor.cube(M4::translate(V3(p.x, 1.6f, p.z)) * M4::rotY(yaw) * M4::scale(V3(3.2f, 3.2f, 0.3f)), T_GRATE);
        L.decor.emis = 1.5f; L.decor.color = hexColor(0xff4040);
        L.decor.cube(M4::translate(V3(p.x, 3.6f, p.z) * 0.99f) * M4::rotY(yaw) * M4::scale(V3(0.5f, 0.5f, 0.2f)), T_WHITE);
        L.decor.emis = 0; L.decor.color = V3(1, 1, 1);
    }
    for (int k = 0; k < 8; k++) {
        float a = k * TAU / 8 + PI / 8;
        V3 p(std::sin(a) * 15, 0, std::cos(a) * 15);
        b.pillar(p.x, p.z, 0, 3.5f, 0.6f, T_MARBLE);
        L.decor.emis = 2; L.decor.color = hexColor(0xffa040);
        L.decor.cube(M4::translate(p + V3(0, 3.9f, 0)) * M4::scale(V3(0.5f, 0.5f, 0.5f)), T_WHITE);
        L.decor.emis = 0; L.decor.color = V3(1, 1, 1);
        b.light(p + V3(0, 4.5f, 0), 10, hexColor(0xffa050), 0.9f);
    }
    b.spawn(V3(0, 0, -17), 0);
    b.lantern(V3(5, 0, -19), 0);
    b.exit(AABB(V3(-2, 0, -21.9f), V3(2, 3.5f, -21)), LV_HUB, 9, "", "", "Leave the Gauntlet");
    char hint[160];
    snprintf(hint, sizeof(hint), "THE GAUNTLET: endless waves. Step into the glowing ring to begin. Best: wave %d.  (The lantern sells upgrades between waves.)", g.save.gauntletBest);
    b.hint(AABB(V3(-22, -1, -22), V3(22, 4, -12)), hint);
    b.emitter(AABB(V3(-20, 0, -20), V3(20, 6, 20)), 1, 8);
}

static void beginWave(Game& g) {
    int w = g.gWave;
    Rng r(hash32((uint32_t)w * 4099u + (uint32_t)g.save.gauntletBest));
    g.gMod = (w >= 3 && r.f() < 0.6f) ? r.irange(0, 9) : -1;
    if (w % 5 == 0) g.gMod = -1;  // mini-boss waves are plain
    g.gQueue = std::min(4 + (int)(w * 1.5f), 26);
    if (g.gMod == 0) g.gQueue = g.gQueue * 17 / 10;
    g.gSpawnT = 1.2f;
    g.gState = 1;
    g.enemyHpMul = std::min(1.f + 0.06f * (w - 1), 3.f);
    if (w % 5 == 0) {
        g.spawnEnemy((w / 5) % 2 ? E_GLOOMKING : E_MEGABOT, arenaGate(r.irange(0, 5)), -1);
        g.enemies.back().state = 1;
        if (w % 10 == 0) { g.spawnEnemy((w / 5) % 2 ? E_MEGABOT : E_GLOOMKING, arenaGate(r.irange(0, 5)), -1); g.enemies.back().state = 1; }
    }
    g.showToast("WAVE " + std::to_string(w), g.gMod >= 0 ? std::string(MODS[g.gMod]) + "  -  " + MODDESC[g.gMod] : (w % 5 == 0 ? "MINI-BOSS!" : "fight!"));
    audio::play(SFX_ROAR, 0.8f, w % 5 == 0 ? 0.7f : 1.4f);
    audio::play(SFX_BELL, 0.7f, 1.2f);
    if (w == 1) audio::setSong(SONG_BOSS);
}

static EnemyType waveEnemy(Game& g) {
    int w = g.gWave;
    std::vector<EnemyType> pool = {E_GLOOMLING, E_GLOOMLING, E_RAT};
    if (w >= 2) { pool.push_back(E_TARBELL); pool.push_back(E_MOTH); }
    if (w >= 4) { pool.push_back(E_KNIGHT); pool.push_back(E_VIRUS); }
    if (w >= 6) { pool.push_back(E_WRAITH); pool.push_back(E_DRONE); pool.push_back(E_AGENT); }
    switch (g.gMod) {
        case 0: pool = {E_GLOOMLING, E_RAT, E_VIRUS}; break;
        case 2: pool = {E_MOTH}; break;
        case 3: pool = {E_RAT, E_RAT, E_RAT, E_MEGABOT}; break;
        case 4: pool = {E_AGENT, E_AGENT, E_DRONE}; break;
        case 5: pool = {E_VIRUS, E_DECOY, E_DRONE}; break;
        case 9: pool = {E_KNIGHT}; break;
        default: break;
    }
    EnemyType t = pool[frng.irange(0, (int)pool.size() - 1)];
    if (t == E_MEGABOT) { int mb = 0; for (auto& e : g.enemies) if (e.alive && e.type == E_MEGABOT) mb++; if (mb >= 1) t = E_RAT; }
    return t;
}

static void updateGauntlet(Game& g, float dt) {
    if (g.gState == 0) {
        if (g.gLast > 0) {
            g.showToast("YOU SURVIVED " + std::to_string(g.gLast) + (g.gLast == 1 ? " WAVE" : " WAVES"), "best: wave " + std::to_string(g.save.gauntletBest) + "   - step into the ring to go again");
            g.gLast = 0;
        }
        if (!g.pl.dead && distXZ(g.pl.pos, V3(0, 0, 0)) < 3 && g.pl.pos.y < 2) { g.gWave = 1; beginWave(g); }
        return;
    }
    if (g.gState == 1) {
        int alive = 0;
        for (auto& e : g.enemies) if (e.alive) alive++;
        g.gSpawnT -= dt;
        if (g.gQueue > 0 && g.gSpawnT <= 0 && alive < 12) {
            g.gSpawnT = 0.55f;
            int k = frng.irange(0, 5);
            for (int tries = 0; tries < 6 && distXZ(arenaGate(k), g.pl.pos) < 7; tries++) k = (k + 1) % 6;
            V3 p = arenaGate(k) + V3(frng.range(-1, 1), 0, frng.range(-1, 1));
            g.spawnEnemy(waveEnemy(g), p, -1);
            g.enemies.back().state = 1; g.enemies.back().alerted = true;
            g.burst(p + V3(0, 1, 0), hexColor(0xff5040), 12, 4, 0.1f, 3, 0.5f, 0);
            g.gQueue--;
        }
        if (g.gMod == 5 && g.fxTick(2.5f)) g.glitch = std::max(g.glitch, 0.3f);
        // nobody hides in the Gauntlet: everything hunts you
        for (auto& e : g.enemies) if (e.alive && e.state == 0 && e.type != E_TARBELL) { e.state = 1; e.alerted = true; }
        if (g.gQueue == 0 && alive == 0) {
            g.gState = 2; g.gT = 0;
            g.save.gauntletBest = std::max(g.save.gauntletBest, g.gWave);
            int emb = 10 + g.gWave * 4;
            if (g.gMod == 8) emb *= 2;
            g.dropEmbers(g.pl.pos + V3(0, 1, 2), emb);
            g.dropWax(g.pl.pos + V3(1, 0, 1)); g.dropWax(g.pl.pos + V3(-1, 0, 1));
            g.showToast("WAVE " + std::to_string(g.gWave) + " CLEARED!", "the crowd goes wild   (+" + std::to_string(emb) + " embers)");
            fireworks(g, V3(0, 0, 0), 6);
            g.writeSave();
        }
        return;
    }
    if (g.gState == 2) {
        g.gT += dt;
        if (g.fxTick(0.7f) && g.gT < 3) fireworks(g, V3(0, 0, 0), 1);
        if (g.gT > 5) { g.gWave++; beginWave(g); }
    }
}

// ------------------------------------------------------------------ Flame Golf
void buildGolf(Level& L, Game& g) {
    LB b(L);
    int hole = std::max(1, g.save.golfHole);
    Rng r(hash32((uint32_t)hole * 7349u + 11u));
    char nm[64]; snprintf(nm, sizeof(nm), "Flame Golf - Hole %d", hole);
    L.name = nm;
    L.song = SONG_ENDING;
    L.env.skyTop = hexColor(0x2a3a6a); L.env.skyHorizon = hexColor(0xe08a6a);
    L.env.fogColor = hexColor(0x8a6a7a); L.env.fogDensity = 0.008f;
    L.env.ambSky = V3(0.5f, 0.46f, 0.55f); L.env.ambGround = V3(0.22f, 0.18f, 0.2f);
    L.env.sunDir = norm(V3(-0.6f, -0.5f, 0.4f)); L.env.sunColor = V3(0.95f, 0.7f, 0.5f);
    L.env.stars = 0.2f; L.env.moonSize = 0;
    L.killY = -24;
    auto island = [&](V3 c, float hw, bool green) {
        b.slab(c.x - hw, c.z - hw, c.x + hw, c.z + hw, c.y, green ? T_MOSS : T_GRASS, T_DIRT, 2);
        L.decor.cone(M4::translate(c + V3(0, -4.5f, 0)) * M4::rotX(PI) * M4::scale(V3(hw * 2.1f, 5, hw * 2.1f)), T_ROCK, 7);
    };
    // the tee
    island(V3(0, 0, 0), 4, false);
    b.spawn(V3(0, 0, -1), 0);
    L.decor.color = hexColor(0xff5050); L.decor.cylinder(M4::translate(V3(-1.2f, 0.3f, 1.5f)) * M4::scale(V3(0.3f, 0.6f, 0.3f)), T_WHITE, 6); L.decor.cylinder(M4::translate(V3(1.2f, 0.3f, 1.5f)) * M4::scale(V3(0.3f, 0.6f, 0.3f)), T_WHITE, 6);
    L.decor.color = V3(1, 1, 1);
    b.arch(V3(-3.5f, 0, -1.5f), 2.2f, 3.4f, 0.6f, T_MARBLE, false);
    b.exit(AABB(V3(-4.0f, 0, -2.6f), V3(-3.6f, 3, -0.4f)), LV_HUB, 10, "", "", "Back to Lumen Hollow");
    // the fairway: a chain of floating islands, sometimes a hook hanging in a gap
    int n = 1 + r.irange(0, std::min(3, 1 + hole / 3));
    V3 pos(0, 0, 0);
    float a = 0, prevHw = 4;
    L.route.push_back({0, V3(0, 0, 1.5f)});
    for (int i = 0; i < n; i++) {
        bool green = i == n - 1;
        float dist = r.range(9.5f, 14.f);
        a = clampf(a + r.range(-0.7f, 0.7f), -1.0f, 1.0f);
        V3 np = pos + V3(std::sin(a) * dist, r.range(-2.6f, -0.4f), std::cos(a) * dist);  // a downhill course: flames fly straight
        
        float hw = green ? 4.5f : r.range(2.4f, 3.6f);
        L.route.push_back({0, pos + norm(flatXZ(np - pos)) * (prevHw - 1.0f)});  // (the bot walks to the edge before each shot)
        island(np, hw, green);
        if (!green && r.f() < 0.35f) b.anchor((pos + np) * 0.5f + V3(r.range(-2, 2), 4.5f, 0));
        if (!green && r.f() < 0.5f) { if (r.f() < 0.5f) b.tree(np + V3(hw * 0.6f, 0, hw * 0.6f), 0.7f, T_LEAVES, false); else b.mushroom(np + V3(-hw * 0.5f, 0, hw * 0.3f), 0.8f); }
        if (!green && hole >= 4 && r.f() < 0.3f) b.enemy(E_GLOOMLING, np + V3(1, 0.2f, 1));
        b.light(np + V3(0, 3, 0), 9, hexColor(0xffd0a0), 0.6f);
        if (!green) L.route.push_back({1, np + V3(0, -0.3f, 0)});
        else L.route.push_back({1, np - norm(flatXZ(np - pos)) * 0.6f + V3(0, -0.3f, 0)});  // onto the green, short of the cup
        pos = np; prevHw = hw;
    }
    // the green: a brazier is the cup, with a flag
    V3 cup = pos + V3(std::sin(a), 0, std::cos(a)) * 2.6f;  // the cup sits at the far side of the green
    b.brazier(cup, 90);
    L.decor.cylinder(M4::translate(cup + V3(1.6f, 2.0f, 0)) * M4::scale(V3(0.08f, 4, 0.08f)), T_WHITE, 6);
    L.decor.color = hexColor(0xff3040); L.decor.cube(M4::translate(cup + V3(2.2f, 3.6f, 0)) * M4::scale(V3(1.1f, 0.7f, 0.06f)), T_CLOTH);
    L.decor.color = V3(1, 1, 1);
    L.route.push_back({2, cup + V3(0, 1.35f, 0)});
    g.golfPar = n + 1;
    g.golfStrokes = 0; g.golfDoneT = 0;
    char hint[200];
    snprintf(hint, sizeof(hint), "HOLE %d - PAR %d.  Throw your flame, WARP to where it lands, and burn the brazier on the green in as few throws as you can.", hole, g.golfPar);
    b.hint(AABB(V3(-4, -1, -4), V3(4, 4, 4)), hint);
    b.emitter(AABB(V3(-30, -6, -10), V3(30, 10, 70)), 0, 8);
}

void golfCup(Game& g) {
    if (g.golfDoneT > 0) return;
    int s = g.golfStrokes, par = g.golfPar, d = s - par;
    const char* name = s == 1 ? "HOLE IN ONE!" : d <= -3 ? "ALBATROSS!" : d == -2 ? "EAGLE!" : d == -1 ? "BIRDIE!" : d == 0 ? "PAR" : d == 1 ? "BOGEY" : d == 2 ? "DOUBLE BOGEY" : "...SINK IT NEXT TIME";
    int emb = std::max(5, 20 + (par - s) * 15 + (s == 1 ? 100 : 0));
    g.save.golfPlayed++; g.save.golfScore += d;
    if (s == 1) g.save.golfAces++;
    char sub[96]; snprintf(sub, sizeof(sub), "%d throws (par %d)   +%d embers", s, par, emb);
    g.showToast(name, sub);
    g.save.embers += emb;
    fireworks(g, g.pl.pos, d < 0 ? 6 : 2);
    if (d < 0) confetti(g, g.pl.pos + V3(0, 1.5f, 0), 80);
    audio::play(SFX_HEART, 0.8f, d < 0 ? 1.3f : 1.0f);
    g.golfDoneT = 3.2f;
    g.writeSave();
}

// ------------------------------------------------------------------ per step
void Game::updateFunModes(float dt) {
    killSlowT = std::max(0.f, killSlowT - dt);
    // chain-reaction explosions
    if (!pendingBlasts.empty()) {
        auto bl = pendingBlasts; pendingBlasts.clear();
        for (V3 c : bl) {
            burst(c, hexColor(0xff9030), 40, 9, 0.14f, 3, 0.6f, 2);
            burst(c, hexColor(0xfff0c0), 12, 4, 0.3f, 1, 0.4f, 0);
            audio::playAt(SFX_BLOOM, c, 0.7f, 1.3f);
            addShake(0.25f);
            g_dmgKind = 2;
            for (auto& e : enemies) if (e.alive && len(e.pos + V3(0, e.height * 0.5f, 0) - c) < 3.6f + e.radius) damageEnemy(e, 2.5f, c, true);
            g_dmgKind = 0;
            for (int i = 0; i < (int)lv.breakables.size(); i++) if (!lv.breakables[i].broken && len(lv.breakables[i].pos - c) < 3.6f) breakUrn(i);
        }
    }
    if (lv.id == LV_ARENA) updateGauntlet(*this, dt);
    if (lv.id == LV_GOLF && golfDoneT > 0) {
        golfDoneT -= dt;
        if (golfDoneT <= 0) { save.golfHole++; requestTransition(LV_GOLF, 0); }
    }
    // party mode: the music gets a horn now and then
    if (toy(3) && fxTick(6.f) && !enemies.empty()) audio::play(SFX_BELL, 0.25f, 2.2f);
}

// HUD for the modes (top centre, under the compass)
void drawFunHUD(Game& g) {
    Renderer* R = g.R;
    float s = (float)R->uiScale();
    float W = (float)R->winW;
    if (g.lv.id == LV_ARENA && g.gState > 0) {
        int alive = 0; for (auto& e : g.enemies) if (e.alive) alive++;
        char b[128];
        snprintf(b, sizeof(b), "WAVE %d%s%s   enemies left %d   best %d", g.gWave, g.gMod >= 0 ? "   " : "", g.gMod >= 0 ? MODS[g.gMod] : "", alive + g.gQueue, g.save.gauntletBest);
        R->uiTextCentered(W / 2, 18 * s, b, s * 1.3f, g.gState == 2 ? C4(0x80ff90) : C4(0xff9060));
        if (g.gState == 2) R->uiTextCentered(W / 2, 30 * s, "next wave in " + std::to_string(std::max(0, (int)std::ceil(5 - g.gT))), s, COL_DIM);
    }
    if (g.lv.id == LV_GOLF) {
        char b[128];
        snprintf(b, sizeof(b), "HOLE %d   PAR %d   THROWS %d", g.save.golfHole, g.golfPar, g.golfStrokes);
        R->uiTextCentered(W / 2, 18 * s, b, s * 1.4f, C4(0xffe0a0));
        char t[128];
        snprintf(t, sizeof(t), "holes played %d   total %+d   holes-in-one %d", g.save.golfPlayed, g.save.golfScore, g.save.golfAces);
        R->uiTextCentered(W / 2, 31 * s, t, s, COL_DIM);
    }
    // a little reminder of what's switched on
    int on = 0; for (int i = 0; i < NUM_TOYS; i++) if (g.toy(i)) on++;
    if (on) R->uiText(8 * s, R->winH - 34 * s, "TOYS ON: " + std::to_string(on) + " (pause > Toy Box)", s, C4(0x70ff80, 0.7f));
}

// party lights
void funLights(Game& g, Renderer* R) {
    if (!g.toy(3)) return;
    for (int k = 0; k < 3; k++) {
        float a = g.time * 1.3f + k * TAU / 3;
        static const uint32_t C[3] = {0xff3080, 0x30a0ff, 0x60ff60};
        R->lights.push_back({g.pl.pos + V3(std::sin(a) * 5, 3.5f, std::cos(a) * 5), 9, hexColor(C[k]), 1.3f});
    }
}
