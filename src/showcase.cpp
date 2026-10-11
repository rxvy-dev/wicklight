// WICKLIGHT - showcase recorder: scripted, cinematic shots of Wick's Island, rendered frame by frame
// at a fixed 60 fps with the game's own synthesized music and sound mixed offline.
//   wicklight --showcase OUT [--width W --height H]   -> OUT.mp4 (video, piped to ffmpeg) + OUT.f32 (stereo float audio)
//   wicklight --survey SEED                           -> ASCII map of an island seed (for planning shots)
#include "game.h"
#include "platform.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <functional>

int surveyIsland(uint32_t seed) {
    VoxWorld w;
    w.generate(seed);
    printf("seed %u   x -> (%d..%d), z down (%d..%d)\n", seed, VoxWorld::X0, VoxWorld::X0 + VoxWorld::NX - 1, VoxWorld::Z0, VoxWorld::Z0 + VoxWorld::NZ - 1);
    for (int z = VoxWorld::Z0; z < VoxWorld::Z0 + VoxWorld::NZ; z += 2) {
        printf("%4d ", z);
        for (int x = VoxWorld::X0; x < VoxWorld::X0 + VoxWorld::NX; x++) {
            int H = w.groundHeight(x, z);
            char c;
            uint8_t top = w.get(x, H, z), under = w.get(x, H - 1, z);
            if (top == BK_LOG || top == BK_LEAVES) c = 'T';
            else if (under == BK_WATER || w.get(x, VoxWorld::SEA - 1, z) == BK_WATER) c = '~';
            else if (under == BK_AIR) c = 'O';  // cave opening at the surface
            else if (H <= VoxWorld::SEA + 1) c = '.';
            else c = H >= 10 ? '#' : (char)('0' + std::max(0, H));
            // crystal or ember ore visible from a cave
            printf("%c", c);
        }
        printf("\n");
    }
    // caves: air pockets under the ground with ore next to them
    int caves = 0, crystal = 0;
    for (int z = VoxWorld::Z0; z < VoxWorld::Z0 + VoxWorld::NZ; z++)
        for (int x = VoxWorld::X0; x < VoxWorld::X0 + VoxWorld::NX; x++)
            for (int y = VoxWorld::Y0 + 1; y < w.groundHeight(x, z) - 1; y++) {
                if (w.get(x, y, z) == BK_AIR) caves++;
                if (w.get(x, y, z) == BK_CRYSTAL_ORE) { crystal++; if (crystal < 40) printf("crystal %d %d %d (ground %d)\n", x, y, z, w.groundHeight(x, z)); }
            }
    printf("cave air %d, crystal %d\n", caves, crystal);
    return 0;
}

// ------------------------------------------------------------------ the shot script
namespace {
struct Caption { float t0, t1; std::string text; int size; float y; V4 col; };

struct Shot {
    const char* name;
    float beats;                                                    // length in beats of the music
    std::function<void(Game&)> setup;                               // once, at the cut
    std::function<void(Game&, float t, float dur, InputState& in)> tick;  // every frame before the sim step
    int order = 0;
    std::function<float(float)> speed;  // sim speed over the shot (0..1 -> x): slow motion
    float flash = 0;                    // white flash at the cut
};

V3 bez(const V3& a, const V3& b, const V3& c, float t) { float u = 1 - t; return a * (u * u) + b * (2 * u * t) + c * (t * t); }
float ease(float t) { t = clampf(t, 0, 1); return t * t * (3 - 2 * t); }
float easeOut(float t) { t = clampf(t, 0, 1); return 1 - (1 - t) * (1 - t) * (1 - t); }
void cam(Game& g, const V3& eye, const V3& tgt) { g.camOverride = 1; g.camOverrideEye = eye; g.camOverrideTarget = tgt; }
void put(Game& g, const V3& p, float yaw) { g.pl.pos = g.pl.safePos = p; g.pl.vel = V3(); g.pl.yaw = yaw; g.camYaw = yaw; g.camTarget = p + V3(0, 1.65f, 0); }
int ground(Game& g, int x, int z) {  // y of the first free cell above the real (edited) ground
    int y = VoxWorld::Y0 + VoxWorld::NY - 1;
    while (y > VoxWorld::Y0 && (g.vox.get(x, y - 1, z) == BK_AIR || g.vox.get(x, y - 1, z) == BK_LEAVES || g.vox.get(x, y - 1, z) == BK_LOG)) y--;
    return y;
}
void setBlock(Game& g, int b, int x, int y, int z, bool fx) {
    if (fx) { g.addItem(b, 1, false); g.mineBlock(x, y, z, false); if (!g.placeBlock(b, x, y, z, 2, 0)) g.vox.set(x, y, z, (uint8_t)b); }
    else g.vox.set(x, y, z, (uint8_t)b);
}
}  // namespace

std::vector<Shot> buildScript(std::vector<Caption>& caps, float spb, int W, int H);  // below

// ------------------------------------------------------------------ recording
void* scBotNew(Game& g);
void scBotFree(void* p);
bool scBotStep(void* p, InputState& in, std::string& section);

