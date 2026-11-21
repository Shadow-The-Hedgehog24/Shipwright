// Shadow the Hedgehog - Chaos Control for Ship of Harkinian 9.3.1  (version 2)
//
// Chaos Control is an item. It sits in the pause menu in the Nayru's Love slot (see "The item" below) and is
// equipped to a C button like the Ocarina or the Deku Stick. Press that C button to use it:
//
//  1. Link/Shadow is locked: he cannot move, attack, use items or be steered while the move starts.
//  2. Shadow plays the Chaos Control pose (SA2 animation 180). The music starts at the right moment so that the
//     shouted word CONTROL lands on frame 50 of the animation.
//  3. On the beat drop time stops for everything dangerous (enemies, bosses, bombs, deku nuts) while Link keeps
//     moving. A bar in the top left (the magic meter's look) shows how long it will last.
//  4. The ending sound plays, the enemies get going again just after it, then there is a cooldown. The bar refills
//     (greyed out) during the cooldown.
//
// The music, the shouted line, the ending sound, the pose animation and the timing numbers come from the separate
// audio mod (chaos_control_audio.o2r in the mods folder, files under custom/chaos_control/). They are played
// through their own SDL audio device, so they do not depend on the game's sound engine.
// If the audio mod is missing, Chaos Control still works: no pose, no music, a game sound, and it starts instantly.
//
// The item
// --------
// Chaos Control reuses the game's "Roc's Feather" item (ITEM_ROCS_FEATHER). That item already has everything a new
// item needs: an icon, a name, an age requirement, and a pause menu slot that it can share with Nayru's Love
// (highlight the slot, press A, move the stick left or right to swap). The icon and the name come from the
// o2r mod (textures/icon_item_static/gRocsFeatherTex and textures/item_name_static/gRocsFeatherItemName*Tex).
//   - If the Nayru's Love slot is empty, Chaos Control is put there right away.
//   - If you own Nayru's Love, it stays in the slot and Chaos Control is the second choice in the same slot.
//   - Roc's Feather's own jump is switched off here (this file takes over the item).
// Not used in randomizer saves, where Roc's Feather keeps its normal meaning.
//
// While time is stopped, frozen actors are skipped by the game's actor update, so they also stop registering
// their hit boxes: Link walks through them and cannot damage them, and they cannot damage Link. That is how the
// game treats every actor it pauses (talking, ocarina, etc.).
//
// Everything is in this one file. CMake picks it up automatically (soh/*.cpp is globbed).
//
// Settings (CVars, change with the in-game console: set <name> <value>):
//   gShadow.ChaosControl.Enabled      1 = on (default), 0 = off
//   gShadow.ChaosControl.Item         1 = Chaos Control is an item in the Nayru's Love slot (default), 0 = no item
//   gShadow.ChaosControl.Button       extra button that also starts it, default 0 = none. 32 = L, 8192 = Z
//   gShadow.ChaosControl.Duration     seconds time stays stopped after the beat drop, default 6
//   gShadow.ChaosControl.ReleaseDelay milliseconds time stays stopped after the ending sound starts, default 600.
//                                     The ending sound builds up to its chime about 0.5 s in, and the sound takes a
//                                     moment to come out of the speakers. Raise this if enemies still move before the
//                                     chime is heard, lower it if they stay frozen too long
//   gShadow.ChaosControl.Cooldown     seconds before it can be used again, default 10
//   gShadow.ChaosControl.Sound        1 = music and sounds (default), 0 = silent, starts after the pose only
//   gShadow.ChaosControl.Volume       0-100, volume of the music, voice and ending sound, default 80
//   gShadow.ChaosControl.DropOffset   milliseconds added to the beat drop time, default 0 (negative = earlier)
//   gShadow.ChaosControl.SyncOffset   milliseconds the music is shifted against the pose, default 0. If the shout
//                                     comes too early, set it positive; too late, negative
//   gShadow.ChaosControl.Animation    1 = play the pose (default), 0 = no pose
//   gShadow.ChaosControl.HitEnemies   1 = Shadow can hit frozen enemies (default), 0 = they cannot be hit
//   gShadow.ChaosControl.PoseLift     extra height added to the pose in model units (100 = 1 game unit), default 0.
//                                     The pose file already keeps the feet above the floor; use this only to fine-tune.
//   gShadow.ChaosControl.Lock         1 = lock the player during the start (default), 0 = no lock
//   gShadow.ChaosControl.Bar          1 = show the duration bar (default), 0 = hide it
//   gShadow.ChaosControl.BarX/BarY    bar position, default 18 / 56 (320x240 screen units, from the left edge)
//   gShadow.ChaosControl.BarCooldown  1 = the bar refills during the cooldown (default), 0 = hide it then

#include <libultraship/bridge/consolevariablebridge.h>

#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/ActorDB.h"
#include "soh/ShipInit.hpp"

#include <SDL2/SDL.h>
#include <ship/Context.h>
#include <ship/resource/File.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/ArchiveManager.h>

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

extern "C" {
#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
extern PlayState* gPlayState;
int16_t OTRGetRectDimensionFromLeftEdge(float v); // x position adjusted for wide screens
// OPEN_DISPS / CLOSE_DISPS declare these inside the function. They have to be found as C functions.
void FrameInterpolation_RecordOpenChild(const void* a, int b);
void FrameInterpolation_RecordCloseChild(void);
}

