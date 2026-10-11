// WICKLIGHT - keep-playing progression:
//  WICK'S LEVEL    everything you do earns XP; every level pays embers and pet treats, every 5th level a new hat
//  DAILY STREAK    come back each day for a bigger gift (day 7: a jackpot and a rare Starling nearby)
//  GOLDEN MONSTERS 1 in 35 enemies is golden: tougher, sparkly, and drops a fortune
#include "builder.h"
#include <cstdio>
#include <ctime>
#include <algorithm>

const char* hatName(int h);
static V4 C4(uint32_t h, float a = 1) { V3 c = hexColor(h); return V4(c.x, c.y, c.z, a); }
static const V4 COL_DIM = C4(0x8a8098), COL_HI = C4(0xffc060);

int xpNeed(int lvl) { return 80 + 40 * lvl; }
static bool progressOn(const Game& g) { return !g.botActive || getenv("WICKLIGHT_XP"); }

static int levelHat(int lvl) {
    switch (lvl) { case 5: return 11; case 10: return 12; case 15: return 13; case 20: return 14; default: return 0; }
}

void Game::gainXp(int xp, V3 at) {
    if (!progressOn(*this) || xp <= 0) return;
    xp = (int)std::lround(xp * (1 + petXpBonus()));
    save.wxp += xp;
    if (at.y > -900 && xp >= 10) floatText(at + V3(0, 2.0f, 0), "+" + std::to_string(xp) + " XP", hexColor(0x80c0ff));
    while (save.wxp >= xpNeed(save.wlvl)) {
        save.wxp -= xpNeed(save.wlvl);
        save.wlvl++;
        int L = save.wlvl;
        int emb = 15 + 5 * L, tr = 2 + L / 5;
        save.embers += emb; save.treats += tr;
        std::string extra;
        int hat = levelHat(L);
        if (hat && !has("hat." + std::to_string(hat))) { setFlag("hat." + std::to_string(hat)); extra = std::string("   NEW HAT: ") + hatName(hat) + "!"; }
        showToast("LEVEL UP!  WICK IS LEVEL " + std::to_string(L), "+" + std::to_string(emb) + " embers   +" + std::to_string(tr) + " pet treats" + extra);
        audio::play(SFX_HEART, 0.8f, 1.2f); audio::play(SFX_BELL, 0.7f, 1.6f);
        burst(pl.pos + V3(0, 1, 0), hexColor(0x80c0ff), 50, 6, 0.1f, 3, 1.0f, -1);
        burst(pl.pos + V3(0, 1, 0), hexColor(0xffe080), 30, 4, 0.12f, 3, 1.2f, -2);
        lvlUpT = 2.5f;
    }
    if (xp >= 30) writeSave();
}

static int dayNumber() {
    if (getenv("WICKLIGHT_DAY")) return atoi(getenv("WICKLIGHT_DAY"));
    return (int)((long long)time(nullptr) / 86400);
}

void Game::checkStreak() {
    if (streakChecked || screen != SC_PLAY || !dialog.empty() || transitioning || areaTitleT > 0.5f || toastT > 0) return;
    if (botActive && !getenv("WICKLIGHT_STREAK")) { streakChecked = true; return; }
    streakChecked = true;
    int d = dayNumber();
    if (save.streakDay == d) return;
    save.streak = (save.streakDay == d - 1) ? save.streak + 1 : 1;
    save.streakDay = d;
    save.bestStreak = std::max(save.bestStreak, save.streak);
    int n = save.streak, day = (n - 1) % 7 + 1;
    int emb = 25 * day, tr = 1 + day / 2;
    bool jackpot = day == 7;
    if (jackpot) emb += 250;
    save.embers += emb; save.treats += tr;
    showToast("DAY " + std::to_string(n) + " STREAK!", "+" + std::to_string(emb) + " embers   +" + std::to_string(tr) + " pet treats" + (jackpot ? "   JACKPOT - and a STARLING is nearby!" : "   - come back tomorrow for more!"));
    audio::play(SFX_CHECKPOINT); audio::play(SFX_SHARD, 0.8f, 1.2f);
    gainXp(20 * day, pl.pos);
    if (jackpot) {  // a rare Starling appears close by
        for (int tries = 0; tries < 30; tries++) {
            float a = tries * 0.9f, r = 5 + (tries % 4);
            V3 p = pl.pos + V3(std::sin(a) * r, 0, std::cos(a) * r);
            float y = groundHeightBelow(p + V3(0, 6, 0), 14);
            if (y < -1e8f || std::fabs(y - pl.pos.y) > 3 || (lv.waterY > -999 && y < lv.waterY + 0.3f)) continue;
            p.y = y;
            WildPet w; w.kind = 7; w.pos = w.home = w.target = p;
            wildPets.push_back(w);
            break;
        }
    }
    writeSave();
}

