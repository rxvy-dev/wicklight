// WICKLIGHT - player movement, the Flame (throw / recall / warp / bloom / sunflare), camera and aiming
#include "game.h"
#include <cstdio>
#include <cstdlib>

int g_dmgKind = 0;  // 0 flame, 1 sunflare (charged), 2 bloom

static V3 headPos(const Player& p) { return p.pos + V3(0, 1.25f, 0); }

void Game::burst(V3 p, V3 col, int n, float speed, float size, int kind, float life, float grav) {
    if (headless || !R) return;
    static Rng rr(777);
    for (int i = 0; i < n; i++) {
        Particle q;
        V3 d = norm(V3(rr.range(-1, 1), rr.range(-1, 1), rr.range(-1, 1)));
        q.pos = p; q.vel = d * speed * rr.range(0.3f, 1.f);
        q.color = col; q.alpha = 1; q.size = size * rr.range(0.6f, 1.2f);
        q.life = q.maxLife = life * rr.range(0.6f, 1.2f);
        q.gravity = grav; q.drag = 2; q.kind = kind;
        R->spawn(q);
    }
}

// ------------------------------------------------------------------ camera
void Game::updateCamera(float dt) {
    // on Wick's Island you can look (almost) straight down and up, for building
    if (lv.id == LV_HOME && vox.active) camPitch = clampf(camPitch, -1.53f, 1.1f);
    else camPitch = clampf(camPitch, -1.25f, 0.85f);
    V3 fwd(std::sin(camYaw) * std::cos(camPitch), std::sin(camPitch), std::cos(camYaw) * std::cos(camPitch));
    V3 right(-std::cos(camYaw), 0, std::sin(camYaw));
    float wantDist = camDist;
    if (boss.active && !boss.dead) wantDist = 7.5f;
    V3 desiredTarget = pl.pos + V3(0, 1.65f, 0) + right * 0.55f;
    float k = 1 - std::exp(-dt * 14);
    camTarget = lerp(camTarget, desiredTarget, k);
    if (len(camTarget - desiredTarget) > 6) camTarget = desiredTarget;  // after teleports
    // collision pull-in
    V3 back = -fwd;
    extern bool g_rayIgnoreBarriers;
    g_rayIgnoreBarriers = true;
    float hit = raycast(camTarget, back, wantDist + 0.3f, nullptr, nullptr, false);
    g_rayIgnoreBarriers = false;
    float d = wantDist;
    if (hit >= 0) d = std::max(0.6f, hit - 0.3f);
    if (d < camDistCur) camDistCur = d;
    else camDistCur = approach(camDistCur, d, dt * 6);
    camEye = camTarget + back * camDistCur;
    if (camOverride > 0) {
        float t = smooth01(camOverride);
        camEye = lerp(camEye, camOverrideEye, t);
        camTarget = lerp(camTarget, camOverrideTarget, t);
    }
}

void Game::computeAim() {
    V3 fwd = norm(camTarget - camEye);
    if (camOverride > 0) fwd = V3(std::sin(camYaw) * std::cos(camPitch), std::sin(camPitch), std::cos(camYaw) * std::cos(camPitch));
    float t = raycast(camEye, fwd, 90);
    aimPoint = t >= 0 ? camEye + fwd * t : camEye + fwd * 90;
    V3 head = headPos(pl);
    aimDir = norm(aimPoint - head);
    aimTarget = -1;
    if (botAim) { aimDir = norm(botAimPoint - head); return; }
    if (!settings.aimAssist) return;
    float range = (flame.charging && flame.charge > 0.75f) ? 26.f : 18.f;
    float bestScore = 1e9f; V3 best;
    auto consider = [&](const V3& c, float maxDeg, int kind) {
        V3 v = c - camEye;
        float l = len(v);
        if (l < 0.5f) return;
        float cosA = dot(v / l, fwd);
        float ang = std::acos(clampf(cosA, -1, 1)) * 180 / PI;
        if (ang > maxDeg) return;
        if (len(c - head) > range) return;
        V3 dd = c - head; float dl = len(dd);
        float hitT = raycast(head, dd / dl, dl, nullptr, nullptr, false);
        if (hitT >= 0 && hitT < dl - 0.6f) return;
        float score = ang + l * 0.05f;
        if (score < bestScore) { bestScore = score; best = c; aimTarget = kind; }
    };
    for (auto& e : enemies) {
        if (!e.alive) continue;
        if (e.type == E_WRAITH && e.reveal < 0.3f) continue;
        consider(e.pos + V3(0, e.height * 0.5f, 0), 9, 1);
    }
    if (boss.active && !boss.dead)
        for (auto& w : boss.wps) consider(w.pos, w.vulnerable ? 12.f : 6.f, w.vulnerable ? 2 : 1);
    for (auto& b : lv.braziers) if (!b.lit && !b.sealed) consider(b.pos, 7, 3);
    for (auto& a : lv.anchors) consider(a.pos, 6, 4);
    if (aimTarget >= 0) aimDir = norm(best - head);
}

