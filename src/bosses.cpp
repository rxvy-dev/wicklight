// WICKLIGHT - boss encounters
//  The Drowned Bellwether (Sunken Wharf)   - core exposed after it tolls
//  The Pendulum Warden   (Clockwood)       - core opens while it rewinds; floor electrifies -> warp to perches
//  The Veil Matron       (Ashen Vaults)    - light all four braziers to dazzle her down
//  The Hush              (Lighthouse Spire)- shrouded hands (Sunflare), lamp wicks (Bloom), the heart
#include "game.h"

extern int g_dmgKind;
static Rng brng(1337);

static const char* BOSS_NAMES[] = {"THE DROWNED BELLWETHER", "GL1TCH, THE HACKER", "THE VEIL MATRON", "THE HUSH", "KING RATTLETRAP"};

void Game::resetBoss() {
    boss = Boss();
}

void Game::startBoss() {
    if (lv.boss.type == B_NONE || boss.active || has(lv.boss.flag)) return;
    resetBoss();
    boss.type = lv.boss.type;
    boss.active = true;
    boss.name = BOSS_NAMES[boss.type];
    V3 C = lv.boss.center;
    boss.pos = C;
    switch (boss.type) {
        case B_BELLWETHER: boss.hp = boss.maxHp = 16; break;
        case B_WARDEN: boss.hp = boss.maxHp = 12; boss.pos = C + V3(0, 5.5f, 0); break;
        case B_MATRON: boss.hp = boss.maxHp = 14; boss.pos = C + V3(9, 8, 0); break;
        case B_HUSH:
            boss.hp = boss.maxHp = 24;
            boss.pos = C + V3(0, 6, 9);
            for (int h = 0; h < 2; h++) {
                boss.hands[h].home = C + V3(h == 0 ? -7.f : 7.f, 5, 2);
                boss.hands[h].pos = boss.hands[h].home;
                boss.hands[h].t = 1.5f + h * 2.2f;
            }
            break;
        case B_RATKING: boss.hp = boss.maxHp = 15; break;
        default: break;
    }
    if (lv.id == LV_ENDLESS) { boss.hp = boss.maxHp = std::ceil(boss.maxHp * std::min(enemyHpMul, 1.8f)); boss.name += "  (DEPTH " + std::to_string(save.endlessDepth) + ")"; }
    for (int g : lv.boss.gates) { lv.dyn[g].open = false; }
    audio::setSong(boss.type == B_HUSH ? SONG_FINAL : SONG_BOSS);
    audio::play(SFX_ROAR);
    addShake(0.5f);
    showToast(boss.name, boss.type == B_HUSH ? "the shadow of the Great Lamp" : (boss.type == B_WARDEN ? "a hacker from across the world" : (boss.type == B_RATKING ? "ruler of the Junkyard" : "guardian of a Beacon-Heart")));
    // short intro camera
    camOverride = 1.0f;
    camOverrideTarget = boss.type == B_WARDEN ? C + V3(0, 4, 0) : (boss.type == B_HUSH ? C + V3(0, 5, 4) : C + V3(0, 2.5f, 0));
    camOverrideEye = pl.pos + V3(0, 3.5f, 0) - norm(flatXZ(C - pl.pos)) * 4;
    boss.f[15] = 2.2f;  // intro timer
}

std::vector<WeakPoint>& Game::bossWeakPoints() { return boss.wps; }

