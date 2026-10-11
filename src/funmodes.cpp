// WICKLIGHT - fun forever:
//  THE TOY BOX   silly modes you can flip on any time (pause menu): big head, tiny Wick, moon gravity, party mode,
//                chain-reaction explosions, turbo, endless bloom, giant flame, jelly enemies, slow-mo kills
//  THE GAUNTLET  an arena of endless waves, each one stranger than the last (swarms, moth storms, FBI raids,
//                low gravity, everything-explodes...), with mini-bosses every 5 waves
//  THE RISING DARK  an endless tower to climb while the Hush's tide rises underneath you
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

// ------------------------------------------------------------------ the Rising Dark: an endless climb
// A tower of floating platforms spiralling up into the night. The Hush's tide rises underneath, faster and faster.
// Jump, warp to hooks, grapple, ride the lifts - how high can you get before the dark catches you?
void buildClimb(Level& L, Game& g) {
    LB b(L);
    L.name = "The Rising Dark";
    L.song = SONG_FINAL;
    L.env.skyTop = hexColor(0x050318); L.env.skyHorizon = hexColor(0x2a1a4a);
    L.env.fogColor = hexColor(0x140c28); L.env.fogDensity = 0.012f;
    L.env.ambSky = V3(0.3f, 0.26f, 0.42f); L.env.ambGround = V3(0.12f, 0.08f, 0.16f);
    L.env.sunColor = V3(0.4f, 0.35f, 0.6f); L.env.stars = 1.6f; L.env.moonSize = 0.1f;
    L.killY = -1000;  // the tide is what kills you here
    g.climbOn = false; g.climbH = 0; g.tideY = -7; g.tideSpeed = 1.1f;
    Rng r(hash32(1234u + (uint32_t)g.save.climbRuns * 7919u));  // a new tower every run
    // the base
    b.slab(-5, -5, 5, 5, 0, T_MARBLE, T_STONEBRICK, 3);
    b.spawn(V3(0, 0, -2), 0);
    b.arch(V3(-3.6f, 0, -2.5f), 2.2f, 3.4f, 0.6f, T_MARBLE, false);
    b.exit(AABB(V3(-4.1f, 0, -3.6f), V3(-3.7f, 3, -1.4f)), LV_HUB, 10, "", "", "Back to Lumen Hollow");
    b.hint(AABB(V3(-5, -1, -5), V3(5, 4, 5)), "THE RISING DARK. Climb! The tide starts rising as soon as you leave this floor. Jump, warp to hooks, grapple, ride the lifts. How high can you go?");
    // the spire everything spirals around
    L.decor.cylinder(M4::translate(V3(0, 500, 0)) * M4::scale(V3(3, 1000, 3)), T_STONEBRICK, 12, false, 200);
    // Platforms go round the spire at a steady radius. Each step's angle is worked out from the edge-to-edge gap we
    // want, so every jump is makeable (Wick jumps ~2.2 high and ~4 across) and nothing ends up stacked on top.
    V3 prev(0, 0, 0); float prevHw = 5, prevRad = 0, liftExtra = 0;
    float ang = r.range(0, 6.28f);
    L.route.push_back({0, V3(0, 0, 0)});
    for (int i = 0; i < 420; i++) {
        float y = prev.y;
        int kind = r.f() < 0.62f ? 0 : (r.f() < 0.6f ? 1 : 2);  // 0 jump, 1 hook gap, 2 moving lift
        if (i < 3) kind = 0;
        float hw = r.range(1.3f, 2.1f) - std::min(0.4f, i * 0.002f);
        float rad = r.range(7.0f, 9.5f);
        float gap = kind == 1 ? r.range(3.5f, 5.5f) : r.range(0.8f, 2.4f + std::min(0.3f, i * 0.002f));
        float dy = kind == 1 ? r.range(3.2f, 4.8f) : r.range(0.8f, 1.5f);
        V3 c;
        if (i == 0) {
            c = V3(std::sin(ang) * (5 + hw + gap), y + dy, std::cos(ang) * (5 + hw + gap));
            rad = 5 + hw + gap;
        } else {
            float d = prevHw + hw + gap + liftExtra;           // centre-to-centre distance we want
            float cs = (prevRad * prevRad + rad * rad - d * d) / (2 * prevRad * rad);
            ang += std::acos(clampf(cs, -1.f, 1.f));
            c = V3(std::sin(ang) * rad, y + dy, std::cos(ang) * rad);
            // the slabs are square, so make sure the boxes themselves don't overlap
            for (int k = 0; k < 40 && std::max(std::fabs(c.x - prev.x), std::fabs(c.z - prev.z)) - prevHw - hw < 0.9f; k++) {
                ang += 0.03f;
                c = V3(std::sin(ang) * rad, y + dy, std::cos(ang) * rad);
            }
        }
        prevRad = rad;
        liftExtra = 0;
        int tile = (i / 25) % 2 ? T_MARBLE : T_STONEBRICK;
        if (kind == 2) {
            // a lift that slides between this spot and a point further round
            V3 off = norm(V3(c.z, 0, -c.x)) * 3.0f;  // slides forward round the tower; the next platform waits beyond it
            liftExtra = 3.0f;
            b.mover(AABB(c + V3(-hw, -0.4f, -hw), c + V3(hw, 0, hw)), V3(0, 0, 0), off, 3.6f, r.f(), T_BRASS, T_METAL);
        } else b.slab(c.x - hw, c.z - hw, c.x + hw, c.z + hw, c.y, tile, T_STONEBRICK, 0.6f);
        if (kind == 1) b.anchor(c + V3(0, 2.2f, 0));
        if (r.f() < 0.22f) { Pickup p; p.type = PK_EMBER; p.pos = c + V3(0, 1.0f, 0); p.value = 3; p.life = 1e9f; L.pickups.push_back(p); }
        if (i % 6 == 0) b.light(c + V3(0, 2.5f, 0), 8, i % 12 ? hexColor(0xffb060) : hexColor(0xa070ff), 0.9f);
        if (i % 25 == 24) {  // a height marker every 25 platforms
            L.decor.emis = 1.5f; L.decor.color = hexColor(0x80e0ff);
            L.decor.cube(M4::translate(V3(0, c.y, 0)) * M4::scale(V3(6.4f, 0.3f, 6.4f)), T_WHITE);
            L.decor.emis = 0; L.decor.color = V3(1, 1, 1);
        }
        L.route.push_back({kind == 1 ? 1 : kind == 2 ? 9 : 0, c});  // op 9 = a lift (it moves, the bot skips those)
        prev = c; prevHw = hw;
    }
    b.emitter(AABB(V3(-14, 0, -14), V3(14, 60, 14)), 4, 10);
}

