// WICKLIGHT - level infrastructure: builder helpers, collision grid, raycasts, movement
#include "builder.h"
#include <cstdio>

// ================================================================== builder
int LB::solid(const AABB& b, int tile, int top, V3 col, bool visible, float uv) {
    Solid s; s.box = b; s.tile = tile; s.topTile = top; s.color = col; s.visible = visible; s.uvScale = uv;
    L.solids.push_back(s);
    return (int)L.solids.size() - 1;
}
void LB::slab(float x0, float z0, float x1, float z1, float y, int top, int side, float depth, V3 col) {
    solid(AABB(V3(std::min(x0, x1), y - depth, std::min(z0, z1)), V3(std::max(x0, x1), y, std::max(z0, z1))), side, top, col);
}
void LB::wall(float x0, float z0, float x1, float z1, float y0, float y1, int tile, V3 col) {
    solid(AABB(V3(std::min(x0, x1), y0, std::min(z0, z1)), V3(std::max(x0, x1), y1, std::max(z0, z1))), tile, -1, col);
    // walls are borders: you can't stand on them, warp over them or throw your flame across them
    int b = solid(AABB(V3(std::min(x0, x1), y1, std::min(z0, z1)), V3(std::max(x0, x1), y1 + 40, std::max(z0, z1))), T_WHITE, -1, V3(1, 1, 1), false);
    L.solids[b].barrier = true;
}
void LB::invisibleWall(float x0, float z0, float x1, float z1, float y0, float y1) {
    solid(AABB(V3(std::min(x0, x1), y0, std::min(z0, z1)), V3(std::max(x0, x1), y1, std::max(z0, z1))), T_WHITE, -1, V3(1, 1, 1), false);
}
int LB::dynBox(DynKind k, const AABB& b, int tile, int top, V3 col) {
    DynSolid d; d.kind = k; d.base = b; d.tile = tile; d.topTile = top; d.color = col;
    L.dyn.push_back(d);
    return (int)L.dyn.size() - 1;
}
int LB::mover(const AABB& b, V3 a, V3 bb, float period, float phase, int tile, int top) {
    int i = dynBox(DK_MOVER, b, tile, top);
    L.dyn[i].a = a; L.dyn[i].b = bb; L.dyn[i].period = period; L.dyn[i].phase = phase;
    return i;
}
int LB::gate(const AABB& b, int group, const std::string& flag, int tile) {
    int i = dynBox(DK_GATE, b, tile, -1, V3(1, 1, 1));
    L.dyn[i].flag = flag;
    L.gateLinks.push_back({group, i, flag});
    return i;
}
int LB::seal(const AABB& b, const std::string& flag) {
    int i = dynBox(DK_SEAL, b, T_SEAL, -1);
    L.dyn[i].flag = flag;
    return i;
}
int LB::crystal(const AABB& b, const std::string& flag) {
    int i = dynBox(DK_CRYSTAL, b, T_CRYSTAL, -1, hexColor(0xc8f0ff));
    L.dyn[i].flag = flag;
    return i;
}
void LB::brazier(V3 base, int group, bool sealed, float timed, bool boss) {
    // stone stand + iron bowl
    L.decor.cylinder(M4::translate(base + V3(0, 0.5f, 0)) * M4::scale(V3(0.5f, 1.f, 0.5f)), sealed ? T_SEAL : T_STONEBRICK, 8);
    L.decor.cylinder(M4::translate(base + V3(0, 1.1f, 0)) * M4::scale(V3(1.0f, 0.3f, 1.0f)), T_METAL, 10);
    if (timed > 0) {  // brass ring marks timed braziers
        L.decor.cylinder(M4::translate(base + V3(0, 0.2f, 0)) * M4::scale(V3(0.9f, 0.15f, 0.9f)), T_BRASS, 10);
    }
    solid(AABB(base + V3(-0.35f, 0, -0.35f), base + V3(0.35f, 1.2f, 0.35f)), T_STONEBRICK, -1, V3(1, 1, 1), false);
    Brazier b; b.pos = base + V3(0, 1.35f, 0); b.gateGroup = group; b.sealed = sealed; b.timed = timed; b.bossBrazier = boss;
    L.braziers.push_back(b);
}
void LB::lantern(V3 base, int spawnIdx) {
    // wrought lantern on a short post
    L.decor.cylinder(M4::translate(base + V3(0, 0.7f, 0)) * M4::scale(V3(0.18f, 1.4f, 0.18f)), T_METAL, 6);
    L.decor.cube(M4::translate(base + V3(0, 1.45f, 0)) * M4::scale(V3(0.7f, 0.12f, 0.7f)), T_METAL);
    L.decor.cone(M4::translate(base + V3(0, 2.3f, 0)) * M4::scale(V3(0.8f, 0.4f, 0.8f)), T_METAL, 4);
    Lantern l; l.pos = base + V3(0, 1.8f, 0); l.spawn = spawnIdx;
    L.lanterns.push_back(l);
}
void LB::pickup(PickupType t, V3 pos, int index, const std::string& flag) {
    Pickup p; p.type = t; p.pos = pos; p.index = index; p.flag = flag;
    L.pickups.push_back(p);
}
void LB::exit(const AABB& zone, LevelId target, int sp, const std::string& need, const std::string& lockedMsg, const std::string& label) {
    L.exits.push_back({zone, target, sp, need, lockedMsg, label});
}
int LB::spawn(V3 pos, float yaw) { L.spawns.push_back({pos, yaw}); return (int)L.spawns.size() - 1; }
void LB::anchor(V3 pos, bool withPost) {
    L.anchors.push_back({pos});
    if (withPost) L.decor.cylinder(M4::translate(pos - V3(0, 1.2f, 0)) * M4::scale(V3(0.12f, 1.6f, 0.12f)), T_BRASS, 6);
}
void LB::hint(const AABB& zone, const std::string& text, const std::string& need, const std::string& hide) {
    L.hints.push_back({zone, text, need, hide});
}
void LB::light(V3 pos, float radius, V3 color, float intensity) { L.lights.push_back({pos, radius, color, intensity}); }
void LB::enemy(EnemyType t, V3 pos, int ar) { L.enemySpawns.push_back({t, pos, ar}); }
int LB::arena(const AABB& zone, std::vector<int> gates, const std::string& flag) {
    Arena a; a.zone = zone; a.gates = gates; a.flag = flag;
    L.arenas.push_back(a);
    return (int)L.arenas.size() - 1;
}
void LB::talker(V3 pos, float yaw, int who) { L.talkers.push_back({pos, yaw, who}); }
void LB::emitter(const AABB& zone, int kind, float rate) { L.emitters.push_back({zone, kind, rate}); }