void Game::bossDamage(int wp, float dmg, bool charged) {
    if (!boss.active || boss.dead) return;
    WeakPoint* w = nullptr;
    for (auto& x : boss.wps) if (x.id == wp) w = &x;
    if (!w) return;
    V3 at = w->pos;
    if (boss.type == B_HUSH && (wp == 10 || wp == 11)) {
        auto& h = boss.hands[wp - 10];
        if (!h.alive) return;
        if (!h.down) { audio::playAt(SFX_SHIELD, at, 0.7f, 0.7f); return; }
        if (h.shroud > 0.5f) {
            if (g_dmgKind == 1) {
                h.shroud = 0;
                audio::playAt(SFX_BREAK, at, 1, 0.7f);
                burst(at, hexColor(0x3a1a5a), 40, 8, 0.2f, 2, 1.0f, 0);
                burst(at, hexColor(0xfff0c0), 20, 8, 0.1f, 3, 0.6f, 0);
                statusMsg = "The shroud burns away!"; statusT = 1.5f;
            } else {
                audio::playAt(SFX_SHIELD, at, 0.8f, 0.6f);
                burst(at, hexColor(0x5a3a8a), 8, 3, 0.1f, 2, 0.5f, 0);
                statusMsg = "The shroud drinks your flame... charge a SUNFLARE (hold attack)"; statusT = 2.2f;
                return;
            }
        }
        h.hp -= dmg;
        h.flash = 0.3f;
        audio::playAt(SFX_HIT, at, 1, 0.7f);
        burst(at, hexColor(0xffb050), 14, 5, 0.1f, 3, 0.4f, 0);
        hitstop = 0.05f;
        if (h.hp <= 0) {
            h.alive = false; h.down = false;
            audio::play(SFX_ROAR, 1, 1.3f);
            burst(at, hexColor(0x1a0a2a), 60, 9, 0.3f, 2, 1.4f, -2);
            addShake(0.6f);
        }
        return;
    }
    if (w->vulnerable) addCombo(at);
    if (boss.type == B_WARDEN && wp < 3) {
        float& hp = boss.f[wp];
        if (hp <= 0) return;
        hp -= std::max(1.f, dmg > 1.5f ? 2.f : 1.f);
        audio::playAt(SFX_HIT, at, 1, 1.4f);
        burst(at, hexColor(0x60ff90), 14, 5, 0.08f, 3, 0.4f, 4);
        burst(at, hexColor(0xffa040), 10, 4, 0.1f, 3, 0.5f, 0);
        if (hp <= 0) {
            hp = 0;
            audio::playAt(SFX_BREAK, at, 1.2f, 0.8f);
            addShake(0.25f);
            int left = 0; for (int k = 0; k < 3; k++) if (boss.f[k] > 0) left++;
            floatText(at + V3(0, 1.5f, 0), left ? "SERVER BURNED - " + std::to_string(left) + " LEFT" : "ALL SERVERS DOWN", hexColor(0xffc060));
        } else floatText(at + V3(0, 1.5f, 0), "OVERHEATING", hexColor(0xffa040));
        return;
    }
    if (boss.type == B_WARDEN && wp == 10 && w->vulnerable) {
        static const char* OW[] = {"GL1TCH: OW my RGB!", "GL1TCH: that's not even FAIR", "GL1TCH: my keyboard is MELTING", "GL1TCH: alt+f4 alt+f4 ALT+F4"};
        statusMsg = OW[(int)boss.hp % 4]; statusT = 2;
    }
    if (boss.type == B_RATKING && wp == 0 && w->vulnerable) {
        static const char* OW[] = {"KING RATTLETRAP: MY WHISKERS!", "KING RATTLETRAP: not the crown!!", "KING RATTLETRAP: EEEK!", "KING RATTLETRAP: that's hot that's HOT"};
        statusMsg = OW[(int)boss.hp % 4]; statusT = 2;
    }
    if (!w->vulnerable) {
        audio::playAt(SFX_SHIELD, at, 0.8f, 0.8f);
        burst(at, hexColor(0xffe0a0), 8, 4, 0.07f, 3, 0.3f, 2);
        if (statusT <= 0) {
            const char* tips[] = {"It is armoured. Wait for it to toll...", "His FIREWALL is up. Burn all 3 SERVERS first!", "Her wings scatter your flame. Dazzle her with light.", "The Lamp's glass holds. Light all three wicks.", "His cockpit is sealed! Dodge the claw - when it gets STUCK, burn the rat!"};
            statusMsg = tips[boss.type]; statusT = 1.6f;
        }
        return;
    }
    boss.hp -= dmg;
    boss.flash = 0.3f;
    hitstop = 0.06f;
    addShake(0.2f);
    audio::playAt(SFX_HIT, at, 1.2f, 0.6f);
    burst(at, hexColor(0xffb050), 18, 6, 0.1f, 3, 0.5f, 0);
    burst(at, hexColor(0x1a1028), 10, 3, 0.2f, 2, 0.8f, 0);
    if (boss.hp <= 0) {
        boss.hp = 0;
        boss.dead = true;
        boss.deathT = 0;
        audio::play(SFX_ROAR, 1.2f, 0.8f);
        addShake(1.0f);
        for (auto& e : enemies) if (e.alive) { e.alive = false; burst(e.pos, hexColor(0x1a1028), 10, 3, 0.2f, 2, 0.6f, 0); }
        projs.clear(); teles.clear(); rings.clear();
    }
}

void Game::bossDefeated() {
    setFlag(lv.boss.flag);
    boss.active = false;
    for (int g : lv.boss.gates) { lv.dyn[g].open = true; }
    if (boss.type == B_HUSH) { startEnding(); return; }
    if (lv.id == LV_ENDLESS) {
        dropEmbers(lv.boss.center + V3(0, 1, 0), 30 + 4 * save.endlessDepth);
        audio::setSong(lv.song);
        showToast("BOSS DEFEATED", "The way deeper is open");
        return;
    }
    // Beacon-Heart appears at the arena centre
    dropEmbers(lv.boss.center + V3(0, 1, 0), 40);
    Pickup p; p.type = PK_HEART; p.index = boss.type; p.pos = lv.boss.center + (boss.type == B_RATKING ? V3(0, 1.4f, -4.5f) : V3(0, 1.4f, 0));
    p.flag = std::string("heart.") + std::to_string((int)boss.type);
    lv.pickups.push_back(p);
    audio::setSong(lv.song);
    showToast("VICTORY", "A Beacon-Heart is free");
    writeSave();
}

