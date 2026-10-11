// WICKLIGHT - menus, settings, journal, HUD and dialog rendering
#include "game.h"
#include <cstdio>
#include <cstring>
#include <algorithm>

static V4 C4(uint32_t h, float a = 1) { V3 c = hexColor(h); return V4(c.x, c.y, c.z, a); }
static const V4 COL_TEXT = C4(0xf0e6d0), COL_DIM = C4(0x8a8098), COL_HI = C4(0xffc060), COL_PANEL = C4(0x0c0a14, 0.86f), COL_BORDER = C4(0x6a5040);

static std::string fmtTime(float s) {
    int t = (int)s;
    char buf[32];
    snprintf(buf, sizeof(buf), "%d:%02d:%02d", t / 3600, (t / 60) % 60, t % 60);
    return buf;
}

static std::string expandKeys(const Game& g, std::string s) {
    static const struct { const char* tok; int a; } T[] = {{"{THROW}", A_THROW}, {"{WARP}", A_WARP}, {"{BLOOM}", A_BLOOM}, {"{JUMP}", A_JUMP}, {"{DASH}", A_DASH}, {"{INTERACT}", A_INTERACT}, {"{JOURNAL}", A_JOURNAL}, {"{GRAPPLE}", A_GRAPPLE}, {"{FIREWALL}", A_FIREWALL}, {"{OVERCLOCK}", A_OVERCLOCK}};
    for (auto& t : T) {
        size_t p;
        while ((p = s.find(t.tok)) != std::string::npos) s.replace(p, strlen(t.tok), bindingName(g.settings.keys[t.a]));
    }
    return s;
}

// word-wrap to a pixel width
static std::string wrap(Renderer* R, const std::string& s, float sc, float width) {
    std::string out, line, word;
    auto flushWord = [&]() {
        if (word.empty()) return;
        std::string test = line.empty() ? word : line + " " + word;
        if (R->uiTextWidth(test, sc) > width && !line.empty()) { out += line + "\n"; line = word; }
        else line = test;
        word.clear();
    };
    for (char c : s) {
        if (c == ' ') flushWord();
        else if (c == '\n') { flushWord(); out += line + "\n"; line.clear(); }
        else word += c;
    }
    flushWord();
    out += line;
    return out;
}

// ------------------------------------------------------------------ menu item model
static std::vector<std::string> menuItems(Game& g) {
    std::vector<std::string> v;
    auto onoff = [](bool b) { return std::string(b ? "On" : "Off"); };
    auto pct = [](float f) { return std::to_string((int)std::lround(f * 100)) + "%"; };
    switch (g.screen) {
        case SC_TITLE:
            if (g.hasSave()) v.push_back("Continue");
            v.push_back("New Game"); v.push_back("How to play"); v.push_back("Settings"); v.push_back("Credits"); v.push_back("Quit");
            break;
        case SC_CONFIRM_NEW: v = {"Yes, start over", "No, go back"}; break;
        case SC_PAUSE: v = {"Resume", "Journal", "Settings", "Return to last lantern", "How to play", "Quit to title"}; break;
        case SC_SETTINGS: {
            char sens[16]; snprintf(sens, sizeof(sens), "%.1f", g.settings.sensitivity);
            static const char* sh[] = {"Off", "Low", "High"};
            v = {"Master volume: " + pct(g.settings.master), "Music volume: " + pct(g.settings.music), "Effects volume: " + pct(g.settings.sfx),
                 std::string("Mouse sensitivity: ") + sens, "Invert mouse Y: " + onoff(g.settings.invertY),
                 "Pixel resolution: " + std::to_string(PIXEL_LINES[g.settings.pixelMode]) + " lines", std::string("Shadows: ") + sh[g.settings.shadows],
                 "Outlines: " + onoff(g.settings.outlines), "Field of view: " + std::to_string((int)g.settings.fov), "Screen shake: " + onoff(g.settings.shake),
                 "Aim assist: " + onoff(g.settings.aimAssist), "UI size: " + (g.settings.uiScale == 0 ? std::string("Auto") : std::to_string(g.settings.uiScale) + "x"), "Fullscreen: " + onoff(g.settings.fullscreen), "VSync: " + onoff(g.settings.vsync),
                 "Controls...", "Back"};
        } break;
        case SC_CONTROLS:
            for (int a = 0; a < A_COUNT; a++) v.push_back(actionName(a) + ": " + (g.rebinding && g.rebindIdx == a ? std::string("press a key...") : bindingName(g.settings.keys[a])));
            v.push_back("Reset to defaults"); v.push_back("Back");
            break;
        case SC_VICTORY: v = {"Continue exploring", "Return to title"}; break;
        default: break;
    }
    return v;
}

void Game::finishRebind(int code) {
    if (code >= 0) {
        // swap if another action already uses this binding
        for (int a = 0; a < A_COUNT; a++) if (a != rebindIdx && settings.keys[a] == code) settings.keys[a] = settings.keys[rebindIdx];
        settings.keys[rebindIdx] = code;
        writeSettings();
        audio::play(SFX_MENU_SELECT);
    } else audio::play(SFX_MENU_BACK);
    rebinding = false;
}

static void adjustSetting(Game& g, int idx, int dir) {
    Settings& s = g.settings;
    switch (idx) {
        case 0: s.master = clampf(std::round((s.master + dir * 0.1f) * 10) / 10, 0, 1); break;
        case 1: s.music = clampf(std::round((s.music + dir * 0.1f) * 10) / 10, 0, 1); break;
        case 2: s.sfx = clampf(std::round((s.sfx + dir * 0.1f) * 10) / 10, 0, 1); break;
        case 3: s.sensitivity = clampf(std::round((s.sensitivity + dir * 0.1f) * 10) / 10, 0.2f, 3.0f); break;
        case 4: s.invertY = !s.invertY; break;
        case 5: s.pixelMode = (s.pixelMode + dir + PIXEL_MODES) % PIXEL_MODES; break;
        case 6: s.shadows = (s.shadows + dir + 3) % 3; break;
        case 7: s.outlines = !s.outlines; break;
        case 8: s.fov = clampf(s.fov + dir * 5, 55, 100); break;
        case 9: s.shake = !s.shake; break;
        case 10: s.aimAssist = !s.aimAssist; break;
        case 11: s.uiScale = (s.uiScale + dir + 6) % 6; break;
        case 12: s.fullscreen = !s.fullscreen; g.displayDirty = true; break;
        case 13: s.vsync = !s.vsync; g.displayDirty = true; break;
        default: return;
    }
    g.applySettings();
    g.writeSettings();
    audio::play(SFX_MENU_MOVE);
}

