// WICKLIGHT - THE CANDLE CUP
// Race four rival candles (Flick, Blaze, Tallow and Smudge) round a track that's new every single race.
// Three cups (Ember, Lantern, Inferno), three races per cup, three laps per race. Boost pads, item boxes
// (Speed Candle, Spark Bolt, Wax Slick, Shield), tar puddles, and your thrown flame stuns whoever it hits.
#include "builder.h"
#include <cstdio>
#include <algorithm>

void dyeColors(int dye, float t, V3& outer, V3& mid, V3& core);
const char* hatName(int h);
static V4 C4(uint32_t h, float a = 1) { V3 c = hexColor(h); return V4(c.x, c.y, c.z, a); }
static const V4 COL_TEXT = C4(0xf0e6d0), COL_DIM = C4(0x8a8098), COL_HI = C4(0xffc060);
static Rng rrng(777);

static const int LAPS = 3, RACES = 3, NRACERS = 5;
static const char* CUP_NAME[3] = {"EMBER CUP", "LANTERN CUP", "INFERNO CUP"};
static const float CUP_SPEED[3] = {7.9f, 8.7f, 9.45f};
static const char* RIVAL[4] = {"FLICK", "BLAZE", "TALLOW", "SMUDGE"};
static const int RIVAL_DYE[4] = {0, 1, 2, 3};
static const uint32_t RIVAL_WAX[4] = {0xf0e0a0, 0xc8dcf0, 0xd8ecc0, 0xdcc8ec};
static const float RIVAL_SKILL[4] = {0.25f, 0.1f, -0.15f, 0.0f};
static const int PTS[5] = {10, 7, 5, 3, 1};
static const char* ITEM_NAME[4] = {"SPEED CANDLE", "SPARK BOLT", "WAX SLICK", "SHIELD"};
static const char* PLACE[5] = {"1st", "2nd", "3rd", "4th", "5th"};

// ------------------------------------------------------------------ the track
static V3 tpos(const Game& g, float s, V3* tan = nullptr) {
    int M = (int)g.track.size();
    float sp = g.trackLen / M;
    s = std::fmod(s, g.trackLen); if (s < 0) s += g.trackLen;
    float f = s / sp; int i = (int)f; float u = f - i;
    V3 a = g.track[i % M], b = g.track[(i + 1) % M];
    if (tan) *tan = norm(flatXZ(b - a));
    return lerp(a, b, u);
}
static V3 rightOf(V3 t) { return V3(-t.z, 0, t.x); }

static void genTrack(Game& g, uint32_t seed) {
    Rng r(hash32(seed) | 1);
    const int N = 120;
    float R0 = r.range(34, 41), sx = r.range(1.0f, 1.35f), sz = r.range(0.85f, 1.05f);
    float a2 = r.range(0.05f, 0.15f), p2 = r.range(0, TAU), a3 = r.range(0.03f, 0.11f), p3 = r.range(0, TAU), a5 = r.range(0.f, 0.04f), p5 = r.range(0, TAU);
    std::vector<V3> raw;
    for (int i = 0; i < N; i++) {
        float th = i * TAU / N;
        float rr = R0 * (1 + a2 * std::sin(2 * th + p2) + a3 * std::sin(3 * th + p3) + a5 * std::sin(5 * th + p5));
        raw.push_back(V3(std::sin(th) * rr * sx, 0, std::cos(th) * rr * sz));
    }
    float L = 0;
    for (int i = 0; i < N; i++) L += len(raw[(i + 1) % N] - raw[i]);
    int M = std::max(60, (int)std::round(L / 2.0f));
    float sp = L / M;
    g.track.clear(); g.trackS.clear();
    // resample at even spacing
    int seg = 0; float segPos = 0;
    for (int k = 0; k < M; k++) {
        float want = k * sp;
        while (true) {
            float sl = len(raw[(seg + 1) % N] - raw[seg]);
            if (want <= segPos + sl || seg >= N - 1) { float u = sl > 0 ? (want - segPos) / sl : 0; g.track.push_back(lerp(raw[seg], raw[(seg + 1) % N], clampf(u, 0, 1))); break; }
            segPos += sl; seg++;
        }
        g.trackS.push_back(want);
    }
    g.trackLen = L;
    g.trackHw = 4.6f;
}

// nearest track sample to p, searching around a hint index (or everywhere when hint < 0)
static int nearestIdx(const Game& g, V3 p, int hint, int window = 10) {
    int M = (int)g.track.size(), best = 0; float bd = 1e30f;
    if (hint < 0) { for (int i = 0; i < M; i++) { float d = distXZ(g.track[i], p); if (d < bd) { bd = d; best = i; } } return best; }
    for (int k = -window; k <= window; k++) { int i = ((hint + k) % M + M) % M; float d = distXZ(g.track[i], p); if (d < bd) { bd = d; best = i; } }
    return best;
}
// arc position and sideways offset of p, near sample i
static void projectOn(const Game& g, V3 p, int i, float& s, float& lat) {
    int M = (int)g.track.size();
    V3 a = g.track[i], b = g.track[(i + 1) % M];
    V3 d = flatXZ(b - a); float dl = len(d); V3 t = dl > 0 ? d / dl : V3(0, 0, 1);
    float u = dot(flatXZ(p - a), t);
    if (u < 0) {  // behind this sample: use the previous segment
        V3 a0 = g.track[(i - 1 + M) % M]; V3 d0 = flatXZ(a - a0); float l0 = len(d0); V3 t0 = l0 > 0 ? d0 / l0 : t;
        float u0 = clampf(dot(flatXZ(p - a0), t0), 0, l0);
        s = g.trackS[(i - 1 + M) % M] + u0; lat = dot(flatXZ(p - a0), rightOf(t0));
        if (s >= g.trackLen) s -= g.trackLen;
        return;
    }
    s = g.trackS[i] + std::min(u, dl); lat = dot(flatXZ(p - a), rightOf(t));
    if (s >= g.trackLen) s -= g.trackLen;
}

static float progress(const Game& g, const Game::Racer& r) { return r.lap * g.trackLen + r.s; }
static uint32_t raceSeed(const Game& g) { return 977u * (uint32_t)g.save.raceRuns + 31u * (uint32_t)g.rcCup + (uint32_t)g.rcRace + 5u; }