namespace {

constexpr const char* kCvarEnabled = "gShadow.ChaosControl.Enabled";
constexpr const char* kCvarItem = "gShadow.ChaosControl.Item";
constexpr const char* kCvarButton = "gShadow.ChaosControl.Button";
constexpr const char* kCvarDuration = "gShadow.ChaosControl.Duration";
constexpr const char* kCvarRelease = "gShadow.ChaosControl.ReleaseDelay";
constexpr const char* kCvarCooldown = "gShadow.ChaosControl.Cooldown";
constexpr const char* kCvarSound = "gShadow.ChaosControl.Sound";
constexpr const char* kCvarVolume = "gShadow.ChaosControl.Volume";
constexpr const char* kCvarDropOffset = "gShadow.ChaosControl.DropOffset";
constexpr const char* kCvarSyncOffset = "gShadow.ChaosControl.SyncOffset";
constexpr const char* kCvarAnimation = "gShadow.ChaosControl.Animation";
constexpr const char* kCvarPoseLift = "gShadow.ChaosControl.PoseLift";
constexpr const char* kCvarHitEnemies = "gShadow.ChaosControl.HitEnemies";
constexpr const char* kCvarLock = "gShadow.ChaosControl.Lock";
constexpr const char* kCvarBar = "gShadow.ChaosControl.Bar";
constexpr const char* kCvarBarX = "gShadow.ChaosControl.BarX";
constexpr const char* kCvarBarY = "gShadow.ChaosControl.BarY";
constexpr const char* kCvarBarCooldown = "gShadow.ChaosControl.BarCooldown";

// Files inside the audio mod (chaos_control_audio.o2r).
constexpr const char* kFileMusic = "custom/chaos_control/music.wav";
constexpr const char* kFileVoice = "custom/chaos_control/voice.wav";
constexpr const char* kFileEnd = "custom/chaos_control/end.wav";
constexpr const char* kFileConfig = "custom/chaos_control/config.txt";
constexpr const char* kFileAnimAdult = "custom/chaos_control/anim_adult.bin";
constexpr const char* kFileAnimChild = "custom/chaos_control/anim_child.bin";

// The game logic runs at 20 frames per second.
constexpr int kFramesPerSecond = 20;
constexpr int kMsPerFrame = 1000 / kFramesPerSecond;
// If the music never reaches the beat drop (bad settings), start the freeze after this many frames anyway.
constexpr int kMaxWindupFrames = 60 * kFramesPerSecond;
// The pose fades in over this many frames and fades out again when Chaos Control starts / is interrupted.
constexpr int kBlendInFrames = 3;
constexpr int kBlendOutFrames = 4;

// ------------------------------------------------------------------------------------------------
// Audio: a tiny mixer on its own SDL audio device (44.1 kHz stereo, 16 bit).
// ------------------------------------------------------------------------------------------------

constexpr int kRate = 44100;
constexpr int kChannels = 2;

enum TrackId { kTrackMusic = 0, kTrackVoice, kTrackEnd, kTrackCount };
constexpr float kTrackLevel[kTrackCount] = { 0.6f, 1.0f, 0.9f };

struct Clip {
    std::vector<int16_t> pcm; // interleaved stereo
    bool ok = false;
};

struct Track {
    const Clip* clip = nullptr;
    size_t pos = 0; // in interleaved samples
    float gain = 0.0f;
    float target = 0.0f;
    float step = 0.0f; // gain change per interleaved sample while fading
    bool playing = false;
};

Clip sClips[kTrackCount];
Track sTracks[kTrackCount];
SDL_AudioDeviceID sDevice = 0;
bool sAssetsTried = false;
int sDropMs = 0;        // music time of the beat drop (freeze starts)
int sControlMs = -1;    // music time where the shouted word CONTROL starts; -1 = unknown (music starts at once)
int sVoiceStartMs = -1; // when the voice line starts; -1 = computed so that it ends at the beat drop
int sAnimControlFrame = 50; // frame of the original (SA2) animation where CONTROL is shouted
int sAnimSourceFps = 24;    // frame rate of the original (SA2) animation

void SDLCALL MixCallback(void*, Uint8* stream, int len) {
    int16_t* out = reinterpret_cast<int16_t*>(stream);
    const int count = len / static_cast<int>(sizeof(int16_t));
    for (int i = 0; i < count; i++) {
        float acc = 0.0f;
        for (int t = 0; t < kTrackCount; t++) {
            Track& tr = sTracks[t];
            if (!tr.playing) {
                continue;
            }
            if (tr.clip == nullptr || tr.pos >= tr.clip->pcm.size()) {
                tr.playing = false;
                continue;
            }
            if (tr.gain > tr.target) {
                tr.gain = std::max(tr.target, tr.gain - tr.step);
            }
            acc += static_cast<float>(tr.clip->pcm[tr.pos++]) * tr.gain;
            if (tr.target <= 0.0f && tr.gain <= 0.0f) {
                tr.playing = false;
            }
        }
        acc = std::max(-32768.0f, std::min(32767.0f, acc));
        out[i] = static_cast<int16_t>(acc);
    }
}

bool EnsureDevice() {
    if (sDevice != 0) {
        return true;
    }
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        return false;
    }
    SDL_AudioSpec want;
    SDL_AudioSpec have;
    SDL_zero(want);
    SDL_zero(have);
    want.freq = kRate;
    want.format = AUDIO_S16SYS;
    want.channels = kChannels;
    want.samples = 1024;
    want.callback = MixCallback;
    sDevice = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (sDevice == 0) {
        return false;
    }
    SDL_PauseAudioDevice(sDevice, 0);
    return true;
}

float MasterVolume() {
    int v = CVarGetInteger(kCvarVolume, 80);
    v = std::max(0, std::min(100, v));
    return static_cast<float>(v) / 100.0f;
}

// skipMs: start this far into the clip (used to make up for the time between two game frames).
void PlayTrack(TrackId id, int skipMs = 0) {
    if (sDevice == 0 || !sClips[id].ok) {
        return;
    }
    const float level = kTrackLevel[id] * MasterVolume();
    size_t startPos = static_cast<size_t>(std::max(0, skipMs)) * static_cast<size_t>(kRate * kChannels) / 1000;
    startPos &= ~static_cast<size_t>(1); // stay on a stereo frame boundary
    SDL_LockAudioDevice(sDevice);
    Track& tr = sTracks[id];
    tr.clip = &sClips[id];
    tr.pos = std::min(startPos, sClips[id].pcm.size());
    tr.gain = level;
    tr.target = level;
    tr.step = 0.0f;
    tr.playing = true;
    SDL_UnlockAudioDevice(sDevice);
}

