// WICKLIGHT - drawing the new things: hats, side-job folk, hacker terminals, the city (searchlights,
// FBI agents, drones, the rescue chopper), the three late powers, fishing and the candle race.
#include "game.h"

static M4 TRS(V3 t, float yaw, V3 s) { return M4::translate(t) * M4::rotY(yaw) * M4::scale(s); }
static void glow(Renderer* R, V3 p, V3 col, float size, float alpha = 1) {
    if (R->shadowPass) return;
    Particle q; q.pos = p; q.color = col; q.alpha = alpha; q.size = size; q.life = q.maxLife = 0.0001f; q.kind = 1;
    R->spawn(q);
}
static void flameShape(Renderer* R, V3 p, float scale, float t, V3 outer, V3 mid, V3 core, float alpha = 1) {
    float f1 = std::sin(t * 13) * 0.08f, f2 = std::sin(t * 17 + 1) * 0.06f;
    R->drawPart(PM_OCTA, TRS(p + V3(0, 0.12f * scale, 0), t * 2, V3(0.42f + f1, 0.75f + f2, 0.42f + f1) * scale), T_WHITE, outer, 2.2f, 0, alpha);
    R->drawPart(PM_OCTA, TRS(p + V3(0, 0.1f * scale, 0), -t * 3, V3(0.28f, 0.52f + f1, 0.28f) * scale), T_WHITE, mid, 2.6f, 0, alpha);
    R->drawPart(PM_OCTA, TRS(p + V3(0, 0.05f * scale, 0), t * 4, V3(0.15f, 0.28f, 0.15f) * scale), T_WHITE, core, 3.0f, 0, alpha);
}
static void beam(Renderer* R, V3 a, V3 b, float thick, V3 col, float emis, float alpha = 1, int seg = 0) {
    V3 d = b - a; float l = len(d);
    if (l < 0.01f) return;
    if (seg <= 0) seg = std::max(1, (int)(l / 0.5f));
    for (int i = 0; i < seg; i++) {
        V3 p = a + d * ((i + 0.5f) / seg);
        R->drawPart(PM_CUBE, M4::translate(p) * M4::scale(V3(thick, thick, thick)), T_WHITE, col, emis, 0, alpha);
    }
}

// ------------------------------------------------------------------ hats
float hatLift(int hat) {
    static const float L[] = {0, 0, 0.1f, 0.06f, 0.1f, 0.4f, 0.42f, 0.5f, 0.12f, 0.2f, 0.08f};
    return (hat >= 0 && hat <= 10) ? L[hat] : 0;
}