// ------------------------------------------------------------------ player
void Game::hurtPlayer(float dmg, V3 from) {
    if (pl.dead || pl.invuln > 0 || pl.dashT > 0) return;
    if (godMode) { testLog.push_back('x'); pl.invuln = 1.1f; return; }
    if (botEquipped(3) && bots[3].charge >= 1) {
        bots[3].charge = 0;
        pl.invuln = 0.8f;
        audio::play(SFX_SHIELD, 1, 1.2f);
        burst(pl.pos + V3(0, 0.8f, 0), hexColor(0xc080ff), 20, 5, 0.1f, 3, 0.5f, 0);
        floatText(pl.pos + V3(0, 2, 0), "BLOCKED!", hexColor(0xc080ff));
        return;
    }
    pl.hp -= dmg;
    if (combo >= 6) { statusMsg = "Combo broken!"; statusT = 1.2f; }
    combo = 0;
    if (getenv("WL_HURTLOG")) fprintf(stderr, "HURT %.1f from (%.1f %.1f %.1f) at (%.1f %.1f %.1f) hp=%.1f t=%.1f\n", dmg, from.x, from.y, from.z, pl.pos.x, pl.pos.y, pl.pos.z, pl.hp, time);
    pl.invuln = 1.1f;
    pl.hurtT = 0.35f;
    V3 away = flatXZ(pl.pos - from);
    if (len(away) < 0.01f) away = V3(0, 0, 1);
    pl.vel = norm(away) * 7.f + V3(0, 6.f, 0);
    pl.grounded = false;
    audio::play(SFX_HURT);
    addShake(0.35f);
    hitstop = 0.06f;
    flashOverlay = 0.35f; flashColor = hexColor(0xff2a2a);
    burst(pl.pos + V3(0, 0.8f, 0), hexColor(0xffe0b0), 14, 5, 0.08f, 0);
    if (pl.hp <= 0) killPlayer();
}

void Game::killPlayer() {
    if (pl.dead) return;
    pl.dead = true; pl.deadT = 0; pl.hp = 0;
    save.deaths++;
    audio::play(SFX_DEATH);
    flame.st = F_HELD;
    burst(pl.pos + V3(0, 1.2f, 0), hexColor(0xffa040), 30, 6, 0.1f, 3, 1.2f);
}

void Game::respawnAtLantern() {
    if (save.level == LV_HOME) respawnAtBed = true;
    enterLevel((LevelId)save.level, save.spawn);
    pl.hp = maxHp();
    respawnFade = 1;
}