void buildRace(Level& L, Game& g) {
    LB b(L);
    L.name = "The Candle Cup";
    L.song = SONG_JUNK;
    L.env.skyTop = hexColor(0x1a2a5a); L.env.skyHorizon = hexColor(0xf0906a);
    L.env.fogColor = hexColor(0x6a5a7a); L.env.fogDensity = 0.006f;
    L.env.ambSky = V3(0.55f, 0.5f, 0.6f); L.env.ambGround = V3(0.25f, 0.2f, 0.18f);
    L.env.sunColor = V3(1.0f, 0.75f, 0.5f); L.env.moonSize = 0.1f; L.env.moonColor = hexColor(0xffd0a0);
    L.killY = -20;
    genTrack(g, raceSeed(g));
    Rng r(hash32(raceSeed(g) * 13u + 1));
    int M = (int)g.track.size();
    float hw = g.trackHw;
    V3 lo(1e9f, 0, 1e9f), hi(-1e9f, 0, -1e9f);
    for (auto& p : g.track) { lo.x = std::min(lo.x, p.x); lo.z = std::min(lo.z, p.z); hi.x = std::max(hi.x, p.x); hi.z = std::max(hi.z, p.z); }
    // the meadow everything sits on
    b.slab(lo.x - 34, lo.z - 34, hi.x + 34, hi.z + 34, 0, T_GRASS, T_DIRT, 2);
    b.invisibleWall(lo.x - 34, lo.z - 34, hi.x + 34, lo.z - 33, 0, 8); b.invisibleWall(lo.x - 34, hi.z + 33, hi.x + 34, hi.z + 34, 0, 8);
    b.invisibleWall(lo.x - 34, lo.z - 34, lo.x - 33, hi.z + 34, 0, 8); b.invisibleWall(hi.x + 33, lo.z - 34, hi.x + 34, hi.z + 34, 0, 8);
    // the road: a smooth ribbon with red-and-white kerbs
    auto quadUp = [&](V3 a, V3 bb, V3 c, V3 d, int tile, float y) {
        a.y = bb.y = c.y = d.y = y;
        auto uv = [](V3 p) { return V2(p.x * 0.35f, p.z * 0.35f); };
        L.decor.tri(a, bb, c, V3(0, 1, 0), uv(a), uv(bb), uv(c), tile);
        L.decor.tri(a, c, d, V3(0, 1, 0), uv(a), uv(c), uv(d), tile);
    };
    for (int i = 0; i < M; i++) {
        V3 p0 = g.track[i], p1 = g.track[(i + 1) % M], p2 = g.track[(i + 2) % M], pm = g.track[(i - 1 + M) % M];
        V3 r0 = rightOf(norm(flatXZ(p1 - pm))), r1 = rightOf(norm(flatXZ(p2 - p0)));
        L.decor.color = hexColor(0x8a8490);
        quadUp(p0 - r0 * hw, p0 + r0 * hw, p1 + r1 * hw, p1 - r1 * hw, T_ASPHALT, 0.04f);
        for (int side = -1; side <= 1; side += 2) {
            L.decor.color = (i / 2) % 2 ? hexColor(0xe03030) : hexColor(0xf0f0f0); L.decor.emis = 0.2f;
            V3 a = p0 + r0 * (hw * side), bb = p0 + r0 * ((hw + 0.6f) * side), c = p1 + r1 * ((hw + 0.6f) * side), d = p1 + r1 * (hw * side);
            if (side < 0) quadUp(bb, a, d, c, T_WHITE, 0.05f); else quadUp(a, bb, c, d, T_WHITE, 0.05f);
            L.decor.emis = 0;
        }
        // centre dashes
        if (i % 3 == 0) {
            L.decor.color = hexColor(0xf0e8c0); L.decor.emis = 0.3f;
            quadUp(p0 - r0 * 0.12f, p0 + r0 * 0.12f, p1 + r1 * 0.12f, p1 - r1 * 0.12f, T_WHITE, 0.055f);
            L.decor.emis = 0;
        }
    }
    L.decor.color = V3(1, 1, 1);
    // the start/finish line: chequered, with a gantry of lights over it
    V3 t0; V3 s0 = tpos(g, 0, &t0); V3 rt0 = rightOf(t0);
    for (int k = 0; k < 12; k++)
        for (int row = 0; row < 2; row++) {
            V3 c = s0 + rt0 * (-hw + (k + 0.5f) * (2 * hw / 12)) + t0 * (row * 0.75f - 0.375f);
            L.decor.color = (k + row) % 2 ? hexColor(0x101010) : hexColor(0xf8f8f8);
            L.decor.cube(M4::translate(c + V3(0, 0.06f, 0)) * M4::rotY(std::atan2(t0.x, t0.z)) * M4::scale(V3(2 * hw / 12, 0.03f, 0.75f)), T_WHITE);
        }
    L.decor.color = V3(1, 1, 1);
    float yaw0 = std::atan2(t0.x, t0.z);
    for (int side = -1; side <= 1; side += 2) b.pillar(s0.x + rt0.x * (hw + 1.4f) * side, s0.z + rt0.z * (hw + 1.4f) * side, 0, 6, 0.45f, T_METAL);
    L.decor.cube(M4::translate(s0 + V3(0, 6.2f, 0)) * M4::rotY(yaw0) * M4::scale(V3(2 * hw + 3.6f, 0.8f, 0.5f)), T_METAL);
    L.decor.emis = 2.2f;
    for (int k = 0; k < 5; k++) { L.decor.color = k < 4 ? hexColor(0xff4030) : hexColor(0x40ff60); L.decor.cube(M4::translate(s0 + rt0 * (-2.4f + k * 1.2f) + V3(0, 6.2f, 0) - t0 * 0.3f) * M4::rotY(yaw0) * M4::scale(V3(0.5f, 0.5f, 0.1f)), T_WHITE); }
    L.decor.emis = 0; L.decor.color = V3(1, 1, 1);
    b.light(s0 + V3(0, 5, 0), 12, hexColor(0xffe0b0), 1.2f);
    // the pit lane / paddock, just outside the start line
    V3 out = dot(rt0, s0) > 0 ? rt0 : rt0 * -1.f;
    V3 pc = s0 + out * (hw + 11) - t0 * 4;
    L.decor.color = hexColor(0xb0a090);
    L.decor.cube(M4::translate(pc + V3(0, 0.03f, 0)) * M4::rotY(yaw0) * M4::scale(V3(14, 0.05f, 16)), T_TILEFLOOR);
    L.decor.color = V3(1, 1, 1);
    float faceTrack = std::atan2(-out.x, -out.z);
    b.spawn(pc + out * 1.5f, faceTrack);                           // 0: the paddock (from the Hollow)
    for (int k = 0; k < 3; k++) b.talker(pc + t0 * (-4.5f + k * 4.5f) - out * 2.5f, faceTrack, 20 + k);
    // the way home
    V3 ex = pc + t0 * 8.5f + out * 2.0f;  // off to the side of the paddock, so it never sits behind the camera
    bool alongX = std::fabs(t0.z) > std::fabs(t0.x);
    b.arch(ex, 3.0f, 4.0f, 1.0f, T_STONEBRICK, alongX);
    b.exit(alongX ? AABB(ex + V3(-1.4f, 0, -0.35f), ex + V3(1.4f, 3.6f, 0.35f)) : AABB(ex + V3(-0.35f, 0, -1.4f), ex + V3(0.35f, 3.6f, 1.4f)), LV_HUB, 10, "", "", "Back to Lumen Hollow");
    b.hint(AABB(pc - V3(7, 1, 7), pc + V3(7, 4, 7)), "THE CANDLE CUP: talk to a sign-up post to race. 3 races per cup, 3 laps each. Boost pads, item boxes ([{INTERACT}] uses an item), and your thrown flame stuns rivals!");
    b.lampPost(pc + t0 * 6 + out * 5, true); b.lampPost(pc - t0 * 7 + out * 3, true);
    // grid slots ahead of the line: rivals in front, Wick at the back
    V3 tg; V3 ps = tpos(g, 5, &tg);
    b.spawn(ps, std::atan2(tg.x, tg.z));                           // 1: Wick's grid slot
    // grandstands by the start straight, full of little cheering lights
    for (int side = -1; side <= 1; side += 2) {
        if (side * dot(rt0, out) > 0) continue;  // the paddock side stays open
        for (int k = 0; k < 3; k++) {
            V3 c = s0 + rt0 * (side * (hw + 4 + k * 1.6f)) + t0 * 6;
            L.decor.cube(M4::translate(c + V3(0, 0.5f + k * 0.8f, 0)) * M4::rotY(yaw0) * M4::scale(V3(1.6f, 1 + k * 1.6f, 22)), T_STONEBRICK);
            for (int q = 0; q < 12; q++) {
                static const uint32_t CC[6] = {0xff6060, 0x60a0ff, 0xffd060, 0x80ff90, 0xd080ff, 0xffffff};
                L.decor.color = hexColor(CC[r.irange(0, 5)]); L.decor.emis = 0.5f;
                L.decor.cube(M4::translate(c + t0 * (-10 + q * 1.8f) + V3(0, 1.4f + k * 1.6f, 0)) * M4::scale(V3(0.5f, 0.9f, 0.5f)), T_CLOTH);
                L.decor.emis = 0; L.decor.color = V3(1, 1, 1);
            }
        }
    }
    // trees, rocks and lamps off the road
    auto clearOfTrack = [&](V3 p, float m) { for (auto& q : g.track) if (distXZ(q, p) < hw + m) return false; return distXZ(p, pc) > 12 && distXZ(p, s0) > 14; };
    for (int k = 0; k < 70; k++) {
        V3 p(r.range(lo.x - 28, hi.x + 28), 0, r.range(lo.z - 28, hi.z + 28));
        if (!clearOfTrack(p, 4)) continue;
        float u = r.f();
        if (u < 0.45f) b.pine(p, r.range(0.8f, 1.4f)); else if (u < 0.8f) b.tree(p, r.range(0.8f, 1.3f)); else b.rock(p + V3(0, 0.3f, 0), r.range(0.8f, 1.6f));
    }
    for (int i = 0; i < M; i += std::max(1, M / 9)) {
        V3 t; V3 p = tpos(g, g.trackS[i], &t);
        V3 lp = p + rightOf(t) * ((i / std::max(1, M / 9)) % 2 ? hw + 2 : -(hw + 2));
        if (distXZ(lp, pc) < 9) continue;
        b.lampPost(lp, true);
    }
    // boost pads, item boxes and tar
    g.rprops.clear();
    float Lt = g.trackLen;
    auto prop = [&](int kind, float s, float lane) { Game::RaceProp p; p.kind = kind; p.s = s; p.lane = lane; V3 t; V3 c = tpos(g, s, &t); p.pos = c + rightOf(t) * lane; p.pos.y = 0; g.rprops.push_back(p); };
    for (int k = 0; k < 5; k++) prop(0, Lt * (0.12f + k * 0.18f) + r.range(-4, 4), r.range(-2.2f, 2.2f));
    for (int k = 0; k < 3; k++) { float s = Lt * (0.22f + k * 0.3f); for (int j = -1; j <= 1; j++) prop(1, s, j * 2.6f); }
    for (int k = 0; k < 3; k++) prop(2, Lt * (0.3f + k * 0.25f) + r.range(-5, 5), r.f() < 0.5f ? -2.6f : 2.6f);
    b.emitter(AABB(V3(lo.x, 0, lo.z), V3(hi.x, 5, hi.z)), 0, 12);
}