void LB::pillar(float x, float z, float y0, float y1, float r, int tile, bool collide, V3 col) {
    V3 c(x, (y0 + y1) / 2, z);
    L.decor.color = col;
    L.decor.cylinder(M4::translate(c) * M4::scale(V3(2 * r, y1 - y0, 2 * r)), tile, 10, true, (y1 - y0) / 2);
    L.decor.cube(M4::translate(V3(x, y0 + 0.2f, z)) * M4::scale(V3(2 * r + 0.4f, 0.4f, 2 * r + 0.4f)), tile);
    L.decor.cube(M4::translate(V3(x, y1 - 0.2f, z)) * M4::scale(V3(2 * r + 0.4f, 0.4f, 2 * r + 0.4f)), tile);
    L.decor.color = V3(1, 1, 1);
    if (collide) solid(AABB(V3(x - r * 0.85f, y0, z - r * 0.85f), V3(x + r * 0.85f, y1, z + r * 0.85f)), tile, -1, V3(1, 1, 1), false);
}
void LB::stairs(V3 s, V3 dir, float width, int steps, float rise, float run, int tile, int top) {
    V3 side = V3(-dir.z, 0, dir.x);
    for (int i = 0; i < steps; i++) {
        V3 c0 = s + dir * (run * i) - side * (width / 2);
        V3 c1 = s + dir * (run * (i + 1)) + side * (width / 2);
        float ytop = s.y + rise * (i + 1);
        AABB b(V3(std::min(c0.x, c1.x), s.y - 0.5f, std::min(c0.z, c1.z)), V3(std::max(c0.x, c1.x), ytop, std::max(c0.z, c1.z)));
        solid(b, tile, top);
    }
}
void LB::lampPost(V3 base, bool lit, V3 col) {
    L.decor.cylinder(M4::translate(base + V3(0, 1.6f, 0)) * M4::scale(V3(0.16f, 3.2f, 0.16f)), T_METAL, 6);
    L.decor.cube(M4::translate(base + V3(0, 0.15f, 0)) * M4::scale(V3(0.5f, 0.3f, 0.5f)), T_METAL);
    L.decor.emis = lit ? 1.2f : 0.f;
    L.decor.color = lit ? col : V3(0.3f, 0.3f, 0.35f);
    L.decor.cube(M4::translate(base + V3(0, 3.35f, 0)) * M4::scale(V3(0.45f, 0.5f, 0.45f)), T_WHITE);
    L.decor.emis = 0; L.decor.color = V3(1, 1, 1);
    L.decor.cone(M4::translate(base + V3(0, 3.75f, 0)) * M4::scale(V3(0.7f, 0.35f, 0.7f)), T_METAL, 4);
    solid(AABB(base + V3(-0.15f, 0, -0.15f), base + V3(0.15f, 3.4f, 0.15f)), T_METAL, -1, V3(1, 1, 1), false);
    if (lit) light(base + V3(0, 3.3f, 0), 9, col, 1.1f);
}
void LB::tree(V3 base, float s, int leafTile, bool collide) {
    float h = 4.5f * s;
    L.decor.cylinder(M4::translate(base + V3(0, h / 2, 0)) * M4::scale(V3(0.7f * s, h, 0.7f * s)), T_BARK, 8, true, h / 2);
    Rng r(hash32((uint32_t)(base.x * 13 + base.z * 7 + 1000)));
    for (int i = 0; i < 4; i++) {
        V3 o(r.range(-1.2f, 1.2f) * s, h + r.range(-0.5f, 1.2f) * s, r.range(-1.2f, 1.2f) * s);
        float sz = r.range(2.2f, 3.2f) * s;
        L.decor.sphere(M4::translate(base + o) * M4::scale(V3(sz, sz * 0.8f, sz)), leafTile, 7, 5);
    }
    if (collide) solid(AABB(base + V3(-0.3f * s, 0, -0.3f * s), base + V3(0.3f * s, h, 0.3f * s)), T_BARK, -1, V3(1, 1, 1), false);
}
void LB::pine(V3 base, float s) {
    L.decor.cylinder(M4::translate(base + V3(0, 1.0f * s, 0)) * M4::scale(V3(0.5f * s, 2.0f * s, 0.5f * s)), T_BARK, 6);
    for (int i = 0; i < 3; i++)
        L.decor.cone(M4::translate(base + V3(0, (2.2f + i * 1.3f) * s, 0)) * M4::scale(V3((3.2f - i * 0.8f) * s, 2.2f * s, (3.2f - i * 0.8f) * s)), T_LEAVES, 7);
    solid(AABB(base + V3(-0.25f * s, 0, -0.25f * s), base + V3(0.25f * s, 4 * s, 0.25f * s)), T_BARK, -1, V3(1, 1, 1), false);
}
void LB::house(float x0, float z0, float x1, float z1, float y, float h, int wallTile, bool collide) {
    AABB b(V3(x0, y, z0), V3(x1, y + h, z1));
    if (collide) solid(b, wallTile, T_ROOF);
    else L.decor.box(b, wallTile, 1, T_ROOF);
    // pitched roof along the longer axis
    float w = x1 - x0, d = z1 - z0;
    bool alongX = w >= d;
    V3 c((x0 + x1) / 2, y + h, (z0 + z1) / 2);
    float span = alongX ? d : w, length = alongX ? w : d;
    float rh = span * 0.45f;
    // roof built as two slanted quads + gables
    V3 ax = alongX ? V3(1, 0, 0) : V3(0, 0, 1);
    V3 sx = alongX ? V3(0, 0, 1) : V3(1, 0, 0);
    float over = 0.4f;
    V3 rA = c - ax * (length / 2 + over), rB = c + ax * (length / 2 + over);
    V3 ridgeA = rA + V3(0, rh, 0), ridgeB = rB + V3(0, rh, 0);
    V3 eA1 = rA + sx * (span / 2 + over) - V3(0, 0.3f, 0), eB1 = rB + sx * (span / 2 + over) - V3(0, 0.3f, 0);
    V3 eA2 = rA - sx * (span / 2 + over) - V3(0, 0.3f, 0), eB2 = rB - sx * (span / 2 + over) - V3(0, 0.3f, 0);
    V3 n1 = norm(cross(eB1 - eA1, ridgeA - eA1)); if (n1.y < 0) n1 = -n1;
    V3 n2 = norm(cross(ridgeA - eA2, eB2 - eA2)); if (n2.y < 0) n2 = -n2;
    float ru = (length + 2 * over), rv = (span / 2 + over) * 1.2f;
    L.decor.tri(eA1, eB1, ridgeB, n1, V2(0, rv), V2(ru, rv), V2(ru, 0), T_ROOF);
    L.decor.tri(eA1, ridgeB, ridgeA, n1, V2(0, rv), V2(ru, 0), V2(0, 0), T_ROOF);
    L.decor.tri(eB2, eA2, ridgeA, n2, V2(0, rv), V2(ru, rv), V2(ru, 0), T_ROOF);
    L.decor.tri(eB2, ridgeA, ridgeB, n2, V2(0, rv), V2(ru, 0), V2(0, 0), T_ROOF);
    // underside so it reads from below
    L.decor.tri(eA1, ridgeB, eB1, -n1, V2(0, 0), V2(1, 1), V2(1, 0), T_DARKWOOD);
    L.decor.tri(eA1, ridgeA, ridgeB, -n1, V2(0, 0), V2(0, 1), V2(1, 1), T_DARKWOOD);
    L.decor.tri(eB2, ridgeA, eA2, -n2, V2(0, 0), V2(1, 1), V2(1, 0), T_DARKWOOD);
    L.decor.tri(eB2, ridgeB, ridgeA, -n2, V2(0, 0), V2(0, 1), V2(1, 1), T_DARKWOOD);
    // gables
    V3 gA1 = c - ax * (length / 2) + sx * (span / 2), gA2 = c - ax * (length / 2) - sx * (span / 2), gA3 = c - ax * (length / 2) + V3(0, rh, 0);
    L.decor.tri(gA2, gA1, gA3, -ax, V2(0, 1), V2(1, 1), V2(0.5f, 0), wallTile);
    V3 gB1 = c + ax * (length / 2) + sx * (span / 2), gB2 = c + ax * (length / 2) - sx * (span / 2), gB3 = c + ax * (length / 2) + V3(0, rh, 0);
    L.decor.tri(gB1, gB2, gB3, ax, V2(0, 1), V2(1, 1), V2(0.5f, 0), wallTile);
    // windows (emissive) on the long sides
    Rng r(hash32((uint32_t)(x0 * 31 + z0 * 17 + 7)));
    int nwin = std::max(1, (int)(length / 3));
    for (int i = 0; i < nwin; i++) {
        float t = (i + 0.5f) / nwin;
        for (int sgn = -1; sgn <= 1; sgn += 2) {
            if (r.f() < 0.35f) continue;
            V3 p = c - ax * (length / 2) + ax * (length * t) + sx * (sgn * (span / 2 + 0.02f));
            p.y = y + h * 0.55f;
            M4 m = M4::translate(p) * M4::scale(alongX ? V3(0.8f, 1.0f, 0.06f) : V3(0.06f, 1.0f, 0.8f));
            bool lit = r.f() < 0.4f;
            L.decor.color = lit ? V3(1, 1, 1) : V3(0.15f, 0.15f, 0.25f);
            L.decor.cube(m, T_WINDOW);
            L.decor.color = V3(1, 1, 1);
            if (lit && r.f() < 0.5f) light(p + sx * (float)sgn * 0.8f, 4.5f, hexColor(0xffb060), 0.6f);
        }
    }
}
void LB::crate(V3 base, float s, bool collide) {
    AABB b(base + V3(-s / 2, 0, -s / 2), base + V3(s / 2, s, s / 2));
    if (collide) solid(b, T_PLANK, -1, V3(1, 1, 1), false);
    L.decor.cube(M4::translate(base + V3(0, s / 2, 0)) * M4::scale(V3(s, s, s)), T_PLANK, std::max(1.f, s));
    L.decor.cube(M4::translate(base + V3(0, s / 2, 0)) * M4::scale(V3(s * 1.02f, s * 0.15f, s * 1.02f)), T_DARKWOOD);
}
void LB::barrel(V3 base, bool collide) {
    L.decor.cylinder(M4::translate(base + V3(0, 0.55f, 0)) * M4::scale(V3(0.8f, 1.1f, 0.8f)), T_PLANK, 8);
    L.decor.cylinder(M4::translate(base + V3(0, 0.3f, 0)) * M4::scale(V3(0.84f, 0.1f, 0.84f)), T_METAL, 8, false);
    L.decor.cylinder(M4::translate(base + V3(0, 0.85f, 0)) * M4::scale(V3(0.84f, 0.1f, 0.84f)), T_METAL, 8, false);
    if (collide) solid(AABB(base + V3(-0.4f, 0, -0.4f), base + V3(0.4f, 1.1f, 0.4f)), T_PLANK, -1, V3(1, 1, 1), false);
}
void LB::rock(V3 c, float s, int tile) {
    Rng r(hash32((uint32_t)(c.x * 11 + c.z * 23 + 3)));
    L.decor.octa(M4::translate(c) * M4::rotY(r.range(0, TAU)) * M4::scale(V3(s * r.range(1.4f, 2.2f), s * r.range(0.9f, 1.4f), s * r.range(1.4f, 2.2f))), tile);
}
void LB::spinner(V3 c, float r, float thick, int axis, float speed, int tile, V3 col) { L.spinners.push_back({c, r, thick, axis, speed, tile, col}); }
void LB::chain(V3 top, float length) {
    int n = (int)(length / 0.4f);
    for (int i = 0; i < n; i++) {
        M4 m = M4::translate(top - V3(0, 0.4f * i + 0.2f, 0)) * M4::rotY(i % 2 ? PI / 2 : 0) * M4::scale(V3(0.12f, 0.35f, 0.04f));
        L.decor.cube(m, T_METAL);
    }
}
void LB::tomb(V3 base, float yaw) {
    M4 m = M4::translate(base) * M4::rotY(yaw);
    L.decor.cube(m * M4::translate(V3(0, 0.6f, 0)) * M4::scale(V3(1.0f, 1.2f, 0.25f)), T_STONEBRICK);
    L.decor.cylinder(m * M4::translate(V3(0, 1.2f, 0)) * M4::rotX(PI / 2) * M4::scale(V3(1.0f, 0.25f, 1.0f)), T_STONEBRICK, 8);
    L.decor.cube(m * M4::translate(V3(0, 0.08f, 0.7f)) * M4::scale(V3(1.0f, 0.16f, 1.4f)), T_DIRT);
    solid(AABB(base + V3(-0.5f, 0, -0.5f), base + V3(0.5f, 1.5f, 0.5f)), T_STONEBRICK, -1, V3(1, 1, 1), false);
}
void LB::candles(V3 base, int n) {
    Rng r(hash32((uint32_t)(base.x * 7 + base.z * 3 + 5)));
    for (int i = 0; i < n; i++) {
        V3 p = base + V3(r.range(-0.4f, 0.4f), 0, r.range(-0.4f, 0.4f));
        float h = r.range(0.15f, 0.45f);
        L.decor.cylinder(M4::translate(p + V3(0, h / 2, 0)) * M4::scale(V3(0.1f, h, 0.1f)), T_WAX, 5);
        L.decor.emis = 2; L.decor.color = hexColor(0xffc060);
        L.decor.cube(M4::translate(p + V3(0, h + 0.05f, 0)) * M4::scale(V3(0.05f, 0.1f, 0.05f)), T_WHITE);
        L.decor.emis = 0; L.decor.color = V3(1, 1, 1);
    }
    light(base + V3(0, 0.6f, 0), 3.5f, hexColor(0xffa040), 0.7f);
}
void LB::crystalCluster(V3 base, float s, V3 color) {
    Rng r(hash32((uint32_t)(base.x * 5 + base.z * 9 + 2)));
    L.decor.color = color; L.decor.emis = 0.6f;
    for (int i = 0; i < 4; i++) {
        V3 o(r.range(-0.6f, 0.6f) * s, 0, r.range(-0.6f, 0.6f) * s);
        float h = r.range(0.8f, 2.0f) * s;
        L.decor.octa(M4::translate(base + o + V3(0, h * 0.4f, 0)) * M4::rotZ(r.range(-0.4f, 0.4f)) * M4::rotX(r.range(-0.3f, 0.3f)) * M4::scale(V3(0.5f * s, h, 0.5f * s)), T_CRYSTAL);
    }
    L.decor.color = V3(1, 1, 1); L.decor.emis = 0;
    light(base + V3(0, 1.0f * s, 0), 6 * s, color, 0.7f);
}
void LB::banner(V3 top, float w, float h, float yaw, int tile) {
    M4 m = M4::translate(top) * M4::rotY(yaw);
    L.decor.cube(m * M4::translate(V3(0, 0, 0)) * M4::scale(V3(w + 0.3f, 0.1f, 0.1f)), T_DARKWOOD);
    L.decor.cube(m * M4::translate(V3(0, -h / 2, 0.02f)) * M4::scale(V3(w, h, 0.04f)), tile, 1);
}
void LB::arch(V3 base, float width, float height, float depth, int tile, bool alongX) {
    // two pillars + lintel
    V3 side = alongX ? V3(1, 0, 0) : V3(0, 0, 1);
    float pw = 0.8f;
    for (int s = -1; s <= 1; s += 2) {
        V3 c = base + side * (s * (width / 2 + pw / 2));
        AABB b(c + V3(alongX ? -pw / 2 : -depth / 2, 0, alongX ? -depth / 2 : -pw / 2), c + V3(alongX ? pw / 2 : depth / 2, height, alongX ? depth / 2 : pw / 2));
        solid(b, tile);
    }
    V3 lc = base + V3(0, height, 0);
    float half = width / 2 + pw;
    AABB lb(lc + V3(alongX ? -half : -depth / 2, 0, alongX ? -depth / 2 : -half), lc + V3(alongX ? half : depth / 2, 0.9f, alongX ? depth / 2 : half));
    solid(lb, tile);
}
void LB::rail(V3 a, V3 b, float h) {
    V3 d = b - a; float l = len(d);
    int n = std::max(1, (int)(l / 1.5f));
    for (int i = 0; i <= n; i++) {
        V3 p = a + d * (float(i) / n);
        L.decor.cube(M4::translate(p + V3(0, h / 2, 0)) * M4::scale(V3(0.12f, h, 0.12f)), T_DARKWOOD);
    }
    V3 m = (a + b) * 0.5f + V3(0, h, 0);
    float yaw = std::atan2(d.x, d.z);
    L.decor.cube(M4::translate(m) * M4::rotY(yaw) * M4::scale(V3(0.1f, 0.1f, l)), T_DARKWOOD);
}
void LB::bones(V3 base) {
    Rng r(hash32((uint32_t)(base.x * 3 + base.z * 5 + 11)));
    for (int i = 0; i < 5; i++) {
        V3 p = base + V3(r.range(-0.6f, 0.6f), 0.06f, r.range(-0.6f, 0.6f));
        L.decor.cube(M4::translate(p) * M4::rotY(r.range(0, TAU)) * M4::scale(V3(0.1f, 0.1f, r.range(0.4f, 0.8f))), T_BONE);
    }
    L.decor.sphere(M4::translate(base + V3(0.2f, 0.18f, 0.1f)) * M4::scale(V3(0.35f, 0.32f, 0.38f)), T_BONE, 6, 5);
}
void LB::post(V3 base, float h, float r, int tile) {
    L.decor.cylinder(M4::translate(base + V3(0, h / 2, 0)) * M4::scale(V3(2 * r, h, 2 * r)), tile, 6, true, h / 2);
}
void LB::mushroom(V3 base, float s) {
    L.decor.cylinder(M4::translate(base + V3(0, 0.5f * s, 0)) * M4::scale(V3(0.3f * s, 1.0f * s, 0.3f * s)), T_WAX, 6);
    L.decor.color = hexColor(0x7ae8c0); L.decor.emis = 0.9f;
    L.decor.sphere(M4::translate(base + V3(0, 1.0f * s, 0)) * M4::scale(V3(1.2f * s, 0.5f * s, 1.2f * s)), T_WHITE, 7, 4);
    L.decor.color = V3(1, 1, 1); L.decor.emis = 0;
    light(base + V3(0, 1.2f * s, 0), 3.5f * s + 1, hexColor(0x5ae0c0), 0.5f);
}
void LB::statue(V3 base, float s, float yaw) {
    M4 m = M4::translate(base) * M4::rotY(yaw) * M4::scale(V3(s, s, s));
    L.decor.cube(m * M4::translate(V3(0, 0.4f, 0)) * M4::scale(V3(1.6f, 0.8f, 1.6f)), T_MARBLE);
    L.decor.cylinder(m * M4::translate(V3(0, 1.6f, 0)) * M4::scale(V3(0.9f, 1.6f, 0.7f)), T_MARBLE, 8);
    L.decor.sphere(m * M4::translate(V3(0, 2.75f, 0)) * M4::scale(V3(0.6f, 0.7f, 0.6f)), T_MARBLE, 7, 5);
    L.decor.cube(m * M4::translate(V3(0.6f, 2.2f, 0.3f)) * M4::rotZ(-0.6f) * M4::scale(V3(0.25f, 1.2f, 0.25f)), T_MARBLE);
    L.decor.cube(m * M4::translate(V3(0.9f, 2.9f, 0.3f)) * M4::scale(V3(0.4f, 0.4f, 0.4f)), T_MARBLE);
    solid(AABB(base + V3(-0.8f * s, 0, -0.8f * s), base + V3(0.8f * s, 3.0f * s, 0.8f * s)), T_MARBLE, -1, V3(1, 1, 1), false);
}

