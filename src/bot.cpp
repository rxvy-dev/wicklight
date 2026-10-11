// WICKLIGHT - autoplay campaign test.
// Plays a fresh save from New Game to the victory screen through the real game simulation:
// the same physics, flame, enemies, bosses, gates and saves the player uses. The bot aims
// perfectly and walks scripted routes; damage is counted (god mode only prevents deaths so a
// run can report how much damage each section dealt).
#include "game.h"
#include <cstdio>
#include <cstdlib>
#include <algorithm>

enum Op { BUYBOT, GOTO, THROW, CHARGE, RECALL, WARP, BLOOM, WAIT, FIGHT, BOSS, TALK, EXPECT, EXPECT_LEVEL, WAITLIFT_DOWN, WAITLIFT_UP, NOTE, HACK, HOLD };
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
static Cmd HACKP(int i) { return {HACK, V3(), (float)i, ""}; }
static Cmd HOLDP() { return {HOLD, V3(), 0, ""}; }

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
            case HACK: {
                // watch the sequence, then burn the terminals in order
                int pi = (int)c.t;
                if (pi >= (int)g.lv.puzzles.size()) { done = true; break; }
                HackPuzzle& P = g.lv.puzzles[pi];
                if (P.state == 3) { if (g.flame.st == F_HELD) done = true; else if (g.flame.st == F_HOVER) press(A_THROW); break; }
                Enemy* e = nearestEnemy(9);
                if (e && P.state != 2) { attack(e->pos + V3(0, e->height * 0.5f, 0)); break; }
                if (P.state == 0) { V3 c2 = P.zone.center(); walk(V3(c2.x, P.zone.lo.y + 1, c2.z), 1.5f); break; }
                if (P.state != 2) { if (g.flame.st == F_HOVER) press(A_THROW); break; }
                if (g.flame.st == F_HOVER) { if (t > 0.05f) press(A_THROW); break; }
                if (g.flame.st != F_HELD) break;
                int slot = P.seq[P.progress];
                for (auto& T : g.lv.terminals) if (T.puzzle == pi && T.slot == slot) {
                    V3 tp = T.pos + V3(std::sin(T.yaw), 0, std::cos(T.yaw)) * 0.2f;
                    aim(tp); face(tp - g.pl.pos);
                    cycT += SIM_DT;
                    if (cycT > 0.15f) { press(A_THROW); cycT = 0; }
                }
                timeout = 90;
            } break;
            case HOLD: {
                // the city helipad: fight whatever comes until the chopper arrives
                const Holdout& H = g.lv.holdout;
                if (H.done) { if (g.flame.st == F_HOVER) press(A_THROW); done = g.flame.st == F_HELD && g.dialog.empty(); break; }
                if (!H.active) { walk(H.pad, 1.5f); break; }
                Enemy* e = nearestEnemy(40);
                if (e) {
                    attack(e->pos + V3(0, e->height * 0.5f, 0), false, 2.f);
                    float d = distXZ(e->pos, g.pl.pos);
                    if (d < 4) in.moveY = -1; else if (d > 12) in.moveY = 1;
                } else { if (g.flame.st == F_HOVER) press(A_THROW); walk(H.pad, 2.f); }
                if (g.ability("firewall") && g.fwCd <= 0 && e && distXZ(e->pos, g.pl.pos) < 6) press(A_FIREWALL);
                timeout = 120;
            } break;
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
                {
                    Enemy* e = nearestEnemy(5.5f);
                    bool down = false; for (auto& w : b.wps) if (w.id == 10 && w.vulnerable) down = true;
                    if (e && !down) { attack(e->pos + V3(0, e->height * 0.5f, 0), false, 1.5f); face(g.pl.pos - e->pos); in.moveY = 1; break; }
                }
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
         N("CLOCK: hacked terminals"), G(0, 18, 172), HACKP(0), E("clock.g2"), G(0, 18, 191),
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
// ---- used by the showcase recorder: the same campaign bot, stepped one sim tick at a time
void* scBotNew(Game& g) { Bot* b = new Bot(g); b->cmds = campaign(); return b; }
void scBotFree(void* p) { delete (Bot*)p; }
bool scBotStep(void* p, InputState& in, std::string& section) {
    Bot* b = (Bot*)p;
    bool ok = b->step();
    b->total += SIM_DT;
    in = b->in; section = b->section;
    return ok || !b->g.dialog.empty();
}