// ------------------------------------------------------------------ race flow
float Game::raceMaxSpeed(float normal) const {
    if (lv.id != LV_RACE || rcState != 2 || racers.empty() || racers[0].done) return normal;
    const Racer& me = racers[0];
    float v = 9.4f;
    if (me.boostT > 0) v = 15.5f;
    if (rcOffT > 0) v = std::min(v, me.boostT > 0 ? 9.f : 5.4f);
    for (auto& p : rprops) if (p.kind == 2 && distXZ(p.pos, pl.pos) < 2.0f) v = std::min(v, 4.2f);
    if (me.stunT > 0) v = 2.0f;
    return v;
}

void Game::startCup(int cup) {
    rcCup = cup; rcRace = 0;
    for (int& p : rcPts) p = 0;
    rcPending = true;
    save.raceRuns++;
    writeSave();
    requestTransition(LV_RACE, 1);
}

static void beginRace(Game& g) {
    g.racers.clear();
    for (int k = 0; k < NRACERS; k++) {
        Game::Racer r;
        if (k == 0) { r.name = "WICK"; r.color = g.save.dye; }
        else { r.name = RIVAL[k - 1]; r.color = RIVAL_DYE[k - 1]; r.wax = (int)RIVAL_WAX[k - 1]; r.skill = RIVAL_SKILL[k - 1] + rrng.range(-0.1f, 0.1f); }
        // grid: rivals in the front four slots, Wick at the back
        int slot = k == 0 ? 4 : k - 1;
        r.s = 14 - (slot / 2) * 4.5f; r.lane = slot == 4 ? 0 : (slot % 2 ? 1.8f : -1.8f); r.laneTarget = r.lane;
        V3 t; r.pos = tpos(g, r.s, &t) + rightOf(t) * r.lane; r.yaw = std::atan2(t.x, t.z);
        r.itemT = rrng.range(2, 5);
        g.racers.push_back(r);
    }
    g.rcIdx = nearestIdx(g, g.pl.pos, -1);
    g.rcState = 1; g.rcT = 0; g.rcOffT = 0; g.rcWrongT = 0; g.rcEndT = 0;
    g.rbolts.clear();
    char b[96]; snprintf(b, sizeof(b), "%s  -  RACE %d of %d", CUP_NAME[g.rcCup], g.rcRace + 1, RACES);
    g.showToast(b, "3 laps. Boost pads, item boxes ([" + bindingName(g.settings.keys[A_INTERACT]) + "]), and throw your flame to stun rivals!");
    audio::setSong(SONG_BOSS);
}

static int placeOf(const Game& g, int who) {
    // finished racers by finish order, then everyone else by distance covered
    const Game::Racer& me = g.racers[who];
    if (me.done) return me.place;
    int p = 1;
    for (int k = 0; k < (int)g.racers.size(); k++) {
        if (k == who) continue;
        const Game::Racer& o = g.racers[k];
        if (o.done || progress(g, o) > progress(g, me)) p++;
    }
    return p;
}

static void hitRacer(Game& g, int k, float stun, const char* by) {
    Game::Racer& r = g.racers[k];
    if (r.shieldT > 0) { r.shieldT = 0; g.burst(r.pos + V3(0, 1, 0), hexColor(0x80d0ff), 20, 4, 0.1f, 3, 0.5f, 0); audio::playAt(SFX_SHIELD, r.pos, 1, 1.2f); return; }
    r.stunT = std::max(r.stunT, stun); r.boostT = 0;
    g.burst(r.pos + V3(0, 1, 0), hexColor(0xffe060), 18, 5, 0.08f, 3, 0.5f, 2);
    audio::playAt(SFX_HIT, r.pos, 1, 0.9f);
    if (k == 0) { g.statusMsg = std::string("ZAPPED") + (by ? std::string(" by ") + by : "") + "!"; g.statusT = 1.3f; g.addShake(0.2f); }
}