namespace {
const float FPS = 60;
const int SR = 44100;

// draws captions with the game's own pixel font: big, outlined, with a little pop on entry
void drawCaptions(Renderer& R, const std::vector<Caption>& caps, float now) {
    if (getenv("SHOWCASE_CLEAN")) return;  // clean plates for thumbnails
    float Wf = (float)R.winW, Hf = (float)R.winH;
    for (auto& c : caps) {
        if (now < c.t0 || now >= c.t1) continue;
        float a = std::min(1.f, std::min((now - c.t0) / 0.08f, (c.t1 - now) / 0.12f));
        float pop = 1 + 0.25f * std::max(0.f, 1 - (now - c.t0) / 0.12f);
        float sc = std::max(1.f, std::round(std::min(Wf, Hf) / 1080.f * c.size * pop));
        float y = Hf * c.y;
        int lines = 1; for (char ch : c.text) if (ch == '\n') lines++;
        size_t p = 0; int li = 0;
        while (p <= c.text.size()) {
            size_t q = c.text.find('\n', p); if (q == std::string::npos) q = c.text.size();
            std::string line = c.text.substr(p, q - p);
            float ly = y + li * 10 * sc - lines * 5 * sc;
            for (int dx = -1; dx <= 1; dx++) for (int dy = -1; dy <= 2; dy++)
                if (dx || dy) R.uiTextCentered(Wf / 2 + dx * sc * 0.6f, ly + dy * sc * 0.6f, line, sc, V4(0.08f, 0.03f, 0.02f, a), false);
            R.uiTextCentered(Wf / 2, ly, line, sc, V4(c.col.x, c.col.y, c.col.z, a * c.col.w), false);
            p = q + 1; li++;
        }
    }
}

// the trailer "look": brighter exposure (visual only), cinema bars, and white flashes on big cuts
struct Look { float exposure = 1.9f; bool bars = false; };
Look gLook;
struct EnvSave { Environment e; };
EnvSave brighten(Game& g) {
    EnvSave sv{g.lv.env};
    float k = gLook.exposure;
    Environment& e = g.lv.env;
    e.ambSky = e.ambSky * k; e.ambGround = e.ambGround * (k * 1.1f);
    e.sunColor = e.sunColor * (0.5f + 0.5f * k);
    e.skyTop = e.skyTop * (0.6f + 0.4f * k); e.skyHorizon = e.skyHorizon * (0.6f + 0.4f * k);
    e.fogColor = e.fogColor * (0.6f + 0.4f * k);
    e.fogDensity *= 0.75f;
    return sv;
}
void unbrighten(Game& g, const EnvSave& sv) { g.lv.env = sv.e; }
void drawBars(Renderer& R) {
    float Wf = (float)R.winW, Hf = (float)R.winH;
    if (gLook.bars && Wf > Hf) {
        float bh = std::round((Hf - Wf / 2.39f) / 2);
        R.uiRect(0, 0, Wf, bh, V4(0, 0, 0, 1)); R.uiRect(0, Hf - bh, Wf, bh, V4(0, 0, 0, 1));
    }
}
void drawFlash(Renderer& R, float flash) {
    if (flash > 0) R.uiRect(0, 0, (float)R.winW, (float)R.winH, V4(1, 0.97f, 0.9f, std::min(1.f, flash)));
}

// one clip = one silent video file + its own sound-effects track; the music is rendered separately
struct Recorder {
    Renderer& R; std::string dir; int W, H;
    FILE* vf = nullptr; FILE* af = nullptr;
    long frames = 0, samples = 0;
    std::vector<unsigned char> px; std::vector<float> abuf;
    std::vector<float> stills; std::string clipName; int order = 0;
    Recorder(Renderer& r, const std::string& d, int w, int h) : R(r), dir(d), W(w), H(h), px((size_t)w * h * 3) {
        if (const char* e = getenv("SHOWCASE_STILLS")) { std::string s = e; size_t p = 0; while (p < s.size()) { size_t q = s.find(',', p); if (q == std::string::npos) q = s.size(); stills.push_back((float)atof(s.substr(p, q - p).c_str())); p = q + 1; } }
    }
    std::string base() const { char b[64]; snprintf(b, sizeof(b), "/clip_%03d", order); return dir + b; }
    void begin(int ord, const std::string& name) {
        order = ord; clipName = name; frames = 0; samples = 0;
        audio::stopSfx();
        R.particles.clear();
        if (stills.empty()) {
            char cmd[1024];
            snprintf(cmd, sizeof(cmd), "ffmpeg -loglevel error -y -f rawvideo -pix_fmt rgb24 -s %dx%d -r %d -i - -vf vflip -c:v libx264 -preset medium -crf 15 -pix_fmt yuv420p '%s.mp4'", W, H, (int)FPS, base().c_str());
            vf = popen(cmd, "w");
            af = fopen((base() + ".f32").c_str(), "wb");
        }
        fprintf(stderr, "[showcase] clip %03d %s\n", ord, name.c_str());
    }
    // the global time for stills is order*1000 + time in clip
    void frame() {
        float now = frames / FPS;
        long want = (long)std::llround((frames + 1) * (double)SR / FPS);
        int n = (int)(want - samples);
        abuf.resize((size_t)n * 2);
        audio::renderOffline(abuf.data(), n);
        if (af) fwrite(abuf.data(), sizeof(float), abuf.size(), af);
        samples = want;
        bool grab = vf != nullptr;
        for (float st : stills) if (std::fabs(st - (order * 1000 + now)) < 0.5f / FPS) grab = true;
        if (grab) {
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glReadPixels(0, 0, W, H, GL_RGB, GL_UNSIGNED_BYTE, px.data());
            if (vf) fwrite(px.data(), 1, px.size(), vf);
            else {
                char name[512]; snprintf(name, sizeof(name), "%s/s_%03d_%05.2f.ppm", dir.c_str(), order, now);
                FILE* p = fopen(name, "wb");
                if (p) { fprintf(p, "P6\n%d %d\n255\n", W, H); for (int y = H - 1; y >= 0; y--) fwrite(px.data() + (size_t)y * W * 3, 1, (size_t)W * 3, p); fclose(p); }
            }
        }
        frames++;
    }
    void end() {
        if (vf) { pclose(vf); vf = nullptr; }
        if (af) { fclose(af); af = nullptr; }
        FILE* m = fopen((dir + "/manifest.txt").c_str(), "a");
        if (m) { fprintf(m, "%03d %.6f %s\n", order, frames / FPS, clipName.c_str()); fclose(m); }
    }
};

float wrapPi(float a) { while (a > PI) a -= 2 * PI; while (a < -PI) a += 2 * PI; return a; }

// smooth cameras for the bot's gameplay (the bot snaps its view around; a film camera shouldn't)
struct FilmCam {
    bool init = false; V3 eye, tgt; float yaw = 0;
    void reset() { init = false; }
    void follow(Game& g, float dt, float dist, float height, float side) {
        float want = g.camYaw;
        if (!init) yaw = want;
        yaw += wrapPi(want - yaw) * (1 - std::exp(-dt * 1.2f));
        V3 fwd(std::sin(yaw), 0, std::cos(yaw)), right(-std::cos(yaw), 0, std::sin(yaw));
        V3 t = g.pl.pos + V3(0, 1.5f, 0) + right * side;
        V3 e = t - fwd * dist + V3(0, height, 0);
        V3 d = e - t; float L = len(d);
        float hit = g.raycast(t, d / L, L + 0.3f);
        if (hit >= 0) e = t + d / L * std::max(0.8f, hit - 0.35f);
        if (!init) { eye = e; tgt = t; init = true; }
        float k = 1 - std::exp(-dt * 3.5f);
        eye = lerp(eye, e, k); tgt = lerp(tgt, t, 1 - std::exp(-dt * 5));
    }
    void boss(Game& g, float dt, float dist, float height) {
        V3 B = g.boss.pos;
        if (len(B) < 0.01f) B = g.lv.boss.center;
        V3 away = flatXZ(g.pl.pos - B); float L = lenXZ(away);
        away = L > 0.1f ? away / L : V3(0, 0, -1);
        V3 sideV(-away.z, 0, away.x);
        V3 t = lerp(g.pl.pos + V3(0, 1.4f, 0), B + V3(0, 2.2f, 0), 0.45f);
        V3 e = g.pl.pos + away * dist + sideV * (dist * 0.35f) + V3(0, height, 0);
        V3 d = e - t; float Ld = len(d);
        float hit = g.raycast(t, d / Ld, Ld + 0.3f);
        if (hit >= 0) e = t + d / Ld * std::max(1.0f, hit - 0.35f);
        if (!init) { eye = e; tgt = t; init = true; }
        eye = lerp(eye, e, 1 - std::exp(-dt * 1.8f)); tgt = lerp(tgt, t, 1 - std::exp(-dt * 3));
    }
};
}  // namespace

std::vector<Shot> buildScript(std::vector<Caption>& caps, float spb, int W, int H);  // the island, below

// ------------------------------------------------------------------ the adventure: the campaign bot plays, we film
// A clip starts `offset` seconds after the bot reaches a section of its route.
enum CamMode { CM_FOLLOW, CM_BOSS, CM_GAME, CM_WIDE, CM_ORBIT, CM_CRANE, CM_TITLE };
struct CampClip {
    int order; const char* name; const char* section; float offset; float beats; CamMode cam;
    std::vector<Caption> caps;  // times in beats from the clip start
    float dist = 6.5f, height = 2.2f;
    std::function<void(Game&)> setup;
    float bpm = 148;            // the song this clip is cut to
    float flash = 0;            // white flash at the cut
    int cine = -1;              // >= 0: a cinematic step (intro or ending) instead of a bot section
    std::function<float(float)> speed;  // sim speed over the clip (0..1 -> x), for slow motion
    float orbit = 0;            // boss/follow cams: slow drift around the action (radians over the clip)
    int menu = -1;              // >= 0: show this real game screen (a shop) instead of playing
};
std::vector<CampClip> campaignClips(int W, int H);  // below
std::vector<CampClip> introClips(int W, int H);     // below

namespace {
// one film frame of whatever the game is doing: visual timers, the trailer look, captions
struct Filmer {
    Game& g; Renderer& R; Recorder& rec;
    std::vector<Caption> caps; float now = 0, flash = 0;
    Filmer(Game& gg, Renderer& rr, Recorder& re) : g(gg), R(rr), rec(re) {
        g.overlayUI = [this]() { drawBars(R); drawCaptions(R, caps, now); drawFlash(R, flash); };
    }
    void start(const CampClip& c) {
        float spb = 60.f / c.bpm;
        caps.clear();
        for (auto cp : c.caps) { cp.t0 *= spb; cp.t1 *= spb; caps.push_back(cp); }
        rec.begin(c.order, c.name);
    }
    void shoot(float t, float flashAmt) {
        now = t; flash = flashAmt;
        g.letterbox = 0;
        EnvSave sv = brighten(g);
        g.draw();
        unbrighten(g, sv);
        rec.frame();
    }
};
}  // namespace