void Game::updateMenus(float dt, const InputState& in) {
    (void)dt;
    if (rebinding) return;  // main loop delivers the next key via finishRebind
    if (screen == SC_JOURNAL) {
        int found = countFlags("candle.");
        if (in.mUp) { journalSel = std::max(0, journalSel - 1); audio::play(SFX_MENU_MOVE); }
        if (in.mDown) { journalSel = std::min(std::max(0, found - 1), journalSel + 1); audio::play(SFX_MENU_MOVE); }
        if (in.mBack || in.pressed[A_JOURNAL] || in.mOk || mouseClick) { screen = prevJournalScreen; audio::play(SFX_MENU_BACK); }
        return;
    }
    if (screen == SC_SHOP) { updateShop(in); return; }
    if (screen == SC_BOTSHOP) { updateBotShop(in); return; }
    if (screen == SC_CRAFT) { updateCraft(in); return; }
    if (screen == SC_SUPPLY) { updateSupply(in); return; }
    if (screen == SC_CHEST) { updateChest(in); return; }
    if (screen == SC_HOWTO) {
        if (in.mBack || in.mOk || mouseClick || in.pressed[A_PAUSE]) { screen = prevJournalScreen; audio::play(SFX_MENU_BACK); }
        return;
    }
    if (screen == SC_CREDITS) {
        if (in.mBack || in.mOk || mouseClick) { screen = SC_TITLE; menuSel = 0; audio::play(SFX_MENU_BACK); }
        return;
    }
    auto items = menuItems(*this);
    int n = (int)items.size();
    if (n == 0) return;
    // mouse hover
    if (mouseMoved) {
        for (auto& r : menuRects)
            if (mouseX >= r.x && mouseX <= r.x + r.w && mouseY >= r.y && mouseY <= r.y + r.h && r.idx != menuSel) { menuSel = r.idx; audio::play(SFX_MENU_MOVE, 0.6f); }
    }
    bool clicked = false;
    if (mouseClick) for (auto& r : menuRects) if (mouseX >= r.x && mouseX <= r.x + r.w && mouseY >= r.y && mouseY <= r.y + r.h) { menuSel = r.idx; clicked = true; }
    if (in.mUp) { menuSel = (menuSel + n - 1) % n; audio::play(SFX_MENU_MOVE); }
    if (in.mDown) { menuSel = (menuSel + 1) % n; audio::play(SFX_MENU_MOVE); }
    menuSel = std::max(0, std::min(n - 1, menuSel));
    std::string item = items[menuSel];
    bool ok = in.mOk || clicked;

    switch (screen) {
        case SC_TITLE:
            if (ok) {
                audio::play(SFX_MENU_SELECT);
                if (item == "Continue") continueGame();
                else if (item == "New Game") { if (hasSave()) { screen = SC_CONFIRM_NEW; menuSel = 1; } else newGame(); }
                else if (item == "Settings") { prevScreen = SC_TITLE; screen = SC_SETTINGS; menuSel = 0; }
                else if (item == "Credits") { screen = SC_CREDITS; }
                else if (item == "How to play") { prevJournalScreen = SC_TITLE; screen = SC_HOWTO; }
                else if (item == "Quit") quit = true;
            }
            break;
        case SC_CONFIRM_NEW:
            if (ok) { if (menuSel == 0) newGame(); else { screen = SC_TITLE; menuSel = 0; } audio::play(SFX_MENU_SELECT); }
            if (in.mBack) { screen = SC_TITLE; menuSel = 0; audio::play(SFX_MENU_BACK); }
            break;
        case SC_PAUSE:
            if (in.mBack || in.pressed[A_PAUSE]) { screen = SC_PLAY; audio::play(SFX_MENU_BACK); break; }
            if (ok) {
                audio::play(SFX_MENU_SELECT);
                if (menuSel == 0) screen = SC_PLAY;
                else if (menuSel == 1) { prevJournalScreen = SC_PAUSE; screen = SC_JOURNAL; journalSel = 0; }
                else if (menuSel == 2) { prevScreen = SC_PAUSE; screen = SC_SETTINGS; menuSel = 0; }
                else if (menuSel == 3) { screen = SC_PLAY; respawnAtLantern(); }
                else if (menuSel == 4) { prevJournalScreen = SC_PAUSE; screen = SC_HOWTO; }
                else if (menuSel == 5) {
                    writeSave();
                    screen = SC_TITLE; menuSel = 0;
                    buildLevel(LV_HUB); applyFlagsToLevel(); enemies.clear(); boss = Boss();
                    audio::setSong(SONG_TITLE);
                }
            }
            break;
        case SC_SETTINGS:
            if (in.mBack) { screen = prevScreen; menuSel = prevScreen == SC_PAUSE ? 2 : 0; audio::play(SFX_MENU_BACK); break; }
            if (in.mLeft) adjustSetting(*this, menuSel, -1);
            if (in.mRight) adjustSetting(*this, menuSel, 1);
            if (ok) {
                if (menuSel == 14) { screen = SC_CONTROLS; menuSel = 0; audio::play(SFX_MENU_SELECT); }
                else if (menuSel == 15) { screen = prevScreen; menuSel = prevScreen == SC_PAUSE ? 2 : 0; audio::play(SFX_MENU_BACK); }
                else adjustSetting(*this, menuSel, 1);
            }
            break;
        case SC_CONTROLS:
            if (in.mBack) { screen = SC_SETTINGS; menuSel = 14; audio::play(SFX_MENU_BACK); break; }
            if (ok) {
                if (menuSel < A_COUNT) { rebinding = true; rebindIdx = menuSel; audio::play(SFX_MENU_SELECT); }
                else if (menuSel == A_COUNT) { Settings d; for (int a = 0; a < A_COUNT; a++) settings.keys[a] = d.keys[a]; writeSettings(); audio::play(SFX_MENU_SELECT); }
                else { screen = SC_SETTINGS; menuSel = 14; audio::play(SFX_MENU_BACK); }
            }
            break;
        case SC_VICTORY:
            if (ok && screenT > 1.0f) {
                audio::play(SFX_MENU_SELECT);
                if (menuSel == 0) { enterLevel(LV_HUB, 0); screen = SC_PLAY; pl.hp = maxHp(); }
                else { screen = SC_TITLE; menuSel = 0; buildLevel(LV_HUB); applyFlagsToLevel(); enemies.clear(); boss = Boss(); audio::setSong(SONG_TITLE); }
            }
            break;
        default: break;
    }
}

// ------------------------------------------------------------------ drawing helpers
void Game::drawMenu(const char* title, const std::vector<std::string>& items, int sel, float y0) {
    float s = (float)R->uiScale();
    float cx = R->winW / 2.f;
    if (title && *title) R->uiTextCentered(cx, y0, title, s * 2, COL_HI);
    float y = y0 + (title && *title ? 30 * s : 0);
    menuRects.clear();
    float lineH = 13 * s;
    // scroll long lists
    int maxVisible = std::max(4, (int)((R->winH - y - 20 * s) / lineH));
    int first = 0;
    if ((int)items.size() > maxVisible) first = std::max(0, std::min((int)items.size() - maxVisible, sel - maxVisible / 2));
    for (int i = first; i < (int)items.size() && i < first + maxVisible; i++) {
        bool on = i == sel;
        std::string txt = on ? "> " + items[i] + " <" : items[i];
        float w = R->uiTextWidth(txt, s * 1.5f);
        if (on) R->uiRect(cx - w / 2 - 6 * s, y - 3 * s, w + 12 * s, 13.5f * s, C4(0x3a2418, 0.7f));
        R->uiTextCentered(cx, y, txt, s * 1.5f, on ? COL_HI : COL_TEXT);
        menuRects.push_back({cx - w / 2 - 6 * s, y - 3 * s, w + 12 * s, 13.5f * s, i});
        y += lineH;
    }
}

static void drawCandleIcon(Renderer* R, float x, float y, float s, bool lit, float t, float partial = 1) {
    // wax body
    R->uiRect(x, y + 4 * s, 5 * s, 8 * s, lit ? C4(0xeadfc4) : C4(0x5a5048));
    R->uiRect(x + 3 * s, y + 4 * s, 2 * s, 8 * s, lit ? C4(0xc8bc9e) : C4(0x4a4038));
    R->uiRect(x + 2 * s, y + 2 * s, 1 * s, 2 * s, C4(0x2a2018));
    if (lit && partial > 0) {
        float f = std::sin(t * 12 + x) > 0 ? 1.f : 0.f;
        R->uiRect(x + 1 * s, y - 2 * s - f * s, 3 * s, 4 * s + f * s, C4(0xff8a2a));
        R->uiRect(x + 2 * s, y - 1 * s, 1 * s, 3 * s, C4(0xfff0a0));
    }
}

