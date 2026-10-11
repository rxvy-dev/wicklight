#include "render.h"
#include "font.h"
#include <cstdio>
#include <cstring>
#include <unordered_map>

// ---------------------------------------------------------------- shaders
static const char* WORLD_VS = R"(#version 330 core
layout(location=0) in vec3 aPos; layout(location=1) in vec3 aNrm; layout(location=2) in vec2 aUV;
layout(location=3) in float aTile; layout(location=4) in vec3 aCol; layout(location=5) in float aEmis;
uniform mat4 uVP; uniform mat4 uModel; uniform mat4 uLightVP; uniform float uTileOverride;
out vec3 vPos; out vec3 vNrm; out vec2 vUV; flat out float vTile; out vec3 vCol; out float vEmis; out vec4 vShadow;
void main(){
  vec4 wp = uModel * vec4(aPos,1.0);
  vPos = wp.xyz; vNrm = mat3(uModel) * aNrm; vUV = aUV;
  vTile = uTileOverride >= 0.0 ? uTileOverride : aTile;
  vCol = aCol; vEmis = aEmis; vShadow = uLightVP * wp;
  gl_Position = uVP * wp;
})";

static const char* WORLD_FS = R"(#version 330 core
in vec3 vPos; in vec3 vNrm; in vec2 vUV; flat in float vTile; in vec3 vCol; in float vEmis; in vec4 vShadow;
uniform sampler2D uAtlas; uniform sampler2D uShadow;
uniform vec3 uTint; uniform float uEmis; uniform float uFlash; uniform float uAlpha;
uniform vec3 uAmbSky, uAmbGround, uSunDir, uSunCol, uFogCol, uCamPos;
uniform float uFogDensity, uFogStart, uTime;
uniform int uNumLights; uniform vec4 uLP[16]; uniform vec4 uLC[16];
uniform int uShadowOn; uniform int uMode; uniform float uShadowTexel;
out vec4 oCol;
float bayer4(vec2 p){ ivec2 i = ivec2(mod(p,4.0)); int idx = i.x + i.y*4;
  float m[16] = float[16](0.,8.,2.,10.,12.,4.,14.,6.,3.,11.,1.,9.,15.,7.,13.,5.); return (m[idx]+0.5)/16.0; }
void main(){
  float dith = bayer4(gl_FragCoord.xy);
  if (uAlpha < 0.999 && dith > uAlpha) discard;
  vec3 n = normalize(vNrm);
  vec3 base; float pixEmis = 0.0; float alpha = 1.0;
  if (uMode == 1) {
    // stylised pixel water
    vec2 q = floor(vPos.xz * 4.0) / 4.0;
    float w = sin(q.x*1.3 + uTime*1.4) + sin(q.y*1.7 - uTime*1.1) + sin((q.x+q.y)*0.6 + uTime*0.7);
    float band = floor((w*0.5+1.5) * 2.0) / 6.0;
    base = uTint * (0.55 + band*0.6);
    n = vec3(0.0,1.0,0.0);
    alpha = 0.86;
  } else {
    vec2 f = clamp(fract(vUV), 0.5/16.0, 15.5/16.0);
    vec2 tileXY = vec2(mod(vTile,16.0), floor(vTile/16.0));
    vec4 tex = texture(uAtlas, (tileXY + f) / 16.0);
    if (tex.a < 0.3) discard;
    if (tex.a < 0.9) pixEmis = 1.0;
    base = tex.rgb * vCol * uTint;
  }
  if (!gl_FrontFacing) n = -n;
  vec3 light = mix(uAmbGround, uAmbSky, n.y*0.5+0.5);
  float ndl = max(dot(n, -uSunDir), 0.0);
  float sh = 1.0;
  if (uShadowOn == 1 && ndl > 0.0) {
    vec3 sp = vShadow.xyz / vShadow.w * 0.5 + 0.5;
    if (sp.x > 0.0 && sp.x < 1.0 && sp.y > 0.0 && sp.y < 1.0 && sp.z < 1.0) {
      float bias = 0.0012 + 0.002*(1.0-ndl);
      float s = 0.0;
      s += (sp.z - bias > texture(uShadow, sp.xy).r) ? 0.0 : 1.0;
      s += (sp.z - bias > texture(uShadow, sp.xy + vec2(uShadowTexel,0.0)).r) ? 0.0 : 1.0;
      s += (sp.z - bias > texture(uShadow, sp.xy + vec2(0.0,uShadowTexel)).r) ? 0.0 : 1.0;
      s += (sp.z - bias > texture(uShadow, sp.xy + vec2(uShadowTexel,uShadowTexel)).r) ? 0.0 : 1.0;
      sh = s * 0.25;
    }
  }
  light += uSunCol * ndl * sh;
  vec3 pl = vec3(0.0);
  for (int i = 0; i < uNumLights; i++) {
    vec3 L = uLP[i].xyz - vPos; float d = length(L);
    float att = clamp(1.0 - d / uLP[i].w, 0.0, 1.0); att *= att;
    float nd = max(dot(n, L / max(d, 0.001)), 0.0) * 0.7 + 0.3;
    pl += uLC[i].rgb * uLC[i].w * att * nd;
  }
  light += pl;
  // dithered light banding: the signature pixel-shaded look
  light = floor(light * 7.0 + dith) / 7.0;
  float e = vEmis + uEmis + pixEmis;
  vec3 col = base * light + base * e * 1.25;
  if (uMode == 1) {
    // light reflections on water
    col += pl * 0.35 * (0.6 + 0.4*sin(vPos.x*5.0 + uTime*3.0)*sin(vPos.z*4.0 - uTime*2.0));
  }
  float dist = length(vPos - uCamPos);
  float fog = 1.0 - exp(-max(dist - uFogStart, 0.0) * uFogDensity);
  fog *= 1.0 - clamp(e, 0.0, 1.0) * 0.6;
  col = mix(col, uFogCol, clamp(fog, 0.0, 1.0));
  col = mix(col, vec3(1.0, 0.95, 0.85), uFlash);
  oCol = vec4(col, alpha);
})";

static const char* DEPTH_VS = R"(#version 330 core
layout(location=0) in vec3 aPos;
uniform mat4 uVP; uniform mat4 uModel;
void main(){ gl_Position = uVP * uModel * vec4(aPos,1.0); })";
static const char* DEPTH_FS = R"(#version 330 core
void main(){ })";

