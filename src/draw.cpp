// WICKLIGHT - drawing the world, characters, enemies and bosses
#include "game.h"

static const V3 FLAME_COL = V3(1.0f, 0.62f, 0.28f);
void drawHat(Renderer* R, const M4& base, int hat, float headY, float t, float alpha);
float hatLift(int hat);
void drawNPCExtra(Renderer* R, int who, V3 pos, float yaw, float t);
void drawEnemyExtra(Game& g, Renderer* R, const Enemy& e);
void drawPickupExtra(Renderer* R, const Pickup& p, V3 pos, float t);
void drawWorldExtras(Game& g, Renderer* R);
void extrasLights(Game& g, Renderer* R);

static M4 TRS(V3 t, float yaw, V3 s) { return M4::translate(t) * M4::rotY(yaw) * M4::scale(s); }

static void glow(Renderer* R, V3 p, V3 col, float size, float alpha = 1) {
    if (R->shadowPass) return;
    Particle q; q.pos = p; q.color = col; q.alpha = alpha; q.size = size; q.life = q.maxLife = 0.0001f; q.kind = 1;
    R->spawn(q);
}

// layered pixel flame
static void drawFlame(Renderer* R, V3 p, float scale, float t, bool white, float alpha = 1) {
    V3 outer = white ? hexColor(0xfff0c0) : hexColor(0xff7a20);
    V3 mid = white ? hexColor(0xffffff) : hexColor(0xffb040);
    V3 core = white ? hexColor(0xffffff) : hexColor(0xfff0a0);
    float f1 = std::sin(t * 13) * 0.08f, f2 = std::sin(t * 17 + 1) * 0.06f;
    R->drawPart(PM_OCTA, TRS(p + V3(0, 0.12f * scale, 0), t * 2, V3(0.42f + f1, 0.75f + f2, 0.42f + f1) * scale), T_WHITE, outer, 2.2f, 0, alpha);
    R->drawPart(PM_OCTA, TRS(p + V3(0, 0.1f * scale, 0), -t * 3, V3(0.28f, 0.52f + f1, 0.28f) * scale), T_WHITE, mid, 2.6f, 0, alpha);
    R->drawPart(PM_OCTA, TRS(p + V3(0, 0.05f * scale, 0), t * 4, V3(0.15f, 0.28f, 0.15f) * scale), T_WHITE, core, 3.0f, 0, alpha);
}

static void drawWick(Game& g, Renderer* R) {
    Player& p = g.pl;
    float t = g.time;
    float melt = p.dead ? std::min(1.f, p.deadT / 1.2f) : 0;
    float alpha = (p.invuln > 0 && !p.dead && std::fmod(t, 0.16f) < 0.08f) ? 0.45f : 1.f;
    float sq = 1 - p.land * 0.18f;
    float walk = std::sin(p.walkPhase * 2.2f);
    float speed = lenXZ(p.vel);
    M4 base = M4::translate(p.pos) * M4::rotY(p.yaw);
    float hs = (1 - melt * 0.7f);
    V3 wax = hexColor(0xeadfc4);
    float flash = p.hurtT > 0 ? 0.6f : 0;
    float bob = g.pl.grounded ? std::fabs(walk) * 0.04f * std::min(1.f, speed / 4) : 0;
    // legs
    for (int s = -1; s <= 1; s += 2) {
        float sw = g.pl.grounded ? walk * s * 0.5f * std::min(1.f, speed / 4) : 0.4f * s;
        R->drawPart(PM_CUBE, base * M4::translate(V3(0.12f * s, 0.18f * hs, 0)) * M4::rotX(sw) * M4::translate(V3(0, -0.08f, 0)) * M4::scale(V3(0.13f, 0.3f * hs, 0.14f)), T_WAX, wax * 0.85f, 0, flash, alpha);
    }
    // body (candle stub)
    R->drawPart(PM_CYL, base * M4::translate(V3(0, (0.55f + bob) * hs * sq, 0)) * M4::scale(V3(0.5f, 0.55f * hs * sq, 0.46f)), T_WAX, wax, 0, flash, alpha);
    // drips
    R->drawPart(PM_CUBE, base * M4::translate(V3(0.16f, 0.62f * hs, 0.17f)) * M4::scale(V3(0.07f, 0.22f * hs, 0.07f)), T_WAX, hexColor(0xfff6e0), 0, flash, alpha);
    // arms
    bool aiming = g.flame.charging;
    for (int s = -1; s <= 1; s += 2) {
        float sw = aiming && s > 0 ? -2.2f : (g.pl.grounded ? -walk * s * 0.6f * std::min(1.f, speed / 4) : -0.8f);
        if (g.pl.dashT > 0) sw = 1.2f;
        R->drawPart(PM_CUBE, base * M4::translate(V3(0.29f * s, 0.72f * hs, 0)) * M4::rotX(sw) * M4::translate(V3(0, -0.13f, 0)) * M4::scale(V3(0.1f, 0.28f, 0.1f)), T_WAX, wax * 0.92f, 0, flash, alpha);
    }
    // head
    float headY = (0.98f + bob) * hs * sq;
    R->drawPart(PM_CYL, base * M4::translate(V3(0, headY, 0)) * M4::scale(V3(0.42f, 0.3f * hs, 0.42f)), T_WAX, wax * 1.02f, 0, flash, alpha);
    // eyes
    for (int s = -1; s <= 1; s += 2)
        R->drawPart(PM_CUBE, base * M4::translate(V3(0.09f * s, headY + 0.02f, 0.2f)) * M4::scale(V3(0.06f, 0.09f, 0.03f)), T_WHITE, hexColor(0x1a1018), 0, 0, alpha);
    // hat
    int hat = (g.screen == SC_INTRO || melt > 0.3f) ? 0 : g.save.hat;
    float lift = hatLift(hat);
    drawHat(R, base, hat, headY, t, alpha);
    // wick
    bool held = g.flame.st == F_HELD && !p.dead;
    R->drawPart(PM_CUBE, base * M4::translate(V3(0, headY + 0.2f + lift, 0)) * M4::scale(V3(0.04f, 0.12f, 0.04f)), T_WHITE, held ? hexColor(0x3a2a20) : hexColor(0xff5020), held ? 0.f : 1.5f, 0, alpha);
    if (held && g.screen != SC_INTRO) {
        float sc = 1.0f;
        bool white = false;
        if (g.flame.charging) { sc = 1 + std::min(g.flame.charge, 0.75f) * 0.8f; white = g.flame.charge >= 0.75f; }
        drawFlame(R, p.pos + V3(0, headY + 0.32f + lift, 0), sc, t, white, alpha);
    }
    // overclock: a cyan afterimage trail
    if (g.ocT > 0 && !R->shadowPass) {
        V3 v = flatXZ(p.vel);
        for (int k = 1; k <= 3; k++) R->drawPart(PM_CYL, M4::translate(p.pos - v * (0.04f * k) + V3(0, 0.6f, 0)) * M4::rotY(p.yaw) * M4::scale(V3(0.5f, 1.1f, 0.46f)), T_WHITE, hexColor(0x60e0ff), 1.5f, 0, 0.25f / k);
    }
    if (g.screen == SC_INTRO && g.flame.charging) drawFlame(R, p.pos + V3(0, headY + 0.32f, 0), 1, t, false);
    if (melt > 0) R->drawPart(PM_CYL, M4::translate(p.pos + V3(0, 0.02f, 0)) * M4::scale(V3(0.5f + melt * 0.8f, 0.04f, 0.5f + melt * 0.8f)), T_WAX, wax, 0, 0, 1);
}

static void drawMarrow(Renderer* R, V3 pos, float yaw, float t) {
    M4 b = M4::translate(pos) * M4::rotY(yaw);
    float breathe = std::sin(t * 1.5f) * 0.02f;
    R->drawPart(PM_CONE, b * M4::translate(V3(0, 0.75f, 0)) * M4::scale(V3(1.1f, 1.5f, 0.9f)), T_CLOTH, hexColor(0x3a4a7a));
    R->drawPart(PM_CYL, b * M4::translate(V3(0, 1.25f + breathe, 0)) * M4::scale(V3(0.62f, 0.7f, 0.5f)), T_CLOTH, hexColor(0x34446e));
    R->drawPart(PM_SPHERE, b * M4::translate(V3(0, 1.82f + breathe, 0)) * M4::scale(V3(0.46f, 0.5f, 0.46f)), T_WHITE, hexColor(0xd8b090));
    R->drawPart(PM_CONE, b * M4::translate(V3(0, 1.52f + breathe, 0.14f)) * M4::rotX(PI) * M4::scale(V3(0.4f, 0.55f, 0.3f)), T_WHITE, hexColor(0xe8e8e8));
    R->drawPart(PM_CYL, b * M4::translate(V3(0, 2.05f + breathe, 0)) * M4::scale(V3(0.6f, 0.06f, 0.6f)), T_CLOTH, hexColor(0x2a2a3a));
    R->drawPart(PM_CONE, b * M4::translate(V3(0, 2.32f + breathe, -0.05f)) * M4::rotX(-0.2f) * M4::scale(V3(0.4f, 0.55f, 0.4f)), T_CLOTH, hexColor(0x2a2a3a));
    for (int s = -1; s <= 1; s += 2) R->drawPart(PM_CUBE, b * M4::translate(V3(0.1f * s, 1.88f, 0.21f)) * M4::scale(V3(0.08f, 0.05f, 0.03f)), T_WHITE, hexColor(0x101010));
    // spectacles glint
    R->drawPart(PM_CUBE, b * M4::translate(V3(0, 1.88f, 0.23f)) * M4::scale(V3(0.3f, 0.02f, 0.02f)), T_WHITE, hexColor(0xc0a060), 0.5f);
    // lantern staff
    R->drawPart(PM_CYL, b * M4::translate(V3(0.6f, 1.1f, 0.2f)) * M4::scale(V3(0.08f, 2.2f, 0.08f)), T_DARKWOOD, V3(1, 1, 1));
    R->drawPart(PM_CUBE, b * M4::translate(V3(0.6f, 2.25f, 0.2f)) * M4::scale(V3(0.2f, 0.25f, 0.2f)), T_WHITE, hexColor(0x806040), 0.3f);
}

