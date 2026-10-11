// WICKLIGHT - small vector/matrix math library
#pragma once
#include <cmath>
#include <cstdint>
#include <algorithm>

static const float PI = 3.14159265358979f;
static const float TAU = 6.28318530717959f;

struct V2 { float x = 0, y = 0; V2() {} V2(float a, float b) : x(a), y(b) {} };

struct V3 {
    float x = 0, y = 0, z = 0;
    V3() {}
    V3(float a, float b, float c) : x(a), y(b), z(c) {}
    explicit V3(float s) : x(s), y(s), z(s) {}
    V3 operator+(const V3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    V3 operator-(const V3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    V3 operator*(float s) const { return {x * s, y * s, z * s}; }
    V3 operator*(const V3& o) const { return {x * o.x, y * o.y, z * o.z}; }
    V3 operator/(float s) const { return {x / s, y / s, z / s}; }
    V3 operator-() const { return {-x, -y, -z}; }
    V3& operator+=(const V3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    V3& operator-=(const V3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    V3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }
    float& operator[](int i) { return (&x)[i]; }
    float operator[](int i) const { return (&x)[i]; }
};

inline float dot(const V3& a, const V3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline V3 cross(const V3& a, const V3& b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
inline float len(const V3& a) { return std::sqrt(dot(a, a)); }
inline float len2(const V3& a) { return dot(a, a); }
inline V3 norm(const V3& a) { float l = len(a); return l > 1e-6f ? a / l : V3(0, 0, 0); }
inline float lenXZ(const V3& a) { return std::sqrt(a.x * a.x + a.z * a.z); }
inline V3 flatXZ(const V3& a) { return {a.x, 0, a.z}; }
inline float distXZ(const V3& a, const V3& b) { return lenXZ(a - b); }
inline V3 lerp(const V3& a, const V3& b, float t) { return a + (b - a) * t; }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline int clampi(int v, int a, int b) { return v < a ? a : (v > b ? b : v); }
inline float smoothstep(float a, float b, float x) { float t = clampf((x - a) / (b - a), 0, 1); return t * t * (3 - 2 * t); }
inline float sat(float v) { return clampf(v, 0.f, 1.f); }
inline float smooth01(float t) { t = sat(t); return t * t * (3 - 2 * t); }
inline float approach(float v, float target, float d) { return v < target ? std::min(v + d, target) : std::max(v - d, target); }
inline float wrapAngle(float a) { while (a > PI) a -= TAU; while (a < -PI) a += TAU; return a; }
inline float angleApproach(float a, float target, float d) {
    float diff = wrapAngle(target - a);
    if (std::fabs(diff) <= d) return target;
    return a + (diff > 0 ? d : -d);
}
inline V3 hexColor(uint32_t h) { return V3(((h >> 16) & 255) / 255.f, ((h >> 8) & 255) / 255.f, (h & 255) / 255.f); }

struct V4 { float x = 0, y = 0, z = 0, w = 0; V4() {} V4(float a, float b, float c, float d) : x(a), y(b), z(c), w(d) {} };

// Column-major 4x4 matrix (OpenGL convention)
struct M4 {
    float m[16];
    M4() { for (int i = 0; i < 16; i++) m[i] = (i % 5 == 0) ? 1.f : 0.f; }
    float& at(int r, int c) { return m[c * 4 + r]; }
    float at(int r, int c) const { return m[c * 4 + r]; }
    M4 operator*(const M4& o) const {
        M4 r;
        for (int c = 0; c < 4; c++)
            for (int rr = 0; rr < 4; rr++) {
                float s = 0;
                for (int k = 0; k < 4; k++) s += at(rr, k) * o.at(k, c);
                r.at(rr, c) = s;
            }
        return r;
    }
    V3 point(const V3& p) const {
        return {at(0, 0) * p.x + at(0, 1) * p.y + at(0, 2) * p.z + at(0, 3),
                at(1, 0) * p.x + at(1, 1) * p.y + at(1, 2) * p.z + at(1, 3),
                at(2, 0) * p.x + at(2, 1) * p.y + at(2, 2) * p.z + at(2, 3)};
    }
    V4 point4(const V3& p) const {
        return {at(0, 0) * p.x + at(0, 1) * p.y + at(0, 2) * p.z + at(0, 3),
                at(1, 0) * p.x + at(1, 1) * p.y + at(1, 2) * p.z + at(1, 3),
                at(2, 0) * p.x + at(2, 1) * p.y + at(2, 2) * p.z + at(2, 3),
                at(3, 0) * p.x + at(3, 1) * p.y + at(3, 2) * p.z + at(3, 3)};
    }
    V3 dir(const V3& p) const {
        return {at(0, 0) * p.x + at(0, 1) * p.y + at(0, 2) * p.z,
                at(1, 0) * p.x + at(1, 1) * p.y + at(1, 2) * p.z,
                at(2, 0) * p.x + at(2, 1) * p.y + at(2, 2) * p.z};
    }
    static M4 translate(const V3& t) { M4 r; r.at(0, 3) = t.x; r.at(1, 3) = t.y; r.at(2, 3) = t.z; return r; }
    static M4 scale(const V3& s) { M4 r; r.at(0, 0) = s.x; r.at(1, 1) = s.y; r.at(2, 2) = s.z; return r; }
    static M4 rotY(float a) { M4 r; float c = std::cos(a), s = std::sin(a); r.at(0, 0) = c; r.at(0, 2) = s; r.at(2, 0) = -s; r.at(2, 2) = c; return r; }
    static M4 rotX(float a) { M4 r; float c = std::cos(a), s = std::sin(a); r.at(1, 1) = c; r.at(1, 2) = -s; r.at(2, 1) = s; r.at(2, 2) = c; return r; }
    static M4 rotZ(float a) { M4 r; float c = std::cos(a), s = std::sin(a); r.at(0, 0) = c; r.at(0, 1) = -s; r.at(1, 0) = s; r.at(1, 1) = c; return r; }
    static M4 perspective(float fovy, float aspect, float zn, float zf) {
        M4 r; float f = 1.f / std::tan(fovy / 2);
        for (int i = 0; i < 16; i++) r.m[i] = 0;
        r.at(0, 0) = f / aspect; r.at(1, 1) = f;
        r.at(2, 2) = (zf + zn) / (zn - zf); r.at(2, 3) = 2 * zf * zn / (zn - zf);
        r.at(3, 2) = -1;
        return r;
    }
    static M4 ortho(float l, float rr, float b, float t, float n, float f) {
        M4 r;
        r.at(0, 0) = 2 / (rr - l); r.at(1, 1) = 2 / (t - b); r.at(2, 2) = -2 / (f - n);
        r.at(0, 3) = -(rr + l) / (rr - l); r.at(1, 3) = -(t + b) / (t - b); r.at(2, 3) = -(f + n) / (f - n);
        return r;
    }
    static M4 lookAt(const V3& eye, const V3& target, const V3& up) {
        V3 f = norm(target - eye);
        V3 s = norm(cross(f, up));
        V3 u = cross(s, f);
        M4 r;
        r.at(0, 0) = s.x; r.at(0, 1) = s.y; r.at(0, 2) = s.z;
        r.at(1, 0) = u.x; r.at(1, 1) = u.y; r.at(1, 2) = u.z;
        r.at(2, 0) = -f.x; r.at(2, 1) = -f.y; r.at(2, 2) = -f.z;
        r.at(0, 3) = -dot(s, eye); r.at(1, 3) = -dot(u, eye); r.at(2, 3) = dot(f, eye);
        return r;
    }
};

// Axis-aligned bounding box
struct AABB {
    V3 lo, hi;
    AABB() {}
    AABB(V3 a, V3 b) : lo(a), hi(b) {}
    V3 center() const { return (lo + hi) * 0.5f; }
    V3 size() const { return hi - lo; }
    bool overlaps(const AABB& o) const {
        return lo.x < o.hi.x && hi.x > o.lo.x && lo.y < o.hi.y && hi.y > o.lo.y && lo.z < o.hi.z && hi.z > o.lo.z;
    }
    bool contains(const V3& p) const {
        return p.x >= lo.x && p.x <= hi.x && p.y >= lo.y && p.y <= hi.y && p.z >= lo.z && p.z <= hi.z;
    }
};

// Ray vs AABB (slab). Returns entry t in [0, maxT] or -1. Outputs face normal.
inline float rayAABB(const V3& o, const V3& d, const AABB& b, float maxT, V3* nrm = nullptr) {
    float tmin = 0, tmax = maxT; int axis = -1; float sign = 0;
    for (int i = 0; i < 3; i++) {
        if (std::fabs(d[i]) < 1e-8f) {
            if (o[i] < b.lo[i] || o[i] > b.hi[i]) return -1;
        } else {
            float inv = 1.f / d[i];
            float t1 = (b.lo[i] - o[i]) * inv, t2 = (b.hi[i] - o[i]) * inv;
            float s = -1;
            if (t1 > t2) { std::swap(t1, t2); s = 1; }
            if (t1 > tmin) { tmin = t1; axis = i; sign = s; }
            if (t2 < tmax) tmax = t2;
            if (tmin > tmax) return -1;
        }
    }
    if (axis < 0) return -1;  // started inside
    if (nrm) { *nrm = V3(0, 0, 0); (*nrm)[axis] = sign; }
    return tmin;
}

// Deterministic RNG
struct Rng {
    uint32_t s;
    explicit Rng(uint32_t seed = 12345) : s(seed ? seed : 1) {}
    uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float f() { return (next() & 0xFFFFFF) / float(0x1000000); }
    float range(float a, float b) { return a + (b - a) * f(); }
    int irange(int a, int b) { return a + int(next() % uint32_t(b - a + 1)); }
};

inline uint32_t hash32(uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352d; x ^= x >> 15; x *= 0x846ca68b; x ^= x >> 16; return x;
}
inline float hashf(int x, int y, int seed = 0) {
    return (hash32(uint32_t(x) * 73856093u ^ uint32_t(y) * 19349663u ^ uint32_t(seed) * 83492791u) & 0xFFFF) / 65535.f;
}