static int runCampaign(Game& g, Renderer& R, Recorder& rec, int W, int H, bool logOnly) {
    std::vector<CampClip> clips = campaignClips(W, H);
    g.botActive = true; g.godMode = true;
    g.newGame();
    InputState skip; skip.mBack = true;
    g.frame(0.016f, skip, 0, 0);
    g.hideHUD = true;
    audio::setVolumes(0.85f, 0.0f, 0.9f);
    void* bot = scBotNew(g);
    std::string section, lastSection;
    float secT = 0, simT = 0;
    size_t ci = 0;
    Filmer film(g, R, rec);
    int lastAlive = 0; float lastBossHp = 0; V3 lastPos = g.pl.pos; float logT = 0;
    FilmCam fc;
    bool running = true;
    auto simStep = [&]() {
        InputState in;
        if (g.screen == SC_ENDING) { g.frame(SIM_DT, in, 0, 0); simT += SIM_DT; return; }
        running = scBotStep(bot, in, section);
        if (section != lastSection) { lastSection = section; secT = 0; if (logOnly) printf("== %7.2f  SECTION %s\n", simT, section.c_str()); }
        g.update(SIM_DT, in);
        simT += SIM_DT; secT += SIM_DT;
    };
    while ((running || g.screen == SC_ENDING) && (logOnly || ci < clips.size())) {
        simStep();
        if (g.screen == SC_VICTORY) break;
        if (logOnly) {
            int alive = 0; for (auto& e : g.enemies) if (e.alive) alive++;
            if (alive < lastAlive) printf("   %7.2f  +%5.2f kill (%d left)\n", simT, secT, alive);
            if (g.boss.active && g.boss.hp < lastBossHp - 0.01f) printf("   %7.2f  +%5.2f boss hit %.1f/%.0f st %d ph %d\n", simT, secT, g.boss.hp, g.boss.maxHp, g.boss.state, g.boss.phase);
            if (len(g.pl.pos - lastPos) > 1.5f) printf("   %7.2f  +%5.2f WARP/teleport\n", simT, secT);
            { static V3 lastBloom; static bool wasCharged = false; static float lastDash = 0;
              if (len(g.bloomPos - lastBloom) > 0.01f) { printf("   %7.2f  +%5.2f BLOOM\n", simT, secT); lastBloom = g.bloomPos; }
              bool ch = g.flame.charged && g.flame.st == F_FLYING; if (ch && !wasCharged) printf("   %7.2f  +%5.2f SUNFLARE\n", simT, secT); wasCharged = ch;
              if (g.pl.dashT > 0 && lastDash <= 0) printf("   %7.2f  +%5.2f DASH\n", simT, secT); lastDash = g.pl.dashT; }
            if (simT - logT > 1.0f) { logT = simT; printf("   %7.2f  +%5.2f lv %d scr %d cine %d/%.1f pos (%.1f %.1f %.1f) enemies %d boss %s hp %.1f camOv %.2f\n", simT, secT, g.lv.id, g.screen, g.cineStep, g.cineT, g.pl.pos.x, g.pl.pos.y, g.pl.pos.z, alive, g.boss.active ? "ON" : "-", g.boss.hp, g.camOverride); }
            lastAlive = alive; lastBossHp = g.boss.active ? g.boss.hp : 0; lastPos = g.pl.pos;
        }
        if (ci >= clips.size()) { if (logOnly) continue; break; }
        CampClip& c = clips[ci];
        bool trig = c.cine >= 0 ? (g.screen == SC_ENDING && g.cineStep == c.cine && g.cineT >= c.offset)
                                : (section == c.section && secT >= c.offset && g.screen == SC_PLAY);
        if (!trig) continue;
        if (c.setup) c.setup(g);
        fprintf(stderr, "[showcase] %s at sim %.2f pos (%.2f %.2f %.2f)\n", c.name, simT, g.pl.pos.x, g.pl.pos.y, g.pl.pos.z);
        if (logOnly || c.beats <= 0) { if (logOnly) printf("== %7.2f  CLIP %s\n", simT, c.name); ci++; continue; }
        float dur = c.beats * 60.f / c.bpm;
        int nf = (int)std::lround(dur * FPS);
        film.start(c);
        fc.reset();
        if (c.menu >= 0) {
            // a real game menu: the world pauses, the cursor walks down the list on the beat
            Screen prev = g.screen;
            g.screen = (Screen)c.menu; g.hideHUD = false; g.shopSel = 0; g.mouseMoved = g.mouseClick = false;
            R.uiScaleOverride = 2;
            int lastSel = 0;
            for (int f = 0; f < nf; f++) {
                float ct = f / FPS;
                int sel = std::min(4, (int)(ct / (2 * 60.f / c.bpm)));
                if (sel != lastSel) { audio::play(SFX_MENU_MOVE); lastSel = sel; }
                g.shopSel = sel;
                R.time += 1.f / FPS; R.updateParticles(1.f / FPS);
                film.shoot(ct, c.flash * std::max(0.f, 1 - ct / 0.25f));
            }
            R.uiScaleOverride = 0; g.hideHUD = true; g.screen = prev;
            rec.end(); ci++;
            continue;
        }
        float visBloomT = 10, simBloomR = g.bloomR, acc = 0;
        float orbit0 = 0;
        for (int f = 0; f < nf && (running || g.screen == SC_ENDING); f++) {
            float ct = f / FPS, ck = ct / dur;
            float sp = c.speed ? c.speed(ck) : 1.f;
            acc += 2 * sp;
            while (acc >= 1) { acc -= 1; simStep(); }
            g.dialog.clear();
            float vdt = sp / FPS;
            R.time += vdt; R.updateParticles(vdt);
            g.screenT += vdt;
            g.toastT = g.statusT = g.areaTitleT = 0;
            g.respawnFade = std::max(0.f, g.respawnFade - vdt * 1.5f);
            if (g.screen != SC_ENDING) g.flashOverlay = std::max(0.f, g.flashOverlay - vdt * 1.5f);
            if (g.bloomR == 0 && simBloomR != 0) visBloomT = 0;
            simBloomR = g.bloomR; visBloomT += vdt;
            float simBloomFlash = g.bloomFlash;
            g.bloomFlash = std::max(0.f, 1 - visBloomT * 2.5f); g.bloomR = visBloomT * 14;
            V3 se = g.camEye, st = g.camTarget;
            if (g.screen == SC_ENDING || g.camOverride > 0.02f || c.cam == CM_GAME) { /* the game's own (cinematic) camera */ }
            else if (c.cam == CM_TITLE) {
                float a = 2.0f + ct * 0.09f;
                g.camEye = V3(std::cos(a) * 46, 18 + ck * 4, std::sin(a) * 46); g.camTarget = V3(0, 14 - ck * 3, 0);
            }
            else if (c.cam == CM_ORBIT || c.cam == CM_CRANE) {
                if (!fc.init) { fc.yaw = g.camYaw; fc.tgt = g.pl.pos + V3(0, 1.3f, 0); fc.init = true; }
                fc.tgt = lerp(fc.tgt, g.pl.pos + V3(0, 1.3f, 0), 1 - std::exp(-vdt * 5));
                float a = fc.yaw + PI + (c.cam == CM_ORBIT ? -0.9f + ck * 1.5f : 0.3f * ck);
                float r = c.cam == CM_ORBIT ? c.dist : c.dist * (0.6f + 0.8f * ease(ck));
                float h = c.cam == CM_ORBIT ? c.height : c.height * (0.4f + 1.6f * ease(ck));
                V3 e = fc.tgt + V3(std::sin(a) * r, h, std::cos(a) * r);
                V3 d = e - fc.tgt; float L = len(d);
                float hit = g.raycast(fc.tgt, d / L, L + 0.3f);
                if (hit >= 0) e = fc.tgt + d / L * std::max(1.0f, hit - 0.35f);
                g.camEye = e; g.camTarget = fc.tgt;
            }
            else if (c.cam == CM_BOSS && g.boss.active) {
                fc.boss(g, vdt, c.dist, c.height);
                // slow drift around the boss
                V3 B = g.boss.pos; if (len(B) < 0.01f) B = g.lv.boss.center;
                float a = c.orbit * (ck - 0.5f);
                V3 rel = fc.eye - B; float cs = std::cos(a), sn = std::sin(a);
                g.camEye = B + V3(rel.x * cs - rel.z * sn, rel.y, rel.x * sn + rel.z * cs); g.camTarget = fc.tgt;
                (void)orbit0;
            }
            else { fc.follow(g, vdt, c.dist, c.height, 0.6f); g.camEye = fc.eye; g.camTarget = fc.tgt; }
            audio::setListener(g.camEye, norm(cross(norm(g.camTarget - g.camEye), V3(0, 1, 0))));
            film.shoot(ct, c.flash * std::max(0.f, 1 - ct / 0.25f));
            g.camEye = se; g.camTarget = st;
            g.bloomFlash = simBloomFlash; g.bloomR = simBloomR;
        }
        rec.end();
        ci++;
    }
    scBotFree(bot);
    if (!logOnly && ci < clips.size()) fprintf(stderr, "[showcase] WARNING: only %zu of %zu campaign clips were reached\n", ci, clips.size());
    return 0;
}

// ------------------------------------------------------------------ the intro cinematic (the Lamp goes out, Marrow wakes Wick)
static int runIntro(Game& g, Renderer& R, Recorder& rec, int W, int H) {
    std::vector<CampClip> clips = introClips(W, H);
    g.newGame();               // starts the intro cinematic
    g.hideHUD = true;
    audio::setVolumes(0.85f, 0.0f, 0.9f);
    Filmer film(g, R, rec);
    size_t ci = 0;
    InputState none;
    while (g.screen == SC_INTRO && ci < clips.size()) {
        CampClip& c = clips[ci];
        if (g.cineStep == c.cine && g.cineT >= c.offset) {
            float dur = c.beats * 60.f / c.bpm;
            int nf = (int)std::lround(dur * FPS);
            film.start(c);
            for (int f = 0; f < nf; f++) {
                float ct = f / FPS;
                float sp = c.speed ? c.speed(ct / dur) : 1.f;
                InputState in;
                g.frame(sp / FPS, in, 0, 0);
                if (g.screen != SC_INTRO) break;
                film.shoot(ct, c.flash * std::max(0.f, 1 - ct / 0.25f));
            }
            rec.end();
            ci++;
            continue;
        }
        g.frame(1.f / FPS, none, 0, 0);
    }
    return 0;
}