static const char* PART_VS = R"(#version 330 core
layout(location=0) in vec3 aPos; layout(location=1) in vec4 aCol; layout(location=2) in vec3 aUV;
uniform mat4 uVP; out vec4 vCol; out vec3 vUV; out vec3 vPos;
void main(){ vCol = aCol; vUV = aUV; vPos = aPos; gl_Position = uVP * vec4(aPos,1.0); })";
static const char* PART_FS = R"(#version 330 core
in vec4 vCol; in vec3 vUV; in vec3 vPos; out vec4 oCol;
uniform vec3 uFogCol; uniform vec3 uCamPos; uniform float uFogDensity; uniform float uFogStart;
float bayer4(vec2 p){ ivec2 i = ivec2(mod(p,4.0)); int idx = i.x + i.y*4;
  float m[16] = float[16](0.,8.,2.,10.,12.,4.,14.,6.,3.,11.,1.,9.,15.,7.,13.,5.); return (m[idx]+0.5)/16.0; }
void main(){
  int kind = int(vUV.z + 0.5);
  vec2 uv = vUV.xy * 2.0 - 1.0;
  float r = length(uv);
  float dist = length(vPos - uCamPos);
  float fog = clamp(1.0 - exp(-max(dist - uFogStart, 0.0) * uFogDensity), 0.0, 1.0);
  if (kind == 1) { // glow, additive, banded
    float a = clamp(1.0 - r, 0.0, 1.0); a = a*a;
    a = floor(a * 5.0 + bayer4(gl_FragCoord.xy)*0.9) / 5.0;
    oCol = vec4(vCol.rgb * a * vCol.a * (1.0 - fog*0.7), 1.0);
  } else if (kind == 3) { // spark square additive
    oCol = vec4(vCol.rgb * vCol.a * (1.0 - fog*0.5), 1.0);
  } else if (kind == 2) { // smoke, dithered alpha
    float a = clamp(1.0 - r, 0.0, 1.0) * vCol.a;
    if (bayer4(gl_FragCoord.xy) > a) discard;
    oCol = vec4(mix(vCol.rgb, uFogCol, fog), 1.0);
  } else { // pixel square
    if (bayer4(gl_FragCoord.xy) > vCol.a) discard;
    oCol = vec4(mix(vCol.rgb, uFogCol, fog), 1.0);
  }
})";

static const char* SKY_VS = R"(#version 330 core
out vec2 vNdc;
void main(){ vec2 p = vec2((gl_VertexID==1)?3.0:-1.0, (gl_VertexID==2)?3.0:-1.0); vNdc = p; gl_Position = vec4(p, 0.9999, 1.0); })";
static const char* SKY_FS = R"(#version 330 core
in vec2 vNdc; out vec4 oCol;
uniform mat4 uInvVP; uniform vec3 uTop, uHorizon, uMoonDir, uMoonCol; uniform float uStars, uMoonSize, uTime;
float h(vec3 p){ return fract(sin(dot(p, vec3(12.9898,78.233,37.719))) * 43758.5453); }
float bayer4(vec2 p){ ivec2 i = ivec2(mod(p,4.0)); int idx = i.x + i.y*4;
  float m[16] = float[16](0.,8.,2.,10.,12.,4.,14.,6.,3.,11.,1.,9.,15.,7.,13.,5.); return (m[idx]+0.5)/16.0; }
void main(){
  vec4 a = uInvVP * vec4(vNdc, -1.0, 1.0); vec4 b = uInvVP * vec4(vNdc, 1.0, 1.0);
  vec3 d = normalize(b.xyz/b.w - a.xyz/a.w);
  float t = clamp(d.y * 1.4 + 0.15, 0.0, 1.0);
  t = floor(t * 10.0 + bayer4(gl_FragCoord.xy)) / 10.0;
  vec3 col = mix(uHorizon, uTop, t);
  if (d.y > 0.0) {
    vec3 cell = floor(d * 140.0);
    float s = h(cell);
    if (s > 0.996) col += vec3(0.8,0.8,1.0) * uStars * (0.5 + 0.5*sin(uTime*2.0 + s*100.0)) * clamp(d.y*3.0,0.0,1.0);
  }
  float md = dot(d, normalize(uMoonDir));
  if (uMoonSize > 0.0) {
    float ang = acos(clamp(md, -1.0, 1.0));
    if (ang < uMoonSize) col = mix(col, uMoonCol, 0.95);
    else if (ang < uMoonSize * 3.5) col += uMoonCol * 0.12 * floor((1.0 - (ang - uMoonSize)/(uMoonSize*2.5)) * 4.0) / 4.0;
  }
  oCol = vec4(col, 1.0);
})";

static const char* POST_VS = R"(#version 330 core
out vec2 vUV;
void main(){ vec2 p = vec2((gl_VertexID==1)?3.0:-1.0, (gl_VertexID==2)?3.0:-1.0); vUV = p*0.5+0.5; gl_Position = vec4(p,0.0,1.0); })";
static const char* POST_FS = R"(#version 330 core
in vec2 vUV; out vec4 oCol;
uniform sampler2D uScene; uniform sampler2D uDepth; uniform vec2 uRes; uniform float uFade; uniform vec4 uOverlay;
uniform float uLevels; uniform int uOutline; uniform float uNear; uniform float uFar; uniform float uGlitch; uniform float uTime;
float gh(float a, float b){ return fract(sin(a * 12.9898 + b * 78.233) * 43758.5453); }
float bayer4(vec2 p){ ivec2 i = ivec2(mod(p,4.0)); int idx = i.x + i.y*4;
  float m[16] = float[16](0.,8.,2.,10.,12.,4.,14.,6.,3.,11.,1.,9.,15.,7.,13.,5.); return (m[idx]+0.5)/16.0; }