void Game::goldenKilled(Enemy& e) {
    save.goldenKills++;
    dropEmbers(e.pos, 30);
    save.treats += 1;
    gainXp(40, e.pos);
    burst(e.pos + V3(0, e.height * 0.5f, 0), hexColor(0xffd040), 50, 7, 0.12f, 3, 1.0f, 2);
    audio::play(SFX_SHARD, 1, 0.9f);
    floatText(e.pos + V3(0, 2.4f, 0), "GOLDEN! +30", hexColor(0xffd040));
    writeSave();
}

// sparkles around golden monsters
void drawGoldenAuras(Game& g, Renderer* R) {
    if (R->shadowPass) return;
    for (auto& e : g.enemies) {
        if (!e.alive || !e.golden) continue;
        V3 c = e.pos + V3(0, e.height * 0.5f, 0);
        R->drawPart(PM_SPHERE, M4::translate(c) * M4::scale(V3(e.radius * 3.2f, e.height * 1.6f, e.radius * 3.2f)), T_WHITE, hexColor(0xffd040), 1.2f, 0, 0.18f + 0.06f * std::sin(g.time * 6));
        for (int k = 0; k < 4; k++) {
            float a = g.time * 2.5f + k * TAU / 4 + e.id;
            R->drawPart(PM_OCTA, M4::translate(c + V3(std::sin(a) * (e.radius + 0.5f), 0.4f * std::sin(g.time * 3 + k), std::cos(a) * (e.radius + 0.5f))) * M4::scale(V3(0.1f, 0.16f, 0.1f)), T_WHITE, hexColor(0xfff0a0), 2.5f);
        }
    }
}
void goldenLights(Game& g, Renderer* R) {
    for (auto& e : g.enemies) if (e.alive && e.golden) R->lights.push_back({e.pos + V3(0, e.height, 0), 6, hexColor(0xffd040), 1.2f});
}

// the XP bar along the bottom of the screen
void drawProgressHUD(Game& g) {
    if (!progressOn(g)) return;
    Renderer* R = g.R;
    float s = (float)R->uiScale();
    float W = (float)R->winW, H = (float)R->winH;
    if (g.lv.id == LV_HOME && g.vox.active) return;  // the hotbar lives there
    float bw = std::min(W * 0.4f, 200 * s), bx = W / 2 - bw / 2, by = H - 9 * s;
    float f = clampf((float)g.save.wxp / xpNeed(g.save.wlvl), 0, 1);
    R->uiRect(bx, by, bw, 4 * s, C4(0x1a1624, 0.8f));
    R->uiRect(bx, by, bw * f, 4 * s, g.lvlUpT > 0 && std::fmod(g.time * 8, 1.f) < 0.5f ? C4(0xffffff) : C4(0x60a8ff));
    char b[48]; snprintf(b, sizeof(b), "Lv %d", g.save.wlvl);
    R->uiText(bx - R->uiTextWidth(b, s) - 4 * s, by - 2 * s, b, s, COL_HI);
    if (g.save.streak > 1) { std::string st = "streak " + std::to_string(g.save.streak); R->uiText(bx + bw + 4 * s, by - 2 * s, st, s, COL_DIM); }
}

void updateProgress(Game& g, float dt) {
    g.lvlUpT = std::max(0.f, g.lvlUpT - dt);
    g.checkStreak();
}