void Game::drawHUD() {
    float s = (float)R->uiScale();
    float W = (float)R->winW, H = (float)R->winH;
    float t = time;
    // health: one candle per point of flame
    int maxh = (int)maxHp();
    for (int i = 0; i < maxh; i++) drawCandleIcon(R, 8 * s + i * 8 * s, 8 * s, s, i < (int)std::ceil(pl.hp - 0.01f), t);
    // collectibles
    char buf[64];
    snprintf(buf, sizeof(buf), "Shards %d/7  Candles %d/10", countFlags("shard."), countFlags("candle."));
    R->uiText(8 * s, 24 * s, buf, s, COL_DIM);
    // embers
    {
        float ex = 8 * s + std::max(maxh, 5) * 8 * s + 8 * s;
        R->uiRect(ex, 9 * s, 4 * s, 6 * s, C4(0xff8a2a));
        R->uiRect(ex + s, 7 * s, 2 * s, 3 * s, C4(0xfff0a0));
        R->uiText(ex + 7 * s, 9 * s, std::to_string(save.embers), s * 1.4f, C4(0xffc070));
    }
    // combo meter
    if (combo >= 2) {
        float a = std::min(1.f, comboT);
        std::string c = std::to_string(combo) + " HITS";
        if (comboMult() > 1.01f) { char m[16]; snprintf(m, sizeof(m), "  x%.1f embers", comboMult()); c += m; }
        R->uiText(W / 2 + 18 * s, H / 2 - 4 * s, c, s, V4(1, 0.85f, 0.4f, a));
        R->uiRect(W / 2 + 18 * s, H / 2 + 5 * s, 40 * s * clampf(comboT / 3.f, 0, 1), s, V4(1, 0.6f, 0.2f, a));
    }
    // floating numbers
    for (auto& f : floats) {
        float sx, sy;
        if (!R->project(f.pos, sx, sy)) continue;
        float a = 1 - f.t / 1.1f;
        R->uiTextCentered(sx, sy, f.text, s, V4(f.col.x, f.col.y, f.col.z, a));
    }
    // lantern upgrade prompt
    if (dialog.empty() && !boss.active)
        for (auto& l : lv.lanterns)
            if (l.lit && distXZ(l.pos, pl.pos) < 3.2f) {
                float sx, sy;
                if (R->project(l.pos + V3(0, 1.2f, 0), sx, sy)) {
                    std::string p = "[" + bindingName(settings.keys[A_INTERACT]) + "] Tend your flame (upgrades)";
                    float w = R->uiTextWidth(p, s) + 8 * s;
                    R->uiFrame(sx - w / 2, sy - 6 * s, w, 12 * s, COL_PANEL, COL_HI);
                    R->uiTextCentered(sx, sy - 3 * s, p, s, COL_HI);
                }
            }
    // abilities
    float ay = H - 22 * s;
    float ax = 8 * s;
    auto abil = [&](const char* name, int key, bool unlocked, float cd, float cdMax, bool active) {
        if (!unlocked || lv.id == LV_HOME) return;
        std::string k = "[" + bindingName(settings.keys[key]) + "]";
        std::string label = std::string(name) + " " + k;
        float w = R->uiTextWidth(label, s) + 8 * s;
        R->uiFrame(ax, ay, w, 14 * s, active ? C4(0x3a2410, 0.85f) : C4(0x0c0a14, 0.7f), active ? COL_HI : COL_BORDER);
        if (cd > 0) R->uiRect(ax + s, ay + s, (w - 2 * s) * (cd / cdMax), 12 * s, C4(0x000000, 0.6f));
        R->uiText(ax + 4 * s, ay + 4 * s, label, s, cd > 0 ? COL_DIM : (active ? COL_HI : COL_TEXT));
        ax += w + 4 * s;
    };
    bool thrown = flame.st != F_HELD;
    abil(thrown ? "Recall" : (ability("sunflare") ? "Throw/Hold" : "Throw"), A_THROW, true, 0, 1, thrown);
    abil("Warp", A_WARP, ability("warp"), flame.warpCd, 0.3f, thrown);
    abil("Bloom", A_BLOOM, ability("bloom"), flame.bloomCd, 2.5f, false);
    abil("Dash", A_DASH, true, pl.dashCd, 0.55f, false);
    abil("Grapple", A_GRAPPLE, ability("grapple"), 0, 1, grappling);
    abil("Firewall", A_FIREWALL, ability("firewall"), fwCd, 8.5f, !firewalls.empty());
    abil(ocT > 0 ? "OVERCLOCK!" : (ocCharge >= 1 ? "Overclock READY" : "Overclock"), A_OVERCLOCK, ability("overclock"), ocT > 0 ? 0.f : 1 - ocCharge, 1, ocT > 0 || ocCharge >= 1);
    // crosshair
    if (dialog.empty() && !pl.dead) {
        float cx = W / 2, cy = H / 2;
        V4 col = aimTarget == 1 ? C4(0xff6050) : aimTarget == 2 ? C4(0xffe060) : aimTarget == 3 ? C4(0xffa040) : aimTarget == 4 ? C4(0xffd070) : C4(0xf0e6d0, 0.8f);
        float g = aimTarget >= 0 ? 4 * s : 3 * s;
        R->uiRect(cx - g - 3 * s, cy - s / 2, 3 * s, s, col);
        R->uiRect(cx + g, cy - s / 2, 3 * s, s, col);
        R->uiRect(cx - s / 2, cy - g - 3 * s, s, 3 * s, col);
        R->uiRect(cx - s / 2, cy + g, s, 3 * s, col);
        if (flame.charging) {
            float k = std::min(1.f, flame.charge / 0.75f);
            R->uiRect(cx - 12 * s, cy + 10 * s, 24 * s, 3 * s, C4(0x000000, 0.6f));
            R->uiRect(cx - 12 * s, cy + 10 * s, 24 * s * k, 3 * s, k >= 1 ? C4(0xffffff) : C4(0xffa040));
            if (k >= 1) R->uiTextCentered(cx, cy + 15 * s, "SUNFLARE", s, C4(0xfff0c0));
        }
    }
    // boss bar
    if (boss.active && !boss.dead) {
        float bw = std::min(W * 0.6f, 260 * s), bx = W / 2 - bw / 2, by = 10 * s;
        R->uiTextCentered(W / 2, by, boss.name, s, C4(0xe0d0ff));
        R->uiFrame(bx, by + 10 * s, bw, 7 * s, C4(0x100818, 0.9f), C4(0x6a4a8a));
        float frac = clampf(boss.hp / boss.maxHp, 0, 1);
        R->uiRect(bx + s, by + 11 * s, (bw - 2 * s) * frac, 5 * s, C4(0xc04060));
        if (boss.type == B_HUSH && boss.phase == 0) {
            for (int h = 0; h < 2; h++) {
                float hx = W / 2 + (h == 0 ? -bw / 2 : bw / 2 - 60 * s);
                float hf = boss.hands[h].alive ? clampf(boss.hands[h].hp / 9.f, 0, 1) : 0;
                R->uiText(hx, by + 20 * s, h == 0 ? "LEFT HAND" : "RIGHT HAND", s, COL_DIM);
                R->uiRect(hx, by + 29 * s, 60 * s, 3 * s, C4(0x100818, 0.9f));
                R->uiRect(hx, by + 29 * s, 60 * s * hf, 3 * s, C4(0x9a5aff));
            }
        }
    }
    // building / endless info
    if (lv.id == LV_HOME && vox.active) drawHotbar();
    if (lv.id == LV_ENDLESS) {
        char db[64]; snprintf(db, sizeof(db), "DEPTH %d   (best %d)", save.endlessDepth, save.endlessBest);
        R->uiText(8 * s, 34 * s, db, s, C4(0xb090ff));
    }
    // mini-boss health bar
    if (!boss.active) {
        for (auto& e : enemies) {
            if (!e.alive || (e.type != E_GLOOMKING && e.type != E_MEGABOT) || len(e.pos - pl.pos) > 30 || e.state == 0) continue;
            float bw = std::min(W * 0.5f, 200 * s), bx = W / 2 - bw / 2, by = 10 * s;
            R->uiTextCentered(W / 2, by, e.type == E_GLOOMKING ? "THE GLOOM KING" : "MEGA-BOT 3000", s, C4(0xffd0a0));
            R->uiFrame(bx, by + 10 * s, bw, 6 * s, C4(0x100818, 0.9f), C4(0x8a6040));
            R->uiRect(bx + s, by + 11 * s, (bw - 2 * s) * clampf(e.hp / e.maxHp, 0, 1), 4 * s, C4(0xe08030));
            break;
        }
    }
    // hints
    std::string hintText;
    for (auto& h : lv.hints) {
        if (!h.zone.contains(pl.pos + V3(0, 0.5f, 0))) continue;
        if (!h.needFlag.empty() && !has(h.needFlag)) continue;
        if (!h.hideFlag.empty() && has(h.hideFlag)) continue;
        hintText = expandKeys(*this, h.text);
    }
    // interaction prompt
    for (auto& tk : lv.talkers) {
        if (len(tk.pos - pl.pos) < 3.0f && dialog.empty()) {
            static const char* names[] = {"Talk to Marrow", "Inspect the pedestals", "", "Sprocket's Robot Friends", "Talk to Gil", "Talk to Tilda", "Talk to Mort", "Talk to Rusty", "Talk to Pip", "Blueprint Table (build)", "Bramble's Supplies",
                                          "Talk to Mira", "Pet Whiskers", "Talk to Old Fenn", "Candle race with Flick", "Wardrobe mirror (change hat)", "Fish here"};
            if (tk.who == 16 && fishState > 0) continue;
            std::string what = names[std::max(0, std::min(16, tk.who))];
            std::string p = "[" + bindingName(settings.keys[A_INTERACT]) + "] " + what;
            float sx, sy;
            if (R->project(tk.pos + V3(0, 2.8f, 0), sx, sy)) {
                float w = R->uiTextWidth(p, s) + 8 * s;
                R->uiFrame(sx - w / 2, sy - 6 * s, w, 12 * s, COL_PANEL, COL_HI);
                R->uiTextCentered(sx, sy - 3 * s, p, s, COL_HI);
            }
        }
    }
    if (!hintText.empty() && dialog.empty()) {
        float bw = std::min(W - 40 * s, 360 * s);
        std::string wtxt = wrap(R, hintText, s, bw - 12 * s);
        int lines = 1 + (int)std::count(wtxt.begin(), wtxt.end(), '\n');
        float bh = lines * 9 * s + 8 * s;
        float by = H - 46 * s - bh;
        R->uiFrame(W / 2 - bw / 2, by, bw, bh, C4(0x0c0a14, 0.75f), C4(0x4a3a30));
        R->uiTextCentered(W / 2, by + 5 * s, wtxt, s, COL_TEXT);
    }
    // status line
    if (statusT > 0 && !statusMsg.empty()) {
        float a = std::min(1.f, statusT * 2);
        R->uiTextCentered(W / 2, H * 0.64f, statusMsg, s, V4(1, 0.9f, 0.75f, a));
    }
    // area title
    if (areaTitleT > 0) {
        float a = std::min(1.f, std::min(areaTitleT, 3.5f - areaTitleT) * 2);
        R->uiTextCentered(W / 2, H * 0.2f, lv.name, s * 3, V4(1, 0.85f, 0.6f, a));
        R->uiRect(W / 2 - 80 * s, H * 0.2f + 26 * s, 160 * s, s, V4(1, 0.7f, 0.4f, a * 0.6f));
    }
    // toast
    if (toastT > 0) {
        float a = std::min(1.f, std::min(toastT, 3.5f - toastT) * 3);
        R->uiTextCentered(W / 2, H * 0.3f, toast, s * 2.5f, V4(1, 0.82f, 0.45f, a));
        R->uiTextCentered(W / 2, H * 0.3f + 24 * s, toastSub, s * 1.2f, V4(0.95f, 0.9f, 0.85f, a));
    }
    drawExtrasHUD();
    // death
    if (pl.dead && pl.deadT > 0.4f) R->uiTextCentered(W / 2, H * 0.45f, "your flame gutters...", s * 2, V4(1, 0.6f, 0.4f, std::min(1.f, pl.deadT - 0.4f)));
}