// ------------------------------------------------------------------ per-boss logic
static void bellwether(Game& g, float dt) {
    Boss& b = g.boss;
    V3 C = g.lv.boss.center;
    V3 toP = flatXZ(g.pl.pos - C);
    float want = std::atan2(toP.x, toP.z);
    if (b.state != 3) b.yaw = angleApproach(b.yaw, want, dt * (b.phase ? 1.4f : 0.9f));
    V3 face(std::sin(b.yaw), 0, std::cos(b.yaw));
    b.stateT += dt;
    float& lift = b.f[0];
    float& tilt = b.f[1];
    bool open = false;
    if (b.phase == 0 && b.hp <= 8) { b.phase = 1; audio::play(SFX_ROAR, 1, 0.9f); g.addShake(0.4f); g.statusMsg = "The Bellwether rages!"; g.statusT = 2; }
    static const int P0[] = {0, 1, 0, 2};
    static const int P1[] = {0, 1, 2, 0, 1, 2};
    switch (b.state) {
        case 0:  // idle
            lift = approach(lift, 0, dt * 2); tilt = approach(tilt, 0, dt * 2);
            if (b.stateT > (b.phase ? 0.7f : 1.1f)) {
                int nxt = b.phase ? P1[b.i[0] % 6] : P0[b.i[0] % 4];
                b.i[0]++;
                b.state = nxt == 0 ? 1 : (nxt == 1 ? 2 : (nxt == 2 ? 3 : 4));
                b.stateT = 0; b.i[1] = 0;
            }
            break;
        case 1:  // slam -> shockwave rings
            if (b.stateT < 0.9f) lift = smooth01(b.stateT / 0.9f) * 2.2f;
            else lift = approach(lift, 0, dt * 18);
            if (b.stateT >= 0.95f && b.i[1] == 0) {
                b.i[1] = 1;
                Ring r; r.c = C + V3(0, 0.05f, 0); r.r = 3.0f; r.speed = b.phase ? 10.5f : 8.5f; r.maxR = 22; r.color = hexColor(0x5ae0e0);
                g.rings.push_back(r);
                audio::playAt(SFX_SLAM, C, 1.2f);
                g.addShake(0.45f);
                g.burst(C + V3(0, 0.3f, 0), hexColor(0x9ad8e8), 40, 9, 0.12f, 0, 0.8f, 12);
            }
            if (b.phase && b.stateT >= 1.65f && b.i[1] == 1) {
                b.i[1] = 2;
                Ring r; r.c = C + V3(0, 0.05f, 0); r.r = 3.0f; r.speed = 8.f; r.maxR = 22; r.color = hexColor(0x5ae0e0);
                g.rings.push_back(r);
                audio::playAt(SFX_SLAM, C, 0.9f);
            }
            if (b.stateT > (b.phase ? 2.4f : 1.9f)) { b.state = 0; b.stateT = 0; }
            break;
        case 2: {  // tar spit volley
            int n = b.phase ? 5 : 3;
            if (b.stateT > 0.6f && b.i[1] < n && b.stateT > 0.6f + b.i[1] * 0.22f) {
                V3 o = C + V3(0, 4.2f, 0) + face * 1.5f;
                V3 tgt = g.pl.pos + V3(brng.range(-2.5f, 2.5f), 0, brng.range(-2.5f, 2.5f)) * (b.i[1] == 0 ? 0.f : 1.f);
                V3 d = tgt - o; float D = lenXZ(d); float T = clampf(D / 10.f, 0.8f, 1.6f);
                Projectile pr; pr.type = PR_TAR; pr.pos = o; pr.gravity = true; pr.radius = 0.45f;
                pr.vel = V3(d.x / T, (d.y + 10 * T * T) / T, d.z / T);
                g.projs.push_back(pr);
                audio::playAt(SFX_SPIT, o, 1, 0.8f);
                b.i[1]++;
            }
            if (b.stateT > 0.6f + n * 0.22f + 0.6f) { b.state = 0; b.stateT = 0; }
        } break;
        case 3:  // toll: ring the bell, then tilt to expose the core
            if (b.stateT < 0.8f) tilt = std::sin(b.stateT * 18) * 0.12f;
            if (b.stateT >= 0.8f && b.i[1] == 0) {
                b.i[1] = 1;
                audio::playAt(SFX_BELL, C, 1.5f);
                Ring r; r.c = C + V3(0, 0.05f, 0); r.r = 2.5f; r.speed = 15; r.maxR = 9; r.color = hexColor(0xffe0a0);
                g.rings.push_back(r);
                g.addShake(0.3f);
                if (b.phase) {
                    int alive = 0;
                    for (auto& e : g.enemies) if (e.alive) alive++;
                    if (alive < 3) {
                        for (int k = 0; k < 2; k++) {
                            float a = brng.range(0, TAU);
                            g.spawnEnemy(E_GLOOMLING, C + V3(std::cos(a) * 11, 0.3f, std::sin(a) * 11), 99);
                        }
                    }
                }
            }
            if (b.stateT > 1.0f && b.stateT < 4.6f) { tilt = approach(tilt, 0.55f, dt * 3); open = true; }
            else if (b.stateT >= 4.6f) tilt = approach(tilt, 0, dt * 2);
            if (b.stateT > 5.2f) { b.state = 0; b.stateT = 0; }
            break;
    }
    // weak points
    b.wps.clear();
    V3 core = C + V3(0, 1.7f + lift, 0) + face * (1.2f + tilt * 1.6f);
    if (open) b.wps.push_back({core, 1.05f, true, 1});
    else b.wps.push_back({C + V3(0, 2.6f + lift, 0), 2.4f, false, 0});
    b.v[0] = core;
    // contact
    if (!g.pl.dead && distXZ(g.pl.pos, C) < 3.1f && g.pl.pos.y < C.y + 3 + lift) {
        g.hurtPlayer(1, C);
    }
}