float lin(float d){ float z = d*2.0-1.0; return 2.0*uNear*uFar/(uFar+uNear - z*(uFar-uNear)); }
void main(){
  ivec2 p = ivec2(clamp(vUV * uRes, vec2(0.0), uRes - 1.0));
  vec3 c = texelFetch(uScene, p, 0).rgb;
  if (uGlitch > 0.001) {
    // glitch: torn horizontal bands, RGB split, green data noise
    float tq = floor(uTime * 18.0);
    float band = floor(float(p.y) / 5.0);
    float r = gh(band, tq);
    int sh = 0;
    if (r > 1.0 - uGlitch * 0.6) sh = int((gh(band, tq + 7.0) - 0.5) * 60.0 * uGlitch);
    int k = int(1.0 + 4.0 * uGlitch);
    ivec2 q = ivec2(clamp(p.x + sh, 0, int(uRes.x) - 1), p.y);
    c.r = texelFetch(uScene, ivec2(clamp(q.x + k, 0, int(uRes.x) - 1), q.y), 0).r;
    c.g = texelFetch(uScene, q, 0).g;
    c.b = texelFetch(uScene, ivec2(clamp(q.x - k, 0, int(uRes.x) - 1), q.y), 0).b;
    if (gh(float(p.x / 3) + band * 17.0, tq) > 1.0 - uGlitch * 0.05) c = vec3(0.3, 1.0, 0.55);
  }
  if (uOutline == 1) {
    float draw = texelFetch(uDepth, p, 0).r;
    if (draw < 0.9999) {
      float d0 = lin(draw);
      float dm = min(min(lin(texelFetch(uDepth, p+ivec2(1,0), 0).r), lin(texelFetch(uDepth, p-ivec2(1,0), 0).r)),
                     min(lin(texelFetch(uDepth, p+ivec2(0,1), 0).r), lin(texelFetch(uDepth, p-ivec2(0,1), 0).r)));
      if (d0 - dm > 0.08 * d0 + 0.25) c *= 0.35;
    }
  }
  vec2 v = vUV - 0.5; c *= 1.0 - dot(v, v) * 0.9;
  vec2 sp = vec2(p);
  c = floor(c * uLevels + bayer4(sp)) / uLevels;
  c = mix(c, uOverlay.rgb, uOverlay.a);
  c *= 1.0 - uFade;
  oCol = vec4(c, 1.0);
})";

static const char* UI_VS = R"(#version 330 core
layout(location=0) in vec2 aPos; layout(location=1) in vec2 aUV; layout(location=2) in vec4 aCol;
uniform vec2 uScreen; out vec2 vUV; out vec4 vCol;
void main(){ vUV = aUV; vCol = aCol; gl_Position = vec4(aPos.x/uScreen.x*2.0-1.0, 1.0-aPos.y/uScreen.y*2.0, 0.0, 1.0); })";
static const char* UI_FS = R"(#version 330 core
in vec2 vUV; in vec4 vCol; out vec4 oCol; uniform sampler2D uFont; uniform sampler2D uAtlas;
void main(){
  if (vUV.x >= 2.0) { vec4 t = texture(uAtlas, vUV - vec2(2.0, 0.0)); if (t.a < 0.3) discard; oCol = vec4(t.rgb * vCol.rgb, vCol.a); return; }
  float a = texture(uFont, vUV).r; if (a < 0.5) discard; oCol = vCol; })";

// ---------------------------------------------------------------- uniform cache
static std::unordered_map<uint64_t, GLint> g_locCache;
static GLint loc(GLuint prog, const char* name) {
    uint64_t key = (uint64_t(prog) << 32) ^ uint64_t(std::hash<std::string>()(name) & 0xFFFFFFFF);
    auto it = g_locCache.find(key);
    if (it != g_locCache.end()) return it->second;
    GLint l = glGetUniformLocation(prog, name);
    g_locCache[key] = l;
    return l;
}

static GLuint compile(GLenum type, const char* src, std::string& err) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[4096]; glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        err += std::string("Shader compile error:\n") + log;
        return 0;
    }
    return s;
}
static GLuint link(const char* vs, const char* fs, std::string& err) {
    GLuint v = compile(GL_VERTEX_SHADER, vs, err), f = compile(GL_FRAGMENT_SHADER, fs, err);
    if (!v || !f) return 0;
    GLuint p = glCreateProgram();
    glAttachShader(p, v); glAttachShader(p, f);
    glLinkProgram(p);
    GLint ok = 0; glGetProgramiv(p, GL_LINK_STATUS, &ok);
    glDeleteShader(v); glDeleteShader(f);
    if (!ok) { char log[4096]; glGetProgramInfoLog(p, sizeof(log), nullptr, log); err += std::string("Link error:\n") + log; return 0; }
    return p;
}

// ---------------------------------------------------------------- Mesh
void Mesh::upload(const std::vector<Vertex>& verts) {
    if (!vao) { glGenVertexArrays(1, &vao); glGenBuffers(1, &vbo); }
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(Vertex), verts.data(), GL_STATIC_DRAW);
    const int S = sizeof(Vertex);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, S, (void*)0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, S, (void*)12);
    glEnableVertexAttribArray(2); glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, S, (void*)24);
    glEnableVertexAttribArray(3); glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, S, (void*)32);
    glEnableVertexAttribArray(4); glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, S, (void*)36);
    glEnableVertexAttribArray(5); glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, S, (void*)48);
    count = (int)verts.size();
}
void Mesh::draw() const { if (count) { glBindVertexArray(vao); glDrawArrays(GL_TRIANGLES, 0, count); } }
void Mesh::destroy() { if (vao) { glDeleteVertexArrays(1, &vao); glDeleteBuffers(1, &vbo); vao = vbo = 0; count = 0; } }