void Game::drawExtrasHUD() {
    float s = (float)R->uiScale();
    float W = (float)R->winW, H = (float)R->winH;
    float t = time;
    std::string ik = "[" + bindingName(settings.keys[A_INTERACT]) + "]";
    // overclock
    if (ocT > 0) {
        R->uiTextCentered(W / 2, H * 0.14f, "OVERCLOCK", s * 2, V4(0.4f, 0.9f, 1, 0.6f + 0.4f * std::sin(t * 12)));
        R->uiRect(W / 2 - 40 * s, H * 0.14f + 18 * s, 80 * s * clampf(ocT / 4.5f, 0, 1), 2 * s, C4(0x60e0ff));
    }
    // Lumen City: alarm and the helipad countdown
    if (lv.id == LV_CITY) {
        float y = 44 * s;
        if (alarmT > 0) {
            bool on = std::fmod(t * 3, 1.f) < 0.6f;
            R->uiTextCentered(W / 2, y, lv.holdout.active ? "!! FBI ASSAULT !!" : "!! SPOTTED !!", s * 1.6f, on ? C4(0xff4040) : C4(0x4060ff));
        } else R->uiTextCentered(W / 2, y, "hidden", s, COL_DIM);
        if (lv.hasHoldout && lv.holdout.active) {
            char b[64]; snprintf(b, sizeof(b), "SPROCKET'S CHOPPER ETA  %02d", std::max(0, (int)std::ceil(lv.holdout.dur - lv.holdout.t)));
            R->uiTextCentered(W / 2, y + 16 * s, b, s * 1.4f, C4(0x60e0ff));
        }
    }
    // candle race
    if (raceIdx >= 0) {
        char b[96]; snprintf(b, sizeof(b), "CANDLE RACE   ring %d/%d   %.1fs", raceIdx + 1, (int)lv.race.size(), raceT);
        R->uiTextCentered(W / 2, 44 * s, b, s * 1.4f, C4(0xffd040));
    }
    // hacking puzzles
    for (auto& P : lv.puzzles) {
        if (!P.zone.contains(pl.pos + V3(0, 0.5f, 0)) || P.state == 0 || P.state == 3) continue;
        std::string m = P.state == 1 ? "WATCH THE SEQUENCE..." : (P.state == 2 ? "BURN IN ORDER  " + std::to_string(P.progress) + "/" + std::to_string(P.seq.size()) : "ACCESS DENIED - replaying");
        R->uiTextCentered(W / 2, 58 * s, m, s * 1.4f, P.state == 4 ? C4(0xff4040) : C4(0x60ff90));
        for (int k = 0; k < (int)P.seq.size(); k++) R->uiRect(W / 2 - P.seq.size() * 5 * s + k * 10 * s, 72 * s, 8 * s, 3 * s, k < P.progress ? C4(0x60ff90) : C4(0x203028));
        break;
    }
    // fishing
    if (fishState > 0) {
        float bw = 160 * s, bx = W / 2 - bw / 2, by = H * 0.72f;
        if (fishState == 1) R->uiTextCentered(W / 2, by, "Waiting for a bite...   (" + ik + " reels in)", s, COL_TEXT);
        else if (fishState == 2) R->uiTextCentered(W / 2, by - 6 * s, "!! BITE !!  PRESS " + ik, s * 2, std::fmod(t * 8, 1.f) < 0.5f ? C4(0xfff060) : C4(0xff8040));
        else {
            R->uiTextCentered(W / 2, by - 12 * s, "Press " + ik + " when the needle is in the GREEN", s, COL_TEXT);
            R->uiFrame(bx, by, bw, 10 * s, C4(0x0c0a14, 0.9f), COL_BORDER);
            R->uiRect(bx + bw * (fishZone - fishZoneW / 2), by + s, bw * fishZoneW, 8 * s, C4(0x40c060));
            R->uiRect(bx + bw * fishNeedle - s, by - 2 * s, 2 * s, 14 * s, C4(0xffffff));
        }
    }
}