void Game::updatePlayer(float dt, const InputState& in) {
    Player& p = pl;
    if (p.dead) {
        p.deadT += dt;
        p.vel = V3(0, 0, 0);
        if (p.deadT > 1.8f) respawnAtLantern();
        return;
    }
    p.invuln = std::max(0.f, p.invuln - dt);
    p.hurtT = std::max(0.f, p.hurtT - dt);
    p.dashCd = std::max(0.f, p.dashCd - dt);
    p.land = std::max(0.f, p.land - dt * 4);

    V3 fwd(std::sin(camYaw), 0, std::cos(camYaw));
    V3 right(-std::cos(camYaw), 0, std::sin(camYaw));
    V3 wish = fwd * in.moveY + right * in.moveX;
    float wl = len(wish);
    if (wl > 0.3f && !has("tut.move")) setFlag("tut.move");
    if (wl > 1) { wish = wish / wl; wl = 1; }

    // jump buffering & coyote time
    if (in.pressed[A_JUMP]) p.jumpBuf = 0.16f;
    else p.jumpBuf = std::max(0.f, p.jumpBuf - dt);
    if (p.grounded) { p.coyote = 0.13f; p.airTime = 0; p.airJumps = upLevel("jump"); }
    else { p.coyote = std::max(0.f, p.coyote - dt); p.airTime += dt; }

    // dash
    static bool airDashUsed = false;
    if (p.grounded) airDashUsed = false;
    if (in.pressed[A_DASH] && p.dashCd <= 0 && p.dashT <= 0 && !(airDashUsed && !p.grounded)) {
        p.dashDir = wl > 0.1f ? wish : V3(std::sin(p.yaw), 0, std::cos(p.yaw));
        p.dashT = 0.17f; p.dashCd = 0.55f;
        if (!p.grounded) airDashUsed = true;
        audio::play(SFX_DASH);
        if (!has("tut.dash")) setFlag("tut.dash");
        burst(p.pos + V3(0, 0.6f, 0), hexColor(0xffd890), 10, 3, 0.07f, 3, 0.4f, 0);
    }
    if (p.dashT > 0) {
        p.dashT -= dt;
        p.vel.x = p.dashDir.x * 21; p.vel.z = p.dashDir.z * 21;
        p.vel.y = std::max(p.vel.y, 0.f) * 0.5f;
        if (headless == false && R && ((int)(p.dashT * 120) % 3 == 0)) {
            Particle q; q.pos = p.pos + V3(0, 0.6f, 0); q.vel = V3(0, 0.5f, 0); q.color = hexColor(0xffb050); q.alpha = 0.8f;
            q.size = 0.25f; q.life = q.maxLife = 0.25f; q.kind = 1; R->spawn(q);
        }
    } else {
        float maxSpeed = 7.2f;
        if (flame.charging) maxSpeed = 4.0f;
        float accel = p.grounded ? 70.f : 28.f;
        V3 hv(p.vel.x, 0, p.vel.z);
        V3 target = wish * maxSpeed;
        V3 diff = target - hv;
        float dl = len(diff);
        float maxD = (wl > 0.05f ? accel : (p.grounded ? 55.f : 8.f)) * dt;
        // after a grapple swing, keep the momentum (only gentle air control)
        if (p.flingT > 0 && !p.grounded) { maxD = (wl > 0.05f ? 9.f : 1.5f) * dt; if (lenXZ(hv) < maxSpeed) maxD = std::max(maxD, 20.f * dt); }
        if (dl > maxD) diff = diff * (maxD / dl);
        if (!grappling) { p.vel.x += diff.x; p.vel.z += diff.z; }
        // gravity
        p.vel.y -= GRAVITY * dt;
        if (p.vel.y < -32) p.vel.y = -32;
        if (p.jumpBuf > 0 && p.coyote > 0) {
            p.vel.y = 11.5f; p.jumpBuf = 0; p.coyote = 0; p.grounded = false;
            audio::play(SFX_JUMP, 0.8f);
            if (!has("tut.jump")) setFlag("tut.jump");
        } else if (p.jumpBuf > 0 && !p.grounded && p.airJumps > 0 && p.vel.y < 6) {
            // Second Wind upgrade: a flame-powered air jump
            p.vel.y = 10.5f; p.jumpBuf = 0; p.airJumps--;
            audio::play(SFX_JUMP, 0.8f, 1.3f);
            burst(p.pos + V3(0, 0.2f, 0), hexColor(0xffa040), 14, 3, 0.08f, 3, 0.4f, 2);
        }
        if (in.released[A_JUMP] && p.vel.y > 4 && !grappling && p.flingT <= 0) p.vel.y *= 0.55f;
    }
    p.flingT = p.grounded ? 0.f : std::max(0.f, p.flingT - dt);
    updateGrapple(dt, in);
    // facing
    V3 hv(p.vel.x, 0, p.vel.z);
    if (flame.charging) p.yaw = angleApproach(p.yaw, std::atan2(aimDir.x, aimDir.z), dt * 14);
    else if (lenXZ(hv) > 0.5f) p.yaw = angleApproach(p.yaw, std::atan2(hv.x, hv.z), dt * 14);

    if (lv.id == LV_HOME && vox.active) voxPlayerPhysics(dt, in);
    bool wasGrounded = p.grounded;
    float fallSpeed = -p.vel.y;
    moveBox(p.pos, p.vel, PLAYER_R, PLAYER_H, dt, p.grounded, &p.groundDyn, true);
    if (p.grounded && !wasGrounded) {
        if (fallSpeed > 8) { audio::play(SFX_LAND, std::min(1.f, fallSpeed / 20)); p.land = 1; burst(p.pos, hexColor(0x8a8090), 6, 2, 0.06f, 0, 0.4f, 6); }
    }
    // footsteps / animation phase
    float sp = lenXZ(p.vel);
    if (p.grounded && sp > 0.5f) {
        p.walkPhase += dt * sp * 1.6f;
        p.stepT -= dt * sp;
        if (p.stepT <= 0) { p.stepT = 2.2f; audio::play(SFX_STEP, 0.5f, 0.9f + 0.2f * hashf((int)(time * 10), 1)); }
    } else p.walkPhase = approach(p.walkPhase, std::round(p.walkPhase / PI) * PI, dt * 6);

    // record a safe position (fully supported, static ground)
    if (p.grounded && p.groundDyn == -1) {
        p.safeT += dt;
        if (p.safeT > 0.1f) {
            bool ok = true;
            for (int c = 0; c < 4 && ok; c++) {
                V3 o = p.pos + V3((c & 1) ? 0.4f : -0.4f, 0.3f, (c & 2) ? 0.4f : -0.4f);
                float t = raycast(o, V3(0, -1, 0), 0.7f);
                if (t < 0) ok = false;
            }
            if (ok) p.safePos = p.pos;
        }
    } else p.safeT = 0;

    // falling into water / abyss
    bool inWater = lv.waterY > -999 && p.pos.y < lv.waterY - 0.5f && lv.waterArea.contains(V3(p.pos.x, lv.waterY, p.pos.z));
    if (inWater || p.pos.y < lv.killY) {
        if (inWater) { audio::play(SFX_SPLASH); burst(V3(p.pos.x, lv.waterY, p.pos.z), hexColor(0x9ad8e8), 20, 6, 0.1f, 0, 0.7f, 12); }
        bool wasInv = p.invuln > 0;
        p.invuln = 0;
        float before = p.hp;
        hurtPlayer(1, p.pos);
        if (godMode || wasInv) p.hp = before;  // still return to safety
        if (!p.dead) {
            p.pos = p.safePos; p.vel = V3(0, 0, 0); p.invuln = 1.2f;
            flame.st = F_HELD;
            respawnFade = 0.6f;
        }
    }
}