// ---------------------------------------------------------------- MeshBuilder
void MeshBuilder::tri(const V3& a, const V3& b, const V3& c, const V3& n, V2 ua, V2 ub, V2 uc, int tile) {
    v.push_back({a.x, a.y, a.z, n.x, n.y, n.z, ua.x, ua.y, (float)tile, color.x, color.y, color.z, emis});
    v.push_back({b.x, b.y, b.z, n.x, n.y, n.z, ub.x, ub.y, (float)tile, color.x, color.y, color.z, emis});
    v.push_back({c.x, c.y, c.z, n.x, n.y, n.z, uc.x, uc.y, (float)tile, color.x, color.y, color.z, emis});
}
void MeshBuilder::quad(const V3& a, const V3& b, const V3& c, const V3& d, const V3& n, float uw, float vh, int tile) {
    // a=bottom-left b=bottom-right c=top-right d=top-left (CCW seen from normal)
    tri(a, b, c, n, V2(0, vh), V2(uw, vh), V2(uw, 0), tile);
    tri(a, c, d, n, V2(0, vh), V2(uw, 0), V2(0, 0), tile);
}
void MeshBuilder::box(const AABB& b, int tile, float s, int topTile, bool bottom) {
    V3 l = b.lo, h = b.hi;
    int tt = topTile >= 0 ? topTile : tile;
    // world-space UVs so textures line up across boxes
    auto Q = [&](V3 a, V3 bb, V3 c, V3 d, V3 n, V2 ua, V2 ub, V2 uc, V2 ud, int t) {
        tri(a, bb, c, n, ua, ub, uc, t);
        tri(a, c, d, n, ua, uc, ud, t);
    };
    // +Y
    Q(V3(l.x, h.y, h.z), V3(h.x, h.y, h.z), V3(h.x, h.y, l.z), V3(l.x, h.y, l.z), V3(0, 1, 0),
      V2(l.x * s, h.z * s), V2(h.x * s, h.z * s), V2(h.x * s, l.z * s), V2(l.x * s, l.z * s), tt);
    if (bottom)
        Q(V3(l.x, l.y, l.z), V3(h.x, l.y, l.z), V3(h.x, l.y, h.z), V3(l.x, l.y, h.z), V3(0, -1, 0),
          V2(l.x * s, l.z * s), V2(h.x * s, l.z * s), V2(h.x * s, h.z * s), V2(l.x * s, h.z * s), tile);
    // +Z
    Q(V3(l.x, l.y, h.z), V3(h.x, l.y, h.z), V3(h.x, h.y, h.z), V3(l.x, h.y, h.z), V3(0, 0, 1),
      V2(l.x * s, -l.y * s), V2(h.x * s, -l.y * s), V2(h.x * s, -h.y * s), V2(l.x * s, -h.y * s), tile);
    // -Z
    Q(V3(h.x, l.y, l.z), V3(l.x, l.y, l.z), V3(l.x, h.y, l.z), V3(h.x, h.y, l.z), V3(0, 0, -1),
      V2(-h.x * s, -l.y * s), V2(-l.x * s, -l.y * s), V2(-l.x * s, -h.y * s), V2(-h.x * s, -h.y * s), tile);
    // +X
    Q(V3(h.x, l.y, h.z), V3(h.x, l.y, l.z), V3(h.x, h.y, l.z), V3(h.x, h.y, h.z), V3(1, 0, 0),
      V2(-h.z * s, -l.y * s), V2(-l.z * s, -l.y * s), V2(-l.z * s, -h.y * s), V2(-h.z * s, -h.y * s), tile);
    // -X
    Q(V3(l.x, l.y, l.z), V3(l.x, l.y, h.z), V3(l.x, h.y, h.z), V3(l.x, h.y, l.z), V3(-1, 0, 0),
      V2(l.z * s, -l.y * s), V2(h.z * s, -l.y * s), V2(h.z * s, -h.y * s), V2(l.z * s, -h.y * s), tile);
}
void MeshBuilder::cube(const M4& m, int tile, float r) {
    static const V3 N[6] = {V3(0, 1, 0), V3(0, -1, 0), V3(0, 0, 1), V3(0, 0, -1), V3(1, 0, 0), V3(-1, 0, 0)};
    for (int f = 0; f < 6; f++) {
        V3 n = N[f];
        V3 u = std::fabs(n.y) > 0.5f ? V3(1, 0, 0) : cross(V3(0, 1, 0), n);
        V3 w = cross(n, u);
        V3 c = n * 0.5f;
        V3 a = c - u * 0.5f - w * 0.5f, b = c + u * 0.5f - w * 0.5f, cc = c + u * 0.5f + w * 0.5f, d = c - u * 0.5f + w * 0.5f;
        V3 nn = norm(m.dir(n));
        quad(m.point(a), m.point(b), m.point(cc), m.point(d), nn, r, r, tile);
    }
}
void MeshBuilder::cylinder(const M4& m, int tile, int segs, bool caps, float r) {
    for (int i = 0; i < segs; i++) {
        float a0 = TAU * i / segs, a1 = TAU * (i + 1) / segs;
        V3 p0(std::cos(a0) * 0.5f, 0, std::sin(a0) * 0.5f), p1(std::cos(a1) * 0.5f, 0, std::sin(a1) * 0.5f);
        V3 n = norm(m.dir(V3(std::cos((a0 + a1) / 2), 0, std::sin((a0 + a1) / 2))));
        V3 A = m.point(p1 + V3(0, -0.5f, 0)), B = m.point(p0 + V3(0, -0.5f, 0)), C = m.point(p0 + V3(0, 0.5f, 0)), D = m.point(p1 + V3(0, 0.5f, 0));
        float u0 = r * i / segs * 3, u1 = r * (i + 1) / segs * 3;
        tri(A, B, C, n, V2(u1, r), V2(u0, r), V2(u0, 0), tile);
        tri(A, C, D, n, V2(u1, r), V2(u0, 0), V2(u1, 0), tile);
        if (caps) {
            V3 top = m.point(V3(0, 0.5f, 0)), bot = m.point(V3(0, -0.5f, 0));
            V3 nu = norm(m.dir(V3(0, 1, 0)));
            tri(top, D, C, nu, V2(0.5f, 0.5f), V2(p1.x + 0.5f, p1.z + 0.5f), V2(p0.x + 0.5f, p0.z + 0.5f), tile);
            tri(bot, B, A, -nu, V2(0.5f, 0.5f), V2(p0.x + 0.5f, p0.z + 0.5f), V2(p1.x + 0.5f, p1.z + 0.5f), tile);
        }
    }
}
void MeshBuilder::cone(const M4& m, int tile, int segs) {
    V3 apex = m.point(V3(0, 0.5f, 0)), bc = m.point(V3(0, -0.5f, 0));
    for (int i = 0; i < segs; i++) {
        float a0 = TAU * i / segs, a1 = TAU * (i + 1) / segs;
        V3 p0(std::cos(a0) * 0.5f, -0.5f, std::sin(a0) * 0.5f), p1(std::cos(a1) * 0.5f, -0.5f, std::sin(a1) * 0.5f);
        float am = (a0 + a1) / 2;
        V3 n = norm(m.dir(V3(std::cos(am), 0.5f, std::sin(am))));
        tri(m.point(p1), m.point(p0), apex, n, V2(1, 1), V2(0, 1), V2(0.5f, 0), tile);
        tri(bc, m.point(p0), m.point(p1), norm(m.dir(V3(0, -1, 0))), V2(0.5f, 0.5f), V2(0, 0), V2(1, 0), tile);
    }
}
void MeshBuilder::sphere(const M4& m, int tile, int segs, int rings) {
    for (int r = 0; r < rings; r++) {
        float t0 = PI * r / rings, t1 = PI * (r + 1) / rings;
        for (int i = 0; i < segs; i++) {
            float a0 = TAU * i / segs, a1 = TAU * (i + 1) / segs;
            auto P = [&](float t, float a) { return V3(std::sin(t) * std::cos(a) * 0.5f, std::cos(t) * 0.5f, std::sin(t) * std::sin(a) * 0.5f); };
            V3 a = P(t0, a0), b = P(t0, a1), c = P(t1, a1), d = P(t1, a0);
            V3 n = norm(m.dir(norm(a + b + c + d)));
            V2 ua(float(i) / segs, float(r) / rings), ub(float(i + 1) / segs, float(r) / rings), uc(float(i + 1) / segs, float(r + 1) / rings), ud(float(i) / segs, float(r + 1) / rings);
            if (r > 0) tri(m.point(a), m.point(c), m.point(b), n, ua, uc, ub, tile);
            if (r < rings - 1) tri(m.point(a), m.point(d), m.point(c), n, ua, ud, uc, tile);
        }
    }
}
void MeshBuilder::octa(const M4& m, int tile) {
    V3 P[6] = {V3(0.5f, 0, 0), V3(-0.5f, 0, 0), V3(0, 0.5f, 0), V3(0, -0.5f, 0), V3(0, 0, 0.5f), V3(0, 0, -0.5f)};
    int F[8][3] = {{0, 2, 4}, {4, 2, 1}, {1, 2, 5}, {5, 2, 0}, {0, 4, 3}, {4, 1, 3}, {1, 5, 3}, {5, 0, 3}};
    for (auto& f : F) {
        V3 a = m.point(P[f[0]]), b = m.point(P[f[1]]), c = m.point(P[f[2]]);
        V3 n = norm(cross(b - a, c - a));
        tri(a, c, b, -n, V2(0, 1), V2(0.5f, 0), V2(1, 1), tile);
    }
}
void MeshBuilder::append(const MeshBuilder& o) { v.insert(v.end(), o.v.begin(), o.v.end()); }