void FadeTrack(TrackId id, float seconds) {
    if (sDevice == 0) {
        return;
    }
    SDL_LockAudioDevice(sDevice);
    Track& tr = sTracks[id];
    if (tr.playing) {
        if (seconds <= 0.0f || tr.gain <= 0.0f) {
            tr.playing = false;
        } else {
            tr.target = 0.0f;
            tr.step = tr.gain / (seconds * static_cast<float>(kRate * kChannels));
        }
    }
    SDL_UnlockAudioDevice(sDevice);
}

bool TrackPlaying(TrackId id) {
    if (sDevice == 0) {
        return false;
    }
    SDL_LockAudioDevice(sDevice);
    const bool playing = sTracks[id].playing;
    SDL_UnlockAudioDevice(sDevice);
    return playing;
}

int TrackMs(TrackId id) {
    if (sDevice == 0) {
        return 0;
    }
    SDL_LockAudioDevice(sDevice);
    const size_t pos = sTracks[id].pos;
    SDL_UnlockAudioDevice(sDevice);
    return static_cast<int>(pos * 1000 / static_cast<size_t>(kRate * kChannels));
}

int ClipMs(const Clip& clip) {
    return static_cast<int>(clip.pcm.size() * 1000 / static_cast<size_t>(kRate * kChannels));
}

std::shared_ptr<Ship::File> LoadModFile(const std::string& path) {
    Ship::Context* ctx = Ship::Context::GetRawInstance();
    if (ctx == nullptr) {
        return nullptr;
    }
    auto resourceManager = ctx->GetResourceManager();
    if (resourceManager == nullptr) {
        return nullptr;
    }
    auto archiveManager = resourceManager->GetArchiveManager();
    if (archiveManager == nullptr || !archiveManager->HasFile(path)) {
        return nullptr;
    }
    auto file = archiveManager->LoadFile(path);
    if (file == nullptr || file->Buffer == nullptr || file->Buffer->empty()) {
        return nullptr;
    }
    return file;
}

// Reads a .wav of any common format and converts it to 44.1 kHz stereo 16 bit.
bool DecodeWav(const std::vector<char>& bytes, Clip& clip) {
    SDL_RWops* rw = SDL_RWFromConstMem(bytes.data(), static_cast<int>(bytes.size()));
    if (rw == nullptr) {
        return false;
    }
    SDL_AudioSpec spec;
    Uint8* data = nullptr;
    Uint32 length = 0;
    if (SDL_LoadWAV_RW(rw, 1, &spec, &data, &length) == nullptr) {
        return false;
    }

    SDL_AudioCVT cvt;
    const int needed = SDL_BuildAudioCVT(&cvt, spec.format, spec.channels, spec.freq, AUDIO_S16SYS, kChannels, kRate);
    bool ok = false;
    if (needed == 0) {
        clip.pcm.assign(reinterpret_cast<int16_t*>(data), reinterpret_cast<int16_t*>(data) + length / 2);
        ok = true;
    } else if (needed > 0) {
        cvt.len = static_cast<int>(length);
        cvt.buf = static_cast<Uint8*>(SDL_malloc(static_cast<size_t>(cvt.len) * static_cast<size_t>(cvt.len_mult)));
        if (cvt.buf != nullptr) {
            std::memcpy(cvt.buf, data, length);
            if (SDL_ConvertAudio(&cvt) == 0) {
                clip.pcm.assign(reinterpret_cast<int16_t*>(cvt.buf),
                                reinterpret_cast<int16_t*>(cvt.buf) + cvt.len_cvt / 2);
                ok = true;
            }
            SDL_free(cvt.buf);
        }
    }
    SDL_FreeWAV(data);
    clip.ok = ok && !clip.pcm.empty();
    return clip.ok;
}

// ------------------------------------------------------------------------------------------------
// The pose animation (SA2 animation 180, already converted to Link's skeleton, 20 frames per second).
// One frame = 22 Vec3s (root position, then 21 limb rotations) + 1 face value, 67 big-endian s16.
// ------------------------------------------------------------------------------------------------

constexpr int kJoints = 22;
constexpr int kShortsPerFrame = kJoints * 3 + 1;

struct AnimData {
    std::vector<int16_t> data;
    int frames = 0;
};

AnimData sAnimAdult;
AnimData sAnimChild;

void LoadAnim(const char* path, AnimData& out) {
    auto file = LoadModFile(path);
    if (file == nullptr) {
        return;
    }
    const std::vector<char>& bytes = *file->Buffer;
    const size_t frameBytes = static_cast<size_t>(kShortsPerFrame) * 2;
    const size_t frames = bytes.size() / frameBytes;
    if (frames == 0) {
        return;
    }
    out.data.resize(frames * static_cast<size_t>(kShortsPerFrame));
    for (size_t i = 0; i < out.data.size(); i++) {
        out.data[i] = static_cast<int16_t>((static_cast<uint8_t>(bytes[i * 2]) << 8) |
                                           static_cast<uint8_t>(bytes[i * 2 + 1]));
    }
    out.frames = static_cast<int>(frames);
}

const AnimData& CurrentAnim() {
    return gSaveContext.linkAge == LINK_AGE_CHILD ? sAnimChild : sAnimAdult;
}

void ParseConfig(const std::string& text) {
    std::istringstream lines(text);
    std::string line;
    while (std::getline(lines, line)) {
        const size_t eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        const std::string key = line.substr(0, eq);
        const int value = std::atoi(line.c_str() + eq + 1);
        if (key == "drop_ms") {
            sDropMs = value;
        } else if (key == "control_ms") {
            sControlMs = value;
        } else if (key == "voice_start_ms") {
            sVoiceStartMs = value;
        } else if (key == "anim_control_frame") {
            sAnimControlFrame = value;
        } else if (key == "anim_source_fps" && value > 0) {
            sAnimSourceFps = value;
        }
    }
}