// ------------------------------------------------------------------ flame
static float segPointDist(const V3& a, const V3& b, const V3& c) {
    V3 ab = b - a; float l2 = len2(ab);
    float t = l2 > 1e-8f ? clampf(dot(c - a, ab) / l2, 0, 1) : 0;
    return len(a + ab * t - c);
}
bool Game::flameHitTest(const V3& a, const V3& b, const V3& c, float r) { return segPointDist(a, b, c) < r; }

void Game::throwFlame(bool charged) {
    V3 head = headPos(pl);
    computeAim();
    if (!has("tut.throw")) setFlag("tut.throw");
    flame.st = F_FLYING;
    flame.charged = charged;
    flame.pos = head + aimDir * 0.3f;
    flame.prevPos = head;
    flame.throwOrigin = head;
    float swift = 1 + 0.15f * upLevel("swift");
    flame.vel = aimDir * (charged ? 40.f : 30.f) * swift;
    flame.maxDist = (charged ? 25.f : 17.f) + 2.5f * upLevel("swift");
    flame.traveled = 0;
    flame.hitIds.clear();
    flame.anchored = -1;
    flame.smother = 0;
    pl.yaw = std::atan2(aimDir.x, aimDir.z);
    audio::play(charged ? SFX_SUNFLARE : SFX_THROW);
    if (charged) { addShake(0.15f); burst(head, hexColor(0xfff0a0), 16, 7, 0.09f, 3, 0.5f, 0); }
}