void drawHat(Renderer* R, const M4& base, int hat, float headY, float t, float alpha) {
    if (hat <= 0) return;
    M4 h = base * M4::translate(V3(0, headY + 0.15f, 0));
    switch (hat) {
        case 1:  // cat ears
            for (int s = -1; s <= 1; s += 2) {
                R->drawPart(PM_CONE, h * M4::translate(V3(0.15f * s, 0.09f, -0.02f)) * M4::rotZ(-0.25f * s) * M4::scale(V3(0.14f, 0.22f, 0.08f)), T_CLOTH, hexColor(0x2a2a30), 0, 0, alpha);
                R->drawPart(PM_CONE, h * M4::translate(V3(0.15f * s, 0.08f, 0.015f)) * M4::rotZ(-0.25f * s) * M4::scale(V3(0.08f, 0.14f, 0.04f)), T_WHITE, hexColor(0xf0a0b0), 0.2f, 0, alpha);
            }
            break;
        case 2:  // lamplighter's cap
            R->drawPart(PM_CYL, h * M4::translate(V3(0, 0.06f, 0)) * M4::scale(V3(0.46f, 0.13f, 0.46f)), T_CLOTH, hexColor(0x2a3a5a), 0, 0, alpha);
            R->drawPart(PM_CUBE, h * M4::translate(V3(0, 0.0f, 0.25f)) * M4::scale(V3(0.32f, 0.03f, 0.16f)), T_CLOTH, hexColor(0x1a2236), 0, 0, alpha);
            R->drawPart(PM_CUBE, h * M4::translate(V3(0, 0.07f, 0.235f)) * M4::scale(V3(0.08f, 0.06f, 0.02f)), T_BRASS, V3(1, 1, 1), 0.6f, 0, alpha);
            break;
        case 3:  // fisher's hat
            R->drawPart(PM_CYL, h * M4::translate(V3(0, 0.0f, 0)) * M4::rotX(-0.08f) * M4::scale(V3(0.66f, 0.03f, 0.66f)), T_CLOTH, hexColor(0xd0b030), 0, 0, alpha);
            R->drawPart(PM_CYL, h * M4::translate(V3(0, 0.05f, 0)) * M4::scale(V3(0.44f, 0.1f, 0.44f)), T_CLOTH, hexColor(0xc0a028), 0, 0, alpha);
            break;
        case 4:  // golden crown
            R->drawPart(PM_CYL, h * M4::translate(V3(0, 0.03f, 0)) * M4::scale(V3(0.46f, 0.08f, 0.46f)), T_GOLD, V3(1, 1, 1), 0.5f, 0, alpha);
            for (int k = 0; k < 5; k++) {
                float a = k * TAU / 5 + 0.3f;
                R->drawPart(PM_CONE, h * M4::translate(V3(std::sin(a) * 0.19f, 0.12f, std::cos(a) * 0.19f)) * M4::scale(V3(0.09f, 0.14f, 0.09f)), T_GOLD, V3(1, 1, 1), 0.5f, 0, alpha);
            }
            R->drawPart(PM_OCTA, h * M4::translate(V3(0, 0.04f, 0.23f)) * M4::scale(V3(0.06f, 0.06f, 0.03f)), T_WHITE, hexColor(0xff3060), 2, 0, alpha);
            break;
        case 5:  // top hat
            R->drawPart(PM_CYL, h * M4::scale(V3(0.62f, 0.03f, 0.62f)), T_CLOTH, hexColor(0x18181e), 0, 0, alpha);
            R->drawPart(PM_CYL, h * M4::translate(V3(0, 0.2f, 0)) * M4::scale(V3(0.38f, 0.4f, 0.38f)), T_CLOTH, hexColor(0x202028), 0, 0, alpha);
            R->drawPart(PM_CYL, h * M4::translate(V3(0, 0.06f, 0)) * M4::scale(V3(0.39f, 0.07f, 0.39f)), T_CLOTH, hexColor(0xa02030), 0, 0, alpha);
            break;
        case 6:  // party hat
            R->drawPart(PM_CONE, h * M4::translate(V3(0, 0.2f, 0)) * M4::scale(V3(0.32f, 0.42f, 0.32f)), T_CLOTH, hexColor(0xff60b0), 0.2f, 0, alpha);
            R->drawPart(PM_CYL, h * M4::translate(V3(0, 0.1f, 0)) * M4::scale(V3(0.25f, 0.04f, 0.25f)), T_WHITE, hexColor(0x60e0ff), 0.6f, 0, alpha);
            R->drawPart(PM_CYL, h * M4::translate(V3(0, 0.22f, 0)) * M4::scale(V3(0.16f, 0.04f, 0.16f)), T_WHITE, hexColor(0xffe040), 0.6f, 0, alpha);
            break;
        case 7:  // wizard hat
            R->drawPart(PM_CYL, h * M4::scale(V3(0.72f, 0.03f, 0.72f)), T_CLOTH, hexColor(0x2a2a8a), 0, 0, alpha);
            R->drawPart(PM_CONE, h * M4::translate(V3(0, 0.26f, -0.02f)) * M4::rotX(-0.15f) * M4::scale(V3(0.42f, 0.55f, 0.42f)), T_CLOTH, hexColor(0x3030a0), 0, 0, alpha);
            R->drawPart(PM_OCTA, h * M4::translate(V3(0.1f, 0.22f, 0.15f)) * M4::rotY(t) * M4::scale(V3(0.08f, 0.08f, 0.03f)), T_WHITE, hexColor(0xffe060), 2.2f, 0, alpha);
            break;
        case 8:  // viking helmet
            R->drawPart(PM_SPHERE, h * M4::translate(V3(0, 0.0f, 0)) * M4::scale(V3(0.48f, 0.28f, 0.48f)), T_METAL, hexColor(0xa0a0a8), 0, 0, alpha);
            for (int s = -1; s <= 1; s += 2)
                R->drawPart(PM_CONE, h * M4::translate(V3(0.27f * s, 0.12f, 0)) * M4::rotZ(-0.9f * s) * M4::scale(V3(0.09f, 0.26f, 0.09f)), T_BONE, hexColor(0xf0e8d0), 0, 0, alpha);
            break;
        case 9:  // propeller cap
            for (int k = 0; k < 4; k++) {
                static const uint32_t C[4] = {0xe03030, 0x3060e0, 0xe0c030, 0x30a050};
                R->drawPart(PM_SPHERE, h * M4::rotY(k * PI / 2) * M4::translate(V3(0.05f, 0.0f, 0.05f)) * M4::scale(V3(0.32f, 0.22f, 0.32f)), T_CLOTH, hexColor(C[k]), 0, 0, alpha);
            }
            R->drawPart(PM_CYL, h * M4::translate(V3(0, 0.18f, 0)) * M4::scale(V3(0.03f, 0.18f, 0.03f)), T_METAL, V3(1, 1, 1), 0, 0, alpha);
            R->drawPart(PM_CUBE, h * M4::translate(V3(0, 0.27f, 0)) * M4::rotY(t * 18) * M4::scale(V3(0.5f, 0.02f, 0.08f)), T_WHITE, hexColor(0xe03030), 0.2f, 0, alpha);
            break;
        case 10:  // FBI cap
            R->drawPart(PM_SPHERE, h * M4::translate(V3(0, 0.0f, 0)) * M4::scale(V3(0.46f, 0.24f, 0.46f)), T_CLOTH, hexColor(0x1a2440), 0, 0, alpha);
            R->drawPart(PM_CUBE, h * M4::translate(V3(0, 0.0f, 0.25f)) * M4::scale(V3(0.34f, 0.03f, 0.2f)), T_CLOTH, hexColor(0x141c30), 0, 0, alpha);
            R->drawPart(PM_CUBE, h * M4::translate(V3(0, 0.07f, 0.22f)) * M4::scale(V3(0.22f, 0.05f, 0.02f)), T_WHITE, hexColor(0xffd040), 1.0f, 0, alpha);
            break;
    }
}