// ================================================================== collision grid
void Game::gridBuild() {
    Grid& g = lv.grid;
    V3 lo(1e9f, 0, 1e9f), hi(-1e9f, 0, -1e9f);
    for (auto& s : lv.solids) { lo.x = std::min(lo.x, s.box.lo.x); lo.z = std::min(lo.z, s.box.lo.z); hi.x = std::max(hi.x, s.box.hi.x); hi.z = std::max(hi.z, s.box.hi.z); }
    if (lv.solids.empty()) { lo = V3(-1, 0, -1); hi = V3(1, 0, 1); }
    g.origin = lo - V3(1, 0, 1);
    g.nx = std::max(1, (int)std::ceil((hi.x - lo.x + 2) / g.cell));
    g.nz = std::max(1, (int)std::ceil((hi.z - lo.z + 2) / g.cell));
    g.cells.assign(g.nx * g.nz, {});
    for (int i = 0; i < (int)lv.solids.size(); i++) {
        const AABB& b = lv.solids[i].box;
        int x0 = std::max(0, (int)((b.lo.x - g.origin.x) / g.cell)), x1 = std::min(g.nx - 1, (int)((b.hi.x - g.origin.x) / g.cell));
        int z0 = std::max(0, (int)((b.lo.z - g.origin.z) / g.cell)), z1 = std::min(g.nz - 1, (int)((b.hi.z - g.origin.z) / g.cell));
        for (int z = z0; z <= z1; z++)
            for (int x = x0; x <= x1; x++) g.cells[z * g.nx + x].push_back(i);
    }
    g.stamp.assign(lv.solids.size(), 0);
    g.stampId = 0;
}