// GL1TCH, a hacker from across the world who wired himself into the Clockwood's machines.
// He hides behind a FIREWALL fed by three SERVERS. Burn all three servers with your flame to crash
// the firewall, then burn GL1TCH himself while he reboots.
static const char* HACK_TAUNTS[] = {
    "GL1TCH: lol. a CANDLE? i'm in your city's mainframe, kid",
    "GL1TCH: who even uses FIRE anymore",
    "GL1TCH: my servers are water-cooled. try harder",
    "GL1TCH: downloading your flame... 12%... 37%...",
    "GL1TCH: the Hush pays in bitcoin btw",
};
V3 hackServerPos(const V3& C, int k) { float a = PI / 3 + k * TAU / 3; return C + V3(std::sin(a) * 8.5f, 0, std::cos(a) * 8.5f); }

static void hacker(Game& g, float dt) {
    Boss& b = g.boss;
    V3 C = g.lv.boss.center;
    b.stateT += dt;
    float* srvHp = &b.f[0];      // 0..2 server health (0 = burned)
    float* reboot = &b.f[3];     // 3..5 reboot timers for burned servers
    float& down = b.f[6];        // firewall-down timer
    float& atk = b.f[7];         // attack timer
    float& taunt = b.f[8];
    if (b.i[0] == 0) {           // first frame: servers online
        b.i[0] = 1;
        for (int k = 0; k < 3; k++) srvHp[k] = 2;
        g.statusMsg = "BURN THE 3 SERVERS with your flame to crash his FIREWALL!"; g.statusT = 5;
    }
    if (b.phase == 0 && b.hp <= 6) {
        b.phase = 1;
        g.statusMsg = "GL1TCH: ok ok OK. enabling turbo mode"; g.statusT = 3;
        audio::play(SFX_ROAR, 1, 1.8f);
    }
    int burned = 0;
    for (int k = 0; k < 3; k++) if (srvHp[k] <= 0) burned++;
    bool firewall = down <= 0;
    if (firewall && burned == 3) {
        down = b.phase ? 6.f : 7.5f;
        firewall = false;
        audio::play(SFX_BREAK, 1.4f, 0.6f);
        g.addShake(0.5f);
        g.statusMsg = "FIREWALL DOWN! Burn GL1TCH now!"; g.statusT = 3;
        g.floatText(C + V3(0, 4, 0), "CONNECTION LOST", hexColor(0x60ff90));
    }
    if (!firewall) {
        down -= dt;
        // stunned on the floor, frantically typing
        b.pos = lerp(b.pos, C + V3(0, 1.3f, 0), 1 - std::exp(-dt * 4));
        if (down <= 0) {
            // reboot everything
            for (int k = 0; k < 3; k++) { srvHp[k] = 2; reboot[k] = 0; }
            audio::play(SFX_UNLOCK, 0.6f, 0.5f);
            g.statusMsg = "GL1TCH: rebooting... lol you thought. BURN THE SERVERS AGAIN"; g.statusT = 3;
        }
    } else {
        // hovering above his terminal, attacking
        b.pos = lerp(b.pos, C + V3(std::sin(g.time * 0.7f) * 2, 5.5f + std::sin(g.time * 1.9f) * 0.4f, std::cos(g.time * 0.5f) * 2), 1 - std::exp(-dt * 2));
        // in phase 2 burned servers come back online after a while (burn them quickly!)
        if (b.phase == 1)
            for (int k = 0; k < 3; k++)
                if (srvHp[k] <= 0) {
                    reboot[k] += dt;
                    if (reboot[k] > 12) { srvHp[k] = 2; reboot[k] = 0; g.floatText(hackServerPos(C, k) + V3(0, 4.5f, 0), "SERVER REBOOTED", hexColor(0x60ff90)); }
                }
        atk += dt;
        float every = b.phase ? 2.6f : 3.4f;
        if (atk > every) {
            atk = 0;
            int pick = b.i[1]++ % 3;
            if (pick == 0) {  // data packets that home in on you (your flame burns them)
                for (int k = 0; k < (b.phase ? 4 : 3); k++) {
                    Projectile pr; pr.type = PR_ORB; pr.pos = b.pos + V3(0, 0.5f, 0); pr.life = 6; pr.radius = 0.35f; pr.dmg = 1;
                    float a = k * TAU / 4 + g.time;
                    pr.vel = V3(std::cos(a) * 4, 3, std::sin(a) * 4);
                    pr.spin = 99;  // marks it as a green data packet for drawing
                    g.projs.push_back(pr);
                }
                audio::playAt(SFX_BLIP, b.pos, 2, 0.6f);
            } else if (pick == 1) {  // "DDoS": strike markers under you
                int n = b.phase ? 4 : 3;
                for (int k = 0; k < n; k++) {
                    Telegraph t; t.pos = g.pl.pos + (k == 0 ? V3(0, 0, 0) : V3(brng.range(-5, 5), 0, brng.range(-5, 5)));
                    t.pos.y = C.y; t.radius = 2.0f; t.delay = 1.4f; t.dmg = 1;
                    g.teles.push_back(t);
                }
            } else {  // malware: little bugs crawl out of the terminal
                int alive = 0;
                for (auto& e : g.enemies) if (e.alive) alive++;
                for (int k = 0; k < 2 && alive < 4; k++, alive++) {
                    float a = brng.range(0, TAU);
                    g.spawnEnemy(E_GLOOMLING, C + V3(std::cos(a) * 3, 0.3f, std::sin(a) * 3), 99);
                }
                g.statusMsg = "GL1TCH: installing malware.exe"; g.statusT = 2;
            }
        }
        taunt += dt;
        if (taunt > 9) { taunt = 0; g.statusMsg = HACK_TAUNTS[brng.irange(0, 4)]; g.statusT = 3; }
    }
    // weak points: servers 0..2, GL1TCH = 10
    b.wps.clear();
    for (int k = 0; k < 3; k++) if (srvHp[k] > 0) b.wps.push_back({hackServerPos(C, k) + V3(0, 2.2f, 0), 1.3f, true, k});
    b.wps.push_back({b.pos + V3(0, 0.3f, 0), 1.2f, !firewall, 10});
    b.v[0] = V3(firewall ? 1.f : 0.f, (float)burned, 0);
}