void drawBots(Game& g, Renderer* R);
void drawHomeExtras(Game& g, Renderer* R);
void homeLights(Game& g, Renderer* R);
void homeWater(Game& g, Renderer* R);
static void drawNPC(Renderer* R, int who, V3 pos, float yaw, float t) {
    M4 b = M4::translate(pos) * M4::rotY(yaw);
    float bob = std::sin(t * 2 + who) * 0.03f;
    switch (who) {
        case 3: case 7: {  // robots: Sprocket (teal) and Rusty (rust)
            V3 col = who == 3 ? hexColor(0x40a0b0) : hexColor(0xa05a30);
            R->drawPart(PM_CUBE, b * M4::translate(V3(0, 0.5f, 0)) * M4::scale(V3(0.8f, 1.0f, 0.6f)), T_METAL, col);
            R->drawPart(PM_CUBE, b * M4::translate(V3(0, 1.35f + bob, 0)) * M4::scale(V3(0.9f, 0.7f, 0.7f)), T_METAL, col * 1.2f);
            R->drawPart(PM_CUBE, b * M4::translate(V3(0, 1.38f + bob, 0.36f)) * M4::scale(V3(0.6f, 0.25f, 0.02f)), T_WHITE, who == 3 ? hexColor(0x40f0ff) : hexColor(0xffa040), 2.5f);
            R->drawPart(PM_CYL, b * M4::translate(V3(0, 1.95f + bob, 0)) * M4::scale(V3(0.06f, 0.5f, 0.06f)), T_METAL, V3(1, 1, 1));
            R->drawPart(PM_SPHERE, b * M4::translate(V3(0, 2.25f + bob, 0)) * M4::scale(V3(0.18f, 0.18f, 0.18f)), T_WHITE, hexColor(0xff4040), 2);
            for (int s = -1; s <= 1; s += 2) R->drawPart(PM_CUBE, b * M4::translate(V3(0.55f * s, 0.65f, 0.1f)) * M4::rotX(std::sin(t * 3 + s) * 0.3f) * M4::scale(V3(0.15f, 0.7f, 0.15f)), T_METAL, col);
        } break;
        case 9: break;  // the blueprint table is level geometry
        case 11: case 12: case 13: case 14: case 15: case 16: drawNPCExtra(R, who, pos, yaw, t); break;
        case 10: {  // Bramble the beaver
            R->drawPart(PM_SPHERE, b * M4::translate(V3(0, 0.75f, 0)) * M4::scale(V3(1.1f, 1.4f, 1.0f)), T_DIRT, hexColor(0x8a5a30));
            R->drawPart(PM_SPHERE, b * M4::translate(V3(0, 1.65f + bob, 0.1f)) * M4::scale(V3(0.8f, 0.7f, 0.8f)), T_DIRT, hexColor(0x9a6a40));
            R->drawPart(PM_CUBE, b * M4::translate(V3(0, 1.45f + bob, 0.48f)) * M4::scale(V3(0.22f, 0.18f, 0.05f)), T_WHITE, V3(1, 1, 0.9f));
            for (int s = -1; s <= 1; s += 2) {
                R->drawPart(PM_CUBE, b * M4::translate(V3(0.16f * s, 1.75f + bob, 0.45f)) * M4::scale(V3(0.08f, 0.08f, 0.03f)), T_WHITE, hexColor(0x101010));
                R->drawPart(PM_SPHERE, b * M4::translate(V3(0.33f * s, 2.0f + bob, 0.1f)) * M4::scale(V3(0.18f, 0.18f, 0.1f)), T_DIRT, hexColor(0x6a4020));
            }
            R->drawPart(PM_CUBE, b * M4::translate(V3(0, 0.2f, -0.8f)) * M4::rotX(0.3f) * M4::scale(V3(0.8f, 0.12f, 1.0f)), T_TAR, hexColor(0x5a4a40));
            R->drawPart(PM_CYL, b * M4::translate(V3(0, 2.15f + bob, 0)) * M4::scale(V3(0.9f, 0.1f, 0.9f)), T_CLOTH, hexColor(0xd0a040));
        } break;
        case 8: {  // Pip the ghost kid
            float a = 0.55f + 0.15f * std::sin(t * 2);
            R->drawPart(PM_CONE, b * M4::translate(V3(0, 0.9f + bob * 4, 0)) * M4::scale(V3(0.9f, 1.4f, 0.9f)), T_WHITE, hexColor(0xd8e8ff), 0.6f, 0, a);
            R->drawPart(PM_SPHERE, b * M4::translate(V3(0, 1.7f + bob * 4, 0)) * M4::scale(V3(0.7f, 0.7f, 0.7f)), T_WHITE, hexColor(0xe8f0ff), 0.6f, 0, a);
            for (int s = -1; s <= 1; s += 2) R->drawPart(PM_CUBE, b * M4::translate(V3(0.13f * s, 1.75f + bob * 4, 0.33f)) * M4::scale(V3(0.08f, 0.14f, 0.02f)), T_WHITE, hexColor(0x202040), 0, 0, 1);
        } break;
        default: {  // human folk: Gil (yellow raincoat), Tilda (green apron), Mort (black coat)
            V3 coat = who == 4 ? hexColor(0xd0b030) : who == 5 ? hexColor(0x3a7a50) : hexColor(0x2a2a30);
            R->drawPart(PM_CONE, b * M4::translate(V3(0, 0.7f, 0)) * M4::scale(V3(0.9f, 1.4f, 0.8f)), T_CLOTH, coat);
            R->drawPart(PM_CYL, b * M4::translate(V3(0, 1.2f + bob, 0)) * M4::scale(V3(0.55f, 0.6f, 0.45f)), T_CLOTH, coat * 0.9f);
            R->drawPart(PM_SPHERE, b * M4::translate(V3(0, 1.72f + bob, 0)) * M4::scale(V3(0.44f, 0.48f, 0.44f)), T_WHITE, hexColor(0xd8a888));
            for (int s = -1; s <= 1; s += 2) R->drawPart(PM_CUBE, b * M4::translate(V3(0.1f * s, 1.76f + bob, 0.21f)) * M4::scale(V3(0.07f, 0.05f, 0.03f)), T_WHITE, hexColor(0x101010));
            if (who == 4) R->drawPart(PM_CONE, b * M4::translate(V3(0, 2.0f + bob, -0.05f)) * M4::scale(V3(0.8f, 0.35f, 0.8f)), T_CLOTH, coat);
            if (who == 5) { R->drawPart(PM_CUBE, b * M4::translate(V3(0, 1.78f + bob, 0.24f)) * M4::scale(V3(0.32f, 0.08f, 0.04f)), T_BRASS, V3(1, 1, 1), 0.5f); R->drawPart(PM_CYL, b * M4::translate(V3(0, 2.0f + bob, 0)) * M4::scale(V3(0.5f, 0.12f, 0.5f)), T_CLOTH, hexColor(0x6a3a20)); }
            if (who == 6) { R->drawPart(PM_CYL, b * M4::translate(V3(0, 2.1f + bob, 0)) * M4::scale(V3(0.45f, 0.45f, 0.45f)), T_CLOTH, hexColor(0x18181c)); R->drawPart(PM_CUBE, b * M4::translate(V3(0.55f, 0.9f, 0.2f)) * M4::scale(V3(0.08f, 1.8f, 0.08f)), T_DARKWOOD, V3(1, 1, 1)); R->drawPart(PM_CUBE, b * M4::translate(V3(0.55f, 0.1f, 0.35f)) * M4::scale(V3(0.4f, 0.1f, 0.3f)), T_METAL, V3(1, 1, 1)); }
        } break;
    }
}