// a shout-out to a friend's game, on the title, credits and victory screens
static void drawFriendPlug(Renderer* R, float W, float H, float s, float t) {
    const char* big = "TRY CABIN CAMPING TOO!";
    const char* small = "a retro horror game by CheezeKneez  -  thecheezekneez.itch.io/cabin-camping";
    float pulse = 0.8f + 0.2f * std::sin(t * 3);
    float y = H - 34 * s;
    R->uiTextCentered(W / 2, y, big, s * 1.6f, V4(0.55f * pulse + 0.3f, 1.0f * pulse, 0.45f * pulse, 1));
    R->uiTextCentered(W / 2, y + 14 * s, small, s, C4(0xb8c8a8));
}

void Game::drawUI() {
    float s = (float)R->uiScale();
    float W = (float)R->winW, H = (float)R->winH;
    // letterbox for cinematics
    if (letterbox > 0) {
        float bh = H * 0.12f * letterbox;
        R->uiRect(0, 0, W, bh, V4(0, 0, 0, 1));
        R->uiRect(0, H - bh, W, bh, V4(0, 0, 0, 1));
    }
    if (hideHUD) { if (overlayUI) overlayUI(); return; }
    if (screen == SC_PLAY || screen == SC_PAUSE || screen == SC_JOURNAL || (screen == SC_SETTINGS && prevScreen == SC_PAUSE) || screen == SC_CONTROLS) {
        if (screen == SC_PLAY) { drawHUD(); if (dialog.empty()) drawTutorial(); }
        // dialog box
        if (!dialog.empty() && screen == SC_PLAY) {
            auto& d = dialog[dialogIdx];
            float bw = std::min(W - 30 * s, 420 * s), bx = W / 2 - bw / 2;
            std::string full = wrap(R, d.text, s * 1.2f, bw - 20 * s);
            int lines = 1 + (int)std::count(full.begin(), full.end(), '\n');
            float bh = lines * 11 * s + 26 * s;
            float by = H - bh - 14 * s;
            R->uiFrame(bx, by, bw, bh, C4(0x0c0a14, 0.92f), COL_BORDER);
            if (!d.who.empty()) R->uiText(bx + 10 * s, by + 6 * s, d.who, s, COL_HI);
            // typewriter, counted on the wrapped text
            std::string shown = full.substr(0, std::min((size_t)dialogChars, full.size()));
            R->uiText(bx + 10 * s, by + 17 * s, shown, s * 1.2f, COL_TEXT);
            if ((size_t)dialogChars >= full.size() && std::fmod(time, 0.8f) < 0.5f)
                R->uiText(bx + bw - 16 * s, by + bh - 10 * s, "v", s, COL_HI);
        }
    }
    if (screen == SC_INTRO || screen == SC_ENDING) {
        static const char* introLines[] = {
            "Lumen Hollow was a city that had forgotten the night.",
            "Atop the Lighthouse Spire burned the Great Lamp -\nbrighter every year, until no one remembered the stars.",
            "Then, one evening, the Lamp went out.\nAnd the Hush came down the Spire like water.",
            "It drank the light from every window.\nThen it drank the people.",
            "At the foot of the Spire, in a cluttered workshop,\nthe last Lamplighter had one flame left.",
            "He gave it a body of wax.\nHe gave it a name.",
            "MARROW: ...Wick. Wake up, Wick."};
        static const char* endLines[] = {
            "The heart of the Hush comes apart like smoke in a draught.",
            "What remains is only an ordinary night -\nthe kind the Hollow had forgotten how to have.",
            "The Great Lamp stands cold and empty. It needs a flame.\nWick has exactly one.",
            "It is a small light. It casts a small shadow.\nThat is how it was always meant to be.",
            "Far below, one by one, the windows of Lumen Hollow begin to glow.",
            "MARROW: ...You did it, little flame. You did what I never could.",
            "Some weeks later, the old Lamplighter carved a new wick,\nand lit it from the Lamp at the top of the Spire.",
            "And Wick woke up."};
        int n = screen == SC_INTRO ? 7 : 8;
        if (cineStep < n) {
            const char* line = screen == SC_INTRO ? introLines[cineStep] : endLines[cineStep];
            float a = std::min(1.f, cineT * 1.5f);
            R->uiTextCentered(W / 2, H - H * 0.1f - 9 * s, line, s * 1.4f, V4(0.97f, 0.92f, 0.82f, a));
        }
        R->uiText(W - 150 * s, H - 9 * s, "[Enter] next   [Esc] skip", s, COL_DIM);
    }
    if (screen == SC_TITLE || screen == SC_CONFIRM_NEW || screen == SC_CREDITS) {
        // logo
        float flick = 0.85f + 0.15f * std::sin(time * 9) * std::sin(time * 3.3f);
        float ls = std::max(2.f, s * 4);
        R->uiTextCentered(W / 2 + ls, H * 0.16f + ls, "WICKLIGHT", ls, V4(0.3f, 0.1f, 0.05f, 1), false);
        R->uiTextCentered(W / 2, H * 0.16f, "WICKLIGHT", ls, V4(1, 0.7f * flick, 0.3f * flick, 1), false);
        R->uiTextCentered(W / 2, H * 0.16f + 9 * ls, "your head is the last flame in the world", s, C4(0xd0c0b0));
        if (saveCorrupt) R->uiTextCentered(W / 2, H - 22 * s, "Your save file was damaged and has been set aside (save.dat.corrupt).", s, C4(0xff8060));
        R->uiText(6 * s, H - 10 * s, "v1.1.0  native Linux build", s, COL_DIM);
        drawFriendPlug(R, W, H, s, time);
    }
    switch (screen) {
        case SC_TITLE: drawMenu(nullptr, menuItems(*this), menuSel, H * 0.5f); break;
        case SC_CONFIRM_NEW:
            R->uiTextCentered(W / 2, H * 0.45f, "Start a new game? Your current progress will be lost.", s * 1.2f, COL_TEXT);
            drawMenu(nullptr, menuItems(*this), menuSel, H * 0.55f);
            break;
        case SC_CREDITS: {
            R->uiRect(0, 0, W, H, V4(0, 0, 0, 0.5f));
            const char* cr =
                "WICKLIGHT\n\nDesign, code, art, sound & music\nmade entirely from code - no external assets.\n\n"
                "Engine: C++17, SDL2, OpenGL 3.3\nPixel font: classic public-domain 5x7\n\n"
                "Thank you for playing.\n\n[Enter] back";
            R->uiTextCentered(W / 2, H * 0.35f, cr, s * 1.2f, COL_TEXT);
        } break;
        case SC_PAUSE:
            R->uiRect(0, 0, W, H, V4(0, 0, 0, 0.55f));
            drawMenu("PAUSED", menuItems(*this), menuSel, H * 0.25f);
            {
                char buf[96];
                snprintf(buf, sizeof(buf), "%s   -   playtime %s", levelName(lv.id), fmtTime(save.playTime).c_str());
                R->uiTextCentered(W / 2, H - 16 * s, buf, s, COL_DIM);
            }
            break;
        case SC_SETTINGS:
            R->uiRect(0, 0, W, H, V4(0, 0, 0, prevScreen == SC_PAUSE ? 0.6f : 0.45f));
            drawMenu("SETTINGS", menuItems(*this), menuSel, H * 0.1f);
            R->uiTextCentered(W / 2, H - 12 * s, "Up/Down select   Left/Right change   Esc back", s, COL_DIM);
            break;
        case SC_CONTROLS:
            R->uiRect(0, 0, W, H, V4(0, 0, 0, 0.6f));
            drawMenu("CONTROLS", menuItems(*this), menuSel, H * 0.1f);
            R->uiTextCentered(W / 2, H - 22 * s, rebinding ? "Press a key or mouse button (Esc cancels)" : "Enter: rebind   Esc: back", s, COL_DIM);
            R->uiTextCentered(W / 2, H - 12 * s, "Gamepad: L-stick move, R-stick look, A jump, B dash, RT throw, LT warp, RB bloom, X interact, Y grapple, L3 firewall, R3 overclock", s, COL_DIM);
            break;
        case SC_JOURNAL: {
            R->uiRect(0, 0, W, H, V4(0.02f, 0.01f, 0.03f, 0.88f));
            R->uiTextCentered(W / 2, 12 * s, "JOURNAL", s * 2, COL_HI);
            float x = 20 * s, y = 36 * s;
            R->uiText(x, y, "POWERS", s * 1.2f, COL_HI); y += 14 * s;
            struct P { const char* n; const char* d; bool on; } pw[] = {
                {"Throw / Recall", "Hurl your flame; it burns going out and coming back.", true},
                {"Dash", "A short burst of speed. You can't be hurt mid-dash.", true},
                {"Warp", "Step into your thrown flame. Hooks catch it.", ability("warp")},
                {"Bloom", "Detonate your flame wherever it is.", ability("bloom")},
                {"Sunflare", "Hold to charge: farther, triple damage, breaks shields, shrouds, crystal.", ability("sunflare")},
                {"Overclock", "Hits charge it. When full, slow the whole world down for a few seconds.", ability("overclock")},
                {"Firewall", "Drop a wall of fire: enemies can't cross it, shots melt in it.", ability("firewall")},
                {"Ember Grapple", "Flame on a hook + hold Grapple: swing on a rope of fire, let go to fling.", ability("grapple")}};
            for (auto& p : pw) {
                R->uiText(x, y, p.on ? p.n : "???", s, p.on ? COL_TEXT : COL_DIM);
                if (p.on) R->uiText(x + 90 * s, y, p.d, s, COL_DIM);
                y += 11 * s;
            }
            y += 6 * s;
            char buf[128];
            snprintf(buf, sizeof(buf), "Beacon-Hearts %d/4    Ember Shards %d/7 (max flame %d)    Memory Candles %d/10    Deaths %d    Time %s",
                     countFlags("heart."), countFlags("shard."), (int)maxHp(), countFlags("candle."), save.deaths, fmtTime(save.playTime).c_str());
            R->uiText(x, y, wrap(R, buf, s, W - 40 * s), s, COL_TEXT); y += 12 * s;
            {
                int hats = 0; for (int h = 1; h <= 10; h++) if (has("hat." + std::to_string(h))) hats++;
                int fish = 0; for (int k = 0; k < 4; k++) if (has("fish." + std::to_string(k))) fish++;
                std::string side = "Hats " + std::to_string(hats) + "/10   Fish species " + std::to_string(fish) + "/4   ";
                side += has("quest.catdone") ? "[x] Mira's cat   " : (has("quest.cat") ? "[ ] Mira's cat   " : "");
                side += has("quest.fenndone") ? "[x] Fenn's braziers   " : (has("quest.fenn") ? "[ ] Fenn's braziers   " : "");
                side += has("quest.carpdone") ? "[x] Golden Carp   " : (has("quest.carp") ? "[ ] Golden Carp   " : "");
                side += has("fbi.done") ? "[x] Lumen City" : (has("boss.1") ? "[ ] Lumen City (east bridge)" : "");
                if (save.raceBest > 0) { char rb[48]; snprintf(rb, sizeof(rb), "   Race best %.1fs", save.raceBest); side += rb; }
                R->uiText(x, y, wrap(R, side, s, W - 40 * s), s, COL_DIM); y += 14 * s;
            }
            R->uiText(x, y, "MEMORIES", s * 1.2f, COL_HI); y += 14 * s;
            std::vector<int> found;
            for (int i = 0; i < 10; i++) if (has("candle." + std::to_string(i))) found.push_back(i);
            if (found.empty()) R->uiText(x, y, "No memory candles found yet. They glow pale blue.", s, COL_DIM);
            else {
                journalSel = std::min(journalSel, (int)found.size() - 1);
                for (int k = 0; k < (int)found.size(); k++) {
                    std::string label = "Memory " + std::to_string(found[k] + 1);
                    R->uiText(x, y + k * 10 * s, (k == journalSel ? "> " : "  ") + label, s, k == journalSel ? COL_HI : COL_TEXT);
                }
                std::string txt = wrap(R, memoryText(found[journalSel]), s, W - x - 110 * s);
                R->uiFrame(x + 90 * s, y - 4 * s, W - x - 100 * s, 60 * s, C4(0x0c0a14, 0.9f), COL_BORDER);
                R->uiText(x + 96 * s, y + 2 * s, txt, s, COL_TEXT);
            }
            R->uiTextCentered(W / 2, H - 12 * s, "Up/Down browse memories   Esc/Tab close", s, COL_DIM);
        } break;
        case SC_HOWTO: drawHowTo(); break;
        case SC_SHOP: drawShop(); break;
        case SC_BOTSHOP: drawBotShop(); break;
        case SC_CRAFT: drawCraft(); break;
        case SC_SUPPLY: drawSupply(); break;
        case SC_CHEST: drawChest(); break;
        case SC_VICTORY: {
            R->uiRect(0, 0, W, H, V4(0.03f, 0.02f, 0.02f, 0.75f));
            float a = std::min(1.f, screenT);
            R->uiTextCentered(W / 2, H * 0.14f, "THE LAMP BURNS AGAIN", s * 3, V4(1, 0.8f, 0.45f, a));
            char buf[256];
            snprintf(buf, sizeof(buf), "Time  %s\nDeaths  %d\nEmber Shards  %d / 7\nMemory Candles  %d / 10", fmtTime(save.playTime).c_str(), save.deaths, countFlags("shard."), countFlags("candle."));
            R->uiTextCentered(W / 2, H * 0.3f, buf, s * 1.4f, V4(0.95f, 0.9f, 0.85f, a));
            bool all = countFlags("shard.") >= 7 && countFlags("candle.") >= 10;
            drawFriendPlug(R, W, H, s, time);
            R->uiTextCentered(W / 2, H * 0.5f, all ? "Every memory remembered. Every shard found. Marrow would be proud." : "Some memories still flicker in the dark corners of the Hollow...", s, C4(0xc0b0a0));
            if (screenT > 1.0f) drawMenu(nullptr, menuItems(*this), menuSel, H * 0.62f);
        } break;
        default: break;
    }
    // global fade for respawn handled in post; transition text
    if (transitioning && transitionT > 0.2f) R->uiTextCentered(W / 2, H / 2, "...", s * 2, COL_DIM);
    if (overlayUI) overlayUI();  // showcase recording: captions on top of real menus
}