static void matron(Game& g, float dt) {
    Boss& b = g.boss;
    V3 C = g.lv.boss.center;
    b.stateT += dt;
    float& orbit = b.f[0];
    if (b.phase == 0 && b.hp <= 7) { b.phase = 1; audio::play(SFX_ROAR, 1, 1.6f); g.statusMsg = "The Matron's veil darkens!"; g.statusT = 2; }
    int litCount = 0;
    for (auto& br : g.lv.braziers) if (br.bossBrazier && br.lit) litCount++;
    bool vulnerable = false;
    switch (b.state) {
        case 0: {  // circle overhead, attack periodically
            orbit += dt * (b.phase ? 0.5f : 0.38f);
            V3 want = C + V3(std::cos(orbit) * 9, 8 + std::sin(g.time * 1.7f) * 0.8f, std::sin(orbit) * 9);
            b.pos = lerp(b.pos, want, 1 - std::exp(-dt * 2));
            if (std::fmod(g.time, 0.6f) < dt) audio::playAt(SFX_FLUTTER, b.pos, 0.7f, 0.5f);
            if (litCount >= 4) { b.state = 4; b.stateT = 0; audio::play(SFX_ROAR, 0.8f, 1.8f); g.statusMsg = "She is dazzled - strike now!"; g.statusT = 2; break; }
            float every = b.phase ? 3.0f : 4.0f;
            if (b.stateT > every) {
                b.stateT = 0;
                int pick = b.i[0]++ % 3;
                if (pick == 2 && litCount == 0) pick = 0;
                if (pick == 0) {  // dust
                    int n = b.phase ? 5 : 3;
                    for (int k = 0; k < n; k++) {
                        Telegraph t; t.pos = g.pl.pos + (k == 0 ? V3(0, 0, 0) : V3(brng.range(-4, 4), 0, brng.range(-4, 4)));
                        t.pos.y = C.y; t.radius = 2.2f; t.delay = 1.3f + k * 0.15f; t.kind = 1;
                        g.teles.push_back(t);
                    }
                    audio::playAt(SFX_FLUTTER, b.pos, 1.5f, 0.6f);
                } else if (pick == 1) {  // swarm
                    int moths = 0;
                    for (auto& e : g.enemies) if (e.alive && e.type == E_MOTH) moths++;
                    int n = std::min(b.phase ? 4 : 3, 6 - moths);
                    for (int k = 0; k < n; k++) g.spawnEnemy(E_MOTH, b.pos + V3(brng.range(-2, 2), -2.5f, brng.range(-2, 2)), 99);
                    audio::playAt(SFX_FLUTTER, b.pos, 1.5f, 1.2f);
                } else {  // gust: snuff a lit brazier
                    b.state = 2; b.i[1] = -1;
                    float best = 1e9f;
                    for (int k = 0; k < (int)g.lv.braziers.size(); k++) {
                        auto& br = g.lv.braziers[k];
                        if (!br.bossBrazier || !br.lit) continue;
                        float d = len(br.pos - b.pos);
                        if (d < best) { best = d; b.i[1] = k; }
                    }
                    if (b.i[1] < 0) b.state = 0;
                }
            }
        } break;
        case 2: {  // fly to the brazier and blow it out
            auto& br = g.lv.braziers[b.i[1]];
            V3 want = br.pos + V3(0, 4.5f, 0);
            b.pos = lerp(b.pos, want, 1 - std::exp(-dt * 3));
            if (b.stateT > 1.6f) {
                br.lit = false;
                audio::playAt(SFX_FLUTTER, br.pos, 2, 0.4f);
                g.burst(br.pos, hexColor(0xd8d0c0), 20, 5, 0.15f, 2, 0.8f, 0);
                if (len(g.pl.pos - br.pos) < 5) { g.pl.vel += norm(flatXZ(g.pl.pos - br.pos) + V3(0, 0.01f, 0)) * 10; }
                b.state = 0; b.stateT = 0;
            }
        } break;
        case 4: {  // dazzled: drops to the floor, vulnerable
            V3 want = C + V3(0, 0.8f, 0);
            b.pos = lerp(b.pos, want, 1 - std::exp(-dt * 3));
            vulnerable = b.stateT > 0.8f && b.stateT < 6.8f;
            if (b.stateT > 6.8f) {
                for (auto& br : g.lv.braziers) if (br.bossBrazier && br.lit) { br.lit = false; g.burst(br.pos, hexColor(0xd8d0c0), 12, 4, 0.15f, 2, 0.6f, 0); }
                audio::play(SFX_FLUTTER, 1.5f, 0.5f);
                b.state = 0; b.stateT = 0;
            }
        } break;
    }
    b.wps.clear();
    b.wps.push_back({b.pos + V3(0, 0.6f, 0), 1.7f, vulnerable, 0});
}