// ------------------------------------------------------------------ side-job folk
void drawNPCExtra(Renderer* R, int who, V3 pos, float yaw, float t) {
    M4 b = M4::translate(pos) * M4::rotY(yaw);
    float bob = std::sin(t * 2 + who) * 0.03f;
    auto person = [&](V3 coat, float sc) {
        M4 m = b * M4::scale(V3(sc, sc, sc));
        R->drawPart(PM_CONE, m * M4::translate(V3(0, 0.7f, 0)) * M4::scale(V3(0.9f, 1.4f, 0.8f)), T_CLOTH, coat);
        R->drawPart(PM_CYL, m * M4::translate(V3(0, 1.2f + bob, 0)) * M4::scale(V3(0.55f, 0.6f, 0.45f)), T_CLOTH, coat * 0.9f);
        R->drawPart(PM_SPHERE, m * M4::translate(V3(0, 1.72f + bob, 0)) * M4::scale(V3(0.44f, 0.48f, 0.44f)), T_WHITE, hexColor(0xd8a888));
        for (int s = -1; s <= 1; s += 2) R->drawPart(PM_CUBE, m * M4::translate(V3(0.1f * s, 1.76f + bob, 0.21f)) * M4::scale(V3(0.07f, 0.05f, 0.03f)), T_WHITE, hexColor(0x101010));
        return m;
    };
    switch (who) {
        case 11: {  // Mira
            M4 m = person(hexColor(0xc04a7a), 1.0f);
            R->drawPart(PM_SPHERE, m * M4::translate(V3(0, 1.8f + bob, -0.1f)) * M4::scale(V3(0.5f, 0.5f, 0.45f)), T_CLOTH, hexColor(0x6a3a20));
            R->drawPart(PM_SPHERE, m * M4::translate(V3(0, 1.55f + bob, -0.25f)) * M4::scale(V3(0.3f, 0.5f, 0.2f)), T_CLOTH, hexColor(0x6a3a20));
        } break;
        case 12: {  // Whiskers the cat
            float tail = std::sin(t * 3) * 0.4f;
            V3 fur = hexColor(0x5a5560);
            R->drawPart(PM_SPHERE, b * M4::translate(V3(0, 0.3f, 0)) * M4::scale(V3(0.4f, 0.36f, 0.7f)), T_CLOTH, fur);
            R->drawPart(PM_SPHERE, b * M4::translate(V3(0, 0.55f, 0.35f)) * M4::scale(V3(0.34f, 0.3f, 0.3f)), T_CLOTH, fur * 1.1f);
            for (int s = -1; s <= 1; s += 2) {
                R->drawPart(PM_CONE, b * M4::translate(V3(0.1f * s, 0.75f, 0.33f)) * M4::scale(V3(0.1f, 0.14f, 0.06f)), T_CLOTH, fur);
                R->drawPart(PM_CUBE, b * M4::translate(V3(0.07f * s, 0.58f, 0.5f)) * M4::scale(V3(0.05f, 0.06f, 0.02f)), T_WHITE, hexColor(0xe0e040), 2.0f);
                for (int f = -1; f <= 1; f += 2) R->drawPart(PM_CUBE, b * M4::translate(V3(0.12f * s, 0.1f, 0.22f * f)) * M4::scale(V3(0.08f, 0.2f, 0.08f)), T_CLOTH, fur);
            }
            R->drawPart(PM_CUBE, b * M4::translate(V3(0, 0.45f, -0.35f)) * M4::rotY(tail) * M4::rotX(-0.7f) * M4::translate(V3(0, 0.2f, 0)) * M4::scale(V3(0.06f, 0.45f, 0.06f)), T_CLOTH, fur);
        } break;
        case 13: {  // Old Fenn the lamplighter
            M4 m = person(hexColor(0x4a5a6a), 1.0f);
            R->drawPart(PM_CONE, m * M4::translate(V3(0, 1.5f + bob, 0.15f)) * M4::rotX(PI) * M4::scale(V3(0.36f, 0.5f, 0.25f)), T_WHITE, hexColor(0xe8e8e8));
            R->drawPart(PM_CYL, m * M4::translate(V3(0, 2.0f + bob, 0)) * M4::scale(V3(0.46f, 0.14f, 0.46f)), T_CLOTH, hexColor(0x2a3a5a));
            R->drawPart(PM_CUBE, m * M4::translate(V3(0, 1.94f + bob, 0.24f)) * M4::scale(V3(0.3f, 0.03f, 0.16f)), T_CLOTH, hexColor(0x1a2236));
            R->drawPart(PM_CYL, m * M4::translate(V3(0.55f, 1.4f, 0.2f)) * M4::scale(V3(0.06f, 2.8f, 0.06f)), T_DARKWOOD, V3(1, 1, 1));
            R->drawPart(PM_CUBE, m * M4::translate(V3(0.55f, 2.85f, 0.32f)) * M4::scale(V3(0.06f, 0.06f, 0.3f)), T_BRASS, V3(1, 1, 1), 0.3f);
        } break;
        case 14: {  // Flick the racer
            M4 m = person(hexColor(0xd03a2a), 0.85f);
            R->drawPart(PM_CYL, m * M4::translate(V3(0, 1.88f + bob, 0)) * M4::scale(V3(0.47f, 0.07f, 0.47f)), T_WHITE, hexColor(0xffffff), 0.3f);
            R->drawPart(PM_SPHERE, m * M4::translate(V3(0, 1.95f + bob, -0.05f)) * M4::scale(V3(0.42f, 0.25f, 0.42f)), T_CLOTH, hexColor(0xe0a020));
            float wave = std::sin(t * 6) * 0.5f;
            R->drawPart(PM_CUBE, m * M4::translate(V3(0.4f, 1.4f, 0)) * M4::rotZ(2.4f + wave) * M4::translate(V3(0, 0.25f, 0)) * M4::scale(V3(0.12f, 0.5f, 0.12f)), T_CLOTH, hexColor(0xd03a2a));
        } break;
        case 15: {  // the wardrobe mirror
            R->drawPart(PM_CUBE, b * M4::translate(V3(0, 1.3f, 0)) * M4::scale(V3(1.3f, 2.6f, 0.2f)), T_DARKWOOD, V3(1, 1, 1));
            R->drawPart(PM_CUBE, b * M4::translate(V3(0, 1.35f, 0.08f)) * M4::scale(V3(1.0f, 2.2f, 0.06f)), T_WHITE, hexColor(0xa8c0d8), 0.5f);
            float sh = std::fmod(t * 0.4f, 2.f) - 0.5f;
            if (sh < 1) R->drawPart(PM_CUBE, b * M4::translate(V3(sh * 0.8f, 1.6f, 0.12f)) * M4::rotZ(0.6f) * M4::scale(V3(0.08f, 1.2f, 0.02f)), T_WHITE, hexColor(0xffffff), 1.0f, 0, 0.6f);
            R->drawPart(PM_SPHERE, b * M4::translate(V3(0, 2.75f, 0)) * M4::scale(V3(0.4f, 0.3f, 0.2f)), T_GOLD, V3(1, 1, 1), 0.3f);
        } break;
        case 16: {  // a fishing spot: bucket and a rod leaning on the post
            R->drawPart(PM_CYL, b * M4::translate(V3(-0.5f, 0.25f, -0.3f)) * M4::scale(V3(0.45f, 0.5f, 0.45f)), T_METAL, hexColor(0x8a9aa0));
            R->drawPart(PM_CYL, b * M4::translate(V3(-0.5f, 0.49f, -0.3f)) * M4::scale(V3(0.38f, 0.02f, 0.38f)), T_WHITE, hexColor(0x3a6a8a), 0.3f);
            R->drawPart(PM_CUBE, b * M4::translate(V3(0.2f, 1.0f, 0.2f)) * M4::rotX(0.5f) * M4::scale(V3(0.04f, 2.0f, 0.04f)), T_DARKWOOD, V3(1, 1, 1));
            R->drawPart(PM_SPHERE, b * M4::translate(V3(0, 1.7f + bob * 3, 0.5f)) * M4::scale(V3(0.18f, 0.18f, 0.18f)), T_WHITE, hexColor(0xff4040), 0.8f);
        } break;
    }
}