void LoadAssets() {
    if (sAssetsTried) {
        return;
    }
    sAssetsTried = true;

    const char* files[kTrackCount] = { kFileMusic, kFileVoice, kFileEnd };
    for (int i = 0; i < kTrackCount; i++) {
        auto file = LoadModFile(files[i]);
        if (file != nullptr) {
            DecodeWav(*file->Buffer, sClips[i]);
        }
    }
    auto config = LoadModFile(kFileConfig);
    if (config != nullptr) {
        ParseConfig(std::string(config->Buffer->begin(), config->Buffer->end()));
    }
    if (sDropMs <= 0 && sClips[kTrackVoice].ok) {
        sDropMs = ClipMs(sClips[kTrackVoice]);
    }
    if (sVoiceStartMs < 0 && sClips[kTrackVoice].ok) {
        sVoiceStartMs = std::max(0, sDropMs - ClipMs(sClips[kTrackVoice]));
    }
    LoadAnim(kFileAnimAdult, sAnimAdult);
    LoadAnim(kFileAnimChild, sAnimChild);
}

// ------------------------------------------------------------------------------------------------
// Chaos Control itself
// ------------------------------------------------------------------------------------------------

enum class State { Idle, Windup, Active };

State sState = State::Idle;
std::vector<Actor*> sThawed; // frozen enemies that were let through for one frame to take a hit
int sActiveFrames = 0;      // frames of freeze left
int sActiveTotal = 1;       // frames of freeze in total (for the bar)
int sEndCueFrames = 0;      // the ending sound starts when sActiveFrames drops to this value
bool sEndCuePlayed = false;
int sCooldownFrames = 0;
int sCooldownTotal = 1;
int sWindupFrames = 0;      // frames since the start (the frame of the pose)
int sPoseTick = 0;          // frame number of the pose that is shown
bool sJustStarted = false;
bool sMusicStarted = false;
bool sVoicePlayed = false;
int sMusicStartMs = 0;      // time after the start when the music starts
int sAnimDelayFrames = 0;   // frames the pose waits before it starts (only when the music has to start first)
bool sInputFlagSet = false; // we set PLAYER_STATE1_INPUT_DISABLED for this frame and have to clear it again
s16 sStartHealth = 0;
int sBlendOut = 0;
bool sHavePose = false;     // sLastPose holds a pose that was shown
Vec3s sLastPose[kJoints];
float sBarAlpha = 0.0f;

bool SoundOn() {
    return CVarGetInteger(kCvarSound, 1) != 0;
}

void PlayGameCue(u16 sfxId) {
    if (SoundOn()) {
        Sfx_PlaySfxCentered(sfxId);
    }
}

// ----- The item ---------------------------------------------------------------------------------

// Roc's Feather's item cycle in the pause menu (shared with Nayru's Love) is only drawn when this randomizer option
// is on. Outside randomizer saves nothing else depends on it, so switch it on for Chaos Control.
void EnsureItemSetup() {
    if (!CVarGetInteger(kCvarEnabled, 1) || !CVarGetInteger(kCvarItem, 1) || IS_RANDO) {
        return;
    }
    Rando::Context::GetInstance()->GetOption(RSK_ROCS_FEATHER).Set(1);
    Flags_SetRandomizerInf(RAND_INF_OBTAINED_ROCS_FEATHER);
    if (INV_CONTENT(ITEM_NAYRUS_LOVE) == ITEM_NAYRUS_LOVE) {
        Flags_SetRandomizerInf(RAND_INF_OBTAINED_NAYRUS_LOVE); // so the slot offers both items
    } else if (INV_CONTENT(ITEM_NAYRUS_LOVE) == ITEM_NONE) {
        INV_CONTENT(ITEM_NAYRUS_LOVE) = ITEM_ROCS_FEATHER;
    }
}

// ----- State machine ----------------------------------------------------------------------------

int AnimControlMs() {
    return sAnimControlFrame * 1000 / std::max(1, sAnimSourceFps);
}

int FreezeFrames() {
    const int seconds = std::max(1, CVarGetInteger(kCvarDuration, 6));
    return seconds * kFramesPerSecond;
}

int ReleaseFrames() {
    const int ms = std::max(0, CVarGetInteger(kCvarRelease, 600));
    return (ms + kMsPerFrame - 1) / kMsPerFrame;
}

void EnterActive() {
    const int extra = ReleaseFrames();
    sState = State::Active;
    sEndCueFrames = extra;
    sActiveFrames = FreezeFrames() + extra;
    sActiveTotal = sActiveFrames;
    sEndCuePlayed = false;
    sBlendOut = kBlendOutFrames; // the pose fades out into Link's normal pose
}

void BeginChaosControl() {
    sVoicePlayed = false;
    sMusicStarted = false;
    sWindupFrames = 1; // the frame the item is used in is frame 0
    sPoseTick = 0;
    sMusicStartMs = 0;
    sAnimDelayFrames = 0;
    sBlendOut = 0;
    sHavePose = false;
    sJustStarted = true;
    sStartHealth = gSaveContext.health;

    if (SoundOn()) {
        LoadAssets();
    }
    const bool haveMusic = SoundOn() && sClips[kTrackMusic].ok && EnsureDevice();
    const bool havePose = CVarGetInteger(kCvarAnimation, 1) != 0 && CurrentAnim().frames > 0;

    if (!haveMusic && !havePose) {
        // Nothing to show: start right away with a game sound.
        PlayGameCue(NA_SE_PL_MAGIC_WIND_WARP);
        EnterActive();
        sBlendOut = 0;
        return;
    }

    if (haveMusic) {
        // The music has to start so that the word CONTROL is shouted exactly on frame 50 of the pose.
        int musicStart = 0;
        if (havePose && sControlMs >= 0) {
            musicStart = AnimControlMs() - sControlMs + CVarGetInteger(kCvarSyncOffset, 0);
        }
        if (musicStart >= 0) {
            sMusicStartMs = musicStart;
        } else {
            // CONTROL comes earlier in the music than in the pose: the pose has to wait instead.
            sMusicStartMs = 0;
            sAnimDelayFrames = (-musicStart + kMsPerFrame - 1) / kMsPerFrame;
        }
    }
    sState = State::Windup;
}

