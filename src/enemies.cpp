// WICKLIGHT - enemies of the Hush, projectiles and area hazards
#include "game.h"
// visual-only randomness: kept apart from the gameplay RNG so rendering never changes the simulation
static Rng& fxRng() { static Rng r(31337); return r; }

extern int g_dmgKind;
static Rng erng(4242);

void Game::spawnEnemy(EnemyType t, V3 pos, int arena) {
    Enemy e; e.type = t; e.id = nextEnemyId++; e.pos = pos; e.home = pos; e.arena = arena;
    e.yaw = erng.range(0, TAU);
    switch (t) {
        case E_GLOOMLING: e.hp = 2; e.radius = 0.45f; e.height = 0.8f; break;
        case E_TARBELL: e.hp = 3; e.radius = 0.7f; e.height = 1.3f; e.cd = erng.range(0.5f, 2.f); break;
        case E_MOTH: e.hp = 1; e.radius = 0.35f; e.height = 0.5f; e.pos.y += 2.5f; e.home = e.pos; break;
        case E_KNIGHT: e.hp = 5; e.radius = 0.55f; e.height = 1.9f; break;
        case E_WRAITH: e.hp = 3; e.radius = 0.5f; e.height = 1.6f; e.cd = 2; break;
        case E_GLOOMKING: e.hp = 16; e.radius = 1.2f; e.height = 2.4f; e.cd = 2.5f; break;
        case E_MEGABOT: e.hp = 24; e.radius = 1.5f; e.height = 4.2f; e.cd = 2.0f; break;
        case E_RAT: e.hp = 1; e.radius = 0.35f; e.height = 0.5f; e.cd = 0.5f; break;
        case E_VIRUS: e.hp = 1; e.radius = 0.38f; e.height = 0.55f; e.cd = 0.4f; break;
        case E_DRONE: e.hp = 2; e.radius = 0.5f; e.height = 0.7f; e.cd = erng.range(1.0f, 2.2f); e.pos.y += 2.6f; e.home = e.pos; break;
        case E_DECOY: e.hp = 1; e.radius = 0.32f; e.height = 1.2f; break;
        case E_AGENT: e.hp = 3; e.radius = 0.4f; e.height = 1.8f; e.cd = 1.0f; e.target = pos + V3(std::sin(e.yaw) * 5, 0, std::cos(e.yaw) * 5); break;
    }
    if (lv.id == LV_ENDLESS) e.hp = std::ceil(e.hp * ((e.type == E_MEGABOT || e.type == E_GLOOMKING) ? std::min(enemyHpMul, 1.5f) : enemyHpMul));
    e.maxHp = e.hp;
    enemies.push_back(e);
    if (arena >= 0) burst(pos + V3(0, 0.8f, 0), hexColor(0x6a3aaa), 20, 4, 0.12f, 2, 0.8f, -1);
}

void Game::dropWax(V3 pos) {
    Pickup p; p.type = PK_WAX; p.pos = pos + V3(0, 0.6f, 0); p.vel = V3(erng.range(-1.5f, 1.5f), 5, erng.range(-1.5f, 1.5f)); p.life = 14;
    lv.pickups.push_back(p);
}

