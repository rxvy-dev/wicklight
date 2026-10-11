// WICKLIGHT - renderer: low-res pixel render target, lit textured geometry,
// shadow map, billboard particles, dithered post-process and pixel-font UI.
#pragma once
#include "platform.h"
#include "mathx.h"
#include <vector>
#include <string>

enum Tile {
    T_WHITE = 0, T_STONEBRICK, T_COBBLE, T_PLANK, T_DARKWOOD, T_METAL, T_RUST, T_MOSS, T_GRASS, T_DIRT,
    T_SAND, T_LEAVES, T_BARK, T_WAX, T_ROOF, T_BRASS, T_BONE, T_CLOTH, T_CRYSTAL, T_TILEFLOOR,
    T_CARPET, T_ASH, T_RUNE, T_TAR, T_WINDOW, T_ROCK, T_MARBLE, T_OBSIDIAN, T_BARNACLE, T_CLOCKFACE,
    T_BOOKS, T_ROPE, T_SHADOW, T_LAVA, T_GRATE, T_IVY, T_PLASTER, T_GOLD, T_SEAL, T_EYE,
    T_DOOR, T_PIPE, T_FLESHGLOW, T_STAR,
    // Wick's Island blocks
    T_STONE, T_GRASS_SIDE, T_LOG_TOP, T_GLASS, T_ORE_EMBER, T_ORE_CRYSTAL, T_ORE_SCRAP, T_LAMPBLOCK, T_CHEST, T_BENCH_TOP,
    T_BENCH_SIDE, T_FURNACE, T_FORGE, T_LADDER, T_JUMPPAD, T_BOMB, T_SAPLING, T_FLOWER_RED, T_FLOWER_YELLOW, T_GEAR_ICON,
    T_WAXLUMP_ICON, T_FLAME_ICON, T_CRYSTAL_ICON, T_BED_ICON, T_DOOR_ICON, T_CANDLE_ICON, T_CRACK,
    T_COUNT
};

struct Vertex {
    float px, py, pz, nx, ny, nz, u, v, tile, r, g, b, emis;
};

struct Mesh {
    GLuint vao = 0, vbo = 0;
    int count = 0;
    void upload(const std::vector<Vertex>& verts);
    void draw() const;
    void destroy();
};

// Geometry builder. UVs are in "tile units": a value of 1.0 spans one 16px texture tile.
struct MeshBuilder {
    std::vector<Vertex> v;
    V3 color = V3(1, 1, 1);
    float emis = 0;
    void tri(const V3& a, const V3& b, const V3& c, const V3& n, V2 ua, V2 ub, V2 uc, int tile);
    void quad(const V3& a, const V3& b, const V3& c, const V3& d, const V3& n, float uw, float vh, int tile);
    // axis aligned box, uv = world units * uvScale
    void box(const AABB& b, int tile, float uvScale = 1.f, int topTile = -1, bool bottom = true);
    // transformed unit cube (-0.5..0.5) with per-face 0..1 uv
    void cube(const M4& m, int tile, float uvRepeat = 1.f);
    void cylinder(const M4& m, int tile, int segs = 10, bool caps = true, float uvRepeat = 1.f);
    void cone(const M4& m, int tile, int segs = 10);
    void sphere(const M4& m, int tile, int segs = 8, int rings = 6);
    void octa(const M4& m, int tile);
    void append(const MeshBuilder& o);
};

struct Light { V3 pos; float radius; V3 color; float intensity; };

struct Particle {
    V3 pos, vel;
    V3 color; float alpha;
    float size, life, maxLife;
    float gravity = 0, drag = 0;
    int kind = 0;  // 0 square pixel (alpha), 1 glow (additive), 2 smoke (alpha soft), 3 spark (additive square)
    float grow = 0;
};