void TickWindup() {
    const int t = sWindupFrames;
    const int ms = t * kMsPerFrame;
    const bool haveMusic = SoundOn() && sClips[kTrackMusic].ok && sDevice != 0;

    if (haveMusic && !sMusicStarted && ms >= sMusicStartMs) {
        PlayTrack(kTrackMusic, ms - sMusicStartMs);
        sMusicStarted = true;
    }
    if (sMusicStarted && !sVoicePlayed && sClips[kTrackVoice].ok && TrackMs(kTrackMusic) >= sVoiceStartMs) {
        PlayTrack(kTrackVoice);
        sVoicePlayed = true;
    }

    sPoseTick = std::max(0, t - sAnimDelayFrames);
    sWindupFrames++;

    bool go = false;
    if (sMusicStarted) {
        const int dropMs = sDropMs + CVarGetInteger(kCvarDropOffset, 0);
        go = TrackMs(kTrackMusic) >= dropMs || !TrackPlaying(kTrackMusic);
    } else if (!haveMusic) {
        // No music: wait until the pose is finished.
        const AnimData& anim = CurrentAnim();
        go = anim.frames <= 0 || t - sAnimDelayFrames >= anim.frames - 1;
    }
    if (go || sWindupFrames > kMaxWindupFrames) {
        EnterActive();
    }
}

// The ending sound and the music fade-out. The time stays stopped a little longer (ReleaseDelay).
void PlayEndCue() {
    if (sEndCuePlayed) {
        return;
    }
    sEndCuePlayed = true;
    FadeTrack(kTrackVoice, 0.2f);
    FadeTrack(kTrackMusic, 1.2f);
    if (SoundOn()) {
        if (sClips[kTrackEnd].ok && sDevice != 0) {
            PlayTrack(kTrackEnd);
        } else {
            PlayGameCue(NA_SE_PL_MAGIC_WIND_VANISH);
        }
    }
}

// normalEnd: the time ran out. Otherwise something interrupted it.
void StopChaosControl(bool normalEnd) {
    if (sState == State::Idle) {
        return;
    }
    const bool wasActive = (sState == State::Active);
    if (normalEnd) {
        PlayEndCue();
    } else {
        FadeTrack(kTrackVoice, 0.2f);
        FadeTrack(kTrackMusic, 0.3f);
    }
    sState = State::Idle;
    sActiveFrames = 0;
    sWindupFrames = 0;
    sJustStarted = false;
    sThawed.clear();

    if (wasActive) {
        int seconds = CVarGetInteger(kCvarCooldown, 10);
        if (seconds < 0) {
            seconds = 0;
        }
        sCooldownFrames = seconds * kFramesPerSecond;
        sCooldownTotal = std::max(1, sCooldownFrames);
    } else {
        sBlendOut = kBlendOutFrames; // interrupted during the start: the pose fades out
    }
}

void ResetChaosControl() {
    if (sState != State::Idle) {
        StopChaosControl(false);
    }
    sCooldownFrames = 0;
    sBlendOut = 0;
    sBarAlpha = 0.0f;
}

// True when Link is not in control or something bigger is going on (cutscenes, talking, dying, scene change).
bool PlayerBusy(Player* player) {
    if (gPlayState->csCtx.state != CS_STATE_IDLE || Player_InCsMode(gPlayState)) {
        return true;
    }
    if (gPlayState->pauseCtx.state != 0 || gPlayState->transitionTrigger != TRANS_TRIGGER_OFF) {
        return true;
    }
    const u32 busyFlags = PLAYER_STATE1_INPUT_DISABLED | PLAYER_STATE1_TALKING | PLAYER_STATE1_DEAD |
                          PLAYER_STATE1_GETTING_ITEM | PLAYER_STATE1_IN_CUTSCENE;
    if (player->stateFlags1 & busyFlags) {
        return true;
    }
    if (player->stateFlags2 & PLAYER_STATE2_OCARINA_PLAYING) {
        return true;
    }
    return false;
}

// Chaos Control can only start while standing on the ground and in control (not swimming, climbing, riding...).
bool CanStart(Player* player) {
    if (PlayerBusy(player)) {
        return false;
    }
    const u32 blocked1 = PLAYER_STATE1_IN_WATER | PLAYER_STATE1_ON_HORSE | PLAYER_STATE1_CLIMBING_LEDGE |
                         PLAYER_STATE1_HANGING_OFF_LEDGE | PLAYER_STATE1_CLIMBING_LADDER | PLAYER_STATE1_DAMAGED |
                         PLAYER_STATE1_FIRST_PERSON | PLAYER_STATE1_CARRYING_ACTOR | PLAYER_STATE1_FREEFALL |
                         PLAYER_STATE1_JUMPING | PLAYER_STATE1_USING_BOOMERANG;
    const u32 blocked2 = PLAYER_STATE2_UNDERWATER | PLAYER_STATE2_DIVING | PLAYER_STATE2_CRAWLING |
                         PLAYER_STATE2_GRABBED_BY_ENEMY | PLAYER_STATE2_FROZEN;
    if ((player->stateFlags1 & blocked1) || (player->stateFlags2 & blocked2)) {
        return false;
    }
    return (player->actor.bgCheckFlags & 1) != 0; // on the ground
}

// Called when the player presses the button the item is on, or the extra button.
void RequestChaosControl(Player* player) {
    if (!CVarGetInteger(kCvarEnabled, 1) || sState != State::Idle) {
        return;
    }
    if (sCooldownFrames > 0 || !CanStart(player)) {
        Sfx_PlaySfxCentered(NA_SE_SY_ERROR);
        return;
    }
    BeginChaosControl();
}

void UpdateBarFade() {
    const bool show = CVarGetInteger(kCvarBar, 1) != 0 &&
                      (sState == State::Active || (sCooldownFrames > 0 && CVarGetInteger(kCvarBarCooldown, 1) != 0));
    if (show) {
        sBarAlpha = std::min(255.0f, sBarAlpha + 51.0f);
    } else {
        sBarAlpha = std::max(0.0f, sBarAlpha - 25.0f);
    }
}