static void drawEnemy(Game& g, Renderer* R, const Enemy& e) {
    float t = e.anim;
    float fl = e.flash > 0 ? 0.7f : 0;
    M4 b = M4::translate(e.pos) * M4::rotY(e.yaw);
    V3 eyeCol = hexColor(0xd8b0ff);
    switch (e.type) {
        case E_GLOOMLING: {
            float sq = e.state == 2 ? 0.7f : (e.grounded ? 1.f + std::sin(t * 12) * 0.06f : 1.15f);
            float w = e.state == 2 ? 1.25f : 1.f / std::sqrt(sq);
            R->drawPart(PM_SPHERE, b * M4::translate(V3(0, 0.42f * sq, 0)) * M4::scale(V3(0.95f * w, 0.85f * sq, 0.95f * w)), T_SHADOW, V3(1, 1, 1), 0, fl);
            for (int s = -1; s <= 1; s += 2) {
                R->drawPart(PM_CUBE, b * M4::translate(V3(0.18f * s, 0.55f * sq, 0.4f)) * M4::scale(V3(0.12f, e.state == 2 ? 0.06f : 0.14f, 0.05f)), T_WHITE, e.state == 2 ? hexColor(0xff6040) : eyeCol, 2.0f);
                R->drawPart(PM_CONE, b * M4::translate(V3(0.25f * s, 0.85f * sq, 0)) * M4::rotZ(-0.5f * s) * M4::scale(V3(0.15f, 0.35f, 0.15f)), T_SHADOW, V3(1, 1, 1), 0, fl);
            }
        } break;
        case E_TARBELL: {
            bool closed = e.state == 3;
            float swell = e.state == 1 ? 1 + std::min(e.t / 0.7f, 1.f) * 0.25f : 1;
            R->drawPart(PM_CONE, b * M4::translate(V3(0, 0.55f, 0)) * M4::scale(V3(1.5f, 1.1f, 1.5f)), T_BARNACLE, V3(1, 1, 1), 0, fl);
            if (!closed) {
                R->drawPart(PM_SPHERE, b * M4::translate(V3(0, 1.05f, 0.1f)) * M4::scale(V3(0.7f, 0.6f, 0.7f) * swell), T_TAR, V3(1, 1, 1), 0, fl);
                R->drawPart(PM_CYL, b * M4::translate(V3(0, 1.2f, 0.42f)) * M4::rotX(1.2f) * M4::scale(V3(0.3f, 0.25f, 0.3f)), T_WHITE, e.state == 1 ? hexColor(0xff8040) : hexColor(0x5a2a6a), e.state == 1 ? 2.f : 0.5f);
            }
            R->drawPart(PM_SPHERE, b * M4::translate(V3(0, closed ? 1.0f : 1.4f, closed ? 0 : -0.3f)) * M4::rotX(closed ? 0 : -0.7f) * M4::scale(V3(1.1f, 0.5f, 1.1f)), T_BARNACLE, V3(0.9f, 0.95f, 1), 0, fl);
        } break;
        case E_MOTH: {
            float flap = std::sin(t * (e.state == 2 ? 40 : 22)) * 0.7f;
            M4 mb = M4::translate(e.pos) * M4::rotY(e.yaw);
            R->drawPart(PM_SPHERE, mb * M4::scale(V3(0.28f, 0.28f, 0.55f)), T_BONE, hexColor(0xd8d0e8), 0, fl);
            for (int s = -1; s <= 1; s += 2)
                R->drawPart(PM_CUBE, mb * M4::rotZ(flap * s) * M4::translate(V3(0.42f * s, 0, 0)) * M4::scale(V3(0.75f, 0.03f, 0.55f)), T_EYE, hexColor(0xc8b8e0), 0.2f, fl);
            R->drawPart(PM_CUBE, mb * M4::translate(V3(0, 0.05f, 0.28f)) * M4::scale(V3(0.18f, 0.06f, 0.04f)), T_WHITE, hexColor(0xff60a0), 2);
        } break;
        case E_KNIGHT: {
            bool wind = e.state == 2 || e.state == 4;
            float step = (e.state == 1 || e.state == 5) ? std::sin(t * (e.state == 5 ? 16 : 7)) : 0;
            V3 armor = hexColor(0x3a3848);
            for (int s = -1; s <= 1; s += 2)
                R->drawPart(PM_CUBE, b * M4::translate(V3(0.22f * s, 0.45f, 0)) * M4::rotX(step * 0.5f * s) * M4::scale(V3(0.26f, 0.9f, 0.3f)), T_METAL, armor, 0, fl);
            R->drawPart(PM_CUBE, b * M4::translate(V3(0, 1.25f, 0)) * M4::scale(V3(0.8f, 0.8f, 0.5f)), T_METAL, armor, 0, fl);
            R->drawPart(PM_CONE, b * M4::translate(V3(0, 1.1f, -0.1f)) * M4::scale(V3(0.95f, 0.9f, 0.7f)), T_CLOTH, hexColor(0x40203a), 0, fl);
            R->drawPart(PM_CUBE, b * M4::translate(V3(0, 1.85f, 0)) * M4::scale(V3(0.5f, 0.5f, 0.5f)), T_METAL, armor * 1.2f, 0, fl);
            R->drawPart(PM_CUBE, b * M4::translate(V3(0, 1.88f, 0.26f)) * M4::scale(V3(0.36f, 0.07f, 0.02f)), T_WHITE, wind ? hexColor(0xff5040) : hexColor(0xb070ff), 2.5f);
            R->drawPart(PM_CONE, b * M4::translate(V3(0, 2.25f, 0)) * M4::scale(V3(0.18f, 0.4f, 0.18f)), T_METAL, armor, 0, fl);
            if (e.shield)
                R->drawPart(PM_CUBE, b * M4::translate(V3(-0.15f, 1.15f, 0.48f)) * M4::scale(V3(0.85f, 1.25f, 0.1f)), T_RUNE, hexColor(0x8a8aa0), 0, fl);
            float swing = e.state == 2 ? -2.4f * std::min(1.f, e.t / 0.5f) : (e.state == 3 ? -2.4f + std::min(1.f, e.t / 0.15f) * 3.4f : -0.3f);
            R->drawPart(PM_CUBE, b * M4::translate(V3(0.5f, 1.35f, 0.1f)) * M4::rotX(swing) * M4::translate(V3(0, 0.75f, 0)) * M4::scale(V3(0.1f, 1.5f, 0.18f)), T_METAL, hexColor(0x8080a0), wind ? 0.6f : 0, fl);
        } break;
        case E_RAT: {
            float run = std::sin(t * 25) * 0.1f;
            R->drawPart(PM_SPHERE, b * M4::translate(V3(0, 0.25f + std::fabs(run) * 0.5f, 0)) * M4::scale(V3(0.45f, 0.38f, 0.8f)), T_PLASTER, hexColor(0x8a8090), 0, fl);
            R->drawPart(PM_SPHERE, b * M4::translate(V3(0, 0.32f, 0.42f)) * M4::scale(V3(0.32f, 0.3f, 0.36f)), T_PLASTER, hexColor(0x9a90a0), 0, fl);
            for (int s2 = -1; s2 <= 1; s2 += 2) {
                R->drawPart(PM_CYL, b * M4::translate(V3(0.13f * s2, 0.52f, 0.36f)) * M4::rotX(PI / 2) * M4::scale(V3(0.2f, 0.04f, 0.2f)), T_WHITE, hexColor(0xe0a0b0), 0, fl);
                R->drawPart(PM_CUBE, b * M4::translate(V3(0.08f * s2, 0.38f, 0.58f)) * M4::scale(V3(0.05f, 0.05f, 0.02f)), T_WHITE, hexColor(0xff3030), 2.5f);
            }
            R->drawPart(PM_CUBE, b * M4::translate(V3(0, 0.22f, -0.6f)) * M4::rotY(run * 3) * M4::scale(V3(0.05f, 0.05f, 0.6f)), T_WHITE, hexColor(0xe0a0b0), 0, fl);
        } break;
        case E_GLOOMKING: {
            float sq = e.state == 2 ? 0.75f : (e.grounded ? 1.f + std::sin(t * 6) * 0.05f : 1.12f);
            R->drawPart(PM_SPHERE, b * M4::translate(V3(0, 1.2f * sq, 0)) * M4::scale(V3(2.6f, 2.4f * sq, 2.6f)), T_SHADOW, V3(1, 1, 1), 0, fl);
            for (int s2 = -1; s2 <= 1; s2 += 2) R->drawPart(PM_CUBE, b * M4::translate(V3(0.5f * s2, 1.5f * sq, 1.15f)) * M4::scale(V3(0.32f, e.state == 2 ? 0.12f : 0.35f, 0.08f)), T_WHITE, e.state == 2 ? hexColor(0xff6040) : hexColor(0xd8b0ff), 2.0f);
            for (int k = 0; k < 5; k++) {
                float a = k * TAU / 5;
                R->drawPart(PM_CONE, b * M4::translate(V3(std::sin(a) * 0.7f, 2.55f * sq, std::cos(a) * 0.7f)) * M4::scale(V3(0.35f, 0.7f, 0.35f)), T_GOLD, V3(1, 1, 1), 0.4f, fl);
            }
            R->drawPart(PM_CYL, b * M4::translate(V3(0, 2.3f * sq, 0)) * M4::scale(V3(1.7f, 0.25f, 1.7f)), T_GOLD, V3(1, 1, 1), 0.3f, fl);
        } break;
        case E_MEGABOT: {
            float step = e.state == 1 ? std::sin(t * 4) : 0;
            float stomp = e.state == 2 ? std::min(1.f, e.t / 0.9f) : 0;
            V3 body = hexColor(0x6a7080);
            for (int s2 = -1; s2 <= 1; s2 += 2) {
                float lift = (s2 > 0 ? stomp : 0) * 1.0f;
                R->drawPart(PM_CUBE, b * M4::translate(V3(0.8f * s2, 0.9f + lift, 0)) * M4::rotX(step * 0.35f * s2) * M4::scale(V3(0.7f, 1.8f, 0.9f)), T_METAL, body * 0.8f, 0, fl);
            }
            R->drawPart(PM_CUBE, b * M4::translate(V3(0, 2.6f, 0)) * M4::scale(V3(2.6f, 1.8f, 1.8f)), T_METAL, body, 0, fl);
            R->drawPart(PM_CUBE, b * M4::translate(V3(0, 2.6f, 0.92f)) * M4::scale(V3(2.7f, 1.9f, 0.15f)), T_RUNE, hexColor(0xa0a8c0), 0, fl);  // front armour
            R->drawPart(PM_CUBE, b * M4::translate(V3(0, 3.8f, 0.2f)) * M4::scale(V3(1.2f, 0.8f, 1.0f)), T_METAL, body * 1.1f, 0, fl);
            R->drawPart(PM_CUBE, b * M4::translate(V3(0, 3.85f, 0.71f)) * M4::scale(V3(0.9f, 0.18f, 0.04f)), T_WHITE, e.state >= 2 ? hexColor(0xff4040) : hexColor(0x40f0ff), 3);
            for (int k = 0; k < 3; k++) {  // glowing vents on the back: the weak spot
                float pulse = 0.6f + 0.4f * std::sin(t * 6 + k);
                R->drawPart(PM_CUBE, b * M4::translate(V3(-0.7f + k * 0.7f, 2.7f, -0.93f)) * M4::scale(V3(0.45f, 1.1f, 0.06f)), T_WHITE, hexColor(0xff8a20) * pulse, 3, 0);
            }
            for (int s2 = -1; s2 <= 1; s2 += 2)
                R->drawPart(PM_CYL, b * M4::translate(V3(1.6f * s2, 3.0f, 0.3f)) * M4::rotX(PI / 2) * M4::scale(V3(0.6f, 1.2f, 0.6f)), T_METAL, body * 0.9f, 0, fl);
            glow(R, e.pos + V3(0, 2.7f, 0) - V3(std::sin(e.yaw), 0, std::cos(e.yaw)) * 1.0f, hexColor(0xff8a20), 1.3f, 0.4f);
        } break;
        case E_WRAITH: {
            float a = clampf(0.12f + e.reveal * 0.88f, 0, 1);
            if (e.state == 2) a = std::max(a, 0.5f + 0.5f * std::sin(e.t * 30));
            float sway = std::sin(t * 2) * 0.1f;
            R->drawPart(PM_CONE, b * M4::translate(V3(0, 0.9f, 0)) * M4::rotZ(sway) * M4::scale(V3(1.0f, 1.8f, 1.0f)), T_SHADOW, V3(1, 1, 1), 0, fl, a);
            R->drawPart(PM_SPHERE, b * M4::translate(V3(0, 1.6f, 0.05f)) * M4::scale(V3(0.55f, 0.6f, 0.55f)), T_SHADOW, V3(1, 1, 1), 0, fl, a);
            for (int s = -1; s <= 1; s += 2) {
                R->drawPart(PM_CUBE, b * M4::translate(V3(0.12f * s, 1.62f, 0.27f)) * M4::scale(V3(0.08f, 0.12f, 0.03f)), T_WHITE, hexColor(0xe0f0ff), 3, 0, std::max(a, 0.35f));
                float claw = e.state == 2 ? -1.6f : -0.3f;
                R->drawPart(PM_CUBE, b * M4::translate(V3(0.45f * s, 1.3f, 0.1f)) * M4::rotX(claw) * M4::translate(V3(0, -0.4f, 0)) * M4::scale(V3(0.12f, 0.8f, 0.12f)), T_SHADOW, V3(1, 1, 1), 0, fl, a);
            }
        } break;
        default: break;
    }
    if (e.type >= E_VIRUS) drawEnemyExtra(g, R, e);
    if (e.type == E_WRAITH && e.reveal < 0.3f) return;
    // small health pips above damaged enemies
    (void)g;
}

