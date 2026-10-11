// WICKLIGHT - PETS
// Eight kinds of little creatures live around the world. Burn one with your thrown flame (or a Bloom) to daze it,
// then press E to catch it. Your buddy follows you everywhere, helps out with its own perk, earns XP when you
// burn enemies, levels up to 10 and evolves at level 5. Tamsin, the pet keeper in the Hollow, sells treats.
#include "builder.h"
#include <cstdio>
#include <algorithm>

extern int g_dmgKind;
static V4 C4(uint32_t h, float a = 1) { V3 c = hexColor(h); return V4(c.x, c.y, c.z, a); }
static const V4 COL_TEXT = C4(0xf0e6d0), COL_DIM = C4(0x8a8098), COL_HI = C4(0xffc060);
static Rng prng(1357);

struct PetKind { const char* name; const char* perk; uint32_t col, col2; const char* where; };
static const PetKind PETS[8] = {
    {"MOTHLING", "Ember Magnet: pulls embers to you from further away", 0xe8e0d0, 0xb0a0ff, "Lumen Hollow, the Clockwood, the Spire"},
    {"EMBER FROG", "Fire Spit: spits fireballs at nearby enemies", 0xff7a30, 0xffd040, "the Sunken Wharf"},
    {"GLOW SNAIL", "Wax Mend: slowly heals your flame", 0x60e080, 0xd0ffa0, "the Sunken Wharf"},
    {"PUFFCAP", "Bouncy: extra jumps in mid-air", 0xe04050, 0xfff0e0, "the Clockwood"},
    {"GLOOM PUP", "Howl: hurts every enemy around you now and then", 0x3a2a50, 0xb060ff, "the Ashen Vaults, the Spire"},
    {"BYTE BUG", "Lucky Code: enemies drop more embers", 0x40ff90, 0x103020, "the Junkyard, Lumen City"},
    {"LANTERN FOX", "Treasure Nose: digs up embers as you explore", 0xff9040, 0xfff0c0, "Lumen Hollow, the Junkyard"},
    {"STARLING", "Lucky Star: you earn more XP", 0xffd040, 0xffffff, "anywhere - but very rare (and on day-7 streak gifts)"},
};
static const char* NAMES[] = {"Biscuit", "Pip", "Mochi", "Sprout", "Nugget", "Pebble", "Waffles", "Noodle", "Ziggy", "Tofu", "Bean", "Pickle",
                              "Sparky", "Muffin", "Taco", "Ember", "Gizmo", "Button", "Cinder", "Wobble", "Snickers", "Fig", "Bramble", "Toast"};
static const int NUM_NAMES = 24;
static int petNeed(int lv) { return 20 + lv * 15; }

const char* petKindName(int k) { return (k >= 0 && k < 8) ? PETS[k].name : "?"; }

// which kinds live where
static std::vector<int> habitat(LevelId id) {
    switch (id) {
        case LV_HUB: return {0, 6};
        case LV_WHARF: return {1, 2};
        case LV_CLOCK: return {3, 0};
        case LV_VAULTS: return {4};
        case LV_SPIRE: return {4, 0};
        case LV_JUNK: return {5, 6};
        case LV_CITY: return {5};
        case LV_ENDLESS: return {0, 1, 2, 3, 4, 5, 6};
        default: return {};
    }
}

// ------------------------------------------------------------------ perks
float Game::petMagnet() const { return (save.activePet == 0) ? 2.0f + 0.6f * save.petLv[0] : 0.f; }
int Game::petAirJumps() const { return save.activePet == 3 ? (save.petLv[3] >= 5 ? 2 : 1) : 0; }
float Game::petEmberBonus() const { return save.activePet == 5 ? 0.1f + 0.04f * save.petLv[5] : 0.f; }
float Game::petXpBonus() const { return save.activePet == 7 ? 0.25f + 0.05f * save.petLv[7] : 0.f; }

void Game::petXp(int kind, int xp) {
    if (kind < 0 || kind >= 8 || save.petLv[kind] <= 0 || save.petLv[kind] >= 10) return;
    save.petXp[kind] += xp;
    while (save.petLv[kind] < 10 && save.petXp[kind] >= petNeed(save.petLv[kind])) {
        save.petXp[kind] -= petNeed(save.petLv[kind]);
        save.petLv[kind]++;
        int lv = save.petLv[kind];
        std::string n = NAMES[save.petName[kind] % NUM_NAMES];
        if (lv == 5) showToast(n + " EVOLVED!", std::string("your ") + PETS[kind].name + " grew bigger and stronger (level 5)");
        else if (lv == 10) showToast(n + " IS MAXED OUT!", std::string("a legendary ") + PETS[kind].name + " - level 10!");
        else { floatText(petPos + V3(0, 1.4f, 0), n + " Lv " + std::to_string(lv) + "!", hexColor(0x80ff90)); }
        audio::play(lv == 5 || lv == 10 ? SFX_HEART : SFX_SHARD, 0.8f, 1.3f);
        burst(petPos + V3(0, 0.6f, 0), hexColor(PETS[kind].col2), 30, 4, 0.1f, 3, 0.8f, 0);
        petFlash = 1;
    }
    if (save.petLv[kind] >= 10) save.petXp[kind] = 0;
}

// ------------------------------------------------------------------ wild pets
static bool groundAt(Game& g, V3& p) {
    float y = g.groundHeightBelow(p + V3(0, 8, 0), 20);
    if (y < -1e8f) return false;
    if (g.lv.waterY > -999 && y < g.lv.waterY + 0.3f) return false;
    p.y = y;
    return g.boxFree(AABB(p + V3(-0.4f, 0.1f, -0.4f), p + V3(0.4f, 1.2f, 0.4f)));
}