void Game::querySolids(const AABB& q, std::vector<AABB>& out, std::vector<int>* dynIds) {
    out.clear();
    if (dynIds) dynIds->clear();
    Grid& g = lv.grid;
    if (++g.stampId == 0x7fffffff) { std::fill(g.stamp.begin(), g.stamp.end(), 0); g.stampId = 1; }
    int x0 = std::max(0, (int)std::floor((q.lo.x - g.origin.x) / g.cell)), x1 = std::min(g.nx - 1, (int)std::floor((q.hi.x - g.origin.x) / g.cell));
    int z0 = std::max(0, (int)std::floor((q.lo.z - g.origin.z) / g.cell)), z1 = std::min(g.nz - 1, (int)std::floor((q.hi.z - g.origin.z) / g.cell));
    for (int z = z0; z <= z1; z++)
        for (int x = x0; x <= x1; x++)
            for (int i : g.cells[z * g.nx + x]) {
                if (g.stamp[i] == g.stampId) continue;
                g.stamp[i] = g.stampId;
                if (lv.solids[i].box.overlaps(q)) { out.push_back(lv.solids[i].box); if (dynIds) dynIds->push_back(-1); }
            }
    for (int i = 0; i < (int)lv.dyn.size(); i++) {
        auto& d = lv.dyn[i];
        if (!d.collides()) continue;
        AABB b = d.box();
        if (b.overlaps(q)) { out.push_back(b); if (dynIds) dynIds->push_back(i); }
    }
    if (lv.id == LV_HOME && vox.active) {
        size_t n0 = out.size();
        vox.query(q, out);
        if (dynIds) for (size_t k = n0; k < out.size(); k++) dynIds->push_back(-1);
    }
}