// ------------------------------------------------------------------ Wick's Island: scripted shots in one continuous session
static int runIsland(Game& g, Renderer& R, Recorder& rec, int W, int H) {
    const float spb = 60.f / 76.f;   // the island act is cut to SONG_ENDING
    gLook.exposure = 1.12f;
    std::vector<Caption> caps;
    std::vector<Shot> shots = buildScript(caps, spb, W, H);
    g.newGame();
    g.screen = SC_PLAY; g.dialog.clear(); g.letterbox = 0;
    g.setFlag("home.unlocked"); g.setFlag("isle.welcome"); g.setFlag("isle.placed"); g.setFlag("isle.mined");
    for (const char* f : {"tut.look", "tut.move", "tut.jump", "tut.throw", "tut.dash", "ab.warp", "ab.bloom", "ab.sunflare"}) g.setFlag(f);
    g.save.voxSeed = getenv("SHOWCASE_SEED") ? (uint32_t)strtoul(getenv("SHOWCASE_SEED"), nullptr, 10) : 42u;
    g.save.dayT = 0.1f;
    g.enterLevel(LV_HOME, 0);
    g.dialog.clear(); g.screen = SC_PLAY;
    g.areaTitleT = 0;
    g.hideHUD = true;
    g.godMode = true;
    audio::setVolumes(0.85f, 0.0f, 0.9f);
    float now = 0;
    float flash = 0;
    g.overlayUI = [&]() { drawBars(R); drawCaptions(R, caps, now); drawFlash(R, flash); };
    float t0 = 0;
    for (size_t si = 0; si < shots.size(); si++) {
        Shot& sh = shots[si];
        float dur = sh.beats * spb;
        int nf = (int)std::lround((t0 + dur) * FPS) - (int)std::lround(t0 * FPS);
        g.camOverride = 1; g.hitstop = 0; g.shake = 0;
        if (sh.setup) sh.setup(g);
        rec.begin(sh.order, sh.name);
        for (int f = 0; f < nf; f++) {
            float t = f / FPS;
            now = t0 + t;
            InputState in;
            if (sh.tick) sh.tick(g, t, dur, in);
            g.dialog.clear();
            if (g.screen != SC_PLAY) g.screen = SC_PLAY;
            float sp = sh.speed ? sh.speed(t / dur) : 1.f;
            g.frame(sp / FPS, in, 0, 0);
            g.toastT = 0; g.statusT = 0; g.areaTitleT = 0;
            if (g.camOverride >= 1) { g.camEye = g.camOverrideEye; g.camTarget = g.camOverrideTarget; }
            flash = sh.flash * std::max(0.f, 1 - t / 0.25f);
            g.letterbox = 0;
            EnvSave sv = brighten(g);
            g.draw();
            unbrighten(g, sv);
            rec.frame();
        }
        rec.end();
        t0 += dur;
    }
    return 0;
}


// ------------------------------------------------------------------ the 1.1 update trailer (TikTok, ~30 s, SONG_CITY)
void* scBotRoute(Game& g, int route);
void scFeatureSave(Game& g);
namespace {
struct Seg {
    const char* name; float beats;
    std::function<void(Game&)> setup;           // fresh feature save first, then this
    int route = -1;                             // bot route (see scBotRoute), -1 none
    float skip = 0;                             // sim seconds to play before filming
    std::function<bool(Game&)> until;           // ...or skip until this is true
    CamMode cam = CM_FOLLOW; float dist = 6.0f, height = 2.2f;
    std::vector<Caption> caps;                  // times in beats
    std::function<void(Game&, float, InputState&)> tick;  // extra input each sim step (t < 0 while skipping)
    std::function<float(float)> speed;
    float flash = 0; bool hud = false;
};
}

static std::vector<Seg> updateSegs(int W, int H) {
    bool tall = H > W;
    V4 gold(1, 0.82f, 0.38f, 1), white(1, 0.97f, 0.9f, 1), red(1, 0.4f, 0.35f, 1), cyan(0.5f, 0.95f, 1, 1), orange(1, 0.62f, 0.25f, 1), green(0.45f, 1, 0.6f, 1);
    float ty = tall ? 0.2f : 0.16f, sy = ty + (tall ? 0.055f : 0.08f);
    auto big = [&](float b0, float b1, const char* t, V4 c) { return Caption{b0, b1, t, 8, ty, c}; };
    auto small = [&](float b0, float b1, const char* t) { return Caption{b0, b1, t, 4, sy, white}; };
    std::vector<Seg> v;
    Seg s;
    // 1. the hook: Wick on a wanted poster's street, the searchlights find him
    s = Seg{"city: spotted", 8};
    s.setup = [](Game& g) { g.save.hat = 10; g.enterLevel(LV_CITY, 1); g.pl.pos = V3(-2, 0, 30); g.camYaw = 0.1f; };
    s.route = 0; s.dist = 6.5f; s.height = 2.4f;
    s.caps = {Caption{0, 3.5f, "WICKLIGHT 1.1", 9, ty, gold}, big(3.5f, 8, tall ? "THE FBI THINKS\nWICK IS\nTHE HACKER" : "THE FBI THINKS WICK IS THE HACKER", red)};
    v.push_back(s);
    // 2. up the tower's fire escapes
    s = Seg{"city: tower climb", 6};
    s.setup = [](Game& g) { g.save.hat = 10; g.setFlag("city.g1"); g.enterLevel(LV_CITY, 2); g.pl.pos = V3(3, 0, 88); };
    s.route = 1; s.skip = 0.6f; s.dist = 7.5f; s.height = 1.4f; s.flash = 0.5f;
    s.caps = {big(0, 6, tall ? "NEW LEVEL:\nLUMEN CITY" : "NEW LEVEL: LUMEN CITY", cyan), small(0.5f, 6, "searchlights, FBI agents, a rooftop escape")};
    v.push_back(s);
    // 3. Firewall on the helipad
    s = Seg{"city: firewall holdout", 6};
    s.setup = [](Game& g) { g.save.hat = 8; g.setFlag("ab.firewall"); g.setFlag("city.g1"); g.setFlag("city.arena"); g.enterLevel(LV_CITY, 4); };
    s.route = 2; s.skip = 7.0f; s.dist = 7.0f; s.height = 3.0f;
    s.tick = [](Game& g, float t, InputState& in) { if (t >= 0.25f && t < 0.26f) { in.pressed[A_FIREWALL] = true; in.down[A_FIREWALL] = true; } };
    s.caps = {big(0, 6, tall ? "NEW POWER:\nFIREWALL" : "NEW POWER: FIREWALL", orange), small(0.5f, 6, "a wall of fire nothing gets through")};
    v.push_back(s);
    // 4. Ember Grapple across the broken bridge
    s = Seg{"hub: grapple", 6};
    s.setup = [](Game& g) { g.save.hat = 9; g.setFlag("ab.grapple"); g.enterLevel(LV_HUB, 0); g.pl.pos = g.pl.safePos = V3(0, 0, 41); g.camYaw = g.pl.yaw = 0; g.camTarget = g.pl.pos + V3(0, 1.65f, 0); };
    s.route = 3; s.skip = 0.25f; s.dist = 5.5f; s.height = 1.6f;
    s.tick = [](Game& g, float t, InputState& in) { if (t > 0.3f && g.flame.st == F_HOVER && g.flame.anchored >= 0) { in.down[A_GRAPPLE] = true; if (!g.grappling) in.pressed[A_GRAPPLE] = true; } };
    s.caps = {big(0, 6, tall ? "NEW POWER:\nEMBER GRAPPLE" : "NEW POWER: EMBER GRAPPLE", gold), small(0.5f, 6, "swing from hooks on a rope of fire")};
    v.push_back(s);
    // 5. Overclock: the world slows down, Wick doesn't
    s = Seg{"wharf: overclock", 6};
    s.setup = [](Game& g) {
        g.save.hat = 6; g.enterLevel(LV_WHARF, 1); g.pl.pos = V3(0, 0, 40); g.camYaw = 0;
        for (V3 p : {V3(-3, 0.3f, 46), V3(3, 0.3f, 47), V3(0, 0.3f, 50), V3(-5, 0.3f, 52), V3(4, 0.3f, 53)}) g.spawnEnemy(E_GLOOMLING, p, -1);
        g.spawnEnemy(E_TARBELL, V3(6, 0, 54), -1);
        g.ocCharge = 1;
    };
    s.route = 4; s.skip = 0.4f; s.dist = 6.0f; s.height = 2.2f;
    s.tick = [](Game& g, float t, InputState& in) { (void)g; if (t >= 0.2f && t < 0.21f) in.pressed[A_OVERCLOCK] = true; };
    s.caps = {big(0, 6, tall ? "NEW POWER:\nOVERCLOCK" : "NEW POWER: OVERCLOCK", cyan), small(0.5f, 6, "slow down time - everyone but you")};
    v.push_back(s);
    // 6. GL1TCH's hacking puzzle
    s = Seg{"clock: hacking puzzle", 7};
    s.setup = [](Game& g) { g.save.hat = 7; g.save.flags.erase("boss.1"); g.enterLevel(LV_CLOCK, 1); g.pl.pos = g.pl.safePos = V3(0, 18, 170); g.camYaw = 0; for (auto& e : g.enemies) e.alive = false; };
    s.route = 5; s.skip = 0.3f; s.dist = 6.5f; s.height = 2.6f;
    s.speed = [](float x) { return x < 0.45f ? 1.8f : 1.0f; };
    s.caps = {big(0, 7, tall ? "HACK THE\nTERMINALS" : "HACK THE TERMINALS", green), small(0.5f, 7, "watch the order, burn them in the same order")};
    v.push_back(s);
    // 7. GL1TCH, rebuilt: teleports, code bolts, fake Wicks
    s = Seg{"clock: GL1TCH", 7};
    s.setup = [](Game& g) {
        g.save.hat = 7; g.save.flags.erase("boss.1"); g.enterLevel(LV_CLOCK, 2);
        g.pl.pos = g.lv.boss.center + V3(-3, 0, -9); g.camYaw = 0.3f; g.startBoss();
        g.boss.hp = g.boss.maxHp = 999; g.boss.phase = 1;
    };
    s.route = 6; s.skip = 9.0f; s.cam = CM_BOSS; s.dist = 8.0f; s.height = 3.0f; s.flash = 0.4f;
    s.caps = {big(0, 7, tall ? "GL1TCH IS\nBACK" : "GL1TCH IS BACK", green), small(0.5f, 7, "teleports, viruses, fake Wicks")};
    v.push_back(s);
    // 8. side jobs: the candle race and fishing
    s = Seg{"hub: candle race", 4};
    s.setup = [](Game& g) { g.save.hat = 4; g.enterLevel(LV_HUB, 0); g.pl.pos = g.pl.safePos = V3(16, 0, 12); g.startRace(); g.raceIdx = 1; };
    s.skip = 0.2f; s.dist = 6.0f; s.height = 2.0f; s.hud = true;
    s.tick = [](Game& g, float, InputState& in) {
        if (g.raceIdx < 0) return;
        const RaceRing& r = g.lv.race[g.raceIdx];
        V3 d = flatXZ(r.pos - g.pl.pos);
        if (lenXZ(d) > 0.01f) g.camYaw = std::atan2(d.x, d.z);
        in.moveY = 1;
        if (g.pl.grounded && g.pl.dashCd <= 0 && lenXZ(d) > 5) { in.pressed[A_DASH] = true; in.down[A_DASH] = true; }
        if (r.high && lenXZ(d) < 3.2f && g.pl.grounded) { in.pressed[A_JUMP] = true; in.down[A_JUMP] = true; } else if (r.high) in.down[A_JUMP] = true;
    };
    s.caps = {big(0, 4, tall ? "CANDLE\nRACES" : "CANDLE RACES", gold)};
    v.push_back(s);
    s = Seg{"wharf: fishing", 5};
    s.setup = [](Game& g) {
        g.save.hat = 3; g.setFlag("quest.carp"); g.save.fishCaught = 2;
        g.enterLevel(LV_WHARF, 0); g.pl.pos = g.pl.safePos = V3(1.8f, 0, 7.5f);
        for (int i = 0; i < (int)g.lv.talkers.size(); i++) if (g.lv.talkers[i].who == 16) { g.talk(i); g.camYaw = g.lv.talkers[i].yaw - 0.5f; break; }
    };
    s.until = [](Game& g) { return g.fishState == 2 && g.fishT > 0.05f; };
    s.dist = 4.5f; s.height = 1.6f; s.hud = true;
    s.tick = [](Game& g, float t, InputState& in) {
        if (t < 0) return;
        if (g.fishState == 2 && t > 0.2f) in.pressed[A_INTERACT] = true;
        if (g.fishState == 3) {
            if (g.fishT < 0.02f) { float sp = 2.2f + 0.8f * g.fishKind; g.fishZone = 0.5f - 0.5f * std::cos(1.1f * sp); }
            if (std::fabs(g.fishNeedle - g.fishZone) < g.fishZoneW * 0.25f && g.fishT > 0.5f) in.pressed[A_INTERACT] = true;
        }
    };
    s.caps = {big(0, 5, tall ? "FISHING" : "FISHING", cyan), small(0.5f, 5, "catch the Golden Carp")};
    v.push_back(s);
    // 9. end card
    s = Seg{"end card", 7};
    s.setup = [](Game& g) { g.save.hat = 4; g.enterLevel(LV_HUB, 0); g.pl.pos = g.pl.safePos = V3(0, 0, -13); g.camYaw = PI; };
    s.cam = CM_ORBIT; s.dist = 5.0f; s.height = 1.2f; s.flash = 0.6f;
    s.caps = {Caption{0, 7, "WICKLIGHT 1.1", 9, tall ? 0.16f : 0.14f, gold},
              Caption{0.6f, 7, tall ? "+ 10 HATS\n+ SIDE QUESTS" : "+ 10 HATS + SIDE QUESTS", 5, tall ? 0.24f : 0.25f, white},
              Caption{1.5f, 7, tall ? "FREE ON LINUX" : "FREE ON LINUX", 7, tall ? 0.70f : 0.70f, green},
              Caption{1.5f, 7, "github.com/rxvy-dev/wicklight", tall ? 4 : 4, tall ? 0.76f : 0.79f, white}};
    v.push_back(s);
    return v;
}