// ---- shared runner for the feature tests below
static int runRoute(Game& g, Bot& bot, float maxT, bool verbose = false) {
    float t = 0;
    while (t < maxT) {
        if (!bot.step() && g.dialog.empty()) break;
        bot.total += SIM_DT;
        g.update(SIM_DT, bot.in); t += SIM_DT;
        if (g.screen != SC_PLAY && g.screen != SC_ENDING) g.screen = SC_PLAY;
        if (verbose && std::fmod(t, 1.f) < SIM_DT) printf("   t=%.0f pc=%zu pos=(%.1f %.1f %.1f) flame=%d\n", t, bot.pc, g.pl.pos.x, g.pl.pos.y, g.pl.pos.z, g.flame.st);
    }
    return bot.failures;
}

static void freshFeatureSave(Game& g) {
    g.botActive = true; g.newGame(); InputState sk; sk.mBack = true; g.frame(0.016f, sk, 0, 0);
    g.dialog.clear();
    for (const char* f : {"talk.marrow0", "talk.fbi", "tut.done", "ab.warp", "ab.bloom", "ab.sunflare", "ab.overclock", "heart.0", "heart.1", "boss.0", "boss.1", "npc.3", "npc.4", "npc.8"}) g.setFlag(f);
}

// ---- used by the update-trailer recorder
void scFeatureSave(Game& g) { freshFeatureSave(g); }
void* scBotRoute(Game& g, int route) {
    Bot* b = new Bot(g);
    switch (route) {
        case 0: b->cmds = {G(0, 0, 30), F(18), G(0, 0, 44), F(18), G(0, 0, 56), F(18)}; break;
        case 1: b->cmds = {G(6, 0, 94), TW(6, 6.6f, 98.6f), TW(-6.5f, 14.6f, 98.6f), TW(0, 16.4f, 101.2f), G(0, 14, 106), F(30)}; break;
        case 2: b->cmds = {HOLDP()}; break;
        case 3: b->cmds = {T(0, 2.2f, 57.5f)}; break;
        case 4: b->cmds = {F(30)}; break;
        case 5: b->cmds = {HACKP(0)}; break;
        case 6: b->cmds = {BOSSF()}; break;
        default: break;
    }
    return b;
}