// ------------------------------------------------------------------ objective tracker & tutorial
std::string Game::currentObjective() const {
    int hearts = countFlags("heart.");
    if (has("game.finished") && lv.id == LV_HUB) return has("home.unlocked") ? "Endless Dark: east portal (best depth " + std::to_string(save.endlessBest) + ").  Wick's Island: west portal." : "Dive into the ENDLESS DARK (portal east of the Spire). Clear 10 depths to unlock Wick's Island!";
    if (!has("talk.marrow0")) return "Talk to Marrow in his workshop [" + bindingName(settings.keys[A_INTERACT]) + "]";
    switch (lv.id) {
        case LV_HUB:
            for (int i : {0, 1, 2, 4})
                if (has("boss." + std::to_string(i)) && !has("heart." + std::to_string(i))) {
                    static const char* where[5] = {"the Sunken Wharf (west)", "the Clockwood (north)", "the Ashen Vaults (east)", "", "the Junkyard (south-west)"};
                    return std::string("You left a Beacon-Heart behind! Go back to ") + where[i] + " and take it";
                }
            if (hearts == 0) return "Go WEST: down the steps to the docks and the Sunken Wharf";
            if (hearts == 1 && !has("boss.1")) return "Go NORTH: throw your flame across the broken bridge, then warp";
            if (hearts == 2 && !has("boss.2")) return "Go EAST: Bloom the red wax seal on the mausoleum";
            if (hearts == 3 && !has("boss.4")) return "Go SOUTH-WEST to the Junkyard gate (tip: buy a robot friend at Sprocket's stall first!)";
            if (has("boss.1") && !has("talk.fbi")) return "Something's going on - talk to Marrow in his workshop";
            if (!has("hub.spire")) return "Place the Beacon-Hearts at the pedestals by the Spire door [" + bindingName(settings.keys[A_INTERACT]) + "]";
            return "Enter the Lighthouse Spire (door on its south side)";
        case LV_WHARF:
            if (has("boss.0")) return has("heart.0") ? "Walk through the glowing door back to the Hollow" : "Take the Beacon-Heart";
            return "Follow the docks north and defeat the Wharf's guardian";
        case LV_CLOCK:
            if (has("boss.1")) return has("heart.1") ? "Walk through the glowing door back to the Hollow" : "Take the Beacon-Heart";
            return "Climb through the Clockwood and stop GL1TCH the hacker";
        case LV_VAULTS:
            if (has("boss.2")) return has("heart.2") ? "Walk through the glowing door back to the Hollow" : "Take the Beacon-Heart";
            return "Descend through the Vaults to the Matron's crypt";
        case LV_SPIRE: return "Climb the Spire and relight the Great Lamp";
        case LV_ENDLESS: return "Reach the portal at the end of Depth " + std::to_string(save.endlessDepth) + (save.endlessDepth % 5 == 0 ? " - BOSS DEPTH!" : "");
        case LV_HOME: return !has("npc.10") ? "Talk to Bramble the beaver for a starter kit [E]" : (!has("isle.mined") ? "Hold the throw button on a block to mine it" : (!has("isle.placed") ? "Place a block: pick it with [1]-[9], then the warp button" : (!has("isle.crafted") ? "Open crafting and make some planks" : "Build anything! Survive the night - or sleep in a bed")));
        case LV_CITY:
            if (has("pw.grapple")) return "Climb into Sprocket's chopper (the glowing door on the helipad)";
            if (has("city.holdout")) return "Grab Sprocket's package on the helipad";
            if (has("city.arena")) return "Cross the bridge to the HELIPAD and hold out until Sprocket arrives";
            if (has("city.g1")) return "Climb the fire escapes to the tower roof";
            return "Get past the FBI to the helipad at the north end - avoid the searchlights!";
        case LV_JUNK:
            if (has("boss.4")) return has("heart.4") ? "Walk through the glowing door back to the Hollow" : "Take the Beacon-Heart";
            return "Get through the Junkyard and dethrone KING RATTLETRAP";
        default: return "";
    }
}