bool Game::boxFree(const AABB& q) {
    std::vector<AABB> v;
    querySolids(q, v);
    return v.empty();
}

static AABB actorBox(const V3& p, float r, float h) { return AABB(V3(p.x - r, p.y, p.z - r), V3(p.x + r, p.y + h, p.z + r)); }

bool Game::moveBox(V3& pos, V3& vel, float r, float h, float dt, bool& grounded, int* groundDyn, bool stepUp) {
    bool wasGrounded = grounded;
    grounded = false;
    if (groundDyn) *groundDyn = -1;
    V3 d = vel * dt;
    AABB sweep = actorBox(pos, r, h);
    sweep.lo = V3(std::min(sweep.lo.x, sweep.lo.x + d.x) - 0.6f, std::min(sweep.lo.y, sweep.lo.y + d.y) - 0.6f, std::min(sweep.lo.z, sweep.lo.z + d.z) - 0.6f);
    sweep.hi = V3(std::max(sweep.hi.x, sweep.hi.x + d.x) + 0.6f, std::max(sweep.hi.y, sweep.hi.y + d.y) + 0.8f, std::max(sweep.hi.z, sweep.hi.z + d.z) + 0.6f);
    static std::vector<AABB> boxes; static std::vector<int> ids;
    querySolids(sweep, boxes, &ids);
    const float EPS = 0.001f;
    // if we start inside something (teleport, moving gate...), push out the shortest way
    // instead of letting the axis sweeps shove us through to the far side
    for (int iter = 0; iter < 3; iter++) {
        AABB a = actorBox(pos, r, h);
        bool any = false;
        for (auto& b : boxes) {
            if (!a.overlaps(b)) continue;
            float px1 = b.hi.x - a.lo.x, px2 = a.hi.x - b.lo.x, pz1 = b.hi.z - a.lo.z, pz2 = a.hi.z - b.lo.z, py = b.hi.y - a.lo.y;
            float m = std::min(std::min(px1, px2), std::min(pz1, pz2));
            if (py < m && py < 0.6f) pos.y += py + EPS;
            else if (m == px1) pos.x += px1 + EPS; else if (m == px2) pos.x -= px2 + EPS;
            else if (m == pz1) pos.z += pz1 + EPS; else pos.z -= pz2 + EPS;
            any = true;
            a = actorBox(pos, r, h);
        }
        if (!any) break;
    }
    bool hitWall = false;
    auto freeAt = [&](const V3& p) {
        AABB a = actorBox(p, r, h);
        for (auto& b : boxes) if (a.overlaps(b)) return false;
        return true;
    };
    // horizontal axes
    for (int axis = 0; axis < 3; axis += 2) {
        float delta = axis == 0 ? d.x : d.z;
        if (delta == 0) continue;
        pos[axis] += delta;
        AABB a = actorBox(pos, r, h);
        for (auto& b : boxes) {
            if (!a.overlaps(b)) continue;
            // try stepping up onto low obstacles
            float rise = b.hi.y - pos.y;
            if (stepUp && rise > 0 && (wasGrounded ? rise <= 0.55f : (vel.y <= 3.f && rise <= 0.8f))) {  // in the air: mantle onto a ledge you just barely missed
                V3 up = pos; up.y = b.hi.y + EPS;
                if (freeAt(up)) { pos = up; a = actorBox(pos, r, h); continue; }
            }
            if (delta > 0) pos[axis] = b.lo[axis] - r - EPS;
            else pos[axis] = b.hi[axis] + r + EPS;
            vel[axis] = 0;
            hitWall = true;
            a = actorBox(pos, r, h);
        }
    }
    // vertical
    if (d.y != 0) {
        pos.y += d.y;
        AABB a = actorBox(pos, r, h);
        for (size_t i = 0; i < boxes.size(); i++) {
            const AABB& b = boxes[i];
            if (!a.overlaps(b)) continue;
            if (d.y < 0) {
                pos.y = b.hi.y + EPS;
                grounded = true;
                if (groundDyn) *groundDyn = ids[i];
            } else {
                pos.y = b.lo.y - h - EPS;
            }
            vel.y = 0;
            a = actorBox(pos, r, h);
        }
    }
    // snap down small steps when walking down stairs
    if (!grounded && wasGrounded && vel.y <= 0) {
        AABB probe = actorBox(pos - V3(0, 0.4f, 0), r, 0.4f + 0.01f);
        float best = -1e9f; int bestId = -1;
        for (size_t i = 0; i < boxes.size(); i++)
            if (probe.overlaps(boxes[i]) && boxes[i].hi.y <= pos.y + 0.01f && boxes[i].hi.y > best) { best = boxes[i].hi.y; bestId = ids[i]; }
        if (best > -1e8f) { pos.y = best + EPS; grounded = true; vel.y = 0; if (groundDyn) *groundDyn = bestId; }
    }
    return hitWall;
}