// ------------------------------------------------------------------ new enemies
void drawEnemyExtra(Game& g, Renderer* R, const Enemy& e) {
    float t = e.anim;
    float fl = e.flash > 0 ? 0.7f : 0;
    M4 b = M4::translate(e.pos) * M4::rotY(e.yaw);
    V3 green = hexColor(0x50ff90);
    switch (e.type) {
        case E_VIRUS: {
            float s = e.gen ? 0.75f : 1.0f;
            M4 m = b * M4::scale(V3(s, s, s));
            float pulse = 0.8f + 0.2f * std::sin(t * 9 + e.id);
            R->drawPart(PM_SPHERE, m * M4::translate(V3(0, 0.32f, 0)) * M4::scale(V3(0.6f, 0.45f, 0.75f)), T_WHITE, green * pulse, 1.2f, fl);
            R->drawPart(PM_OCTA, m * M4::translate(V3(0, 0.36f, 0)) * M4::rotY(t * 3) * M4::scale(V3(0.8f, 0.3f, 0.8f)), T_CODE, V3(1, 1, 1), 1.0f, fl, 0.8f);
            for (int s2 = -1; s2 <= 1; s2 += 2)
                for (int k = 0; k < 3; k++) {
                    float sw = std::sin(t * 25 + k * 2 + s2) * 0.4f;
                    R->drawPart(PM_CUBE, m * M4::translate(V3(0.3f * s2, 0.2f, -0.2f + k * 0.2f)) * M4::rotZ(s2 * (0.9f + sw)) * M4::translate(V3(0, -0.12f, 0)) * M4::scale(V3(0.05f, 0.28f, 0.05f)), T_WHITE, hexColor(0x1a3a22), 0, fl);
                }
            for (int s2 = -1; s2 <= 1; s2 += 2) R->drawPart(PM_CUBE, m * M4::translate(V3(0.1f * s2, 0.42f, 0.36f)) * M4::scale(V3(0.08f, 0.08f, 0.04f)), T_WHITE, hexColor(0xff3030), 2.5f);
            glow(R, e.pos + V3(0, 0.35f, 0), green, 0.7f * s, 0.35f);
        } break;
        case E_DRONE: {
            R->drawPart(PM_SPHERE, b * M4::scale(V3(0.7f, 0.45f, 0.7f)), T_METAL, hexColor(0x40444c), 0, fl);
            R->drawPart(PM_CUBE, b * M4::translate(V3(0, 0, 0.33f)) * M4::scale(V3(0.4f, 0.22f, 0.04f)), T_CODE, V3(1, 1, 1), 1.5f);
            for (int k = 0; k < 4; k++) {
                float a = k * PI / 2 + PI / 4;
                V3 arm(std::sin(a) * 0.55f, 0.1f, std::cos(a) * 0.55f);
                R->drawPart(PM_CUBE, b * M4::translate(arm * 0.5f) * M4::rotY(a) * M4::scale(V3(0.08f, 0.06f, 0.55f)), T_METAL, V3(1, 1, 1), 0, fl);
                R->drawPart(PM_CYL, b * M4::translate(arm + V3(0, 0.08f, 0)) * M4::rotY(t * 40 + k) * M4::scale(V3(0.45f, 0.02f, 0.08f)), T_WHITE, hexColor(0xb0b0b8), 0.2f, 0, 0.7f);
            }
            bool ready = e.cd < 0.4f && e.state == 1;
            R->drawPart(PM_SPHERE, b * M4::translate(V3(0, -0.22f, 0.15f)) * M4::scale(V3(0.18f, 0.18f, 0.18f)), T_WHITE, ready ? hexColor(0xff4040) : green, 2.5f);
            glow(R, e.pos, ready ? hexColor(0xff4040) : green, 0.8f, 0.3f);
        } break;
        case E_DECOY: {
            // a fake Wick made of glitches: green wax, jittery, splitting colours
            float jit = std::sin(t * 47 + e.id) * std::sin(t * 31) * 0.07f;
            float alpha = (std::fmod(t * 7 + e.id, 1.f) < 0.15f) ? 0.4f : 0.85f;
            M4 m = M4::translate(e.pos + V3(jit, 0, 0)) * M4::rotY(e.yaw);
            V3 wax = hexColor(0x9ae8c0);
            for (int s = -1; s <= 1; s += 2) R->drawPart(PM_CUBE, m * M4::translate(V3(0.12f * s, 0.16f, 0)) * M4::rotX(std::sin(t * 14) * 0.6f * s) * M4::scale(V3(0.13f, 0.3f, 0.14f)), T_WAX, wax * 0.85f, 0.3f, fl, alpha);
            R->drawPart(PM_CYL, m * M4::translate(V3(0, 0.55f, 0)) * M4::scale(V3(0.5f, 0.55f, 0.46f)), T_WAX, wax, 0.3f, fl, alpha);
            R->drawPart(PM_CYL, m * M4::translate(V3(0, 0.98f, 0)) * M4::scale(V3(0.42f, 0.3f, 0.42f)), T_WAX, wax, 0.3f, fl, alpha);
            for (int s = -1; s <= 1; s += 2) R->drawPart(PM_CUBE, m * M4::translate(V3(0.09f * s, 1.0f, 0.2f)) * M4::scale(V3(0.06f, 0.09f, 0.03f)), T_WHITE, hexColor(0xff2040), 2.5f, 0, alpha);
            float sc = e.state == 1 ? 1 + std::min(1.f, e.t / 0.55f) * 0.9f : 1.f;
            flameShape(R, e.pos + V3(-jit, 1.3f, 0), sc, g.time * 1.3f + e.id, hexColor(0x30ff80), hexColor(0x80ffc0), hexColor(0xe0fff0), alpha);
            R->drawPart(PM_CUBE, m * M4::translate(V3(0.04f, 0.6f, 0)) * M4::scale(V3(0.52f, 0.06f, 0.48f)), T_WHITE, hexColor(0xff40ff), 1.5f, 0, 0.5f);
            glow(R, e.pos + V3(0, 1.3f, 0), hexColor(0x40ff90), 0.9f * sc, 0.5f);
        } break;
        case E_AGENT: {
            float step = lenXZ(e.vel) > 0.3f ? std::sin(t * 9) : 0;
            V3 suit = hexColor(0x16161c);
            for (int s = -1; s <= 1; s += 2) R->drawPart(PM_CUBE, b * M4::translate(V3(0.14f * s, 0.42f, 0)) * M4::rotX(step * 0.6f * s) * M4::translate(V3(0, -0.1f, 0)) * M4::scale(V3(0.2f, 0.8f, 0.24f)), T_CLOTH, suit, 0, fl);
            R->drawPart(PM_CUBE, b * M4::translate(V3(0, 1.15f, 0)) * M4::scale(V3(0.62f, 0.8f, 0.36f)), T_CLOTH, suit, 0, fl);
            R->drawPart(PM_CUBE, b * M4::translate(V3(0, 1.3f, 0.17f)) * M4::scale(V3(0.18f, 0.45f, 0.04f)), T_WHITE, hexColor(0xe8e8e8), 0, fl);
            R->drawPart(PM_CUBE, b * M4::translate(V3(0, 1.28f, 0.2f)) * M4::scale(V3(0.06f, 0.36f, 0.03f)), T_WHITE, hexColor(0x101018), 0, fl);
            R->drawPart(PM_CUBE, b * M4::translate(V3(0, 1.74f, 0)) * M4::scale(V3(0.36f, 0.4f, 0.36f)), T_WHITE, hexColor(0xc89878), 0, fl);
            R->drawPart(PM_CUBE, b * M4::translate(V3(0, 1.92f, 0)) * M4::scale(V3(0.38f, 0.1f, 0.38f)), T_CLOTH, hexColor(0x2a1a10), 0, fl);
            R->drawPart(PM_CUBE, b * M4::translate(V3(0, 1.78f, 0.18f)) * M4::scale(V3(0.32f, 0.08f, 0.04f)), T_WHITE, hexColor(0x050508), 0.5f);
            R->drawPart(PM_CUBE, b * M4::translate(V3(0.17f, 1.72f, 0.1f)) * M4::scale(V3(0.04f, 0.1f, 0.04f)), T_WHITE, hexColor(0x202020));  // earpiece
            // arm with flashlight (patrol) or pistol (chase / aim)
            bool aiming = e.state == 2;
            float arm = aiming ? -1.5f : (e.state == 0 ? -0.9f : -0.5f);
            M4 am = b * M4::translate(V3(0.38f, 1.42f, 0)) * M4::rotX(arm);
            R->drawPart(PM_CUBE, am * M4::translate(V3(0, -0.28f, 0)) * M4::scale(V3(0.14f, 0.56f, 0.16f)), T_CLOTH, suit, 0, fl);
            if (e.state == 0) {
                R->drawPart(PM_CYL, am * M4::translate(V3(0, -0.6f, 0.05f)) * M4::scale(V3(0.1f, 0.24f, 0.1f)), T_METAL, V3(1, 1, 1));
                R->drawPart(PM_CYL, am * M4::translate(V3(0, -0.73f, 0.05f)) * M4::scale(V3(0.12f, 0.02f, 0.12f)), T_WHITE, hexColor(0xfff8d0), 3);
                if (!R->shadowPass) {  // the flashlight cone
                    V3 fwd(std::sin(e.yaw), 0, std::cos(e.yaw));
                    V3 tip = e.pos + V3(0, 1.1f, 0) + fwd * 0.5f;
                    M4 cm = M4::translate(tip + fwd * 3.2f + V3(0, -0.5f, 0)) * M4::rotY(e.yaw) * M4::rotX(-PI / 2 - 0.15f) * M4::scale(V3(3.2f, 6.4f, 2.0f));
                    R->drawPart(PM_CONE, cm, T_WHITE, hexColor(0xfff0c0), 1.0f, 0, 0.1f);
                }
            } else {
                R->drawPart(PM_CUBE, am * M4::translate(V3(0, -0.6f, 0.06f)) * M4::scale(V3(0.08f, 0.14f, 0.24f)), T_METAL, hexColor(0x303038));
            }
            if (aiming && !R->shadowPass && e.t < 0.85f) {
                V3 o = e.pos + V3(0, 1.3f, 0) + V3(std::sin(e.yaw), 0, std::cos(e.yaw)) * 0.5f;
                float blink = (e.t > 0.6f && std::fmod(e.t * 20, 1.f) < 0.5f) ? 1.0f : 0.6f;
                beam(R, o, e.target2, 0.035f, hexColor(0xff2020), 2.5f, blink);
                glow(R, e.target2, hexColor(0xff2020), 0.4f, 0.8f);
            }
            if (e.alerted && !R->shadowPass) R->drawPart(PM_CUBE, M4::translate(e.pos + V3(0, 2.35f + 0.05f * std::sin(t * 8), 0)) * M4::scale(V3(0.1f, 0.3f, 0.1f)), T_WHITE, hexColor(0xff3030), 2.5f);
        } break;
        default: break;
    }
}