void Game::spawnWildPets(int n) {
    auto hab = habitat(lv.id);
    if (hab.empty()) return;
    std::vector<V3> seeds;
    for (auto& l : lv.lanterns) seeds.push_back(l.pos);
    for (auto& s : lv.spawns) seeds.push_back(s.pos);
    for (auto& b : lv.braziers) seeds.push_back(b.pos);
    for (auto& t : lv.talkers) seeds.push_back(t.pos);
    if (seeds.empty()) return;
    for (int k = 0; k < n; k++) {
        for (int tries = 0; tries < 40; tries++) {
            V3 c = seeds[prng.irange(0, (int)seeds.size() - 1)];
            float a = prng.range(0, TAU), d = prng.range(4, 12);
            V3 p = c + V3(std::sin(a) * d, 0, std::cos(a) * d);
            p.y = c.y;
            if (!groundAt(*this, p)) continue;
            if (std::fabs(p.y - c.y) > 6) continue;
            if (distXZ(p, pl.pos) < 6) continue;
            WildPet w; w.kind = hab[prng.irange(0, (int)hab.size() - 1)];
            float rare = lv.id == LV_ENDLESS ? 0.08f : 0.03f;
            if (prng.f() < rare) w.kind = 7;
            w.pos = w.home = w.target = p; w.yaw = prng.range(0, TAU); w.t = prng.range(0, 3);
            wildPets.push_back(w);
            break;
        }
    }
}

static void catchPet(Game& g, Game::WildPet& w) {
    w.alive = false;
    int k = w.kind;
    g.burst(w.pos + V3(0, 0.5f, 0), hexColor(PETS[k].col2), 40, 4, 0.1f, 3, 1.0f, -1);
    audio::play(SFX_CATCH, 1, 1.3f); audio::play(SFX_SHARD, 0.8f, 1.5f);
    if (g.save.petLv[k] <= 0) {
        g.save.petLv[k] = 1; g.save.petXp[k] = 0; g.save.petName[k] = prng.irange(0, NUM_NAMES - 1);
        g.save.petsCaught++;
        bool first = g.save.activePet < 0;
        if (first) { g.save.activePet = k; g.petPos = w.pos; }
        g.showToast(std::string("CAUGHT A ") + PETS[k].name + "!", std::string("say hi to ") + NAMES[g.save.petName[k]] + "   (" + std::to_string(g.save.petsCaught) + "/8 kinds)" + (first ? "  - your new buddy!" : "  - swap buddies in the pause menu > Pets"));
        g.gainXp(50, w.pos);
        if (first && !g.has("pet.tut")) {
            g.setFlag("pet.tut");
            g.startDialog({{"", std::string(NAMES[g.save.petName[k]]) + " the " + PETS[k].name + " will follow you everywhere now."},
                           {"", std::string("Each kind of pet has a PERK. ") + PETS[k].perk + "."},
                           {"", "Pets earn XP when you burn enemies, grow up to level 10, and EVOLVE at level 5. Pause menu > Pets to see them all."},
                           {"", "Tamsin the pet keeper in Lumen Hollow sells treats that give lots of XP. There are 8 kinds of pet to find!"}});
        }
    } else {
        g.save.treats += 2;
        g.petXp(k, 30);
        g.showToast(std::string("ANOTHER ") + PETS[k].name + "!", std::string(NAMES[g.save.petName[k] % NUM_NAMES]) + " made a friend: +30 pet XP and +2 treats");
        g.gainXp(15, w.pos);
    }
    g.writeSave();
}

