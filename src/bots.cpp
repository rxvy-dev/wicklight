// WICKLIGHT - robot friends from Sprocket's stall, bought with embers
#include "game.h"
#include <cstdio>
#include <algorithm>

extern int g_dmgKind;

struct BotDef { const char* name; const char* desc; int cost; uint32_t color; };
static const BotDef BOTS[5] = {
    {"SPARK", "Flying drone that shoots fireballs at enemies", 80, 0xff7040},
    {"MAGS", "Vacuums up embers and wax from far away", 50, 0x40c0ff},
    {"PATCH", "Medic bot: heals 1 flame every 15 seconds", 120, 0x60ff80},
    {"BUBBLES", "Shield bot: blocks one hit every 12 seconds", 150, 0xc080ff},
    {"BOOMER", "Lobs firebombs into groups of enemies", 200, 0xffd040},
};
static V4 C4(uint32_t h, float a = 1) { V3 c = hexColor(h); return V4(c.x, c.y, c.z, a); }

bool Game::buyOrToggleBot(int i) {
    if (i < 0 || i >= 5) return false;
    std::string own = "bot.own." + std::to_string(i), eq = "bot.eq." + std::to_string(i);
    if (!botOwned(i)) {
        if (save.embers < BOTS[i].cost) { statusMsg = "Not enough embers"; statusT = 1.5f; audio::play(SFX_MENU_BACK); return false; }
        save.embers -= BOTS[i].cost;
        setFlag(own);
        audio::play(SFX_UNLOCK);
        if (botsEquipped() < botSlots()) setFlag(eq);
        showToast(std::string(BOTS[i].name) + " JOINED YOU!", BOTS[i].desc);
        bots[i].pos = pl.pos + V3(0, 2, 0);
        writeSave();
        return true;
    }
    if (botEquipped(i)) { save.flags.erase(eq); audio::play(SFX_MENU_BACK); }
    else if (botsEquipped() >= botSlots()) { statusMsg = "All robot slots are full - send one home first"; statusT = 2; audio::play(SFX_MENU_BACK); return false; }
    else { setFlag(eq); audio::play(SFX_MENU_SELECT); bots[i].pos = pl.pos + V3(0, 2, 0); }
    writeSave();
    return true;
}