bool g_rayIgnoreBarriers = false;
float Game::raycast(const V3& o, const V3& d, float maxT, V3* nrm, int* dynHit, bool ignoreCrystals) {
    V3 e = o + d * maxT;
    AABB q(V3(std::min(o.x, e.x), std::min(o.y, e.y), std::min(o.z, e.z)) - V3(0.1f), V3(std::max(o.x, e.x), std::max(o.y, e.y), std::max(o.z, e.z)) + V3(0.1f));
    float best = -1; V3 bn;
    if (dynHit) *dynHit = -1;
    Grid& g = lv.grid;
    if (++g.stampId == 0x7fffffff) { std::fill(g.stamp.begin(), g.stamp.end(), 0); g.stampId = 1; }
    int x0 = std::max(0, (int)std::floor((q.lo.x - g.origin.x) / g.cell)), x1 = std::min(g.nx - 1, (int)std::floor((q.hi.x - g.origin.x) / g.cell));
    int z0 = std::max(0, (int)std::floor((q.lo.z - g.origin.z) / g.cell)), z1 = std::min(g.nz - 1, (int)std::floor((q.hi.z - g.origin.z) / g.cell));
    for (int z = z0; z <= z1; z++)
        for (int x = x0; x <= x1; x++)
            for (int i : g.cells[z * g.nx + x]) {
                if (g.stamp[i] == g.stampId) continue;
                g.stamp[i] = g.stampId;
                if (g_rayIgnoreBarriers && lv.solids[i].barrier) continue;
                V3 n;
                float t = rayAABB(o, d, lv.solids[i].box, maxT, &n);
                if (t >= 0 && (best < 0 || t < best)) { best = t; bn = n; if (dynHit) *dynHit = -1; }
            }
    for (int i = 0; i < (int)lv.dyn.size(); i++) {
        auto& dd = lv.dyn[i];
        if (!dd.collides()) continue;
        if (ignoreCrystals && dd.kind == DK_CRYSTAL) continue;
        V3 n;
        float t = rayAABB(o, d, dd.box(), maxT, &n);
        if (t >= 0 && (best < 0 || t < best)) { best = t; bn = n; if (dynHit) *dynHit = i; }
    }
    if (lv.id == LV_HOME && vox.active) {
        V3 vn;
        float t = vox.raycast(o, d, maxT, &vn, nullptr, nullptr, nullptr, true);
        if (t >= 0 && (best < 0 || t < best)) { best = t; bn = vn; if (dynHit) *dynHit = -1; }
    }
    if (nrm) *nrm = bn;
    return best;
}