// ------------------------------------------------------------------ per step
void Game::updatePets(float dt, const InputState& in, bool& usedE) {
    // wild pets for this level
    if (wildLevelId != (int)lv.id) {
        wildLevelId = (int)lv.id;
        wildPets.clear();
        wildRespawnT = 60;
        if (!botActive || getenv("WICKLIGHT_PETS")) spawnWildPets(lv.id == LV_ENDLESS ? 1 : 2);
        if (save.activePet >= 0) petPos = pl.pos + V3(1, 0, -1);
    }
    wildRespawnT -= dt;
    if (wildRespawnT <= 0) { wildRespawnT = 90; int alive = 0; for (auto& w : wildPets) if (w.alive) alive++; if (alive < 2 && (!botActive || getenv("WICKLIGHT_PETS"))) spawnWildPets(1); }
    for (auto& w : wildPets) {
        if (!w.alive) continue;
        w.t += dt;
        w.daze = std::max(0.f, w.daze - dt);
        // dazed by the flame or a Bloom
        bool hitFlame = (flame.st == F_FLYING || flame.st == F_HOVER) && len(flame.pos - (w.pos + V3(0, 0.4f, 0))) < 1.2f;
        bool hitBloom = bloomFlash > 0.95f && len(bloomPos - w.pos) < 5.5f;
        if ((hitFlame || hitBloom) && w.daze <= 0) {
            w.daze = 6; w.flee = 0;
            burst(w.pos + V3(0, 0.6f, 0), hexColor(0xffe080), 14, 3, 0.07f, 3, 0.5f, 0);
            audio::playAt(SFX_TICK, w.pos, 1, 1.8f);
            floatText(w.pos + V3(0, 1.2f, 0), "DAZED!", hexColor(0xffe080));
            if (statusT <= 0.3f) { statusMsg = std::string("The ") + PETS[w.kind].name + " is dazed - quick, press [" + bindingName(settings.keys[A_INTERACT]) + "] next to it to CATCH it!"; statusT = 3; }
        }
        float dp = distXZ(w.pos, pl.pos);
        if (w.daze > 0) {
            if (in.pressed[A_INTERACT] && !usedE && dp < 2.4f && !pl.dead) { usedE = true; catchPet(*this, w); }
            continue;
        }
        // skittish: hop away when Wick comes close
        V3 want = w.target;
        if (dp < 4.5f && !pl.dead) { w.flee = 1.5f; }
        if (w.flee > 0) { w.flee -= dt; V3 away = flatXZ(w.pos - pl.pos); if (len(away) > 0.01f) want = w.pos + norm(away) * 3; }
        else if (distXZ(w.pos, w.target) < 0.6f || prng.f() < dt * 0.15f) {
            float a = prng.range(0, TAU), d = prng.range(1, 4.5f);
            w.target = w.home + V3(std::sin(a) * d, 0, std::cos(a) * d);
        }
        V3 to = flatXZ(want - w.pos); float tl = len(to);
        float sp = w.flee > 0 ? 4.6f : 1.4f;
        if (w.kind == 7 && w.flee > 0) sp = 5.6f;   // starlings are quick
        if (tl > 0.1f) {
            V3 step = to / tl * std::min(tl, sp * dt);
            V3 np = w.pos + step;
            V3 g2 = np;
            if (groundAt(*this, g2) && std::fabs(g2.y - w.pos.y) < 1.2f && distXZ(g2, w.home) < 14) { w.pos = g2; w.yaw = std::atan2(step.x, step.z); }
            else { w.target = w.home; w.flee = 0; }
        }
        w.hop += dt * (w.flee > 0 ? 9 : 4);
    }
    wildPets.erase(std::remove_if(wildPets.begin(), wildPets.end(), [](const WildPet& w) { return !w.alive; }), wildPets.end());

    // your buddy
    int k = save.activePet;
    if (k < 0 || save.petLv[k] <= 0) { save.activePet = -1; return; }
    petT += dt; petFlash = std::max(0.f, petFlash - dt);
    V3 fwd(std::sin(pl.yaw), 0, std::cos(pl.yaw)), side(fwd.z, 0, -fwd.x);
    V3 want = pl.pos - fwd * 1.5f + side * 1.0f;
    if (len(petPos - pl.pos) > 16 || !(petPos.y > -1e6f)) petPos = want;
    V3 d = want - petPos;
    float dl = len(flatXZ(d));
    V3 mv = flatXZ(d) * std::min(1.f, dt * 4.f);
    petPos.x += mv.x; petPos.z += mv.z;
    petPos.y += (pl.pos.y - petPos.y) * std::min(1.f, dt * 6.f);
    if (dl > 0.3f) petYaw = std::atan2(mv.x, mv.z); else petYaw = std::atan2(pl.pos.x - petPos.x, pl.pos.z - petPos.z);
    petHop += dt * (dl > 0.5f ? 10 : 2);
    if (pl.dead) return;
    int lvl = save.petLv[k];
    // perks
    if (k == 1) {  // Fire Spit
        petSpitT -= dt;
        if (petSpitT <= 0) {
            Enemy* best = nullptr; float bd = 9.f;
            for (auto& e : enemies) if (e.alive && e.type != E_DECOY) { float dd = len(e.pos - petPos); if (dd < bd) { bd = dd; best = &e; } }
            if (best) {
                petSpitT = std::max(1.2f, 3.0f - 0.15f * lvl);
                V3 a = petPos + V3(0, 0.5f, 0), b = best->pos + V3(0, best->height * 0.5f, 0);
                for (int i = 0; i <= 6; i++) burst(lerp(a, b, i / 6.f), hexColor(0xff8030), 2, 1, 0.1f, 3, 0.3f, 0);
                audio::playAt(SFX_THROW, a, 0.6f, 1.6f);
                damageEnemy(*best, 1.0f + 0.25f * (lvl / 3), petPos, false);
            } else petSpitT = 0.5f;
        }
    }
    if (k == 2) {  // Wax Mend
        petHealT += dt;
        if (petHealT > std::max(8.f, 25.f - 1.6f * lvl)) {
            petHealT = 0;
            if (pl.hp < maxHp()) { pl.hp = std::min(maxHp(), pl.hp + 1); burst(pl.pos + V3(0, 1, 0), hexColor(0x80ff90), 16, 2, 0.08f, 3, 0.6f, -1); audio::play(SFX_CATCH, 0.6f, 1.6f); floatText(pl.pos + V3(0, 1.8f, 0), "+1", hexColor(0x80ff90)); }
        }
    }
    if (k == 4) {  // Howl
        petHowlT -= dt;
        if (petHowlT <= 0) {
            int near = 0; for (auto& e : enemies) if (e.alive && len(e.pos - pl.pos) < 5.5f) near++;
            if (near) {
                petHowlT = std::max(3.5f, 9.f - 0.5f * lvl);
                audio::play(SFX_ROAR, 0.5f, 1.8f);
                burst(petPos + V3(0, 0.6f, 0), hexColor(0xb060ff), 30, 6, 0.1f, 3, 0.5f, 0);
                g_dmgKind = 2;
                for (auto& e : enemies) if (e.alive && len(e.pos - pl.pos) < 5.5f) damageEnemy(e, 1.0f + 0.2f * (lvl / 3), petPos, false);
                g_dmgKind = 0;
            } else petHowlT = 0.5f;
        }
    }
    if (k == 6) {  // Treasure Nose
        if (len(flatXZ(pl.vel)) > 2) petDigT += dt;
        if (petDigT > std::max(15.f, 40.f - 2.5f * lvl)) {
            petDigT = 0;
            burst(petPos + V3(0, 0.2f, 0), hexColor(0x8a6040), 20, 3, 0.1f, 0, 0.6f, 10);
            dropEmbers(petPos, 5 + lvl * 2);
            audio::playAt(SFX_BREAK, petPos, 0.5f, 1.6f);
        }
    }
}