static void drawRing(Renderer* R, V3 c, float r, V3 col, float emis, float thick = 0.18f, int segs = 48) {
    for (int i = 0; i < segs; i++) {
        float a = TAU * i / segs;
        V3 p = c + V3(std::cos(a) * r, 0.12f, std::sin(a) * r);
        R->drawPart(PM_CUBE, TRS(p, -a, V3(thick, 0.22f, TAU * r / segs * 1.05f)), T_WHITE, col, emis);
    }
}

static void drawBoss(Game& g, Renderer* R) {
    Boss& b = g.boss;
    V3 C = g.lv.boss.center;
    float t = g.time;
    float fl = b.flash > 0 ? 0.6f : 0;
    float dissolve = b.dead ? std::max(0.f, 1 - b.deathT / 3.f) : 1;
    switch (b.type) {
        case B_BELLWETHER: {
            float lift = b.f[0], tilt = b.f[1];
            M4 m = M4::translate(C + V3(0, lift, 0)) * M4::rotY(b.yaw) * M4::rotX(tilt);
            // tentacles
            for (int i = 0; i < 8; i++) {
                float a = i * TAU / 8;
                float sw = std::sin(t * 2 + i) * 0.3f;
                R->drawPart(PM_CYL, M4::translate(C + V3(std::cos(a) * 2.8f, 0.4f, std::sin(a) * 2.8f)) * M4::rotZ(std::cos(a) * (0.9f + sw)) * M4::rotX(-std::sin(a) * (0.9f + sw)) * M4::scale(V3(0.5f, 2.2f, 0.5f)), T_TAR, V3(1, 1, 1), 0, fl, dissolve);
            }
            R->drawPart(PM_CYL, m * M4::translate(V3(0, 0.4f, 0)) * M4::scale(V3(6.4f, 0.8f, 6.4f)), T_BRASS, hexColor(0x6a8a80), 0, fl, dissolve);
            R->drawPart(PM_CONE, m * M4::translate(V3(0, 2.6f, 0)) * M4::scale(V3(5.8f, 4.4f, 5.8f)), T_BARNACLE, V3(1, 1, 1), 0, fl, dissolve);
            R->drawPart(PM_SPHERE, m * M4::translate(V3(0, 4.4f, 0)) * M4::scale(V3(2.6f, 2.0f, 2.6f)), T_BARNACLE, V3(0.9f, 1, 1), 0, fl, dissolve);
            R->drawPart(PM_CUBE, m * M4::translate(V3(0, 5.6f, 0)) * M4::scale(V3(0.6f, 0.9f, 0.6f)), T_BRASS, V3(1, 1, 1), 0, fl, dissolve);
            // eyes
            for (int i = 0; i < 3; i++) {
                float a = (i - 1) * 0.5f;
                R->drawPart(PM_SPHERE, m * M4::translate(V3(std::sin(a) * 2.2f, 3.9f + (i == 1) * 0.4f, std::cos(a) * 2.2f)) * M4::scale(V3(0.5f, 0.5f, 0.5f)), T_EYE, V3(1, 1, 1), 0.5f, fl, dissolve);
            }
            // the core (clapper)
            bool open = !b.wps.empty() && b.wps[0].vulnerable;
            float pulse = 0.8f + 0.2f * std::sin(t * 8);
            R->drawPart(PM_SPHERE, M4::translate(b.v[0]) * M4::scale(V3(1.4f, 1.4f, 1.4f) * pulse), T_FLESHGLOW, open ? V3(1, 1, 1) : V3(0.4f, 0.3f, 0.3f), open ? 1.5f : 0.2f, fl, dissolve);
            if (open) glow(R, b.v[0], hexColor(0xff8040), 2.2f, 0.8f);
            if (b.state == 2 && b.stateT < 0.6f) glow(R, C + V3(0, 4.2f, 0) + V3(std::sin(b.yaw), 0, std::cos(b.yaw)) * 1.5f, hexColor(0xa040ff), 1.5f);
        } break;
        case B_WARDEN: {
            // the hacker's terminal, servers and firewall
            extern V3 hackServerPos(const V3& C, int k);
            V3 green = hexColor(0x40ff80);
            bool firewall = b.v[0].x > 0.5f;
            // central terminal
            R->drawPart(PM_CUBE, M4::translate(C + V3(0, 0.6f, 0)) * M4::scale(V3(3.0f, 1.2f, 2.0f)), T_METAL, hexColor(0x30343c));
            R->drawPart(PM_CUBE, M4::translate(C + V3(0, 1.9f, -0.6f)) * M4::rotX(-0.2f) * M4::scale(V3(2.8f, 1.6f, 0.15f)), T_WHITE, green * 0.6f, 1.5f);
            for (int k = 0; k < 3; k++) {
                V3 p = hackServerPos(C, k);
                bool alive = b.f[k] > 0;
                V3 body = alive ? hexColor(0x22262e) : hexColor(0x140c0a);
                R->drawPart(PM_CUBE, M4::translate(p + V3(0, 2.0f, 0)) * M4::scale(V3(1.8f, 4.0f, 1.8f)), T_METAL, body);
                for (int r = 0; r < 6; r++) {
                    bool blink = std::fmod(t * 3 + r * 0.7f + k, 1.f) < 0.6f;
                    V3 c = alive ? (blink ? green : green * 0.3f) : hexColor(0xff5020) * (blink ? 1.f : 0.4f);
                    R->drawPart(PM_CUBE, M4::translate(p + V3(0, 0.6f + r * 0.6f, 0.91f)) * M4::scale(V3(1.3f, 0.12f, 0.03f)), T_WHITE, c, 2.0f);
                    R->drawPart(PM_CUBE, M4::translate(p + V3(0, 0.6f + r * 0.6f, -0.91f)) * M4::scale(V3(1.3f, 0.12f, 0.03f)), T_WHITE, c, 2.0f);
                }
                if (alive) {
                    // health pips + uplink beam to the firewall
                    for (int h = 0; h < 2; h++) R->drawPart(PM_CUBE, M4::translate(p + V3(-0.3f + h * 0.6f, 4.4f, 0)) * M4::scale(V3(0.4f, 0.2f, 0.4f)), T_WHITE, h < b.f[k] ? green : hexColor(0x303030), 2);
                    if (firewall) {
                        V3 a = p + V3(0, 4, 0), z = b.pos;
                        for (int s2 = 0; s2 < 10; s2++) { V3 q = lerp(a, z, (s2 + std::fmod(t * 2, 1.f)) / 10.f); R->drawPart(PM_CUBE, M4::translate(q) * M4::scale(V3(0.15f, 0.15f, 0.15f)), T_WHITE, green, 2.5f, 0, 0.8f); }
                    }
                    glow(R, p + V3(0, 2.2f, 0), green, 1.6f, 0.4f);
                } else if (!R->shadowPass && std::fmod(t, 0.2f) < 0.03f) g.burst(p + V3(0, 4, 0), hexColor(0x3a3030), 2, 1, 0.3f, 2, 1.0f, -2);
            }
            // GL1TCH himself: hoodie, glowing screen-face, floating laptop, headset
            V3 P = b.pos;
            float yaw = std::atan2(g.pl.pos.x - P.x, g.pl.pos.z - P.z);
            M4 m = M4::translate(P) * M4::rotY(yaw);
            float bob = firewall ? 0 : std::sin(t * 20) * 0.03f;
            R->drawPart(PM_CONE, m * M4::translate(V3(0, -0.2f, 0)) * M4::scale(V3(1.4f, 1.8f, 1.1f)), T_CLOTH, hexColor(0x2a2a3a), 0, fl, dissolve);
            R->drawPart(PM_SPHERE, m * M4::translate(V3(0, 0.95f + bob, 0)) * M4::scale(V3(0.9f, 0.95f, 0.9f)), T_CLOTH, hexColor(0x22222e), 0, fl, dissolve);
            R->drawPart(PM_CUBE, m * M4::translate(V3(0, 0.95f + bob, 0.36f)) * M4::scale(V3(0.55f, 0.45f, 0.12f)), T_WHITE, firewall ? green : hexColor(0xff4040), 2.5f, fl, dissolve);
            for (int e = -1; e <= 1; e += 2) R->drawPart(PM_CUBE, m * M4::translate(V3(0.12f * e, 1.0f + bob, 0.43f)) * M4::scale(V3(0.09f, firewall ? 0.09f : 0.03f, 0.02f)), T_WHITE, hexColor(0x081008), 0, 0, dissolve);
            R->drawPart(PM_CUBE, m * M4::translate(V3(0, 1.4f, 0)) * M4::scale(V3(1.0f, 0.1f, 0.15f)), T_METAL, V3(1, 1, 1), 0, 0, dissolve);
            for (int e = -1; e <= 1; e += 2) R->drawPart(PM_CUBE, m * M4::translate(V3(0.48f * e, 0.95f, 0)) * M4::scale(V3(0.12f, 0.3f, 0.3f)), T_METAL, V3(1, 1, 1), 0, 0, dissolve);
            R->drawPart(PM_CUBE, m * M4::translate(V3(0, 0.35f, 0.75f)) * M4::rotX(0.3f) * M4::scale(V3(0.9f, 0.05f, 0.6f)), T_METAL, hexColor(0x404048), 0, 0, dissolve);
            R->drawPart(PM_CUBE, m * M4::translate(V3(0, 0.62f, 0.5f)) * M4::rotX(-0.9f) * M4::scale(V3(0.9f, 0.6f, 0.04f)), T_WHITE, green * 0.8f, 1.8f, 0, dissolve);
            // firewall dome
            if (firewall) {
                R->drawPart(PM_SPHERE, M4::translate(P + V3(0, 0.6f, 0)) * M4::rotY(t * 0.5f) * M4::scale(V3(4.2f, 4.2f, 4.2f)), T_GRATE, green, 1.2f, 0, 0.35f);
                glow(R, P + V3(0, 0.6f, 0), green, 3.0f, 0.25f);
            } else {
                glow(R, P + V3(0, 1, 0), hexColor(0xff4040), 1.8f, 0.6f);
                if (!R->shadowPass && std::fmod(t, 0.15f) < 0.03f) g.burst(P + V3(0, 1.2f, 0), hexColor(0x60ff90), 3, 3, 0.05f, 3, 0.3f, 3);
            }
        } break;
        case B_MATRON: {
            V3 P = b.pos;
            float flap = b.state == 4 ? std::sin(t * 3) * 0.15f - 0.4f : std::sin(t * 6) * 0.6f;
            if (!R->shadowPass) {
                V3 vel = P - b.v[7];
                b.v[7] = P;
                float yaw = lenXZ(vel) > 0.001f ? std::atan2(vel.x, vel.z) : b.yaw;
                b.yaw = angleApproach(b.yaw, yaw, 0.1f);
            }
            M4 m = M4::translate(P) * M4::rotY(b.yaw);
            R->drawPart(PM_SPHERE, m * M4::scale(V3(1.4f, 1.4f, 3.0f)), T_BONE, hexColor(0xc8c0d8), 0, fl, dissolve);
            R->drawPart(PM_SPHERE, m * M4::translate(V3(0, -0.1f, -1.9f)) * M4::scale(V3(1.3f, 1.2f, 2.0f)), T_FLESHGLOW, b.state == 4 ? V3(1, 1, 1) : V3(0.6f, 0.5f, 0.6f), b.state == 4 ? 1.3f : 0.4f, fl, dissolve);
            R->drawPart(PM_SPHERE, m * M4::translate(V3(0, 0.2f, 1.6f)) * M4::scale(V3(1.0f, 1.0f, 0.9f)), T_BONE, hexColor(0xb0a8c0), 0, fl, dissolve);
            for (int s = -1; s <= 1; s += 2) {
                R->drawPart(PM_CUBE, m * M4::rotZ(flap * s) * M4::translate(V3(3.4f * s, 0.2f, 0.3f)) * M4::scale(V3(6.0f, 0.12f, 4.2f)), T_EYE, hexColor(0xd8c8f0), 0.25f, fl, dissolve);
                R->drawPart(PM_CUBE, m * M4::rotZ(flap * s * 0.7f) * M4::translate(V3(2.4f * s, 0.1f, -2.0f)) * M4::scale(V3(3.6f, 0.1f, 2.6f)), T_BONE, hexColor(0xb8a8d0), 0, fl, dissolve);
                R->drawPart(PM_CUBE, m * M4::translate(V3(0.35f * s, 0.9f, 2.4f)) * M4::rotX(-0.8f) * M4::rotZ(0.4f * s) * M4::scale(V3(0.08f, 1.6f, 0.08f)), T_BONE, V3(1, 1, 1), 0, 0, dissolve);
                R->drawPart(PM_SPHERE, m * M4::translate(V3(0.45f * s, 0.35f, 2.35f)) * M4::scale(V3(0.4f, 0.4f, 0.4f)), T_WHITE, hexColor(0xff60b0), 2, 0, dissolve);
            }
            if (b.state == 4) glow(R, P + V3(0, 0.5f, 0), hexColor(0xffa060), 3, 0.6f);
            if (b.state == 2) for (int k = 0; k < 2; k++) glow(R, P + V3(std::sin(t * 9 + k) * 2, -2, std::cos(t * 7 + k) * 2), hexColor(0xd8d0c0), 0.8f, 0.4f);
        } break;
        case B_RATKING: {
            float open = b.f[0];
            M4 m = M4::translate(C) * M4::rotY(b.yaw);
            // junk tank base with treads
            R->drawPart(PM_CYL, m * M4::translate(V3(0, 1.5f, 0)) * M4::scale(V3(5.4f, 3.0f, 5.4f)), T_RUST, V3(1, 1, 1), 0, fl, dissolve);
            for (int s2 = -1; s2 <= 1; s2 += 2) R->drawPart(PM_CUBE, m * M4::translate(V3(2.8f * s2, 0.6f, 0)) * M4::scale(V3(1.0f, 1.2f, 6.0f)), T_TAR, V3(1, 1, 1), 0, fl, dissolve);
            R->drawPart(PM_CYL, m * M4::translate(V3(0, 3.3f, 0)) * M4::scale(V3(3.4f, 0.6f, 3.4f)), T_METAL, V3(1, 1, 1), 0, fl, dissolve);
            for (int k = 0; k < 6; k++) R->drawPart(PM_CUBE, m * M4::rotY(k) * M4::translate(V3(2.6f, 2.0f + (k % 2), 0)) * M4::scale(V3(0.8f, 0.6f, 1.0f)), k % 2 ? T_RUST : T_METAL, V3(1, 1, 1), 0, 0, dissolve);
            // THE RAT, wearing the Beacon-Heart as a crown
            V3 rp = C + V3(0, 4.6f + open * 0.5f, 0);
            M4 rm = M4::translate(rp) * M4::rotY(b.yaw);
            R->drawPart(PM_SPHERE, rm * M4::scale(V3(1.6f, 1.5f, 1.9f)), T_PLASTER, hexColor(0x8a8090), 0, fl, dissolve);
            R->drawPart(PM_SPHERE, rm * M4::translate(V3(0, 0.5f, 0.9f)) * M4::scale(V3(1.1f, 1.0f, 1.2f)), T_PLASTER, hexColor(0x9a90a0), 0, fl, dissolve);
            for (int s2 = -1; s2 <= 1; s2 += 2) {
                R->drawPart(PM_CYL, rm * M4::translate(V3(0.45f * s2, 1.15f, 0.8f)) * M4::rotX(PI / 2) * M4::scale(V3(0.7f, 0.1f, 0.7f)), T_WHITE, hexColor(0xe0a0b0), 0, fl, dissolve);
                R->drawPart(PM_CUBE, rm * M4::translate(V3(0.25f * s2, 0.65f, 1.45f)) * M4::scale(V3(0.15f, 0.15f, 0.05f)), T_WHITE, hexColor(0xff3030), 2.5f, 0, dissolve);
            }
            for (int k = 0; k < 4; k++) R->drawPart(PM_CONE, rm * M4::translate(V3(-0.45f + k * 0.3f, 1.35f, 0.6f)) * M4::scale(V3(0.2f, 0.45f, 0.2f)), T_GOLD, V3(1, 1, 1), 0.6f, 0, dissolve);
            if (!g.has("boss.4")) R->drawPart(PM_SPHERE, rm * M4::translate(V3(0, 1.45f, 0.6f)) * M4::scale(V3(0.35f, 0.35f, 0.35f)), T_FLESHGLOW, hexColor(0xffe0a0), 1.6f, 0, dissolve);
            // glass hatch (lifts when the claw is stuck)
            R->drawPart(PM_SPHERE, M4::translate(C + V3(0, 4.8f + open * 2.6f, 0)) * M4::scale(V3(3.6f, 2.6f, 3.6f)), T_WINDOW, hexColor(0x80c0e0), 0.3f, 0, open > 0.6f ? 0.25f : 0.5f);
            if (open > 0.6f) glow(R, rp + V3(0, 0.6f, 0), hexColor(0xffa040), 2.0f, 0.6f);
            // crane arm and claw
            V3 claw = b.v[0];
            V3 base = C + V3(0, 3.6f, 0);
            V3 elbow = (base + claw) * 0.5f + V3(0, 3.5f, 0);
            for (int seg = 0; seg < 2; seg++) {
                V3 a0 = seg ? elbow : base, a1 = seg ? claw + V3(0, 0.8f, 0) : elbow;
                for (int k = 0; k <= 8; k++) R->drawPart(PM_CUBE, M4::translate(lerp(a0, a1, k / 8.f)) * M4::scale(V3(0.45f, 0.45f, 0.45f)), T_RUST, V3(1, 1, 1), 0, 0, dissolve);
            }
            R->drawPart(PM_CYL, M4::translate(claw + V3(0, 0.6f, 0)) * M4::scale(V3(1.2f, 0.5f, 1.2f)), T_METAL, V3(1, 1, 1), 0, fl, dissolve);
            for (int k = 0; k < 3; k++) {
                float a = k * TAU / 3 + t * 0.3f;
                R->drawPart(PM_CUBE, M4::translate(claw + V3(std::cos(a) * 0.6f, -0.1f, std::sin(a) * 0.6f)) * M4::rotY(-a) * M4::rotZ(0.4f) * M4::scale(V3(0.18f, 1.4f, 0.25f)), T_METAL, V3(0.8f, 0.8f, 0.85f), 0, fl, dissolve);
            }
        } break;
        case B_HUSH: {
            V3 P = b.pos;
            float a = dissolve;
            // body rising behind the lamp
            R->drawPart(PM_CONE, M4::translate(C + V3(0, 3, 11)) * M4::scale(V3(16, 14, 10)), T_SHADOW, V3(1, 1, 1), 0, fl, a);
            R->drawPart(PM_SPHERE, M4::translate(P + V3(0, 4, 0)) * M4::scale(V3(9, 8, 7)), T_SHADOW, V3(1, 1, 1), 0, fl, a);
            R->drawPart(PM_SPHERE, M4::translate(P + V3(0, 10, -1)) * M4::scale(V3(5, 5.5f, 5)), T_SHADOW, V3(1, 1, 1), 0, fl, a);
            for (int i = 0; i < 7; i++) {
                float ea = (i - 3) * 0.32f;
                V3 ep = P + V3(std::sin(ea) * 2.6f, 10.6f + std::cos(ea * 2) * 0.8f, -1 - std::cos(ea) * 2.4f);
                float blink = std::fmod(t * 0.7f + i * 0.37f, 3.f) < 0.12f ? 0.1f : 1.f;
                R->drawPart(PM_SPHERE, M4::translate(ep) * M4::scale(V3(0.8f, 0.8f * blink, 0.5f)), T_EYE, V3(1, 1, 1), 0.8f, 0, a);
            }
            // crown of spikes
            for (int i = 0; i < 9; i++) {
                float ca = (i - 4) * 0.35f;
                R->drawPart(PM_CONE, M4::translate(P + V3(std::sin(ca) * 3.5f, 14, -1 + std::cos(ca) * 1.5f)) * M4::rotZ(-ca * 0.6f) * M4::scale(V3(0.7f, 3.0f, 0.7f)), T_OBSIDIAN, V3(1, 1, 1), 0, 0, a);
            }
            // hands
            for (int h = 0; h < 2; h++) {
                auto& H = b.hands[h];
                if (!H.alive) continue;
                float hf = H.flash > 0 ? 0.7f : 0;
                M4 hm = M4::translate(H.pos) * M4::rotY(h == 0 ? 0.3f : -0.3f);
                R->drawPart(PM_SPHERE, hm * M4::scale(V3(3.0f, 1.4f, 3.2f)), T_SHADOW, V3(1, 1, 1), 0, hf, a);
                for (int f = 0; f < 4; f++)
                    R->drawPart(PM_CONE, hm * M4::translate(V3(-1.2f + f * 0.8f, -0.2f, 1.9f)) * M4::rotX(1.9f) * M4::scale(V3(0.5f, 1.8f, 0.5f)), T_SHADOW, V3(1, 1, 1), 0, hf, a);
                bool exposed = H.down && H.shroud < 0.5f;
                R->drawPart(PM_SPHERE, hm * M4::translate(V3(0, 0.6f, 0)) * M4::scale(V3(1.2f, 0.7f, 1.2f)), T_EYE, V3(1, 1, 1), H.down ? 1.2f : 0.3f, hf, a);
                if (H.down && H.shroud > 0.5f) {
                    R->drawPart(PM_SPHERE, hm * M4::translate(V3(0, 0.4f, 0)) * M4::scale(V3(3.6f, 2.4f, 3.6f)), T_SHADOW, hexColor(0x5a3a8a), 0.3f, 0, 0.55f);
                    if (!R->shadowPass && std::fmod(t, 0.1f) < 0.02f) g.burst(H.pos + V3(0, 1, 0), hexColor(0x2a1a3a), 2, 1, 0.3f, 2, 0.8f, -1);
                }
                if (exposed) glow(R, H.pos + V3(0, 0.8f, 0), hexColor(0xc080ff), 2.2f, 0.8f);
            }
            // tendril towards a wick
            if (b.f[2] > 0 && b.i[4] >= 0 && b.i[4] < (int)g.lv.braziers.size()) {
                V3 from = C + V3(0, 6, 4), to = g.lv.braziers[b.i[4]].pos;
                float k = 1 - b.f[2] / 2.2f;
                for (int i = 0; i < 16; i++) {
                    float u = i / 15.f * k;
                    V3 p = lerp(from, to, u) + V3(0, std::sin(u * PI) * 3 + std::sin(t * 6 + i) * 0.3f, 0);
                    R->drawPart(PM_SPHERE, M4::translate(p) * M4::scale(V3(0.6f, 0.6f, 0.6f)), T_SHADOW, V3(1, 1, 1), 0, 0, a);
                }
            }
            // heart
            float pulse = 0.85f + 0.15f * std::sin(t * 6);
            V3 hp = b.phase == 2 ? b.v[2] : C + V3(0, 3.2f, 0);
            bool vul = false; for (auto& w : b.wps) if (w.id >= 20 && w.vulnerable) vul = true;
            if (b.phase >= 1) {
                R->drawPart(PM_OCTA, M4::translate(hp) * M4::rotY(t) * M4::scale(V3(1.6f, 2.0f, 1.6f) * pulse), T_SHADOW, V3(1, 1, 1), vul ? 1.0f : 0.2f, fl, a);
                R->drawPart(PM_SPHERE, M4::translate(hp) * M4::scale(V3(1.0f, 1.0f, 1.0f) * pulse), T_EYE, V3(1, 1, 1), 1.5f, fl, a);
                if (vul) glow(R, hp, hexColor(0xb060ff), 2.5f, 0.9f);
            }
        } break;
        default: break;
    }
}

