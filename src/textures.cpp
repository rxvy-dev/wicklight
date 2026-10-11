// WICKLIGHT - procedurally painted 16x16 pixel-art texture tiles.
// Alpha channel convention: 255 = opaque, ~180 = emissive pixel (glows), 0 = hole.
#include "render.h"

namespace {
struct Painter {
    std::vector<uint8_t>& img;
    int tx, ty;
    int seed;
    void px(int x, int y, V3 c, int a = 255) {
        x &= 15; y &= 15;
        int i = ((ty * 16 + y) * 256 + tx * 16 + x) * 4;
        img[i] = (uint8_t)clampf(c.x * 255, 0, 255);
        img[i + 1] = (uint8_t)clampf(c.y * 255, 0, 255);
        img[i + 2] = (uint8_t)clampf(c.z * 255, 0, 255);
        img[i + 3] = (uint8_t)a;
    }
    float n(int x, int y, int s = 0) const { return hashf(x, y, seed * 31 + s); }
    void fill(V3 c, float noise = 0.06f) {
        for (int y = 0; y < 16; y++)
            for (int x = 0; x < 16; x++) px(x, y, c * (1 - noise + 2 * noise * n(x, y)));
    }
};
V3 H(uint32_t h) { return hexColor(h); }
}  // namespace

