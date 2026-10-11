// WICKLIGHT - autoplay campaign test.
// Plays a fresh save from New Game to the victory screen through the real game simulation:
// the same physics, flame, enemies, bosses, gates and saves the player uses. The bot aims
// perfectly and walks scripted routes; damage is counted (god mode only prevents deaths so a
// run can report how much damage each section dealt).
#include "game.h"
#include <cstdio>
#include <cstdlib>
#include <algorithm>

enum Op { BUYBOT, GOTO, THROW, CHARGE, RECALL, WARP, BLOOM, WAIT, FIGHT, BOSS, TALK, EXPECT, EXPECT_LEVEL, WAITLIFT_DOWN, WAITLIFT_UP, NOTE };
struct Cmd { Op op; V3 p; float t; std::string s; };

static Cmd G(float x, float y, float z) { return {GOTO, V3(x, y, z), 0, ""}; }
static Cmd T(float x, float y, float z) { return {THROW, V3(x, y, z), 0, ""}; }
static Cmd TC(float x, float y, float z) { return {CHARGE, V3(x, y, z), 0, ""}; }
static Cmd TW(float x, float y, float z) { return {THROW, V3(x, y, z), 1, ""}; }  // throw then warp
static Cmd R_() { return {RECALL, V3(), 0, ""}; }
static Cmd B() { return {BLOOM, V3(), 0, ""}; }
static Cmd W(float t) { return {WAIT, V3(), t, ""}; }
static Cmd F(float radius = 22) { return {FIGHT, V3(), radius, ""}; }
static Cmd BOSSF() { return {BOSS, V3(), 0, ""}; }
static Cmd TALKC() { return {TALK, V3(), 0, ""}; }
static Cmd E(const char* f) { return {EXPECT, V3(), 0, f}; }
static Cmd EL(int lv) { return {EXPECT_LEVEL, V3(), (float)lv, ""}; }
static Cmd N(const char* s) { return {NOTE, V3(), 0, s}; }
static Cmd BUY(int i) { return {BUYBOT, V3(), (float)i, ""}; }

struct Bot {
    Game& g;
    std::vector<Cmd> cmds;
    size_t pc = 0;
    float t = 0, stuck = 0, total = 0;
    int phase = 0;
    InputState in;
    bool holdThrow = false;
    int failures = 0;
    int startLevel = -1;
    bool sawFlight = false;
    std::string section = "start";
    float dmgAtSection = 0;
    explicit Bot(Game& gg) : g(gg) {}

    void face(V3 dir) { if (lenXZ(dir) > 0.01f) g.camYaw = std::atan2(dir.x, dir.z); }
    void aim(V3 p) { g.botAim = true; g.botAimPoint = p; }
    void press(Action a) { in.pressed[a] = true; in.down[a] = true; }

    // walk toward p; returns true when arrived
    bool airOk = true;  // the endless test sets this false so hops land before moving on
    bool walk(V3 p, float tol = 0.7f) {
        V3 d = flatXZ(p - g.pl.pos);
        if (lenXZ(d) < tol && (g.pl.grounded || airOk)) return true;
        face(d);
        float dist = lenXZ(d);
        in.moveY = g.pl.grounded ? clampf(dist / 2.0f, 0.3f, 1.f) : (dist < (airOk ? 0.6f : 1.0f) ? (airOk ? -0.3f : -0.5f) : 1.f);  // brake in the air over the target so hops don't overshoot  // brake in the air near the target
        // brake: cancel excess speed near the waypoint
        if (g.pl.grounded && dist < 1.5f && lenXZ(g.pl.vel) > 3) in.moveY = 0;
        // jump over gaps or onto blocks
        V3 dir = norm(d);
        if (g.pl.grounded) {
            float ahead = g.raycast(g.pl.pos + dir * 0.5f + V3(0, 0.4f, 0), V3(0, -1, 0), 1.6f);
            bool targetLower = p.y < g.pl.pos.y - 0.6f;
            if (ahead < 0 && !targetLower && lenXZ(d) > 1.2f) press(A_JUMP);
            if (lenXZ(g.pl.vel) < 1.0f) { stuck += SIM_DT; if (stuck > 0.15f) { press(A_JUMP); stuck = 0; } }
            else stuck = 0;
        }
        return false;
    }

    Enemy* nearestEnemy(float radius) {
        Enemy* best = nullptr; float bd = radius;
        int activeArena = -1;
        for (int i = 0; i < (int)g.lv.arenas.size(); i++) if (g.lv.arenas[i].active) activeArena = i;
        if (activeArena >= 0) bd = 1e9f;
        for (auto& e : g.enemies) {
            if (!e.alive) continue;
            if (activeArena >= 0 && e.arena != activeArena) continue;
            float d = len(e.pos - g.pl.pos);
            if (d >= bd) continue;
            V3 head = g.pl.pos + V3(0, 1.25f, 0), c = e.pos + V3(0, e.height * 0.5f, 0);
            float dl = len(c - head);
            float hit = g.raycast(head, (c - head) / dl, dl);
            if (activeArena < 0 && hit >= 0 && hit < dl - 0.8f) continue;  // out of sight
            bd = d; best = &e;
        }
        return best;
    }

    // one throw/recall cycle at a point (overshooting so the return pass hits as well)
    float cycT = 0; int cyc = 0;
    void attack(V3 target, bool charged = false, float overshoot = 2.5f) {
        V3 head = g.pl.pos + V3(0, 1.25f, 0);
        V3 dir = norm(target - head);
        V3 ap = target + dir * overshoot;
        aim(ap);
        face(dir);
        cycT += SIM_DT;
        if (g.flame.st == F_HELD) {
            if (charged && g.ability("sunflare")) {
                if (!g.flame.charging) { if (cycT > 0.1f) { press(A_THROW); cycT = 0; } }
                else if (g.flame.charge > 0.8f) { /* release */ }
                else in.down[A_THROW] = true;
            } else if (cycT > 0.12f) { press(A_THROW); cycT = 0; }
        } else if (g.flame.st == F_HOVER) {
            if (cycT > 0.15f) { press(A_THROW); cycT = 0; }  // recall
        }
    }

    void dodgeRings() {
        for (auto& r : g.rings) {
            float d = distXZ(g.pl.pos, r.c);
            if (d - r.r > 0 && d - r.r < 1.4f && g.pl.grounded) press(A_JUMP);
        }
    }