static void drawLamp(Game& g, Renderer* R) {
    // the Great Lamp at the summit
    V3 C = g.lv.boss.center;
    bool lit = g.has("game.finished") && (g.screen != SC_ENDING || g.flame.charging);
    float open = g.boss.type == B_HUSH ? g.boss.f[5] : 0;
    R->drawPart(PM_CYL, M4::translate(C + V3(0, 1.4f, 0)) * M4::scale(V3(5.4f, 0.4f, 5.4f)), T_BRASS, V3(1, 1, 1));
    for (int i = 0; i < 6; i++) {
        float a = i * TAU / 6 + open * 0.3f;
        float r = 2.4f + open * 1.6f;
        R->drawPart(PM_CUBE, TRS(C + V3(std::sin(a) * r, 3.6f, std::cos(a) * r), a, V3(0.25f, 4.4f, 0.25f)), T_BRASS, V3(1, 1, 1));
        R->drawPart(PM_CUBE, TRS(C + V3(std::sin(a + 0.52f) * r * 0.95f, 3.6f, std::cos(a + 0.52f) * r * 0.95f), a + 0.52f, V3(2.2f, 4.2f, 0.08f)), T_WINDOW,
                    lit ? V3(1, 1, 1) : hexColor(0x1a1830), lit ? 1.0f : 0, 0, open > 0.5f ? 0.3f : 0.85f);
    }
    R->drawPart(PM_CONE, M4::translate(C + V3(0, 6.6f, 0)) * M4::scale(V3(6.4f, 2.0f, 6.4f)), T_METAL, V3(1, 1, 1));
    if (lit) {
        drawFlame(R, C + V3(0, 2.6f, 0), 3.0f, g.time, false);
        glow(R, C + V3(0, 3.2f, 0), hexColor(0xffc060), 6, 0.9f);
    }
}