std::vector<uint8_t> generateAtlas() {
    std::vector<uint8_t> img(256 * 256 * 4, 255);
    for (int t = 0; t < T_COUNT; t++) {
        Painter p{img, t % 16, t / 16, t * 7 + 3};
        switch (t) {
            case T_WHITE: p.fill(V3(1, 1, 1), 0.0f); break;
            case T_STONEBRICK: {
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++) {
                        int row = y / 4, off = (row & 1) * 4;
                        int bx = (x + off) / 8;
                        bool mortar = (y % 4 == 3) || ((x + off) % 8 == 7);
                        float shade = 0.85f + 0.3f * hashf(bx, row, 9);
                        V3 c = mortar ? H(0x2c2a36) : H(0x6b6f80) * shade * (0.92f + 0.12f * p.n(x, y));
                        if (!mortar && (y % 4 == 0)) c = c * 1.12f;
                        p.px(x, y, c);
                    }
            } break;
            case T_COBBLE: {
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++) {
                        // simple voronoi
                        float best = 99, second = 99; int id = 0;
                        for (int k = 0; k < 7; k++) {
                            float cx = hashf(k, 1, 77) * 16, cy = hashf(k, 2, 77) * 16;
                            for (int ox = -16; ox <= 16; ox += 16)
                                for (int oy = -16; oy <= 16; oy += 16) {
                                    float d = (x - cx - ox) * (x - cx - ox) + (y - cy - oy) * (y - cy - oy);
                                    if (d < best) { second = best; best = d; id = k; }
                                    else if (d < second) second = d;
                                }
                        }
                        bool edge = std::sqrt(second) - std::sqrt(best) < 1.2f;
                        V3 c = edge ? H(0x262430) : H(0x5d5a66) * (0.8f + 0.35f * hashf(id, 3, 5)) * (0.94f + 0.1f * p.n(x, y));
                        p.px(x, y, c);
                    }
            } break;
            case T_PLANK: case T_DARKWOOD: {
                V3 base = t == T_PLANK ? H(0x8a5a33) : H(0x4a2f22);
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++) {
                        int plank = y / 4;
                        bool gap = y % 4 == 3;
                        float grain = (hashf(x / 3, y, 4) > 0.75f) ? 0.85f : 1.f;
                        V3 c = gap ? base * 0.45f : base * (0.85f + 0.25f * hashf(plank, 0, 11)) * grain;
                        if (!gap && x == ((plank * 5 + 2) & 15) && y % 4 == 1) c = H(0x2a2a2a);
                        p.px(x, y, c);
                    }
            } break;
            case T_METAL: {
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++) {
                        V3 c = H(0x56606e) * (0.92f + 0.12f * p.n(x, y));
                        if (x == 0 || y == 0) c = c * 1.3f;
                        if (x == 15 || y == 15) c = c * 0.55f;
                        if ((x == 2 || x == 13) && (y == 2 || y == 13)) c = H(0xa8b0bc);
                        p.px(x, y, c);
                    }
            } break;
            case T_RUST: {
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++) {
                        float v = p.n(x, y) * 0.6f + p.n(x / 2, y / 2, 3) * 0.4f;
                        V3 c = v > 0.55f ? H(0x8c4a26) : H(0x5a3a2e);
                        if (v > 0.85f) c = H(0xb5662e);
                        p.px(x, y, c * (0.9f + 0.15f * p.n(x, y, 2)));
                    }
            } break;
            case T_MOSS: {
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++) {
                        float v = p.n(x, y) * 0.5f + p.n(x / 3, y / 3, 5) * 0.5f;
                        V3 c = v > 0.5f ? H(0x3f6b3a) : H(0x2c4a2e);
                        if (v > 0.82f) c = H(0x6b9a4a);
                        p.px(x, y, c);
                    }
            } break;
            case T_GRASS: {
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++) {
                        V3 c = H(0x35593a) * (0.85f + 0.25f * p.n(x, y));
                        if (p.n(x, y, 7) > 0.8f) c = H(0x5c8a4a);
                        if (p.n(x, y, 8) > 0.97f) c = H(0xc9b45a);
                        p.px(x, y, c);
                    }
            } break;
            case T_DIRT: {
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++) {
                        V3 c = H(0x4f3a2c) * (0.8f + 0.3f * p.n(x, y));
                        if (p.n(x, y, 4) > 0.93f) c = H(0x7d6b5a);
                        p.px(x, y, c);
                    }
            } break;
            case T_SAND: {
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++) {
                        V3 c = H(0x9c8a64) * (0.88f + 0.18f * p.n(x, y));
                        if (p.n(x, y, 4) > 0.94f) c = H(0x6e6048);
                        p.px(x, y, c);
                    }
            } break;
            case T_LEAVES: {
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++) {
                        float v = p.n(x / 2, y / 2) * 0.7f + p.n(x, y, 2) * 0.3f;
                        V3 c = v > 0.6f ? H(0x3c6e44) : (v > 0.3f ? H(0x2a5236) : H(0x1b3626));
                        if (v > 0.9f) c = H(0x6fa860);
                        p.px(x, y, c);
                    }
            } break;
            case T_BARK: {
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++) {
                        bool line = hashf(x, y / 5, 6) > 0.7f;
                        V3 c = H(0x4d3a2c) * (line ? 0.6f : (0.9f + 0.2f * p.n(x, y)));
                        p.px(x, y, c);
                    }
            } break;
            case T_WAX: {
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++) {
                        V3 c = H(0xe8dcc0) * (0.95f + 0.06f * p.n(x, y));
                        int drip = int(hashf(x, 0, 3) * 10);
                        if (y < drip && hashf(x, 1, 3) > 0.6f) c = H(0xfff4dc);
                        p.px(x, y, c);
                    }
            } break;
            case T_ROOF: {
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++) {
                        int row = y / 4, off = (row & 1) * 2;
                        int lx = (x + off) % 4;
                        bool edge = (y % 4 == 3) || (lx == 0 && y % 4 > 1);
                        V3 c = edge ? H(0x3a1c1c) : H(0x7a3328) * (0.85f + 0.25f * hashf((x + off) / 4, row, 3));
                        p.px(x, y, c);
                    }
            } break;
            case T_BRASS: {
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++) {
                        float band = 0.8f + 0.3f * std::sin((x + y) * 0.6f);
                        V3 c = H(0xb08a3c) * band * (0.95f + 0.08f * p.n(x, y));
                        if (y == 0 || y == 15) c = H(0x6a4e22);
                        p.px(x, y, c);
                    }
            } break;
            case T_BONE: {
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++) {
                        V3 c = H(0xcfc4a8) * (0.9f + 0.12f * p.n(x, y));
                        if (p.n(x, y / 3, 3) > 0.94f) c = H(0x7a705a);
                        p.px(x, y, c);
                    }
            } break;
            case T_CLOTH: {
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++) {
                        bool weave = ((x + y) & 1) == 0;
                        V3 c = H(0x8a2a2e) * (weave ? 1.0f : 0.82f);
                        if (y == 13) c = H(0xc49a3c);
                        p.px(x, y, c);
                    }
            } break;
            case T_CRYSTAL: {
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++) {
                        bool facet = (x + y) % 7 == 0 || (x - y + 16) % 9 == 0;
                        V3 c = facet ? H(0xd8fbff) : H(0x4fb8d0) * (0.8f + 0.3f * p.n(x / 2, y / 2));
                        p.px(x, y, c, 180);
                    }
            } break;
            case T_TILEFLOOR: {
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++) {
                        bool chk = ((x / 8) + (y / 8)) & 1;
                        bool grout = x % 8 == 7 || y % 8 == 7;
                        V3 c = grout ? H(0x2a2630) : (chk ? H(0x6c6478) : H(0x8e8696)) * (0.94f + 0.08f * p.n(x, y));
                        p.px(x, y, c);
                    }
            } break;
            case T_CARPET: {
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++) {
                        int dx = std::abs(x - 8), dy = std::abs(y - 8);
                        V3 c = H(0x5a1a28);
                        if (dx + dy == 5) c = H(0xc09a40);
                        if (dx + dy < 2) c = H(0xc09a40);
                        p.px(x, y, c * (0.95f + 0.07f * p.n(x, y)));
                    }
            } break;
            case T_ASH: {
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++) {
                        V3 c = H(0x3a3438) * (0.8f + 0.3f * p.n(x, y));
                        int a = 255;
                        if (p.n(x, y, 9) > 0.96f) { c = H(0xff7a2a); a = 180; }
                        p.px(x, y, c, a);
                    }
            } break;
            case T_RUNE: {
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++) {
                        V3 c = H(0x34303e) * (0.9f + 0.15f * p.n(x, y));
                        int a = 255;
                        bool rune = (x == 7 && y > 2 && y < 13) || (y == 4 && x > 4 && x < 11) || (x - 4 == y - 8 && x > 3 && x < 8) ||
                                    (11 - x == y - 8 && x > 7 && x < 12);
                        if (rune) { c = H(0x7ae8ff); a = 180; }
                        p.px(x, y, c, a);
                    }
            } break;
            case T_TAR: {
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++) {
                        V3 c = H(0x141018) * (0.9f + 0.2f * p.n(x, y));
                        if (p.n(x / 2, y / 2, 4) > 0.85f) c = H(0x4a2a5a);
                        p.px(x, y, c);
                    }
            } break;
            case T_WINDOW: {
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++) {
                        bool frame = x < 2 || x > 13 || y < 2 || y > 13 || x == 7 || x == 8 || y == 7;
                        V3 c = frame ? H(0x2a1e18) : H(0xffc060) * (0.85f + 0.2f * p.n(x, y));
                        p.px(x, y, c, frame ? 255 : 180);
                    }
            } break;
            case T_ROCK: {
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++) {
                        float v = p.n(x / 2, y / 2) * 0.6f + p.n(x, y) * 0.4f;
                        V3 c = H(0x5a5450) * (0.75f + 0.4f * v);
                        if (hashf(x + y * 2, 0, 8) > 0.9f && y % 5 == 2) c = H(0x2a2626);
                        p.px(x, y, c);
                    }
            } break;
            case T_MARBLE: {
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++) {
                        float vein = std::sin(x * 0.7f + y * 0.35f + p.n(x / 4, y / 4) * 4);
                        V3 c = H(0xd4d0d8) * (0.92f + 0.06f * p.n(x, y));
                        if (std::fabs(vein) < 0.12f) c = H(0x8a8496);
                        p.px(x, y, c);
                    }
            } break;
            case T_OBSIDIAN: {
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++) {
                        V3 c = H(0x1a1424) * (0.85f + 0.3f * p.n(x, y));
                        if ((x * 3 + y * 5) % 17 == 0) c = H(0x5a3a7a);
                        p.px(x, y, c);
                    }
            } break;
            case T_BARNACLE: {
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++) {
                        V3 c = H(0x3c5058) * (0.85f + 0.2f * p.n(x, y));
                        for (int k = 0; k < 5; k++) {
                            float cx = hashf(k, 0, 21) * 16, cy = hashf(k, 1, 21) * 16;
                            float d = std::sqrt((x - cx) * (x - cx) + (y - cy) * (y - cy));
                            if (d < 2.6f && d > 1.3f) c = H(0xa8b4a8);
                            else if (d <= 1.3f) c = H(0x1a2226);
                        }
                        p.px(x, y, c);
                    }
            } break;
            case T_CLOCKFACE: {
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++) {
                        float dx = x - 7.5f, dy = y - 7.5f, d = std::sqrt(dx * dx + dy * dy);
                        V3 c = H(0xe8dfc0); int a = 180;
                        if (d > 7.6f) { c = H(0x6a4e22); a = 255; }
                        else if (d > 6.6f) { c = H(0xb08a3c); a = 255; }
                        else {
                            float ang = std::atan2(dy, dx);
                            float tick = std::fmod(ang + TAU, TAU / 12);
                            if (d > 4.8f && (tick < 0.12f || tick > TAU / 12 - 0.12f)) { c = H(0x2a2018); a = 255; }
                            if ((x == 7 || x == 8) && y >= 3 && y <= 8) { c = H(0x2a2018); a = 255; }
                            if ((y == 7 || y == 8) && x >= 7 && x <= 11) { c = H(0x2a2018); a = 255; }
                        }
                        p.px(x, y, c, a);
                    }
            } break;
            case T_BOOKS: {
                static const uint32_t cols[] = {0x7a2a2a, 0x2a4a7a, 0x3a6a3a, 0x7a6a2a, 0x5a2a6a, 0x2a2a2a};
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++) {
                        bool shelf = y == 7 || y == 15;
                        int b = (x / 2) + (y / 8) * 3;
                        V3 c = shelf ? H(0x3a2618) : H(cols[hash32(b * 3 + 1) % 6]) * (y % 8 == 2 ? 1.3f : 1.0f);
                        if (!shelf && x % 2 == 1 && hashf(b, 1, 1) > 0.5f) c = c * 0.7f;
                        p.px(x, y, c);
                    }
            } break;
            case T_ROPE: {
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++) {
                        bool tw = ((x + y) % 4) < 2;
                        p.px(x, y, H(0x9a7a4a) * (tw ? 1.0f : 0.72f));
                    }
            } break;
            case T_SHADOW: {
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++) {
                        V3 c = H(0x0c0812) * (0.8f + 0.4f * p.n(x, y));
                        int a = 255;
                        if (p.n(x, y, 3) > 0.94f) { c = H(0x9a5aff); a = 180; }
                        p.px(x, y, c, a);
                    }
            } break;
            case T_LAVA: {
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++) {
                        float v = p.n(x / 2, y / 2) * 0.6f + p.n(x, y) * 0.4f;
                        V3 c = v > 0.55f ? H(0xff8a2a) : (v > 0.35f ? H(0xc23a1a) : H(0x3a1a14));
                        p.px(x, y, c, v > 0.35f ? 180 : 255);
                    }
            } break;
            case T_GRATE: {
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++) {
                        bool bar = x % 4 < 1 || y % 8 < 1;
                        p.px(x, y, H(0x3e3a44) * (0.9f + 0.2f * p.n(x, y)), bar ? 255 : 0);
                    }
            } break;
            case T_IVY: {
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++) {
                        float v = p.n(x / 2, y / 2);
                        bool leaf = v > 0.45f;
                        p.px(x, y, (v > 0.75f ? H(0x5a8a4a) : H(0x2e5a34)), leaf ? 255 : 0);
                    }
            } break;
            case T_PLASTER: {
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++) {
                        V3 c = H(0x8c8070) * (0.92f + 0.1f * p.n(x, y));
                        if (p.n(x / 2, y, 4) > 0.93f) c = H(0x5a5048);
                        p.px(x, y, c);
                    }
            } break;
            case T_GOLD: {
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++) {
                        V3 c = H(0xe0b84a) * (0.85f + 0.25f * p.n(x / 2, y / 2));
                        if ((x + y) % 6 == 0) c = H(0xfff0a0);
                        p.px(x, y, c);
                    }
            } break;
            case T_SEAL: {  // cracked wax seal (breakable by Bloom)
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++) {
                        float dx = x - 7.5f, dy = y - 7.5f, d = std::sqrt(dx * dx + dy * dy);
                        V3 c = H(0x9a2a24) * (0.85f + 0.2f * p.n(x, y));
                        if (d < 4.5f && d > 3.2f) c = H(0xd04a3a);
                        if (std::abs(x - y) == 0 && d < 3) c = H(0xd04a3a);
                        if (hashf(x, y, 33) > 0.93f) c = H(0x4a1010);
                        p.px(x, y, c);
                    }
            } break;
            case T_EYE: {
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++) {
                        float dx = x - 7.5f, dy = (y - 7.5f) * 1.6f, d = std::sqrt(dx * dx + dy * dy);
                        V3 c = H(0x120a18); int a = 255;
                        if (d < 7) { c = H(0xe8d0ff); a = 180; }
                        if (d < 4) { c = H(0xa040ff); a = 180; }
                        if (d < 1.8f) { c = H(0x080008); a = 255; }
                        p.px(x, y, c, a);
                    }
            } break;
            case T_DOOR: {
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++) {
                        V3 c = H(0x4a3020) * (x % 4 == 0 ? 0.6f : (0.9f + 0.15f * p.n(x, y)));
                        if (y == 3 || y == 12) c = H(0x3a3a44);
                        p.px(x, y, c);
                    }
            } break;
            case T_PIPE: {
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++) {
                        float sh = 0.6f + 0.6f * std::sin(x / 15.f * PI);
                        V3 c = H(0x7a6a50) * sh;
                        if (y % 8 == 0) c = H(0xb09a60);
                        p.px(x, y, c);
                    }
            } break;
            case T_FLESHGLOW: {
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++) {
                        float v = p.n(x / 2, y / 2);
                        V3 c = v > 0.5f ? H(0xffb050) : H(0xff6a2a);
                        p.px(x, y, c, 180);
                    }
            } break;
            case T_STAR: {
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++) {
                        float dx = std::fabs(x - 7.5f), dy = std::fabs(y - 7.5f);
                        bool on = dx * dy < 3.0f && dx + dy < 8;
                        p.px(x, y, on ? H(0xfff2c0) : H(0x000000), on ? 180 : 0);
                    }
            } break;
            // ---------------------------------------------------------------- Wick's Island blocks
            case T_STONE: {
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++) {
                        float v = p.n(x / 3, y / 2) * 0.5f + p.n(x, y) * 0.5f;
                        V3 c = H(0x6e6c78) * (0.82f + 0.3f * v);
                        if (p.n(x, y, 3) > 0.93f) c = H(0x4a4854);
                        p.px(x, y, c);
                    }
            } break;
            case T_GRASS_SIDE: {
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++) {
                        int edge = 3 + (int)(hashf(x, 0, 11) * 3);
                        V3 c = y < edge ? H(0x40683f) * (0.85f + 0.25f * p.n(x, y)) : H(0x5a4230) * (0.8f + 0.3f * p.n(x, y));
                        if (y >= edge && p.n(x, y, 4) > 0.93f) c = H(0x7d6b5a);
                        p.px(x, y, c);
                    }
            } break;
            case T_LOG_TOP: {
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++) {
                        float r = std::sqrt((x - 7.5f) * (x - 7.5f) + (y - 7.5f) * (y - 7.5f));
                        V3 c = r > 6.6f ? H(0x4d3a2c) : (((int)r) % 2 ? H(0xa98458) : H(0x8a6a44));
                        p.px(x, y, c * (0.92f + 0.1f * p.n(x, y)));
                    }
            } break;
            case T_GLASS: {
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++) {
                        bool frame = x == 0 || y == 0 || x == 15 || y == 15;
                        bool shine = (x - y == 3 || x - y == 4) && x > 4 && x < 12;
                        if (frame) p.px(x, y, H(0xc8e4ec));
                        else if (shine) p.px(x, y, H(0xffffff));
                        else p.px(x, y, H(0x000000), 0);
                    }
            } break;
            case T_ORE_EMBER: case T_ORE_CRYSTAL: case T_ORE_SCRAP: {
                uint32_t oc = t == T_ORE_EMBER ? 0xff7a2a : (t == T_ORE_CRYSTAL ? 0x7ae8ff : 0xb8b0a0);
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++) {
                        float v = p.n(x / 3, y / 2) * 0.5f + p.n(x, y) * 0.5f;
                        V3 c = H(0x6e6c78) * (0.82f + 0.3f * v);
                        bool ore = hashf(x / 3, y / 3, 50 + t) > 0.55f && hashf(x, y, 60 + t) > 0.35f;
                        if (ore) { p.px(x, y, H(oc) * (0.8f + 0.3f * p.n(x, y, 5)), (t == T_ORE_SCRAP ? 255 : 180)); continue; }
                        p.px(x, y, c);
                    }
            } break;
            case T_LAMPBLOCK: {
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++) {
                        bool frame = x < 2 || y < 2 || x > 13 || y > 13 || x == 7 || x == 8 || y == 7 || y == 8;
                        if (frame) p.px(x, y, H(0x5a4a34) * (0.9f + 0.15f * p.n(x, y)));
                        else p.px(x, y, H(0xffd890) * (0.9f + 0.12f * p.n(x, y)), 180);
                    }
            } break;
            case T_CHEST: {
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++) {
                        V3 c = H(0x8a5e34) * (x % 5 == 0 ? 0.7f : (0.92f + 0.12f * p.n(x, y)));
                        if (y == 5 || y == 6) c = H(0x4a3020);
                        if (x == 0 || x == 15 || y == 0 || y == 15) c = H(0x4a3020);
                        if (x >= 7 && x <= 8 && y >= 4 && y <= 8) c = H(0xd8c070);
                        p.px(x, y, c);
                    }
            } break;
            case T_BENCH_TOP: {
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++) {
                        V3 c = H(0x9a7448) * (0.9f + 0.15f * p.n(x, y));
                        if (x == 0 || y == 0 || x == 15 || y == 15 || x == 5 || x == 10 || y == 5 || y == 10) c = H(0x5a3e24);
                        p.px(x, y, c);
                    }
            } break;
            case T_BENCH_SIDE: {
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++) {
                        V3 c = H(0x8a6a44) * (x % 4 == 0 ? 0.75f : (0.9f + 0.15f * p.n(x, y)));
                        if (y < 3) c = H(0x5a3e24);
                        if ((x == 3 && y > 4 && y < 12) || (y == 5 && x > 1 && x < 6)) c = H(0xa0a0a8);    // hammer
                        if ((x == 11 && y > 4 && y < 13) || (x == 12 && y > 4 && y < 8)) c = H(0xc8c8d0);  // saw
                        p.px(x, y, c);
                    }
            } break;
            case T_FURNACE: case T_FORGE: {
                bool forge = t == T_FORGE;
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++) {
                        V3 c = H(forge ? 0x4a4450 : 0x5d5a66) * (0.85f + 0.25f * p.n(x / 2, y / 2));
                        if (y % 4 == 3 || (x + (y / 4) * 3) % 6 == 0) c = H(0x2c2a36);
                        bool mouth = x >= 4 && x <= 11 && y >= 8 && y <= 13;
                        if (mouth) { p.px(x, y, (forge ? H(0xff8030) : H(0xff6020)) * (0.7f + 0.5f * p.n(x, y, 9)), 180); continue; }
                        if (forge && y < 3 && x > 3 && x < 12) { p.px(x, y, H(0xffc060), 180); continue; }
                        p.px(x, y, c);
                    }
            } break;
            case T_LADDER: {
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++) {
                        bool rail = x == 2 || x == 3 || x == 12 || x == 13;
                        bool rung = (y % 5 == 2) && x > 1 && x < 14;
                        if (rail || rung) p.px(x, y, H(0x8a6a44) * (0.85f + 0.2f * p.n(x, y)));
                        else p.px(x, y, H(0), 0);
                    }
            } break;
            case T_JUMPPAD: {
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++) {
                        float r = std::sqrt((x - 7.5f) * (x - 7.5f) + (y - 7.5f) * (y - 7.5f));
                        V3 c = H(0x707884);
                        bool arrow = (y >= 3 && y <= 12 && x >= 7 && x <= 8) || (y >= 3 && y <= 6 && std::abs(x - 7.5f) <= 6.5f - y);
                        if (r < 7) c = H(0x40e080);
                        if (arrow) { p.px(x, y, H(0xffffff), 180); continue; }
                        p.px(x, y, c, r < 7 ? 180 : 255);
                    }
            } break;
            case T_BOMB: {
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++) {
                        V3 c = H(0xb02a2a) * (0.9f + 0.12f * p.n(x, y));
                        if (y == 4 || y == 11) c = H(0xe8dcc0);
                        if (y > 5 && y < 10 && x > 2 && x < 13) c = H(0xe8dcc0);
                        if (y > 6 && y < 9 && x > 3 && x < 12 && ((x / 2) % 2)) c = H(0xff7a2a);
                        p.px(x, y, c);
                    }
            } break;
            case T_SAPLING: case T_FLOWER_RED: case T_FLOWER_YELLOW: {
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++) p.px(x, y, H(0), 0);
                if (t == T_SAPLING) {
                    for (int y = 6; y < 16; y++) p.px(7, y, H(0x5a4230));
                    for (int y = 1; y < 10; y++)
                        for (int x = 2; x < 14; x++) {
                            float d = std::fabs(x - 7.5f) / 6 + std::fabs(y - 5.f) / 5;
                            if (d < 1 && hashf(x, y, 21) > 0.25f) p.px(x, y, H(0x3c7a44) * (0.8f + 0.4f * hashf(x, y, 22)));
                        }
                } else {
                    uint32_t pc = t == T_FLOWER_RED ? 0xe0405a : 0xf0d040;
                    for (int y = 7; y < 16; y++) p.px(7, y, H(0x3c7a44));
                    p.px(6, 11, H(0x3c7a44)); p.px(5, 10, H(0x3c7a44)); p.px(8, 12, H(0x3c7a44)); p.px(9, 11, H(0x3c7a44));
                    for (int y = 2; y < 8; y++)
                        for (int x = 4; x < 12; x++) {
                            float d = (x - 7.5f) * (x - 7.5f) + (y - 4.5f) * (y - 4.5f);
                            if (d < 9) p.px(x, y, d < 1.6f ? H(0xffe8a0) : H(pc) * (0.85f + 0.2f * hashf(x, y, 23)));
                        }
                }
            } break;
            case T_GEAR_ICON: case T_WAXLUMP_ICON: case T_FLAME_ICON: case T_CRYSTAL_ICON: case T_BED_ICON: case T_DOOR_ICON: case T_CANDLE_ICON: {
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++) {
                        float dx = x - 7.5f, dy = y - 7.5f, r = std::sqrt(dx * dx + dy * dy);
                        V3 c(0, 0, 0); int a = 0;
                        if (t == T_GEAR_ICON) {
                            float ang = std::atan2(dy, dx);
                            float rr = 5.5f + (std::fmod(ang + PI, PI / 4) < PI / 8 ? 1.5f : 0.f);
                            if (r < rr && r > 2.2f) { c = H(0xb8b0a0) * (0.8f + 0.25f * (dx < 0)); a = 255; }
                        } else if (t == T_WAXLUMP_ICON) {
                            if (std::fabs(dx) / 6 + std::fabs(dy - 1) / 5 < 1 || (std::fabs(dx) < 3 && dy > -5 && dy < 0)) { c = H(0xe8dcc0) * (0.85f + 0.2f * (dx < 0)); a = 255; }
                        } else if (t == T_FLAME_ICON) {
                            float w = (y - 1) / 15.f;
                            if (y > 1 && std::fabs(dx) < 5.5f * std::sin(w * PI * 0.95f)) { c = y > 9 ? H(0xff8a2a) : H(0xffe070); if (std::fabs(dx) < 1.5f && y > 7) c = H(0xffffff); a = 180; }
                        } else if (t == T_CRYSTAL_ICON) {
                            if (std::fabs(dx) * 2.2f + std::fabs(dy) < 8) { c = std::fabs(dx) < 1 ? H(0xd8fbff) : H(0x4fb8d0); a = 180; }
                        } else if (t == T_BED_ICON) {
                            if (y > 6 && y < 13 && x > 0 && x < 15) { c = x < 5 ? H(0xf0eee0) : H(0xc03a3a); if (y > 10) c = H(0x8a6a44); a = 255; }
                        } else if (t == T_DOOR_ICON) {
                            if (x > 3 && x < 12) { c = H(0x8a5e34) * ((x % 3 == 0) ? 0.75f : 1.f); if (x == 10 && y == 8) c = H(0xd8c070); a = 255; }
                        } else if (t == T_CANDLE_ICON) {
                            if (x > 5 && x < 10 && y > 6) { c = H(0xe8dcc0); a = 255; }
                            if (std::fabs(dx) < 1.5f && y > 1 && y < 6) { c = H(0xffb040); a = 180; }
                        }
                        p.px(x, y, c, a);
                    }
            } break;
            case T_CRACK: {
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++) {
                        bool crack = hashf(x / 2, y, 77) > 0.8f || hashf(x, y / 2, 78) > 0.82f;
                        p.px(x, y, H(0x101010), crack ? 255 : 0);
                    }
            } break;
            case T_CODE: {  // green glyphs on a black screen (emissive)
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++) {
                        int col = x / 2, row = y;
                        bool glyph = (x % 2 == 0) && hashf(col, row, 41) > 0.45f && hashf(col, 0, 42) > 0.2f;
                        float fade = 0.35f + 0.65f * std::fmod(row / 16.f + hashf(col, 1, 43), 1.f);
                        if (glyph) p.px(x, y, H(0x50ff90) * fade, 180);
                        else p.px(x, y, H(0x06140a));
                    }
            } break;
            case T_ASPHALT: {
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++) {
                        float g = 0.85f + 0.3f * p.n(x, y);
                        V3 c = H(0x2a2a30) * g;
                        if (p.n(x, y, 3) > 0.93f) c = H(0x4a4a52);
                        p.px(x, y, c);
                    }
            } break;
            case T_BRICK: {
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++) {
                        int row = y / 4, off = (row & 1) * 4;
                        bool mortar = (y % 4 == 3) || ((x + off) % 8 == 7);
                        float shade = 0.8f + 0.35f * hashf((x + off) / 8, row, 19);
                        p.px(x, y, mortar ? H(0x3a3030) : H(0x8a3a2e) * shade * (0.92f + 0.12f * p.n(x, y)));
                    }
            } break;
            case T_OFFICE: {  // tower wall: grid of windows, some lit
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++) {
                        bool frame = (x % 8) < 1 || (y % 8) < 2;
                        int wx = x / 8, wy = y / 8;
                        bool lit = hashf(wx, wy, 55) > 0.55f;
                        if (frame) p.px(x, y, H(0x30343e) * (0.9f + 0.15f * p.n(x, y)));
                        else if (lit) p.px(x, y, (hashf(wx, wy, 56) > 0.5f ? H(0xffe0a0) : H(0xa0d0ff)) * (0.85f + 0.15f * p.n(x, y)), 180);
                        else p.px(x, y, H(0x141c2a));
                    }
            } break;
            default: p.fill(V3(1, 0, 1), 0); break;
        }
    }
    return img;
}