void OnPlayerUpdateChaosControl() {
    // Undo the input lock that was set for this frame's player update.
    if (gPlayState != nullptr && sInputFlagSet) {
        GET_PLAYER(gPlayState)->stateFlags1 &= ~PLAYER_STATE1_INPUT_DISABLED;
    }
    sInputFlagSet = false;

    if (gPlayState == nullptr || !CVarGetInteger(kCvarEnabled, 1)) {
        ResetChaosControl();
        return;
    }

    Player* player = GET_PLAYER(gPlayState);
    if (player == nullptr) {
        return;
    }

    EnsureItemSetup();

    if (sBlendOut > 0 && sState != State::Windup && !sJustStarted) {
        sBlendOut--;
    }
    UpdateBarFade();

    switch (sState) {
        case State::Active:
            if (PlayerBusy(player)) {
                StopChaosControl(false);
            } else {
                if (!sEndCuePlayed && sActiveFrames <= sEndCueFrames) {
                    PlayEndCue();
                }
                if (--sActiveFrames <= 0) {
                    StopChaosControl(true);
                }
            }
            return;
        case State::Windup:
            if (sJustStarted) {
                // The frame the item was used in: the lock and the movement stop start with the next frame.
                sJustStarted = false;
                return;
            }
            if (PlayerBusy(player) || gSaveContext.health < sStartHealth) {
                StopChaosControl(false); // cutscene, talking, or Link got hurt
            } else {
                // Hold Link still while the pose plays.
                player->linearVelocity = 0.0f;
                player->actor.speedXZ = 0.0f;
                player->actor.velocity.x = 0.0f;
                player->actor.velocity.z = 0.0f;
                TickWindup();
            }
            return;
        case State::Idle:
            break;
    }

    if (sCooldownFrames > 0) {
        sCooldownFrames--;
    }

    // Optional extra button (off by default).
    const int button = CVarGetInteger(kCvarButton, 0);
    if (button != 0 && CHECK_BTN_ANY(gPlayState->state.input[0].press.button, button)) {
        RequestChaosControl(player);
        sJustStarted = false; // started at the end of this frame already
    }
}

// Before every actor update: the Player gets the game's own "input disabled" flag while the pose plays.
// With that flag the game ignores the whole controller for Link, so he cannot walk, roll, attack, use
// items, talk or be steered. The flag is taken off again at the end of the player update.
// Also: which actors does time stop for?
bool IsFrozenByChaosControl(Actor* actor, Player* player) {
    if (actor == &player->actor || actor == player->heldActor || actor == player->naviActor) {
        return false;
    }
    switch (actor->category) {
        case ACTORCAT_ENEMY:
        case ACTORCAT_BOSS:
        case ACTORCAT_EXPLOSIVE: // bombs, bombchus, bomb flowers
            return true;
        default:
            break;
    }
    // Deku scrub nuts and similar enemy projectiles that are not in the enemy category.
    return actor->id == ACTOR_EN_NUTSBALL;
}

// ----- Hitting frozen enemies -------------------------------------------------------------------
// The game only lets an actor take part in hit tests while the actor itself is updating, and a frozen enemy does not
// update. So while it is frozen we do the one thing its update would do for hits: we put its colliders on the list of
// things that can be hit. When something hits it, the enemy gets one normal update (it takes the damage and starts
// its reaction), then it is frozen again, so it holds that reaction until time moves on.

constexpr int kMaxHitColliders = 24;
// An enemy's colliders sit inside its own struct, and each one starts with a pointer back to the enemy. This finds
// them by looking for that pointer, and checks the data after it to be sure it is a collider. With onlyHittable, only
// colliders that can be hit are returned.
int FindColliders(Actor* actor, Collider** out, int maxOut, bool onlyHittable) {
    if (ActorDB::Instance == nullptr) {
        return 0;
    }
    const size_t size = ActorDB::Instance->RetrieveEntry(actor->id).entry.instanceSize;
    uint8_t* base = reinterpret_cast<uint8_t*>(actor);
    int count = 0;
    for (size_t off = sizeof(Actor); off + sizeof(Collider) <= size && count < maxOut; off += alignof(Collider)) {
        Collider* c = reinterpret_cast<Collider*>(base + off);
        if (c->actor != actor || c->shape > COLSHAPE_QUAD) {
            continue;
        }
        if (c->shape == COLSHAPE_JNTSPH) {
            const ColliderJntSph* sph = reinterpret_cast<const ColliderJntSph*>(c);
            if (sph->count <= 0 || sph->count > 64 || sph->elements == nullptr) {
                continue;
            }
        }
        if (onlyHittable && (!(c->acFlags & AC_ON) || !(c->acFlags & AC_TYPE_ALL))) {
            continue;
        }
        out[count++] = c;
    }
    return count;
}

// Called for a frozen enemy. True = it was hit and should run its update this frame. Otherwise its colliders are put
// on the hit list for this frame.
bool LetEnemyTakeHit(Actor* actor) {
    Collider* colliders[kMaxHitColliders];
    const int count = FindColliders(actor, colliders, kMaxHitColliders, true);
    if (count == 0) {
        return false;
    }
    for (int i = 0; i < count; i++) {
        if (colliders[i]->acFlags & AC_HIT) {
            sThawed.push_back(actor);
            return true;
        }
    }
    for (int i = 0; i < count; i++) {
        CollisionCheck_SetAC(gPlayState, &gPlayState->colChkCtx, colliders[i]);
    }
    return false;
}

// After the enemy took its hit: make sure the hit is not seen a second time.
void OnActorUpdateChaosControl(void* refActor) {
    if (sThawed.empty()) {
        return;
    }
    Actor* actor = static_cast<Actor*>(refActor);
    const auto it = std::find(sThawed.begin(), sThawed.end(), actor);
    if (it == sThawed.end()) {
        return;
    }
    sThawed.erase(it);
    Collider* colliders[kMaxHitColliders];
    const int count = FindColliders(actor, colliders, kMaxHitColliders, false);
    for (int i = 0; i < count; i++) {
        colliders[i]->acFlags &= ~AC_HIT;
        colliders[i]->ac = nullptr;
    }
}