// ------------------------------------------------------------------ the pet menu (pause > Pets, or Tamsin)
void Game::updatePetMenu(const InputState& in) {
    int rows = 8 + (petMenuShop ? 1 : 0) + 1;  // 8 kinds (+ the shop row) + Back
    if (in.mUp) { shopSel = (shopSel + rows - 1) % rows; audio::play(SFX_MENU_MOVE); }
    if (in.mDown) { shopSel = (shopSel + 1) % rows; audio::play(SFX_MENU_MOVE); }
    if (in.mLeft) { petAct = (petAct + 2) % 3; audio::play(SFX_MENU_MOVE); }
    if (in.mRight) { petAct = (petAct + 1) % 3; audio::play(SFX_MENU_MOVE); }
    if (mouseMoved) for (auto& r : menuRects) if (mouseX >= r.x && mouseX <= r.x + r.w && mouseY >= r.y && mouseY <= r.y + r.h) { if (r.idx >= 100) petAct = r.idx - 100; else shopSel = r.idx; }
    bool clicked = false;
    if (mouseClick) for (auto& r : menuRects) if (mouseX >= r.x && mouseX <= r.x + r.w && mouseY >= r.y && mouseY <= r.y + r.h) { if (r.idx >= 100) petAct = r.idx - 100; else shopSel = r.idx; clicked = true; }
    auto back = [&]() { if (petMenuShop) screen = SC_PLAY; else { screen = SC_PAUSE; menuSel = 6; } audio::play(SFX_MENU_BACK); };
    if (in.mBack || in.pressed[A_PAUSE]) { back(); return; }
    if (!(in.mOk || clicked)) return;
    if (shopSel == rows - 1) { back(); return; }
    if (petMenuShop && shopSel == 8) {
        if (save.embers >= 30) { save.embers -= 30; save.treats += 3; audio::play(SFX_SHARD, 0.8f, 1.4f); statusMsg = "+3 treats"; statusT = 1.2f; writeSave(); }
        else { audio::play(SFX_MENU_BACK); statusMsg = "Not enough embers (30)"; statusT = 1.2f; }
        return;
    }
    int k = shopSel;
    if (save.petLv[k] <= 0) { audio::play(SFX_MENU_BACK); return; }
    if (petAct == 0) { save.activePet = save.activePet == k ? -1 : k; petPos = pl.pos + V3(1, 0, -1); audio::play(SFX_UNLOCK, 0.8f, 1.3f); }
    else if (petAct == 1) {
        if (save.treats <= 0) { audio::play(SFX_MENU_BACK); statusMsg = "No treats! Tamsin sells them in Lumen Hollow."; statusT = 1.6f; }
        else if (save.petLv[k] >= 10) { audio::play(SFX_MENU_BACK); }
        else { save.treats--; int pa = save.activePet; petXp(k, 25); save.activePet = pa; audio::play(SFX_CATCH, 1, 1.5f); }
    } else { save.petName[k] = (save.petName[k] + 1 + prng.irange(0, NUM_NAMES - 2)) % NUM_NAMES; audio::play(SFX_BLIP, 1, 1.4f); }
    writeSave();
}