static void drawGeometry(Game& g, Renderer* R) {
    R->drawMesh(g.lv.staticMesh, M4());
    for (auto& d : g.lv.dyn) {
        if ((d.kind == DK_SEAL || d.kind == DK_CRYSTAL) && d.broken) continue;
        if (d.kind == DK_GATE && d.openAmt > 0.99f) continue;
        float emis = d.kind == DK_CRYSTAL ? 0.5f : 0;
        R->drawMesh(d.mesh, M4::translate(d.offset), V3(1, 1, 1), emis);
    }
    for (auto& s : g.lv.spinners) {
        M4 rot = s.axis == 0 ? M4::rotZ(PI / 2) * M4::rotY(s.angle) : (s.axis == 1 ? M4::rotY(s.angle) : M4::rotX(PI / 2) * M4::rotY(s.angle));
        if (s.axis == 0) rot = M4::rotX(s.angle) * M4::rotZ(PI / 2);
        if (s.axis == 2) rot = M4::rotZ(s.angle) * M4::rotX(PI / 2);
        M4 m = M4::translate(s.pos) * rot;
        R->drawPart(PM_CYL, m * M4::scale(V3(s.r * 2, s.thick, s.r * 2)), s.tile, s.color);
        int teeth = std::max(8, (int)(s.r * 6));
        for (int i = 0; i < teeth; i++) {
            float a = TAU * i / teeth;
            R->drawPart(PM_CUBE, m * M4::translate(V3(std::cos(a) * s.r, 0, std::sin(a) * s.r)) * M4::rotY(-a) * M4::scale(V3(0.5f, s.thick * 0.9f, 0.35f)), s.tile, s.color * 0.9f);
        }
        R->drawPart(PM_CYL, m * M4::scale(V3(s.r * 0.5f, s.thick * 1.4f, s.r * 0.5f)), T_METAL, V3(1, 1, 1));
    }
    float t = g.time;
    // braziers
    for (auto& b : g.lv.braziers) {
        if (b.lit) {
            float fade = b.timed > 0 ? clampf(b.litT / b.timed, 0.3f, 1.f) : 1;
            drawFlame(R, b.pos + V3(0, 0.05f, 0), 1.7f * fade, t + b.pos.x, false);
            if (b.timed > 0) {  // countdown ring
                int segs = (int)(16 * b.litT / b.timed);
                for (int i = 0; i < segs; i++) {
                    float a = TAU * i / 16;
                    R->drawPart(PM_CUBE, TRS(b.pos + V3(std::cos(a) * 0.75f, -0.18f, std::sin(a) * 0.75f), -a, V3(0.08f, 0.08f, 0.2f)), T_WHITE, hexColor(0xffd070), 2);
                }
            }
        } else {
            R->drawPart(PM_CYL, M4::translate(b.pos - V3(0, 0.1f, 0)) * M4::scale(V3(0.8f, 0.12f, 0.8f)), b.sealed ? T_SEAL : T_ASH, V3(1, 1, 1), b.sealed ? 0.f : 0.3f);
        }
    }
    // lanterns
    for (auto& l : g.lv.lanterns) {
        R->drawPart(PM_CUBE, TRS(l.pos, t * 0.3f, V3(0.55f, 0.75f, 0.55f)), T_WINDOW, l.lit ? V3(1, 1, 1) : hexColor(0x30303a), l.lit ? 0.8f : 0);
        if (l.lit) drawFlame(R, l.pos - V3(0, 0.15f, 0), 0.6f, t + l.pos.z, false);
    }
    // anchors
    for (auto& a : g.lv.anchors) {
        float p = 1 + a.pulse * 0.5f;
        for (int i = 0; i < 12; i++) {
            float ang = TAU * i / 12 + t * 0.6f;
            R->drawPart(PM_CUBE, M4::translate(a.pos + V3(std::cos(ang) * 0.5f * p, std::sin(ang) * 0.5f * p, 0)) * M4::rotZ(ang) * M4::scale(V3(0.12f, 0.28f, 0.12f)), T_BRASS, V3(1, 1, 1), 0.4f + a.pulse);
        }
        R->drawPart(PM_OCTA, TRS(a.pos, t * 2, V3(0.22f, 0.22f, 0.22f)), T_WHITE, hexColor(0xffd070), 1.5f);
    }
    // pickups
    for (auto& p : g.lv.pickups) {
        if (p.taken) continue;
        float bob = std::sin(p.bob * 2.5f) * 0.15f;
        V3 pos = p.pos + V3(0, bob, 0);
        switch (p.type) {
            case PK_SHARD: R->drawPart(PM_OCTA, TRS(pos, t * 2, V3(0.5f, 0.8f, 0.5f)), T_CRYSTAL, hexColor(0xff7040), 1.2f); break;
            case PK_CANDLE:
                R->drawPart(PM_CYL, M4::translate(pos) * M4::scale(V3(0.25f, 0.6f, 0.25f)), T_WAX, hexColor(0xd0e0ff), 0.3f);
                drawFlame(R, pos + V3(0, 0.4f, 0), 0.5f, t * 1.3f + p.index, true);
                break;
            case PK_HEART:
                R->drawPart(PM_SPHERE, TRS(pos, t, V3(1.0f, 1.0f, 1.0f) * (1 + 0.1f * std::sin(t * 6))), T_FLESHGLOW, hexColor(0xffe0a0), 1.6f);
                R->drawPart(PM_OCTA, TRS(pos, -t * 1.5f, V3(1.6f, 1.6f, 1.6f)), T_GOLD, V3(1, 1, 1), 0.6f, 0, 0.5f);
                break;
            case PK_EMBER: {
                float sc = 0.18f + 0.03f * std::min(p.value, 5);
                R->drawPart(PM_OCTA, TRS(pos, t * 4 + p.bob, V3(sc, sc * 1.4f, sc)), T_WHITE, hexColor(0xffa030), 2.5f);
            } break;
            case PK_WAX: R->drawPart(PM_SPHERE, TRS(pos, t * 3, V3(0.3f, 0.4f, 0.3f)), T_WAX, V3(1, 1, 1), 0.6f); break;
            case PK_HAT: case PK_POWER: drawPickupExtra(R, p, pos, t); break;
        }
    }
    // wards above closed barriers (faint, only nearby)
    if (!R->shadowPass)
        for (auto& d : g.lv.dyn) {
            AABB w;
            if (!g.wardBox(d, w)) continue;
            V3 c = w.center();
            if (distXZ(c, g.pl.pos) > 16) continue;
            V3 sz = w.size(); sz.y = 6;
            V3 cc(c.x, d.base.hi.y + 3, c.z);
            float a = 0.10f + 0.05f * std::sin(t * 3);
            R->drawPart(PM_CUBE, M4::translate(cc) * M4::scale(V3(std::max(sz.x, 0.05f), sz.y, std::max(sz.z, 0.05f))), T_SHADOW, hexColor(0x9a6aff), 1.0f, 0, a);
        }
    // breakable urns
    for (auto& u : g.lv.breakables) {
        if (u.broken) continue;
        int tile = u.kind == 0 ? T_PLASTER : T_BRASS;
        V3 col = u.kind == 0 ? hexColor(0xc87a50) : V3(1, 1, 1);
        R->drawPart(PM_SPHERE, M4::translate(u.pos + V3(0, 0.38f, 0)) * M4::scale(V3(0.65f, 0.7f, 0.65f)), tile, col);
        R->drawPart(PM_CYL, M4::translate(u.pos + V3(0, 0.78f, 0)) * M4::scale(V3(0.34f, 0.2f, 0.34f)), tile, col * 0.85f);
        R->drawPart(PM_CUBE, M4::translate(u.pos + V3(0, 0.4f, 0.31f)) * M4::scale(V3(0.12f, 0.12f, 0.03f)), T_WHITE, hexColor(0xffb050), 1.2f);
    }
    // exits (doors of light)
    for (auto& e : g.lv.exits) {
        bool open = e.needFlag.empty() || g.has(e.needFlag);
        V3 c = e.zone.center(), s = e.zone.size();
        bool thinX = s.x < s.z;
        V3 sz = thinX ? V3(0.12f, s.y, s.z) : V3(s.x, s.y, 0.12f);
        R->drawPart(PM_CUBE, M4::translate(c) * M4::scale(sz), T_WHITE, open ? hexColor(0xffd890) : hexColor(0x5a3a8a), open ? 1.4f : 0.4f, 0, open ? 0.35f + 0.1f * std::sin(t * 3) : 0.25f);
    }
    // Spire pedestal hearts
    if (g.lv.id == LV_HUB) {
        static const int HI[4] = {0, 1, 2, 4};
        for (int i = 0; i < 4; i++) {
            if (!g.has("placed." + std::to_string(HI[i]))) continue;
            V3 p(-5.25f + i * 3.5f, 2.0f + 0.1f * std::sin(t * 2 + i), (i == 1 || i == 2) ? -10.5f : -9.8f);
            R->drawPart(PM_SPHERE, TRS(p, t, V3(0.7f, 0.7f, 0.7f)), T_FLESHGLOW, hexColor(0xffe0a0), 1.6f);
        }
    }
    // the people of the Hollow
    for (auto& tk : g.lv.talkers) if (tk.who >= 3) {
        float yaw = tk.yaw;
        if (g.screen == SC_PLAY && len(g.pl.pos - tk.pos) < 8 && tk.who != 15 && tk.who != 16) yaw = std::atan2(g.pl.pos.x - tk.pos.x, g.pl.pos.z - tk.pos.z);
        drawNPC(R, tk.who, tk.pos, yaw, t);
    }
    drawBots(g, R);
    drawHomeExtras(g, R);
    // Marrow
    for (auto& tk : g.lv.talkers) if (tk.who == 0) {
        float yaw = tk.yaw;
        if (g.screen == SC_PLAY && len(g.pl.pos - tk.pos) < 8) yaw = std::atan2(g.pl.pos.x - tk.pos.x, g.pl.pos.z - tk.pos.z);
        drawMarrow(R, tk.pos, yaw, t);
    }
    if (g.lv.id == LV_SPIRE) drawLamp(g, R);
    // enemies
    for (auto& e : g.enemies) if (e.alive) drawEnemy(g, R, e);
    // boss
    if (g.boss.active) drawBoss(g, R);
    // player & flame
    if (!(g.screen == SC_ENDING && g.cineStep >= 2 && g.cineStep < 6)) drawWick(g, R);
    if (g.flame.st != F_HELD && !g.pl.dead) {
        float sc = g.flame.charged ? 1.5f : 1.1f;
        drawFlame(R, g.flame.pos - V3(0, 0.15f, 0), sc, t, g.flame.charged);
    }
    // projectiles
    for (auto& p : g.projs) {
        switch (p.type) {
            case PR_TAR:
                if (g.boss.active && g.boss.type == B_RATKING) R->drawPart(PM_CUBE, TRS(p.pos, p.spin, V3(0.8f, 0.6f, 0.7f)), T_RUST, V3(1, 1, 1), 0.1f);
                else R->drawPart(PM_SPHERE, TRS(p.pos, p.spin, V3(0.7f, 0.6f, 0.7f)), T_TAR, V3(1, 1, 1), 0.2f);
                break;
            case PR_GEAR: {
                float yaw = std::atan2(p.vel.x, p.vel.z);
                M4 m = M4::translate(p.pos) * M4::rotY(yaw) * M4::rotX(p.spin * 2) * M4::rotZ(PI / 2);
                R->drawPart(PM_CYL, m * M4::scale(V3(p.radius * 2, 0.25f, p.radius * 2)), T_BRASS, V3(1, 1, 1));
                for (int i = 0; i < 10; i++) {
                    float a = TAU * i / 10;
                    R->drawPart(PM_CUBE, m * M4::translate(V3(std::cos(a) * p.radius, 0, std::sin(a) * p.radius)) * M4::rotY(-a) * M4::scale(V3(0.4f, 0.22f, 0.25f)), T_METAL, V3(1, 1, 1), 0.3f);
                }
            } break;
            case PR_ORB:
                if (g.boss.active && g.boss.type == B_WARDEN)
                    R->drawPart(PM_CUBE, TRS(p.pos, t * 5, V3(0.5f, 0.5f, 0.5f)), T_WHITE, hexColor(0x40ff80), 2.0f);
                else R->drawPart(PM_SPHERE, TRS(p.pos, p.spin, V3(0.7f, 0.7f, 0.7f)), T_SHADOW, V3(1, 1, 1), 0.6f);
                break;
            default: break;
        }
    }
    drawWorldExtras(g, R);
    // shockwave rings
    for (auto& r : g.rings) drawRing(R, r.c, r.r, r.color, 1.5f);
    // telegraphs
    for (auto& tg : g.teles) {
        float k = clampf(tg.t / tg.delay, 0, 1);
        V3 col = tg.kind == 1 ? hexColor(0xe8e0c8) : (tg.kind == 2 ? hexColor(0x40ff80) : hexColor(0xff3a3a));
        if (!tg.fired) {
            R->drawPart(PM_CYL, M4::translate(tg.pos + V3(0, 0.04f, 0)) * M4::scale(V3(tg.radius * 2 * k, 0.03f, tg.radius * 2 * k)), T_WHITE, col, 0.8f, 0, 0.45f);
            drawRing(R, tg.pos - V3(0, 0.08f, 0), tg.radius, col, 1.2f, 0.12f, 32);
            if (tg.kind == 1) {
                R->drawPart(PM_SPHERE, M4::translate(tg.pos + V3(0, 9 * (1 - k), 0)) * M4::scale(V3(1.2f, 1.2f, 1.2f)), T_WHITE, col, 0.6f, 0, 0.5f);
            }
        }
    }
}