void Game::recallFlame() {
    if (flame.st == F_HELD || flame.st == F_RETURNING) return;
    flame.st = F_RETURNING;
    if (!has("tut.recall")) setFlag("tut.recall");
    flame.traveled = 0;
    flame.hitIds.clear();
    flame.anchored = -1;
    flame.vel = V3(0, 0, 0);
    audio::playAt(SFX_RECALL, flame.pos);
    // clinging moths get dragged along and burn
    for (auto& e : enemies)
        if (e.alive && e.type == E_MOTH && e.clinging) { e.clinging = false; g_dmgKind = 0; damageEnemy(e, 1, flame.pos, false); }
    flame.smother = 0;
}

void Game::doWarp() {
    if (wardBlocks(pl.pos + V3(0, 1.0f, 0), flame.pos)) {
        audio::play(SFX_SHIELD, 0.8f, 0.5f);
        statusMsg = "A ward of shadow blocks the warp"; statusT = 1.5f;
        return;
    }
    V3 target = flame.pos - V3(0, 0.55f, 0);
    // if the flame rests on a surface, put the feet on that surface
    float g = raycast(flame.pos + V3(0, 0.1f, 0), V3(0, -1, 0), 3.2f);
    if (g >= 0) target.y = flame.pos.y + 0.1f - g + 0.01f;
    static const V3 offs[] = {V3(0, 0, 0), V3(0, 0.3f, 0), V3(0, 0.7f, 0), V3(0, 1.1f, 0), V3(0, -0.5f, 0), V3(0.5f, 0, 0), V3(-0.5f, 0, 0),
                              V3(0, 0, 0.5f), V3(0, 0, -0.5f), V3(0.5f, 0.6f, 0.5f), V3(-0.5f, 0.6f, -0.5f), V3(0.5f, 0.6f, -0.5f), V3(-0.5f, 0.6f, 0.5f)};
    for (auto& o : offs) {
        V3 t = target + o;
        AABB a(V3(t.x - PLAYER_R, t.y, t.z - PLAYER_R), V3(t.x + PLAYER_R, t.y + PLAYER_H, t.z + PLAYER_R));
        if (boxFree(a)) {
            V3 from = pl.pos;
            burst(from + V3(0, 0.6f, 0), hexColor(0xffc060), 18, 4, 0.1f, 1, 0.5f, 0);
            pl.pos = t;
            pl.vel = V3(0, 2.5f, 0);
            pl.grounded = false;
            pl.airJumps = upLevel("jump");
            pl.invuln = std::max(pl.invuln, 0.25f);
            flame.st = F_HELD;
            flame.hitIds.clear();
            flame.warpCd = 0.3f;
            flame.anchored = -1;
            flame.smother = 0;
            for (auto& e : enemies) if (e.type == E_MOTH) e.clinging = false;
            audio::play(SFX_WARP);
            burst(t + V3(0, 0.6f, 0), hexColor(0xfff0b0), 24, 6, 0.1f, 3, 0.5f, 0);
            flashOverlay = std::max(flashOverlay, 0.12f); flashColor = hexColor(0xffe0a0);
            return;
        }
    }
    audio::play(SFX_MENU_BACK);
    statusMsg = "No room to warp there"; statusT = 1.2f;
}