void Game::drawTutorial() {
    float s = (float)R->uiScale();
    float W = (float)R->winW;
    // current goal, top right
    std::string obj = currentObjective();
    if (!obj.empty() && !(boss.active && !boss.dead)) {
        std::string txt = "GOAL: " + obj;
        float w = std::min(R->uiTextWidth(txt, s) + 10 * s, W * 0.4f);
        std::string wtxt = wrap(R, txt, s, w - 10 * s);
        int lines = 1 + (int)std::count(wtxt.begin(), wtxt.end(), '\n');
        R->uiFrame(W - w - 6 * s, 6 * s, w, lines * 9 * s + 7 * s, C4(0x0c0a14, 0.7f), C4(0x6a5040));
        R->uiText(W - w - 1 * s, 10 * s, wtxt, s, COL_HI);
    }
    // basics checklist (until learned)
    if (!has("talk.marrow0") || has("tut.done")) return;
    struct Step { const char* flag; std::string text; } steps[] = {
        {"tut.look", "Look around with the MOUSE"},
        {"tut.move", "Move with " + bindingName(settings.keys[A_FWD]) + bindingName(settings.keys[A_LEFT]) + bindingName(settings.keys[A_BACK]) + bindingName(settings.keys[A_RIGHT])},
        {"tut.jump", "Jump with [" + bindingName(settings.keys[A_JUMP]) + "] (works while running)"},
        {"tut.dash", "Dash with [" + bindingName(settings.keys[A_DASH]) + "] - you can't be hurt mid-dash"},
        {"tut.throw", "THROW your flame: aim with the crosshair, [" + bindingName(settings.keys[A_THROW]) + "]"},
        {"tut.recall", "RECALL it: press [" + bindingName(settings.keys[A_THROW]) + "] again (it burns on the way back)"},
    };
    int done = 0;
    for (auto& st : steps) if (has(st.flag)) done++;
    if (done == 6) {
        const_cast<Game*>(this)->setFlag("tut.done");
        statusMsg = "Basics learned! Follow the GOAL in the top-right corner."; statusT = 4;
        audio::play(SFX_CHECKPOINT);
        return;
    }
    std::string line;
    for (auto& st : steps) if (!has(st.flag)) { line = st.text; break; }
    std::string txt = "TUTORIAL " + std::to_string(done + 1) + "/6: " + line;
    float w = R->uiTextWidth(txt, s) + 10 * s;
    float x = 8 * s, y = (lv.id == LV_HOME || lv.id == LV_ENDLESS) ? 46 * s : 34 * s;  // below the depth / resources line
    R->uiFrame(x, y, w, 13 * s, C4(0x0c0a14, 0.7f), C4(0x6a5040));
    R->uiText(x + 5 * s, y + 3 * s, txt, s, COL_TEXT);
}