// ---------------------------------------------------------------- Renderer
static GLuint makeTex(int w, int h, GLenum ifmt, GLenum fmt, GLenum type, const void* data, GLenum filter) {
    GLuint t; glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexImage2D(GL_TEXTURE_2D, 0, ifmt, w, h, 0, fmt, type, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return t;
}

bool Renderer::init(std::string& err) {
    progWorld = link(WORLD_VS, WORLD_FS, err);
    progDepth = link(DEPTH_VS, DEPTH_FS, err);
    progParticle = link(PART_VS, PART_FS, err);
    progPost = link(POST_VS, POST_FS, err);
    progUI = link(UI_VS, UI_FS, err);
    progSky = link(SKY_VS, SKY_FS, err);
    if (!progWorld || !progDepth || !progParticle || !progPost || !progUI || !progSky) return false;

    auto atlas = generateAtlas();
    atlasTex = makeTex(256, 256, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, atlas.data(), GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    // font texture: 16x6 cells of 8x8, glyph 5x7. Cell 95 is solid (used for rectangles).
    std::vector<uint8_t> font(128 * 64, 0);
    for (int ci = 0; ci < 96; ci++) {
        int cx = (ci % 16) * 8, cy = (ci / 16) * 8;
        for (int x = 0; x < 8; x++)
            for (int y = 0; y < 8; y++) {
                bool on;
                if (ci == 95) on = true;
                else on = x < 5 && y < 7 && ((FONT5X7[ci * 5 + x] >> y) & 1);
                font[(cy + y) * 128 + cx + x] = on ? 255 : 0;
            }
    }
    fontTex = makeTex(128, 64, GL_R8, GL_RED, GL_UNSIGNED_BYTE, font.data(), GL_NEAREST);

    glGenVertexArrays(1, &emptyVAO);

    // primitive part meshes
    {
        MeshBuilder b;
        b.cube(M4(), 0); parts[PM_CUBE].upload(b.v);
        b.v.clear(); b.cylinder(M4(), 0, 12); parts[PM_CYL].upload(b.v);
        b.v.clear(); b.sphere(M4(), 0, 10, 7); parts[PM_SPHERE].upload(b.v);
        b.v.clear(); b.cone(M4(), 0, 10); parts[PM_CONE].upload(b.v);
        b.v.clear(); b.octa(M4(), 0); parts[PM_OCTA].upload(b.v);
    }
    // particle buffer: pos3 col4 uv3
    glGenVertexArrays(1, &dynVAO); glGenBuffers(1, &dynVBO);
    glBindVertexArray(dynVAO); glBindBuffer(GL_ARRAY_BUFFER, dynVBO);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 40, (void*)0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 40, (void*)12);
    glEnableVertexAttribArray(2); glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 40, (void*)28);
    // ui buffer
    glGenVertexArrays(1, &uiVAO); glGenBuffers(1, &uiVBO);
    glBindVertexArray(uiVAO); glBindBuffer(GL_ARRAY_BUFFER, uiVBO);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 32, (void*)0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 32, (void*)8);
    glEnableVertexAttribArray(2); glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 32, (void*)16);
    glBindVertexArray(0);

    setShadowQuality(shadowQuality);
    resize(winW, winH);
    GLenum e = glGetError();
    if (e != GL_NO_ERROR) fprintf(stderr, "[render] GL error after init: 0x%x\n", e);
    return true;
}