static void climbFinish(Game& g, int h) {
    g.save.climbRuns++;
    bool best = h > g.save.climbBest;
    if (best) g.save.climbBest = h;
    g.save.embers += h / 2;
    g.writeSave();
}

static void updateClimb(Game& g, float dt) {
    if (g.climbLast >= 0) {
        g.showToast("THE DARK GOT YOU AT " + std::to_string(g.climbLast) + "m", "best " + std::to_string(g.save.climbBest) + "m   (+" + std::to_string(g.climbLast / 2) + " embers)   - climb again!");
        g.climbLast = -1;
    }
    if (g.pl.dead) return;
    if (!g.climbOn) {
        if (g.pl.pos.y > 0.6f || len(flatXZ(g.pl.pos)) > 5.5f) { g.climbOn = true; audio::play(SFX_ROAR, 0.7f, 0.6f); g.statusMsg = "THE DARK IS RISING! CLIMB!"; g.statusT = 2.5f; }
        return;
    }
    g.climbH = std::max(g.climbH, (int)g.pl.pos.y);
    // the tide speeds up over time, and never falls too far behind
    g.tideSpeed = std::min(3.2f, g.tideSpeed + dt * 0.012f);
    g.tideY += g.tideSpeed * dt;
    if (g.tideY < g.pl.pos.y - 22) g.tideY = g.pl.pos.y - 22;
    if (g.pl.pos.y < g.tideY + 0.2f) {
        climbFinish(g, g.climbH);
        g.burst(g.pl.pos + V3(0, 1, 0), hexColor(0x6a3aaa), 50, 6, 0.2f, 2, 1.2f, 0);
        g.killPlayer();
    }
    // milestones
    static int lastMile = 0;
    int mile = g.climbH / 50;
    if (mile > lastMile && mile > 0) { g.showToast(std::to_string(mile * 50) + " METRES!", g.climbH > g.save.climbBest ? "a new record!" : "keep going!"); audio::play(SFX_BELL, 0.7f, 1.4f); }
    lastMile = mile;
    if (g.fxTick(0.08f) && !g.headless && g.R) {
        Particle q; q.pos = V3(frng.range(-14, 14), g.tideY + 0.3f, frng.range(-14, 14)) ; q.vel = V3(0, frng.range(1, 3), 0);
        q.color = hexColor(0x8050d0); q.alpha = 0.8f; q.size = 0.4f; q.grow = 0.6f; q.life = q.maxLife = 1.4f; q.kind = 2;
        g.R->spawn(q);
    }
}

// the tide, drawn as a slab of living shadow
void drawFunWorld(Game& g, Renderer* R) {
    if (g.lv.id != LV_CLIMB || R->shadowPass) return;
    float y = g.climbOn || g.pl.dead ? g.tideY : -7;
    R->drawPart(PM_CUBE, M4::translate(V3(0, y - 15, 0)) * M4::scale(V3(90, 30, 90)), T_SHADOW, V3(1, 1, 1), 0.2f, 0, 0.92f);
    for (int k = 0; k < 24; k++) {
        float a = k * TAU / 24 + g.time * 0.2f;
        R->drawPart(PM_SPHERE, M4::translate(V3(std::sin(a) * 12, y + 0.2f + 0.3f * std::sin(g.time * 2 + k), std::cos(a) * 12)) * M4::scale(V3(3, 1, 3)), T_EYE, V3(1, 1, 1), 0.6f, 0, 0.6f);
    }
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
    if (lv.id == LV_CLIMB) updateClimb(*this, dt);
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
        R->uiTextCentered(W / 2, 34 * s, b, s * 1.3f, g.gState == 2 ? C4(0x80ff90) : C4(0xff9060));
        if (g.gState == 2) R->uiTextCentered(W / 2, 46 * s, "next wave in " + std::to_string(std::max(0, (int)std::ceil(5 - g.gT))), s, COL_DIM);
    }
    if (g.lv.id == LV_CLIMB) {
        char b[128];
        snprintf(b, sizeof(b), "HEIGHT %dm   BEST %dm", g.climbOn ? g.climbH : 0, g.save.climbBest);
        R->uiTextCentered(W / 2, 34 * s, b, s * 1.4f, C4(0xd0c0ff));
        if (g.climbOn) {
            float gap = g.pl.pos.y - g.tideY;
            char t[64]; snprintf(t, sizeof(t), "the dark is %.0fm below you", std::max(0.f, gap));
            R->uiTextCentered(W / 2, 47 * s, t, s, gap < 6 ? (std::fmod(g.time * 4, 1.f) < 0.5f ? C4(0xff4060) : C4(0xb080ff)) : COL_DIM);
        }
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