void Game::drawHowTo() {
    float s = (float)R->uiScale();
    float W = (float)R->winW, H = (float)R->winH;
    R->uiRect(0, 0, W, H, V4(0.02f, 0.01f, 0.03f, 0.9f));
    R->uiTextCentered(W / 2, 12 * s, "HOW TO PLAY", s * 2, COL_HI);
    auto K = [&](int a) { return "[" + bindingName(settings.keys[a]) + "]"; };
    std::string t =
        "YOUR GOAL\n"
        "The Great Lamp is dark and the Hush is drinking the city. Win the three\n"
        "Beacon-Hearts (Wharf to the WEST, Clockwood to the NORTH, Vaults to the EAST),\n"
        "set them at the Spire door, climb the Spire and relight the Lamp.\n"
        "The GOAL box in the top-right corner always tells you what to do next.\n\n"
        "YOUR FLAME IS YOUR HEAD - AND YOUR HEALTH (candles, top left)\n"
        "  " + K(A_THROW) + " throw it where the crosshair points. Press again to call it back.\n"
        "  It burns enemies going OUT and coming BACK, and lights braziers.\n"
        "  " + K(A_WARP) + " WARP: teleport to your thrown flame (earned in the Wharf).\n"
        "  " + K(A_BLOOM) + " BLOOM: explode the flame wherever it is (earned in the Clockwood).\n"
        "  Hold " + K(A_THROW) + " to charge a SUNFLARE (earned in the Vaults).\n"
        "  " + K(A_OVERCLOCK) + " OVERCLOCK slows time (charged by hits)   " + K(A_FIREWALL) + " FIREWALL   " + K(A_GRAPPLE) + " hold to GRAPPLE a hooked flame.\n\n"
        "MOVING\n"
        "  " + bindingName(settings.keys[A_FWD]) + bindingName(settings.keys[A_LEFT]) + bindingName(settings.keys[A_BACK]) + bindingName(settings.keys[A_RIGHT]) +
        " move   mouse look   " + K(A_JUMP) + " jump   " + K(A_DASH) + " dash (invulnerable)\n"
        "  " + K(A_INTERACT) + " talk / use   " + K(A_JOURNAL) + " journal   " + K(A_PAUSE) + " pause\n\n"
        "TIPS\n"
        "  Brass hooks catch your flame - throw at one and warp up to it.\n"
        "  Knights block from the front: throw past them, then recall into their backs.\n"
        "  Lanterns restore your flame and save. Falling in water only costs one candle.\n"
        "  Bosses have a moment of weakness - watch for the glow.\n"
        "  Hacked terminals flash a sequence: burn them in the same order.\n"
        "  Side jobs: Mira's cat, Old Fenn's braziers, Flick's candle race, fishing with Gil.\n"
        "  Hats hide in secret corners - wear them via the mirror in Marrow's workshop.";
    R->uiText(20 * s, 34 * s, wrap(R, t, s, W - 40 * s), s, COL_TEXT);
    R->uiTextCentered(W / 2, H - 12 * s, "Press Esc or Enter to go back", s, COL_DIM);
}

// ------------------------------------------------------------------ lantern upgrades
struct Upgrade { const char* id; const char* name; const char* desc; int max; int cost[3]; const char* need; };
static const Upgrade UPGRADES[] = {
    {"wax", "Thicker Wax", "+1 maximum flame (health)", 3, {40, 80, 140}, ""},
    {"heat", "Hotter Flame", "+50% damage from every throw and bloom", 2, {50, 110, 0}, ""},
    {"swift", "Swift Wick", "Faster throws that fly farther", 2, {30, 70, 0}, ""},
    {"jump", "Second Wind", "Jump again in mid-air (resets when you warp)", 1, {60, 0, 0}, ""},
    {"magnet", "Ember Magnet", "Pull embers and wax from much farther away", 2, {20, 50, 0}, ""},
    {"bloom", "Wide Bloom", "Bigger blooms that recharge faster", 2, {60, 120, 0}, "ab.bloom"},
};
static const int NUM_UP = 6;

void Game::updateShop(const InputState& in) {
    int n = NUM_UP + 1;
    if (in.mUp) { shopSel = (shopSel + n - 1) % n; audio::play(SFX_MENU_MOVE); }
    if (in.mDown) { shopSel = (shopSel + 1) % n; audio::play(SFX_MENU_MOVE); }
    if (mouseMoved) for (auto& r : menuRects) if (mouseX >= r.x && mouseX <= r.x + r.w && mouseY >= r.y && mouseY <= r.y + r.h) shopSel = r.idx;
    bool clicked = false;
    if (mouseClick) for (auto& r : menuRects) if (mouseX >= r.x && mouseX <= r.x + r.w && mouseY >= r.y && mouseY <= r.y + r.h) { shopSel = r.idx; clicked = true; }
    if (in.mBack || in.pressed[A_PAUSE] || in.pressed[A_INTERACT]) { screen = SC_PLAY; audio::play(SFX_MENU_BACK); return; }
    if (!(in.mOk || clicked)) return;
    if (shopSel == NUM_UP) { screen = SC_PLAY; audio::play(SFX_MENU_BACK); return; }
    const Upgrade& u = UPGRADES[shopSel];
    int lvl = upLevel(u.id);
    if (*u.need && !has(u.need)) { statusMsg = "Not yet - your flame hasn't learned to bloom"; statusT = 1.5f; audio::play(SFX_MENU_BACK); return; }
    if (lvl >= u.max) { audio::play(SFX_MENU_BACK); return; }
    int cost = u.cost[lvl];
    if (save.embers < cost) { statusMsg = "Not enough embers"; statusT = 1.5f; audio::play(SFX_MENU_BACK); return; }
    save.embers -= cost;
    setFlag(std::string("up.") + u.id + "." + std::to_string(lvl + 1));
    if (std::string(u.id) == "wax") pl.hp = maxHp();
    audio::play(SFX_UNLOCK);
    flashOverlay = 0.3f; flashColor = hexColor(0xffd080);
    burst(pl.pos + V3(0, 1.2f, 0), hexColor(0xffc060), 30, 4, 0.1f, 3, 0.8f, -1);
    writeSave();
}

void Game::drawShop() {
    float s = (float)R->uiScale();
    float W = (float)R->winW, H = (float)R->winH;
    float pw = std::min(W - 20 * s, 360 * s), px = W / 2 - pw / 2, py = H * 0.12f;
    float ph = 40 * s + (NUM_UP + 1) * 20 * s;
    R->uiFrame(px, py, pw, ph, C4(0x0c0a14, 0.93f), COL_HI);
    R->uiTextCentered(W / 2, py + 6 * s, "TEND YOUR FLAME", s * 1.6f, COL_HI);
    R->uiTextCentered(W / 2, py + 20 * s, "Embers: " + std::to_string(save.embers), s, C4(0xffc070));
    menuRects.clear();
    float y = py + 34 * s;
    for (int i = 0; i <= NUM_UP; i++) {
        bool sel = i == shopSel;
        if (sel) R->uiRect(px + 3 * s, y - 2 * s, pw - 6 * s, 19 * s, C4(0x3a2418, 0.8f));
        menuRects.push_back({px, y - 2 * s, pw, 19 * s, i});
        if (i == NUM_UP) { R->uiText(px + 10 * s, y + 4 * s, "Leave", s * 1.2f, sel ? COL_HI : COL_TEXT); break; }
        const Upgrade& u = UPGRADES[i];
        int lvl = upLevel(u.id);
        bool locked = *u.need && !has(u.need);
        std::string pips;
        for (int k = 0; k < u.max; k++) pips += k < lvl ? "#" : ".";
        R->uiText(px + 10 * s, y, std::string(u.name) + "  [" + pips + "]", s * 1.2f, locked ? COL_DIM : (sel ? COL_HI : COL_TEXT));
        R->uiText(px + 10 * s, y + 10 * s, locked ? "Locked - learn Bloom first" : u.desc, s, COL_DIM);
        std::string price = lvl >= u.max ? "MAX" : std::to_string(u.cost[lvl]);
        bool afford = lvl < u.max && save.embers >= u.cost[lvl] && !locked;
        R->uiText(px + pw - R->uiTextWidth(price, s * 1.2f) - 10 * s, y + 3 * s, price, s * 1.2f, lvl >= u.max ? C4(0x7ac070) : (afford ? C4(0xffc070) : COL_DIM));
        y += 20 * s;
    }
    R->uiTextCentered(W / 2, py + ph + 6 * s, "Enemies and urns drop embers - chain hits for a combo multiplier!\nEnter: buy   Esc/E: leave", s, COL_DIM);
}