void Renderer::setShadowQuality(int q) {
    shadowQuality = q;
    int want = q == 0 ? 0 : (q == 1 ? 1024 : 2048);
    if (want == shadowSize) return;
    if (shadowFBO) { glDeleteFramebuffers(1, &shadowFBO); glDeleteTextures(1, &shadowTex); shadowFBO = shadowTex = 0; }
    shadowSize = want;
    if (!want) return;
    shadowTex = makeTex(want, want, GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr, GL_NEAREST);
    glGenFramebuffers(1, &shadowFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowTex, 0);
    glDrawBuffer(GL_NONE); glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::setPixelLines(int lines) { pixelLines = lines; resize(winW, winH); }

void Renderer::resize(int w, int h) {
    winW = std::max(w, 1); winH = std::max(h, 1);
    lowH = std::min(pixelLines, winH);
    lowW = std::max(1, (int)std::lround(lowH * (double)winW / winH));
    if (lowFBO) { glDeleteFramebuffers(1, &lowFBO); glDeleteTextures(1, &lowColor); glDeleteTextures(1, &lowDepth); }
    lowColor = makeTex(lowW, lowH, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, nullptr, GL_NEAREST);
    lowDepth = makeTex(lowW, lowH, GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr, GL_NEAREST);
    glGenFramebuffers(1, &lowFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, lowFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, lowColor, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, lowDepth, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) fprintf(stderr, "[render] low-res framebuffer incomplete\n");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::shutdown() {
    for (auto& p : parts) p.destroy();
}

static const float NEAR_Z = 0.1f, FAR_Z = 220.f;

void Renderer::setCamera(const V3& eye, const V3& target, float fovDeg) {
    camPos = eye;
    view = M4::lookAt(eye, target, V3(0, 1, 0));
    proj = M4::perspective(fovDeg * PI / 180.f, (float)lowW / lowH, NEAR_Z, FAR_Z);
    viewProj = proj * view;
    camFwd = norm(target - eye);
    camRight = norm(cross(camFwd, V3(0, 1, 0)));
    camUp = cross(camRight, camFwd);
}

void Renderer::useProgram(GLuint p) { if (curProg != p) { glUseProgram(p); curProg = p; } }

void Renderer::beginShadow(const V3& focus) {
    shadowPass = true;
    if (!shadowSize) return;
    V3 sd = env.sunDir;
    const float ext = 34.f;
    // stabilise: snap the focus point to shadow texels in light space
    M4 lv = M4::lookAt(V3(0, 0, 0), sd, std::fabs(sd.y) > 0.95f ? V3(1, 0, 0) : V3(0, 1, 0));
    V3 fl = lv.point(focus);
    float texel = 2 * ext / shadowSize;
    fl.x = std::floor(fl.x / texel) * texel; fl.y = std::floor(fl.y / texel) * texel;
    M4 lp = M4::ortho(fl.x - ext, fl.x + ext, fl.y - ext, fl.y + ext, -fl.z - 90, -fl.z + 90);
    lightVP = lp * lv;
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO);
    glViewport(0, 0, shadowSize, shadowSize);
    glEnable(GL_DEPTH_TEST); glDepthMask(GL_TRUE);
    glClear(GL_DEPTH_BUFFER_BIT);
    glDisable(GL_CULL_FACE);
    glEnable(GL_POLYGON_OFFSET_FILL); glPolygonOffset(1.5f, 2.f);
    useProgram(progDepth);
    glUniformMatrix4fv(loc(progDepth, "uVP"), 1, GL_FALSE, lightVP.m);
}
void Renderer::endShadow() {
    glDisable(GL_POLYGON_OFFSET_FILL);
    shadowPass = false;
}

void Renderer::setWorldUniforms(GLuint p) {
    glUniformMatrix4fv(loc(p, "uVP"), 1, GL_FALSE, viewProj.m);
    glUniformMatrix4fv(loc(p, "uLightVP"), 1, GL_FALSE, lightVP.m);
    glUniform1i(loc(p, "uAtlas"), 0);
    glUniform1i(loc(p, "uShadow"), 1);
    glUniform3f(loc(p, "uAmbSky"), env.ambSky.x, env.ambSky.y, env.ambSky.z);
    glUniform3f(loc(p, "uAmbGround"), env.ambGround.x, env.ambGround.y, env.ambGround.z);
    glUniform3f(loc(p, "uSunDir"), env.sunDir.x, env.sunDir.y, env.sunDir.z);
    glUniform3f(loc(p, "uSunCol"), env.sunColor.x, env.sunColor.y, env.sunColor.z);
    glUniform3f(loc(p, "uFogCol"), env.fogColor.x, env.fogColor.y, env.fogColor.z);
    glUniform3f(loc(p, "uCamPos"), camPos.x, camPos.y, camPos.z);
    glUniform1f(loc(p, "uFogDensity"), env.fogDensity);
    glUniform1f(loc(p, "uFogStart"), env.fogStart);
    glUniform1f(loc(p, "uTime"), time);
    glUniform1i(loc(p, "uShadowOn"), shadowSize ? 1 : 0);
    glUniform1f(loc(p, "uShadowTexel"), shadowSize ? 1.f / shadowSize : 0.f);
    // pick the 16 most relevant lights (closest to the camera focus)
    std::vector<std::pair<float, int>> order;
    for (int i = 0; i < (int)lights.size(); i++) {
        float d = len(lights[i].pos - camPos) - lights[i].radius;
        order.push_back({d, i});
    }
    std::sort(order.begin(), order.end());
    int n = std::min<int>(16, (int)order.size());
    float lp[64], lc[64];
    for (int i = 0; i < n; i++) {
        const Light& L = lights[order[i].second];
        lp[i * 4] = L.pos.x; lp[i * 4 + 1] = L.pos.y; lp[i * 4 + 2] = L.pos.z; lp[i * 4 + 3] = L.radius;
        lc[i * 4] = L.color.x; lc[i * 4 + 1] = L.color.y; lc[i * 4 + 2] = L.color.z; lc[i * 4 + 3] = L.intensity;
    }
    glUniform1i(loc(p, "uNumLights"), n);
    if (n) { glUniform4fv(loc(p, "uLP"), n, lp); glUniform4fv(loc(p, "uLC"), n, lc); }
}

void Renderer::beginScene() {
    glBindFramebuffer(GL_FRAMEBUFFER, lowFBO);
    glViewport(0, 0, lowW, lowH);
    glDepthMask(GL_TRUE);
    glClearColor(env.fogColor.x, env.fogColor.y, env.fogColor.z, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    // sky
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    useProgram(progSky);
    M4 vpNoT = proj * M4::lookAt(V3(0, 0, 0), camFwd, V3(0, 1, 0));
    // invert view-projection without translation (compute numerically)
    {
        // Generic 4x4 inverse
        const float* m = vpNoT.m; float inv[16];
        inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
        inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
        inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
        inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
        inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
        inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
        inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
        inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
        inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
        inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
        inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
        inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
        inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
        inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
        inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
        inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];
        float det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
        if (std::fabs(det) > 1e-12f) for (float& x : inv) x /= det;
        glUniformMatrix4fv(loc(progSky, "uInvVP"), 1, GL_FALSE, inv);
    }
    glUniform3f(loc(progSky, "uTop"), env.skyTop.x, env.skyTop.y, env.skyTop.z);
    glUniform3f(loc(progSky, "uHorizon"), env.skyHorizon.x, env.skyHorizon.y, env.skyHorizon.z);
    glUniform3f(loc(progSky, "uMoonDir"), env.moonDir.x, env.moonDir.y, env.moonDir.z);
    glUniform3f(loc(progSky, "uMoonCol"), env.moonColor.x, env.moonColor.y, env.moonColor.z);
    glUniform1f(loc(progSky, "uStars"), env.stars);
    glUniform1f(loc(progSky, "uMoonSize"), env.moonSize);
    glUniform1f(loc(progSky, "uTime"), time);
    glBindVertexArray(emptyVAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE); glCullFace(GL_BACK);
    glDisable(GL_BLEND);
    useProgram(progWorld);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, shadowTex);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, atlasTex);
    setWorldUniforms(progWorld);
    glUniform1i(loc(progWorld, "uMode"), 0);
}