    bool step() {
        in = InputState();
        g.botAim = false;
        if (!g.dialog.empty()) { if (((int)(total * 120)) % 6 == 0) press(A_INTERACT); return true; }
        if (pc >= cmds.size()) return false;
        Cmd& c = cmds[pc];
        t += SIM_DT;
        bool done = false;
        float timeout = 60;
        switch (c.op) {
            case BUYBOT: {
                int i = (int)c.t;
                printf("[bot]   embers before buying robot %d: %d\n", i, g.save.embers);
                bool ok = g.buyOrToggleBot(i);
                g.screen = SC_PLAY;
                if (!ok || !g.botEquipped(i)) { printf("[bot] FAIL: could not buy/equip robot %d\n", i); failures++; }
                else printf("[bot]   ok: robot %d bought and following\n", i);
                done = true;
            } break;
            case NOTE:
                printf("[bot] %-40s dmg-taken-in-prev-section=%.0f  t=%.0fs\n", c.s.c_str(), g.save.playTime - 0 >= 0 ? (float)(g.testLog.size() - dmgAtSection) : 0.f, total);
                dmgAtSection = (float)g.testLog.size();
                section = c.s; done = true; break;
            case GOTO:
                if (t <= SIM_DT * 1.5f) startLevel = g.lv.id;
                done = walk(c.p) || g.lv.id != startLevel || g.transitioning; timeout = 40; break;
            case THROW: {
                if (phase == 0) {
                    if (g.flame.st != F_HELD) { press(A_THROW); break; }
                    aim(c.p); face(c.p - g.pl.pos);
                    if (t > 0.1f) { press(A_THROW); phase = 1; t = 0; }
                } else if (phase == 1) {
                    aim(c.p);
                    if (g.flame.st == F_FLYING || g.flame.st == F_HOVER) sawFlight = true;
                    if (sawFlight && (g.flame.st == F_HOVER || g.flame.st == F_HELD)) {
                        if (c.t > 0) {
                            if (g.flame.st == F_HOVER) { press(A_WARP); phase = 2; t = 0; }
                            else { phase = 0; sawFlight = false; }  // flame came back (e.g. smothered): throw again
                        }
                        else done = true;
                    }
                } else if (t > 0.6f && g.pl.grounded) done = true;
                timeout = 12;
            } break;
            case CHARGE: {
                aim(c.p); face(c.p - g.pl.pos);
                if (phase == 0) { if (g.flame.st != F_HELD) { press(A_THROW); break; } press(A_THROW); phase = 1; t = 0; }
                else if (phase == 1) { in.down[A_THROW] = true; if (t > 0.9f) { phase = 2; t = 0; } }
                else if (t > 0.5f) done = true;
                timeout = 5;
            } break;
            case RECALL:
                if (g.flame.st == F_HOVER || g.flame.st == F_FLYING) { if (t > 0.05f) press(A_THROW); }
                if (g.flame.st == F_HELD && t > 0.1f) done = true;
                timeout = 5; break;
            case WARP: if (g.flame.st != F_HELD) press(A_WARP); done = t > 0.4f; break;
            case BLOOM:
                if (phase == 0) { if (g.flame.bloomCd <= 0) { press(A_BLOOM); phase = 1; t = 0; } }
                else done = t > 0.3f;
                timeout = 6; break;
            case WAIT: done = t >= c.t; break;
            case TALK: if (t < 0.05f) press(A_INTERACT); done = t > 0.5f && g.dialog.empty(); break;
            case EXPECT:
                if (!g.has(c.s)) { printf("[bot] FAIL: expected flag '%s' in section '%s' at (%.1f %.1f %.1f)\n", c.s.c_str(), section.c_str(), g.pl.pos.x, g.pl.pos.y, g.pl.pos.z); failures++; }
                else printf("[bot]   ok: %s\n", c.s.c_str());
                done = true; break;
            case EXPECT_LEVEL:
                if (g.transitioning) break;
                if (g.lv.id != (int)c.t) { if (t > 3) { printf("[bot] FAIL: expected level %d, in %d\n", (int)c.t, g.lv.id); failures++; done = true; } }
                else { printf("[bot]   ok: entered %s\n", levelName(g.lv.id)); done = true; }
                break;
            case WAITLIFT_DOWN: {
                for (auto& d : g.lv.dyn) if (d.kind == DK_MOVER && d.a.y == 0 && d.b.y > 20) { done = d.offset.y < 0.05f; }
                timeout = 30;
            } break;
            case WAITLIFT_UP: {
                for (auto& d : g.lv.dyn) if (d.kind == DK_MOVER && d.a.y == 0 && d.b.y > 20) { done = d.offset.y > d.b.y - 0.05f; }
                timeout = 30;
            } break;
            case FIGHT: {
                Enemy* e = nearestEnemy(c.t);
                bool arenaActive = false;
                for (auto& a : g.lv.arenas) if (a.active) arenaActive = true;
                if (!e) { if (!arenaActive && g.flame.st != F_HELD) press(A_THROW); done = !arenaActive && g.flame.st == F_HELD && t > 0.3f; break; }
                V3 tgt = e->pos + V3(0, e->height * 0.5f, 0);
                float d = distXZ(e->pos, g.pl.pos);
                bool charged = e->type == E_KNIGHT && e->shield;
                // moths are best handled with a bloom when they cluster
                int near = 0;
                for (auto& o : g.enemies) if (o.alive && len(o.pos - g.pl.pos) < 4.5f) near++;
                if (g.ability("bloom") && g.flame.bloomCd <= 0 && near >= 2) press(A_BLOOM);
                attack(tgt, charged, e->type == E_KNIGHT ? 4.f : (e->type == E_MEGABOT ? 6.f : 2.5f));
                if (e->type == E_MEGABOT) {
                    // circle round to its unarmoured back
                    V3 fwd(std::sin(e->yaw), 0, std::cos(e->yaw));
                    V3 to = flatXZ(g.pl.pos - e->pos);
                    if (dot(norm(to), fwd) > -0.3f || d > 7) {
                        V3 side = norm(V3(-fwd.z, 0, fwd.x)); if (dot(side, to) < 0) side = side * -1.f;
                        V3 want = e->pos + side * 5.f - fwd * 3.f;
                        want.x = clampf(want.x, -7.5f, 7.5f);
                        V3 dd = flatXZ(want - g.pl.pos); face(dd); in.moveY = 1;
                        if (g.pl.dashCd <= 0 && d < 4) press(A_DASH);
                    }
                    timeout = 90; break;
                }
                // keep some distance
                if (d < 4.5f) { in.moveY = -1; }
                else if (d > 12) { in.moveY = 1; }
                if (d < 3.2f && g.pl.dashCd <= 0 && (e->state == 2 || e->state == 4)) { in.moveY = -1; press(A_DASH); }
                timeout = 90;
            } break;
            case BOSS: done = bossStep(); timeout = 400; break;
        }
        if (t > timeout && !done) {
            printf("[bot] FAIL: timeout on command %zu (op %d) in section '%s' at pos (%.1f %.1f %.1f) level %d\n", pc, c.op, section.c_str(), g.pl.pos.x, g.pl.pos.y, g.pl.pos.z, g.lv.id);
            failures++;
            done = true;
        }
        if (failures > 0) return false;  // stop at the first failure: later steps would only cascade
        if (done) { pc++; t = 0; phase = 0; stuck = 0; sawFlight = false; }
        return true;
    }