static void useItem(Game& g, int k) {
    Game::Racer& r = g.racers[k];
    int it = r.item; r.item = -1;
    if (it < 0) return;
    if (k == 0) audio::play(SFX_MENU_SELECT, 0.8f, 1.3f);
    switch (it) {
        case 0: r.boostT = 2.4f; audio::playAt(SFX_DASH, r.pos, 1, 0.8f); break;
        case 1: {  // a homing spark at whoever is just ahead (or the leader, if you're leading: nobody)
            int myPlace = placeOf(g, k), target = -1;
            for (int o = 0; o < (int)g.racers.size(); o++) if (o != k && !g.racers[o].done && placeOf(g, o) == myPlace - 1) target = o;
            if (target < 0) for (int o = 0; o < (int)g.racers.size(); o++) if (o != k && !g.racers[o].done && placeOf(g, o) == myPlace + 1) target = o;
            if (target >= 0) { Game::Bolt2 b; b.pos = r.pos + V3(0, 1.2f, 0); b.target = target; g.rbolts.push_back(b); audio::playAt(SFX_BLIP, r.pos, 1, 1.8f); }
        } break;
        case 2: {
            Game::RaceProp p; p.kind = 3; p.s = r.s - 2.5f; p.lane = r.lane; V3 t; p.pos = tpos(g, p.s, &t) + rightOf(t) * p.lane; p.pos.y = 0; p.t = 25;
            g.rprops.push_back(p);
            audio::playAt(SFX_SPLASH, r.pos, 0.7f, 1.5f);
        } break;
        case 3: r.shieldT = 9; audio::playAt(SFX_SHIELD, r.pos, 1, 1.5f); break;
    }
}

static int rollItem(Game& g, int k) {
    int place = placeOf(g, k);
    float u = rrng.f();
    if (place >= 4) return u < 0.45f ? 0 : (u < 0.8f ? 1 : (u < 0.9f ? 3 : 2));
    if (place == 1) return u < 0.15f ? 0 : (u < 0.35f ? 1 : (u < 0.7f ? 2 : 3));
    return u < 0.3f ? 0 : (u < 0.6f ? 1 : (u < 0.8f ? 2 : 3));
}

static void finishRacer(Game& g, int k) {
    Game::Racer& r = g.racers[k];
    if (r.done) return;
    int n = 0; for (auto& o : g.racers) if (o.done) n++;
    r.done = true; r.place = n + 1; r.doneT = g.rcT;
}

static void raceResults(Game& g) {
    // settle the order: anyone not finished is placed by how far they got
    std::vector<int> order;
    for (int k = 0; k < NRACERS; k++) order.push_back(k);
    std::sort(order.begin(), order.end(), [&](int a, int b) {
        const auto &A = g.racers[a], &B = g.racers[b];
        if (A.done != B.done) return A.done;
        if (A.done) return A.place < B.place;
        return progress(g, A) > progress(g, B);
    });
    for (int i = 0; i < NRACERS; i++) { g.racers[order[i]].place = i + 1; g.racers[order[i]].done = true; g.rcPts[order[i]] += PTS[i]; }
    int place = g.racers[0].place;
    static const int EMB[5] = {60, 40, 25, 15, 10};
    g.save.embers += EMB[place - 1];
    if (place == 1) g.save.raceWins++;
    if (place <= 2) g.bountyEvent(8, 1);
    g.gainXp(20 + (5 - place) * 10, g.pl.pos);
    char t[64]; snprintf(t, sizeof(t), "%d:%04.1f", (int)(g.racers[0].doneT / 60), std::fmod(g.racers[0].doneT, 60.f));
    g.showToast(place == 1 ? "YOU WIN!" : std::string(PLACE[place - 1]) + " PLACE", std::string("time ") + t + "   +" + std::to_string(EMB[place - 1]) + " embers   +" + std::to_string(PTS[place - 1]) + " cup points");
    audio::play(place <= 3 ? SFX_CHECKPOINT : SFX_MENU_BACK);
    if (place == 1) audio::play(SFX_BELL, 1, 1.4f);
    g.rcState = 3; g.rcEndT = 0;
    g.writeSave();
}

static void endCup(Game& g) {
    std::vector<int> order = {0, 1, 2, 3, 4};
    std::stable_sort(order.begin(), order.end(), [&](int a, int b) { return g.rcPts[a] > g.rcPts[b]; });
    int place = 1; for (int i = 0; i < 5; i++) if (order[i] == 0) place = i + 1;
    // ties go to Wick
    for (int i = 0; i < 5; i++) if (order[i] != 0 && g.rcPts[order[i]] == g.rcPts[0]) { place = std::min(place, i + 1); }
    int medal = place <= 3 ? 4 - place : 0;
    int c = g.rcCup;
    bool better = medal > g.save.cupBest[c];
    if (better) g.save.cupBest[c] = medal;
    static const int CUPEMB[3] = {120, 220, 400};
    int emb = medal ? CUPEMB[c] * medal / 3 : 0;
    g.save.embers += emb;
    if (medal == 3) { g.save.treats += 3; g.gainXp(100 + 50 * c, g.pl.pos); } else if (medal) g.gainXp(40 + 20 * c, g.pl.pos);
    std::vector<DialogLine> L;
    std::string standings;
    for (int i = 0; i < 5; i++) standings += std::string(i ? ",  " : "") + PLACE[i] + " " + (order[i] == 0 ? "WICK" : RIVAL[order[i] - 1]) + " " + std::to_string(g.rcPts[order[i]]);
    static const char* MED[4] = {"no trophy", "BRONZE", "SILVER", "GOLD"};
    L.push_back({"MARSHAL", std::string("That's the ") + CUP_NAME[c] + "! Final points: " + standings + "."});
    if (medal) L.push_back({"MARSHAL", std::string(PLACE[place - 1]) + " overall - a " + MED[medal] + " trophy!" + (emb ? " +" + std::to_string(emb) + " embers." : "") + (medal == 3 ? " And 3 pet treats." : "")});
    else L.push_back({"MARSHAL", std::string(PLACE[place - 1]) + " overall. No trophy this time - the podium is top three. Have another go!"});
    if (medal && c < 2 && better) L.push_back({"MARSHAL", std::string("The ") + CUP_NAME[c + 1] + " is open to you now. Faster rivals, meaner tracks."});
    bool helmet = medal == 3 && c == 2 && !g.has("hat.15");
    if (helmet) L.push_back({"MARSHAL", "GOLD in the Inferno Cup?! Nobody's done that. Here - the champion's RACING HELMET."});
    g.rcState = 0;
    g.startDialog(L, [&g, helmet]() {
        if (helmet) { g.setFlag("hat.15"); g.save.hat = 15; g.showToast(std::string("NEW HAT: ") + hatName(15), "Swap hats at the mirror in Marrow's workshop"); audio::play(SFX_SHARD, 1, 1.2f); }
        g.writeSave();
        g.requestTransition(LV_RACE, 0);
    });
    g.writeSave();
}