void Renderer::drawMesh(const Mesh& m, const M4& model, const V3& tint, float emis, int tileOverride, float flash, float alpha) {
    if (shadowPass) {
        if (!shadowSize || alpha < 0.5f) return;
        glUniformMatrix4fv(loc(progDepth, "uModel"), 1, GL_FALSE, model.m);
        m.draw();
        return;
    }
    glUniformMatrix4fv(loc(progWorld, "uModel"), 1, GL_FALSE, model.m);
    glUniform3f(loc(progWorld, "uTint"), tint.x, tint.y, tint.z);
    glUniform1f(loc(progWorld, "uEmis"), emis);
    glUniform1f(loc(progWorld, "uTileOverride"), (float)tileOverride);
    glUniform1f(loc(progWorld, "uFlash"), flash);
    glUniform1f(loc(progWorld, "uAlpha"), alpha);
    m.draw();
}

void Renderer::drawPart(PartMesh pm, const M4& model, int tile, const V3& tint, float emis, float flash, float alpha) {
    // parts are closed meshes but we may scale them negatively; disable culling for safety
    if (!shadowPass) glDisable(GL_CULL_FACE);
    drawMesh(parts[pm], model, tint, emis, tile, flash, alpha);
    if (!shadowPass) glEnable(GL_CULL_FACE);
}

void Renderer::drawWater(const Mesh& m, const V3& color, float alpha) {
    if (shadowPass) return;
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUniform1i(loc(progWorld, "uMode"), 1);
    drawMesh(m, M4(), color, 0, -1, 0, alpha);
    glUniform1i(loc(progWorld, "uMode"), 0);
    glDisable(GL_BLEND);
}

void Renderer::updateParticles(float dt) {
    for (size_t i = 0; i < particles.size();) {
        Particle& p = particles[i];
        p.life -= dt;
        if (p.life <= 0) { particles[i] = particles.back(); particles.pop_back(); continue; }
        p.vel.y -= p.gravity * dt;
        p.vel *= 1.f / (1.f + p.drag * dt);
        p.pos += p.vel * dt;
        p.size += p.grow * dt;
        if (p.size < 0) p.size = 0;
        i++;
    }
}

void Renderer::drawParticles() {
    if (particles.empty()) return;
    std::vector<float> alphaBuf, addBuf;
    alphaBuf.reserve(particles.size() * 60);
    for (auto& p : particles) {
        float t = p.life / p.maxLife;
        float a = p.alpha * std::min(1.f, t * 3.f);
        bool additive = p.kind == 1 || p.kind == 3;
        std::vector<float>& buf = additive ? addBuf : alphaBuf;
        V3 r = camRight * p.size, u = camUp * p.size;
        V3 c[4] = {p.pos - r - u, p.pos + r - u, p.pos + r + u, p.pos - r + u};
        float uv[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
        int idx[6] = {0, 1, 2, 0, 2, 3};
        for (int k : idx) {
            buf.insert(buf.end(), {c[k].x, c[k].y, c[k].z, p.color.x, p.color.y, p.color.z, a, uv[k][0], uv[k][1], (float)p.kind});
        }
    }
    useProgram(progParticle);
    glUniformMatrix4fv(loc(progParticle, "uVP"), 1, GL_FALSE, viewProj.m);
    glUniform3f(loc(progParticle, "uFogCol"), env.fogColor.x, env.fogColor.y, env.fogColor.z);
    glUniform3f(loc(progParticle, "uCamPos"), camPos.x, camPos.y, camPos.z);
    glUniform1f(loc(progParticle, "uFogDensity"), env.fogDensity);
    glUniform1f(loc(progParticle, "uFogStart"), env.fogStart);
    glBindVertexArray(dynVAO);
    glBindBuffer(GL_ARRAY_BUFFER, dynVBO);
    glDisable(GL_CULL_FACE);
    if (!alphaBuf.empty()) {
        glBufferData(GL_ARRAY_BUFFER, alphaBuf.size() * 4, alphaBuf.data(), GL_STREAM_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)alphaBuf.size() / 10);
    }
    if (!addBuf.empty()) {
        glDepthMask(GL_FALSE);
        glEnable(GL_BLEND); glBlendFunc(GL_ONE, GL_ONE);
        glBufferData(GL_ARRAY_BUFFER, addBuf.size() * 4, addBuf.data(), GL_STREAM_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)addBuf.size() / 10);
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
    }
    glEnable(GL_CULL_FACE);
    useProgram(progWorld);
}