void ShouldActorUpdateChaosControl(void* refActor, bool* result) {
    if (gPlayState == nullptr || refActor == nullptr) {
        return;
    }
    Player* player = GET_PLAYER(gPlayState);
    if (player == nullptr) {
        return;
    }
    Actor* actor = static_cast<Actor*>(refActor);

    if (sState == State::Windup && !sJustStarted && CVarGetInteger(kCvarLock, 1) && actor == &player->actor) {
        if (!(player->stateFlags1 & PLAYER_STATE1_INPUT_DISABLED)) {
            player->stateFlags1 |= PLAYER_STATE1_INPUT_DISABLED;
            sInputFlagSet = true;
        }
        return;
    }

    // Time only stops after the beat drop (State::Active), not during the build-up.
    if (sState == State::Active && IsFrozenByChaosControl(actor, player)) {
        if (actor->category == ACTORCAT_ENEMY && CVarGetInteger(kCvarHitEnemies, 1) != 0 && LetEnemyTakeHit(actor)) {
            return; // it was hit: it updates this one frame
        }
        *result = false;
    }
}

// ----- The pose ---------------------------------------------------------------------------------

inline int16_t LerpS16(int16_t a, int16_t b, float w) {
    return static_cast<int16_t>(a + static_cast<int>(std::lround(static_cast<float>(b - a) * w)));
}

// Rotations: blend the short way around the circle.
inline int16_t LerpAngle(int16_t a, int16_t b, float w) {
    const int16_t diff = static_cast<int16_t>(b - a);
    return static_cast<int16_t>(a + static_cast<int>(std::lround(static_cast<float>(diff) * w)));
}

// Runs right before the game draws the frame, after the game has put Link's normal pose into his joint table.
// The pose of the animation replaces it (blended in and out, so there are no jumps).
void OnPlayDrawBeginChaosControl() {
    if (gPlayState == nullptr) {
        return;
    }
    const bool posing = sState == State::Windup && !sJustStarted && CVarGetInteger(kCvarAnimation, 1) != 0 &&
                        CurrentAnim().frames > 0;
    const bool fadingOut = !posing && sBlendOut > 0 && sHavePose;
    if (!posing && !fadingOut) {
        return;
    }
    Player* player = GET_PLAYER(gPlayState);
    if (player == nullptr || player->skelAnime.jointTable == nullptr) {
        return;
    }

    const int joints = std::min<int>(kJoints, player->skelAnime.limbCount);
    Vec3s target[kJoints];
    float weight;
    if (posing) {
        const AnimData& anim = CurrentAnim();
        const int frame = std::max(0, std::min(sPoseTick, anim.frames - 1));
        const int16_t* src = &anim.data[static_cast<size_t>(frame) * kShortsPerFrame];
        for (int j = 0; j < kJoints; j++) {
            target[j].x = src[j * 3 + 0];
            target[j].y = src[j * 3 + 1];
            target[j].z = src[j * 3 + 2];
        }
        // Optional extra height for the whole pose (the pose file itself already keeps the soles above the floor).
        const int lift = CVarGetInteger(kCvarPoseLift, 0);
        if (lift != 0) {
            target[0].y = static_cast<int16_t>(std::max(-32768, std::min(32767, static_cast<int>(target[0].y) + lift)));
        }
        weight = std::min(1.0f, static_cast<float>(sPoseTick + 1) / static_cast<float>(kBlendInFrames));
    } else {
        std::memcpy(target, sLastPose, sizeof(target));
        weight = static_cast<float>(sBlendOut) / static_cast<float>(kBlendOutFrames);
    }

    Vec3s* table = player->skelAnime.jointTable;
    for (int j = 0; j < joints; j++) {
        Vec3s out;
        if (j == 0) { // root position
            out.x = LerpS16(table[j].x, target[j].x, weight);
            out.y = LerpS16(table[j].y, target[j].y, weight);
            out.z = LerpS16(table[j].z, target[j].z, weight);
        } else { // limb rotations
            out.x = LerpAngle(table[j].x, target[j].x, weight);
            out.y = LerpAngle(table[j].y, target[j].y, weight);
            out.z = LerpAngle(table[j].z, target[j].z, weight);
        }
        table[j] = out;
        if (posing) {
            sLastPose[j] = out;
        }
    }
    if (posing) {
        sHavePose = true;
    }
}

} // namespace

// Used by Jump.cpp and Boost.cpp: true only while the start pose plays (Link is held still), so Y and X cannot break
// out of the pose. Once time has stopped (State::Active) Link is free, so he can jump, double jump and boost.
extern "C" bool ShadowChaosControl_IsBusy(void) {
    return sState == State::Windup;
}

// Read by the status report in Jump.cpp: tells it that this file is in the build, and which version it is.
// 2 = the version where Y and X work once time has stopped.
extern "C" {
int ShadowAbilityVersion_ChaosControl = 2;
}

// ----- The duration bar ---------------------------------------------------------------------------
// Drawn with the same textures and the same drawing steps as the magic meter (Interface_DrawMagicBar), so it
// looks like it belongs to the HUD.

alignas(2) static const char kTexMagicEnd[] = "__OTR__textures/parameter_static/gMagicMeterEndTex";
alignas(2) static const char kTexMagicMid[] = "__OTR__textures/parameter_static/gMagicMeterMidTex";
alignas(2) static const char kTexMagicFill[] = "__OTR__textures/parameter_static/gMagicMeterFillTex";

static constexpr int kBarLength = 96; // same as the big magic meter