    bool bossStep() {
        Boss& b = g.boss;
        V3 C = g.lv.boss.center;
        if (!b.active) {
            if (g.has(g.lv.boss.flag)) return true;
            walk(C, 2);  // walk in to trigger
            return false;
        }
        if (b.dead) return false;
        dodgeRings();
        switch (b.type) {
            case B_BELLWETHER: {
                V3 rel = flatXZ(g.pl.pos - C);
                float r = lenXZ(rel);
                V3 tangent = norm(V3(-rel.z, 0, rel.x));
                V3 want = C + norm(rel) * 9 + tangent * 2;
                bool vul = !b.wps.empty() && b.wps[0].vulnerable;
                if (vul) attack(b.wps[0].pos, false, 1.0f);
                else if (g.flame.st != F_HELD) press(A_THROW);
                V3 d = flatXZ(want - g.pl.pos);
                if (!vul) { face(d); in.moveY = 1; }
                (void)r;
            } break;
            case B_WARDEN: {
                // burn the servers, then the hacker while his firewall is down
                const WeakPoint* target = nullptr;
                for (auto& w : b.wps) if (w.id == 10 && w.vulnerable) target = &w;
                if (!target) { float bd = 1e9f; for (auto& w : b.wps) if (w.id < 3) { float d = len(w.pos - g.pl.pos); if (d < bd) { bd = d; target = &w; } } }
                if (target) {
                    float d = distXZ(target->pos, g.pl.pos);
                    if (d > 12) walk(target->pos, 10);
                    else if (d < 4) { face(g.pl.pos - target->pos); in.moveY = 1; }
                    attack(target->pos, false, 1.0f);
                }
                for (auto& tg : g.teles) if (!tg.fired && distXZ(tg.pos, g.pl.pos) < tg.radius + 0.5f) { V3 away = norm(flatXZ(g.pl.pos - tg.pos) + V3(0.01f, 0, 0)); face(away); in.moveY = 1; }
            } break;
            case B_MATRON: {
                if (b.state == 4 && !b.wps.empty() && b.wps[0].vulnerable) { attack(b.wps[0].pos, false, 1.0f); break; }
                int unlitW = 0, unlitE = 0;
                for (auto& br : g.lv.braziers) if (br.bossBrazier && !br.lit) { if (br.pos.x < C.x) unlitW++; else unlitE++; }
                if (g.flame.st != F_HELD) { press(A_THROW); break; }
                V3 target = unlitW ? C + V3(-8, 0, 0) : (unlitE ? C + V3(8, 0, 0) : C + V3(0, 0, -6));
                if (walk(target, 0.8f) && (unlitW || unlitE) && g.flame.bloomCd <= 0) press(A_BLOOM);
            } break;
            case B_RATKING: {
                bool vul = !b.wps.empty() && b.wps[0].vulnerable;
                bool dodging = false;
                for (auto& tg : g.teles) if (!tg.fired && distXZ(tg.pos, g.pl.pos) < tg.radius + 0.8f) {
                    V3 away = norm(flatXZ(g.pl.pos - tg.pos) + V3(0.01f, 0, 0)); face(away); in.moveY = 1; dodging = true;
                    if (g.pl.dashCd <= 0) press(A_DASH);
                }
                if (vul) { attack(b.wps[0].pos, false, 0.8f); break; }
                Enemy* e = nearestEnemy(10);
                if (e) attack(e->pos + V3(0, 0.3f, 0), false, 2);
                else if (g.flame.st == F_HOVER) press(A_THROW);
                if (!dodging) { V3 rel = flatXZ(g.pl.pos - C); float r = lenXZ(rel); if (r < 7) { face(rel); in.moveY = 1; } else if (r > 10) { face(-rel); in.moveY = 1; } }
            } break;
            case B_HUSH: {
                if (b.phase == 0) {
                    int h = -1;
                    for (int k = 0; k < 2; k++) if (b.hands[k].alive && b.hands[k].down) h = k;
                    if (h >= 0) {
                        auto& H = b.hands[h];
                        V3 eye = H.pos + V3(0, 0.4f, 0);
                        attack(eye, H.shroud > 0.5f, 0.8f);
                        if (len(g.pl.pos - H.pos) < 5) in.moveY = -1;
                    } else {
                        if (g.flame.st == F_HOVER) press(A_THROW);
                        V3 want = C + V3(0, 0, -11);
                        walk(want, 1.5f);
                    }
                    // sidestep telegraphs
                    for (auto& tg : g.teles) if (!tg.fired && distXZ(tg.pos, g.pl.pos) < tg.radius + 0.5f) { V3 away = norm(flatXZ(g.pl.pos - tg.pos) + V3(0.01f, 0, 0)); face(away); in.moveY = 1; if (g.pl.dashCd <= 0) press(A_DASH); }
                } else if (b.phase == 1) {
                    bool open = !b.wps.empty() && b.wps[0].vulnerable;
                    if (open) { attack(b.wps[0].pos, true, 0.8f); break; }
                    if (g.flame.st != F_HELD && !g.flame.charging) { press(A_THROW); break; }
                    int target = -1; float bd = 1e9f;
                    for (int k = 0; k < (int)g.lv.braziers.size(); k++) {
                        auto& br = g.lv.braziers[k];
                        if (!br.bossBrazier || br.lit) continue;
                        float d = distXZ(br.pos, g.pl.pos);
                        if (d < bd) { bd = d; target = k; }
                    }
                    if (target >= 0) {
                        V3 p = g.lv.braziers[target].pos;
                        V3 stand = p + norm(flatXZ(p - C)) * 1.8f;
                        if (walk(stand, 1.0f) && g.flame.bloomCd <= 0) press(A_BLOOM);
                    }
                } else {
                    if (!b.wps.empty()) attack(b.wps[0].pos, true, 0.5f);
                }
            } break;
            default: break;
        }
        return false;
    }
};