void Game::doBloom() {
    V3 c = flame.st == F_HELD ? headPos(pl) : flame.pos;
    const float R0 = 4.5f + 0.9f * upLevel("bloom");
    float heat = 1 + 0.5f * upLevel("heat");
    for (int i = 0; i < (int)lv.breakables.size(); i++) if (!lv.breakables[i].broken && len(lv.breakables[i].pos - c) < R0) breakUrn(i);
    g_dmgKind = 2;
    for (auto& e : enemies) {
        if (!e.alive) continue;
        if (len(e.pos + V3(0, e.height / 2, 0) - c) < R0 + e.radius) damageEnemy(e, 3 * heat, c, true);
    }
    if (boss.active && !boss.dead) {
        auto wps = boss.wps;
        for (auto& w : wps) if (len(w.pos - c) < R0 + w.r) bossDamage(w.id, 3 * heat, true);
    }
    g_dmgKind = 0;
    for (int i = 0; i < (int)lv.braziers.size(); i++)
        if (!lv.braziers[i].lit && len(lv.braziers[i].pos - c) < R0 + 0.5f) lightBrazier(i, true);
    for (auto& d : lv.dyn) {
        if (d.kind != DK_SEAL || d.broken) continue;
        AABB b = d.box();
        V3 q(clampf(c.x, b.lo.x, b.hi.x), clampf(c.y, b.lo.y, b.hi.y), clampf(c.z, b.lo.z, b.hi.z));
        if (len(q - c) < R0) {
            d.broken = true;
            if (!d.flag.empty()) setFlag(d.flag);
            audio::playAt(SFX_BREAK, b.center());
            burst(b.center(), hexColor(0xd04a3a), 40, 7, 0.14f, 0, 1.0f, 12);
            addShake(0.3f);
        }
    }
    for (auto& pr : projs) if (pr.alive && len(pr.pos - c) < R0 && pr.type != PR_GEAR) { pr.alive = false; burst(pr.pos, hexColor(0x6a4a8a), 6, 3, 0.08f, 0); }
    bloomFlash = 1; bloomPos = c; bloomR = 0;
    flame.st = F_HELD; flame.hitIds.clear(); flame.anchored = -1; flame.smother = 0;
    for (auto& e : enemies) if (e.type == E_MOTH) e.clinging = false;
    flame.bloomCd = 2.5f - 0.5f * upLevel("bloom");
    audio::playAt(SFX_BLOOM, c);
    addShake(0.4f);
    burst(c, hexColor(0xffb040), 60, 11, 0.14f, 3, 0.7f, 2);
    burst(c, hexColor(0xfff0c0), 20, 5, 0.3f, 1, 0.5f, 0);
}