static Gfx* TextureIA8(Gfx* dl, const char* tex, s32 texW, s32 texH, s16 left, s16 top, s16 width, s16 height, u16 dsdx,
                u16 dtdy) {
    gDPLoadTextureBlock(dl++, tex, G_IM_FMT_IA, G_IM_SIZ_8b, texW, texH, 0, G_TX_NOMIRROR | G_TX_WRAP,
                        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gSPWideTextureRectangle(dl++, left << 2, top << 2, (left + width) << 2, (top + height) << 2, G_TX_RENDERTILE, 0, 0,
                            dsdx, dtdy);
    return dl;
}

static void DrawBar(float fraction, u8 r, u8 g, u8 b, u8 alpha) {
    GraphicsContext* gfxCtx = gPlayState->state.gfxCtx;
    const s16 y = static_cast<s16>(CVarGetInteger(kCvarBarY, 56));
    const s16 barX = OTRGetRectDimensionFromLeftEdge(static_cast<float>(CVarGetInteger(kCvarBarX, 18)));
    const s16 fillX = barX + 8;
    const s16 filled = static_cast<s16>(std::lround(std::max(0.0f, std::min(1.0f, fraction)) * kBarLength));

    OPEN_DISPS(gfxCtx);

    Gfx_SetupDL_39Overlay(gfxCtx);

    // Border: left end, middle, right end (the left end mirrored).
    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 255, 255, 255, alpha);
    gDPSetEnvColor(OVERLAY_DISP++, 100, 50, 50, 255);

    OVERLAY_DISP = TextureIA8(OVERLAY_DISP, kTexMagicEnd, 8, 16, barX, y, 8, 16, 1 << 10, 1 << 10);
    OVERLAY_DISP = TextureIA8(OVERLAY_DISP, kTexMagicMid, 24, 16, fillX, y, kBarLength, 16, 1 << 10, 1 << 10);

    gDPLoadTextureBlock(OVERLAY_DISP++, kTexMagicEnd, G_IM_FMT_IA, G_IM_SIZ_8b, 8, 16, 0, G_TX_MIRROR | G_TX_WRAP,
                        G_TX_NOMIRROR | G_TX_WRAP, 3, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gSPWideTextureRectangle(OVERLAY_DISP++, ((barX + kBarLength) + 8) << 2, y << 2, ((barX + kBarLength) + 16) << 2,
                            (y + 16) << 2, G_TX_RENDERTILE, 256, 0, 1 << 10, 1 << 10);

    // Fill.
    gDPPipeSync(OVERLAY_DISP++);
    gDPSetCombineLERP(OVERLAY_DISP++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, 0, 0, 0, PRIMITIVE, PRIMITIVE,
                      ENVIRONMENT, TEXEL0, ENVIRONMENT, 0, 0, 0, PRIMITIVE);
    gDPSetEnvColor(OVERLAY_DISP++, 0, 0, 0, 255);
    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, r, g, b, alpha);

    gDPLoadMultiBlock_4b(OVERLAY_DISP++, kTexMagicFill, 0, G_TX_RENDERTILE, G_IM_FMT_I, 16, 16, 0,
                         G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
                         G_TX_NOLOD);
    if (filled > 0) {
        gSPWideTextureRectangle(OVERLAY_DISP++, fillX << 2, (y + 3) << 2, (fillX + filled) << 2, (y + 10) << 2,
                                G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
    }
    gDPPipeSync(OVERLAY_DISP++);

    CLOSE_DISPS(gfxCtx);
}

// Runs while the world is drawn, before the HUD (so the HUD is drawn on top, and nothing is drawn in the pause menu).
static void OnPlayDrawEndChaosControl() {
    if (gPlayState == nullptr || sBarAlpha <= 0.0f || gSaveContext.gameMode != GAMEMODE_NORMAL) {
        return;
    }
    const u8 alpha = static_cast<u8>(sBarAlpha);
    if (sState == State::Active) {
        const float fraction = static_cast<float>(sActiveFrames) / static_cast<float>(std::max(1, sActiveTotal));
        // Shadow's red; it blinks in the last second.
        const bool blink = sActiveFrames < kFramesPerSecond && ((sActiveFrames / 3) % 2) == 0;
        DrawBar(fraction, blink ? 255 : 235, blink ? 160 : 30, blink ? 160 : 40, alpha);
    } else if (sCooldownFrames > 0) {
        const float fraction = 1.0f - static_cast<float>(sCooldownFrames) / static_cast<float>(std::max(1, sCooldownTotal));
        DrawBar(fraction, 90, 100, 130, static_cast<u8>(sBarAlpha * 0.7f)); // greyed out while recharging
    } else {
        DrawBar(0.0f, 90, 100, 130, alpha); // fading out
    }
}

static void RegisterChaosControl() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayerUpdate>(OnPlayerUpdateChaosControl);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::ShouldActorUpdate>(ShouldActorUpdateChaosControl);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnActorUpdate>(OnActorUpdateChaosControl);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayDrawBegin>(OnPlayDrawBeginChaosControl);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayDrawEnd>(OnPlayDrawEndChaosControl);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneInit>(
        [](int16_t) { ResetChaosControl(); });
    // The pause menu does not run player updates, so end Chaos Control (and its music) when it opens.
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnKaleidoscopeUpdate>(
        [](int16_t) { StopChaosControl(false); sBlendOut = 0; sBarAlpha = 0.0f; });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnLoadGame>([](int32_t) {
        ResetChaosControl();
        if (SoundOn()) {
            LoadAssets();
        }
    });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnExitGame>(
        [](int32_t) { ResetChaosControl(); });

    // Using the item: the C button it is on. This replaces Roc's Feather's jump.
    REGISTER_VB_SHOULD(VB_OVERRIDE_BUTTON_ITEM_USED, {
        (void)should;
        (void)va_arg(args, s32*); // button index
        Player* player = va_arg(args, Player*);
        s32* item = va_arg(args, s32*);
        if (*item == ITEM_ROCS_FEATHER && !IS_RANDO && CVarGetInteger(kCvarItem, 1) &&
            CVarGetInteger(kCvarEnabled, 1)) {
            *item = ITEM_NONE_FE; // the game then does nothing else with this button press
            RequestChaosControl(player);
        }
    });
}

static RegisterShipInitFunc sChaosControlInit(RegisterChaosControl);
