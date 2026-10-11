// WICKLIGHT - software synthesizer: procedural sound effects and generative music.
#pragma once
#include "mathx.h"

enum Sfx {
    SFX_JUMP, SFX_LAND, SFX_DASH, SFX_THROW, SFX_RECALL, SFX_CATCH, SFX_WARP, SFX_BLOOM, SFX_CHARGE, SFX_SUNFLARE,
    SFX_HIT, SFX_KILL, SFX_HURT, SFX_DEATH, SFX_BRAZIER, SFX_GATE, SFX_SHARD, SFX_CANDLE, SFX_HEART, SFX_CHECKPOINT,
    SFX_MENU_MOVE, SFX_MENU_SELECT, SFX_MENU_BACK, SFX_ROAR, SFX_SLAM, SFX_SPIT, SFX_BELL, SFX_SWING, SFX_CLANK,
    SFX_FLUTTER, SFX_BLINK, SFX_STEP, SFX_BLIP, SFX_SHIELD, SFX_UNLOCK, SFX_WHISPER, SFX_SPLASH, SFX_BREAK, SFX_TICK,
    SFX_COUNT
};

enum Song { SONG_NONE = -1, SONG_TITLE = 0, SONG_HUB, SONG_WHARF, SONG_CLOCK, SONG_VAULTS, SONG_SPIRE, SONG_BOSS, SONG_FINAL, SONG_ENDING, SONG_JUNK, SONG_CITY, SONG_COUNT };

namespace audio {
bool init();
void shutdown();
void play(Sfx s, float vol = 1.f, float pitch = 1.f);
void playAt(Sfx s, const V3& pos, float vol = 1.f, float pitch = 1.f);
void setListener(const V3& pos, const V3& right);
void setSong(Song s);
void setVolumes(float master, float music, float sfx);
bool available();
bool initOffline();                         // showcase recording: no device, pull samples yourself
void renderOffline(float* out, int frames);  // interleaved stereo float
void forceSong(Song s);                      // hard cut to a song, starting on beat 1
void stopSfx();                              // silence every sound effect
}  // namespace audio