static int runUpdate(Game& g, Renderer& R, Recorder& rec, int W, int H) {
    const float spb = 60.f / 124.f;  // SONG_CITY
    std::vector<Seg> segs = updateSegs(W, H);
    float now = 0, flash = 0;
    std::vector<Caption> caps;
    void* bot = nullptr;
    FilmCam fc;
    int order = 0;
    for (auto& s : segs) {
        if (bot) { scBotFree(bot); bot = nullptr; }
        scFeatureSave(g);
        audio::setVolumes(0.85f, 0.0f, 0.9f);
        g.godMode = true;
        if (s.setup) s.setup(g);
        g.dialog.clear(); g.screen = SC_PLAY; g.letterbox = 0; g.camOverride = 0;
        g.hideHUD = !s.hud;
        R.uiScaleOverride = s.hud ? (H > W ? 4 : 3) : 0;
        g.overlayUI = [&]() { drawBars(R); drawCaptions(R, caps, now); drawFlash(R, flash); };
        if (s.route >= 0) bot = scBotRoute(g, s.route);
        std::string sec;
        auto simStep = [&](float t) {
            InputState in;
            if (bot) {
                bool ok = scBotStep(bot, in, sec);
                if (!ok && g.dialog.empty()) { scBotFree(bot); bot = nullptr; in = InputState(); }
            }
            if (s.tick) s.tick(g, t, in);
            g.update(SIM_DT, in);
            g.dialog.clear();
            if (g.screen != SC_PLAY) g.screen = SC_PLAY;
        };
        for (int k = 0; k < (int)(s.skip * 120); k++) simStep(-1);
        if (s.until) for (int k = 0; k < 120 * 12 && !s.until(g); k++) simStep(-1);
        float dur = s.beats * spb;
        int nf = (int)std::lround(dur * FPS);
        caps.clear();
        for (auto cp : s.caps) { cp.t0 *= spb; cp.t1 *= spb; caps.push_back(cp); }
        rec.begin(order++, s.name);
        fc.reset();
        float acc = 0;
        for (int f = 0; f < nf; f++) {
            float ct = f / FPS, ck = ct / dur;
            float sp = s.speed ? s.speed(ck) : 1.f;
            acc += 2 * sp;
            while (acc >= 1) { acc -= 1; simStep(ct); }
            float vdt = sp / FPS;
            R.time += vdt; R.updateParticles(vdt);
            g.screenT += vdt;
            if (!s.hud) g.toastT = g.statusT = 0;
            g.areaTitleT = 0;
            g.respawnFade = std::max(0.f, g.respawnFade - vdt * 1.5f);
            g.flashOverlay = std::max(0.f, g.flashOverlay - vdt * 1.5f);
            g.glitch = std::max(0.f, g.glitch - vdt * 1.6f);
            g.bloomFlash = std::max(0.f, g.bloomFlash - vdt * 2.5f); g.bloomR += vdt * 14;
            V3 se = g.camEye, st = g.camTarget;
            if (s.cam == CM_BOSS && g.boss.active) { fc.boss(g, vdt, s.dist, s.height); g.camEye = fc.eye; g.camTarget = fc.tgt; }
            else if (s.cam == CM_ORBIT) {
                if (!fc.init) { fc.yaw = g.camYaw; fc.tgt = g.pl.pos + V3(0, 1.3f, 0); fc.init = true; }
                float a = fc.yaw + PI - 0.7f + ck * 1.2f;
                g.camEye = fc.tgt + V3(std::sin(a) * s.dist, s.height, std::cos(a) * s.dist); g.camTarget = fc.tgt + V3(0, 0.3f, 0);
            } else { fc.follow(g, vdt, s.dist, s.height, 0.5f); g.camEye = fc.eye; g.camTarget = fc.tgt; }
            audio::setListener(g.camEye, norm(cross(norm(g.camTarget - g.camEye), V3(0, 1, 0))));
            now = ct; flash = s.flash * std::max(0.f, 1 - ct / 0.25f);
            g.letterbox = 0;
            EnvSave sv = brighten(g);
            g.draw();
            unbrighten(g, sv);
            rec.frame();
            g.camEye = se; g.camTarget = st;
        }
        rec.end();
    }
    if (bot) scBotFree(bot);
    return 0;
}

// ------------------------------------------------------------------ the music bed: adventure (FINAL), a beat of silence, island (JUNK)
static int runMusic(const std::string& dir) {
    audio::setVolumes(0.85f, 0.62f, 0.0f);
    const char* spec = getenv("SHOWCASE_MUSIC");  // "song:seconds,song:seconds,..."  (song -1 = silence)
    std::string s = spec ? spec : "";
    FILE* af = fopen((dir + "/music.f32").c_str(), "wb");
    std::vector<float> buf;
    size_t p = 0;
    while (p < s.size()) {
        size_t q = s.find(',', p); if (q == std::string::npos) q = s.size();
        std::string part = s.substr(p, q - p); p = q + 1;
        int song = atoi(part.c_str()); double secs = atof(part.substr(part.find(':') + 1).c_str());
        audio::forceSong((Song)song);
        long n = (long)std::llround(secs * SR);
        buf.resize((size_t)n * 2);
        audio::renderOffline(buf.data(), (int)n);
        fwrite(buf.data(), sizeof(float), buf.size(), af);
    }
    fclose(af);
    return 0;
}