static std::vector<Cmd> campaign() {
    std::vector<Cmd> c;
    auto add = [&](std::initializer_list<Cmd> l) { c.insert(c.end(), l.begin(), l.end()); };
    // ---------------- hub: meet Marrow, head west
    add({N("HUB: start"), E("talk.marrow0"), G(0, 0, -18), G(-12, 0, -12), G(-30, 0, -2), G(-43, 0, 0), G(-50, -2, 0), G(-60, -2, 0), EL(LV_WHARF)});
    // ---------------- wharf
    add({N("WHARF: tutorial"), G(0, 0, 9), G(0, 0, 15), G(0, 0, 24), T(8, 1.35f, 26), W(0.5f), E("wharf.g1"), R_(),
         G(0, 0, 33), N("WHARF: market fight"), F(25), G(-7, 0, 55.6f), G(-10.4f, 2.2f, 56), E("candle.0"), G(-6, 0, 56), G(0, 0, 57.5f),
         N("WHARF: stepping stones"), G(0, 0.3f, 61), G(3, 0.3f, 65), G(-1, 0.3f, 69), G(2.5f, 0.3f, 73), G(0, 0.3f, 77), G(-3, 0.3f, 81), G(0, 0.3f, 85), G(0, 0.3f, 88.4f), G(0, 0, 91),
         G(0, 0, 96), N("WHARF: net-yard ambush"), F(30), E("wharf.arena"), G(-10.3f, 0, 116), E("candle.1"), G(0, 0, 117), G(0, 0, 126),
         N("WHARF: timed braziers"), G(0, 0, 131), T(-9.5f, 1.35f, 132), R_(), T(9.5f, 1.35f, 132), W(0.3f), E("wharf.g2"), R_(),
         G(3, 0, 122), G(9.6f, 0, 122), E("candle.2"), G(3, 0, 122), G(0, 0, 130), G(0, 0, 147), G(0, 0, 153),
         N("WHARF: boss - Drowned Bellwether"), BOSSF(), E("boss.0"), G(0, 0, 170), W(0.5f), E("heart.0"), E("ab.warp"),
         G(0, 0, 186), G(0, 0, 191), EL(LV_HUB)});
    // ---------------- hub -> clockwood (warp across the broken bridge)
    add({N("HUB: broken bridge"), G(-44, 0, 0), G(-34, 0, 4), G(-16, 0, 10), G(-4, 0, 26), G(0, 0, 36), G(0, 0, 42), TW(0, 2.2f, 57.5f), G(0, 0, 68.5f), EL(LV_CLOCK)});
    // ---------------- clockwood
    add({N("CLOCK: chasm + cliff"), G(0, 0, 12), TW(0, 2.0f, 29), G(5.5f, 0, 33), G(6.8f, 0, 33.5f), E("candle.3"), G(0, 0, 30), TW(0, 7.6f, 36.9f),
         G(0, 6, 58), TW(0, 7.6f, 74.5f), G(0, 6, 78), N("CLOCK: moths"), F(16), G(0, 6, 101),
         N("CLOCK: great tree"), TW(6.5f, 10.3f, 107), TW(6.5f, 13.3f, 117), G(8.3f, 12, 119.3f), E("candle.4"), G(6.5f, 12, 117),
         TW(-6.5f, 16.3f, 117), TW(-6.5f, 19.3f, 107), G(-11.5f, 18, 106), G(-11.5f, 18, 133), G(-11.5f, 18, 138), G(0, 18, 146),
         N("CLOCK: gearwork ambush"), F(30), E("clock.arena"), G(12, 18, 163.3f), E("candle.5"), G(0, 18, 163), G(0, 18, 172),
         N("CLOCK: timed pillars"), T(-9, 23.35f, 180), R_(), T(9, 23.35f, 180), W(0.3f), E("clock.g2"), R_(), G(0, 18, 191),
         G(0, 18, 201), N("CLOCK: boss - GL1TCH the hacker"), BOSSF(), E("boss.1"), G(0, 18, 218), W(0.5f), E("ab.bloom"),
         G(4, 18, 226), G(4, 18, 234), G(0, 18, 236), G(0, 18, 240), EL(LV_HUB)});
    // ---------------- hub -> vaults (bloom the wax seal)
    add({N("HUB: wax seal"), G(0, 0, 57), TW(0, 2.2f, 42.5f), G(0, 0, 36), G(2, 0, 24), G(16, 0, 8), G(36, 0, 0), G(42, 0, 0), B(), E("hub.vaultseal"), G(48.5f, 0, 0), EL(LV_VAULTS)});
    // ---------------- vaults
    add({N("VAULTS: sealed brazier"), G(0, 0, 10.5f), B(), W(0.3f), E("vaults.g1"), G(3, 0, 9), B(), E("vaults.seal1"), G(9, 0, 9), E("candle.6"), G(2, 0, 9),
         G(0, 0, 25), G(0, 0, 33), N("VAULTS: knight hall"), F(30), G(0, 0, 72), N("VAULTS: lava + moths"), G(0, 0, 79.4f), G(0, 0, 86), F(14), G(0, 0, 91.4f), G(0, 0, 97),
         F(14), TW(8.5f, 4.4f, 97.5f), G(9.5f, 3, 99.3f), E("candle.7"), G(7, 3, 99), G(2, 0, 104), G(0, 0, 112), G(0, 0, 118),
         N("VAULTS: ossuary ambush"), F(30), E("vaults.arena"), G(0, 0, 150), N("VAULTS: three sealed wicks"), G(-4.2f, 0, 156.5f), B(), G(3.8f, 0, 166), B(), W(0.3f), E("vaults.g2"),
         G(0, 0, 173), G(0, 0, 182), N("VAULTS: boss - Veil Matron"), BOSSF(), E("boss.2"), G(0, 0, 194), W(0.5f), E("ab.sunflare"),
         G(0, 0, 211), G(0, 0, 216), EL(LV_HUB)});
    // ---------------- hub: buy a robot friend, then the Junkyard
    add({N("HUB: Sprocket's robots"), G(22, 0, 0), G(12.5f, 0, -12), G(12.5f, 0, -13.5f), TALKC(), W(0.3f), BUY(0), BUY(1), G(8, 0, -12), G(0, 0, -12.5f), G(-10, 0, -15), G(-14, 0, -20), G(-14, 0, -36), G(-14, 0, -46), EL(LV_JUNK)});
    add({N("JUNK: rat street"), G(0, 0, 8), F(14), G(0, 0, 22), F(14), G(0, 0, 33), F(14), G(0, 0, 39), G(0, 0, 55.5f), TW(0, 2.2f, 72), G(0, 0, 76), F(14),
         G(-8.3f, 0, 81.8f), E("shard.junk0"), G(0, 0, 82), G(0, 0, 91), N("JUNK: MEGA-BOT 3000"), F(30), E("junk.arena"), G(11.8f, 0, 88.3f), E("candle.9"),
         G(0, 0, 100), G(0, 0, 118), G(0, 0, 123), N("JUNK: furnaces"), T(-6, 1.35f, 127), R_(), T(6, 1.35f, 127), R_(), T(0, 1.35f, 136), W(0.3f), E("junk.g2"), R_(),
         G(0, 0, 147), G(0, 0, 153), N("JUNK: boss - King Rattletrap"), BOSSF(), E("boss.4"), G(0, 0, 167.5f), W(0.5f), E("heart.4"), E("spark.core"),
         G(-6, 0, 180), G(0, 0, 192), G(0, 0, 198), EL(LV_HUB)});
    // ---------------- hub: pedestals -> spire
    add({N("HUB: pedestals"), G(-14, 0, -20), G(-6, 0, -13), G(0, 0, -11.5f), TALKC(), W(0.5f), E("hub.spire"), G(0, 0, -6), G(0, 0, -4.5f), EL(LV_SPIRE)});
    // ---------------- spire
    add({N("SPIRE: wraith hall"), G(0, 0, 2), F(18), G(0, 0, 13), G(0, 6, 27), N("SPIRE: shielded knight"), F(16), G(0, 6, 40), TC(0, 8.5f, 44.3f), E("spire.c1"), G(0, 6, 47),
         N("SPIRE: outer climb"), TW(0, 12.2f, 54.5f), TW(8, 16.2f, 55), TW(8, 20.2f, 65), TW(0, 24.2f, 67), TW(0, 28.2f, 74.5f), G(0, 26, 80),
         N("SPIRE: bell gallery ambush"), F(30), E("spire.arena"), G(0, 26, 103),
         N("SPIRE: remote bloom"), G(-3, 26, 108), T(5.5f, 27.35f, 108), B(), G(-4, 26, 108), W(0.2f), B(), W(0.3f), E("spire.g2"), G(-6.3f, 26, 123.6f), E("candle.8"), G(0, 26, 121),
         {WAITLIFT_DOWN, V3(), 0, ""}, G(0, 26, 128), {WAITLIFT_UP, V3(), 0, ""}, G(0, 60, 136), G(0, 60, 143),
         N("SPIRE: final boss - The Hush"), BOSSF(), E("boss.3"), E("game.finished")});
    return c;
}