int runFeatureTest(Game& g, const std::string& which) {
    if (which == "city") {
        // Lumen City: through GL1TCH's backdoor in the Clockwood, to the helipad rescue and home
        freshFeatureSave(g);
        g.godMode = getenv("WICKLIGHT_MORTAL") == nullptr;
        g.setFlag("clock.g2");
        g.enterLevel(LV_CLOCK, 2);
        g.dialog.clear();
        Bot bot(g);
        bot.cmds = {N("CITY: GL1TCH's backdoor"), G(0, 18, 201), G(0, 18, 210), G(19, 18, 218), G(26, 18, 218), EL(LV_CITY),
                    N("CITY: alley"), G(0, 0, 8), F(14), G(0, 0, 20),
                    N("CITY: main street"), G(0, 0, 30), F(18), G(0, 0, 44), F(18), G(0, 0, 56), F(18), G(0, 0, 60),
                    N("CITY: hacked barricade"), HACKP(0), E("city.g1"), G(0, 0, 74), F(22),
                    N("CITY: tower climb"), G(6, 0, 94), TW(6, 6.6f, 98.6f), TW(-6.5f, 14.6f, 98.6f), TW(0, 16.4f, 101.2f), G(0, 14, 106),
                    N("CITY: rooftop ambush"), F(30), E("city.arena"), G(0, 14, 126), G(0, 14, 134), G(0, 14, 145),
                    N("CITY: helipad holdout"), HOLDP(), E("city.holdout"), G(0, 14, 151), W(0.5f), E("pw.grapple"), E("ab.grapple"), E("fbi.done"),
                    G(0, 14, 157), G(0, 14, 160), EL(LV_HUB)};
        runRoute(g, bot, 900, getenv("WL_CTRACE") != nullptr);
        bool ok = bot.failures == 0 && g.lv.id == LV_HUB && g.has("pw.grapple");
        printf("[bot] city: %s in %.0fs, would-be hits %zu, alarms raised, agents fought (failures %d)\n", ok ? "PASS" : "FAIL", bot.total, g.testLog.size(), bot.failures);
        return ok ? 0 : 2;
    }
    if (which == "hacker") {
        // the Clockwood's optional server room: 5-terminal puzzle -> the FIREWALL power; one deliberate mistake first
        freshFeatureSave(g);
        g.godMode = true;
        g.setFlag("clock.g2");
        g.enterLevel(LV_CLOCK, 2);
        g.dialog.clear();
        int pi = 1;
        HackPuzzle& P = g.lv.puzzles[pi];
        Bot bot(g);
        bot.cmds = {G(0, 18, 192), G(-6, 18, 192.5f)};
        runRoute(g, bot, 30);
        // wait for the show, then burn a wrong terminal on purpose
        for (int k = 0; k < 120 * 8 && P.state != 2; k++) g.update(SIM_DT, InputState());
        int wrong = (P.seq[0] + 1) % 5;
        V3 a = g.pl.pos + V3(0, 1.25f, 0), at;
        for (auto& T : g.lv.terminals) if (T.puzzle == pi && T.slot == wrong) g.hitTerminal(a, T.pos, at);
        int drones = 0; for (auto& e : g.enemies) if (e.alive && e.type == E_DRONE) drones++;
        bool denied = P.state == 4 && drones >= 2 && g.glitch > 0.5f;
        printf("[bot] hacker: wrong terminal -> ACCESS DENIED + %d firewall drones + glitch: %s\n", drones, denied ? "OK" : "FAIL");
        Bot b2(g);
        b2.cmds = {HACKP(pi), E("clock.servers"), G(-8, 18, 192.5f), W(0.5f), E("pw.firewall"), E("ab.firewall")};
        runRoute(g, b2, 200);
        bool ok = denied && b2.failures == 0;
        // GL1TCH's new tricks all show up in a long fight
        g.save.flags.erase("boss.1");
        g.enterLevel(LV_CLOCK, 2);
        g.pl.pos = g.lv.boss.center + V3(0, 0, -8); g.startBoss();
        bool sawDecoy = false, sawVirus = false, sawCode = false, sawTele = false;
        V3 p0 = g.boss.v[1];
        g.boss.hp = 6;  // turbo mode
        for (int k = 0; k < 120 * 40; k++) {
            g.update(SIM_DT, InputState());
            for (auto& e : g.enemies) if (e.alive) { sawDecoy |= e.type == E_DECOY; sawVirus |= e.type == E_VIRUS; }
            for (auto& pr : g.projs) sawCode |= pr.type == PR_CODE;
            if (g.boss.i[0] && len(g.boss.v[1] - p0) > 1) sawTele = true;
            g.pl.hp = 99; g.pl.dead = false;
        }
        printf("[bot] hacker: GL1TCH teleports %s, code bolts %s, viruses %s, fake Wicks %s\n", sawTele ? "yes" : "NO", sawCode ? "yes" : "NO", sawVirus ? "yes" : "NO", sawDecoy ? "yes" : "NO");
        ok = ok && sawDecoy && sawVirus && sawCode && sawTele;
        printf("[bot] hacker: %s\n", ok ? "PASS" : "FAIL");
        return ok ? 0 : 2;
    }
    if (which == "powers") {
        freshFeatureSave(g);
        g.godMode = true;
        for (const char* f : {"ab.firewall", "ab.grapple"}) g.setFlag(f);
        int fails = 0;
        auto check = [&](const char* what, bool ok, const std::string& extra = "") { printf("[bot] powers: %-46s %s %s\n", what, ok ? "OK" : "FAIL", extra.c_str()); if (!ok) fails++; };
        auto sim = [&](float secs, InputState in) { for (float t = 0; t < secs; t += SIM_DT) { g.update(SIM_DT, in); in.clearEdges(); g.dialog.clear(); g.screen = SC_PLAY; } };
        // overclock: combos charge it; it slows enemies but not Wick
        g.enterLevel(LV_WHARF, 1);
        g.dialog.clear();
        for (int k = 0; k < 15; k++) g.addCombo(g.pl.pos);
        check("hits charge Overclock to full", g.ocCharge >= 1);
        g.enemies.clear();
        g.spawnEnemy(E_GLOOMLING, g.pl.pos + V3(0, 0.3f, 30), -1);
        g.enemies[0].state = 0;
        InputState oc; oc.pressed[A_OVERCLOCK] = true; sim(SIM_DT, oc);
        bool active = g.ocT > 0;
        float e0 = g.enemies[0].anim; sim(1.0f, InputState()); float slowed = g.enemies[0].anim - e0;
        check("Overclock slows the world to ~30%", active && slowed > 0.25f && slowed < 0.35f, "enemy time in 1s: " + std::to_string(slowed));
        sim(5, InputState());
        check("Overclock wears off", g.ocT <= 0);
        // firewall: gloomlings can't walk through it, and it burns them
        g.enterLevel(LV_WHARF, 1);
        g.dialog.clear();
        g.camYaw = 0; g.camPitch = -0.2f; g.pl.yaw = 0;
        g.updateCamera(SIM_DT); g.computeAim();
        InputState fw; fw.pressed[A_FIREWALL] = true; sim(SIM_DT, fw);
        check("Firewall drops a wall of fire", g.firewalls.size() == 1);
        V3 mid = g.firewalls.empty() ? g.pl.pos : (g.firewalls[0].a + g.firewalls[0].b) * 0.5f;
        g.enemies.clear();
        g.spawnEnemy(E_KNIGHT, mid + V3(0, 0.1f, 6), -1);
        Enemy& kn = g.enemies[0];
        kn.state = 1; kn.yaw = PI;
        float hp0 = kn.hp; bool crossed = false;
        for (int k = 0; k < 120 * 4; k++) { g.update(SIM_DT, InputState()); if (g.enemies[0].pos.z < mid.z - 0.3f) crossed = true; g.dialog.clear(); }
        check("knights can't cross the Firewall, and burn", !crossed && g.enemies[0].hp < hp0, "hp " + std::to_string(g.enemies[0].hp) + "/" + std::to_string(hp0));
        // grapple: throw to a hook, hold grapple, get pulled up to it
        g.enterLevel(LV_HUB, 0);
        g.dialog.clear();
        g.pl.pos = V3(0, 0, 42); g.pl.safePos = g.pl.pos;  // the broken north bridge: hook at (0, 2.2, 57.5)
        Bot bot(g);
        bot.cmds = {T(0, 2.2f, 57.5f)};
        runRoute(g, bot, 10);
        bool hooked = g.flame.st == F_HOVER && g.flame.anchored >= 0;
        float z0 = g.pl.pos.z, maxY = g.pl.pos.y;
        InputState gr; gr.pressed[A_GRAPPLE] = true; gr.down[A_GRAPPLE] = true;
        bool swung = false;
        for (int k = 0; k < 120 * 3; k++) {
            g.update(SIM_DT, gr); gr.pressed[A_GRAPPLE] = false; maxY = std::max(maxY, g.pl.pos.y); if (g.grappling) swung = true;
            if (getenv("WL_GTRACE") && k % 12 == 0) printf("   k=%d grap=%d rope=%.1f pos=(%.1f %.1f %.1f) vel=(%.1f %.1f %.1f) flame=%d anch=%d\n", k, g.grappling, g.ropeLen, g.pl.pos.x, g.pl.pos.y, g.pl.pos.z, g.pl.vel.x, g.pl.vel.y, g.pl.vel.z, g.flame.st, g.flame.anchored);
        }
        check("Ember Grapple pulls Wick across the gap", hooked && swung && g.pl.pos.z > z0 + 10 && g.pl.pos.y > -0.5f, "z " + std::to_string(z0) + " -> " + std::to_string(g.pl.pos.z) + ", peak y " + std::to_string(maxY));
        printf("[bot] powers: %s (%d failures)\n", fails ? "FAIL" : "PASS", fails);
        return fails ? 2 : 0;
    }
    if (which == "fun") {
        freshFeatureSave(g);
        g.godMode = true;
        int fails = 0;
        auto check = [&](const char* what, bool ok, const std::string& extra = "") { printf("[bot] fun: %-48s %s %s\n", what, ok ? "OK" : "FAIL", extra.c_str()); if (!ok) fails++; };
        // 1. the Toy Box: kaboom chain reaction, moon gravity
        g.enterLevel(LV_WHARF, 1); g.dialog.clear();
        g.setFlag("toy.4"); g.setFlag("toy.2");
        g.enemies.clear();
        for (int k = 0; k < 5; k++) g.spawnEnemy(E_GLOOMLING, g.pl.pos + V3(-4 + k * 2.2f, 0.3f, 8), -1);
        for (int k = 0; k < 30; k++) g.update(SIM_DT, InputState());
        g.damageEnemy(g.enemies[0], 99, g.pl.pos, false);
        for (int k = 0; k < 120; k++) g.update(SIM_DT, InputState());
        int alive = 0; for (auto& e : g.enemies) if (e.alive) alive++;
        check("Toy Box: Kaboom chain reaction", alive == 0, std::to_string(alive) + " left of 5");
        { InputState j; j.pressed[A_JUMP] = true; j.down[A_JUMP] = true; float y0 = g.pl.pos.y, peak = y0; for (int k = 0; k < 240; k++) { g.update(SIM_DT, j); j.pressed[A_JUMP] = false; peak = std::max(peak, g.pl.pos.y); }
          check("Toy Box: Moon Gravity jumps", peak - y0 > 5, "jump height " + std::to_string(peak - y0)); }
        g.save.flags.erase("toy.4"); g.save.flags.erase("toy.2");
        // 2. the Gauntlet: fight waves with the bot
        g.enterLevel(LV_ARENA, 0); g.dialog.clear();
        Bot bot(g);
        bot.cmds = {G(0, 0, 0)};
        float t = 0;
        while (t < 420 && g.save.gauntletBest < 6) {
            if (!bot.step()) { bot.cmds = {F(40), W(0.5f)}; bot.pc = 0; }
            if (getenv("WL_GAUNT") && std::fmod(t, 5.f) < SIM_DT) { printf("   t=%.0f wave %d state %d queue %d pos (%.1f %.1f %.1f)\n", t, g.gWave, g.gState, g.gQueue, g.pl.pos.x, g.pl.pos.y, g.pl.pos.z); for (auto& e : g.enemies) if (e.alive) printf("      e type %d (%.1f %.1f %.1f) st %d hp %.1f\n", e.type, e.pos.x, e.pos.y, e.pos.z, e.state, e.hp); }
            g.update(SIM_DT, bot.in); t += SIM_DT;
            g.dialog.clear(); if (g.screen != SC_PLAY) g.screen = SC_PLAY;
        }
        check("the Gauntlet: survive 6 waves", g.save.gauntletBest >= 6, "best wave " + std::to_string(g.save.gauntletBest) + " in " + std::to_string((int)t) + "s");
        // 3. the Rising Dark: an endless climb with a rising tide
        g.enterLevel(LV_CLIMB, 0); g.dialog.clear();
        {
            auto& R = g.lv.route;
            // every step must be makeable: small jumps, or a hook within warp range
            int bad = 0;
            for (size_t i = 1; i < R.size(); i++) {
                float dy = R[i].p.y - R[i - 1].p.y, dxz = lenXZ(R[i].p - R[i - 1].p);
                bool hook = R[i].op == 1;
                if (dy <= 0 || dy > (hook ? 5.0f : 1.6f) || dxz > (hook ? 13.5f : 10.5f)) { bad++; printf("   bad gap %zu: op %d dy %.2f dxz %.2f\n", i, R[i].op, dy, dxz); }
            }
            check("Rising Dark: 420 platforms, every gap makeable", R.size() == 421 && bad == 0, std::to_string(bad) + " bad gaps, top at " + std::to_string((int)R.back().p.y) + "m");
            // the bot climbs for real until the first moving lift
            Bot cb(g); cb.airOk = false;
            size_t lim = 1;
            while (lim < R.size() && R[lim].op != 9 && lim < 14) lim++;
            for (size_t i = 1; i < lim; i++) {
                if (R[i].op == 0) cb.cmds.push_back(G(R[i].p.x, R[i].p.y, R[i].p.z));
                if (R[i].op == 1) { cb.cmds.push_back(TW(R[i].p.x, R[i].p.y + 2.2f, R[i].p.z)); cb.cmds.push_back(W(0.3f)); cb.cmds.push_back(G(R[i].p.x, R[i].p.y, R[i].p.z)); }
            }
            float tt = 0;
            if (getenv("WL_CLIMB")) for (size_t i = 0; i < lim; i++) printf("   route %zu op %d (%.1f %.1f %.1f)\n", i, R[i].op, R[i].p.x, R[i].p.y, R[i].p.z);
            while (tt < 60 && cb.step() && !g.pl.dead) {
                g.update(SIM_DT, cb.in); tt += SIM_DT; g.dialog.clear();
                if (getenv("WL_CLIMB") && std::fmod(tt, 0.25f) < SIM_DT) printf("   t=%.2f pc=%zu pos=(%.1f %.1f %.1f) gnd=%d fails=%d\n", tt, cb.pc, g.pl.pos.x, g.pl.pos.y, g.pl.pos.z, (int)g.pl.grounded, cb.failures);
            }
            for (int k = 0; k < 60; k++) g.update(SIM_DT, InputState());
            float want = R[lim - 1].p.y;
            check("Rising Dark: the bot climbs the first platforms", g.climbOn && g.pl.pos.y > want - 0.5f && !g.pl.dead, "reached " + std::to_string(g.pl.pos.y) + " of " + std::to_string(want) + " (" + std::to_string(lim - 1) + " platforms)");
            // then hop up the rest of the tower: the height counter and the tide follow you
            for (size_t i = lim; i < 120; i++) {
                if (R[i].op == 9) continue;
                g.pl.pos = R[i].p + V3(0, 0.05f, 0); g.pl.vel = V3(0, 0, 0);
                for (int k = 0; k < 30; k++) g.update(SIM_DT, InputState());
                g.dialog.clear();
            }
            int h = g.climbH;
            check("Rising Dark: height counts and the tide follows", h > 150 && g.tideY > g.pl.pos.y - 23 && g.tideY < g.pl.pos.y, "height " + std::to_string(h) + "m, tide " + std::to_string((int)(g.pl.pos.y - g.tideY)) + "m below");
            // stand still and let the dark catch up
            int runs = g.save.climbRuns, emb = g.save.embers;
            for (int k = 0; k < 120 * 30 && g.save.climbRuns == runs; k++) g.update(SIM_DT, InputState());
            for (int k = 0; k < 120 * 4; k++) { g.update(SIM_DT, InputState()); g.dialog.clear(); }
            check("Rising Dark: the tide ends the run and saves the record", g.save.climbRuns == runs + 1 && g.save.climbBest == h && g.save.embers > emb && g.lv.id == LV_CLIMB && !g.climbOn && g.pl.pos.y < 1, "best " + std::to_string(g.save.climbBest) + "m");
        }
        g.writeSave(); SaveData keep = g.save; g.save = SaveData(); g.loadGame();
        check("save/load keeps Gauntlet and climb records", g.save.gauntletBest == keep.gauntletBest && g.save.climbBest == keep.climbBest && g.save.climbRuns == keep.climbRuns);
        printf("[bot] fun: %s (%d failures)\n", fails ? "FAIL" : "PASS", fails);
        return fails ? 2 : 0;
    }
    if (which == "postgame") {
        // after the ending: bounties, the Boss Rush, flame dyes, trophies
        setenv("WICKLIGHT_BOUNTIES", "1", 1);
        setenv("WICKLIGHT_BOUNTY_DAY", "20000", 1);
        freshFeatureSave(g);
        g.godMode = true;
        for (const char* f : {"game.finished", "heart.2", "heart.4", "boss.2", "boss.3", "boss.4", "ab.sunflare", "placed.0", "placed.1", "placed.2", "placed.4", "hub.spire", "hub.spiredoor"}) g.setFlag(f);
        int fails = 0;
        auto check = [&](const char* what, bool ok, const std::string& extra = "") { printf("[bot] postgame: %-46s %s %s\n", what, ok ? "OK" : "FAIL", extra.c_str()); if (!ok) fails++; };
        g.enterLevel(LV_HUB, 4);
        g.dialog.clear();
        // 1. bounties: today's three, and finishing one pays out
        g.ensureBounties();
        printf("[bot] postgame: today's board: %s | %s | %s\n", g.bountyLine(0).c_str(), g.bountyLine(1).c_str(), g.bountyLine(2).c_str());
        int e0 = g.save.embers;
        for (int k = 0; k < 9; k++) for (int n = 0; n < 900; n++) { g.bountyEvent(k, k == 1 ? n : 1); if (g.save.bountyDone == 7) break; }
        check("bounties complete and pay embers", g.save.bountyDone == 7 && g.save.bounties == 3 && g.save.embers >= e0 + 750, std::to_string(g.save.embers - e0) + " embers");
        // 2. the Boss Rush, all five with the bot
        int statue = -1; for (int i = 0; i < (int)g.lv.talkers.size(); i++) if (g.lv.talkers[i].who == 18) statue = i;
        g.talk(statue);
        for (int k = 0; k < 40 && !g.dialog.empty(); k++) { InputState a; a.pressed[A_INTERACT] = true; g.update(SIM_DT, a); for (int j = 0; j < 30; j++) g.update(SIM_DT, InputState()); }
        bool started = g.rushActive;
        Bot bot(g);
        float t = 0; int stages = 0, lastStage = -1;
        while ((g.rushActive || g.transitioning) && t < 900) {
            if (g.rushStage != lastStage) { lastStage = g.rushStage; stages++; bot.cmds = {BOSSF()}; bot.pc = 0; printf("[bot] postgame:   rush stage %d: %s\n", g.rushStage + 1, levelName(g.lv.id)); }
            if (!bot.step()) { bot.pc = 0; }
            g.update(SIM_DT, bot.in); t += SIM_DT;
            g.dialog.clear(); if (g.screen != SC_PLAY) g.screen = SC_PLAY;
        }
        for (int k = 0; k < 240; k++) { g.update(SIM_DT, InputState()); g.dialog.clear(); }
        check("Boss Rush: all five echoes, back to back", started && g.has("rush.done") && g.save.rushBest > 0 && g.lv.id == LV_HUB, std::to_string(stages) + " stages, time " + std::to_string(g.save.rushBest) + "s");
        check("still finished (no second ending)", g.has("game.finished") && g.screen == SC_PLAY);
        // 3. flame dyes
        g.save.embers = 6000;
        g.screen = SC_DYES; g.shopSel = 7;
        { InputState ok; ok.mOk = true; g.updateDyes(ok); }
        check("buy the Rainbow flame from Wren", g.has("dye.7") && g.save.dye == 7 && g.save.embers == 1000);
        g.screen = SC_PLAY;
        // 4. trophies are awarded one at a time
        for (int k = 0; k < 40; k++) { g.toastT = 0; g.checkTrophies(); }
        int tro = g.countFlags("tro.");
        check("trophies awarded", tro >= 5 && g.has("tro.25") && g.has("tro.29"), std::to_string(tro) + " of 30");
        // 5. saves keep all of it
        g.writeSave(); SaveData keep = g.save; g.save = SaveData(); g.loadGame();
        check("save/load keeps dye, stats, bounties", g.save.dye == 7 && g.save.bounties == keep.bounties && g.save.bountyDone == 7 && std::fabs(g.save.rushBest - keep.rushBest) < 0.01f);
        printf("[bot] postgame: %s (%d failures)\n", fails ? "FAIL" : "PASS", fails);
        return fails ? 2 : 0;
    }
    if (which == "extras") {
        freshFeatureSave(g);
        g.godMode = true;
        int fails = 0;
        auto check = [&](const char* what, bool ok, const std::string& extra = "") { printf("[bot] extras: %-46s %s %s\n", what, ok ? "OK" : "FAIL", extra.c_str()); if (!ok) fails++; };
        auto talkTo = [&](int who) {
            for (int i = 0; i < (int)g.lv.talkers.size(); i++) if (g.lv.talkers[i].who == who) {
                g.talk(i);
                for (int k = 0; k < 40 && !g.dialog.empty(); k++) { InputState a; a.pressed[A_INTERACT] = true; g.update(SIM_DT, a); for (int j = 0; j < 30; j++) g.update(SIM_DT, InputState()); }
                return true;
            }
            return false;
        };
        g.enterLevel(LV_HUB, 0);
        g.dialog.clear();
        // 1. Mira's cat, up in the treehouse
        talkTo(11);
        Bot b1(g);
        b1.cmds = {G(-8, 0, -14), G(-18, 0, -8), G(-21, 0, 0), TW(-26.3f, 8.6f, 0), G(-27.6f, 7, 0.3f), TALKC(), E("quest.catfound"), G(-26.5f, 7, 0), G(-24, 0, 0), G(-22.3f, 0, -2.6f)};
        runRoute(g, b1, 60);
        talkTo(11);
        check("Mira's cat: rescued, Cat Ears received", b1.failures == 0 && g.has("quest.catdone") && g.has("hat.1"));
        // 2. Old Fenn's six braziers
        talkTo(13);
        Bot b2(g);
        struct LP { V3 stand, lamp; } lamps[] = {{V3(-28, 0, 7), V3(-33, 0, 9)}, {V3(-6, 0, 28), V3(-2, 0, 31)}, {V3(18, 0, 25), V3(18, 0, 31)},
                                                 {V3(26, 0, 0), V3(31, 0, -2)}, {V3(16, 0, -22), V3(16, 0, -28)}, {V3(-16, 0, -22), V3(-18, 0, -28)}};
        b2.cmds.push_back(G(-20, 0, 4));
        for (auto& l : lamps) { if (l.lamp.x == 18 && l.lamp.z == 31) b2.cmds.push_back(G(0, 0, 23));
            if (l.lamp.x == -18 && l.lamp.z == -28) { b2.cmds.push_back(G(9, 0, -16)); b2.cmds.push_back(G(-9, 0, -16)); } b2.cmds.push_back(G(l.stand.x, 0, l.stand.z)); b2.cmds.push_back(T(l.lamp.x, 1.35f, l.lamp.z)); b2.cmds.push_back(R_()); }
        b2.cmds.push_back(E("quest.lamps"));
        b2.cmds.push_back(G(-10, 0, -8)); b2.cmds.push_back(G(-6, 0, 12));
        runRoute(g, b2, 120);
        talkTo(13);
        check("Old Fenn's braziers: all six lit, cap received", b2.failures == 0 && g.has("quest.fenndone") && g.has("hat.2"));
        // 3. the candle race (the bot runs it with dashes)
        g.pl.pos = V3(2, 0, 12); g.pl.vel = V3();
        talkTo(14);
        bool started = g.raceIdx == 0;
        {
            float t = 0;
            while (g.raceIdx >= 0 && t < 80) {
                InputState in;
                const RaceRing& r = g.lv.race[g.raceIdx];
                V3 d = flatXZ(r.pos - g.pl.pos);
                if (lenXZ(d) > 0.01f) g.camYaw = std::atan2(d.x, d.z);
                in.moveY = 1;
                if (g.pl.grounded && g.pl.dashCd <= 0 && lenXZ(d) > 5) { in.pressed[A_DASH] = true; in.down[A_DASH] = true; }
                if (r.high && lenXZ(d) < 3.2f && g.pl.grounded) { in.pressed[A_JUMP] = true; in.down[A_JUMP] = true; }
                else if (r.high && !g.pl.grounded) in.down[A_JUMP] = true;
                g.update(SIM_DT, in); t += SIM_DT;
            }
        }
        float best = g.save.raceBest;
        for (int k = 0; k < 40 && !g.dialog.empty(); k++) { InputState a; a.pressed[A_INTERACT] = true; g.update(SIM_DT, a); for (int j = 0; j < 30; j++) g.update(SIM_DT, InputState()); }
        check("candle race: all 12 rings, medal + crown", started && best > 0 && g.has("race.m1"), "time " + std::to_string(best) + "s" + (g.has("hat.4") ? " GOLD (crown)" : (g.has("race.m2") ? " silver" : " bronze")));
        // 4. the mirror swaps hats
        int h0 = g.save.hat; g.cycleHat(); int h1 = g.save.hat; g.dialog.clear();
        check("wardrobe mirror swaps hats", h0 != h1, std::to_string(h0) + " -> " + std::to_string(h1));
        // 5. fishing for Gil's Golden Carp
        g.enterLevel(LV_WHARF, 0);
        g.dialog.clear();
        talkTo(4);
        bool quest = g.has("quest.carp");
        int caught0 = g.save.fishCaught;
        for (int tries = 0; tries < 12 && !g.has("fish.3"); tries++) {
            g.pl.pos = V3(1.8f, 0, 7.5f); g.pl.vel = V3();
            int spot = -1; for (int i = 0; i < (int)g.lv.talkers.size(); i++) if (g.lv.talkers[i].who == 16) { spot = i; break; }
            g.talk(spot);
            for (int k = 0; k < 120 * 6 && g.fishState == 1; k++) g.update(SIM_DT, InputState());
            { InputState a; a.pressed[A_INTERACT] = true; g.update(SIM_DT, a); }
            for (int k = 0; k < 120 * 6 && g.fishState == 3; k++) {
                InputState a;
                if (std::fabs(g.fishNeedle - g.fishZone) < g.fishZoneW * 0.3f) a.pressed[A_INTERACT] = true;
                g.update(SIM_DT, a);
            }
            g.dialog.clear();
        }
        talkTo(4);
        check("fishing: Golden Carp caught, Fisher's Hat received", quest && g.has("fish.3") && g.has("hat.3"), std::to_string(g.save.fishCaught - caught0) + " fish caught");
        // 5b. fishing on the Hollow's west docks, with real button presses (pressing E while waiting must not reel in)
        {
            g.enterLevel(LV_HUB, 1); g.dialog.clear();
            int spot = -1; for (int i = 0; i < (int)g.lv.talkers.size(); i++) if (g.lv.talkers[i].who == 16) { spot = i; break; }
            int c0 = g.save.fishCaught;
            bool held = true;
            for (int tries = 0; tries < 6 && g.save.fishCaught == c0 && spot >= 0; tries++) {
                g.pl.pos = g.lv.talkers[spot].pos + V3(0.6f, 0, -0.8f); g.pl.vel = V3(); g.pl.safePos = g.pl.pos;
                for (int k = 0; k < 20; k++) g.update(SIM_DT, InputState());
                { InputState a; a.pressed[A_INTERACT] = true; a.down[A_INTERACT] = true; g.update(SIM_DT, a); }
                for (int k = 0; k < 60; k++) g.update(SIM_DT, InputState());
                { InputState a; a.pressed[A_INTERACT] = true; g.update(SIM_DT, a); }  // impatient press while waiting
                if (g.fishState != 1) held = false;
                for (int k = 0; k < 120 * 6 && g.fishState == 1; k++) g.update(SIM_DT, InputState());
                for (int k = 0; k < 40; k++) g.update(SIM_DT, InputState());   // a slow-ish reaction (1/3 s)
                { InputState a; a.pressed[A_INTERACT] = true; g.update(SIM_DT, a); }
                for (int k = 0; k < 120 * 6 && g.fishState == 3; k++) {
                    InputState a;
                    if (std::fabs(g.fishNeedle - g.fishZone) < g.fishZoneW * 0.3f) a.pressed[A_INTERACT] = true;
                    g.update(SIM_DT, a);
                }
                g.dialog.clear();
            }
            check("fishing on the Hollow docks with real key presses", spot >= 0 && held && g.save.fishCaught > c0, held ? "" : "an early press reeled the line in");
        }
        // 6. secret hats: pick up the Top Hat on the tiny island
        g.enterLevel(LV_WHARF, 0); g.dialog.clear();
        g.pl.pos = V3(-14, 0.3f, 78.5f); g.pl.vel = V3();
        for (int k = 0; k < 60; k++) g.update(SIM_DT, InputState());
        g.dialog.clear();
        check("secret Top Hat on the Wharf islet", g.has("hat.5") && g.save.hat == 5);
        // 7. save/load keeps hat + fish + race
        g.writeSave(); SaveData keep = g.save; g.save = SaveData(); g.loadGame();
        check("save/load keeps hats, fish and race times", g.save.hat == keep.hat && g.save.fishCaught == keep.fishCaught && std::fabs(g.save.raceBest - keep.raceBest) < 0.01f);
        printf("[bot] extras: %s (%d failures)\n", fails ? "FAIL" : "PASS", fails);
        return fails ? 2 : 0;
    }
    return -1;
}

int runAutotest(Game& g, const std::string& which) {
    setenv("WICKLIGHT_NO_SAVE", "", 0);
    { int r = runFeatureTest(g, which); if (r >= 0) return r; }
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