int runShowcase(Game& g, Renderer& R, const std::string& out, int W, int H) {
    // out = a directory; SHOWCASE_PART = campaign | island | music | log
    std::string part = getenv("SHOWCASE_PART") ? getenv("SHOWCASE_PART") : "island";
    gLook.bars = getenv("SHOWCASE_CLEAN") == nullptr;
    if (part == "music") return runMusic(out);
    Recorder rec(R, out, W, H);
    if (part == "campaign" || part == "log") return runCampaign(g, R, rec, W, H, part == "log");
    if (part == "intro") return runIntro(g, R, rec, W, H);
    if (part == "update") { gLook.bars = false; gLook.exposure = 1.5f; return runUpdate(g, R, rec, W, H); }
    return runIsland(g, R, rec, W, H);
}

// ================================================================== the script
// Island seed 42. Plaza at (0,0,-21); the house goes on the flat ground east of it; the hill is around (4, 9, 8).
namespace {
const int HX0 = 12, HX1 = 18, HZ0 = -21, HZ1 = -16, HY = 2;  // house footprint and floor height

void flatten(Game& g, int x0, int x1, int z0, int z1, int y) {
    for (int z = z0; z <= z1; z++) for (int x = x0; x <= x1; x++) {
        for (int yy = VoxWorld::Y0 + 1; yy < VoxWorld::Y0 + VoxWorld::NY; yy++) {
            uint8_t b = yy < y - 1 ? (g.vox.get(x, yy, z) == BK_AIR ? BK_DIRT : g.vox.get(x, yy, z)) : (yy == y - 1 ? BK_GRASS : BK_AIR);
            if (yy < y - 3 && b == BK_AIR) b = BK_STONE;
            g.vox.set(x, yy, z, b);
        }
    }
}

struct Put { int x, y, z, b; uint8_t m; };

std::vector<Put> gPlan; size_t gDone = 0;
void clearTrees(Game& g, int x0, int x1, int z0, int z1) {
    for (int z = z0; z <= z1; z++) for (int x = x0; x <= x1; x++) for (int y = VoxWorld::Y0; y < VoxWorld::Y0 + VoxWorld::NY; y++) {
        uint8_t b = g.vox.get(x, y, z);
        if (b == BK_LOG || b == BK_LEAVES) g.vox.set(x, y, z, BK_AIR);
    }
}
std::vector<Put> housePlan() {
    std::vector<Put> v;
    for (int z = HZ0; z <= HZ1; z++) for (int x = HX0; x <= HX1; x++) v.push_back({x, HY - 1, z, BK_COBBLE, 0});
    for (int y = HY; y < HY + 3; y++)
        for (int z = HZ0; z <= HZ1; z++) for (int x = HX0; x <= HX1; x++) {
            bool edgeX = x == HX0 || x == HX1, edgeZ = z == HZ0 || z == HZ1;
            if (!edgeX && !edgeZ) continue;
            int b = (edgeX && edgeZ) ? BK_LOG : BK_PLANKS;
            if (y == HY + 1 && !(edgeX && edgeZ) && ((edgeZ && (x == HX0 + 1 || x == HX1 - 1)) || (edgeX && z == (HZ0 + HZ1) / 2))) b = BK_GLASS;
            if (z == HZ0 && x == 15 && y < HY + 2) continue;  // the door goes here
            v.push_back({x, y, z, b, 0});
        }
    v.push_back({15, HY, HZ0, BK_DOOR, 2});
    v.push_back({15, HY + 1, HZ0, BK_DOOR_TOP, 2});
    for (int k = 0; k < 4; k++) {
        int y = HY + 3 + k;
        for (int z = HZ0 - 1 + k; z <= HZ1 + 1 - k; z++) for (int x = HX0 - 1 + k; x <= HX1 + 1 - k; x++) {
            if (k < 3 && x > HX0 - 1 + k && x < HX1 + 1 - k && z > HZ0 - 1 + k && z < HZ1 + 1 - k) continue;  // hollow roof
            v.push_back({x, y, z, BK_ROOF, 0});
        }
    }
    // chimney + lamps by the door + a lamp inside
    for (int y = HY + 3; y < HY + 8; y++) v.push_back({HX1 - 1, y, HZ1 - 1, BK_STONEBRICK, 0});
    v.push_back({13, HY, HZ0 - 2, BK_LOG, 0}); v.push_back({13, HY + 1, HZ0 - 2, BK_LAMP, 0});
    v.push_back({17, HY, HZ0 - 2, BK_LOG, 0}); v.push_back({17, HY + 1, HZ0 - 2, BK_LAMP, 0});
    v.push_back({16, HY, HZ1 - 1, BK_BED, 0});
    v.push_back({13, HY, HZ1 - 1, BK_CHEST, 0});
    v.push_back({HX0 + 1, HY + 2, HZ0 + 1, BK_LAMP, 0});
    v.push_back({11, HY, HZ0 - 1, BK_FLOWER_RED, 0}); v.push_back({19, HY, HZ0 - 1, BK_FLOWER_YELLOW, 0});
    v.push_back({12, HY, HZ0 - 3, BK_FLOWER_YELLOW, 0}); v.push_back({18, HY, HZ0 - 3, BK_FLOWER_RED, 0});
    return v;
}

// a crystal cave dug into the hill: a tunnel opening into an egg-shaped chamber with ore and glowing crystals
const V3 CAVE(5.5f, 2.2f, 8.5f);
void carveCave(Game& g) {
    Rng r(4242);
    for (int z = -4; z <= 16; z++) for (int y = -3; y <= 8; y++) for (int x = -2; x <= 13; x++) {
        V3 d(x + 0.5f - CAVE.x, y + 0.5f - CAVE.y, z + 0.5f - CAVE.z);
        float e = (d.x * d.x) / 25.f + (d.y * d.y) / 7.f + (d.z * d.z) / 22.f;
        bool tunnel = z >= -3 && z <= 5 && x >= 4 && x <= 6 && y >= 1 && y <= 3;
        bool tunnelWall = z >= -3 && z <= 5 && x >= 3 && x <= 7 && y >= 0 && y <= 4 && !tunnel;
        bool floor = y <= 0 && e < 1.6f;
        if ((e < 1 && y >= 0) || tunnel) { g.vox.set(x, y, z, BK_AIR); continue; }
        if (e < 1.7f || tunnelWall || floor) {
            float k = r.f();
            uint8_t nb = BK_STONE;
            if (k < 0.09f) nb = BK_CRYSTAL_ORE; else if (k < 0.19f) nb = BK_EMBER_ORE; else if (k < 0.23f) nb = BK_SCRAP_ORE; else if (k < 0.33f) nb = BK_COBBLE;
            if (y >= g.vox.groundHeight(x, z) - 1) continue;  // keep the hill's skin
            g.vox.set(x, y, z, nb);
        }
    }
    // glowing crystals where rays from the middle hit the walls
    for (int k = 0; k < 16; k++) {
        float a = k * 2.399f, el = -0.55f + 1.2f * r.f();
        V3 dir = norm(V3(std::cos(a) * std::cos(el), std::sin(el), std::sin(a) * std::cos(el)));
        V3 p = CAVE + V3(0, 0.2f, 0);
        for (int st = 0; st < 40; st++) {
            p += dir * 0.25f;
            int x = (int)std::floor(p.x), y = (int)std::floor(p.y), z = (int)std::floor(p.z);
            if (g.vox.get(x, y, z) != BK_AIR) { if (z > 5) g.vox.set(x, y, z, k % 3 == 0 ? BK_CRYSTAL_ORE : BK_GLOWCRYSTAL); break; }
        }
    }
    g.vox.set(4, 0, 9, BK_CANDLE); g.vox.set(4, -1, 9, BK_STONE);
    g.voxLightsDirty = true;
}
}  // namespace