static void hush(Game& g, float dt) {
    Boss& b = g.boss;
    V3 C = g.lv.boss.center;
    b.stateT += dt;
    b.t += dt;
    b.pos = C + V3(0, 7 + std::sin(g.time * 0.7f) * 0.4f, 10);
    b.wps.clear();
    float& lampOpen = b.f[5];
    if (b.phase == 0) {
        // ---------------- the hands
        int alive = 0;
        for (int h = 0; h < 2; h++) {
            auto& H = b.hands[h];
            H.flash = std::max(0.f, H.flash - dt);
            if (!H.alive) continue;
            alive++;
            H.t -= dt;
            switch (H.state) {
                case 0: {  // drift near the player
                    V3 want = H.home + (flatXZ(g.pl.pos - C)) * 0.35f;
                    H.pos = lerp(H.pos, want, 1 - std::exp(-dt * 2));
                    // don't let both hands slam at once
                    bool otherBusy = b.hands[1 - h].alive && b.hands[1 - h].state != 0;
                    if (H.t <= 0 && !otherBusy && !g.pl.dead) {
                        H.state = 1; H.t = 1.15f; H.target = g.pl.pos; H.target.y = C.y;
                        Telegraph t; t.pos = H.target; t.radius = 2.6f; t.delay = 1.15f; t.dmg = 1; g.teles.push_back(t);
                        audio::playAt(SFX_WHISPER, H.pos, 0.8f);
                    }
                } break;
                case 1: {  // rising over the target
                    V3 want = H.target + V3(0, 5, 0);
                    H.pos = lerp(H.pos, want, 1 - std::exp(-dt * 6));
                    if (H.t <= 0) { H.state = 2; H.t = 0.12f; }
                } break;
                case 2: {  // slam
                    H.pos = lerp(H.pos, H.target + V3(0, 0.9f, 0), 1 - std::exp(-dt * 30));
                    if (H.t <= 0) {
                        H.state = 3; H.t = 3.6f; H.down = true; H.shroud = 1;
                        Ring r; r.c = H.target + V3(0, 0.05f, 0); r.r = 2.6f; r.speed = 9; r.maxR = 11; r.color = hexColor(0x9a5aff);
                        g.rings.push_back(r);
                        g.addShake(0.4f);
                    }
                } break;
                case 3:  // pinned to the floor: eye exposed (under its shroud)
                    if (H.t <= 0) { H.state = 4; H.t = 0.8f; H.down = false; H.shroud = 1; }
                    break;
                case 4:
                    H.pos = lerp(H.pos, H.home, 1 - std::exp(-dt * 3));
                    if (H.t <= 0) { H.state = 0; H.t = brng.range(1.2f, 2.2f); }
                    break;
            }
            b.wps.push_back({H.pos + V3(0, 0.4f, 0), 1.3f, H.down && H.shroud < 0.5f, 10 + h});
        }
        // wraiths join the fight
        b.f[0] += dt;
        if (b.f[0] > 14) {
            b.f[0] = 0;
            int w = 0;
            for (auto& e : g.enemies) if (e.alive && e.type == E_WRAITH) w++;
            if (w < 2) { float a = brng.range(0, TAU); g.spawnEnemy(E_WRAITH, C + V3(std::cos(a) * 12, 0.2f, std::sin(a) * 12), 99); }
        }
        if (alive == 0) {
            b.phase = 1; b.stateT = 0;
            audio::play(SFX_ROAR, 1.3f, 0.7f);
            g.addShake(0.8f);
            g.showToast("THE HEART OF THE HUSH", "Bloom the three sealed wicks around the Lamp");
            for (auto& e : g.enemies) if (e.alive && e.type == E_WRAITH) { e.alive = false; }
        }
    } else if (b.phase == 1 || b.phase == 2) {
        // ---------------- the lamp and its wicks
        int lit = 0;
        std::vector<int> wicks;
        for (int k = 0; k < (int)g.lv.braziers.size(); k++) if (g.lv.braziers[k].bossBrazier) { wicks.push_back(k); if (g.lv.braziers[k].lit) lit++; }
        if (b.phase == 1) {
            if (lit >= (int)wicks.size() && lampOpen < 0.01f && b.i[3] == 0) {
                b.i[3] = 1; b.f[6] = 0;
                audio::play(SFX_BREAK, 1.2f, 0.6f);
                g.statusMsg = "The Lamp opens - strike the heart!"; g.statusT = 2;
            }
            if (b.i[3] == 1) {
                b.f[6] += dt;
                lampOpen = approach(lampOpen, b.f[6] < 7.5f ? 1.f : 0.f, dt * 2);
                if (b.f[6] > 8.0f) {
                    b.i[3] = 0;
                    for (int k : wicks) g.lv.braziers[k].lit = false;
                    audio::play(SFX_WHISPER, 1.2f, 0.5f);
                }
            } else {
                // a tendril snuffs a lit wick from time to time
                b.f[1] += dt;
                if (b.f[1] > 8 && lit > 0) {
                    b.f[1] = 0;
                    for (int k : wicks) if (g.lv.braziers[k].lit) { b.i[4] = k; break; }
                    b.f[2] = 2.2f;  // tendril timer
                    audio::playAt(SFX_WHISPER, g.lv.braziers[b.i[4]].pos, 1);
                }
                if (b.f[2] > 0) {
                    b.f[2] -= dt;
                    if (b.f[2] <= 0 && g.lv.braziers[b.i[4]].lit) {
                        g.lv.braziers[b.i[4]].lit = false;
                        g.burst(g.lv.braziers[b.i[4]].pos, hexColor(0x2a1a3a), 20, 4, 0.2f, 2, 0.8f, 0);
                    }
                }
            }
            if (b.hp <= 8) {
                b.phase = 2; b.stateT = 0;
                lampOpen = 1;
                b.v[2] = C + V3(0, 3, 0);
                audio::play(SFX_ROAR, 1.4f, 0.6f);
                g.addShake(0.8f);
                g.showToast("IT FLEES THE LAMP", "Finish it");
                for (int k : wicks) g.lv.braziers[k].lit = true;
            }
        }
        // shadow rain + orbs
        b.f[3] += dt;
        float rainEvery = b.phase == 2 ? 2.4f : 3.2f;
        if (b.f[3] > rainEvery) {
            b.f[3] = 0;
            int n = b.phase == 2 ? 3 : 2;
            for (int k = 0; k < n; k++) {
                Telegraph t; t.pos = g.pl.pos + (k == 0 ? V3(0, 0, 0) : V3(brng.range(-5, 5), 0, brng.range(-5, 5)));
                t.pos.y = C.y; t.radius = 2.0f; t.delay = 1.3f; g.teles.push_back(t);
            }
        }
        b.f[4] += dt;
        if (b.f[4] > (b.phase == 2 ? 3.5f : 5.5f)) {
            b.f[4] = 0;
            V3 src = b.phase == 2 ? b.v[2] : b.pos;
            for (int k = 0; k < (b.phase == 2 ? 3 : 2); k++) {
                Projectile pr; pr.type = PR_ORB; pr.pos = src + V3(brng.range(-1, 1), brng.range(-1, 1), brng.range(-1, 1));
                pr.vel = V3(brng.range(-3, 3), 2, brng.range(-3, 3)); pr.life = 7; pr.radius = 0.4f;
                g.projs.push_back(pr);
            }
            audio::playAt(SFX_WHISPER, src, 1, 1.4f);
        }
        if (b.phase == 1) {
            b.wps.push_back({C + V3(0, 3.2f, 0), 1.3f, lampOpen > 0.6f, 20});
        } else {
            // the heart darts around the summit
            float a = g.time * 0.6f;
            V3 want = C + V3(std::cos(a) * 7, 5.5f + std::sin(g.time * 1.9f) * 1.5f, std::sin(a * 1.3f) * 7);
            b.v[2] = lerp(b.v[2], want, 1 - std::exp(-dt * 1.5f));
            b.wps.push_back({b.v[2], 1.3f, true, 21});
        }
    }
}