void Game::updateFlame(float dt, const InputState& in) {
    Flame& f = flame;
    f.bloomCd = std::max(0.f, f.bloomCd - dt);
    f.warpCd = std::max(0.f, f.warpCd - dt);
    f.flicker += dt;
    V3 head = headPos(pl);
    if (pl.dead) { f.st = F_HELD; f.pos = head; f.charging = false; return; }

    // --- input
    bool canSunflare = ability("sunflare");
    if (f.st == F_HELD) {
        if (in.pressed[A_THROW]) {
            if (canSunflare) { f.charging = true; f.charge = 0; }
            else throwFlame(false);
        }
        if (f.charging) {
            float before = f.charge;
            f.charge += dt;
            if (before < 0.75f && f.charge >= 0.75f) { audio::play(SFX_CHARGE, 0.8f, 1.4f); burst(head, hexColor(0xfff0a0), 10, 3, 0.07f, 3, 0.3f, 0); }
            if (before < 0.2f && f.charge >= 0.2f) audio::play(SFX_CHARGE, 0.6f);
            if (!in.down[A_THROW]) { f.charging = false; throwFlame(f.charge >= 0.75f); }
        }
    } else {
        f.charging = false;
        if (in.pressed[A_THROW] && (f.st == F_FLYING || f.st == F_HOVER)) recallFlame();
    }
    if (in.pressed[A_WARP]) {
        if (!ability("warp")) { statusMsg = "Warp is learned from the Sunken Wharf's Beacon-Heart"; statusT = 1.5f; }
        else if ((f.st == F_FLYING || f.st == F_HOVER) && f.warpCd <= 0) doWarp();
    }
    if (in.pressed[A_BLOOM]) {
        if (!ability("bloom")) { statusMsg = "Bloom is learned from the Clockwood's Beacon-Heart"; statusT = 1.5f; }
        else if (f.bloomCd <= 0) doBloom();
    }

    // --- states
    f.prevPos = f.pos;
    switch (f.st) {
        case F_HELD:
            f.pos = head + V3(0, 0.05f * std::sin(time * 6), 0);
            break;
        case F_FLYING: {
            V3 step = f.vel * dt;
            float sl = len(step);
            V3 dir = step / sl;
            V3 n; int dh;
            float t = raycast(f.pos, dir, sl + 0.2f, &n, &dh, false);
            if (t >= 0 && dh >= 0 && lv.dyn[dh].kind == DK_CRYSTAL && f.charged) {
                auto& d = lv.dyn[dh];
                d.broken = true;
                if (!d.flag.empty()) setFlag(d.flag);
                audio::playAt(SFX_BREAK, d.box().center(), 1, 1.4f);
                burst(d.box().center(), hexColor(0xc8f8ff), 50, 8, 0.12f, 3, 1.0f, 10);
                addShake(0.25f);
                t = raycast(f.pos, dir, sl + 0.2f, &n, &dh, false);
            }
            V3 np = f.pos + step;
            bool stop = false;
            if (t >= 0 && t <= sl + 0.2f) {
                np = f.pos + dir * std::max(0.f, t - 0.25f) + n * 0.3f;
                stop = true;
                if (dh >= 0 && lv.dyn[dh].kind == DK_CRYSTAL) { audio::playAt(SFX_SHIELD, np); statusMsg = "Only a Sunflare can shatter crystal"; statusT = 1.5f; }
                else audio::playAt(SFX_CLANK, np, 0.5f);
                burst(np, hexColor(0xffb050), 6, 3, 0.06f, 3, 0.3f, 3);
            }
            // anchors catch the flame
            for (int i = 0; i < (int)lv.anchors.size(); i++) {
                if (len(lv.anchors[i].pos - f.throwOrigin) < 3.5f) continue;  // don't snag hooks right next to you
                if (segPointDist(f.pos, np, lv.anchors[i].pos) < 1.1f) {
                    np = lv.anchors[i].pos; stop = true; f.anchored = i;
                    lv.anchors[i].pulse = 1;
                    audio::playAt(SFX_CATCH, np, 0.8f, 0.8f);
                    break;
                }
            }
            {
                V3 at;
                if (!stop && hitTerminal(f.pos, np, at)) { np = at; stop = true; }
            }
            if (wardBlocks(f.pos, np)) {
                // the Hush's ward: the flame can't pass over a closed gate, seal or crystal
                np = f.pos; stop = true;
                audio::playAt(SFX_SHIELD, np, 0.7f, 0.5f);
                burst(np, hexColor(0x9a5aff), 12, 3, 0.08f, 3, 0.4f, 0);
                if (statusT <= 0) { statusMsg = "A ward of shadow seals this way - open the gate properly"; statusT = 2; }
            }
            f.pos = np;
            f.traveled += sl;
            if (f.traveled >= f.maxDist) stop = true;
            if (stop) { f.st = F_HOVER; f.hoverT = f.anchored >= 0 ? 9.f : 3.2f; f.vel = V3(0, 0, 0); }
        } break;
        case F_HOVER: {
            float rate = 1 + f.smother * 0.7f;
            f.hoverT -= dt * rate;
            if (f.anchored >= 0) f.pos = lv.anchors[f.anchored].pos + V3(0, 0.08f * std::sin(time * 4), 0);
            else f.pos.y += 0.15f * std::sin(time * 3) * dt;
            if (std::fmod(time, 0.6f) < dt) f.hitIds.clear();  // hovering flame burns what touches it
            if (f.hoverT <= 0) recallFlame();
        } break;
        case F_RETURNING: {
            V3 to = head - f.pos;
            float d = len(to);
            f.traveled += dt;  // reused as return timer
            float speed = std::min(52.f, 22.f + f.traveled * 60.f);
            V3 step = (to / std::max(d, 0.001f)) * speed * dt;
            if (len(step) >= d || d < 0.7f) {
                f.st = F_HELD; f.pos = head; f.hitIds.clear(); f.charged = false;
                audio::play(SFX_CATCH, 0.7f);
                burst(head, hexColor(0xffd070), 8, 2.5f, 0.06f, 3, 0.3f, 0);
            } else f.pos += step;
        } break;
    }

    // --- damage along the flame's path (flying, hovering, returning)
    if (f.st != F_HELD) {
        g_dmgKind = f.charged ? 1 : 0;
        float dmg = (f.charged ? 3.f : 1.f) * (1 + 0.5f * upLevel("heat"));
        for (int i = 0; i < (int)lv.breakables.size(); i++)
            if (!lv.breakables[i].broken && segPointDist(f.prevPos, f.pos, lv.breakables[i].pos + V3(0, 0.4f, 0)) < 0.8f) breakUrn(i);
        for (auto& e : enemies) {
            if (!e.alive || e.clinging) continue;
            if (f.hitIds.count(e.id)) continue;
            V3 c = e.pos + V3(0, e.height * 0.5f, 0);
            if (segPointDist(f.prevPos, f.pos, c) < e.radius + 0.45f) {
                f.hitIds.insert(e.id);
                damageEnemy(e, dmg, f.prevPos, f.charged);
            }
        }
        if (boss.active && !boss.dead) {
            auto wps = boss.wps;
            for (auto& w : wps) {
                int key = 100000 + w.id;
                if (f.hitIds.count(key)) continue;
                if (segPointDist(f.prevPos, f.pos, w.pos) < w.r + 0.4f) {
                    f.hitIds.insert(key);
                    bossDamage(w.id, dmg, f.charged);
                }
            }
        }
        for (int i = 0; i < (int)lv.braziers.size(); i++) {
            auto& b = lv.braziers[i];
            if (b.lit || b.sealed) continue;
            if (segPointDist(f.prevPos, f.pos, b.pos) < 0.95f) lightBrazier(i, false);
        }
        g_dmgKind = 0;
        // trail
        if (!headless && R) {
            Particle q; q.pos = f.pos + V3(hashf((int)(time * 999), 1) - 0.5f, hashf((int)(time * 999), 2) - 0.5f, 0) * 0.2f;
            q.vel = V3(0, 1.2f, 0); q.color = f.charged ? hexColor(0xfff4c0) : hexColor(0xffa040); q.alpha = 1;
            q.size = f.charged ? 0.14f : 0.09f; q.life = q.maxLife = 0.35f; q.kind = 3; q.drag = 1;
            R->spawn(q);
        }
    }
}