void Game::drawPets() {
    float s = (float)R->uiScale();
    float W = (float)R->winW, H = (float)R->winH;
    R->uiRect(0, 0, W, H, V4(0, 0, 0, 0.6f));
    float pw = std::min(W - 20 * s, 420 * s), px = W / 2 - pw / 2, py = H * 0.06f;
    int rows = 8 + (petMenuShop ? 1 : 0) + 1;
    float ph = 60 * s + rows * 21 * s;
    R->uiFrame(px, py, pw, ph, C4(0x0c0a14, 0.94f), COL_HI);
    R->uiTextCentered(W / 2, py + 5 * s, "PETS", s * 1.6f, COL_HI);
    char hdr[96]; snprintf(hdr, sizeof(hdr), "%d/8 kinds caught     treats: %d     embers: %d", save.petsCaught, save.treats, save.embers);
    R->uiTextCentered(W / 2, py + 20 * s, hdr, s, COL_DIM);
    menuRects.clear();
    float y = py + 34 * s;
    for (int i = 0; i < rows; i++) {
        bool sel = i == shopSel;
        if (sel) R->uiRect(px + 3 * s, y - 2 * s, pw - 6 * s, 20 * s, C4(0x3a2418, 0.8f));
        menuRects.push_back({px, y - 2 * s, pw * 0.62f, 20 * s, i});
        if (i == rows - 1) { R->uiText(px + 10 * s, y + 3 * s, "Back", s * 1.2f, sel ? COL_HI : COL_TEXT); break; }
        if (petMenuShop && i == 8) { R->uiText(px + 10 * s, y + 3 * s, "Buy 3 treats - 30 embers", s * 1.2f, sel ? COL_HI : COL_TEXT); y += 21 * s; continue; }
        int lv = save.petLv[i];
        V3 c = hexColor(PETS[i].col);
        R->uiRect(px + 8 * s, y + 1 * s, 10 * s, 10 * s, lv > 0 ? V4(c.x, c.y, c.z, 1) : C4(0x302838));
        if (lv <= 0) {
            R->uiText(px + 24 * s, y, "???", s * 1.2f, sel ? COL_HI : COL_DIM);
            R->uiText(px + 24 * s, y + 11 * s, std::string("lives in: ") + PETS[i].where, s, COL_DIM);
        } else {
            char b[96]; snprintf(b, sizeof(b), "%s the %s   Lv %d%s%s", NAMES[save.petName[i] % NUM_NAMES], PETS[i].name, lv, lv >= 5 ? (lv >= 10 ? "  MAX" : "  evolved") : "", save.activePet == i ? "   <- buddy" : "");
            R->uiText(px + 24 * s, y, b, s * 1.2f, sel ? COL_HI : (save.activePet == i ? C4(0x80ff90) : COL_TEXT));
            R->uiText(px + 24 * s, y + 11 * s, PETS[i].perk, s, COL_DIM);
            if (lv < 10) {
                float bw = 50 * s, f = clampf((float)save.petXp[i] / petNeed(lv), 0, 1);
                R->uiRect(px + pw - bw - 10 * s, y + 13 * s, bw, 3 * s, C4(0x302838));
                R->uiRect(px + pw - bw - 10 * s, y + 13 * s, bw * f, 3 * s, C4(0x80ff90));
            }
        }
        y += 21 * s;
    }
    // actions for the selected pet
    if (shopSel < 8 && save.petLv[shopSel] > 0) {
        float by = py + ph - 16 * s;
        for (int a = 0; a < 3; a++) {
            float ax = W / 2 - 75 * s + a * 50 * s;
            std::string t = a == 0 ? (save.activePet == shopSel ? "PUT AWAY" : "MAKE BUDDY") : (a == 1 ? "GIVE TREAT" : "RENAME");
            R->uiFrame(ax, by, 48 * s, 11 * s, a == petAct ? C4(0x5a3a20, 0.95f) : C4(0x1a1424, 0.9f), a == petAct ? COL_HI : C4(0x4a3a30));
            R->uiTextCentered(ax + 24 * s, by + 2 * s, t, s, a == petAct ? COL_HI : COL_TEXT);
            menuRects.push_back({ax, by, 48 * s, 11 * s, 100 + a});
        }
    }
    R->uiTextCentered(W / 2, py + ph + 6 * s, "Up/Down: pick a pet   Left/Right: BUDDY / TREAT (+25 XP) / RENAME   Enter: do it   Esc: back", s, COL_DIM);
    R->uiTextCentered(W / 2, py + ph + 16 * s, "Catch pets: hit one with your thrown flame (or a Bloom) to daze it, then press [" + bindingName(settings.keys[A_INTERACT]) + "] next to it.", s, COL_DIM);
}

// Tamsin, the pet keeper
bool talkPets(Game& g, int ti) {
    if (g.lv.talkers[ti].who != 23) return false;
    auto open = [&g]() { g.screen = SC_PETS; g.shopSel = 0; g.petAct = 0; g.petMenuShop = true; audio::play(SFX_CHECKPOINT); };
    if (!g.has("npc.23")) {
        g.setFlag("npc.23");
        g.save.treats += 3;
        g.startDialog({{"TAMSIN", "Oh! A walking candle. Careful near the hay. ...I'm Tamsin. I look after the little creatures of the Hollow."},
                       {"TAMSIN", "There are EIGHT kinds of pet hiding all over the world - mothlings, ember frogs, glow snails, puffcaps, even a gloom pup or two."},
                       {"TAMSIN", "They're shy. Toss your flame at one to DAZE it - it won't hurt, promise - then walk up and press [" + bindingName(g.settings.keys[A_INTERACT]) + "] to scoop it up."},
                       {"TAMSIN", "Your buddy follows you, helps you, and grows up as you burn the Hush. At level 5 they EVOLVE!"},
                       {"TAMSIN", "Here, three treats on the house. Each one's +25 XP for a pet. I sell more: three for 30 embers."}},
                      open);
    } else open();
    return true;
}