void Renderer::endScene() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, winW, winH);
    glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE); glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    useProgram(progPost);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, lowDepth);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, lowColor);
    glUniform1i(loc(progPost, "uScene"), 0);
    glUniform1i(loc(progPost, "uDepth"), 1);
    glUniform2f(loc(progPost, "uRes"), (float)lowW, (float)lowH);
    glUniform1f(loc(progPost, "uFade"), sat(fade));
    glUniform4f(loc(progPost, "uOverlay"), overlayCol.x, overlayCol.y, overlayCol.z, sat(overlayA));
    glUniform1f(loc(progPost, "uLevels"), colorLevels);
    glUniform1f(loc(progPost, "uGlitch"), sat(glitch));
    glUniform1f(loc(progPost, "uTime"), time);
    glUniform1i(loc(progPost, "uOutline"), outlines ? 1 : 0);
    glUniform1f(loc(progPost, "uNear"), NEAR_Z);
    glUniform1f(loc(progPost, "uFar"), FAR_Z);
    glBindVertexArray(emptyVAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

// ---------------------------------------------------------------- UI
int Renderer::uiScale() const {
    if (uiScaleOverride > 0) return uiScaleOverride;
    // ~2x at 720p-1080p, 3x at 1440p, 4-5x at 4K: readable without swallowing the screen
    return std::max(1, std::min(winW / 640, (int)std::lround(winH / 450.0)));
}

void Renderer::uiRect(float x, float y, float w, float h, const V4& c) {
    // solid cell 95 -> (15,5) in a 16x6 grid of 8px cells on 128x64
    float u0 = (15 * 8 + 2) / 128.f, v0 = (5 * 8 + 2) / 64.f, u1 = (15 * 8 + 6) / 128.f, v1 = (5 * 8 + 6) / 64.f;
    UIVert q[6] = {{x, y, u0, v0, c.x, c.y, c.z, c.w}, {x + w, y, u1, v0, c.x, c.y, c.z, c.w}, {x + w, y + h, u1, v1, c.x, c.y, c.z, c.w},
                   {x, y, u0, v0, c.x, c.y, c.z, c.w}, {x + w, y + h, u1, v1, c.x, c.y, c.z, c.w}, {x, y + h, u0, v1, c.x, c.y, c.z, c.w}};
    ui.insert(ui.end(), q, q + 6);
}
float Renderer::uiTextWidth(const std::string& s, float scale) const {
    float w = 0, best = 0;
    for (char ch : s) { if (ch == '\n') { best = std::max(best, w); w = 0; } else w += 6 * scale; }
    return std::max(best, w) - (s.empty() ? 0 : scale);
}
void Renderer::uiText(float x, float y, const std::string& s, float sc, const V4& col, bool shadow) {
    if (shadow) uiText(x + sc, y + sc, s, sc, V4(0, 0, 0, col.w * 0.8f), false);
    float cx = x, cy = y;
    for (char ch : s) {
        if (ch == '\n') { cx = x; cy += 9 * sc; continue; }
        int ci = (unsigned char)ch - 32;
        if (ci < 0 || ci > 94) ci = '?' - 32;
        if (ci != 0) {
            float u0 = (ci % 16) * 8 / 128.f, v0 = (ci / 16) * 8 / 64.f;
            float u1 = u0 + 5 / 128.f, v1 = v0 + 7 / 64.f;
            float w = 5 * sc, h = 7 * sc;
            UIVert q[6] = {{cx, cy, u0, v0, col.x, col.y, col.z, col.w}, {cx + w, cy, u1, v0, col.x, col.y, col.z, col.w},
                           {cx + w, cy + h, u1, v1, col.x, col.y, col.z, col.w}, {cx, cy, u0, v0, col.x, col.y, col.z, col.w},
                           {cx + w, cy + h, u1, v1, col.x, col.y, col.z, col.w}, {cx, cy + h, u0, v1, col.x, col.y, col.z, col.w}};
            ui.insert(ui.end(), q, q + 6);
        }
        cx += 6 * sc;
    }
}
void Renderer::uiTextCentered(float cx, float y, const std::string& s, float sc, const V4& col, bool shadow) {
    // center each line individually
    size_t start = 0; float yy = y;
    while (true) {
        size_t nl = s.find('\n', start);
        std::string line = s.substr(start, nl == std::string::npos ? std::string::npos : nl - start);
        uiText(std::floor(cx - uiTextWidth(line, sc) / 2), yy, line, sc, col, shadow);
        if (nl == std::string::npos) break;
        start = nl + 1; yy += 9 * sc;
    }
}
void Renderer::uiFrame(float x, float y, float w, float h, const V4& fill, const V4& border) {
    float s = (float)uiScale();
    uiRect(x, y, w, h, border);
    uiRect(x + s, y + s, w - 2 * s, h - 2 * s, fill);
}
void Renderer::uiIcon(float x, float y, float size, int tile, const V4& c) {
    // atlas tiles are flagged with u >= 2 so the UI shader samples the atlas instead of the font
    float tu = (tile % 16) / 16.f + 2.f, tv = (tile / 16) / 16.f, e = 1 / 16.f, in = 0.2f / 256.f;
    float u0 = tu + in, v0 = tv + in, u1 = tu + e - in, v1 = tv + e - in;
    UIVert q[6] = {{x, y, u0, v0, c.x, c.y, c.z, c.w}, {x + size, y, u1, v0, c.x, c.y, c.z, c.w}, {x + size, y + size, u1, v1, c.x, c.y, c.z, c.w},
                   {x, y, u0, v0, c.x, c.y, c.z, c.w}, {x + size, y + size, u1, v1, c.x, c.y, c.z, c.w}, {x, y + size, u0, v1, c.x, c.y, c.z, c.w}};
    ui.insert(ui.end(), q, q + 6);
}

void Renderer::uiFlush() {
    if (ui.empty()) return;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, winW, winH);
    glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    useProgram(progUI);
    glUniform2f(loc(progUI, "uScreen"), (float)winW, (float)winH);
    glUniform1i(loc(progUI, "uFont"), 0);
    glUniform1i(loc(progUI, "uAtlas"), 1);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, atlasTex);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, fontTex);
    glBindVertexArray(uiVAO);
    glBindBuffer(GL_ARRAY_BUFFER, uiVBO);
    glBufferData(GL_ARRAY_BUFFER, ui.size() * sizeof(UIVert), ui.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)ui.size());
    glDisable(GL_BLEND);
    ui.clear();
}
bool Renderer::project(const V3& w, float& sx, float& sy) const {
    V4 c = viewProj.point4(w);
    if (c.w <= 0.01f) return false;
    float nx = c.x / c.w, ny = c.y / c.w;
    sx = (nx * 0.5f + 0.5f) * winW;
    sy = (1 - (ny * 0.5f + 0.5f)) * winH;
    return nx > -1.2f && nx < 1.2f && ny > -1.2f && ny < 1.2f;
}

bool Renderer::screenshot(const std::string& path) {
    std::vector<uint8_t> px(winW * winH * 3);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, winW, winH, GL_RGB, GL_UNSIGNED_BYTE, px.data());
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;
    fprintf(f, "P6\n%d %d\n255\n", winW, winH);
    for (int y = winH - 1; y >= 0; y--) fwrite(&px[y * winW * 3], 1, winW * 3, f);
    fclose(f);
    return true;
}