// ------------------------------------------------------------------ embers, combos, urns
void Game::addCombo(V3 at) {
    combo++;
    if (ability("overclock") && ocT <= 0) {
        float before = ocCharge;
        ocCharge = std::min(1.f, ocCharge + 0.07f);
        if (before < 1 && ocCharge >= 1) { floatText(pl.pos + V3(0, 2.2f, 0), "OVERCLOCK READY", hexColor(0x60e0ff)); audio::play(SFX_CHARGE, 0.7f, 1.6f); }
    }
    comboT = 3.0f;
    bestCombo = std::max(bestCombo, combo);
    if (combo == 6 || combo == 12 || combo == 18 || combo == 24) {
        floatText(at + V3(0, 1.5f, 0), "COMBO x" + std::to_string((int)comboMult()) + "!", hexColor(0xfff0a0));
        audio::play(SFX_SHARD, 0.6f, 1.0f + combo * 0.02f);
    }
}

void Game::dropEmbers(V3 pos, int amount) {
    if (amount <= 0) return;
    static Rng r(31337);
    int pieces = std::min(amount, 8);
    for (int i = 0; i < pieces; i++) {
        Pickup p; p.type = PK_EMBER; p.pos = pos + V3(0, 0.6f, 0);
        p.vel = V3(r.range(-3, 3), r.range(4, 7), r.range(-3, 3));
        p.life = 14;
        p.value = amount / pieces + (i < amount % pieces ? 1 : 0);
        lv.pickups.push_back(p);
    }
    floatText(pos + V3(0, 1.2f, 0), "+" + std::to_string(amount), hexColor(0xffa040));
}

void Game::breakUrn(int i) {
    auto& b = lv.breakables[i];
    if (b.broken) return;
    b.broken = true;
    static Rng r(4711);
    audio::playAt(SFX_BREAK, b.pos, 0.6f, 1.4f + r.f() * 0.4f);
    burst(b.pos + V3(0, 0.4f, 0), b.kind == 0 ? hexColor(0xa0603a) : hexColor(0x8a8090), 16, 4, 0.09f, 0, 0.7f, 14);
    dropEmbers(b.pos, r.irange(2, 4));
    if (pl.hp < maxHp() && r.f() < 0.12f) dropWax(b.pos);
}