// ------------------------------------------------------------------ drawing
static M4 TRS(V3 t, float yaw, V3 s) { return M4::translate(t) * M4::rotY(yaw) * M4::scale(s); }
void drawPetModel(Renderer* R, int k, V3 pos, float yaw, float t, float hop, float scale, bool glow, bool gold, float alpha = 1) {
    M4 b = M4::translate(pos) * M4::rotY(yaw) * M4::scale(V3(scale, scale, scale));
    V3 c1 = hexColor(PETS[k].col), c2 = hexColor(PETS[k].col2);
    float h = std::fabs(std::sin(hop)) * 0.18f;
    float em = glow ? 0.5f : 0.f;
    auto eyes = [&](float y, float z, float sp, float sz) {
        for (int s = -1; s <= 1; s += 2) R->drawPart(PM_CUBE, b * M4::translate(V3(sp * s, y, z)) * M4::scale(V3(sz, sz * 1.3f, 0.03f)), T_WHITE, hexColor(0x101018), 0, 0, alpha);
    };
    switch (k) {
        case 0: {  // mothling: fluffy body, big soft wings
            float f = std::sin(t * 14) * 0.6f;
            R->drawPart(PM_SPHERE, b * M4::translate(V3(0, 0.75f + h, 0)) * M4::scale(V3(0.4f, 0.45f, 0.45f)), T_CLOTH, c1, em, 0, alpha);
            for (int s = -1; s <= 1; s += 2) R->drawPart(PM_CUBE, b * M4::translate(V3(0.1f * s, 0.8f + h, 0)) * M4::rotZ(f * s) * M4::translate(V3(0.3f * s, 0, 0)) * M4::scale(V3(0.55f, 0.04f, 0.45f)), T_WHITE, c2, 0.4f + em, 0, alpha * 0.85f);
            for (int s = -1; s <= 1; s += 2) R->drawPart(PM_CUBE, b * M4::translate(V3(0.08f * s, 1.05f + h, 0.12f)) * M4::rotZ(0.4f * s) * M4::scale(V3(0.03f, 0.22f, 0.03f)), T_WHITE, c1 * 0.7f, 0, 0, alpha);
            eyes(0.8f + h, 0.21f, 0.09f, 0.07f);
        } break;
        case 1: {  // ember frog
            R->drawPart(PM_SPHERE, b * M4::translate(V3(0, 0.25f + h, 0)) * M4::scale(V3(0.6f, 0.4f, 0.65f)), T_WHITE, c1, 0.3f + em, 0, alpha);
            for (int s = -1; s <= 1; s += 2) {
                R->drawPart(PM_SPHERE, b * M4::translate(V3(0.16f * s, 0.46f + h, 0.15f)) * M4::scale(V3(0.18f, 0.18f, 0.18f)), T_WHITE, c2, 0.5f, 0, alpha);
                R->drawPart(PM_CUBE, b * M4::translate(V3(0.16f * s, 0.48f + h, 0.24f)) * M4::scale(V3(0.06f, 0.08f, 0.03f)), T_WHITE, hexColor(0x101010), 0, 0, alpha);
                R->drawPart(PM_CUBE, b * M4::translate(V3(0.26f * s, 0.08f, -0.1f)) * M4::scale(V3(0.18f, 0.12f, 0.3f)), T_WHITE, c1 * 0.8f, 0, 0, alpha);
            }
            R->drawPart(PM_OCTA, b * M4::translate(V3(0, 0.55f + h, -0.05f)) * M4::rotY(t * 3) * M4::scale(V3(0.12f, 0.22f + 0.04f * std::sin(t * 12), 0.12f)), T_WHITE, hexColor(0xffd050), 2.2f, 0, alpha);
        } break;
        case 2: {  // glow snail
            R->drawPart(PM_CUBE, b * M4::translate(V3(0, 0.1f, 0.05f)) * M4::scale(V3(0.3f, 0.2f, 0.8f)), T_WAX, c2 * 0.9f, 0.3f, 0, alpha);
            R->drawPart(PM_SPHERE, b * M4::translate(V3(0, 0.38f, -0.1f)) * M4::rotX(t) * M4::scale(V3(0.32f, 0.5f, 0.5f)), T_WHITE, c1, 0.9f + em, 0, alpha);
            for (int s = -1; s <= 1; s += 2) R->drawPart(PM_SPHERE, b * M4::translate(V3(0.08f * s, 0.42f + 0.03f * std::sin(t * 3 + s), 0.42f)) * M4::scale(V3(0.06f, 0.06f, 0.06f)), T_WHITE, hexColor(0x101010), 0, 0, alpha);
        } break;
        case 3: {  // puffcap: a mushroom on little feet
            float sq = 1 + std::sin(hop) * 0.08f;
            R->drawPart(PM_CYL, b * M4::translate(V3(0, 0.25f + h, 0)) * M4::scale(V3(0.32f, 0.4f * sq, 0.32f)), T_WHITE, c2, em, 0, alpha);
            R->drawPart(PM_SPHERE, b * M4::translate(V3(0, 0.55f + h, 0)) * M4::scale(V3(0.75f, 0.42f, 0.75f)), T_WHITE, c1, 0.2f + em, 0, alpha);
            for (int i = 0; i < 5; i++) { float a = i * TAU / 5; R->drawPart(PM_SPHERE, b * M4::translate(V3(std::sin(a) * 0.22f, 0.7f + h, std::cos(a) * 0.22f)) * M4::scale(V3(0.1f, 0.06f, 0.1f)), T_WHITE, V3(1, 1, 1), 0.3f, 0, alpha); }
            eyes(0.3f + h, 0.16f, 0.07f, 0.05f);
        } break;
        case 4: {  // gloom pup
            R->drawPart(PM_SPHERE, b * M4::translate(V3(0, 0.35f + h, -0.05f)) * M4::scale(V3(0.45f, 0.4f, 0.65f)), T_SHADOW, c1, 0.1f, 0, alpha);
            R->drawPart(PM_SPHERE, b * M4::translate(V3(0, 0.6f + h, 0.3f)) * M4::scale(V3(0.38f, 0.36f, 0.36f)), T_SHADOW, c1, 0.1f, 0, alpha);
            for (int s = -1; s <= 1; s += 2) {
                R->drawPart(PM_CONE, b * M4::translate(V3(0.12f * s, 0.82f + h, 0.25f)) * M4::scale(V3(0.12f, 0.2f, 0.08f)), T_SHADOW, c1, 0, 0, alpha);
                R->drawPart(PM_CUBE, b * M4::translate(V3(0.08f * s, 0.63f + h, 0.47f)) * M4::scale(V3(0.06f, 0.05f, 0.03f)), T_WHITE, c2, 2.0f, 0, alpha);
                for (int f = -1; f <= 1; f += 2) R->drawPart(PM_CUBE, b * M4::translate(V3(0.14f * s, 0.1f, 0.18f * f)) * M4::rotX(std::sin(hop) * 0.6f * s * f) * M4::scale(V3(0.09f, 0.22f, 0.09f)), T_SHADOW, c1, 0, 0, alpha);
            }
            R->drawPart(PM_CUBE, b * M4::translate(V3(0, 0.5f + h, -0.42f)) * M4::rotX(-0.8f + std::sin(t * 12) * 0.3f) * M4::scale(V3(0.06f, 0.3f, 0.06f)), T_SHADOW, c1, 0, 0, alpha);
        } break;
        case 5: {  // byte bug: a glitchy beetle
            R->drawPart(PM_SPHERE, b * M4::translate(V3(0, 0.22f + h * 0.5f, 0)) * M4::scale(V3(0.45f, 0.3f, 0.6f)), T_CODE, V3(1, 1, 1), 1.0f, 0, alpha);
            R->drawPart(PM_SPHERE, b * M4::translate(V3(0, 0.24f + h * 0.5f, 0.32f)) * M4::scale(V3(0.26f, 0.2f, 0.22f)), T_WHITE, c2, 0, 0, alpha);
            for (int s = -1; s <= 1; s += 2) {
                R->drawPart(PM_CUBE, b * M4::translate(V3(0.07f * s, 0.27f + h * 0.5f, 0.43f)) * M4::scale(V3(0.05f, 0.05f, 0.02f)), T_WHITE, c1, 2.0f, 0, alpha);
                for (int f = -1; f <= 1; f++) R->drawPart(PM_CUBE, b * M4::translate(V3(0.26f * s, 0.1f, f * 0.15f)) * M4::rotZ(0.6f * s + std::sin(hop + f) * 0.3f) * M4::scale(V3(0.18f, 0.03f, 0.03f)), T_WHITE, c2, 0, 0, alpha);
            }
        } break;
        case 6: {  // lantern fox: a fox with a glowing tail-tip
            R->drawPart(PM_SPHERE, b * M4::translate(V3(0, 0.35f + h, -0.05f)) * M4::scale(V3(0.38f, 0.36f, 0.62f)), T_CLOTH, c1, em, 0, alpha);
            R->drawPart(PM_SPHERE, b * M4::translate(V3(0, 0.58f + h, 0.3f)) * M4::scale(V3(0.34f, 0.3f, 0.32f)), T_CLOTH, c1, em, 0, alpha);
            R->drawPart(PM_CONE, b * M4::translate(V3(0, 0.55f + h, 0.5f)) * M4::rotX(PI / 2) * M4::scale(V3(0.14f, 0.18f, 0.12f)), T_CLOTH, c2, 0, 0, alpha);
            for (int s = -1; s <= 1; s += 2) {
                R->drawPart(PM_CONE, b * M4::translate(V3(0.11f * s, 0.8f + h, 0.27f)) * M4::scale(V3(0.12f, 0.2f, 0.07f)), T_CLOTH, c1, 0, 0, alpha);
                R->drawPart(PM_CUBE, b * M4::translate(V3(0.08f * s, 0.62f + h, 0.45f)) * M4::scale(V3(0.05f, 0.05f, 0.02f)), T_WHITE, hexColor(0x101010), 0, 0, alpha);
                for (int f = -1; f <= 1; f += 2) R->drawPart(PM_CUBE, b * M4::translate(V3(0.12f * s, 0.1f, 0.18f * f)) * M4::rotX(std::sin(hop) * 0.6f * s * f) * M4::scale(V3(0.08f, 0.22f, 0.08f)), T_CLOTH, c1 * 0.6f, 0, 0, alpha);
            }
            M4 tail = b * M4::translate(V3(0, 0.5f + h, -0.4f)) * M4::rotX(-0.9f + std::sin(t * 4) * 0.2f);
            R->drawPart(PM_SPHERE, tail * M4::translate(V3(0, 0.25f, 0)) * M4::scale(V3(0.2f, 0.5f, 0.2f)), T_CLOTH, c1, 0, 0, alpha);
            R->drawPart(PM_OCTA, tail * M4::translate(V3(0, 0.55f, 0)) * M4::rotY(t * 3) * M4::scale(V3(0.16f, 0.26f, 0.16f)), T_WHITE, hexColor(0xffd060), 2.5f, 0, alpha);
        } break;
        case 7: {  // starling: a little star-bird
            float f = std::sin(t * 16) * 0.7f;
            R->drawPart(PM_SPHERE, b * M4::translate(V3(0, 0.8f + h, 0)) * M4::scale(V3(0.36f, 0.34f, 0.42f)), T_GOLD, V3(1, 1, 1), 0.8f, 0, alpha);
            for (int s = -1; s <= 1; s += 2) R->drawPart(PM_CUBE, b * M4::translate(V3(0.12f * s, 0.85f + h, 0)) * M4::rotZ(f * s) * M4::translate(V3(0.22f * s, 0, 0)) * M4::scale(V3(0.4f, 0.04f, 0.22f)), T_WHITE, c2, 1.5f, 0, alpha);
            R->drawPart(PM_CONE, b * M4::translate(V3(0, 0.8f + h, 0.25f)) * M4::rotX(PI / 2) * M4::scale(V3(0.08f, 0.14f, 0.08f)), T_WHITE, hexColor(0xff9020), 0.5f, 0, alpha);
            R->drawPart(PM_OCTA, b * M4::translate(V3(0, 1.15f + h, 0)) * M4::rotY(t * 2) * M4::scale(V3(0.12f, 0.12f, 0.04f)), T_WHITE, hexColor(0xfff0a0), 2.5f, 0, alpha);
            eyes(0.86f + h, 0.18f, 0.08f, 0.05f);
        } break;
    }
    if (gold && !R->shadowPass) {  // level 10: a golden halo
        R->drawPart(PM_CYL, b * M4::translate(V3(0, 1.3f, 0)) * M4::rotY(t) * M4::scale(V3(0.45f, 0.04f, 0.45f)), T_GOLD, V3(1, 1, 1), 1.5f, 0, alpha);
    }
}

