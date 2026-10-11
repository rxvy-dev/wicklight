// WICKLIGHT - level construction helpers (used by levels.cpp)
#pragma once
#include "game.h"

struct LB {
    Level& L;
    explicit LB(Level& l) : L(l) {}
    // collision + visuals
    int solid(const AABB& b, int tile, int top = -1, V3 col = V3(1, 1, 1), bool visible = true, float uv = 1.f);
    void slab(float x0, float z0, float x1, float z1, float y, int top, int side, float depth = 3.f, V3 col = V3(1, 1, 1));
    void wall(float x0, float z0, float x1, float z1, float y0, float y1, int tile, V3 col = V3(1, 1, 1));
    void invisibleWall(float x0, float z0, float x1, float z1, float y0, float y1);
    int dynBox(DynKind k, const AABB& b, int tile, int top = -1, V3 col = V3(1, 1, 1));
    int mover(const AABB& b, V3 a, V3 bb, float period, float phase, int tile, int top = -1);
    int gate(const AABB& b, int group, const std::string& flag, int tile = T_GRATE);
    int seal(const AABB& b, const std::string& flag);
    int crystal(const AABB& b, const std::string& flag);
    void brazier(V3 base, int group, bool sealed = false, float timed = 0, bool boss = false);
    void lantern(V3 base, int spawn);
    void pickup(PickupType t, V3 pos, int index, const std::string& flag);
    void exit(const AABB& zone, LevelId target, int spawn, const std::string& need, const std::string& lockedMsg, const std::string& label);
    int spawn(V3 pos, float yaw);
    void anchor(V3 pos, bool post = false);
    void hint(const AABB& zone, const std::string& text, const std::string& need = "", const std::string& hide = "");
    void light(V3 pos, float radius, V3 color, float intensity);
    void enemy(EnemyType t, V3 pos, int arena = -1);
    int arena(const AABB& zone, std::vector<int> gates, const std::string& flag);
    void talker(V3 pos, float yaw, int who);
    void emitter(const AABB& zone, int kind, float rate);
    // geometry/decor
    void pillar(float x, float z, float y0, float y1, float r, int tile, bool collide = true, V3 col = V3(1, 1, 1));
    void stairs(V3 start, V3 dir, float width, int steps, float rise, float run, int tile, int top = -1);
    void lampPost(V3 base, bool lit, V3 col = hexColor(0xffb060));
    void tree(V3 base, float s, int leafTile = T_LEAVES, bool collide = true);
    void pine(V3 base, float s);
    void house(float x0, float z0, float x1, float z1, float y, float h, int wallTile, bool collide = true);
    void crate(V3 base, float s, bool collide = true);
    void barrel(V3 base, bool collide = true);
    void rock(V3 c, float s, int tile = T_ROCK);
    void spinner(V3 c, float r, float thick, int axis, float speed, int tile = T_BRASS, V3 col = V3(1, 1, 1));
    void chain(V3 top, float length);
    void tomb(V3 base, float yaw);
    void candles(V3 base, int n = 4);
    void crystalCluster(V3 base, float s, V3 color);
    void banner(V3 top, float w, float h, float yaw, int tile = T_CLOTH);
    void arch(V3 base, float width, float height, float depth, int tile, bool alongX);
    void rail(V3 a, V3 b, float h);
    void bones(V3 base);
    void post(V3 base, float h, float r = 0.15f, int tile = T_DARKWOOD);
    void mushroom(V3 base, float s);
    void statue(V3 base, float s, float yaw);
    void urns(std::initializer_list<V3> pts, int kind = 0) { for (auto& p : pts) { Level::Breakable b; b.pos = p; b.kind = kind; L.breakables.push_back(b); } }
};