// KING RATTLETRAP - a rat in a junk crane. Dodge the claw; when it gets STUCK in the floor
// the cockpit pops open and the rat is exposed.
static const char* RAT_TAUNTS[] = {
    "KING RATTLETRAP: SQUEAK! This junk is MINE!",
    "KING RATTLETRAP: I'll recycle you into a nightlight!",
    "KING RATTLETRAP: cheese for the cheese god!!",
    "KING RATTLETRAP: my claw machine NEVER loses",
};
static void ratking(Game& g, float dt) {
    Boss& b = g.boss;
    V3 C = g.lv.boss.center;
    b.stateT += dt;
    V3& claw = b.v[0];
    if (b.i[2] == 0) { b.i[2] = 1; claw = C + V3(0, 7, 4); g.statusMsg = "Dodge the CLAW - when it gets STUCK, burn the RAT in the cockpit!"; g.statusT = 5; }
    if (b.phase == 0 && b.hp <= 8) { b.phase = 1; audio::play(SFX_ROAR, 1, 2.2f); g.statusMsg = "KING RATTLETRAP: you're making me SQUEAKY MAD"; g.statusT = 3; }
    b.yaw = angleApproach(b.yaw, std::atan2(g.pl.pos.x - C.x, g.pl.pos.z - C.z), dt * 1.5f);
    bool open = false;
    static const int P0[] = {0, 1, 0, 2};
    static const int P1[] = {0, 0, 1, 2, 0, 1};
    switch (b.state) {
        case 0:
            claw = lerp(claw, C + V3(std::sin(b.yaw) * 4, 7, std::cos(b.yaw) * 4), 1 - std::exp(-dt * 3));
            if (b.stateT > (b.phase ? 0.8f : 1.2f)) {
                int nxt = b.phase ? P1[b.i[0] % 6] : P0[b.i[0] % 4];
                b.i[0]++;
                b.state = 1 + nxt; b.stateT = 0; b.i[1] = 0;
                if (b.state == 1) {
                    b.v[1] = g.pl.pos; b.v[1].y = C.y;
                    Telegraph t; t.pos = b.v[1]; t.radius = 2.4f; t.delay = b.phase ? 1.2f : 1.5f; t.dmg = 1; g.teles.push_back(t);
                    audio::playAt(SFX_CLANK, claw, 1.2f, 0.8f);
                }
            }
            break;
        case 1: {  // claw hovers over the marked spot, slams, gets stuck
            float tele = b.phase ? 1.2f : 1.5f;
            if (b.stateT < tele) claw = lerp(claw, b.v[1] + V3(0, 6, 0), 1 - std::exp(-dt * 6));
            else if (b.stateT < tele + 0.12f) claw = lerp(claw, b.v[1] + V3(0, 0.8f, 0), 1 - std::exp(-dt * 30));
            else {
                if (b.i[1] == 0) {
                    b.i[1] = 1;
                    Ring r; r.c = b.v[1] + V3(0, 0.05f, 0); r.r = 2; r.speed = 8; r.maxR = 8; r.color = hexColor(0xffa040);
                    g.rings.push_back(r);
                    g.addShake(0.4f);
                    audio::playAt(SFX_SLAM, b.v[1], 1.4f);
                    g.floatText(C + V3(0, 7, 0), "CLAW STUCK!", hexColor(0xffe060));
                }
                float stuck = b.phase ? 3.0f : 3.6f;
                open = b.stateT < tele + 0.12f + stuck;
                if (std::fmod(b.stateT, 0.3f) < dt) { g.burst(b.v[1] + V3(0, 0.5f, 0), hexColor(0x8a7060), 4, 3, 0.1f, 0, 0.5f, 6); audio::playAt(SFX_CLANK, claw, 0.5f, 1.2f); }
                if (!open) { b.state = 0; b.stateT = 0; }
            }
        } break;
        case 2: {  // scrap toss
            int n = b.phase ? 5 : 3;
            if (b.i[1] < n && b.stateT > 0.5f + b.i[1] * 0.25f) {
                V3 o = C + V3(0, 5.5f, 0);
                V3 tgt = g.pl.pos + V3(brng.range(-3, 3), 0, brng.range(-3, 3)) * (b.i[1] ? 1.f : 0.f);
                V3 d = tgt - o; float D = lenXZ(d); float T = clampf(D / 10.f, 0.8f, 1.5f);
                Projectile pr; pr.type = PR_TAR; pr.pos = o; pr.gravity = true; pr.radius = 0.45f; pr.spin = 77;
                pr.vel = V3(d.x / T, (d.y + 10 * T * T) / T, d.z / T);
                g.projs.push_back(pr);
                audio::playAt(SFX_SPIT, o, 1, 0.6f);
                b.i[1]++;
            }
            if (b.stateT > 0.5f + n * 0.25f + 0.6f) { b.state = 0; b.stateT = 0; }
        } break;
        case 3: {  // rat swarm
            if (b.i[1] == 0) {
                b.i[1] = 1;
                int alive = 0; for (auto& e : g.enemies) if (e.alive) alive++;
                for (int k = 0; k < 4 && alive < 8; k++, alive++) { float a = brng.range(0, TAU); g.spawnEnemy(E_RAT, C + V3(std::cos(a) * 4, 0.3f, std::sin(a) * 4), 99); }
                g.statusMsg = RAT_TAUNTS[brng.irange(0, 3)]; g.statusT = 2.5f;
                audio::playAt(SFX_FLUTTER, C, 1.5f, 2);
            }
            if (b.stateT > 1.2f) { b.state = 0; b.stateT = 0; }
        } break;
    }
    b.f[0] = open ? approach(b.f[0], 1, dt * 4) : approach(b.f[0], 0, dt * 3);  // hatch open amount
    b.wps.clear();
    b.wps.push_back({C + V3(0, 5.6f, 0), 1.4f, b.f[0] > 0.6f, 0});
    b.wps.push_back({claw, 1.0f, false, 1});
}