std::vector<Shot> buildScript(std::vector<Caption>& caps, float spb, int W, int H) {
    std::vector<Shot> s;
    bool tall = H > W;
    float capY = tall ? 0.24f : 0.22f;
    V4 gold(1, 0.82f, 0.38f, 1), white(1, 0.97f, 0.9f, 1);
    float T = 0;
    auto add = [&](Shot sh) { sh.order = 100 + (int)s.size(); s.push_back(sh); T += sh.beats * spb; };
    auto cap = [&](float from, float to, const std::string& text, int size, float y, V4 col) { caps.push_back({T + from * spb, T + to * spb, text, size, y, col}); };
    auto ramp = [](float a, float b, float slow) { return [=](float k) { return (k > a && k < b) ? slow : 1.f; }; };

    // 1. DAWN: skimming the sea toward the island as the sun comes up
    cap(1.0f, 5, tall ? "NEW:\nWICK'S ISLAND" : "NEW: WICK'S ISLAND", 9, capY, gold);
    cap(1.8f, 5, tall ? "a whole block world\nto explore" : "a whole block world to explore", 4, capY + (tall ? 0.085f : 0.09f), white);
    { Shot sh{"dawn flyover", 5, [](Game& g) {
        g.enemies.clear(); g.save.dayT = 0.985f;
        put(g, V3(-6.5f, (float)ground(g, -7, 30), 30.5f), PI);
    }, [](Game& g, float t, float d, InputState& in) {
        float k = t / d;
        g.save.dayT = 0.985f + 0.06f * ease(k); if (g.save.dayT >= 1) g.save.dayT -= 1;
        g.mobT = 99; for (auto& e : g.enemies) e.alive = false;
        V3 eye = bez(V3(-14, 0.5f, 47), V3(-12, 15.0f, 39), V3(2, 29, 12), ease(k));
        V3 tgt = bez(V3(-4, 2, 20), V3(0, 3, 6), V3(8, 1, -16), ease(k));
        cam(g, eye, tgt); (void)in;
    }}; sh.flash = 0.9f; add(sh); }


    // 3. BUILD: a god's-eye timelapse that swings down to street level
    cap(0.3f, 6, tall ? "MINE & BUILD\nANYTHING" : "MINE & BUILD ANYTHING", 8, capY, white);
    { Shot sh{"build timelapse", 6, [](Game& g) {
        g.save.dayT = 0.06f;
        clearTrees(g, -2, 34, -42, -12);
        flatten(g, HX0 - 3, HX1 + 3, HZ0 - 5, HZ1 + 2, HY);
        g.vox.set(15, HY - 1, HZ0 - 1, BK_COBBLE); g.vox.set(15, HY - 1, HZ0 - 2, BK_COBBLE); g.vox.set(15, HY - 1, HZ0 - 3, BK_COBBLE);
        gPlan = housePlan(); gDone = 0;
        put(g, V3(15.5f, (float)HY, HZ0 - 4.5f), 0);
    }, [](Game& g, float t, float d, InputState& in) {
        std::vector<Put>& plan = gPlan; size_t& done = gDone;
        float buildEnd = d - 0.8f;
        size_t want = std::min(plan.size(), (size_t)(plan.size() * easeOut(t / buildEnd) + 0.5f));
        int sounds = 0;
        while (done < want) {
            auto& p = plan[done++];
            g.vox.set(p.x, p.y, p.z, (uint8_t)p.b, p.m);
            if (p.b == BK_CHEST) g.save.chests[VoxWorld::idx(p.x, p.y, p.z)];
            g.burst(V3(p.x + 0.5f, p.y + 0.5f, p.z + 0.5f), V3(1, 0.9f, 0.7f), 3, 1.5f, 0.05f, 0, 0.3f, 6);
            if (sounds++ < 1) audio::play(SFX_STEP, 0.55f, 0.8f + 0.6f * (float)done / plan.size());
        }
        if (done == plan.size() && t >= buildEnd - 1.f / 60 && t < buildEnd + 0.5f / 60) { audio::play(SFX_UNLOCK, 0.9f); g.burst(V3(15.5f, HY + 6.f, -18.5f), V3(1, 0.85f, 0.4f), 40, 6, 0.1f, 3, 1.0f, 2); }
        g.voxLightsDirty = true;
        float k = ease(t / d);
        V3 c(15.5f, HY + 2.f, -18.5f);
        float a = -PI / 2 - 0.55f + k * 0.25f;
        float r = 15 - 2.5f * k, h = 11 - 2 * k;
        cam(g, c + V3(std::cos(a) * r, h, std::sin(a) * r), c + V3(0, -0.5f, 0));
        g.pl.yaw = 0; (void)in;
    }}; add(sh); }

    // 4. CAVE: the camera follows Wick down a dark tunnel into a chamber of crystals
    cap(0.2f, 3, tall ? "DIG FOR CRYSTALS\n& ORE" : "DIG FOR CRYSTALS & ORE", 8, capY, V4(0.6f, 0.9f, 1, 1));
    { Shot sh{"cave fly-through", 3, [](Game& g) {
        carveCave(g);
        g.save.dayT = 0.8f; g.mobT = 99;
        put(g, V3(5.5f, 1.0f, 0.5f), 0);
    }, [](Game& g, float t, float d, InputState& in) {
        g.save.dayT = 0.8f; g.mobT = 99;
        for (auto& e : g.enemies) e.alive = false;
        float k = ease(t / d);
        in.moveY = g.pl.pos.z < 8.0f ? 0.8f : 0;
        g.camYaw = 0;
        V3 p = g.pl.pos;
        cam(g, V3(5.5f + 0.8f * k, 2.4f + 0.6f * k, p.z - 2.6f - 0.8f * k), V3(5.5f - k, 1.6f + 0.6f * k, p.z + 4)); (void)d;
    }}; sh.flash = 0.5f; add(sh); }


    // 6. BOMBS: a chain of ember bombs in slow motion
    cap(0.2f, 3, tall ? "BLOW STUFF UP" : "BLOW STUFF UP", 8, capY, V4(1, 0.7f, 0.3f, 1));
    { Shot sh{"bomb chain", 3, [](Game& g) {
        g.save.dayT = 0.12f; g.enemies.clear();
        for (int z = -24; z <= -12; z++) for (int x = -16; x <= -9; x++) {
            int top = 4 - std::max(0, std::abs(z + 18) - 4) / 2 - (x > -11 ? x + 11 : 0);
            for (int y = 0; y <= std::max(top, 0); y++) g.vox.set(x, y, z, y == top ? BK_GRASS : ((x * 7 + y * 3 + z) % 5 == 0 ? BK_EMBER_ORE : (y > top - 2 ? BK_DIRT : BK_STONE)));
        }
        g.bombs.clear();
        for (int k = 0; k < 5; k++) g.vox.set(-10, 1, -22 + k * 2, BK_BOMB);
        g.bombs.push_back({-10, 1, -22, 0.55f});
        put(g, V3(-4.5f, 0.0f, -10.5f), -PI * 0.75f);
        audio::play(SFX_CHARGE, 0.8f, 1.4f);
    }, [](Game& g, float t, float d, InputState& in) {
        float k = ease(t / d);
        cam(g, V3(-2.5f - k * 1.0f, 2.2f + k * 0.8f, -25.5f + k * 1.5f), V3(-10.5f, 1.6f, -17.0f)); (void)in;
    }}; sh.speed = ramp(0.2f, 0.9f, 0.4f); add(sh); }

    // 7. NIGHT: lamps glow, gloomlings come, Wick fights back
    cap(0.2f, 4, tall ? "SURVIVE\nTHE NIGHT" : "SURVIVE THE NIGHT", 8, capY, V4(0.8f, 0.7f, 1, 1));
    cap(0.7f, 4, "gloomlings come out after dark", 4, capY + (tall ? 0.085f : 0.075f), white);
    { Shot sh{"night fight", 4, [](Game& g) {
        g.save.dayT = 0.7f; g.mobT = 99;
        g.enemies.clear();
        put(g, V3(15.5f, (float)HY, HZ0 - 5.5f), PI);
        for (int k = 0; k < 5; k++) g.spawnEnemy(E_GLOOMLING, V3(10.5f + k * 2.5f, (float)ground(g, 10 + k * 2, -33) + 0.3f, -33.f + (k % 2) * 2), -1);
    }, [](Game& g, float t, float d, InputState& in) {
        g.mobT = 99; g.save.dayT = 0.7f;
        static float next = 0;
        if (t < 0.02f) next = 0.3f;
        if (t > next) {
            Enemy* best = nullptr; float bd = 1e9f;
            for (auto& e : g.enemies) if (e.alive && len(e.pos - g.pl.pos) < bd) { bd = len(e.pos - g.pl.pos); best = &e; }
            if (best && g.flame.st == F_HELD && bd < 14) {
                g.botAim = true; g.botAimPoint = best->pos + V3(0, 0.4f, 0);
                g.pl.yaw = std::atan2(best->pos.x - g.pl.pos.x, best->pos.z - g.pl.pos.z);
                g.throwFlame(false);
                g.botAim = false;
                next = t + 0.7f;
            } else next = t + 0.1f;
        }
        if ((g.flame.st == F_FLYING || g.flame.st == F_HOVER) && len(g.flame.pos - g.pl.pos) > 8) g.recallFlame();
        V3 p = g.pl.pos;
        float k = ease(t / d);
        cam(g, p + V3(4.0f - k * 1.0f, 1.2f, -1.5f), p + V3(-1.0f, 1.0f, -4.5f)); (void)in;
    }}; sh.speed = [](float k) { return k < 0.5f ? 1.f : 0.6f; }; add(sh); }

    // 8. THE END: golden hour, pull back from the house to the whole island
    float ly = tall ? 0.17f : 0.19f;
    cap(0.3f, 9, "WICKLIGHT", 12, ly, gold);
    cap(1.0f, 9, "OUT NOW", 8, ly + (tall ? 0.07f : 0.115f), white);
    cap(2.0f, 9, "free & open source  -  Linux", 4, ly + (tall ? 0.115f : 0.19f), white);
    cap(2.5f, 9, "github.com/rxvy-dev/wicklight", 4, ly + (tall ? 0.145f : 0.24f), gold);
    if (tall) cap(2.5f, 9, "(link in bio)", 3, ly + 0.17f, white);
    cap(4.5f, 9, tall ? "WHAT SHOULD WE\nADD NEXT?" : "WHAT SHOULD WE ADD NEXT?", 6, tall ? 0.70f : 0.80f, white);
    { Shot sh{"end", 9, [](Game& g) {
        g.enemies.clear();
        g.save.dayT = 0.5f;
        put(g, V3(15.5f, (float)HY, HZ0 - 2.5f), PI);
    }, [](Game& g, float t, float d, InputState& in) {
        float k = ease(t / d);
        g.save.dayT = 0.5f + 0.06f * k;
        g.mobT = 99;
        for (auto& e : g.enemies) e.alive = false;
        V3 c(15.5f, HY + 1.5f, -20.f);
        cam(g, c + V3(-2 - k * 14, 1.2f + k * 16, -5 - k * 20), c + V3(-k * 8, 0.8f + k * 0.5f, k * 6)); (void)in;
    }}; sh.flash = 0.6f; add(sh); }
    (void)W;
    return s;
}