void Game::damageEnemy(Enemy& e, float dmg, V3 from, bool charged) {
    if (!e.alive) return;
    if (e.type == E_WRAITH && e.reveal < 0.3f && g_dmgKind != 2) return;  // unseen wraiths can't be burned
    if (e.type == E_TARBELL && e.state == 3 && g_dmgKind != 2) {
        audio::playAt(SFX_SHIELD, e.pos, 0.8f, 1.3f);
        burst(e.pos + V3(0, 1, 0), hexColor(0xffe0a0), 6, 3, 0.06f, 3, 0.3f, 2);
        e.flash = 0.3f;
        return;
    }
    if (e.type == E_MEGABOT && g_dmgKind != 2) {
        V3 fwd(std::sin(e.yaw), 0, std::cos(e.yaw));
        V3 to = flatXZ(from - e.pos);
        bool front = len(to) < 0.01f || dot(norm(to), fwd) > -0.1f;
        if (front) {
            if (charged) dmg *= 0.5f;  // a Sunflare dents the armour a little
            else {
                audio::playAt(SFX_SHIELD, e.pos, 1, 0.7f);
                burst(e.pos + V3(0, 2.5f, 0) + fwd * 1.4f, hexColor(0xffe0a0), 8, 4, 0.06f, 3, 0.3f, 2);
                e.flash = 0.1f;
                if (statusT <= 0) { statusMsg = "Its front is armored! Get BEHIND it - hit the glowing vents"; statusT = 2.5f; }
                return;
            }
        }
    }
    if (e.type == E_KNIGHT && e.shield && g_dmgKind != 2) {
        V3 fwd(std::sin(e.yaw), 0, std::cos(e.yaw));
        V3 to = flatXZ(from - e.pos);
        bool front = len(to) < 0.01f || dot(norm(to), fwd) > 0.34f;
        if (front) {
            if (charged) {
                e.shield = false;
                audio::playAt(SFX_BREAK, e.pos, 1, 1.2f);
                burst(e.pos + V3(0, 1.2f, 0) + fwd * 0.6f, hexColor(0xa0a8c0), 25, 6, 0.1f, 0, 0.8f, 12);
                statusMsg = "Shield shattered!"; statusT = 1.2f;
            } else {
                audio::playAt(SFX_SHIELD, e.pos);
                burst(e.pos + V3(0, 1.2f, 0) + fwd * 0.6f, hexColor(0xffe0a0), 8, 4, 0.06f, 3, 0.3f, 2);
                e.flash = 0.15f;
                if (statusT <= 0) { statusMsg = "Blocked! Strike its back"; statusT = 1.0f; }
                return;
            }
        }
    }
    e.hp -= dmg;
    e.flash = 0.25f;
    addCombo(e.pos);
    hitstop = std::max(hitstop, 0.035f);
    V3 kb = flatXZ(e.pos - from);
    if (len(kb) > 0.01f) kb = norm(kb);
    if (e.type == E_GLOOMLING || e.type == E_RAT) { e.vel = kb * 6 + V3(0, 4, 0); e.state = 1; e.cd = 0.7f; }
    if (e.type == E_GLOOMKING && e.hp > 0 && erng.f() < 0.3f) {
        int minions = 0;
        for (auto& o : enemies) if (o.alive && o.type == E_GLOOMLING) minions++;
        if (minions < 4) { V3 p = e.pos + V3(erng.range(-2, 2), 0.5f, erng.range(-2, 2)); Enemy dummy; (void)dummy; pendingSpawns.push_back({E_GLOOMLING, p, e.arena}); }
    }
    if (e.type == E_WRAITH) { e.reveal = 1; }
    if (e.type == E_AGENT) { e.alerted = true; if (e.state == 0) e.state = 1; }
    if (e.type == E_VIRUS) { e.vel = kb * 5 + V3(0, 3, 0); }
    if (e.type == E_MOTH) { e.clinging = false; }
    audio::playAt(SFX_HIT, e.pos);
    burst(e.pos + V3(0, e.height * 0.5f, 0), hexColor(0xffb050), 10, 4, 0.08f, 3, 0.35f, 3);
    if (e.hp <= 0) {
        e.alive = false;
        save.kills++;
        bountyEvent(0, 1);
        audio::playAt(SFX_KILL, e.pos);
        burst(e.pos + V3(0, e.height * 0.5f, 0), hexColor(0x1a1028), 26, 4, 0.2f, 2, 0.9f, -2);
        burst(e.pos + V3(0, e.height * 0.5f, 0), hexColor(0x9a5aff), 12, 5, 0.07f, 3, 0.6f, 0);
        addShake(0.12f);
        static const int REWARD[] = {3, 5, 2, 8, 6, 40, 60, 2, 1, 4, 0, 6};
        int amount = (int)std::lround(REWARD[e.type] * comboMult());
        dropEmbers(e.pos, amount);
        if (e.type == E_GLOOMKING) {
            for (int k = 0; k < 3; k++) pendingSpawns.push_back({E_GLOOMLING, e.pos + V3(std::cos(k * 2.1f) * 1.5f, 0.5f, std::sin(k * 2.1f) * 1.5f), e.arena});
            showToast("GLOOM KING DEFEATED", "...and it split apart!");
        }
        if (e.type == E_VIRUS && e.gen == 0) {  // a virus copies itself once
            for (int k = 0; k < 2; k++) pendingSpawns.push_back({E_VIRUS, e.pos + V3(k ? 0.5f : -0.5f, 0.3f, 0), e.arena, 1});
        }
        if (e.type == E_DECOY) { audio::playAt(SFX_BLINK, e.pos, 1, 1.6f); glitch = std::max(glitch, 0.3f); }
        if (e.type == E_MEGABOT) { showToast("MEGA-BOT 3000 DESTROYED", "+60 embers"); addShake(0.6f); burst(e.pos + V3(0, 2, 0), hexColor(0xffa040), 60, 9, 0.15f, 3, 1.0f, 4); }
        float chance = e.type == E_MOTH ? 0.08f : (e.type == E_KNIGHT ? 0.6f : 0.25f);
        if ((pl.hp < maxHp() || lv.id == LV_HOME) && erng.f() < (lv.id == LV_HOME ? 0.6f : chance)) dropWax(e.pos);
    }
}

static V3 dirTo(const V3& a, const V3& b) { V3 d = flatXZ(b - a); float l = len(d); return l > 0.001f ? d / l : V3(0, 0, 1); }