void Game::updateBots(float dt) {
    bool core = has("spark.core");
    float speedMul = core ? 1.4f : 1.f;
    int slot = 0, n = botsEquipped();
    for (int i = 0; i < 5; i++) {
        if (!botEquipped(i)) continue;
        BotRT& b = bots[i];
        b.flash = std::max(0.f, b.flash - dt);
        // hover in a little formation behind the player
        float a = pl.yaw + PI + (slot - (n - 1) * 0.5f) * 0.9f;
        V3 want = pl.pos + V3(std::sin(a) * 1.6f, 2.0f + 0.2f * std::sin(time * 3 + i), std::cos(a) * 1.6f);
        if (len(b.pos - pl.pos) > 25) b.pos = want;
        b.pos = lerp(b.pos, want, 1 - std::exp(-dt * 5));
        slot++;
        if (pl.dead) continue;
        b.timer += dt * speedMul;
        switch (i) {
            case 0: {  // SPARK: fireballs
                if (b.timer < 1.0f) break;
                Enemy* best = nullptr; float bd = 14;
                for (auto& e : enemies) {
                    if (!e.alive || e.clinging || (e.type == E_WRAITH && e.reveal < 0.3f)) continue;
                    float d = len(e.pos - b.pos);
                    if (d < bd) { bd = d; best = &e; }
                }
                V3 tgt; bool have = false;
                if (best) { tgt = best->pos + V3(0, best->height * 0.5f, 0); have = true; }
                else if (boss.active && !boss.dead) for (auto& w : boss.wps) if (w.vulnerable && len(w.pos - b.pos) < 20) { tgt = w.pos; have = true; break; }
                if (have) {
                    Bolt bo; bo.pos = b.pos; bo.target = tgt; bo.vel = norm(tgt - b.pos) * 18; bo.life = 1.5f;
                    bolts.push_back(bo);
                    b.timer = 0; b.flash = 0.15f;
                    audio::playAt(SFX_THROW, b.pos, 0.35f, 1.8f);
                }
            } break;
            case 1: {  // MAGS: vacuum
                for (auto& p : lv.pickups) {
                    if (p.taken || (p.type != PK_EMBER && p.type != PK_WAX)) continue;
                    if (p.type == PK_WAX && pl.hp >= maxHp()) continue;
                    if (len(p.pos - pl.pos) < 16) p.pos = lerp(p.pos, pl.pos + V3(0, 0.6f, 0), std::min(1.f, dt * 5));
                }
            } break;
            case 2: {  // PATCH: heal
                if (pl.hp < maxHp() && b.timer > 15) {
                    pl.hp = std::min(maxHp(), pl.hp + 1);
                    b.timer = 0; b.flash = 0.5f;
                    audio::play(SFX_CATCH, 0.8f, 1.5f);
                    burst(pl.pos + V3(0, 1, 0), hexColor(0x60ff80), 16, 3, 0.08f, 3, 0.6f, -1);
                    floatText(pl.pos + V3(0, 2, 0), "+1 HEAL", hexColor(0x60ff80));
                } else if (pl.hp >= maxHp()) b.timer = std::min(b.timer, 15.f);
            } break;
            case 3:  // BUBBLES: shield charge
                b.charge = std::min(1.f, b.charge + dt * speedMul / 12.f);
                break;
            case 4: {  // BOOMER: firebombs at groups
                if (b.timer < 5) break;
                Enemy* best = nullptr; int bestN = 0;
                for (auto& e : enemies) {
                    if (!e.alive || len(e.pos - pl.pos) > 14) continue;
                    int cnt = 0;
                    for (auto& o : enemies) if (o.alive && len(o.pos - e.pos) < 3) cnt++;
                    if (cnt > bestN) { bestN = cnt; best = &e; }
                }
                if (best) {
                    Bolt bo; bo.bomb = true; bo.pos = b.pos; bo.target = best->pos; bo.fuse = 0.9f; bo.life = 3;
                    V3 d = best->pos - b.pos; float T = 0.9f;
                    bo.vel = V3(d.x / T, (d.y + 0.5f * 20 * T * T) / T, d.z / T);
                    bolts.push_back(bo);
                    b.timer = 0; b.flash = 0.3f;
                    audio::playAt(SFX_SPIT, b.pos, 0.6f, 1.6f);
                }
            } break;
        }
    }
    // projectiles from robots
    for (auto& bo : bolts) {
        if (!bo.alive) continue;
        bo.life -= dt;
        if (bo.life <= 0) { bo.alive = false; continue; }
        if (!bo.bomb) {
            bo.pos += bo.vel * dt;
            g_dmgKind = 0;
            for (auto& e : enemies) {
                if (!e.alive || e.clinging) continue;
                if (len(e.pos + V3(0, e.height * 0.5f, 0) - bo.pos) < e.radius + 0.4f) {
                    g_dmgKind = 2;  // robots ignore shields
                    damageEnemy(e, core ? 1.5f : 1.f, bo.pos - bo.vel * 0.1f, false);
                    g_dmgKind = 0;
                    bo.alive = false; break;
                }
            }
            if (bo.alive && boss.active && !boss.dead) {
                auto wps = boss.wps;
                for (auto& w : wps) if (w.vulnerable && len(w.pos - bo.pos) < w.r + 0.3f) { bossDamage(w.id, 0.5f, false); bo.alive = false; break; }
            }
            if (bo.alive && raycast(bo.pos, norm(bo.vel), 0.3f) >= 0) bo.alive = false;
            if (!headless && R) { Particle q; q.pos = bo.pos; q.color = hexColor(0xff8040); q.alpha = 1; q.size = 0.12f; q.life = q.maxLife = 0.2f; q.kind = 3; R->spawn(q); }
        } else {
            bo.vel.y -= 20 * dt;
            bo.pos += bo.vel * dt;
            bo.fuse -= dt;
            if (bo.fuse <= 0) {
                bo.alive = false;
                g_dmgKind = 2;
                for (auto& e : enemies) if (e.alive && len(e.pos - bo.pos) < 3.0f) damageEnemy(e, core ? 3.f : 2.f, bo.pos, true);
                g_dmgKind = 0;
                audio::playAt(SFX_BLOOM, bo.pos, 0.6f, 1.4f);
                burst(bo.pos, hexColor(0xffb040), 40, 8, 0.12f, 3, 0.6f, 2);
                addShake(0.15f);
            }
        }
    }
    bolts.erase(std::remove_if(bolts.begin(), bolts.end(), [](const Bolt& b) { return !b.alive; }), bolts.end());
}