void drawPetsWorld(Game& g, Renderer* R) {
    float t = g.time;
    // Tamsin the pet keeper: overalls, a straw hat, and a couple of residents snoozing in the pen
    for (auto& tk : g.lv.talkers) if (tk.who == 23) {
        float yaw = tk.yaw;
        if (g.screen == SC_PLAY && len(g.pl.pos - tk.pos) < 8) yaw = std::atan2(g.pl.pos.x - tk.pos.x, g.pl.pos.z - tk.pos.z);
        M4 b = M4::translate(tk.pos) * M4::rotY(yaw);
        float bob = std::sin(t * 2) * 0.03f;
        R->drawPart(PM_CONE, b * M4::translate(V3(0, 0.7f, 0)) * M4::scale(V3(0.85f, 1.4f, 0.75f)), T_CLOTH, hexColor(0x4a7a40));
        R->drawPart(PM_CYL, b * M4::translate(V3(0, 1.2f + bob, 0)) * M4::scale(V3(0.55f, 0.6f, 0.45f)), T_CLOTH, hexColor(0xe0c890));
        R->drawPart(PM_CUBE, b * M4::translate(V3(0, 1.1f + bob, 0.2f)) * M4::scale(V3(0.4f, 0.4f, 0.06f)), T_CLOTH, hexColor(0x4a7a40));
        R->drawPart(PM_SPHERE, b * M4::translate(V3(0, 1.72f + bob, 0)) * M4::scale(V3(0.44f, 0.48f, 0.44f)), T_WHITE, hexColor(0xe0a880));
        for (int s = -1; s <= 1; s += 2) R->drawPart(PM_CUBE, b * M4::translate(V3(0.1f * s, 1.76f + bob, 0.21f)) * M4::scale(V3(0.07f, 0.05f, 0.03f)), T_WHITE, hexColor(0x101010));
        R->drawPart(PM_CYL, b * M4::translate(V3(0, 1.98f + bob, 0)) * M4::scale(V3(0.9f, 0.05f, 0.9f)), T_SAND, hexColor(0xe8d070));
        R->drawPart(PM_CYL, b * M4::translate(V3(0, 2.1f + bob, 0)) * M4::scale(V3(0.45f, 0.22f, 0.45f)), T_SAND, hexColor(0xe8d070));
        R->drawPart(PM_CYL, b * M4::translate(V3(0, 2.04f + bob, 0)) * M4::scale(V3(0.47f, 0.06f, 0.47f)), T_CLOTH, hexColor(0xd04040));
        drawPetModel(R, 0, tk.pos + V3(-2.2f, 0, 1.0f), t * 0.3f, t, t * 2, 0.9f, false, false);
        drawPetModel(R, 3, tk.pos + V3(-1.0f, 0, 1.6f), 2.5f, t, t * 3, 0.9f, false, false);
    }
    for (auto& w : g.wildPets) {
        if (!w.alive) continue;
        drawPetModel(R, w.kind, w.pos, w.daze > 0 ? w.yaw + std::sin(t * 6) * 0.3f : w.yaw, t, w.daze > 0 ? 0 : w.hop, 1.0f, w.kind == 7, false);
        if (R->shadowPass) continue;
        if (w.daze > 0) for (int s = 0; s < 3; s++) { float a = t * 5 + s * TAU / 3; R->drawPart(PM_OCTA, M4::translate(w.pos + V3(std::sin(a) * 0.45f, 1.25f, std::cos(a) * 0.45f)) * M4::scale(V3(0.1f, 0.1f, 0.1f)), T_WHITE, hexColor(0xffe060), 2.5f); }
        else if (w.kind == 7 && g.fxTick(0.15f)) g.burst(w.pos + V3(0, 0.9f, 0), hexColor(0xffe080), 1, 1, 0.06f, 3, 0.6f, -1);
    }
    int k = g.save.activePet;
    if (k >= 0 && g.save.petLv[k] > 0 && g.lv.id != LV_HOME && !(g.screen == SC_TITLE)) {
        int lv = g.save.petLv[k];
        float sc = lv >= 10 ? 1.45f : (lv >= 5 ? 1.3f : 1.0f);
        if (g.petFlash > 0) sc *= 1 + 0.3f * g.petFlash;
        drawPetModel(R, k, g.petPos, g.petYaw, t, g.petHop, sc, lv >= 5, lv >= 10);
    }
}