static const char* THEME_NAME(int d) { static const char* n[5] = {"drowned", "ticking", "ashen", "rusted", "starlit"}; return n[(d - 1) % 5]; }
int runAutotest(Game& g, const std::string& which) {
    setenv("WICKLIGHT_NO_SAVE", "", 0);
    if (which.rfind("endless", 0) == 0) {
        // play generated Endless Dark levels back to back: "endless" (with all powers) or "endless-nowarp"
        bool nowarp = which.find("nowarp") != std::string::npos;
        int N = nowarp ? 8 : 30;
        if (const char* e = getenv("WL_ENDLESS_N")) N = atoi(e);
        g.botActive = true; g.newGame(); InputState sk; sk.mBack = true; g.frame(0.016f, sk, 0, 0);
        g.dialog.clear();
        g.setFlag("talk.marrow0");
        if (!nowarp) for (const char* f : {"ab.warp", "ab.bloom", "ab.sunflare"}) g.setFlag(f);
        g.godMode = getenv("WICKLIGHT_MORTAL") == nullptr;
        g.save.endlessDepth = 1;
        int cleared = 0, fails = 0;
        int d0 = getenv("WL_ENDLESS_START") ? atoi(getenv("WL_ENDLESS_START")) : 1;
        for (int depth = d0; depth < d0 + N; depth++) {
            g.save.endlessDepth = depth;
            g.enterLevel(LV_ENDLESS, 0);
            g.dialog.clear();
            Bot bot(g);
            bot.airOk = false;
            for (auto& st : g.lv.route) {
                switch (st.op) {
                    case 0: bot.cmds.push_back(G(st.p.x, st.p.y, st.p.z)); break;
                    case 1: bot.cmds.push_back(TW(st.p.x, st.p.y, st.p.z)); break;
                    case 2: bot.cmds.push_back(T(st.p.x, st.p.y, st.p.z)); bot.cmds.push_back(R_()); break;
                    case 3: bot.cmds.push_back(F(22)); break;
                    case 4: bot.cmds.push_back(BOSSF()); break;
                    case 5: bot.cmds.push_back(B()); break;
                }
            }
            bot.cmds.push_back(W(2));
            float t = 0;
            int hits0 = (int)g.testLog.size();
            while (t < 400 && g.save.endlessDepth == depth) {
                if (!bot.step() && g.dialog.empty()) break;
                g.update(SIM_DT, bot.in); t += SIM_DT;
                if (g.screen != SC_PLAY) g.screen = SC_PLAY;
                if (getenv("WL_ETRACE") && atoi(getenv("WL_ETRACE")) == depth && std::fmod(t, getenv("WL_ETRACE_DT") ? atof(getenv("WL_ETRACE_DT")) : 0.5f) < SIM_DT) {
                    int al = 0, act = 0; for (auto& e : g.enemies) if (e.alive) al++; for (auto& a : g.lv.arenas) if (a.active) act++;
                    printf("   t=%.1f pc=%zu/%zu op=%d pos=(%.1f %.1f %.1f) g=%d alive=%d arenaActive=%d flame=%d\n", t, bot.pc, bot.cmds.size(), bot.pc < bot.cmds.size() ? bot.cmds[bot.pc].op : -1, g.pl.pos.x, g.pl.pos.y, g.pl.pos.z, g.pl.grounded, al, act, g.flame.st);
                    if (std::fmod(t, 10.f) < SIM_DT) for (auto& e : g.enemies) if (e.alive) printf("      enemy type %d at (%.1f %.1f %.1f) hp %.1f\n", (int)e.type, e.pos.x, e.pos.y, e.pos.z, e.hp);
                }
            }
            // let the transition finish
            for (int k = 0; k < 120 && g.transitioning; k++) g.update(SIM_DT, InputState());
            bool ok = g.save.endlessDepth == depth + 1;
            if (getenv("WL_ETRACE") && atoi(getenv("WL_ETRACE")) == depth) for (size_t k = 0; k < bot.cmds.size(); k++) printf("   cmd %zu op %d p=(%.1f %.1f %.1f)\n", k, bot.cmds[k].op, bot.cmds[k].p.x, bot.cmds[k].p.y, bot.cmds[k].p.z);
            printf("[bot] endless depth %2d (%s%s): %s in %4.0fs, %zu segments-route, hits %d\n", depth, depth % 5 == 0 ? "BOSS " : "", THEME_NAME(depth), ok ? "CLEARED" : "FAILED", t, g.lv.route.size(), (int)g.testLog.size() - hits0);
            if (ok) cleared++; else { fails++; printf("[bot]   stuck at (%.1f %.1f %.1f)\n", g.pl.pos.x, g.pl.pos.y, g.pl.pos.z); g.save.endlessDepth = depth + 1; }
        }
        printf("[bot] endless: %d/%d depths cleared, best %d, island unlocked: %s, embers %d, wood %d stone %d crystal %d gears %d\n", cleared, N, g.save.endlessBest,
               g.has("home.unlocked") ? "yes" : "no", g.save.embers, g.itemCount(BK_LOG), g.itemCount(BK_COBBLE), g.itemCount(IT_CRYSTAL), g.itemCount(IT_GEAR));
        return fails == 0 ? 0 : 2;
    }
    if (which == "build" || which == "island") {
        // Wick's Island block world: real input for mining/placing, then every usable block
        g.botActive = true; g.newGame(); InputState sk; sk.mBack = true; g.frame(0.016f, sk, 0, 0);
        g.dialog.clear();
        g.setFlag("home.unlocked");
        g.save.dayT = 0.05f;
        g.enterLevel(LV_HOME, 0);
        g.dialog.clear(); g.screen = SC_PLAY;
        int fails = 0;
        auto check = [&](const char* what, bool ok, const std::string& extra = "") {
            printf("[bot] island: %-44s %s %s\n", what, ok ? "OK" : "FAIL", extra.c_str());
            if (!ok) fails++;
        };
        auto sim = [&](float secs, InputState in) {
            for (float t = 0; t < secs; t += SIM_DT) {
                g.update(SIM_DT, in);
                in.clearEdges();
                if (!g.dialog.empty()) g.dialog.clear();
                if (g.screen != SC_PLAY) g.screen = SC_PLAY;
            }
        };
        VoxWorld& w = g.vox;
        // 1. the generated island
        int counts[BK_COUNT] = {};
        for (auto b : w.blk) counts[b]++;
        check("generated island has hills, sea, trees, ores", counts[BK_GRASS] > 1500 && counts[BK_WATER] > 2000 && counts[BK_LOG] > 40 && counts[BK_EMBER_ORE] > 50 && counts[BK_CRYSTAL_ORE] > 3 && counts[BK_SCRAP_ORE] > 10,
              "grass " + std::to_string(counts[BK_GRASS]) + " water " + std::to_string(counts[BK_WATER]) + " logs " + std::to_string(counts[BK_LOG]) +
              " ore " + std::to_string(counts[BK_EMBER_ORE]) + "/" + std::to_string(counts[BK_CRYSTAL_ORE]) + "/" + std::to_string(counts[BK_SCRAP_ORE]));
        sim(0.5f, InputState());
        check("standing on the plaza (not falling)", g.pl.grounded && std::fabs(g.pl.pos.y) < 0.05f, "y=" + std::to_string(g.pl.pos.y));
        // 2. Bramble's starter kit
        for (int i = 0; i < (int)g.lv.talkers.size(); i++) if (g.lv.talkers[i].who == 10) g.talk(i);
        g.dialog.clear(); g.screen = SC_PLAY;
        check("Bramble's starter kit", g.itemCount(BK_BED) == 1 && g.itemCount(BK_LOG) == 10 && g.itemCount(BK_DOOR) == 1);
        // 3. mine with the real mouse button: look down in front, hold throw
        g.camYaw = 0; g.camPitch = -0.75f;
        sim(0.3f, InputState());
        int before = g.itemCount(BK_STONEBRICK) + g.itemCount(BK_DIRT) + g.itemCount(BK_COBBLE);
        bool hadTarget = g.tgtValid;
        { InputState in; in.down[A_THROW] = true; in.pressed[A_THROW] = true; sim(0.05f, in); in.pressed[A_THROW] = false; sim(4.0f, in); }
        sim(0.2f, InputState());
        int after = g.itemCount(BK_STONEBRICK) + g.itemCount(BK_DIRT) + g.itemCount(BK_COBBLE);
        check("hold [throw] mines blocks", hadTarget && after > before, std::to_string(after - before) + " blocks mined, flame thrown: " + (g.flame.st != F_HELD ? "yes(bad)" : "no"));
        // 4. place with the real button: select cobblestone, press warp
        g.save.hot[3] = BK_COBBLE; g.hotSel = 3;
        g.pl.pos.z += 1.2f;
        sim(0.2f, InputState());
        int cob0 = g.itemCount(BK_COBBLE);
        { InputState in; in.pressed[A_WARP] = true; in.down[A_WARP] = true; sim(0.05f, in); }
        sim(0.3f, InputState());
        check("[warp] places the held block", g.itemCount(BK_COBBLE) == cob0 - 1, "cobble " + std::to_string(cob0) + " -> " + std::to_string(g.itemCount(BK_COBBLE)) + " target " + std::to_string(g.tgtValid) + " " + std::to_string(g.tgtX) + "," + std::to_string(g.tgtY) + "," + std::to_string(g.tgtZ) + " n " + std::to_string(g.tgtN.y) + " player " + std::to_string(g.pl.pos.z) + " status " + g.statusMsg);
        // 5. number keys pick hotbar slots
        { InputState in; in.numKey = 5; g.update(SIM_DT, in); }
        check("number keys pick the hotbar slot", g.hotSel == 4);
        g.hotSel = 0;
        // 6. a wall blocks walking; placing inside yourself is refused
        V3 p0 = g.pl.pos;
        int wx = (int)std::floor(p0.x), wz = (int)std::floor(p0.z) + 2;
        g.addItem(BK_COBBLE, 20, false);
        for (int dx = -2; dx <= 2; dx++) for (int y = 0; y < 3; y++) g.placeBlock(BK_COBBLE, wx + dx, y, wz, 0, 0);
        g.camYaw = 0;
        { InputState in; in.moveY = 1; in.down[A_FWD] = true; sim(2.0f, in); }
        check("placed blocks are solid (can't walk through)", g.pl.pos.z < wz - 0.2f, "z=" + std::to_string(g.pl.pos.z) + " wall at " + std::to_string(wz));
        check("can't place a block inside yourself", !g.placeBlock(BK_COBBLE, (int)std::floor(g.pl.pos.x), (int)std::floor(g.pl.pos.y), (int)std::floor(g.pl.pos.z), 0, 0));
        // 7. doors: two tall, open and close
        int dx = wx, dz = wz;
        g.mineBlock(dx, 0, dz, true); g.mineBlock(dx, 1, dz, true);
        bool placed = g.placeBlock(BK_DOOR, dx, 0, dz, 0, 0);
        AABB probe(V3(dx + 0.3f, 0.3f, dz + 0.05f), V3(dx + 0.7f, 0.8f, dz + 0.15f));
        bool closedSolid = !g.boxFree(probe);
        g.useBlock(dx, 1, dz);
        bool openFree = g.boxFree(probe);
        check("door: 2 blocks tall, opens and closes", placed && w.get(dx, 1, dz) == BK_DOOR_TOP && closedSolid && openFree);
        g.camYaw = 0;
        g.pl.pos = V3(dx + 0.5f, 0.05f, dz - 1.5f); g.pl.vel = V3();
        { InputState in; in.moveY = 1; in.down[A_FWD] = true; sim(1.5f, in); }
        check("walk through the open door", g.pl.pos.z > dz + 0.9f, "z=" + std::to_string(g.pl.pos.z));
        // 8. crafting: planks anywhere, a door at the workbench
        g.pl.pos = V3(-3.5f, 0.05f, -23.5f); g.pl.vel = V3();
        int pl0 = g.itemCount(BK_PLANKS);
        bool c1 = g.craft(0);
        g.addItem(BK_PLANKS, 6, false);
        int door0 = g.itemCount(BK_DOOR);
        int doorRec = -1;
        for (int r = 0; r < 64; r++) { if (!g.craft(r)) continue; if (g.itemCount(BK_DOOR) > door0) { doorRec = r; break; } }
        check("crafting (planks by hand, door at the workbench)", c1 && g.itemCount(BK_PLANKS) >= pl0 + 4 - 6 && doorRec >= 0);
        // 9. chests keep things, and give them back when broken
        int cx = 8, cz = -21;
        for (int y = 0; y < 3; y++) g.mineBlock(cx, y, cz, false);
        g.placeBlock(BK_CHEST, cx, 0, cz, 0, 0);
        int key = VoxWorld::idx(cx, 0, cz);
        g.save.chests[key][IT_GEAR] = 7;
        int gear0 = g.itemCount(IT_GEAR);
        g.mineBlock(cx, 0, cz, true);
        check("chest contents come back when it's broken", g.itemCount(IT_GEAR) == gear0 + 7 && g.itemCount(BK_CHEST) >= 1);
        // 10. ladder climbing
        int lx = 10, lz = -14;
        for (int y = 0; y < 6; y++) { g.mineBlock(lx, y, lz, false); g.mineBlock(lx, y, lz - 1, false); }
        for (int y = 0; y < 6; y++) w.set(lx, y, lz, BK_STONE);
        g.addItem(BK_LADDER, 6, false);
        bool lad = true;
        for (int y = 0; y < 5; y++) lad &= g.placeBlock(BK_LADDER, lx, y, lz - 1, 0, 0);
        g.pl.pos = V3(lx + 0.5f, w.groundHeight(lx, lz - 1) > 0 ? 0.05f : 0.05f, lz - 0.6f); g.pl.vel = V3();
        for (int y = -3; y < 0; y++) if (w.get(lx, y, lz - 1) == BK_AIR) w.set(lx, y, lz - 1, BK_STONE);
        float y0 = g.pl.pos.y;
        g.camYaw = 0;
        { InputState in; in.moveY = 1; in.down[A_FWD] = true; sim(1.2f, in); }
        check("climb a ladder", lad && g.pl.pos.y > y0 + 2.5f, "climbed " + std::to_string(g.pl.pos.y - y0));
        // 11. jump pad
        int jx = -4, jz = -21;
        g.addItem(BK_JUMPPAD, 1, false);
        bool jp = g.placeBlock(BK_JUMPPAD, jx, 0, jz, 0, 0);
        g.pl.pos = V3(jx + 0.5f, 0.9f, jz + 0.5f); g.pl.vel = V3();
        float peak = 0;
        for (float t = 0; t < 1.2f; t += SIM_DT) { g.update(SIM_DT, InputState()); peak = std::max(peak, g.pl.pos.y); }
        check("jump pad launches you", jp && peak > 6, "peak y " + std::to_string(peak) + " placed " + std::to_string(jp) + " ground " + std::to_string(w.groundHeight(jx, jz)) + " above " + std::to_string(w.get(jx, 1, jz)));
        sim(2, InputState());
        // 12. ember bomb
        int bx = 20, bz = 4, by = w.groundHeight(bx, bz) - 3;
        for (int y = by - 2; y <= by + 2; y++) for (int z = bz - 2; z <= bz + 2; z++) for (int x = bx - 2; x <= bx + 2; x++) w.set(x, y, z, BK_STONE);
        w.set(bx, by, bz, BK_BOMB);
        g.pl.pos = V3(0.5f, 0.05f, -23); g.pl.vel = V3();
        g.useBlock(bx, by, bz);
        sim(3.5f, InputState());
        int holes = 0;
        for (int y = by - 2; y <= by + 2; y++) for (int z = bz - 2; z <= bz + 2; z++) for (int x = bx - 2; x <= bx + 2; x++) if (w.get(x, y, z) == BK_AIR) holes++;
        check("ember bomb blasts a crater", holes >= 15, std::to_string(holes) + " blocks gone");
        // 13. swimming: you float in the sea instead of sinking
        int sx = 0, sz = 44;
        g.pl.pos = V3(sx + 0.5f, -1.6f, sz + 0.5f); g.pl.vel = V3();
        bool sea = w.get(sx, -2, sz) == BK_WATER;
        { InputState in; in.down[A_JUMP] = true; sim(3, in); }
        check("swim at the surface of the sea", sea && g.pl.pos.y > -3.0f && !g.pl.dead, "y=" + std::to_string(g.pl.pos.y));
        // 14. night: gloomlings come out away from lights; sunrise burns them
        g.pl.pos = V3(18.5f, (float)w.groundHeight(18, 12), 12.5f); g.pl.vel = V3(); g.pl.hp = 99;
        g.godMode = true;
        g.save.dayT = 0.7f;
        sim(25, InputState());
        int night = 0; for (auto& e : g.enemies) if (e.alive) night++;
        g.save.dayT = 0.05f;
        sim(20, InputState());
        int day = 0; for (auto& e : g.enemies) if (e.alive) day++;
        check("gloomlings spawn at night, burn at sunrise", night > 0 && day == 0, std::to_string(night) + " at night, " + std::to_string(day) + " after sunrise");
        // 15. bed: sleep through the night, respawn at the bed
        int ex = 6, ez = -18;
        g.mineBlock(ex, 0, ez, false);
        g.pl.pos = V3(ex + 0.5f, 0.05f, ez - 1.5f); g.pl.vel = V3();
        bool bed = g.placeBlock(BK_BED, ex, 0, ez, 0, 0);
        for (auto& e : g.enemies) e.alive = false;
        g.save.dayT = 0.75f;
        g.useBlock(ex, 0, ez);
        bool sleeping = g.sleepT > 0;
        sim(3, InputState());
        check("sleep in a bed skips the night", bed && sleeping && !g.isNight() && g.save.hasBed);
        g.godMode = false;
        g.pl.pos = V3(30, 2, 30);
        g.killPlayer();
        sim(2.5f, InputState());
        check("respawn at your bed", len(g.pl.pos - V3(ex + 0.5f, 0, ez + 0.5f)) < 1.6f, "at " + std::to_string(g.pl.pos.x) + "," + std::to_string(g.pl.pos.y) + "," + std::to_string(g.pl.pos.z));
        // 16. saplings grow into trees
        int tx = -14, tz = 0, ty = w.groundHeight(tx, tz);
        w.set(tx, ty - 1, tz, BK_GRASS);
        for (int y = ty; y < ty + 8; y++) for (int z = tz - 2; z <= tz + 2; z++) for (int x = tx - 2; x <= tx + 2; x++) w.set(x, y, z, BK_AIR);
        g.addItem(BK_SAPLING, 1, false);
        bool sap = g.placeBlock(BK_SAPLING, tx, ty, tz, 0, 0);
        g.pl.pos = V3(0.5f, 0.05f, -23); g.godMode = true;
        g.save.dayT = 0.05f;
        float grow = 0;
        for (; grow < 400 && w.get(tx, ty, tz) != BK_LOG; grow += 1) sim(1, InputState());
        check("a sapling grows into a tree", sap && w.get(tx, ty, tz) == BK_LOG && w.get(tx, ty + 3, tz) == BK_LOG, "after " + std::to_string((int)grow) + " s");
        // 17. save + load keeps blocks, inventory, chests and the bed
        g.placeBlock(BK_CHEST, cx, 0, cz, 0, 0);
        g.save.chests[key][IT_CRYSTAL] = 3;
        int logs = g.itemCount(BK_LOG);
        uint8_t doorB = w.get(dx, 0, dz);
        g.writeSave();
        g.save = SaveData();
        g.loadGame();
        g.enterLevel(LV_HOME, 0);
        g.dialog.clear();
        check("save/load keeps blocks, items, chests, bed", g.vox.get(dx, 0, dz) == doorB && doorB == BK_DOOR && g.itemCount(BK_LOG) == logs && g.save.chests[key][IT_CRYSTAL] == 3 && g.save.hasBed &&
              g.vox.get(cx, 0, cz) == BK_CHEST, std::to_string(g.save.voxLines.size()) + " changed blocks saved");
        // 18. old saves: blueprint buildings are refunded as blocks
        g.save.builds.push_back({8, 0, 0, 0}); g.save.res[0] = 5;
        int lg = g.itemCount(BK_LOG);
        g.migrateOldIsland();
        check("old blueprint saves are refunded as blocks", g.itemCount(BK_LOG) == lg + 15 && g.save.builds.empty());
        printf("[bot] island: %s (%d failures)\n", fails ? "FAIL" : "PASS", fails);
        return fails ? 2 : 0;
    }
    if (which == "robots") {
        g.botActive = true; g.newGame(); InputState sk; sk.mBack = true; g.frame(0.016f, sk, 0, 0);
        g.enterLevel(LV_WHARF, 1);
        g.dialog.clear();
        g.godMode = true;
        for (int i = 0; i < 5; i++) g.setFlag("bot.own." + std::to_string(i));
        g.setFlag("spark.core");
        InputState idle;
        int pass = 0;
        auto run = [&](float secs) { for (int k = 0; k < (int)(secs * 120); k++) g.update(SIM_DT, idle); };
        auto only = [&](std::initializer_list<int> ids) { for (int i = 0; i < 5; i++) g.save.flags.erase("bot.eq." + std::to_string(i)); for (int i : ids) g.setFlag("bot.eq." + std::to_string(i)); };
        // SPARK + BOOMER kill enemies on their own
        only({0, 4});
        g.enemies.clear();
        for (int k = 0; k < 4; k++) g.spawnEnemy(E_GLOOMLING, g.pl.pos + V3(-5 + k * 3.f, 0.3f, 7), -1);
        run(20);
        int alive = 0; for (auto& e : g.enemies) if (e.alive) alive++;
        printf("[bot] robots SPARK+BOOMER: %d of 4 enemies left after 20s %s\n", alive, alive == 0 ? "(OK)" : "(FAIL)"); pass += alive == 0;
        // PATCH heals
        only({2}); g.pl.hp = 2; run(16);
        printf("[bot] robot PATCH: hp %.0f -> %.0f %s\n", 2.f, g.pl.hp, g.pl.hp > 2 ? "(OK)" : "(FAIL)"); pass += g.pl.hp > 2;
        // BUBBLES blocks a hit
        only({3}); g.godMode = false; g.pl.hp = 5; g.pl.invuln = 0; run(10); g.pl.invuln = 0; g.hurtPlayer(1, g.pl.pos + V3(1, 0, 0));
        printf("[bot] robot BUBBLES: hp after a hit %.0f %s\n", g.pl.hp, g.pl.hp == 5 ? "(OK)" : "(FAIL)"); pass += g.pl.hp == 5; g.godMode = true;
        // MAGS vacuums embers
        only({1}); int before = g.save.embers; g.dropEmbers(g.pl.pos + V3(10, 0, 0), 10); run(5);
        printf("[bot] robot MAGS: embers %d -> %d %s\n", before, g.save.embers, g.save.embers >= before + 10 ? "(OK)" : "(FAIL)"); pass += g.save.embers >= before + 10;
        return pass == 4 ? 0 : 2;
    }
    if (which == "gatewarp") {
        // try to cheat past the Wharf's first gate by throwing over it and warping
        g.botActive = true; g.newGame(); InputState sk; sk.mBack = true; g.frame(0.016f, sk, 0, 0);
        g.setFlag("ab.warp");
        g.enterLevel(LV_WHARF, 0);
        Bot bot(g);
        bot.cmds = {G(0, 0, 27), TW(0, 9, 38), TW(-12, 6, 36), TW(12, 6, 36), W(1)};
        for (int i = 0; i < 120 * 40 && bot.step(); i++) g.update(SIM_DT, bot.in);
        bool passed = g.pl.pos.z > 30.6f;
        printf("[bot] warp past closed gate: %s (player z=%.1f)\n", passed ? "BYPASSED (bad)" : "blocked (good)", g.pl.pos.z);
        // try to warp up onto the net-yard's border wall, and to walk through a wall from inside it
        g.enterLevel(LV_WHARF, 1);
        g.pl.pos = V3(-9, 0, 100); g.pl.safePos = g.pl.pos;
        Bot b2(g);
        b2.cmds = {TW(-12.5f, 3.6f, 100), TW(-12.5f, 6, 104), W(1)};
        for (int i = 0; i < 120 * 30 && b2.step(); i++) g.update(SIM_DT, b2.in);
        bool onWall = g.pl.pos.y > 2.5f || g.pl.pos.x < -12;
        printf("[bot] warp onto border wall: %s (player at %.1f %.1f %.1f)\n", onWall ? "GOT ON/OVER (bad)" : "blocked (good)", g.pl.pos.x, g.pl.pos.y, g.pl.pos.z);
        g.pl.pos = V3(-12.2f, 0, 100); g.pl.vel = V3(0, 0, 0);  // stuck halfway into the wall
        InputState walkIn; walkIn.moveY = 1; g.camYaw = -PI / 2;  // walk toward -x (outwards)
        for (int i = 0; i < 120 * 2; i++) g.update(SIM_DT, walkIn);
        bool through = g.pl.pos.x < -13;
        printf("[bot] walk out through a wall: %s (x=%.1f)\n", through ? "WENT THROUGH (bad)" : "blocked (good)", g.pl.pos.x);
        return (passed || onWall || through) ? 2 : 0;
    }
    if (which == "warden" || which == "bellwether" || which == "matron" || which == "hush" || which == "ratking") {
        // fight one boss with real damage, starting from the lantern before it
        g.botActive = true; g.newGame(); InputState sk; sk.mBack = true; g.frame(0.016f, sk, 0, 0);
        for (const char* f : {"talk.marrow0", "ab.warp", "ab.bloom", "ab.sunflare"}) g.setFlag(f);
        int lvl = which == "bellwether" ? LV_WHARF : which == "warden" ? LV_CLOCK : which == "matron" ? LV_VAULTS : which == "ratking" ? LV_JUNK : LV_SPIRE;
        g.enterLevel((LevelId)lvl, 2);
        if (lvl == LV_SPIRE) { g.pl.pos = V3(0, 60, 136); g.pl.safePos = g.pl.pos; }
        g.godMode = false;
        Bot bot(g);
        bot.cmds = {BOSSF()};
        int deaths0 = g.save.deaths;
        float t = 0;
        while (t < 600 && !g.boss.dead && g.save.deaths == deaths0) {
            if (!bot.step() && g.dialog.empty()) break;
            g.update(SIM_DT, bot.in); t += SIM_DT;
            if (getenv("WL_BTRACE") && std::fmod(t, 0.5f) < SIM_DT) printf("  t=%.1f pos=(%.1f %.1f %.1f) g=%d flame=%d anch=%d fpos=(%.1f %.1f %.1f) boss st=%d stT=%.1f shock=%.0f hp=%.0f\n", t, g.pl.pos.x, g.pl.pos.y, g.pl.pos.z, g.pl.grounded, g.flame.st, g.flame.anchored, g.flame.pos.x, g.flame.pos.y, g.flame.pos.z, g.boss.state, g.boss.stateT, g.boss.f[3], g.pl.hp);
        }
        if (which == "warden") printf("[bot] (the Clockwood boss is now GL1TCH, the hacker)\n");
        printf("[bot] %s with real damage: %s after %.0fs, player hp %.0f/%.0f, boss hp %.1f/%.0f\n", which.c_str(),
               g.boss.dead ? "WON" : (g.save.deaths > deaths0 ? "DIED" : "TIMEOUT"), t, g.pl.hp, g.maxHp(), g.boss.hp, g.boss.maxHp);
        return g.boss.dead ? 0 : 2;
    }
    printf("[bot] WICKLIGHT autoplay campaign test (fresh save)\n");
    g.botActive = true;
    g.godMode = getenv("WICKLIGHT_MORTAL") == nullptr;
    g.newGame();
    InputState skip; skip.mBack = true;
    g.frame(0.016f, skip, 0, 0);
    if (g.screen != SC_PLAY) { printf("[bot] FAIL: intro did not lead to gameplay\n"); return 1; }
    Bot bot(g);
    bot.cmds = campaign();
    long steps = 0;
    const long maxSteps = 120L * 60 * 90;  // 90 simulated minutes
    while (steps < maxSteps) {
        if (g.screen == SC_ENDING || g.screen == SC_VICTORY) break;
        if (!bot.step() && g.dialog.empty()) break;
        bot.total += SIM_DT;
        g.update(SIM_DT, bot.in);
        if (getenv("WL_TRACE") && steps % 15 == 0 && bot.total > atof(getenv("WL_TRACE"))) printf("  t=%.2f pc=%zu pos=(%.2f %.2f %.2f) vel=(%.1f %.1f %.1f) g=%d jump=%d\n", bot.total, bot.pc, g.pl.pos.x, g.pl.pos.y, g.pl.pos.z, g.pl.vel.x, g.pl.vel.y, g.pl.vel.z, g.pl.grounded, bot.in.pressed[A_JUMP]);
        if (getenv("WL_TRACE") && steps % 120 == 0 && bot.total > atof(getenv("WL_TRACE"))) for (auto& e : g.enemies) if (e.alive) printf("     enemy type=%d arena=%d pos=(%.1f %.1f %.1f) hp=%.0f st=%d flame=%d\n", e.type, e.arena, e.pos.x, e.pos.y, e.pos.z, e.hp, e.state, g.flame.st);
        if (getenv("WL_TRACE") && steps % 60 == 0 && bot.total > atof(getenv("WL_TRACE")) && g.boss.active) printf("     BOSS hp=%.1f st=%d ph=%d stT=%.1f wps=%zu vul=%d flame=%d fpos=(%.1f %.1f %.1f) hits=%zu\n", g.boss.hp, g.boss.state, g.boss.phase, g.boss.stateT, g.boss.wps.size(), g.boss.wps.empty()?0:(int)g.boss.wps[0].vulnerable, g.flame.st, g.flame.pos.x, g.flame.pos.y, g.flame.pos.z, g.testLog.size());
        steps++;
        if (g.pl.dead) { printf("[bot] note: player died in '%s'\n", bot.section.c_str()); }
    }
    // play out the ending to the victory screen
    InputState none;
    for (int i = 0; i < 120 * 60 && g.screen == SC_ENDING; i++) g.frame(1.f / 120, none, 0, 0);
    bool victory = g.screen == SC_VICTORY;
    printf("[bot] ----------------------------------------------\n");
    printf("[bot] simulated play time: %.1f min, deaths: %d, damage taken (would-be hits): %zu\n", g.save.playTime / 60, g.save.deaths, g.testLog.size());
    printf("[bot] hearts %d/4, shards %d/7, candles %d/10, robots owned %d\n", g.countFlags("heart."), g.countFlags("shard."), g.countFlags("candle."), g.countFlags("bot.own."));
    printf("[bot] ending reached: %s, victory screen: %s, failures: %d\n", g.has("game.finished") ? "YES" : "NO", victory ? "YES" : "NO", bot.failures);
    // regression: a boss beaten but its heart left behind -> heart must still be there on return
    {
        SaveData keep = g.save;
        g.save.flags.erase("heart.0"); g.save.flags.erase("ab.warp");
        g.enterLevel(LV_WHARF, 2);
        bool waiting = false;
        for (auto& p : g.lv.pickups) if (p.type == PK_HEART && !p.taken) waiting = true;
        g.save.flags.insert("heart.0");
        g.writeSave(); g.save = SaveData(); g.loadGame();
        bool repaired = g.has("ab.warp");
        printf("[bot] left-behind heart reappears: %s, save repair grants warp: %s\n", waiting ? "OK" : "FAILED", repaired ? "OK" : "FAILED");
        if (!waiting || !repaired) bot.failures++;
        g.save = keep;
    }
    // economy + upgrade shop check
    int earned = g.save.embers;
    printf("[bot] embers earned: %d, best combo: %d hits, urns broken: %d\n", earned, g.bestCombo,
           (int)std::count_if(g.lv.breakables.begin(), g.lv.breakables.end(), [](const Level::Breakable& b) { return b.broken; }));
    g.save.embers = std::max(g.save.embers, 40);
    g.screen = SC_SHOP; g.shopSel = 0;
    InputState buy; buy.mOk = true;
    float hpBefore = g.maxHp();
    g.updateShop(buy);
    bool shopOk = g.has("up.wax.1") && g.maxHp() == hpBefore + 1;
    printf("[bot] upgrade shop (buy Thicker Wax): %s\n", shopOk ? "OK" : "FAILED");
    if (!shopOk) bot.failures++;
    // save round-trip check
    g.writeSave();
    SaveData before = g.save;
    g.save = SaveData();
    bool loaded = g.loadGame();
    printf("[bot] save/load round trip: %s (%zu flags)\n", loaded && g.save.flags == before.flags ? "OK" : "FAILED", g.save.flags.size());
    bool pass = victory && bot.failures == 0 && loaded;
    printf("[bot] RESULT: %s\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 2;
}