struct Environment {
    V3 skyTop = hexColor(0x0b0f24), skyHorizon = hexColor(0x2b2448);
    V3 ambSky = V3(0.18f, 0.2f, 0.3f), ambGround = V3(0.08f, 0.07f, 0.1f);
    V3 sunDir = norm(V3(-0.4f, -1.f, -0.3f)), sunColor = V3(0.25f, 0.28f, 0.42f);
    V3 fogColor = hexColor(0x1d1a33);
    float fogDensity = 0.03f, fogStart = 8.f;
    float stars = 1.f;
    V3 moonDir = norm(V3(0.4f, 0.5f, 0.8f));
    V3 moonColor = V3(0.9f, 0.9f, 1.f);
    float moonSize = 0.06f;
};

enum PartMesh { PM_CUBE = 0, PM_CYL, PM_SPHERE, PM_CONE, PM_OCTA, PM_COUNT };

struct UIVert { float x, y, u, v, r, g, b, a; };

struct Renderer {
    int winW = 1280, winH = 720;
    int lowW = 480, lowH = 270;
    int pixelLines = 270;      // vertical resolution of the pixel buffer
    int shadowQuality = 2;     // 0 off, 1 low (1024), 2 high (2048)
    bool outlines = true;
    float fov = 70.f;
    float time = 0;

    GLuint progWorld = 0, progDepth = 0, progParticle = 0, progPost = 0, progUI = 0, progSky = 0;
    GLuint atlasTex = 0, fontTex = 0;
    GLuint lowFBO = 0, lowColor = 0, lowDepth = 0;
    GLuint shadowFBO = 0, shadowTex = 0; int shadowSize = 0;
    GLuint emptyVAO = 0;
    Mesh parts[PM_COUNT];
    GLuint dynVAO = 0, dynVBO = 0;  // particles
    GLuint uiVAO = 0, uiVBO = 0;

    // frame state
    M4 view, proj, viewProj, lightVP;
    V3 camPos, camFwd, camRight, camUp;
    Environment env;
    std::vector<Light> lights;
    bool shadowPass = false;
    GLuint curProg = 0;
    float overlayA = 0; V3 overlayCol;
    float fade = 0;
    float colorLevels = 24.f;
    int uiScaleOverride = 0;  // 0 = auto

    std::vector<Particle> particles;
    std::vector<UIVert> ui;

    bool init(std::string& err);
    void resize(int w, int h);
    void setPixelLines(int lines);
    void setShadowQuality(int q);
    void shutdown();

    void setCamera(const V3& eye, const V3& target, float fovDeg);
    // Scene rendering: call beginShadow/endShadow around geometry submission, then beginScene ... endScene
    void beginShadow(const V3& focus);
    void endShadow();
    void beginScene();
    void drawMesh(const Mesh& m, const M4& model, const V3& tint = V3(1, 1, 1), float emis = 0, int tileOverride = -1,
                  float flash = 0, float alpha = 1);
    void drawPart(PartMesh pm, const M4& model, int tile, const V3& tint, float emis = 0, float flash = 0, float alpha = 1);
    void drawWater(const Mesh& m, const V3& color, float alpha);
    void drawParticles();
    void endScene();  // post-process to screen

    void updateParticles(float dt);
    void spawn(const Particle& p) { if (particles.size() < 4000) particles.push_back(p); }

    // UI (screen pixels, origin top-left)
    int uiScale() const;
    void uiRect(float x, float y, float w, float h, const V4& col);
    void uiText(float x, float y, const std::string& s, float scale, const V4& col, bool shadow = true);
    float uiTextWidth(const std::string& s, float scale) const;
    void uiTextCentered(float cx, float y, const std::string& s, float scale, const V4& col, bool shadow = true);
    void uiFrame(float x, float y, float w, float h, const V4& fill, const V4& border);
    void uiIcon(float x, float y, float size, int tile, const V4& tint);  // a 16x16 atlas tile
    void uiFlush();
    bool project(const V3& world, float& sx, float& sy) const;  // to UI pixels

    bool screenshot(const std::string& path);

   private:
    void useProgram(GLuint p);
    void setWorldUniforms(GLuint p);
};

std::vector<uint8_t> generateAtlas();  // 256x256 RGBA
