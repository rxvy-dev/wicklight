// WICKLIGHT - the three late powers
//  OVERCLOCK     (from GL1TCH's Beacon-Heart): land hits to charge it, then slow the whole world down
//  FIREWALL      (from the Clockwood server room): drop a wall of fire that enemies can't cross
//  EMBER GRAPPLE (from the Lumen City rescue): hold on to a hooked flame and swing on it like a rope
#include "game.h"

extern int g_dmgKind;

static float segDistXZ(const V3& a, const V3& b, const V3& p, float* tOut = nullptr) {
    V3 ab = flatXZ(b - a), ap = flatXZ(p - a);
    float l2 = len2(ab);
    float t = l2 > 1e-6f ? clampf(dot(ap, ab) / l2, 0, 1) : 0;
    if (tOut) *tOut = t;
    return len(ap - ab * t);
}

void Game::placeFirewall() {
    V3 f = flatXZ(aimDir);
    if (len(f) < 0.1f) f = V3(std::sin(pl.yaw), 0, std::cos(pl.yaw));
    f = norm(f);
    V3 side(-f.z, 0, f.x);
    V3 c = pl.pos + f * 2.6f;
    float g = groundHeightBelow(c + V3(0, 2.0f, 0), 6);
    c.y = g > -1e8f ? g : pl.pos.y;
    FireWall w; w.a = c - side * 3.2f; w.b = c + side * 3.2f; w.t = 0; w.life = 6.0f + 1.0f * upLevel("heat");
    firewalls.push_back(w);
    if (firewalls.size() > 2) firewalls.erase(firewalls.begin());
    fwCd = 8.5f;
    audio::playAt(SFX_BRAZIER, c, 1.3f, 0.7f);
    audio::playAt(SFX_BLOOM, c, 0.6f, 1.4f);
    addShake(0.15f);
    for (int k = 0; k <= 8; k++) burst(lerp(w.a, w.b, k / 8.f) + V3(0, 0.4f, 0), hexColor(0xffa040), 6, 4, 0.1f, 3, 0.6f, -3);
    if (!has("tut.firewall")) setFlag("tut.firewall");
}

void Game::updateGrapple(float dt, const InputState& in) {
    Player& p = pl;
    bool anchored = flame.st == F_HOVER && flame.anchored >= 0;
    if (!grappling) {
        if (!ability("grapple") || lv.id == LV_HOME || p.dead) return;
        bool want = in.pressed[A_GRAPPLE] || (in.down[A_GRAPPLE] && anchored);
        if (!want) return;
        if (!anchored) {
            if (in.pressed[A_GRAPPLE] && statusT <= 0.2f) { statusMsg = "Throw your flame onto a brass hook, then hold GRAPPLE to swing"; statusT = 1.6f; }
            return;
        }
        grappling = true;
        ropeAnchor = flame.pos;
        ropeLen = std::max(2.4f, len(ropeAnchor - (p.pos + V3(0, 1.0f, 0))));
        p.grounded = false;
        if (p.vel.y < 0) p.vel.y *= 0.5f;
        audio::playAt(SFX_CATCH, ropeAnchor, 1.0f, 0.6f);
        audio::play(SFX_DASH, 0.7f, 0.7f);
        if (!has("tut.grapple")) setFlag("tut.grapple");
        return;
    }
    // released, or the flame left the hook
    auto release = [&](bool pop) {
        grappling = false;
        p.flingT = 1.2f;
        if (pop) { p.vel.y = std::max(p.vel.y, 11.f); burst(p.pos + V3(0, 0.8f, 0), hexColor(0xffc060), 16, 4, 0.08f, 3, 0.4f, 0); audio::play(SFX_JUMP, 0.9f, 1.3f); }
        else p.vel.y += 2.5f;
        p.airJumps = upLevel("jump");
        if (flame.st == F_HOVER) recallFlame();
    };
    if (!anchored || p.dead) { grappling = false; return; }
    if (!in.down[A_GRAPPLE]) { release(false); return; }
    flame.hoverT = 9;
    V3 hand = p.pos + V3(0, 1.0f, 0);
    // reel in (the rope never goes slack), lighten gravity, and steer the swing with the movement keys
    ropeLen = std::max(2.2f, std::min(ropeLen, len(hand - ropeAnchor)) - 11.f * dt);
    p.vel.y += GRAVITY * 0.6f * dt;
    if (hand.y < ropeAnchor.y - 0.5f) p.vel.y += 9.f * dt;  // the fire-rope lifts as it pulls
    V3 fwd(std::sin(camYaw), 0, std::cos(camYaw)), right(-std::cos(camYaw), 0, std::sin(camYaw));
    V3 wish = fwd * in.moveY + right * in.moveX;
    if (len(wish) > 1) wish = norm(wish);
    p.vel += wish * (16.f * dt);
    // rope constraint (soft): no stretching past the rope length
    V3 np = hand + p.vel * dt;
    V3 d = np - ropeAnchor;
    float dl = len(d);
    if (dl > ropeLen && dl > 1e-4f) {
        V3 n = d / dl;
        float vr = dot(p.vel, n);
        if (vr > 0) p.vel -= n * vr;
        p.vel -= n * std::min((dl - ropeLen) / dt * 0.35f, 30.f);
    }
    float sp = len(p.vel);
    if (sp > 26) p.vel = p.vel * (26 / sp);
    if (len(hand - ropeAnchor) < 3.0f) release(true);
    // rope sparks
    if (!headless && R && fxTick(0.05f)) burst(lerp(hand, ropeAnchor, 0.5f), hexColor(0xffb050), 1, 1, 0.06f, 3, 0.25f, 2);
}