float Game::groundHeightBelow(const V3& p, float maxDown) {
    float t = raycast(p, V3(0, -1, 0), maxDown);
    return t < 0 ? -1e9f : p.y - t;
}

// ================================================================== level build
void buildHub(Level& L, Game& g);
void buildWharf(Level& L, Game& g);
void buildClock(Level& L, Game& g);
void buildVaults(Level& L, Game& g);
void buildSpire(Level& L, Game& g);
void buildJunk(Level& L, Game& g);
void buildEndless(Level& L, Game& g);
void buildHome(Level& L, Game& g);
void buildCity(Level& L, Game& g);
void buildArena(Level& L, Game& g);
void buildRace(Level& L, Game& g);
void applyExtraFlags(Game& g);

void Game::buildLevel(LevelId id) {
    if (!headless) {
        lv.staticMesh.destroy(); lv.waterMesh.destroy();
        for (auto& d : lv.dyn) d.mesh.destroy();
    }
    lv = Level();
    lv.id = id;
    vox.active = false;
    enemyHpMul = 1;
    switch (id) {
        case LV_HUB: buildHub(lv, *this); break;
        case LV_WHARF: buildWharf(lv, *this); break;
        case LV_CLOCK: buildClock(lv, *this); break;
        case LV_VAULTS: buildVaults(lv, *this); break;
        case LV_SPIRE: buildSpire(lv, *this); break;
        case LV_JUNK: buildJunk(lv, *this); break;
        case LV_ENDLESS: buildEndless(lv, *this); break;
        case LV_HOME: buildHome(lv, *this); break;
        case LV_CITY: buildCity(lv, *this); break;
        case LV_ARENA: buildArena(lv, *this); break;
        case LV_RACE: buildRace(lv, *this); break;
        default: break;
    }
    gridBuild();
    if (!headless) {
        MeshBuilder mb;
        for (auto& s : lv.solids) {
            if (!s.visible) continue;
            mb.color = s.color;
            mb.box(s.box, s.tile, s.uvScale, s.topTile);
        }
        mb.color = V3(1, 1, 1);
        mb.append(lv.decor);
        lv.staticMesh.upload(mb.v);
        for (auto& d : lv.dyn) {
            MeshBuilder db; db.color = d.color;
            db.box(d.base, d.tile, d.kind == DK_SEAL ? 0.5f : 1.f, d.topTile);
            d.mesh.upload(db.v);
        }
        if (lv.waterY > -999) {
            MeshBuilder wb;
            AABB a = lv.waterArea;
            wb.quad(V3(a.lo.x, lv.waterY, a.hi.z), V3(a.hi.x, lv.waterY, a.hi.z), V3(a.hi.x, lv.waterY, a.lo.z), V3(a.lo.x, lv.waterY, a.lo.z), V3(0, 1, 0), 1, 1, 0);
            lv.waterMesh.upload(wb.v);
        }
    }
    lv.decor.v.clear();
    lv.decor.v.shrink_to_fit();
}

