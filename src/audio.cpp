#include "audio.h"
#include "platform.h"
#include <atomic>
#include <cstring>
#include <cstdio>

namespace {
const int RATE = 44100;
enum Wave { W_SINE, W_SQUARE, W_SAW, W_TRI, W_NOISE };

struct Voice {
    bool on = false, music = false;
    int wave = W_SINE;
    float f0 = 440, f1 = 440, slide = 0;
    float t = 0, dur = 0.1f, atk = 0.005f, decay = 0;
    float vol = 0.3f, panL = 1, panR = 1;
    float phase = 0, duty = 0.5f;
    uint32_t nz = 22222; float nzPhase = 0, nzVal = 0;
    float vib = 0, vibRate = 6;
    float lp = 0, lpCoef = 1;  // one-pole lowpass (1 = off)
};

const int MAXV = 96;
Voice voices[MAXV];
SDL_AudioDeviceID dev = 0;
bool offline = false;  // showcase recording: mixed on demand instead of by a device
float gMaster = 0.8f, gMusic = 0.6f, gSfx = 0.8f;
V3 listenerPos, listenerRight(1, 0, 0);
std::atomic<int> pendingSong{-1};
int curSong = -1;
float musicGain = 0, musicTarget = 1;
double songClock = 0; int step = 0;
Rng mrng(99);

float midiFreq(float n) { return 440.f * std::pow(2.f, (n - 69.f) / 12.f); }

Voice* alloc(bool music) {
    for (auto& v : voices) if (!v.on) { v = Voice(); v.on = true; v.music = music; return &v; }
    // steal the oldest of the same class
    Voice* best = nullptr;
    for (auto& v : voices) if (v.music == music && (!best || v.t / v.dur > best->t / best->dur)) best = &v;
    if (best) { *best = Voice(); best->on = true; best->music = music; }
    return best;
}

Voice* V(int wave, float f0, float f1, float dur, float vol, bool music = false) {
    Voice* v = alloc(music);
    if (!v) return nullptr;
    v->wave = wave; v->f0 = f0; v->f1 = f1; v->slide = dur; v->dur = dur; v->vol = vol;
    return v;
}

// ---------------------------------------------------------------- music definitions
struct SongDef {
    float bpm; int root; int scale[7]; int prog[8]; int progLen; int barsPerChord;
    bool pad, bass, arp, bell, drums, lead, waves, tick, choir;
    int arpWave, leadWave; float drive;
};
const SongDef SONGS[SONG_COUNT] = {
    // TITLE: slow, music box, aeolian
    {68, 57, {0, 2, 3, 5, 7, 8, 10}, {0, 5, 3, 4}, 4, 2, true, false, false, true, false, false, false, false, false, W_TRI, W_SINE, 0},
    // HUB: warm dorian, music box + soft bass
    {80, 50, {0, 2, 3, 5, 7, 9, 10}, {0, 3, 0, 4}, 4, 2, true, true, false, true, false, true, false, false, false, W_TRI, W_SINE, 0},
    // WHARF: phrygian, waves, slow arps
    {74, 52, {0, 1, 3, 5, 7, 8, 10}, {0, 1, 0, 6}, 4, 2, true, true, true, false, false, false, true, false, false, W_TRI, W_SQUARE, 0},
    // CLOCKWOOD: lydian-ish, ticking, plucky arps
    {104, 55, {0, 2, 4, 6, 7, 9, 11}, {0, 1, 5, 4}, 4, 1, true, true, true, true, false, true, false, true, false, W_SQUARE, W_TRI, 0},
    // VAULTS: harmonic minor, choir drones
    {62, 45, {0, 2, 3, 5, 7, 8, 11}, {0, 5, 1, 4}, 4, 2, true, true, false, false, false, true, false, false, true, W_TRI, W_SINE, 0},
    // SPIRE: tense minor, driving
    {96, 47, {0, 2, 3, 5, 7, 8, 10}, {0, 6, 5, 4}, 4, 1, true, true, true, false, true, false, false, false, true, W_SAW, W_SQUARE, 0.4f},
    // BOSS
    {138, 52, {0, 1, 3, 5, 7, 8, 10}, {0, 5, 6, 4}, 4, 1, true, true, true, false, true, true, false, false, false, W_SQUARE, W_SAW, 1.0f},
    // FINAL
    {148, 50, {0, 2, 3, 5, 7, 8, 11}, {0, 5, 3, 4, 0, 5, 6, 4}, 8, 1, true, true, true, false, true, true, false, false, true, W_SAW, W_SQUARE, 1.0f},
    // ENDING: major, gentle, hopeful
    {76, 55, {0, 2, 4, 5, 7, 9, 11}, {0, 4, 5, 3}, 4, 2, true, true, true, true, false, true, false, false, true, W_TRI, W_SINE, 0},
    // JUNK: bouncy synth-funk in mixolydian
    {116, 48, {0, 2, 4, 5, 7, 9, 10}, {0, 6, 3, 4}, 4, 1, true, true, true, false, true, true, false, true, false, W_SQUARE, W_SAW, 0.6f},
    // CITY: night-time spy thriller, minor with a ticking pulse and driving bass
    {124, 50, {0, 2, 3, 5, 7, 8, 11}, {0, 0, 5, 4}, 4, 1, true, true, true, false, true, true, false, true, false, W_SQUARE, W_SAW, 0.7f},
};

int noteOf(const SongDef& s, int degree, int octave) {
    int o = degree / 7 + (degree < 0 && degree % 7 ? -1 : 0);
    int d = ((degree % 7) + 7) % 7;
    return s.root + s.scale[d] + 12 * (o + octave);
}

void musicStep() {
    if (curSong < 0) return;
    const SongDef& s = SONGS[curSong];
    int stepsPerChord = 16 * s.barsPerChord;
    int chordIdx = (step / stepsPerChord) % s.progLen;
    int deg = s.prog[chordIdx];
    int inChord = step % stepsPerChord;
    int st = step % 16;
    float beat = 60.f / s.bpm / 4;  // seconds per step
    if (s.pad && inChord == 0) {
        for (int k = 0; k < 3; k++) {
            Voice* v = V(s.choir ? W_SAW : W_TRI, midiFreq((float)noteOf(s, deg + k * 2, 0)), 0, beat * stepsPerChord, s.choir ? 0.045f : 0.05f, true);
            if (v) { v->f1 = v->f0; v->atk = beat * 4; v->lpCoef = s.choir ? 0.05f : 0.12f; v->vib = s.choir ? 0.004f : 0; v->vibRate = 4.5f; }
        }
    }
    if (s.bass) {
        bool hit = st == 0 || st == 8 || (s.drive > 0.5f && (st == 6 || st == 14)) || (s.drive > 0 && st == 11);
        if (hit) {
            Voice* v = V(s.drive > 0.5f ? W_SQUARE : W_TRI, midiFreq((float)noteOf(s, deg, -2)), 0, beat * (s.drive > 0 ? 1.8f : 5), 0.16f, true);
            if (v) { v->f1 = v->f0; v->decay = 2.5f; v->lpCoef = 0.18f; v->duty = 0.3f; }
        }
    }
    if (s.arp) {
        int every = s.drive > 0.5f ? 1 : 2;
        if (st % every == 0) {
            int pat[8] = {0, 2, 4, 7, 4, 2, 0, 4};
            int n = noteOf(s, deg + pat[(st / every) % 8], s.drive > 0.5f ? 0 : 1);
            Voice* v = V(s.arpWave, midiFreq((float)n), 0, beat * 1.6f, s.drive > 0.5f ? 0.045f : 0.055f, true);
            if (v) { v->f1 = v->f0; v->decay = 7; v->duty = 0.25f; v->lpCoef = 0.35f; }
        }
    }
    if (s.bell && (st == 0 || st == 6 || st == 10 || (st == 13 && mrng.f() < 0.4f))) {
        if (mrng.f() < 0.75f) {
            int n = noteOf(s, deg + mrng.irange(0, 4) * 2 - 0, 2);
            Voice* v = V(W_SINE, midiFreq((float)n), 0, 1.6f, 0.07f, true);
            if (v) { v->f1 = v->f0; v->decay = 2.2f; }
            Voice* o = V(W_SINE, midiFreq((float)n) * 2.01f, 0, 0.6f, 0.02f, true);
            if (o) { o->f1 = o->f0; o->decay = 5; }
        }
    }
    if (s.lead && (st % 4 == 0 || (s.drive > 0.5f && st % 2 == 0)) && mrng.f() < (s.drive > 0.5f ? 0.55f : 0.35f)) {
        int n = noteOf(s, deg + mrng.irange(-2, 6), 1);
        Voice* v = V(s.leadWave, midiFreq((float)n), 0, beat * mrng.irange(2, 4), s.drive > 0.5f ? 0.04f : 0.05f, true);
        if (v) { v->f1 = v->f0; v->atk = 0.02f; v->decay = 1.5f; v->vib = 0.006f; v->lpCoef = 0.25f; v->duty = 0.5f; }
    }
    if (s.drums) {
        if (st == 0 || st == 8 || (s.drive > 0.5f && (st == 4 || st == 12 || st == 10))) {
            Voice* v = V(W_SINE, 140, 40, 0.18f, 0.32f, true);
            if (v) v->decay = 10;
        }
        if (st == 4 || st == 12) {
            Voice* v = V(W_NOISE, 3000, 2000, 0.14f, 0.12f, true);
            if (v) { v->decay = 18; v->lpCoef = 0.5f; }
        }
        if (st % 2 == 0) {
            Voice* v = V(W_NOISE, 9000, 9000, 0.04f, 0.035f, true);
            if (v) v->decay = 50;
        }
    }
    if (s.tick && st % 4 == 0) {
        Voice* v = V(W_NOISE, 7000, 7000, 0.03f, st % 8 == 0 ? 0.06f : 0.035f, true);
        if (v) v->decay = 60;
        if (st == 0) { Voice* w = V(W_SQUARE, 1800, 1800, 0.03f, 0.02f, true); if (w) w->decay = 60; }
    }
    if (s.waves && step % 32 == 0) {
        Voice* v = V(W_NOISE, 400, 300, 4.0f, 0.05f, true);
        if (v) { v->atk = 1.6f; v->lpCoef = 0.02f; }
    }
}

void mix(void*, Uint8* stream, int len) {
    float* out = (float*)stream;
    int frames = len / 8;
    memset(stream, 0, len);
    int ps = pendingSong.load();
    if (ps != curSong) {
        musicTarget = 0;
        if (musicGain <= 0.001f) {
            curSong = ps;
            for (auto& v : voices) if (v.music) v.on = false;
            step = 0; songClock = 0; musicTarget = 1;
        }
    }
    for (int i = 0; i < frames; i++) {
        // sequencer
        if (curSong >= 0) {
            songClock -= 1.0 / RATE;
            if (songClock <= 0) {
                musicStep();
                step++;
                songClock += 60.0 / SONGS[curSong].bpm / 4;
            }
        }
        musicGain = approach(musicGain, musicTarget, 1.0f / RATE);
        float L = 0, R = 0;
        for (auto& v : voices) {
            if (!v.on) continue;
            if (v.t < 0) { v.t += 1.f / RATE; continue; }  // delayed start
            float k =v.slide > 0 ? std::min(v.t / v.slide, 1.f) : 1.f;
            float f = v.f0 + (v.f1 - v.f0) * k;
            if (v.vib > 0) f *= 1 + v.vib * std::sin(v.t * v.vibRate * TAU);
            float s;
            v.phase += f / RATE;
            if (v.phase >= 1) v.phase -= std::floor(v.phase);
            switch (v.wave) {
                case W_SINE: s = std::sin(v.phase * TAU); break;
                case W_SQUARE: s = v.phase < v.duty ? 0.8f : -0.8f; break;
                case W_SAW: s = v.phase * 2 - 1; break;
                case W_TRI: s = v.phase < 0.5f ? v.phase * 4 - 1 : 3 - v.phase * 4; break;
                default: {
                    v.nzPhase += f / RATE;
                    if (v.nzPhase >= 1) { v.nzPhase -= std::floor(v.nzPhase); v.nz ^= v.nz << 13; v.nz ^= v.nz >> 17; v.nz ^= v.nz << 5; v.nzVal = (v.nz & 0xFFFF) / 32768.f - 1; }
                    s = v.nzVal;
                }
            }
            v.lp += (s - v.lp) * v.lpCoef;
            s = v.lp;
            float env = v.t < v.atk ? v.t / v.atk : 1.f;
            float rel = std::min(0.02f, v.dur * 0.3f);
            if (v.t > v.dur - rel) env *= std::max(0.f, (v.dur - v.t) / rel);
            if (v.decay > 0) env *= std::exp(-v.t * v.decay);
            float g = v.vol * env * (v.music ? gMusic * musicGain : gSfx);
            L += s * g * v.panL; R += s * g * v.panR;
            v.t += 1.f / RATE;
            if (v.t >= v.dur) v.on = false;
        }
        L *= gMaster; R *= gMaster;
        // soft clip
        out[i * 2] = std::tanh(L);
        out[i * 2 + 1] = std::tanh(R);
    }
}

void spawnSfx(Sfx s, float vol, float pitch, float pl, float pr) {
    auto P = [&](int w, float f0, float f1, float dur, float v) -> Voice* {
        Voice* x = V(w, f0 * pitch, f1 * pitch, dur, v * vol);
        if (x) { x->panL = pl; x->panR = pr; }
        return x;
    };
    Voice* v;
    switch (s) {
        case SFX_JUMP: if ((v = P(W_SQUARE, 300, 620, 0.12f, 0.12f))) { v->duty = 0.25f; v->lpCoef = 0.4f; } break;
        case SFX_LAND: if ((v = P(W_NOISE, 900, 300, 0.08f, 0.12f))) v->lpCoef = 0.2f; break;
        case SFX_STEP: if ((v = P(W_NOISE, 1400, 900, 0.04f, 0.04f))) v->lpCoef = 0.25f; break;
        case SFX_DASH: if ((v = P(W_NOISE, 2500, 600, 0.2f, 0.2f))) v->lpCoef = 0.3f; break;
        case SFX_THROW:
            if ((v = P(W_NOISE, 1800, 3500, 0.18f, 0.14f))) v->lpCoef = 0.35f;
            if ((v = P(W_TRI, 500, 900, 0.12f, 0.1f))) v->decay = 10;
            break;
        case SFX_RECALL: if ((v = P(W_TRI, 900, 500, 0.15f, 0.12f))) v->decay = 6; if ((v = P(W_NOISE, 3000, 1500, 0.15f, 0.06f))) v->lpCoef = 0.3f; break;
        case SFX_CATCH: if ((v = P(W_SINE, 880, 1320, 0.1f, 0.12f))) v->decay = 12; break;
        case SFX_WARP:
            if ((v = P(W_SAW, 200, 1600, 0.22f, 0.12f))) v->lpCoef = 0.2f;
            if ((v = P(W_SINE, 1600, 400, 0.3f, 0.1f))) v->decay = 5;
            break;
        case SFX_BLOOM:
            if ((v = P(W_NOISE, 1200, 120, 0.6f, 0.4f))) { v->lpCoef = 0.25f; v->decay = 4; }
            if ((v = P(W_SINE, 120, 50, 0.5f, 0.4f))) v->decay = 5;
            break;
        case SFX_CHARGE: if ((v = P(W_SAW, 200, 800, 0.8f, 0.07f))) { v->lpCoef = 0.1f; v->vib = 0.02f; v->vibRate = 14; } break;
        case SFX_SUNFLARE:
            if ((v = P(W_SAW, 1200, 200, 0.4f, 0.16f))) v->lpCoef = 0.3f;
            if ((v = P(W_NOISE, 4000, 800, 0.4f, 0.2f))) v->lpCoef = 0.4f;
            break;
        case SFX_HIT:
            if ((v = P(W_SQUARE, 220, 90, 0.1f, 0.16f))) v->duty = 0.3f;
            if ((v = P(W_NOISE, 3000, 800, 0.08f, 0.14f))) v->lpCoef = 0.5f;
            break;
        case SFX_KILL:
            if ((v = P(W_NOISE, 2000, 200, 0.35f, 0.22f))) { v->lpCoef = 0.3f; v->decay = 6; }
            if ((v = P(W_SQUARE, 600, 100, 0.25f, 0.1f))) v->duty = 0.2f;
            break;
        case SFX_HURT:
            if ((v = P(W_SQUARE, 400, 120, 0.25f, 0.18f))) v->duty = 0.5f;
            if ((v = P(W_NOISE, 1200, 300, 0.2f, 0.12f))) v->lpCoef = 0.3f;
            break;
        case SFX_DEATH:
            if ((v = P(W_SAW, 400, 40, 1.4f, 0.2f))) v->lpCoef = 0.15f;
            if ((v = P(W_NOISE, 800, 100, 1.2f, 0.15f))) { v->lpCoef = 0.1f; v->decay = 2; }
            break;
        case SFX_BRAZIER:
            if ((v = P(W_NOISE, 600, 1800, 0.5f, 0.2f))) { v->lpCoef = 0.2f; v->decay = 3; }
            if ((v = P(W_SINE, 523, 523, 0.6f, 0.08f))) v->decay = 3;
            if ((v = P(W_SINE, 784, 784, 0.6f, 0.06f))) v->decay = 3;
            break;
        case SFX_GATE: if ((v = P(W_SAW, 70, 55, 1.2f, 0.18f))) { v->lpCoef = 0.06f; } if ((v = P(W_NOISE, 300, 200, 1.2f, 0.1f))) v->lpCoef = 0.05f; break;
        case SFX_SHARD:
            for (int k = 0; k < 3; k++) if ((v = P(W_SINE, 880.f * std::pow(1.26f, (float)k), 0, 0.4f, 0.1f))) { v->f1 = v->f0; v->decay = 5; v->t = -0.07f * k; v->dur += 0.07f * k; }
            break;
        case SFX_CANDLE:
            for (int k = 0; k < 4; k++) if ((v = P(W_TRI, 523.f * std::pow(1.19f, (float)k), 0, 0.7f, 0.09f))) { v->f1 = v->f0; v->decay = 3; v->t = -0.1f * k; v->dur += 0.1f * k; }
            break;
        case SFX_HEART: case SFX_UNLOCK:
            for (int k = 0; k < 5; k++) {
                static const float iv[5] = {1, 1.25f, 1.5f, 2, 2.5f};
                if ((v = P(W_TRI, 392.f * iv[k], 0, 1.6f, 0.1f))) { v->f1 = v->f0; v->decay = 1.5f; v->t = -0.12f * k; v->dur += 0.12f * k; v->vib = 0.004f; }
            }
            break;
        case SFX_CHECKPOINT:
            if ((v = P(W_SINE, 660, 660, 0.8f, 0.12f))) v->decay = 3;
            if ((v = P(W_SINE, 990, 990, 0.8f, 0.08f))) { v->decay = 3; v->t = -0.1f; v->dur += 0.1f; }
            break;
        case SFX_MENU_MOVE: if ((v = P(W_SQUARE, 700, 700, 0.04f, 0.06f))) v->duty = 0.25f; break;
        case SFX_MENU_SELECT: if ((v = P(W_SQUARE, 600, 1200, 0.1f, 0.08f))) v->duty = 0.25f; break;
        case SFX_MENU_BACK: if ((v = P(W_SQUARE, 600, 300, 0.1f, 0.07f))) v->duty = 0.25f; break;
        case SFX_ROAR:
            if ((v = P(W_SAW, 90, 50, 1.4f, 0.22f))) { v->lpCoef = 0.08f; v->vib = 0.05f; v->vibRate = 9; }
            if ((v = P(W_NOISE, 500, 150, 1.4f, 0.18f))) v->lpCoef = 0.08f;
            break;
        case SFX_SLAM:
            if ((v = P(W_SINE, 90, 30, 0.6f, 0.45f))) v->decay = 4;
            if ((v = P(W_NOISE, 900, 100, 0.5f, 0.3f))) { v->lpCoef = 0.15f; v->decay = 5; }
            break;
        case SFX_SPIT: if ((v = P(W_NOISE, 600, 1400, 0.18f, 0.14f))) v->lpCoef = 0.2f; if ((v = P(W_SQUARE, 180, 90, 0.12f, 0.06f))) v->duty = 0.4f; break;
        case SFX_BELL:
            for (int k = 0; k < 3; k++) {
                static const float iv[3] = {1, 2.76f, 5.4f};
                if ((v = P(W_SINE, 110.f * iv[k], 0, 2.5f, 0.2f / (k + 1)))) { v->f1 = v->f0; v->decay = 1.2f + k; }
            }
            break;
        case SFX_SWING: if ((v = P(W_NOISE, 500, 3000, 0.3f, 0.16f))) v->lpCoef = 0.2f; break;
        case SFX_CLANK: if ((v = P(W_SQUARE, 1500, 1400, 0.12f, 0.08f))) { v->decay = 25; v->duty = 0.1f; } if ((v = P(W_NOISE, 5000, 4000, 0.08f, 0.08f))) v->decay = 30; break;
        case SFX_FLUTTER: if ((v = P(W_NOISE, 200, 260, 0.25f, 0.08f))) { v->lpCoef = 0.1f; v->vib = 0.4f; v->vibRate = 30; } break;
        case SFX_BLINK: if ((v = P(W_SINE, 1500, 200, 0.25f, 0.1f))) v->vib = 0.1f; break;
        case SFX_BLIP: if ((v = P(W_SQUARE, 900, 900, 0.025f, 0.035f))) v->duty = 0.5f; break;
        case SFX_SHIELD: if ((v = P(W_SQUARE, 1200, 900, 0.18f, 0.12f))) { v->duty = 0.15f; v->decay = 12; } if ((v = P(W_SINE, 2400, 2300, 0.3f, 0.06f))) v->decay = 8; break;
        case SFX_WHISPER: if ((v = P(W_NOISE, 2400, 1800, 1.5f, 0.12f))) { v->lpCoef = 0.12f; v->atk = 0.5f; v->vib = 0.2f; v->vibRate = 3; } break;
        case SFX_SPLASH: if ((v = P(W_NOISE, 1500, 400, 0.45f, 0.22f))) { v->lpCoef = 0.3f; v->decay = 4; } break;
        case SFX_BREAK:
            if ((v = P(W_NOISE, 2500, 300, 0.5f, 0.3f))) { v->lpCoef = 0.4f; v->decay = 5; }
            if ((v = P(W_SQUARE, 300, 80, 0.3f, 0.12f))) v->duty = 0.3f;
            break;
        case SFX_TICK: if ((v = P(W_NOISE, 6000, 6000, 0.03f, 0.08f))) v->decay = 50; break;
        default: break;
    }
}
}  // namespace