void Game::updateRace(float dt, const InputState& in) {
    if (lv.id != LV_RACE) { if (rcState) { rcState = 0; racers.clear(); } return; }
    if (rcPending) { rcPending = false; beginRace(*this); }
    if (rcState == 0 || racers.empty() || !dialog.empty()) return;
    rcT += dt;
    if (rcState == 1) {
        int before = (int)(rcT - dt), now = (int)rcT;
        if (now != before && now <= 3) audio::play(now == 3 ? SFX_BELL : SFX_TICK, 1, now == 3 ? 1.5f : 1.0f);
        if (rcT >= 3) { rcState = 2; rcT = 0; statusMsg = "GO!"; statusT = 1; }
        // stay on the grid
        pl.vel = V3(0, pl.vel.y, 0);
        return;
    }
    int M = (int)track.size();
    // --- Wick
    Racer& me = racers[0];
    {
        int prevIdx = rcIdx;
        rcIdx = nearestIdx(*this, pl.pos, rcIdx, 12);
        float s, lat; projectOn(*this, pl.pos, rcIdx, s, lat);
        // checkpoints at a quarter, half and three quarters: no cutting across the meadow
        int q = rcIdx * 4 / M;
        if (q == 1 && !(me.cp & 1)) me.cp |= 1;
        if (q == 2 && (me.cp & 1)) me.cp |= 2;
        if (q == 3 && (me.cp & 2)) me.cp |= 4;
        if (!me.done && prevIdx > M * 3 / 4 && rcIdx < M / 4) {
            if (me.cp == 7) {
                me.lap++; me.cp = 0;
                if (me.lap >= LAPS) { finishRacer(*this, 0); me.doneT = rcT; }
                else { showToast(me.lap == LAPS - 1 ? "FINAL LAP!" : "LAP " + std::to_string(me.lap + 1) + "/" + std::to_string(LAPS), ""); audio::play(SFX_BELL, 0.8f, me.lap == LAPS - 1 ? 1.7f : 1.3f); toastT = 1.6f; }
            } else if (statusT <= 0) { statusMsg = "You skipped part of the track - that lap doesn't count!"; statusT = 2.5f; }
        }
        me.s = s; me.pos = pl.pos;
        float off = std::fabs(lat) - trackHw;
        rcOffT = off > 0.2f ? 0.2f : std::max(0.f, rcOffT - dt);
        if (off > 14 && !pl.dead) {  // way out in the meadow: back onto the road
            V3 t; V3 c = tpos(*this, s, &t);
            pl.pos = c + V3(0, 0.2f, 0); pl.vel = V3(); pl.yaw = std::atan2(t.x, t.z); me.stunT = 0.6f;
            statusMsg = "BACK ON TRACK"; statusT = 1.2f;
        }
        V3 t; tpos(*this, s, &t);
        V3 hv = flatXZ(pl.vel);
        if (len(hv) > 3 && dot(norm(hv), t) < -0.5f) rcWrongT += dt; else rcWrongT = std::max(0.f, rcWrongT - dt * 2);
        me.stunT = std::max(0.f, me.stunT - dt); me.boostT = std::max(0.f, me.boostT - dt); me.shieldT = std::max(0.f, me.shieldT - dt);
        if (in.pressed[A_INTERACT] && me.item >= 0 && !me.done) useItem(*this, 0);
        if (me.stunT > 0 && fxTick(0.1f)) burst(pl.pos + V3(0, 1.6f, 0), hexColor(0xffe060), 2, 2, 0.06f, 3, 0.4f, 0);
        if (me.boostT > 0 && fxTick(0.04f)) burst(pl.pos + V3(0, 0.5f, 0), hexColor(0x60e0ff), 2, 1, 0.12f, 1, 0.3f, 0);
    }
    // --- the rivals
    float base = CUP_SPEED[rcCup];
    float myProg = progress(*this, me);
    for (int k = 1; k < (int)racers.size(); k++) {
        Racer& r = racers[k];
        r.stunT = std::max(0.f, r.stunT - dt); r.boostT = std::max(0.f, r.boostT - dt); r.shieldT = std::max(0.f, r.shieldT - dt); r.boxCd = std::max(0.f, r.boxCd - dt);
        float v = base + r.skill;
        float d = myProg - progress(*this, r);
        if (!me.done) {
            if (d > 30) v += 1.5f; else if (d > 12) v += 0.6f;
            static const float SLOW[3] = {1.7f, 1.1f, 0.5f};
            if (d < -30) v -= SLOW[rcCup]; else if (d < -14) v -= SLOW[rcCup] * 0.4f;
        }
        // ease off in tight corners
        V3 ta, tb; tpos(*this, r.s, &ta); tpos(*this, r.s + 7, &tb);
        float bend = std::acos(clampf(dot(ta, tb), -1, 1));
        if (bend > 0.4f) v *= 0.92f;
        if (r.boostT > 0) v = base + 5.5f;
        for (auto& p : rprops) if (p.kind == 2 && std::fabs(p.s - r.s) < 1.6f && std::fabs(p.lane - r.lane) < 1.8f) v = std::min(v, 4.2f);
        if (r.stunT > 0) v = 1.2f;
        if (r.done) v = 4.5f;
        r.speed += clampf(v - r.speed, -14 * dt, 7 * dt);
        float s0 = r.s;
        r.s += r.speed * dt;
        if (r.s >= trackLen) {
            r.s -= trackLen; r.lap++;
            if (r.lap >= LAPS && !r.done) finishRacer(*this, k);
        }
        // lanes: wander a little, steer round tar and slicks ahead
        r.itemT -= dt;
        if (rrng.f() < dt * 0.25f) r.laneTarget = rrng.range(-trackHw + 1.3f, trackHw - 1.3f);
        for (auto& p : rprops) if ((p.kind == 2 || (p.kind == 3 && p.alive)) && p.s - r.s > 0 && p.s - r.s < 12 && std::fabs(p.lane - r.lane) < 2.0f)
            r.laneTarget = clampf(p.lane + (p.lane > 0 ? -3.2f : 3.2f), -trackHw + 1, trackHw - 1);
        r.lane += clampf(r.laneTarget - r.lane, -2.4f * dt, 2.4f * dt);
        // boost pads, item boxes, slicks
        for (auto& p : rprops) {
            bool crossed = (s0 <= p.s && r.s >= p.s) || (s0 > r.s && (p.s >= s0 || p.s <= r.s));
            if (!crossed || std::fabs(p.lane - r.lane) > 1.6f) continue;
            if (p.kind == 0) { r.boostT = 1.3f; }
            if (p.kind == 1 && p.alive && r.item < 0 && !r.done) { r.item = rollItem(*this, k); r.itemT = rrng.range(1.0f, 4.0f); p.alive = false; p.cd = 3; }
            if (p.kind == 3 && p.alive) { p.alive = false; hitRacer(*this, k, 1.1f, nullptr); }
        }
        if (r.item >= 0 && r.itemT <= 0 && !r.done) useItem(*this, k);
        V3 t; r.pos = tpos(*this, r.s, &t) + rightOf(t) * r.lane; r.pos.y = 0;
        r.yaw = std::atan2(t.x, t.z);
        if (r.stunT > 0) r.yaw += rcT * 14;
        r.hopT += dt * r.speed;
        // bumping into Wick: dash into a rival to knock it off balance
        float dd = distXZ(r.pos, pl.pos);
        if (dd < 0.8f && dd > 0.001f && pl.pos.y < 1.2f) {
            V3 push = norm(flatXZ(pl.pos - r.pos));
            pl.pos += push * (0.8f - dd);
            if (pl.dashT > 0 && r.stunT <= 0) hitRacer(*this, k, 0.7f, nullptr);
        }
        // Wick's thrown flame stuns
        if ((flame.st == F_FLYING || flame.st == F_HOVER) && len(flame.pos - (r.pos + V3(0, 0.8f, 0))) < 1.1f && r.stunT <= 0) { hitRacer(*this, k, 1.0f, nullptr); recallFlame(); }
    }
    // Wick's own props: pads, boxes, slicks
    for (auto& p : rprops) {
        if (p.kind == 1 && !p.alive) { p.cd -= dt; if (p.cd <= 0) p.alive = true; }
        if (p.kind == 3 && p.t > 0) { p.t -= dt; if (p.t <= 0) p.alive = false; }
        if (me.done || pl.pos.y > 1.5f || distXZ(p.pos, pl.pos) > 1.5f) continue;
        if (p.kind == 0) { if (me.boostT < 1.0f) audio::play(SFX_DASH, 0.8f, 1.4f); me.boostT = std::max(me.boostT, 1.4f); }
        if (p.kind == 1 && p.alive && me.item < 0) { me.item = rollItem(*this, 0); p.alive = false; p.cd = 3; audio::play(SFX_SHARD, 0.7f, 1.6f); statusMsg = std::string("Got a ") + ITEM_NAME[me.item] + "! [" + bindingName(settings.keys[A_INTERACT]) + "] to use"; statusT = 2; }
        if (p.kind == 3 && p.alive) { p.alive = false; hitRacer(*this, 0, 1.0f, nullptr); }
    }
    // spark bolts
    for (auto& b : rbolts) {
        if (b.target < 0 || b.target >= (int)racers.size()) { b.life = 0; continue; }
        V3 to = racers[b.target].pos + V3(0, 0.9f, 0) - b.pos;
        float l = len(to);
        b.life -= dt;
        if (l < 0.9f) { hitRacer(*this, b.target, 1.4f, nullptr); b.life = 0; continue; }
        b.pos += to / l * std::min(l, 32 * dt);
    }
    rbolts.erase(std::remove_if(rbolts.begin(), rbolts.end(), [](const Bolt2& b) { return b.life <= 0; }), rbolts.end());
    for (int k = 0; k < (int)racers.size(); k++) if (!racers[k].done) racers[k].place = placeOf(*this, k);
    // the end of a race
    if (rcState == 2) {
        bool rivalsDone = true; for (int k = 1; k < (int)racers.size(); k++) if (!racers[k].done) rivalsDone = false;
        if (rivalsDone && !me.done) { rcEndT += dt; if (rcEndT > 25) { me.doneT = rcT; raceResults(*this); return; } }
        if (me.done) { raceResults(*this); return; }
    }
    if (rcState == 3) {
        rcEndT += dt;
        if (rcEndT > 6) {
            if (rcRace < RACES - 1) { rcRace++; rcPending = true; save.raceRuns++; requestTransition(LV_RACE, 1); rcState = 0; }
            else endCup(*this);
        }
    }
}