// ------------------------------------------------------------------ Sprocket's shop UI
void Game::updateBotShop(const InputState& in) {
    int n = 6;
    if (in.mUp) { shopSel = (shopSel + n - 1) % n; audio::play(SFX_MENU_MOVE); }
    if (in.mDown) { shopSel = (shopSel + 1) % n; audio::play(SFX_MENU_MOVE); }
    if (mouseMoved) for (auto& r : menuRects) if (mouseX >= r.x && mouseX <= r.x + r.w && mouseY >= r.y && mouseY <= r.y + r.h) shopSel = r.idx;
    bool clicked = false;
    if (mouseClick) for (auto& r : menuRects) if (mouseX >= r.x && mouseX <= r.x + r.w && mouseY >= r.y && mouseY <= r.y + r.h) { shopSel = r.idx; clicked = true; }
    if (in.mBack || in.pressed[A_PAUSE] || in.pressed[A_INTERACT]) { screen = SC_PLAY; audio::play(SFX_MENU_BACK); return; }
    if (!(in.mOk || clicked)) return;
    if (shopSel == 5) { screen = SC_PLAY; audio::play(SFX_MENU_BACK); return; }
    buyOrToggleBot(shopSel);
}

void Game::drawBotShop() {
    float s = (float)R->uiScale();
    float W = (float)R->winW, H = (float)R->winH;
    float pw = std::min(W - 20 * s, 380 * s), px = W / 2 - pw / 2, py = H * 0.1f;
    float ph = 44 * s + 6 * 22 * s;
    R->uiFrame(px, py, pw, ph, C4(0x08101a, 0.94f), C4(0x40f0ff));
    R->uiTextCentered(W / 2, py + 6 * s, "SPROCKET'S ROBOT FRIENDS", s * 1.6f, C4(0x40f0ff));
    char buf[96];
    snprintf(buf, sizeof(buf), "Embers: %d     Robot slots: %d/%d%s", save.embers, botsEquipped(), botSlots(), has("spark.core") ? "  (Spark Core!)" : "");
    R->uiTextCentered(W / 2, py + 21 * s, buf, s, C4(0xffc070));
    menuRects.clear();
    float y = py + 36 * s;
    for (int i = 0; i < 6; i++) {
        bool sel = i == shopSel;
        if (sel) R->uiRect(px + 3 * s, y - 2 * s, pw - 6 * s, 21 * s, C4(0x183040, 0.85f));
        menuRects.push_back({px, y - 2 * s, pw, 21 * s, i});
        if (i == 5) { R->uiText(px + 10 * s, y + 4 * s, "Leave", s * 1.2f, sel ? C4(0x40f0ff) : C4(0xf0e6d0)); break; }
        const BotDef& b = BOTS[i];
        // little robot icon
        R->uiRect(px + 8 * s, y + 1 * s, 12 * s, 12 * s, C4(b.color));
        R->uiRect(px + 11 * s, y + 4 * s, 6 * s, 3 * s, C4(0x101018));
        R->uiText(px + 26 * s, y, b.name, s * 1.2f, sel ? C4(0x40f0ff) : C4(0xf0e6d0));
        R->uiText(px + 26 * s, y + 10 * s, b.desc, s, C4(0x8a8098));
        std::string tag;
        V4 tc = C4(0xffc070);
        if (!botOwned(i)) { tag = std::to_string(b.cost); if (save.embers < b.cost) tc = C4(0x8a8098); }
        else if (botEquipped(i)) { tag = "WITH YOU"; tc = C4(0x7ac070); }
        else { tag = "AT HOME"; tc = C4(0x8a8098); }
        R->uiText(px + pw - R->uiTextWidth(tag, s * 1.2f) - 10 * s, y + 4 * s, tag, s * 1.2f, tc);
        y += 22 * s;
    }
    R->uiTextCentered(W / 2, py + ph + 6 * s, "Enter: buy / bring along / send home    Esc: leave", s, C4(0x8a8098));
}