// ------------------------------------------------------------------ pickups
void drawPickupExtra(Renderer* R, const Pickup& p, V3 pos, float t) {
    if (p.type == PK_HAT) {
        M4 base = M4::translate(pos - V3(0, 0.6f, 0)) * M4::rotY(t * 1.5f) * M4::scale(V3(1.6f, 1.6f, 1.6f));
        drawHat(R, base, p.index, 0.2f, t, 1);
        R->drawPart(PM_CYL, M4::translate(pos - V3(0, 0.75f, 0)) * M4::scale(V3(1.2f, 0.04f, 1.2f)), T_WHITE, hexColor(0xffe0a0), 1.5f, 0, 0.4f + 0.2f * std::sin(t * 4));
        glow(R, pos, hexColor(0xffe0a0), 1.4f, 0.5f);
    } else if (p.type == PK_POWER) {
        V3 col = p.index == 1 ? hexColor(0xff8030) : hexColor(0xffd070);
        R->drawPart(PM_OCTA, TRS(pos, t * 2, V3(0.9f, 1.2f, 0.9f)), T_WHITE, col, 2.0f);
        for (int k = 0; k < 3; k++) R->drawPart(PM_CUBE, M4::translate(pos) * M4::rotY(t * (1 + k * 0.3f) + k) * M4::rotX(t * 0.7f + k) * M4::translate(V3(1.0f, 0, 0)) * M4::scale(V3(0.18f, 0.18f, 0.18f)), T_WHITE, col, 2.5f);
        glow(R, pos, col, 2.4f, 0.8f);
    }
}