void Game::updateEnemies(float dt) {
    V3 pc = pl.pos + V3(0, 0.6f, 0);
    int smother = 0;
    for (auto& e : enemies) {
        if (!e.alive) continue;
        e.flash = std::max(0.f, e.flash - dt);
        e.anim += dt;
        e.t += dt;
        e.cd = std::max(0.f, e.cd - dt);
        float dist = distXZ(e.pos, pl.pos);
        bool playerOk = !pl.dead;
        switch (e.type) {
            // ------------------------------------------------ Gloomling: hops, lunges
            case E_GLOOMLING: {
                V3 want(0, 0, 0);
                if (e.state == 0) {
                    if (playerOk && dist < 13 && std::fabs(pl.pos.y - e.pos.y) < 5) e.state = 1;
                    if (e.t > 2) { e.t = 0; e.target = e.home + V3(erng.range(-3, 3), 0, erng.range(-3, 3)); }
                    if (distXZ(e.pos, e.target) > 0.5f) want = dirTo(e.pos, e.target) * 1.5f;
                } else if (e.state == 1) {
                    if (!playerOk || dist > 22) { e.state = 0; break; }
                    want = dirTo(e.pos, pl.pos) * 3.8f;
                    // fear of a hovering flame
                    if (flame.st == F_HOVER && distXZ(flame.pos, e.pos) < 2.6f) want = dirTo(flame.pos, e.pos) * 4.f;
                    if (dist < 3.4f && e.grounded && e.cd <= 0) { e.state = 2; e.t = 0; }
                } else if (e.state == 2) {
                    e.yaw = angleApproach(e.yaw, std::atan2(pl.pos.x - e.pos.x, pl.pos.z - e.pos.z), dt * 10);
                    if (e.t > 0.45f) {
                        V3 d = dirTo(e.pos, pl.pos);
                        e.vel = d * 9.5f + V3(0, 6, 0); e.grounded = false;
                        e.state = 3; e.t = 0;
                        audio::playAt(SFX_JUMP, e.pos, 0.6f, 0.6f);
                    }
                } else if (e.state == 3) {
                    if (playerOk && dist < 0.95f && std::fabs(pc.y - (e.pos.y + 0.4f)) < 1.0f) hurtPlayer(1, e.pos);
                    if (e.grounded && e.t > 0.15f) { e.state = 1; e.cd = 1.3f; }
                }
                if (e.state != 3) {
                    float k = e.grounded ? 12.f : 3.f;
                    e.vel.x = approach(e.vel.x, want.x, k * dt * 3);
                    e.vel.z = approach(e.vel.z, want.z, k * dt * 3);
                    // little hops while moving
                    if (e.grounded && lenXZ(want) > 0.5f && std::fmod(e.anim, 0.45f) < dt) e.vel.y = 3.2f;
                    if (lenXZ(e.vel) > 0.3f) e.yaw = angleApproach(e.yaw, std::atan2(e.vel.x, e.vel.z), dt * 8);
                }
                e.vel.y -= GRAVITY * dt;
                moveBox(e.pos, e.vel, e.radius, e.height, dt, e.grounded, nullptr, true);
                if (e.pos.y < lv.killY || (lv.waterY > -999 && e.pos.y < lv.waterY - 0.6f)) e.alive = false;
            } break;
            // ------------------------------------------------ Tarbell: barnacle turret
            case E_TARBELL: {
                e.yaw = angleApproach(e.yaw, std::atan2(pl.pos.x - e.pos.x, pl.pos.z - e.pos.z), dt * 3);
                if (playerOk && dist < 4.5f) {
                    if (e.state != 3) { e.state = 3; audio::playAt(SFX_SHIELD, e.pos, 0.5f, 0.6f); }
                    break;
                }
                if (e.state == 3) { e.state = 0; e.cd = std::max(e.cd, 0.8f); }
                if (e.state == 0) {
                    if (playerOk && dist < 21 && e.cd <= 0) {
                        V3 o = e.pos + V3(0, 1.2f, 0);
                        V3 d = pc - o;
                        float t = raycast(o, norm(d), len(d));
                        if (t < 0 || t > len(d) - 0.5f) { e.state = 1; e.t = 0; }
                        else e.cd = 0.5f;
                    }
                } else if (e.state == 1 && e.t > 0.7f) {
                    V3 o = e.pos + V3(0, 1.3f, 0);
                    V3 tgt = pl.pos + V3(pl.vel.x, 0, pl.vel.z) * 0.35f;
                    V3 d = tgt - o;
                    float D = lenXZ(d);
                    float T = clampf(D / 11.f, 0.6f, 1.5f);
                    const float g = 20;
                    Projectile pr; pr.type = PR_TAR; pr.pos = o; pr.gravity = true; pr.radius = 0.35f;
                    pr.vel = V3(d.x / T, (d.y + 0.5f * g * T * T) / T, d.z / T);
                    projs.push_back(pr);
                    audio::playAt(SFX_SPIT, o);
                    e.state = 0; e.cd = erng.range(2.0f, 2.8f);
                }
            } break;
            // ------------------------------------------------ Moth: dives, smothers the flame
            case E_MOTH: {
                bool flameOut = flame.st == F_HOVER || flame.st == F_FLYING;
                if (e.clinging) {
                    if (flame.st == F_HELD || flame.st == F_RETURNING) { e.clinging = false; e.state = 1; break; }
                    float a = e.anim * 5 + e.id;
                    e.pos = flame.pos + V3(std::cos(a) * 0.45f, std::sin(a * 1.3f) * 0.2f - 0.2f, std::sin(a) * 0.45f);
                    smother++;
                    break;
                }
                V3 tgt;
                float spd = 5;
                if (e.state == 0) {
                    float a = e.anim * 0.8f + e.id;
                    tgt = e.home + V3(std::cos(a) * 2.5f, std::sin(a * 1.7f) * 0.6f, std::sin(a) * 2.5f);
                    spd = 3;
                    if (playerOk && distXZ(e.pos, pl.pos) < 14) e.state = 1;
                } else if (e.state == 1) {
                    float a = e.anim * 1.1f + e.id * 2.1f;
                    tgt = pl.pos + V3(std::cos(a) * 4, 2.8f + std::sin(a * 2) * 0.6f, std::sin(a) * 4);
                    if (flameOut && len(flame.pos - e.pos) < 15) e.state = 3;
                    else if (e.cd <= 0 && playerOk) { e.state = 2; e.t = 0; audio::playAt(SFX_FLUTTER, e.pos); }
                    if (!playerOk || dist > 24) e.state = 0;
                } else if (e.state == 2) {  // windup then dive
                    tgt = e.pos;
                    e.pos += V3(erng.range(-1, 1), erng.range(-1, 1), erng.range(-1, 1)) * 0.03f;
                    if (e.t > 0.55f) { e.state = 4; e.t = 0; e.target = pc; e.vel = norm(pc - e.pos) * 12; }
                } else if (e.state == 4) {
                    e.pos += e.vel * dt;
                    if (playerOk && len(pc - e.pos) < 0.8f) { hurtPlayer(1, e.pos); e.state = 1; e.cd = 2.6f; }
                    if (e.t > 0.9f || e.pos.y < pl.pos.y - 0.3f) { e.state = 1; e.cd = erng.range(2.2f, 3.2f); }
                    break;
                } else if (e.state == 3) {
                    if (!flameOut) { e.state = 1; break; }
                    tgt = flame.pos; spd = 9;
                    if (len(flame.pos - e.pos) < 0.6f) { e.clinging = true; audio::playAt(SFX_FLUTTER, e.pos, 1, 1.3f); }
                }
                V3 d = tgt - e.pos;
                float l = len(d);
                if (l > 0.05f) e.pos += d / l * std::min(l, spd * dt);
                e.yaw = std::atan2(d.x, d.z);
            } break;
            // ------------------------------------------------ Hushknight: shield + sword
            case E_KNIGHT: {
                float toYaw = std::atan2(pl.pos.x - e.pos.x, pl.pos.z - e.pos.z);
                V3 fwd(std::sin(e.yaw), 0, std::cos(e.yaw));
                V3 want(0, 0, 0);
                if (e.state == 0) {
                    if (playerOk && dist < 14 && std::fabs(pl.pos.y - e.pos.y) < 4) e.state = 1;
                } else if (e.state == 1) {
                    if (!playerOk || dist > 24) { e.state = 0; break; }
                    e.yaw = angleApproach(e.yaw, toYaw, dt * 2.2f);
                    if (dist > 2.0f) want = fwd * 2.6f;
                    if (dist < 2.7f && e.cd <= 0) { e.state = 2; e.t = 0; audio::playAt(SFX_SWING, e.pos, 0.5f, 0.6f); }
                    else if (dist > 6 && dist < 11 && e.cd <= 0 && erng.f() < dt * 0.6f) { e.state = 4; e.t = 0; audio::playAt(SFX_ROAR, e.pos, 0.4f, 2.5f); }
                } else if (e.state == 2) {
                    e.yaw = angleApproach(e.yaw, toYaw, dt * 1.2f);
                    if (e.t > 0.75f) { e.state = 3; e.t = 0; audio::playAt(SFX_SWING, e.pos); }
                } else if (e.state == 3) {
                    if (e.t < 0.1f + dt && e.t >= 0.1f - dt * 0.5f) {}
                    if (playerOk && dist < 3.0f && std::fabs(pl.pos.y - e.pos.y) < 2) {
                        V3 to = dirTo(e.pos, pl.pos);
                        if (dot(to, fwd) > -0.2f) hurtPlayer(2, e.pos);
                    }
                    if (e.t > 0.3f) { e.state = 1; e.cd = 1.4f; }
                } else if (e.state == 4) {  // charge windup
                    e.yaw = angleApproach(e.yaw, toYaw, dt * 3);
                    if (e.t > 0.9f) { e.state = 5; e.t = 0; }
                } else if (e.state == 5) {
                    want = fwd * 12;
                    if (playerOk && dist < 1.3f) hurtPlayer(2, e.pos);
                    if (e.t > 0.6f) { e.state = 1; e.cd = 1.6f; }
                }
                e.vel.x = approach(e.vel.x, want.x, 40 * dt);
                e.vel.z = approach(e.vel.z, want.z, 40 * dt);
                e.vel.y -= GRAVITY * dt;
                bool hit = moveBox(e.pos, e.vel, e.radius, e.height, dt, e.grounded, nullptr, true);
                if (hit && e.state == 5) { e.state = 1; e.cd = 1.2f; addShake(0.1f); audio::playAt(SFX_SLAM, e.pos, 0.4f); }
                if (e.pos.y < lv.killY) e.alive = false;
            } break;
            // ------------------------------------------------ Rat: fast scurrying biter
            case E_RAT: {
                V3 want(0, 0, 0);
                if (e.state == 0) { if (playerOk && dist < 16) e.state = 1; }
                else {
                    want = dirTo(e.pos, pl.pos) * 6.0f;
                    want += V3(std::sin(e.anim * 9 + e.id), 0, std::cos(e.anim * 7 + e.id)) * 1.5f;
                    if (playerOk && dist < 0.9f && e.cd <= 0 && std::fabs(pl.pos.y - e.pos.y) < 1) { hurtPlayer(1, e.pos); e.cd = 1.2f; }
                }
                e.vel.x = approach(e.vel.x, want.x, 40 * dt); e.vel.z = approach(e.vel.z, want.z, 40 * dt);
                if (lenXZ(e.vel) > 0.3f) e.yaw = std::atan2(e.vel.x, e.vel.z);
                e.vel.y -= GRAVITY * dt;
                moveBox(e.pos, e.vel, e.radius, e.height, dt, e.grounded, nullptr, true);
                if (e.pos.y < lv.killY || (lv.waterY > -999 && e.pos.y < lv.waterY - 0.6f)) e.alive = false;
            } break;
            // ------------------------------------------------ Virus: a green bug that scuttles at you and copies itself when burned
            case E_VIRUS: {
                V3 want(0, 0, 0);
                if (e.state == 0) { if (playerOk && dist < 18) e.state = 1; }
                else {
                    want = dirTo(e.pos, pl.pos) * 5.2f + V3(std::sin(e.anim * 11 + e.id), 0, std::cos(e.anim * 8 + e.id)) * 2.2f;
                    if (playerOk && dist < 0.85f && e.cd <= 0 && std::fabs(pl.pos.y - e.pos.y) < 1) { hurtPlayer(1, e.pos); e.cd = 1.3f; }
                }
                e.vel.x = approach(e.vel.x, want.x, 30 * dt); e.vel.z = approach(e.vel.z, want.z, 30 * dt);
                if (lenXZ(e.vel) > 0.3f) e.yaw = std::atan2(e.vel.x, e.vel.z);
                e.vel.y -= GRAVITY * dt;
                moveBox(e.pos, e.vel, e.radius, e.height, dt, e.grounded, nullptr, true);
                if (e.pos.y < lv.killY || (lv.waterY > -999 && e.pos.y < lv.waterY - 0.6f)) e.alive = false;
            } break;
            // ------------------------------------------------ Firewall drone: hovers, circles, fires straight code bolts
            case E_DRONE: {
                if (e.state == 0) { if (playerOk && dist < 20 && std::fabs(pl.pos.y - e.pos.y) < 9) e.state = 1; e.target = e.home; }
                else {
                    float a = e.anim * 0.6f + e.id * 1.7f;
                    float ground = groundHeightBelow(pl.pos + V3(0, 1, 0), 6);
                    float baseY = (ground > -1e8f ? ground : pl.pos.y);
                    e.target = pl.pos + V3(std::cos(a) * 6.5f, 0, std::sin(a) * 6.5f);
                    e.target.y = baseY + 2.8f + 0.3f * std::sin(e.anim * 2);
                    e.yaw = std::atan2(pl.pos.x - e.pos.x, pl.pos.z - e.pos.z);
                    if (e.cd <= 0 && playerOk && dist < 22) {
                        if (e.t < 0) {}
                        V3 o = e.pos + V3(0, 0.2f, 0);
                        V3 d = pc + V3(0, 0.2f, 0) - o;
                        float l = len(d);
                        float hit = raycast(o, d / l, l);
                        if (hit < 0 || hit > l - 0.5f) {
                            Projectile pr; pr.type = PR_CODE; pr.pos = o; pr.vel = d / l * 11.f; pr.radius = 0.3f; pr.life = 4;
                            projs.push_back(pr);
                            audio::playAt(SFX_BLIP, o, 0.8f, 2.2f);
                            e.cd = erng.range(1.8f, 2.6f);
                        } else e.cd = 0.4f;
                    }
                    if (!playerOk || dist > 30) e.state = 0;
                }
                V3 d = e.target - e.pos;
                float l = len(d);
                V3 np = e.pos + (l > 0.05f ? d / l * std::min(l, 4.5f * dt) : V3());
                if (boxFree(AABB(np - V3(0.4f, 0.3f, 0.4f), np + V3(0.4f, 0.5f, 0.4f)))) e.pos = np;
                else e.pos.y += 2 * dt;
            } break;
            // ------------------------------------------------ Decoy: a fake Wick made of glitches that runs at you and pops
            case E_DECOY: {
                V3 want(0, 0, 0);
                if (e.state == 0) {
                    if (playerOk) want = dirTo(e.pos, pl.pos) * 6.0f;
                    if (playerOk && dist < 1.7f) { e.state = 1; e.t = 0; audio::playAt(SFX_CHARGE, e.pos, 0.6f, 1.8f); }
                    if (e.t > 9) { e.state = 1; e.t = 0; }
                } else if (e.t > 0.55f) {
                    if (playerOk && distXZ(e.pos, pl.pos) < 2.3f && std::fabs(pl.pos.y - e.pos.y) < 1.5f) hurtPlayer(1, e.pos);
                    burst(e.pos + V3(0, 0.7f, 0), hexColor(0x60ff90), 24, 6, 0.1f, 3, 0.5f, 0);
                    audio::playAt(SFX_BLOOM, e.pos, 0.5f, 1.8f);
                    glitch = std::max(glitch, 0.35f);
                    e.alive = false;
                    break;
                }
                e.vel.x = approach(e.vel.x, want.x, 35 * dt); e.vel.z = approach(e.vel.z, want.z, 35 * dt);
                if (lenXZ(e.vel) > 0.3f) e.yaw = std::atan2(e.vel.x, e.vel.z);
                e.vel.y -= GRAVITY * dt;
                moveBox(e.pos, e.vel, e.radius, e.height, dt, e.grounded, nullptr, true);
                if (e.grounded && lenXZ(want) > 0.5f && std::fmod(e.anim, 0.5f) < dt) e.vel.y = 3.5f;
                if (e.pos.y < lv.killY) e.alive = false;
            } break;
            // ------------------------------------------------ FBI agent: patrols with a flashlight, chases, aims a laser and fires
            case E_AGENT: {
                V3 fwd(std::sin(e.yaw), 0, std::cos(e.yaw));
                V3 want(0, 0, 0);
                bool sees = false;
                if (playerOk && dist < 13 && std::fabs(pl.pos.y - e.pos.y) < 5) {
                    V3 to = dirTo(e.pos, pl.pos);
                    bool inCone = dot(to, fwd) > 0.78f || dist < 2.5f;
                    if (inCone || e.alerted) {
                        V3 o = e.pos + V3(0, 1.6f, 0), d = pc - o; float l = len(d);
                        float hit = raycast(o, d / l, l);
                        sees = hit < 0 || hit > l - 0.4f;
                    }
                }
                if (e.state == 0) {  // patrol between home and target
                    V3 goal = std::fmod(e.anim, 12.f) < 6 ? e.target : e.home;
                    if (distXZ(e.pos, goal) > 0.4f) { want = dirTo(e.pos, goal) * 2.0f; e.yaw = angleApproach(e.yaw, std::atan2(want.x, want.z), dt * 4); }
                    if (sees) { e.state = 1; e.alerted = true; spotted(e.pos); audio::playAt(SFX_ROAR, e.pos, 0.5f, 2.6f); floatText(e.pos + V3(0, 2.4f, 0), "FREEZE!", hexColor(0xff5050)); }
                    if (alarmT > 0 && distXZ(e.pos, pl.pos) < 40) { e.state = 1; e.alerted = true; }
                } else if (e.state == 1) {  // chase
                    float toYaw = std::atan2(pl.pos.x - e.pos.x, pl.pos.z - e.pos.z);
                    e.yaw = angleApproach(e.yaw, toYaw, dt * 6);
                    if (dist > 6) want = dirTo(e.pos, pl.pos) * 4.6f;
                    else if (dist < 3) want = dirTo(pl.pos, e.pos) * 2.5f;
                    if (sees && dist < 14 && e.cd <= 0) { e.state = 2; e.t = 0; audio::playAt(SFX_CHARGE, e.pos, 0.5f, 2.0f); }
                    if (playerOk && dist < 1.2f && e.cd <= 0) { hurtPlayer(1, e.pos); e.cd = 1.2f; audio::playAt(SFX_TICK, e.pos, 1, 0.5f); }
                    if (!playerOk || (dist > 45 && alarmT <= 0)) { e.state = 0; e.alerted = false; }
                } else if (e.state == 2) {  // aim: a red laser dot tracks you, then it fires
                    e.yaw = angleApproach(e.yaw, std::atan2(pl.pos.x - e.pos.x, pl.pos.z - e.pos.z), dt * (e.t < 0.6f ? 8.f : 1.f));
                    if (e.t < 0.6f) e.target2 = pc;
                    if (e.t > 0.85f) {
                        V3 o = e.pos + V3(0, 1.3f, 0) + fwd * 0.5f;
                        V3 d = e.target2 - o; float l = len(d);
                        Projectile pr; pr.type = PR_BULLET; pr.pos = o; pr.vel = d / std::max(l, 0.01f) * 24.f; pr.radius = 0.22f; pr.life = 2.5f;
                        projs.push_back(pr);
                        audio::playAt(SFX_CLANK, o, 1.2f, 2.2f);
                        e.state = 1; e.cd = erng.range(1.5f, 2.3f);
                    }
                }
                e.vel.x = approach(e.vel.x, want.x, 30 * dt); e.vel.z = approach(e.vel.z, want.z, 30 * dt);
                e.vel.y -= GRAVITY * dt;
                moveBox(e.pos, e.vel, e.radius, e.height, dt, e.grounded, nullptr, true);
                if (e.pos.y < lv.killY) e.alive = false;
            } break;
            // ------------------------------------------------ Gloom King: giant leaping gloomling (mini-boss)
            case E_GLOOMKING: {
                V3 want(0, 0, 0);
                if (e.state == 0) { if (playerOk && dist < 20) { e.state = 1; audio::play(SFX_ROAR, 0.8f, 1.4f); } }
                else if (e.state == 1) {
                    want = dirTo(e.pos, pl.pos) * 2.2f;
                    e.yaw = angleApproach(e.yaw, std::atan2(pl.pos.x - e.pos.x, pl.pos.z - e.pos.z), dt * 3);
                    if (e.grounded && std::fmod(e.anim, 0.7f) < dt) e.vel.y = 4;
                    if (e.cd <= 0 && e.grounded && playerOk) { e.state = 2; e.t = 0; e.target = pl.pos; }
                } else if (e.state == 2) {  // crouch, then leap at where you were
                    if (e.t > 0.7f) {
                        V3 d = flatXZ(e.target - e.pos);
                        e.vel = d * (1.f / 1.0f) + V3(0, 15, 0);
                        e.grounded = false; e.state = 3; e.t = 0;
                        Telegraph tg; tg.pos = e.target; tg.radius = 2.4f; tg.delay = 1.0f; tg.dmg = 1; teles.push_back(tg);
                        audio::playAt(SFX_JUMP, e.pos, 1, 0.4f);
                    }
                } else if (e.state == 3) {
                    if (e.grounded && e.t > 0.2f) {
                        Ring r; r.c = e.pos + V3(0, 0.05f, 0); r.r = 1.5f; r.speed = 9; r.maxR = 10; r.color = hexColor(0x9a5aff);
                        rings.push_back(r);
                        audio::playAt(SFX_SLAM, e.pos, 1.2f);
                        addShake(0.4f);
                        e.state = 1; e.cd = erng.range(2.8f, 3.8f);
                    }
                }
                if (e.state == 1 || e.state == 0) { e.vel.x = approach(e.vel.x, want.x, 20 * dt); e.vel.z = approach(e.vel.z, want.z, 20 * dt); }
                else if (e.state == 2) { e.vel.x = 0; e.vel.z = 0; }
                e.vel.y -= GRAVITY * dt;
                moveBox(e.pos, e.vel, e.radius, e.height, dt, e.grounded, nullptr, false);
                if (e.pos.y < lv.killY || (lv.waterY > -999 && e.pos.y < lv.waterY - 0.6f)) { e.pos = e.home; e.vel = V3(0, 0, 0); }
            } break;
            // ------------------------------------------------ Mega-Bot 3000: armoured walker, weak back (mini-boss)
            case E_MEGABOT: {
                float toYaw = std::atan2(pl.pos.x - e.pos.x, pl.pos.z - e.pos.z);
                V3 fwd(std::sin(e.yaw), 0, std::cos(e.yaw));
                V3 want(0, 0, 0);
                if (e.state == 0) { if (playerOk && dist < 22) { e.state = 1; audio::play(SFX_ROAR, 0.8f, 0.6f); statusMsg = "MEGA-BOT 3000 ONLINE. Its back is the weak spot!"; statusT = 3; } }
                else if (e.state == 1) {
                    e.yaw = angleApproach(e.yaw, toYaw, dt * 0.9f);  // turns slowly: run around it!
                    if (dist > 4) want = fwd * 1.6f;
                    if (e.cd <= 0 && playerOk) { e.state = dist < 6 ? 2 : 3; e.t = 0; }
                    if (std::fmod(e.anim, 0.55f) < dt) audio::playAt(SFX_CLANK, e.pos, 0.6f, 0.5f);
                } else if (e.state == 2) {  // stomp
                    if (e.t > 0.9f) {
                        Ring r; r.c = e.pos + V3(0, 0.05f, 0); r.r = 1.8f; r.speed = 10; r.maxR = 10; r.color = hexColor(0xffa040);
                        rings.push_back(r);
                        audio::playAt(SFX_SLAM, e.pos, 1.4f, 0.8f); addShake(0.4f);
                        e.state = 1; e.cd = erng.range(2.5f, 3.5f);
                    }
                } else if (e.state == 3) {  // missile barrage
                    if (e.t > 0.8f) {
                        for (int k = 0; k < 3; k++) {
                            Telegraph tg; tg.pos = pl.pos + (k == 0 ? V3(0, 0, 0) : V3(erng.range(-4, 4), 0, erng.range(-4, 4)));
                            tg.pos.y = e.pos.y; tg.radius = 2.0f; tg.delay = 1.3f + k * 0.2f; tg.dmg = 1; teles.push_back(tg);
                        }
                        audio::playAt(SFX_SPIT, e.pos, 1.5f, 0.5f);
                        e.state = 1; e.cd = erng.range(2.8f, 3.6f);
                    }
                }
                e.vel.x = approach(e.vel.x, want.x, 10 * dt); e.vel.z = approach(e.vel.z, want.z, 10 * dt);
                e.vel.y -= GRAVITY * dt;
                moveBox(e.pos, e.vel, e.radius, e.height, dt, e.grounded, nullptr, true);
                if (playerOk && dist < e.radius + 0.4f && std::fabs(pl.pos.y - e.pos.y) < 2) hurtPlayer(1, e.pos);
            } break;
            // ------------------------------------------------ Wraith: invisible unless lit
            case E_WRAITH: {
                float lightAmt = 0;
                if (len(flame.pos - (e.pos + V3(0, 0.8f, 0))) < (flame.st == F_HELD ? 3.0f : 6.5f)) lightAmt = 1;
                for (auto& b : lv.braziers) if (b.lit && len(b.pos - e.pos) < 6) lightAmt = 1;
                if (bloomFlash > 0.5f && len(bloomPos - e.pos) < 9) lightAmt = 1;
                float before = e.reveal;
                e.reveal = approach(e.reveal, lightAmt, dt * (lightAmt > 0 ? 4.f : 0.7f));
                if (before < 0.6f && e.reveal >= 0.6f) { audio::playAt(SFX_WHISPER, e.pos, 1, 1.6f); e.state = 4; e.t = 0; }
                float ground = groundHeightBelow(e.pos + V3(0, 2, 0), 12);
                float hoverY = (ground > -1e8f ? ground : e.pos.y) + 0.35f + 0.15f * std::sin(e.anim * 2);
                V3 want(0, 0, 0);
                if (e.state == 0) {
                    if (playerOk && dist < 16) e.state = 1;
                } else if (e.state == 1) {
                    if (!playerOk || dist > 26) { e.state = 0; break; }
                    if (dist > 1.7f) want = dirTo(e.pos, pl.pos) * 3.0f;
                    e.yaw = std::atan2(pl.pos.x - e.pos.x, pl.pos.z - e.pos.z);
                    if (dist < 2.2f && e.cd <= 0) { e.state = 2; e.t = 0; }
                    if (e.cd <= 0 && dist > 7 && e.reveal < 0.2f && erng.f() < dt * 0.4f) {
                        // blink next to the player
                        float a = erng.range(0, TAU);
                        V3 np = pl.pos + V3(std::cos(a) * 4, 1.5f, std::sin(a) * 4);
                        float g = groundHeightBelow(np, 6);
                        if (g > -1e8f) {
                            burst(e.pos + V3(0, 0.8f, 0), hexColor(0x3a2a5a), 12, 2, 0.15f, 2, 0.6f, 0);
                            e.pos = V3(np.x, g, np.z);
                            audio::playAt(SFX_BLINK, e.pos, 0.7f);
                            e.cd = 1.0f;
                        }
                    }
                } else if (e.state == 2) {  // windup (shows itself briefly)
                    e.reveal = std::max(e.reveal, 0.35f);
                    if (e.t > 0.6f) {
                        if (playerOk && dist < 2.4f) hurtPlayer(1, e.pos);
                        audio::playAt(SFX_SWING, e.pos, 0.6f, 1.5f);
                        e.state = 1; e.cd = 1.7f;
                    }
                } else if (e.state == 4) {  // flee from the light
                    V3 away = dirTo(flame.pos, e.pos);
                    want = away * 3.5f;
                    if (e.t > 1.4f) e.state = 1;
                }
                e.vel.x = approach(e.vel.x, want.x, 20 * dt);
                e.vel.z = approach(e.vel.z, want.z, 20 * dt);
                V3 np = e.pos + V3(e.vel.x, 0, e.vel.z) * dt;
                AABB a(V3(np.x - 0.3f, hoverY + 0.2f, np.z - 0.3f), V3(np.x + 0.3f, hoverY + 1.4f, np.z + 0.3f));
                if (boxFree(a)) { e.pos.x = np.x; e.pos.z = np.z; }
                e.pos.y = approach(e.pos.y, hoverY, dt * 4);
            } break;
        }
    }
    flame.smother = smother;
    auto ps = pendingSpawns; pendingSpawns.clear();
    for (auto& p : ps) {
        spawnEnemy(p.t, p.pos, p.arena);
        if (p.gen) { enemies.back().gen = p.gen; enemies.back().state = 1; enemies.back().radius = 0.28f; }
    }
    // separate overlapping ground enemies a little
    for (size_t i = 0; i < enemies.size(); i++)
        for (size_t j = i + 1; j < enemies.size(); j++) {
            Enemy &a = enemies[i], &b = enemies[j];
            if (!a.alive || !b.alive || a.type == E_MOTH || b.type == E_MOTH || a.type == E_TARBELL || b.type == E_TARBELL || a.type == E_DRONE || b.type == E_DRONE) continue;
            V3 d = flatXZ(a.pos - b.pos);
            float l = len(d), m = a.radius + b.radius;
            if (l < m && l > 0.001f) {
                V3 push = d / l * ((m - l) * 0.5f);
                a.pos += push; b.pos -= push;
            }
        }
    // purge dead enemies occasionally
    if (enemies.size() > 64) enemies.erase(std::remove_if(enemies.begin(), enemies.end(), [](const Enemy& e) { return !e.alive; }), enemies.end());
}