// the sign-up posts in the paddock
bool talkRace(Game& g, int ti) {
    int who = g.lv.talkers[ti].who;
    if (who < 20 || who > 22) return false;
    int c = who - 20;
    if (c > 0 && g.save.cupBest[c - 1] == 0) {
        g.startDialog({{"MARSHAL", std::string("The ") + CUP_NAME[c] + " is for proven racers. Get on the podium in the " + CUP_NAME[c - 1] + " first!"}});
        return true;
    }
    static const char* MED[4] = {"none yet", "BRONZE", "SILVER", "GOLD"};
    static const char* DESC[3] = {"Easy-going rivals. A good place to learn the tricks.", "Faster rivals that fight back with items.", "The fastest candles alive. Every boost pad counts."};
    std::string lines = std::string(DESC[c]) + " Your best: " + MED[g.save.cupBest[c]] + ".";
    g.startDialog({{"MARSHAL", std::string("The ") + CUP_NAME[c] + "! " + lines},
                   {"MARSHAL", "Three races, three laps each, a new track every time. Points for every finish: 10, 7, 5, 3, 1. Top three overall takes a trophy."},
                   {"MARSHAL", "Racers, to the grid!"}},
                  [&g, c]() { g.startCup(c); });
    return true;
}

// ------------------------------------------------------------------ drawing
static M4 TRS(V3 t, float yaw, V3 s) { return M4::translate(t) * M4::rotY(yaw) * M4::scale(s); }
static void flame3(Renderer* R, V3 p, float sc, float t, int dye) {
    V3 o, m, c; dyeColors(dye, t, o, m, c);
    float f1 = std::sin(t * 13) * 0.08f, f2 = std::sin(t * 17 + 1) * 0.06f;
    R->drawPart(PM_OCTA, TRS(p + V3(0, 0.12f * sc, 0), t * 2, V3(0.42f + f1, 0.75f + f2, 0.42f + f1) * sc), T_WHITE, o, 2.2f);
    R->drawPart(PM_OCTA, TRS(p + V3(0, 0.1f * sc, 0), -t * 3, V3(0.28f, 0.52f + f1, 0.28f) * sc), T_WHITE, m, 2.6f);
    R->drawPart(PM_OCTA, TRS(p + V3(0, 0.05f * sc, 0), t * 4, V3(0.15f, 0.28f, 0.15f) * sc), T_WHITE, c, 3.0f);
}