// ------------------------------------------------------------------ world extras
void drawWorldExtras(Game& g, Renderer* R) {
    float t = g.time;
    Level& L = g.lv;
    // hacker terminals
    for (auto& T : L.terminals) {
        const HackPuzzle& P = L.puzzles[T.puzzle];
        static const uint32_t SLOT[6] = {0x40ff80, 0x40e0ff, 0xff50d0, 0xffe040, 0xff8030, 0xa070ff};
        V3 base = hexColor(SLOT[T.slot % 6]);
        V3 col = base * 0.25f; float emis = 1.0f;
        if (P.state == 3) { col = hexColor(0x40ff80) * 0.6f; emis = 1.4f; }
        if (T.flash > 0) {
            if (T.flashKind == 2) col = (std::fmod(t * 12, 1.f) < 0.5f) ? hexColor(0xff2020) : hexColor(0x400000);
            else if (T.flashKind == 1) col = hexColor(0xffffff);
            else col = base * 1.3f;
            emis = 3.0f;
        }
        M4 m = M4::translate(T.pos) * M4::rotY(T.yaw);
        R->drawPart(PM_CUBE, m * M4::translate(V3(0, 0, 0.05f)) * M4::scale(V3(0.95f, 0.75f, 0.05f)), T.flash > 0 ? T_WHITE : T_CODE, col, emis);
        // slot colour stripe so the order is easy to tell apart
        R->drawPart(PM_CUBE, m * M4::translate(V3(0, -0.42f, 0.04f)) * M4::scale(V3(0.95f, 0.08f, 0.05f)), T_WHITE, base, 1.5f);
        if (T.flash > 0) glow(R, T.pos + V3(std::sin(T.yaw), 0, std::cos(T.yaw)) * 0.3f, T.flashKind == 2 ? hexColor(0xff3030) : base, 1.6f, 0.7f);
    }
    // searchlights: a beam from the sky and a bright pool on the ground
    if (!R->shadowPass)
        for (auto& s : L.searchlights) {
            bool alarm = g.alarmT > 0;
            V3 c = alarm ? hexColor(0xffb0b0) : hexColor(0xf0f4ff);
            R->drawPart(PM_CYL, M4::translate(s.spot + V3(0, 0.05f, 0)) * M4::scale(V3(s.radius * 2, 0.04f, s.radius * 2)), T_WHITE, c, 1.2f, 0, 0.35f);
            V3 top = s.spot + V3(-8, 34, -6);
            V3 d = s.spot - top; float l = len(d);
            V3 dn = d / l;
            float yaw = std::atan2(dn.x, dn.z), pitch = std::acos(clampf(-dn.y, -1, 1));
            M4 bm = M4::translate((top + s.spot) * 0.5f) * M4::rotY(yaw) * M4::rotX(pitch) * M4::scale(V3(s.radius * 1.5f, l, s.radius * 1.5f));
            R->drawPart(PM_CONE, bm * M4::rotX(PI), T_WHITE, c, 0.8f, 0, 0.07f);
        }
    // the helipad holdout: an FBI helicopter circling, then Sprocket's rescue chopper
    if (L.hasHoldout) {
        const Holdout& H = L.holdout;
        auto chopper = [&](V3 p, float yaw, V3 body, bool rescue) {
            M4 m = M4::translate(p) * M4::rotY(yaw);
            R->drawPart(PM_SPHERE, m * M4::scale(V3(2.4f, 2.0f, 3.6f)), T_METAL, body);
            R->drawPart(PM_SPHERE, m * M4::translate(V3(0, 0.2f, 1.2f)) * M4::scale(V3(1.8f, 1.3f, 1.6f)), T_WINDOW, hexColor(0x80a0c0), 0.4f);
            R->drawPart(PM_CUBE, m * M4::translate(V3(0, 0.3f, -3.0f)) * M4::scale(V3(0.4f, 0.5f, 3.4f)), T_METAL, body);
            R->drawPart(PM_CUBE, m * M4::translate(V3(0.25f, 0.6f, -4.6f)) * M4::rotX(t * 30) * M4::scale(V3(0.05f, 1.6f, 0.15f)), T_METAL, V3(0.8f, 0.8f, 0.8f));
            R->drawPart(PM_CYL, m * M4::translate(V3(0, 1.3f, 0)) * M4::scale(V3(0.3f, 0.5f, 0.3f)), T_METAL, V3(1, 1, 1));
            for (int k = 0; k < 2; k++) R->drawPart(PM_CUBE, m * M4::translate(V3(0, 1.6f, 0)) * M4::rotY(t * 25 + k * PI / 2) * M4::scale(V3(9.0f, 0.06f, 0.35f)), T_METAL, hexColor(0x303038), 0, 0, 0.85f);
            for (int s = -1; s <= 1; s += 2) R->drawPart(PM_CUBE, m * M4::translate(V3(1.0f * s, -1.15f, 0)) * M4::scale(V3(0.12f, 0.12f, 3.4f)), T_METAL, V3(0.7f, 0.7f, 0.7f));
            if (rescue) {
                R->drawPart(PM_CUBE, m * M4::translate(V3(1.21f, 0.1f, 0)) * M4::scale(V3(0.02f, 0.6f, 1.6f)), T_WHITE, hexColor(0x40f0ff), 2.0f);
                R->drawPart(PM_SPHERE, m * M4::translate(V3(0, 2.2f, 0)) * M4::scale(V3(0.25f, 0.25f, 0.25f)), T_WHITE, hexColor(0xff4040), std::fmod(t, 1.f) < 0.5f ? 3.f : 0.5f);
            } else {
                bool blink = std::fmod(t * 3, 1.f) < 0.5f;
                R->drawPart(PM_CUBE, m * M4::translate(V3(-0.5f, -0.9f, 1.6f)) * M4::scale(V3(0.3f, 0.2f, 0.2f)), T_WHITE, hexColor(0xff3030), blink ? 3.f : 0.3f);
                R->drawPart(PM_CUBE, m * M4::translate(V3(0.5f, -0.9f, 1.6f)) * M4::scale(V3(0.3f, 0.2f, 0.2f)), T_WHITE, hexColor(0x3060ff), blink ? 0.3f : 3.f);
            }
        };
        if (H.active || (!H.done && g.alarmT > 0)) {
            float a = t * 0.35f;
            V3 p = H.pad + V3(std::sin(a) * 22, 14 + std::sin(t * 0.7f) * 1.5f, std::cos(a) * 22);
            chopper(p, a + PI / 2, hexColor(0x202028), false);
            if (!R->shadowPass) {  // its searchlight follows Wick
                V3 d = g.pl.pos - p; float l = len(d); V3 dn = d / l;
                M4 bm = M4::translate((p + g.pl.pos) * 0.5f) * M4::rotY(std::atan2(dn.x, dn.z)) * M4::rotX(std::acos(clampf(-dn.y, -1, 1))) * M4::scale(V3(3.2f, l, 3.2f));
                R->drawPart(PM_CONE, bm * M4::rotX(PI), T_WHITE, hexColor(0xf0f4ff), 0.8f, 0, 0.06f);
            }
        }
        if (H.done) {
            float k = clampf(H.rescueT / 4.f, 0, 1);
            V3 land = H.pad + V3(0, 1.4f, 5);
            V3 p = lerp(land + V3(-30, 26, 20), land, smooth01(k));
            chopper(p, PI + (1 - k) * 0.8f, hexColor(0x40a0b0), true);
        }
    }
    // firewalls
    for (auto& w : g.firewalls) {
        float life = clampf(std::min(w.t * 4, (w.life - w.t) * 1.5f), 0, 1);
        int n = 10;
        for (int i = 0; i <= n; i++) {
            V3 p = lerp(w.a, w.b, i / (float)n);
            float h = (1.4f + 0.5f * std::sin(t * 9 + i * 1.7f)) * life;
            R->drawPart(PM_OCTA, TRS(p + V3(0, h * 0.5f, 0), t * 3 + i, V3(0.5f, h, 0.5f)), T_WHITE, hexColor(0xff7020), 2.4f, 0, 0.9f);
            R->drawPart(PM_OCTA, TRS(p + V3(0, h * 0.35f, 0), -t * 4 + i, V3(0.3f, h * 0.6f, 0.3f)), T_WHITE, hexColor(0xffe080), 3.0f, 0, 0.9f);
        }
        R->drawPart(PM_CUBE, M4::translate((w.a + w.b) * 0.5f + V3(0, 0.03f, 0)) * M4::rotY(std::atan2(w.b.x - w.a.x, w.b.z - w.a.z)) * M4::scale(V3(0.5f, 0.06f, len(w.b - w.a))), T_WHITE, hexColor(0xff5010), 2, 0, life);
    }
    // the grapple rope
    if (g.grappling) {
        V3 hand = g.pl.pos + V3(0.2f, 1.0f, 0);
        V3 d = g.ropeAnchor - hand; float l = len(d);
        int seg = std::max(4, (int)(l / 0.35f));
        for (int i = 0; i < seg; i++) {
            float u = (i + 0.5f) / seg;
            V3 p = hand + d * u + V3(0, std::sin(u * PI) * std::sin(t * 20) * 0.05f, 0);
            R->drawPart(PM_CUBE, M4::translate(p) * M4::scale(V3(0.09f, 0.09f, 0.09f)), T_WHITE, (i % 2) ? hexColor(0xffa040) : hexColor(0xfff0a0), 2.5f);
        }
    }
    // the candle race rings
    if (!L.race.empty()) {
        for (int i = 0; i < (int)L.race.size(); i++) {
            const RaceRing& r = L.race[i];
            bool racing = g.raceIdx >= 0;
            bool next = racing && i == g.raceIdx, after = racing && i == g.raceIdx + 1, passed = racing && i < g.raceIdx;
            if (passed) continue;
            if (!racing && distXZ(r.pos, g.pl.pos) > 30) continue;
            V3 col = next ? hexColor(0xffd040) : (after ? hexColor(0xb08040) : hexColor(0x6a5030));
            float emis = next ? 2.5f : (racing ? 1.0f : 0.5f);
            float rr = 1.5f;
            int segs = 20;
            for (int k = 0; k < segs; k++) {
                float a = k * TAU / segs + (next ? t : 0);
                V3 off = V3(std::cos(a) * rr, std::sin(a) * rr, 0);
                M4 m = M4::translate(r.pos) * M4::rotY(r.yaw) * M4::translate(off) * M4::rotZ(a) * M4::scale(V3(0.14f, 0.5f, 0.14f));
                R->drawPart(PM_CUBE, m, T_WHITE, col, emis, 0, racing ? 1.f : 0.6f);
            }
            if (next) glow(R, r.pos, hexColor(0xffd040), 2.5f, 0.35f);
        }
    }
    // fishing line and bobber
    if (g.fishState > 0 && g.fishSpot >= 0 && g.fishSpot < (int)L.talkers.size()) {
        const Talker& tk = L.talkers[g.fishSpot];
        V3 fw(std::sin(tk.yaw), 0, std::cos(tk.yaw));
        V3 rodTip = g.pl.pos + fw * 0.9f + V3(0, 2.2f, 0);
        R->drawPart(PM_CUBE, M4::translate(g.pl.pos + fw * 0.45f + V3(0.2f, 1.6f, 0)) * M4::rotY(tk.yaw) * M4::rotX(0.7f) * M4::scale(V3(0.05f, 0.05f, 1.4f)), T_DARKWOOD, V3(1, 1, 1));
        V3 bob = tk.pos + fw * 3.2f; bob.y = L.waterY + (g.fishState == 2 ? -0.15f + 0.1f * std::sin(t * 30) : 0.05f * std::sin(t * 3));
        beam(R, rodTip, bob, 0.025f, hexColor(0xe0e0e0), 0.4f, 0.8f, 14);
        R->drawPart(PM_SPHERE, M4::translate(bob + V3(0, 0.1f, 0)) * M4::scale(V3(0.22f, 0.22f, 0.22f)), T_WHITE, hexColor(0xff4040), 1.2f);
    }
    // new projectiles
    for (auto& p : g.projs) {
        if (p.type == PR_CODE) {
            V3 d = len(p.vel) > 0.01f ? norm(p.vel) : V3(0, 0, 1);
            for (int k = 0; k < 3; k++) R->drawPart(PM_CUBE, M4::translate(p.pos - d * (k * 0.25f)) * M4::rotY(t * 8 + k) * M4::scale(V3(0.22f - k * 0.05f, 0.22f - k * 0.05f, 0.22f - k * 0.05f)), T_WHITE, hexColor(0x50ff90), 2.5f, 0, 1 - k * 0.25f);
        } else if (p.type == PR_BULLET) {
            V3 d = len(p.vel) > 0.01f ? norm(p.vel) : V3(0, 0, 1);
            R->drawPart(PM_CUBE, M4::translate(p.pos) * M4::rotY(std::atan2(d.x, d.z)) * M4::scale(V3(0.08f, 0.08f, 0.5f)), T_WHITE, hexColor(0xfff0a0), 3.0f);
        }
    }
}