void Game::draw() {
    if (!R) return;
    // environment, with the ending's dawn fading in
    Environment env = lv.env;
    if (screen == SC_ENDING && flame.charging) {
        float k = clampf((cineStep - 2 + cineT / 6.f) / 3.f, 0, 1);
        env.skyTop = lerp(env.skyTop, hexColor(0x2a3a6a), k);
        env.skyHorizon = lerp(env.skyHorizon, hexColor(0xe08a6a), k);
        env.fogColor = lerp(env.fogColor, hexColor(0x6a5a72), k);
        env.ambSky = lerp(env.ambSky, V3(0.45f, 0.42f, 0.5f), k);
        env.sunColor = lerp(env.sunColor, V3(0.9f, 0.65f, 0.45f), k);
    }
    R->env = env;
    V3 eye = camEye, target = camTarget;
    if (shake > 0) {
        float s = shake * shake * 0.6f;
        V3 off(std::sin(time * 71) * s, std::sin(time * 53 + 1) * s, std::sin(time * 61 + 2) * s);
        eye += off; target += off * 0.5f;
    }
    if (len(target - eye) < 0.01f) target = eye + V3(0, 0, 1);
    R->setCamera(eye, target, settings.fov);

    // lights
    R->lights = lv.lights;
    float flick = 0.9f + 0.1f * std::sin(time * 23) * std::sin(time * 7.3f);
    bool showFlame = !(screen == SC_INTRO && !flame.charging) && !(screen == SC_TITLE);
    if (!pl.dead && showFlame && !(screen == SC_ENDING && cineStep >= 2)) {
        float r = flame.st == F_HELD ? 10.f : (flame.charged ? 14.f : 12.f);
        if (flame.charging) r += flame.charge * 3;
        V3 col = flame.charged || (flame.charging && flame.charge >= 0.75f) ? V3(1, 0.9f, 0.7f) : FLAME_COL;
        R->lights.push_back({flame.st == F_HELD ? pl.pos + V3(0, 1.6f, 0) : flame.pos, r, col, 1.7f * flick});
        if (flame.st != F_HELD) R->lights.push_back({pl.pos + V3(0, 1.4f, 0), 2.8f, FLAME_COL, 0.5f});
        glow(R, flame.st == F_HELD ? pl.pos + V3(0, 1.6f, 0) : flame.pos, FLAME_COL, flame.st == F_HELD ? 0.9f : 1.2f, 0.5f);
    }
    if (screen == SC_TITLE) {
        R->lights.push_back({V3(0, 3.5f, -26), 9, hexColor(0xffb060), 1.0f});
    }
    for (auto& b : lv.braziers) if (b.lit) {
        R->lights.push_back({b.pos + V3(0, 0.6f, 0), 9.f, V3(1, 0.6f, 0.25f), 1.4f * flick});
        glow(R, b.pos + V3(0, 0.4f, 0), FLAME_COL, 1.4f, 0.5f);
    }
    for (auto& l : lv.lanterns) if (l.lit) { R->lights.push_back({l.pos, 7.f, V3(1, 0.7f, 0.35f), 1.1f}); glow(R, l.pos, FLAME_COL, 0.9f, 0.4f); }
    for (auto& a : lv.anchors) if (len(a.pos - pl.pos) < 30) glow(R, a.pos, hexColor(0xffd070), 0.5f + a.pulse, 0.35f);
    for (auto& p : lv.pickups) {
        if (p.taken) continue;
        if (p.type == PK_SHARD) { R->lights.push_back({p.pos, 4, hexColor(0xff6030), 1.0f}); glow(R, p.pos, hexColor(0xff7040), 0.8f, 0.5f); }
        if (p.type == PK_CANDLE) { R->lights.push_back({p.pos, 4, hexColor(0xd0e0ff), 0.9f}); glow(R, p.pos + V3(0, 0.4f, 0), hexColor(0xe0f0ff), 0.7f, 0.5f); }
        if (p.type == PK_HEART) { R->lights.push_back({p.pos, 10, hexColor(0xffd070), 1.6f}); glow(R, p.pos, hexColor(0xffd070), 2.4f, 0.7f); }
    }
    for (auto& e : lv.exits) if (e.needFlag.empty() || has(e.needFlag)) R->lights.push_back({e.zone.center(), 5, hexColor(0xffd890), 0.6f});
    if (bloomFlash > 0) R->lights.push_back({bloomPos, 6 + bloomR, V3(1, 0.7f, 0.3f), bloomFlash * 3});
    if (boss.active) {
        for (auto& w : boss.wps) if (w.vulnerable) R->lights.push_back({w.pos, 8, boss.type == B_HUSH ? hexColor(0xb060ff) : hexColor(0xff9040), 1.4f});
        if (boss.type == B_HUSH) R->lights.push_back({boss.pos + V3(0, 10, -2), 14, hexColor(0x8040ff), 0.5f});
    }
    if (lv.id == LV_SPIRE && has("game.finished") && (screen != SC_ENDING || flame.charging)) R->lights.push_back({lv.boss.center + V3(0, 3.5f, 0), 30, hexColor(0xffc070), 2.0f});
    for (auto& p : projs) if (p.type == PR_ORB) R->lights.push_back({p.pos, 3, hexColor(0x9a5aff), 0.8f});
    if (lv.id == LV_HUB && screen != SC_TITLE) {
        { static const int HI[4] = {0, 1, 2, 4}; for (int i = 0; i < 4; i++) if (has("placed." + std::to_string(HI[i]))) R->lights.push_back({V3(-5.25f + i * 3.5f, 2.2f, -10), 5, hexColor(0xffd070), 1.0f}); }
    }

    homeLights(*this, R);
    extrasLights(*this, R);
    // shadow + scene passes
    R->beginShadow(screen == SC_TITLE ? V3(0, 0, 0) : pl.pos);
    drawGeometry(*this, R);
    R->endShadow();
    R->beginScene();
    drawGeometry(*this, R);
    if (lv.waterY > -999) R->drawWater(lv.waterMesh, lv.waterColor, 1);
    homeWater(*this, R);
    R->drawParticles();
    // post-process parameters
    float fadeAmt = 0;
    if (transitioning) fadeAmt = clampf(transitionT / 0.45f, 0, 1);
    fadeAmt = std::max(fadeAmt, respawnFade);
    if (pl.dead) fadeAmt = std::max(fadeAmt, clampf((pl.deadT - 0.8f) / 1.0f, 0, 1));
    R->fade = fadeAmt;
    R->overlayA = flashOverlay * 0.6f;
    R->overlayCol = flashColor;
    if (ocT > 0 && flashOverlay < 0.2f) { R->overlayA = std::max(R->overlayA, 0.12f * std::min(1.f, ocT)); R->overlayCol = hexColor(0x40c0ff); }
    R->glitch = (screen == SC_PLAY || screen == SC_PAUSE) ? glitch : 0.f;
    if (bloomFlash > 0.7f) { R->overlayA = std::max(R->overlayA, (bloomFlash - 0.7f) * 0.5f); R->overlayCol = hexColor(0xffd0a0); }
    if (lv.id == LV_HOME && vox.active && vox.get((int)std::floor(eye.x), (int)std::floor(eye.y), (int)std::floor(eye.z)) == BK_WATER) { R->overlayA = std::max(R->overlayA, 0.45f); R->overlayCol = hexColor(0x1a4a7a); }
    R->endScene();
    drawUI();
    R->uiFlush();
}