// a rival candle (or a sign-up post's little figure)
void drawCandleFolk(Renderer* R, V3 pos, float yaw, float walk, float speed, uint32_t waxCol, int dye, float t, float flash) {
    M4 b = M4::translate(pos) * M4::rotY(yaw);
    V3 wax = hexColor(waxCol);
    float w = std::sin(walk * 2.2f), k = std::min(1.f, speed / 4);
    float bob = std::fabs(w) * 0.05f * k;
    for (int s = -1; s <= 1; s += 2)
        R->drawPart(PM_CUBE, b * M4::translate(V3(0.12f * s, 0.18f, 0)) * M4::rotX(w * s * 0.6f * k) * M4::translate(V3(0, -0.08f, 0)) * M4::scale(V3(0.13f, 0.3f, 0.14f)), T_WAX, wax * 0.85f, 0, flash);
    R->drawPart(PM_CYL, b * M4::translate(V3(0, 0.55f + bob, 0)) * M4::scale(V3(0.5f, 0.55f, 0.46f)), T_WAX, wax, 0, flash);
    for (int s = -1; s <= 1; s += 2)
        R->drawPart(PM_CUBE, b * M4::translate(V3(0.29f * s, 0.72f + bob, 0)) * M4::rotX(-w * s * 0.7f * k - 0.3f * k) * M4::translate(V3(0, -0.13f, 0)) * M4::scale(V3(0.1f, 0.28f, 0.1f)), T_WAX, wax * 0.92f, 0, flash);
    R->drawPart(PM_CYL, b * M4::translate(V3(0, 0.98f + bob, 0)) * M4::scale(V3(0.42f, 0.3f, 0.42f)), T_WAX, wax * 1.02f, 0, flash);
    for (int s = -1; s <= 1; s += 2) R->drawPart(PM_CUBE, b * M4::translate(V3(0.09f * s, 1.0f + bob, 0.2f)) * M4::scale(V3(0.06f, 0.09f, 0.03f)), T_WHITE, hexColor(0x1a1018));
    // a racing number band
    R->drawPart(PM_CYL, b * M4::translate(V3(0, 0.62f + bob, 0)) * M4::scale(V3(0.52f, 0.1f, 0.48f)), T_CLOTH, hexColor(0x202030));
    R->drawPart(PM_CUBE, b * M4::translate(V3(0, 1.2f + bob, 0)) * M4::scale(V3(0.04f, 0.12f, 0.04f)), T_WHITE, hexColor(0xff5020), 1.5f);
    flame3(R, pos + V3(0, 1.32f + bob, 0), 1.0f, t + waxCol % 7, dye);
}

void drawRaceWorld(Game& g, Renderer* R) {
    if (g.lv.id != LV_RACE) return;
    float t = g.time;
    // props
    for (auto& p : g.rprops) {
        V3 tg; tpos(g, p.s, &tg); float yaw = std::atan2(tg.x, tg.z);
        switch (p.kind) {
            case 0:  // boost pad: glowing chevrons
                if (R->shadowPass) break;
                R->drawPart(PM_CUBE, TRS(p.pos + V3(0, 0.07f, 0), yaw, V3(2.4f, 0.04f, 2.6f)), T_METAL, hexColor(0x204060), 0.2f);
                for (int k = 0; k < 3; k++) {
                    float a = std::fmod(t * 2.5f + k / 3.f, 1.f);
                    for (int s = -1; s <= 1; s += 2)
                        R->drawPart(PM_CUBE, M4::translate(p.pos + tg * (-0.9f + k * 0.9f) + V3(0, 0.1f, 0)) * M4::rotY(yaw + 0.7f * s) * M4::translate(V3(0.45f * s, 0, 0)) * M4::scale(V3(0.18f, 0.03f, 1.0f)), T_WHITE, hexColor(0x60f0ff), 1.5f + 1.5f * a);
                }
                break;
            case 1:  // item box: a spinning rainbow cube
                if (!p.alive) break;
                {
                    float h = std::fmod(t * 0.3f + p.lane * 0.1f, 1.f);
                    V3 col(0.5f + 0.5f * std::sin(h * TAU), 0.5f + 0.5f * std::sin(h * TAU + 2.1f), 0.5f + 0.5f * std::sin(h * TAU + 4.2f));
                    V3 c = p.pos + V3(0, 1.0f + 0.15f * std::sin(t * 3 + p.s), 0);
                    R->drawPart(PM_CUBE, M4::translate(c) * M4::rotY(t * 1.5f) * M4::rotX(0.6f) * M4::scale(V3(0.8f, 0.8f, 0.8f)), T_WHITE, col, 1.2f, 0, 0.75f);
                    R->drawPart(PM_OCTA, M4::translate(c) * M4::rotY(-t * 3) * M4::scale(V3(0.3f, 0.45f, 0.3f)), T_WHITE, V3(1, 1, 1), 2.5f);
                }
                break;
            case 2:  // tar
                R->drawPart(PM_CYL, TRS(p.pos + V3(0, 0.07f, 0), p.s, V3(3.0f, 0.03f, 2.4f)), T_TAR, V3(0.6f, 0.6f, 0.6f), 0.1f);
                break;
            case 3:  // a wax slick someone dropped
                if (!p.alive) break;
                R->drawPart(PM_CYL, TRS(p.pos + V3(0, 0.08f, 0), p.s, V3(1.8f, 0.03f, 1.5f)), T_WAX, hexColor(0xfff4d0), 0.4f);
                break;
        }
    }
    // the rivals
    for (int k = 1; k < (int)g.racers.size(); k++) {
        auto& r = g.racers[k];
        float hop = std::fabs(std::sin(r.hopT * 0.5f)) * (r.speed > 3 ? 0.12f : 0);
        drawCandleFolk(R, r.pos + V3(0, hop, 0), r.yaw, r.hopT * 0.9f, r.speed, RIVAL_WAX[k - 1], r.color, t, r.stunT > 0 ? 0.3f : 0);
        if (R->shadowPass) continue;
        if (r.shieldT > 0) R->drawPart(PM_SPHERE, M4::translate(r.pos + V3(0, 0.9f, 0)) * M4::scale(V3(1.7f, 2.0f, 1.7f)), T_WHITE, hexColor(0x80d0ff), 1.0f, 0, 0.25f);
        if (r.stunT > 0) for (int s = 0; s < 3; s++) { float a = t * 6 + s * TAU / 3; R->drawPart(PM_OCTA, M4::translate(r.pos + V3(std::sin(a) * 0.5f, 1.9f, std::cos(a) * 0.5f)) * M4::scale(V3(0.12f, 0.12f, 0.12f)), T_WHITE, hexColor(0xffe060), 2.5f); }
        if (r.boostT > 0) R->drawPart(PM_CONE, M4::translate(r.pos + V3(0, 0.6f, 0) - V3(std::sin(r.yaw), 0, std::cos(r.yaw)) * 0.8f) * M4::rotY(r.yaw) * M4::rotX(-PI / 2) * M4::scale(V3(0.5f, 1.2f, 0.5f)), T_WHITE, hexColor(0x60e0ff), 2.0f, 0, 0.5f);
    }
    if (!g.racers.empty() && !R->shadowPass && g.racers[0].shieldT > 0) R->drawPart(PM_SPHERE, M4::translate(g.pl.pos + V3(0, 0.9f, 0)) * M4::scale(V3(1.7f, 2.0f, 1.7f)), T_WHITE, hexColor(0x80d0ff), 1.0f, 0, 0.25f);
    for (auto& b : g.rbolts) if (!R->shadowPass) R->drawPart(PM_OCTA, M4::translate(b.pos) * M4::rotY(t * 20) * M4::scale(V3(0.35f, 0.35f, 0.35f)), T_WHITE, hexColor(0xfff060), 3.0f);
    // the sign-up posts: a flag on a pole, in the cup's colour, with a little marshal candle
    for (auto& tk : g.lv.talkers) if (tk.who >= 20 && tk.who <= 22) {
        static const uint32_t CUPCOL[3] = {0xff8a30, 0xffd060, 0xff3040};
        int c = tk.who - 20;
        bool open = c == 0 || g.save.cupBest[c - 1] > 0;
        R->drawPart(PM_CYL, M4::translate(tk.pos + V3(0, 1.6f, 0)) * M4::scale(V3(0.12f, 3.2f, 0.12f)), T_METAL, V3(1, 1, 1));
        R->drawPart(PM_CUBE, M4::translate(tk.pos + V3(0, 2.8f, 0)) * M4::rotY(tk.yaw + 0.2f * std::sin(t * 2 + c)) * M4::translate(V3(0.55f, 0, 0)) * M4::scale(V3(1.1f, 0.7f, 0.04f)), T_CLOTH, open ? hexColor(CUPCOL[c]) : hexColor(0x505060), open ? 0.6f : 0);
        for (int m = 0; m < 3; m++) if (g.save.cupBest[c] > m) R->drawPart(PM_OCTA, M4::translate(tk.pos + V3(-0.35f + m * 0.35f, 3.6f, 0)) * M4::rotY(t * 2) * M4::scale(V3(0.15f, 0.2f, 0.15f)), T_GOLD, V3(1, 1, 1), 1.0f);
        R->drawPart(PM_CUBE, M4::translate(tk.pos + V3(0, 0.4f, 0)) * M4::rotY(tk.yaw) * M4::scale(V3(1.0f, 0.8f, 0.6f)), T_PLANK, V3(1, 1, 1));
    }
}