void Game::applyFlagsToLevel() {
    // a defeated boss whose Beacon-Heart was never picked up leaves the heart waiting in its arena
    if (lv.boss.type != B_NONE && lv.boss.type != B_HUSH && has(lv.boss.flag)) {
        std::string hf = "heart." + std::to_string((int)lv.boss.type);
        if (!has(hf)) {
            Pickup p; p.type = PK_HEART; p.index = lv.boss.type; p.pos = lv.boss.center + (lv.boss.type == B_RATKING ? V3(0, 1.4f, -4.5f) : V3(0, 1.4f, 0)); p.flag = hf;
            lv.pickups.push_back(p);
        }
    }
    for (auto& d : lv.dyn) {
        if (d.flag.empty()) continue;
        if (has(d.flag)) {
            if (d.kind == DK_GATE) { d.open = true; d.openAmt = 1; d.offset = V3(0, -(d.base.hi.y - d.base.lo.y) - 0.05f, 0); }
            else d.broken = true;
        }
    }
    for (auto& p : lv.pickups) if (!p.flag.empty() && has(p.flag)) p.taken = true;
    for (auto& a : lv.arenas) if (!a.flag.empty() && has(a.flag)) a.done = true;
    for (int i = 0; i < (int)lv.lanterns.size(); i++)
        if (has("lantern." + std::to_string(lv.id) + "." + std::to_string(i))) lv.lanterns[i].lit = true;
    // braziers whose gate is already open stay lit (non-timed)
    for (auto& b : lv.braziers) {
        if (b.gateGroup < 0 || b.bossBrazier) continue;
        for (auto& gl : lv.gateLinks)
            if (gl.group == b.gateGroup && !gl.flag.empty() && has(gl.flag)) { b.lit = true; b.timed = 0; }
    }
    applyExtraFlags(*this);
}

// ================================================================== wards
// Closed gates, intact wax seals and crystals project a tall "ward" so the flame can't be thrown over
// them (or their side walls) and warped through. The ward is the barrier's thin slab extended upward
// and sideways along its long axis.
bool Game::wardBox(const DynSolid& d, AABB& out) const {
    if (d.kind == DK_MOVER) return false;
    if (d.kind == DK_GATE && d.open) return false;
    if ((d.kind == DK_SEAL || d.kind == DK_CRYSTAL) && d.broken) return false;
    AABB b = d.base;
    V3 sz = b.size();
    const float side = 9.f, up = 40.f;
    if (sz.x >= sz.z) { b.lo.x -= side; b.hi.x += side; b.lo.z -= 0.15f; b.hi.z += 0.15f; }
    else { b.lo.z -= side; b.hi.z += side; b.lo.x -= 0.15f; b.hi.x += 0.15f; }
    b.hi.y += up;
    out = b;
    return true;
}

bool Game::wardBlocks(const V3& a, const V3& b) const {
    V3 d = b - a;
    float l = len(d);
    if (l < 1e-4f) return false;
    for (auto& ds : lv.dyn) {
        AABB w;
        if (!wardBox(ds, w)) continue;
        if (w.contains(a) || w.contains(b)) return true;
        if (rayAABB(a, d / l, w, l) >= 0) return true;
    }
    return false;
}