void Game::updateBoss(float dt) {
    if (!boss.active) return;
    if (boss.f[15] > 0) {  // intro
        boss.f[15] -= dt;
        camOverride = std::min(1.f, boss.f[15] / 0.6f);
        if (boss.f[15] <= 0) camOverride = 0;
    }
    boss.flash = std::max(0.f, boss.flash - dt);
    if (boss.dead) {
        boss.deathT += dt;
        if (std::fmod(boss.deathT, 0.15f) < dt) {
            V3 p = (boss.type == B_HUSH ? lv.boss.center + V3(0, 4, 0) : (boss.wps.empty() ? boss.pos : boss.wps[0].pos)) +
                   V3(brng.range(-2, 2), brng.range(-1, 2), brng.range(-2, 2));
            burst(p, hexColor(0xffb050), 12, 6, 0.12f, 3, 0.6f, 0);
            burst(p, hexColor(0x1a1028), 8, 3, 0.3f, 2, 0.9f, -1);
            audio::playAt(SFX_HIT, p, 0.6f, 0.5f + brng.f());
        }
        if (boss.deathT > 3.0f) bossDefeated();
        return;
    }
    if (boss.f[15] > 0.6f) return;  // let the intro breathe
    switch (boss.type) {
        case B_BELLWETHER: bellwether(*this, dt); break;
        case B_WARDEN: hacker(*this, dt); break;
        case B_MATRON: matron(*this, dt); break;
        case B_HUSH: hush(*this, dt); break;
        case B_RATKING: ratking(*this, dt); break;
        default: break;
    }
}