void petLights(Game& g, Renderer* R) {
    int k = g.save.activePet;
    if (k >= 0 && g.save.petLv[k] >= 5) R->lights.push_back({g.petPos + V3(0, 0.8f, 0), 4, hexColor(PETS[k].col2), 0.8f});
    for (auto& w : g.wildPets) if (w.alive && w.kind == 7) R->lights.push_back({w.pos + V3(0, 1, 0), 5, hexColor(0xffe080), 1.0f});
}

void drawPetsHUD(Game& g) {
    Renderer* R = g.R;
    float s = (float)R->uiScale();
    for (auto& w : g.wildPets) {
        if (!w.alive || distXZ(w.pos, g.pl.pos) > 14) continue;
        float sx, sy;
        if (!R->project(w.pos + V3(0, 1.7f, 0), sx, sy)) continue;
        bool owned = g.save.petLv[w.kind] > 0;
        std::string t = w.daze > 0 ? (distXZ(w.pos, g.pl.pos) < 2.4f ? "[" + bindingName(g.settings.keys[A_INTERACT]) + "] CATCH!" : std::string("dazed! get close")) : std::string(owned ? "" : "NEW! ") + "wild " + PETS[w.kind].name;
        V4 col = w.daze > 0 ? C4(0xffe060) : (owned ? COL_DIM : C4(0x80ff90));
        float bw = R->uiTextWidth(t, s) + 8 * s;
        R->uiFrame(sx - bw / 2, sy - 6 * s, bw, 12 * s, V4(0.05f, 0.04f, 0.08f, 0.7f), col);
        R->uiTextCentered(sx, sy - 3 * s, t, s, col);
    }
}