void Game::updateProjectiles(float dt) {
    V3 pc = pl.pos + V3(0, 0.6f, 0);
    for (auto& p : projs) {
        if (!p.alive) continue;
        p.life -= dt;
        p.spin += dt * 8;
        if (p.life <= 0) { p.alive = false; continue; }
        switch (p.type) {
            case PR_TAR: {
                p.vel.y -= 20 * dt;
                V3 step = p.vel * dt;
                float sl = len(step);
                float t = raycast(p.pos, step / sl, sl);
                bool water = lv.waterY > -999 && p.pos.y < lv.waterY;
                if (!pl.dead && len(pc - p.pos) < 0.75f) { hurtPlayer(1, p.pos); p.alive = false; }
                else if (t >= 0 || water) {
                    V3 hp = t >= 0 ? p.pos + step / sl * t : p.pos;
                    if (!pl.dead && len(pl.pos - hp) < 1.5f) hurtPlayer(1, hp);
                    burst(hp, hexColor(0x2a1a3a), 14, 4, 0.1f, 0, 0.6f, 10);
                    audio::playAt(water ? SFX_SPLASH : SFX_LAND, hp, 0.6f);
                    p.alive = false;
                } else p.pos += step;
                if (!headless && R && fxRng().f() < 0.5f) {
                    Particle q; q.pos = p.pos; q.color = hexColor(0x3a2050); q.alpha = 1; q.size = 0.12f; q.life = q.maxLife = 0.4f; q.kind = 2;
                    R->spawn(q);
                }
            } break;
            case PR_GEAR: {
                p.pos += p.vel * dt;
                if (!pl.dead && distXZ(pl.pos, p.pos) < p.radius + 0.35f && pl.pos.y < p.pos.y + p.radius) hurtPlayer(1, p.pos);
                if (boss.active && distXZ(p.pos, lv.boss.center) > lv.boss.radius + 1) p.alive = false;
                if (!headless && R && fxRng().f() < 0.4f) {
                    Particle q; q.pos = p.pos - V3(0, p.radius - 0.1f, 0); q.vel = V3(fxRng().range(-2, 2), 3, fxRng().range(-2, 2));
                    q.color = hexColor(0xffd070); q.alpha = 1; q.size = 0.05f; q.life = q.maxLife = 0.3f; q.kind = 3; q.gravity = 10;
                    R->spawn(q);
                }
            } break;
            case PR_ORB: {
                V3 to = pc - p.pos;
                float l = len(to);
                if (l > 0.01f) {
                    V3 want = to / l * 6.5f;
                    p.vel = lerp(p.vel, want, std::min(1.f, dt * 1.2f));
                }
                p.pos += p.vel * dt;
                if (!pl.dead && l < 0.7f) { hurtPlayer(1, p.pos); p.alive = false; }
                // the flame burns orbs away
                if (flame.st != F_HELD && len(flame.pos - p.pos) < 0.9f) {
                    p.alive = false; burst(p.pos, hexColor(0x9a5aff), 10, 3, 0.08f, 3, 0.4f, 0);
                    audio::playAt(SFX_HIT, p.pos, 0.5f, 1.5f);
                }
            } break;
            case PR_DUST: break;
            case PR_CODE: case PR_BULLET: {
                V3 step = p.vel * dt;
                float sl = len(step);
                float t = sl > 1e-5f ? raycast(p.pos, step / sl, sl) : -1;
                // the flame burns code bolts and bullets out of the air
                if (flame.st != F_HELD && len(flame.pos - p.pos) < 0.9f) {
                    p.alive = false; burst(p.pos, p.type == PR_CODE ? hexColor(0x60ff90) : hexColor(0xffe0a0), 8, 3, 0.07f, 3, 0.3f, 0);
                    audio::playAt(SFX_HIT, p.pos, 0.4f, 1.8f); break;
                }
                if (!pl.dead && len(pc + V3(0, 0.15f, 0) - p.pos) < p.radius + 0.45f) { hurtPlayer(1, p.pos); p.alive = false; break; }
                if (t >= 0) { p.alive = false; burst(p.pos + step / sl * t, p.type == PR_CODE ? hexColor(0x60ff90) : hexColor(0xc0c0c0), 6, 2, 0.06f, 3, 0.25f, 4); break; }
                p.pos += step;
            } break;
        }
    }
    projs.erase(std::remove_if(projs.begin(), projs.end(), [](const Projectile& p) { return !p.alive; }), projs.end());

    for (auto& r : rings) {
        if (!r.alive) continue;
        float prev = r.r;
        r.r += r.speed * dt;
        if (r.r > r.maxR) { r.alive = false; continue; }
        float d = distXZ(pl.pos, r.c);
        if (!r.hitPlayer && !pl.dead && pl.pos.y < r.c.y + 0.75f && d > prev - 0.45f && d < r.r + 0.45f) { r.hitPlayer = true; hurtPlayer(r.dmg, r.c); }
    }
    rings.erase(std::remove_if(rings.begin(), rings.end(), [](const Ring& r) { return !r.alive; }), rings.end());

    for (auto& t : teles) {
        if (!t.alive) continue;
        t.t += dt;
        if (!t.fired && t.t >= t.delay) {
            t.fired = true;
            if (!pl.dead && distXZ(pl.pos, t.pos) < t.radius && pl.pos.y < t.pos.y + 3.0f) hurtPlayer(t.dmg, t.pos);
            audio::playAt(t.kind == 1 ? SFX_WHISPER : SFX_SLAM, t.pos, t.kind == 1 ? 0.5f : 0.7f);
            burst(t.pos + V3(0, 0.3f, 0), t.kind == 1 ? hexColor(0xd8d0c0) : (t.kind == 2 ? hexColor(0x40ff80) : hexColor(0x5a2a8a)), 24, 6, 0.14f, t.kind == 1 ? 2 : 0, 0.7f, 6);
            addShake(0.15f);
        }
        if (t.t > t.delay + t.linger) t.alive = false;
    }
    teles.erase(std::remove_if(teles.begin(), teles.end(), [](const Telegraph& t) { return !t.alive; }), teles.end());
}