// ================================================================== the adventure clips
// Offsets come from a logged run (SHOWCASE_PART=log): they land on warps, kills and boss hits.
// `order` is the position in the finished trailer; clips are listed here in the order the bot plays them.
std::vector<CampClip> campaignClips(int W, int H) {
    bool tall = H > W;
    if (getenv("SHOWCASE_SURVEY")) {
        std::vector<CampClip> v;
        for (int st = 0; st < 8; st++) { CampClip k{70 + st, "ending survey", "", 0.0f, 5.8f * 148 / 60, CM_GAME, {}}; k.cine = st; v.push_back(k); }
        return v;
    }
    float y = tall ? 0.24f : 0.14f;
    V4 gold(1, 0.82f, 0.38f, 1), white(1, 0.97f, 0.9f, 1), red(1, 0.45f, 0.35f, 1), blue(0.6f, 0.9f, 1, 1);
    auto cap = [&](float b0, float b1, const char* wide, const char* narrow, int size, V4 col, float yy = -1) {
        return Caption{b0, b1, tall ? narrow : wide, size, yy < 0 ? y : yy, col};
    };
    std::vector<CampClip> c;
    (void)y; (void)gold; (void)white; (void)red; (void)blue; (void)cap;
    auto clip = [&](int order, const char* name, const char* section, float offset, float beats, CamMode cam, float dist = 6.5f, float height = 2.2f) {
        CampClip k{order, name, section, offset, beats, cam, {}, dist, height}; c.push_back(k); return &c.back();
    };
    // ACT 2, on SONG_FINAL: what you can do, one thing per shot, on a steady chase camera
    float fy = tall ? 0.22f : 0.21f, sy = fy + (tall ? 0.05f : 0.075f);
    auto feat = [&](CampClip* k, const char* big, const char* bigTall, const char* small, V4 col) {
        k->caps = {Caption{0, k->beats, tall ? bigTall : big, 8, fy, col}};
        if (small && *small) k->caps.push_back(Caption{0.5f, k->beats, small, 4, sy, white});
    };
    V4 cyan(0.5f, 0.95f, 1, 1), orange(1, 0.7f, 0.3f, 1), green(0.5f, 1, 0.6f, 1);
    clip(6, "title: Lumen Hollow", "HUB: start", 1.2f, 4, CM_TITLE)->caps = {Caption{0.0f, 4, "WICKLIGHT", 13, tall ? 0.42f : 0.40f, gold}};
    feat(clip(7, "throw: market fight", "WHARF: market fight", 0.3f, 5, CM_FOLLOW, 6.0f, 2.2f),
         "THROW YOUR FLAME", "THROW YOUR\nFLAME", "it burns going out and coming back", gold);
    feat(clip(8, "warp: stepping stones", "WHARF: stepping stones", 3.6f, 5, CM_FOLLOW, 6.5f, 2.4f),
         "WARP TO IT", "WARP TO IT", "teleport to wherever your flame lands", cyan);
    feat(clip(15, "boss: Bellwether", "WHARF: boss - Drowned Bellwether", 13.4f, 4, CM_BOSS, 7.0f, 2.6f), "BEAT 5 BOSSES", "BEAT 5\nBOSSES", "", red);
    feat(clip(11, "worlds: great tree", "CLOCK: great tree", 0.2f, 5, CM_FOLLOW, 6.5f, 2.4f),
         "EXPLORE 6 WORLDS", "EXPLORE\n6 WORLDS", "harbors, clockwork forests, lava vaults, a junkyard", gold);
    clip(16, "boss: GL1TCH", "CLOCK: boss - GL1TCH the hacker", 6.6f, 4, CM_FOLLOW, 7.5f, 3.2f);
    feat(clip(9, "bloom: sealed brazier", "VAULTS: sealed brazier", 1.1f, 5, CM_FOLLOW, 6.5f, 2.6f),
         "BLOOM", "BLOOM", "blow up your flame wherever it is", orange);
    clip(17, "boss: Veil Matron", "VAULTS: boss - Veil Matron", 6.3f, 4, CM_BOSS, 7.0f, 2.6f);
    c.push_back({0, "setup: robot squad", "HUB: Sprocket's robots", 14.0f, 0, CM_FOLLOW, {}, 6, 2, [](Game& g) {
        for (int i = 0; i < 5; i++) g.setFlag("bot.own." + std::to_string(i));
        g.setFlag("spark.core");
        for (int i = 0; i < 5; i++) g.save.flags.erase("bot.eq." + std::to_string(i));
        for (int i : {0, 3, 4}) g.setFlag("bot.eq." + std::to_string(i));
    }});
    { auto* k = clip(12, "menu: robot shop", "HUB: Sprocket's robots", 14.05f, 5, CM_GAME); k->menu = SC_BOTSHOP;
      k->caps = {Caption{0, 5, tall ? "BUY ROBOT\nSIDEKICKS" : "BUY ROBOT SIDEKICKS", 8, tall ? 0.80f : 0.80f, cyan}}; }
    feat(clip(13, "robots: rat street", "JUNK: rat street", 1.0f, 5, CM_FOLLOW, 6.5f, 2.6f),
         "THEY FIGHT WITH YOU", "THEY FIGHT\nWITH YOU", "fireball drones, bomb lobbers, shields, medics", green);
    clip(18, "boss: King Rattletrap", "JUNK: boss - King Rattletrap", 5.2f, 4, CM_BOSS, 7.0f, 2.6f);
    feat(clip(10, "sunflare: shielded knight", "SPIRE: shielded knight", 1.0f, 5, CM_FOLLOW, 6.5f, 2.6f),
         "CHARGE A SUNFLARE", "CHARGE A\nSUNFLARE", "triple damage - smashes shields", V4(1, 0.95f, 0.6f, 1));
    { auto* k = clip(19, "reveal: The Hush", "SPIRE: final boss - The Hush", 0.4f, 4, CM_GAME); k->flash = 0.6f;
    }
    { auto* k = clip(20, "kill: The Hush (slow motion)", "SPIRE: final boss - The Hush", 31.1f, 6, CM_BOSS, 8.0f, 2.6f);
      k->speed = [](float x) { return x < 0.4f ? 1.f : 0.35f; }; }
    // the ending cinematic: the Great Lamp burns again - SONG_ENDING starts as it ignites
    { auto* k = clip(22, "ending: before the Lamp", "", 2.5f - 2 * 60.f / 148, 2, CM_GAME); k->cine = 2; }
    { auto* k = clip(50, "ending: the Lamp ignites", "", 2.5f, 4, CM_GAME); k->cine = 2; k->bpm = 76; k->flash = 1.0f; }
    return c;
}

// ================================================================== the intro clips (SONG_TITLE)
std::vector<CampClip> introClips(int W, int H) {
    std::vector<CampClip> c;
    (void)W; (void)H;
    if (getenv("SHOWCASE_SURVEY")) {
        for (int st = 0; st < 7; st++) { CampClip k{60 + st, "intro survey", "", 0.0f, 5.4f * 68 / 60, CM_GAME, {}}; k.cine = st; k.bpm = 68; c.push_back(k); }
        return c;
    }
    bool tall = H > W;
    V4 white(1, 0.97f, 0.9f, 1), gold(1, 0.82f, 0.38f, 1);
    float mid = tall ? 0.42f : 0.40f;
    auto clip = [&](int order, const char* name, int step, float t0, float beats, float bpm) {
        CampClip k{order, name, "", t0, beats, CM_GAME, {}}; k.cine = step; k.bpm = bpm; c.push_back(k); return &c.back();
    };
    // cold open, on the music box (SONG_TITLE, 68 bpm)
    clip(1, "intro: the Spire", 1, 0.6f, 3, 68)->caps = {Caption{0.4f, 3, tall ? "THE LIGHT\nWENT OUT." : "THE LIGHT WENT OUT.", 9, mid, white}};
    clip(3, "intro: the workshop", 4, 1.0f, 2, 68)->caps = {Caption{0.2f, 2, tall ? "ONE FLAME\nWAS LEFT." : "ONE FLAME WAS LEFT.", 9, mid, white}};
    clip(4, "intro: before the spark", 5, 1.5f - 60.f / 68, 1, 68);
    // the drop: Wick ignites exactly on beat one of SONG_FINAL
    CampClip* w = clip(5, "intro: Wick ignites", 5, 1.5f, 4, 148);
    w->flash = 1.0f;
    w->caps = {Caption{0.5f, 4, tall ? "WAKE UP,\nWICK." : "WAKE UP, WICK.", 10, tall ? 0.30f : 0.22f, gold}};
    return c;
}