namespace audio {
bool init() {
    if (sdl::InitSubSystem(SDL_INIT_AUDIO) != 0) { fprintf(stderr, "[audio] init failed: %s\n", sdl::GetError()); return false; }
    SDL_AudioSpec want{}, have{};
    want.freq = RATE; want.format = AUDIO_F32SYS; want.channels = 2; want.samples = 1024; want.callback = mix;
    dev = sdl::OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (!dev) { fprintf(stderr, "[audio] no audio device: %s (continuing without sound)\n", sdl::GetError()); return false; }
    if (have.format != AUDIO_F32SYS || have.channels != 2) {
        fprintf(stderr, "[audio] unexpected format, disabling audio\n");
        sdl::CloseAudioDevice(dev); dev = 0; return false;
    }
    sdl::PauseAudioDevice(dev, 0);
    return true;
}
bool initOffline() { offline = true; return true; }
void forceSong(Song s) {  // hard cut, no fade: the new song starts on its first beat right now
    for (auto& v : voices) if (v.music) v.on = false;
    pendingSong.store((int)s); curSong = (int)s; step = 0; songClock = 0; musicGain = 1; musicTarget = 1;
}
void stopSfx() { for (auto& v : voices) if (!v.music) v.on = false; }
void renderOffline(float* out, int frames) { mix(nullptr, (Uint8*)out, frames * 8); }
void shutdown() { if (dev) { sdl::CloseAudioDevice(dev); dev = 0; } }
bool available() { return dev != 0 || offline; }
void setVolumes(float master, float music, float sfx) {
    if (dev) sdl::LockAudioDevice(dev);
    gMaster = master; gMusic = music; gSfx = sfx;
    if (dev) sdl::UnlockAudioDevice(dev);
}
void play(Sfx s, float vol, float pitch) {
    if (!dev && !offline) return;
    if (dev) sdl::LockAudioDevice(dev);
    spawnSfx(s, vol, pitch, 1, 1);
    if (dev) sdl::UnlockAudioDevice(dev);
}
void playAt(Sfx s, const V3& pos, float vol, float pitch) {
    if (!dev && !offline) return;
    V3 d = pos - listenerPos;
    float dist = len(d);
    float att = 1.f / (1.f + dist * dist * 0.004f);
    if (att < 0.03f) return;
    float pan = dist > 0.01f ? dot(d / dist, listenerRight) : 0;
    float pl = 1 - std::max(0.f, pan) * 0.6f, pr = 1 + std::min(0.f, pan) * 0.6f;
    if (dev) sdl::LockAudioDevice(dev);
    spawnSfx(s, vol * att, pitch, pl, pr);
    if (dev) sdl::UnlockAudioDevice(dev);
}
void setListener(const V3& pos, const V3& right) { listenerPos = pos; listenerRight = right; }
void setSong(Song s) { pendingSong.store((int)s); }
}  // namespace audio