void raceLights(Game& g, Renderer* R) {
    if (g.lv.id != LV_RACE) return;
    for (int k = 1; k < (int)g.racers.size(); k++) { V3 o, m, c; dyeColors(g.racers[k].color, g.time, o, m, c); R->lights.push_back({g.racers[k].pos + V3(0, 1.5f, 0), 5, m, 1.0f}); }
    for (auto& p : g.rprops) if (p.kind == 0 && distXZ(p.pos, g.pl.pos) < 40) R->lights.push_back({p.pos + V3(0, 0.6f, 0), 4, hexColor(0x60f0ff), 0.8f});
}

// ------------------------------------------------------------------ HUD
void drawRaceHUD(Game& g) {
    if (g.lv.id != LV_RACE || g.racers.empty() || g.rcState == 0) return;
    Renderer* R = g.R;
    float s = (float)R->uiScale();
    float W = (float)R->winW, H = (float)R->winH;
    auto& me = g.racers[0];
    // name tags over the rivals
    for (int k = 1; k < (int)g.racers.size(); k++) {
        float sx, sy;
        if (distXZ(g.racers[k].pos, g.pl.pos) < 30 && R->project(g.racers[k].pos + V3(0, 2.4f, 0), sx, sy)) {
            std::string n = std::string(PLACE[g.racers[k].place - 1]) + " " + g.racers[k].name;
            V3 o, m, c; dyeColors(g.racers[k].color, 0, o, m, c);
            R->uiTextCentered(sx, sy, n, s, V4(m.x, m.y, m.z, 0.9f));
        }
    }
    if (g.rcState == 1) {
        int n = 3 - (int)g.rcT;
        R->uiTextCentered(W / 2, H * 0.35f, n > 0 ? std::to_string(n) : "GO!", s * 6, n > 0 ? C4(0xff5040) : C4(0x60ff70));
        return;
    }
    // lap and time
    char b[96];
    int lapShown = std::min(me.lap + 1, LAPS);
    snprintf(b, sizeof(b), "%s  RACE %d/%d   LAP %d/%d   %d:%04.1f", CUP_NAME[g.rcCup], g.rcRace + 1, RACES, lapShown, LAPS, (int)(g.rcT / 60), std::fmod(g.rcT, 60.f));
    R->uiTextCentered(W / 2, 34 * s, b, s * 1.2f, COL_HI);
    // place, big, on the right
    int place = me.place > 0 ? me.place : 5;
    static const uint32_t PC[5] = {0xffd040, 0xd0d8e0, 0xd08040, 0xa0a0b0, 0x908090};
    float pw = R->uiTextWidth(PLACE[place - 1], s * 4), sw = R->uiTextWidth("/5", s * 1.5f);
    R->uiText(W - pw - sw - 12 * s, 40 * s, PLACE[place - 1], s * 4, C4(PC[place - 1]));
    R->uiText(W - sw - 8 * s, 54 * s, "/5", s * 1.5f, COL_DIM);
    // standings
    std::vector<int> order; for (int k = 0; k < (int)g.racers.size(); k++) order.push_back(k);
    std::sort(order.begin(), order.end(), [&](int a, int bb) { return g.racers[a].place < g.racers[bb].place; });
    float y = 60 * s;
    for (int i = 0; i < (int)order.size(); i++) {
        auto& r = g.racers[order[i]];
        V3 o, m, c; dyeColors(r.color, 0, o, m, c);
        R->uiText(8 * s, y, std::string(PLACE[i]) + "  " + r.name + (g.rcState == 3 ? "  +" + std::to_string(PTS[i]) : ""), s * (order[i] == 0 ? 1.3f : 1.1f), order[i] == 0 ? COL_HI : V4(m.x, m.y, m.z, 1));
        y += 11 * s;
    }
    y += 4 * s;
    R->uiText(8 * s, y, "cup points:", s, COL_DIM); y += 9 * s;
    for (int k = 0; k < NRACERS; k++) { R->uiText(8 * s, y, (k == 0 ? std::string("WICK") : std::string(RIVAL[k - 1])) + " " + std::to_string(g.rcPts[k]), s, k == 0 ? COL_HI : COL_DIM); y += 8 * s; }
    // item slot
    float bx = W / 2 - 60 * s, by = H - 46 * s;
    R->uiFrame(bx, by, 120 * s, 16 * s, C4(0x0c0a14, 0.8f), me.item >= 0 ? COL_HI : C4(0x4a3a30));
    R->uiTextCentered(W / 2, by + 5 * s, me.item >= 0 ? "[" + bindingName(g.settings.keys[A_INTERACT]) + "] " + ITEM_NAME[me.item] : std::string("no item - hit a ? box"), s, me.item >= 0 ? COL_HI : COL_DIM);
    if (g.rcWrongT > 0.8f && std::fmod(g.time * 3, 1.f) < 0.6f) R->uiTextCentered(W / 2, H * 0.3f, "WRONG WAY!", s * 3, C4(0xff4040));
    else if (g.rcOffT > 0 && g.rcState == 2) R->uiTextCentered(W / 2, H * 0.3f, "OFF TRACK - slow!", s * 1.6f, C4(0xffa040, 0.8f));
    if (me.boostT > 0) R->uiTextCentered(W / 2, H * 0.36f, "BOOST!", s * 2, C4(0x60f0ff));
    if (g.rcState == 3) R->uiTextCentered(W / 2, H * 0.62f, g.rcRace < RACES - 1 ? "next race in " + std::to_string(std::max(0, (int)std::ceil(6 - g.rcEndT))) + "..." : std::string("cup results coming up..."), s * 1.3f, COL_TEXT);
}