void Game::updatePowers(float dt, const InputState& in) {
    fwCd = std::max(0.f, fwCd - dt);
    // ---- overclock
    if (ocT > 0) {
        ocT -= dt;
        if (ocT <= 0) { ocT = 0; audio::play(SFX_UNLOCK, 0.6f, 0.6f); }
    }
    if (in.pressed[A_OVERCLOCK] && lv.id != LV_HOME && !pl.dead) {
        if (!ability("overclock")) { statusMsg = "Overclock is earned by beating GL1TCH in the Clockwood"; statusT = 1.5f; }
        else if (ocT > 0) {}
        else if (ocCharge >= 1) {
            ocT = 4.5f + 0.5f * upLevel("heat"); ocCharge = 0;
            audio::play(SFX_CHARGE, 1.0f, 0.5f);
            audio::play(SFX_WARP, 0.8f, 0.6f);
            flashOverlay = 0.4f; flashColor = hexColor(0x60e0ff);
            glitch = std::max(glitch, 0.35f);
            if (!has("tut.overclock")) setFlag("tut.overclock");
        } else { char b[96]; snprintf(b, sizeof(b), "Overclock is charging (%d%%) - land hits and combos to fill it", (int)(ocCharge * 100)); statusMsg = b; statusT = 1.5f; }
    }
    // ---- firewall
    if (in.pressed[A_FIREWALL] && lv.id != LV_HOME && !pl.dead) {
        if (!ability("firewall")) { statusMsg = "Firewall is hidden in the Clockwood's server room"; statusT = 1.5f; }
        else if (fwCd > 0) { audio::play(SFX_MENU_BACK, 0.5f); }
        else placeFirewall();
    }
    for (auto& w : firewalls) {
        w.t += dt;
        V3 n = norm(V3(-(w.b.z - w.a.z), 0, w.b.x - w.a.x));
        // enemies can't cross, and burn while they touch it
        for (auto& e : enemies) {
            if (!e.alive || e.clinging) continue;
            if (e.pos.y > w.a.y + 2.4f || e.pos.y + e.height < w.a.y - 0.3f) continue;
            float tt;
            float d = segDistXZ(w.a, w.b, e.pos, &tt);
            float reach = e.radius + 0.35f;
            if (d > reach) continue;
            float s = dot(flatXZ(e.pos - w.a), n);
            float sgn = s >= 0 ? 1.f : -1.f;
            if (tt > 0.001f && tt < 0.999f) {
                e.pos += n * (sgn * (reach - std::fabs(s)));
                float vn = dot(e.vel, n);
                if (vn * sgn < 0) e.vel -= n * vn;
            }
            e.burnT -= dt;
            if (e.burnT <= 0) {
                e.burnT = 0.5f;
                g_dmgKind = 2;  // fire from all sides: ignores shields
                damageEnemy(e, 1 + 0.5f * upLevel("heat"), e.pos - n * sgn, false);
                g_dmgKind = 0;
            }
        }
        // enemy shots burn up in it
        for (auto& pr : projs) {
            if (!pr.alive || pr.type == PR_GEAR) continue;
            if (pr.pos.y < w.a.y - 0.3f || pr.pos.y > w.a.y + 2.8f) continue;
            if (segDistXZ(w.a, w.b, pr.pos) < pr.radius + 0.3f) { pr.alive = false; burst(pr.pos, hexColor(0xffa040), 8, 3, 0.08f, 3, 0.3f, 0); audio::playAt(SFX_HIT, pr.pos, 0.4f, 1.6f); }
        }
        for (int i = 0; i < (int)lv.braziers.size(); i++)
            if (!lv.braziers[i].lit && std::fabs(lv.braziers[i].pos.y - w.a.y - 1.3f) < 1.5f && segDistXZ(w.a, w.b, lv.braziers[i].pos) < 0.8f) lightBrazier(i, false);
        for (int i = 0; i < (int)lv.breakables.size(); i++)
            if (!lv.breakables[i].broken && std::fabs(lv.breakables[i].pos.y - w.a.y) < 1.5f && segDistXZ(w.a, w.b, lv.breakables[i].pos) < 0.7f) breakUrn(i);
        if (!headless && R && fxTick(0.03f)) {
            float u = hashf((int)(time * 97), 3);
            V3 q = lerp(w.a, w.b, u);
            Particle pt; pt.pos = q + V3(0, 0.2f, 0); pt.vel = V3(0, 3.f, 0); pt.color = hexColor(0xffa040); pt.alpha = 1; pt.size = 0.14f;
            pt.life = pt.maxLife = 0.5f; pt.kind = 3; pt.drag = 1;
            R->spawn(pt);
        }
    }
    firewalls.erase(std::remove_if(firewalls.begin(), firewalls.end(), [](const FireWall& w) { return w.t > w.life; }), firewalls.end());
}