// lights for the above (called before the shadow pass)
void extrasLights(Game& g, Renderer* R) {
    for (auto& w : g.firewalls) R->lights.push_back({(w.a + w.b) * 0.5f + V3(0, 1, 0), 8, hexColor(0xff8030), 1.6f});
    for (auto& T : g.lv.terminals) if (T.flash > 0) R->lights.push_back({T.pos, 4, T.flashKind == 2 ? hexColor(0xff3030) : hexColor(0x80ffc0), 1.2f});
    for (auto& s : g.lv.searchlights) R->lights.push_back({s.spot + V3(0, 2.5f, 0), s.radius * 2.2f, g.alarmT > 0 ? hexColor(0xffc0c0) : hexColor(0xe8f0ff), 1.8f});
    for (auto& e : g.enemies) {
        if (!e.alive) continue;
        if (e.type == E_AGENT && e.state == 0) R->lights.push_back({e.pos + V3(std::sin(e.yaw), 0, std::cos(e.yaw)) * 3.0f + V3(0, 0.5f, 0), 5, hexColor(0xfff0c0), 1.0f});
        if (e.type == E_VIRUS || e.type == E_DECOY) R->lights.push_back({e.pos + V3(0, 0.6f, 0), 3, hexColor(0x50ff90), 0.8f});
    }
    for (auto& p : g.projs) if (p.type == PR_CODE) R->lights.push_back({p.pos, 2.5f, hexColor(0x50ff90), 0.8f});
    if (g.grappling) R->lights.push_back({(g.ropeAnchor + g.pl.pos) * 0.5f, 6, hexColor(0xffa040), 1.0f});
    if (g.raceIdx >= 0 && g.raceIdx < (int)g.lv.race.size()) R->lights.push_back({g.lv.race[g.raceIdx].pos, 6, hexColor(0xffd040), 1.4f});
    if (g.lv.id == LV_CITY && g.alarmT > 0) {
        bool red = std::fmod(g.time * 2, 1.f) < 0.5f;
        R->lights.push_back({g.pl.pos + V3(0, 5, 0), 14, red ? hexColor(0xff2020) : hexColor(0x2040ff), 0.7f});
    }
}