// ------------------------------------------------------------------ drawing robots
void drawBots(Game& g, Renderer* R) {
    float t = g.time;
    for (int i = 0; i < 5; i++) {
        if (!g.botEquipped(i)) continue;
        const auto& b = g.bots[i];
        V3 col = hexColor(BOTS[i].color);
        float yaw = g.pl.yaw;
        M4 m = M4::translate(b.pos) * M4::rotY(yaw);
        float fl = b.flash > 0 ? 0.6f : 0;
        switch (i) {
            case 0: R->drawPart(PM_OCTA, m * M4::rotY(t * 3) * M4::scale(V3(0.5f, 0.6f, 0.5f)), T_METAL, col, 0.4f, fl); break;
            case 1: R->drawPart(PM_CYL, m * M4::scale(V3(0.55f, 0.35f, 0.55f)), T_METAL, col, 0.3f, fl);
                    R->drawPart(PM_CUBE, m * M4::translate(V3(0, -0.3f, 0)) * M4::scale(V3(0.7f, 0.12f, 0.2f)), T_METAL, hexColor(0xc04040), 0.3f); break;
            case 2: R->drawPart(PM_CUBE, m * M4::scale(V3(0.5f, 0.5f, 0.5f)), T_WHITE, V3(0.9f, 0.95f, 0.9f), 0.2f, fl);
                    R->drawPart(PM_CUBE, m * M4::translate(V3(0, 0, 0.26f)) * M4::scale(V3(0.32f, 0.1f, 0.02f)), T_WHITE, col, 2);
                    R->drawPart(PM_CUBE, m * M4::translate(V3(0, 0, 0.26f)) * M4::scale(V3(0.1f, 0.32f, 0.02f)), T_WHITE, col, 2); break;
            case 3: R->drawPart(PM_SPHERE, m * M4::scale(V3(0.5f, 0.5f, 0.5f)), T_METAL, col, 0.3f, fl);
                    if (b.charge >= 1) R->drawPart(PM_SPHERE, M4::translate(g.pl.pos + V3(0, 0.7f, 0)) * M4::scale(V3(1.7f, 1.9f, 1.7f)), T_WHITE, col, 0.8f, 0, 0.18f);
                    break;
            case 4: R->drawPart(PM_SPHERE, m * M4::scale(V3(0.6f, 0.55f, 0.6f)), T_TAR, V3(1, 1, 1), 0, fl);
                    R->drawPart(PM_CYL, m * M4::translate(V3(0, 0.35f, 0)) * M4::scale(V3(0.12f, 0.25f, 0.12f)), T_ROPE, V3(1, 1, 1));
                    R->drawPart(PM_CUBE, m * M4::translate(V3(0, 0.5f, 0)) * M4::scale(V3(0.08f, 0.08f, 0.08f)), T_WHITE, col, 3); break;
        }
        // eye + propeller
        R->drawPart(PM_CUBE, m * M4::translate(V3(0, 0.05f, 0.3f)) * M4::scale(V3(0.18f, 0.1f, 0.04f)), T_WHITE, hexColor(0xe0f8ff), 2.5f);
        R->drawPart(PM_CUBE, m * M4::translate(V3(0, 0.42f, 0)) * M4::rotY(t * 25) * M4::scale(V3(0.8f, 0.03f, 0.1f)), T_METAL, V3(0.8f, 0.8f, 0.8f));
    }
    for (auto& bo : g.bolts) {
        if (bo.bomb) R->drawPart(PM_SPHERE, M4::translate(bo.pos) * M4::scale(V3(0.45f, 0.45f, 0.45f)), T_TAR, V3(1, 1, 1), 0.5f);
        else R->drawPart(PM_OCTA, M4::translate(bo.pos) * M4::scale(V3(0.3f, 0.3f, 0.3f)), T_WHITE, hexColor(0xff9040), 3);
    }
}
