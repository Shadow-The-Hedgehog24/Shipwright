// Shadow the Hedgehog - Chaos Control for Ship of Harkinian 9.3.1  (version 4)
//
// Version 4: Chaos Control can be started with the boomerang in hand (it used to refuse while the boomerang was the
// item in hand). Changed with gShadow.ChaosControl.ItemsInHand (default 1).
//
// Version 3: time stays stopped for 30 seconds (the bar keeps its size and empties more slowly), and while time is
// stopped the world is greyscale (the HUD and Shadow keep their colour), Shadow leaves red see-through after images
// behind him and a soft red aura pulses around him whenever a new one appears, and every enemy that gets hit while it
// is frozen leaves a red see-through copy of its body where it was when the hit landed. (The same look as the boost's
// blue one in Boost.cpp, in red.)
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
//   gShadow.ChaosControl.Duration     seconds time stays stopped after the beat drop, default 30 (was 6 up to version 2;
//                                     if you set this number yourself earlier, your number still wins)
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
//   gShadow.ChaosControl.ItemsInHand  1 = it works with the boomerang in hand (default), 0 = it refuses while the boomerang is
//                                     the item in hand (the rule up to version 3). Aiming with the item button held (first-
//                                     person view) still blocks it
//   gShadow.ChaosControl.PoseLift     extra height added to the pose in model units (100 = 1 game unit), default 0.
//                                     The pose file already keeps the feet above the floor; use this only to fine-tune.
//   gShadow.ChaosControl.Lock         1 = lock the player during the start (default), 0 = no lock
//   gShadow.ChaosControl.Bar          1 = show the duration bar (default), 0 = hide it
//   gShadow.ChaosControl.BarX/BarY    bar position, default 18 / 56 (320x240 screen units, from the left edge)
//   gShadow.ChaosControl.BarCooldown  1 = the bar refills during the cooldown (default), 0 = hide it then
//   gShadow.ChaosControl.Effect       1 = the red time stop look (greyscale world, red after images, aura) on (default), 0 = off
//   gShadow.ChaosControl.Greyscale    1 = the world turns greyscale while time is stopped (default), 0 = no greyscale
//   gShadow.ChaosControl.Afterimages  1 = red after images behind Shadow (default), 0 = none (needs shadow_ghost.o2r)
//   gShadow.ChaosControl.AfterimageInterval  game frames between two after images, default 2
//   gShadow.ChaosControl.AfterimageLife      game frames an after image lasts, default 10
//   gShadow.ChaosControl.AfterimageOpacity   opacity of a new after image in percent, default 55
//   gShadow.ChaosControl.Aura         1 = the soft red aura around Shadow (default), 0 = none
//   gShadow.ChaosControl.AuraStrength aura strength in percent, default 100
//   gShadow.ChaosControl.HitAfterimages      1 = a red see-through copy of an enemy where it was hit (default), 0 = none
//   gShadow.ChaosControl.HitAfterimageLife   game frames such a copy lasts, default 16
//   gShadow.ChaosControl.HitAfterimageOpacity opacity of a new one in percent, default 55

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
// Finds a skeleton resource by its path (the same call the game makes when it starts a skeleton). nullptr if missing.
SkeletonHeader* ResourceMgr_LoadSkeletonByName(const char* path, SkelAnime* skelAnime);
// The player's own draw function (the one the game puts in his actor when he is created).
void Player_Draw(Actor* thisx, PlayState* play2);
}

// The red look (further down in this file). These are used by the code in the nameless namespace below.
static void FxOnDrop();
static void FxReset();
static void FxPreload();
static void FxRecordEnemyHit(Actor* actor, Collider** colliders, int count);

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
constexpr const char* kCvarItemsInHand = "gShadow.ChaosControl.ItemsInHand";
constexpr const char* kCvarLock = "gShadow.ChaosControl.Lock";
constexpr const char* kCvarBar = "gShadow.ChaosControl.Bar";
constexpr const char* kCvarBarX = "gShadow.ChaosControl.BarX";
constexpr const char* kCvarBarY = "gShadow.ChaosControl.BarY";
constexpr const char* kCvarBarCooldown = "gShadow.ChaosControl.BarCooldown";
constexpr const char* kCvarFx = "gShadow.ChaosControl.Effect";
constexpr const char* kCvarFxGrey = "gShadow.ChaosControl.Greyscale";
constexpr const char* kCvarFxAfter = "gShadow.ChaosControl.Afterimages";
constexpr const char* kCvarFxInterval = "gShadow.ChaosControl.AfterimageInterval";
constexpr const char* kCvarFxLife = "gShadow.ChaosControl.AfterimageLife";
constexpr const char* kCvarFxOpacity = "gShadow.ChaosControl.AfterimageOpacity";
constexpr const char* kCvarFxAura = "gShadow.ChaosControl.Aura";
constexpr const char* kCvarFxAuraStrength = "gShadow.ChaosControl.AuraStrength";
constexpr const char* kCvarFxHit = "gShadow.ChaosControl.HitAfterimages";
constexpr const char* kCvarFxHitLife = "gShadow.ChaosControl.HitAfterimageLife";
constexpr const char* kCvarFxHitOpacity = "gShadow.ChaosControl.HitAfterimageOpacity";

// The see-through copy of Shadow's model for the after images (shadow_ghost.o2r, the same file the boost uses).
constexpr const char* kGhostSkelAdult = "objects/object_link_boy/gLinkAdultGhostSkel";
constexpr const char* kGhostSkelChild = "objects/object_link_child/gLinkChildGhostSkel";

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
    const int seconds = std::max(1, CVarGetInteger(kCvarDuration, 30));
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
    ::FxOnDrop();                // the greyscale, the flash and the ring start with the freeze
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
    ::FxPreload(); // loads the after image model now, so the first after image does not make the game hitch

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
    ::FxReset();
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
                         PLAYER_STATE1_JUMPING;
    const u32 blocked2 = PLAYER_STATE2_UNDERWATER | PLAYER_STATE2_DIVING | PLAYER_STATE2_CRAWLING |
                         PLAYER_STATE2_GRABBED_BY_ENEMY | PLAYER_STATE2_FROZEN;
    if ((player->stateFlags1 & blocked1) || (player->stateFlags2 & blocked2)) {
        return false;
    }
    // Items in hand do not stop Chaos Control (version 4). PLAYER_STATE1_USING_BOOMERANG is set the whole time the
    // boomerang is the item in hand (taken out, in flight, being caught), not only while it is being thrown. Aiming
    // with it (the item button held down) puts Link into the first-person view, which stays blocked above.
    // gShadow.ChaosControl.ItemsInHand = 0 brings the old rule back.
    if ((player->stateFlags1 & PLAYER_STATE1_USING_BOOMERANG) && CVarGetInteger(kCvarItemsInHand, 1) == 0) {
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
            // The enemy was struck. Its hit boxes still sit where it stood when the hit landed (it has not had its
            // update yet), so this is the moment to leave the red copy behind.
            ::FxRecordEnemyHit(actor, colliders, count);
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
// 3 = 30 second time stop, greyscale world with red after images and aura, red copies of struck enemies.
// 4 = works with the boomerang (and other items) in hand.
extern "C" {
int ShadowAbilityVersion_ChaosControl = 4;
}

// ----- The red time stop look (version 3) ---------------------------------------------------------
// From the beat drop until time starts again:
//  * The world is greyscale. The HUD and Shadow keep their colour. The renderer has a "grayscale" switch (the one the
//    medallion pause screen uses). It is switched on at the start of the world's display lists and off again at the
//    start of the HUD's. Shadow's own draw is wrapped (the player's draw function is replaced while the look is on) so
//    the switch is off while he is drawn.
//  * Shadow leaves red see-through after images behind him. A copy of his pose is stored every few frames and drawn
//    after the whole world, with the see-through copy of his model from shadow_ghost.o2r (the same file the boost
//    uses). They stay where they are in the world and fade out.
//  * A soft red aura glows around him and pulses whenever a new after image appears. A flash and a ring go out at the
//    drop.
//  * Every enemy that is hit while it is frozen leaves a red see-through copy of its body at the place it stood when
//    the hit landed. The copy is made of the enemy's own hit spheres (the game's hit boxes, which follow its limbs),
//    so it is a body shaped blob of the right size and place, not the enemy's real model. It fades out where it is
//    while the enemy itself flinches a little away.
//
// Boost.cpp has a draw wrapper of its own for the same job. The two have to live together: this file only puts its
// wrapper in when the player's draw function is the game's own one. When Boost's wrapper is already there it keeps
// Shadow in colour for both, and this file leaves it alone. (Putting a second wrapper on top of Boost's would make the
// two call each other forever.)

static constexpr int kFxMaxGhosts = 16;
static constexpr int kFxMaxHitGhosts = 8;
static constexpr int kFxMaxHitSpheres = 24;
static constexpr int kFxFadeFrames = 5;       // the greyscale and the aura fade in and out over this many game frames
static constexpr int kFxDropFlashFrames = 9;  // the flash and the ring at the drop

// One after image of Shadow: his place and pose when it was made. It stays where it is (in the world) and fades out.
struct FxGhost {
    bool used;
    bool child;
    uint32_t id;
    int age;                // game frames since it was made
    Vec3f pos;
    Vec3s rot;              // actor.shape.rot
    Vec3f scale;            // actor.scale
    float yOffset;          // actor.shape.yOffset * scale.y
    float rootDrop;         // Player::unk_6C4, taken off the root height when drawing
    Vec3s upperRot;
    Vec3s headRot;
    s16 upperYawSecondary;
    Vec3s joints[kJoints];  // the joint table that was drawn
};

// A ball (or a squashed ball) in the world: one hit sphere of an enemy.
struct FxHitSphere {
    float x, y, z;
    float rx, ry, rz;
};

// The red copy of an enemy that was hit. Only numbers, no pointer to the enemy (it may be gone a moment later).
struct FxHitGhost {
    bool used;
    uint32_t id;
    int age;
    const void* key; // which enemy it was; only compared, never used as a pointer
    int count;
    FxHitSphere sph[kFxMaxHitSpheres];
};

static FxGhost sFxGhosts[kFxMaxGhosts];
static FxHitGhost sFxHitGhosts[kFxMaxHitGhosts];
static uint32_t sFxNextId = 1;
static int sFxSinceSpawn = 0;          // game frames since the last after image
static Vec3f sFxLastGhostPos = { 0.0f, 0.0f, 0.0f };
static bool sFxHaveLastGhostPos = false;
static float sFxWeight = 0.0f;         // 0..1: how much of the look is on (greyscale amount, aura strength)
static float sFxPulse = 0.0f;          // aura pulse, set to 1 when a new after image is made
static int sFxDropAge = -2;            // game frames since the drop (flash and ring), -2 = none
static uint32_t sFxTick = 0;           // counts game frames
static uint32_t sFxSpawnTick = 0xFFFFFFFFu;
static bool sFxGrayOn = false;         // greyscale was switched on in this frame's display lists
static Actor* sFxWrapped = nullptr;    // the player actor our draw function was put into
static int sFxGhostSkelState[2] = { -1, -1 }; // adult, child: -1 not checked, 0 missing, 1 found

static bool FxEnabled() {
    return CVarGetInteger(kCvarEnabled, 1) != 0 && CVarGetInteger(kCvarFx, 1) != 0;
}

static bool FxIsChild() {
    return gSaveContext.linkAge == LINK_AGE_CHILD;
}

static int FxClamp(int v, int lo, int hi) {
    return std::max(lo, std::min(hi, v));
}

static u8 FxToByte(float v) {
    return static_cast<u8>(std::lround(std::max(0.0f, std::min(1.0f, v)) * 255.0f));
}

static bool FxArchiveHasFile(const char* path) {
    Ship::Context* ctx = Ship::Context::GetRawInstance();
    if (ctx == nullptr) {
        return false;
    }
    auto resourceManager = ctx->GetResourceManager();
    if (resourceManager == nullptr) {
        return false;
    }
    auto archiveManager = resourceManager->GetArchiveManager();
    return archiveManager != nullptr && archiveManager->HasFile(path);
}

// The see-through copy of Shadow's skeleton, or nullptr when shadow_ghost.o2r is not installed.
static const FlexSkeletonHeader* FxGhostSkeleton(bool child) {
    int& state = sFxGhostSkelState[child ? 1 : 0];
    const char* path = child ? kGhostSkelChild : kGhostSkelAdult;
    if (state < 0) {
        state = FxArchiveHasFile(path) ? 1 : 0;
    }
    if (state == 0) {
        return nullptr;
    }
    const FlexSkeletonHeader* skeleton =
        reinterpret_cast<const FlexSkeletonHeader*>(ResourceMgr_LoadSkeletonByName(path, nullptr));
    if (skeleton == nullptr || skeleton->sh.segment == nullptr || skeleton->sh.limbCount < 21) {
        state = 0;
        return nullptr;
    }
    return skeleton;
}

static u8 FxGrayAmount() {
    return FxToByte(sFxWeight);
}

// Shadow himself is drawn in colour: the greyscale switch is off while the player's draw function runs.
static void FxPlayerDraw(Actor* thisx, PlayState* play) {
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    OPEN_DISPS(gfxCtx);
    if (sFxGrayOn) {
        gSPGrayscale(POLY_OPA_DISP++, false);
        gSPGrayscale(POLY_XLU_DISP++, false);
    }
    Player_Draw(thisx, play);
    if (sFxGrayOn) {
        const u8 amount = FxGrayAmount();
        gSPGrayscale(POLY_OPA_DISP++, true);
        gDPSetGrayscaleColor(POLY_OPA_DISP++, 255, 255, 255, amount);
        gSPGrayscale(POLY_XLU_DISP++, true);
        gDPSetGrayscaleColor(POLY_XLU_DISP++, 255, 255, 255, amount);
    }
    CLOSE_DISPS(gfxCtx);
}

// Puts our draw function in the player's place while the look is on, and the game's own back afterwards.
static void FxSetDrawWrapper(Player* player, bool wrap) {
    Actor* actor = &player->actor;
    if (wrap) {
        if (actor->draw == FxPlayerDraw) {
            sFxWrapped = actor;
        } else if (actor->draw == Player_Draw) {
            actor->draw = FxPlayerDraw;
            sFxWrapped = actor;
        }
        // Anything else is Boost's wrapper: it keeps Shadow in colour, nothing to add.
    } else if (actor->draw == FxPlayerDraw) {
        actor->draw = Player_Draw;
    }
}

static void FxReset() {
    std::memset(sFxGhosts, 0, sizeof(sFxGhosts));
    std::memset(sFxHitGhosts, 0, sizeof(sFxHitGhosts));
    sFxSinceSpawn = 0;
    sFxHaveLastGhostPos = false;
    sFxWeight = 0.0f;
    sFxPulse = 0.0f;
    sFxDropAge = -2;
    sFxGrayOn = false;
    if (sFxWrapped != nullptr && gPlayState != nullptr) {
        // Only touch the actor if it is still the live player (after a scene change it is gone).
        for (Actor* a = gPlayState->actorCtx.actorLists[ACTORCAT_PLAYER].head; a != nullptr; a = a->next) {
            if (a == sFxWrapped && a->draw == FxPlayerDraw) {
                a->draw = Player_Draw;
            }
        }
    }
    sFxWrapped = nullptr;
}

// Loads the see-through skeleton ahead of time, so the first after image does not make the game hitch.
static void FxPreload() {
    if (FxEnabled() && CVarGetInteger(kCvarFxAfter, 1) != 0) {
        (void)FxGhostSkeleton(FxIsChild());
    }
}

static void FxOnDrop() {
    sFxDropAge = -1; // the game frame update that follows makes it 0, the first frame that is drawn
    sFxSinceSpawn = 1000;
    sFxHaveLastGhostPos = false;
    sFxPulse = 1.0f;
}

// Once per game frame, after the player's update.
static void FxTick() {
    const bool on = FxEnabled() && sState == State::Active;
    const float step = 1.0f / static_cast<float>(kFxFadeFrames);
    if (on) {
        sFxWeight = std::min(1.0f, sFxWeight + step);
    } else {
        sFxWeight = std::max(0.0f, sFxWeight - step);
    }
    sFxPulse *= 0.6f;
    if (sFxPulse < 0.03f) {
        sFxPulse = 0.0f;
    }
    if (sFxDropAge > -2 && ++sFxDropAge > kFxDropFlashFrames) {
        sFxDropAge = -2;
    }
    if (sFxSinceSpawn < 1000) {
        sFxSinceSpawn++;
    }
    if (!FxEnabled()) {
        std::memset(sFxGhosts, 0, sizeof(sFxGhosts));
        std::memset(sFxHitGhosts, 0, sizeof(sFxHitGhosts));
    }
    const int life = FxClamp(CVarGetInteger(kCvarFxLife, 10), 3, 40);
    for (FxGhost& g : sFxGhosts) {
        if (g.used && ++g.age >= life) {
            g.used = false;
        }
    }
    const int hitLife = FxClamp(CVarGetInteger(kCvarFxHitLife, 16), 3, 80);
    for (FxHitGhost& g : sFxHitGhosts) {
        if (g.used && ++g.age >= hitLife) {
            g.used = false;
        }
    }
    sFxTick++;
}

// Turns the greyscale on for the world's display lists (and off for the HUD's), and puts our draw function in place.
// The greyscale switch stays set from one display list to the next, so every frame that turns it on also turns it off
// again at the start of the HUD's list; nothing has to be undone in the frame after.
static void FxDrawBegin() {
    sFxGrayOn = false;
    if (gPlayState == nullptr) {
        return;
    }
    Player* player = GET_PLAYER(gPlayState);
    const bool want = player != nullptr && FxEnabled() && CVarGetInteger(kCvarFxGrey, 1) != 0 && sFxWeight > 0.01f &&
                      gPlayState->pauseCtx.state == 0 && gSaveContext.gameMode == GAMEMODE_NORMAL;
    if (want) {
        GraphicsContext* gfxCtx = gPlayState->state.gfxCtx;
        OPEN_DISPS(gfxCtx);
        const u8 amount = FxGrayAmount();
        gSPGrayscale(POLY_OPA_DISP++, true);
        gDPSetGrayscaleColor(POLY_OPA_DISP++, 255, 255, 255, amount);
        gSPGrayscale(POLY_XLU_DISP++, true);
        gDPSetGrayscaleColor(POLY_XLU_DISP++, 255, 255, 255, amount);
        gSPGrayscale(OVERLAY_DISP++, false); // the HUD stays in colour
        CLOSE_DISPS(gfxCtx);
    }
    sFxGrayOn = want;
    if (player != nullptr) {
        FxSetDrawWrapper(player, want);
    }
}

// Stores a new after image when it is time. Runs at the end of the world's draw, so the joint table is the one that was
// just drawn (every pose change of every file is in it).
static void FxTrySpawnGhost(Player* player) {
    if (sFxSpawnTick == sFxTick) {
        return;
    }
    sFxSpawnTick = sFxTick;
    if (!FxEnabled() || CVarGetInteger(kCvarFxAfter, 1) == 0 || sState != State::Active) {
        return;
    }
    const int interval = FxClamp(CVarGetInteger(kCvarFxInterval, 2), 1, 20);
    if (sFxSinceSpawn < interval || player->skelAnime.jointTable == nullptr || player->skelAnime.limbCount < kJoints ||
        FxGhostSkeleton(FxIsChild()) == nullptr) {
        return;
    }
    const Vec3f& pos = player->actor.world.pos;
    if (sFxHaveLastGhostPos) {
        const float dx = pos.x - sFxLastGhostPos.x;
        const float dy = pos.y - sFxLastGhostPos.y;
        const float dz = pos.z - sFxLastGhostPos.z;
        if (dx * dx + dy * dy + dz * dz < 12.0f * 12.0f) {
            return; // standing still: no new after image
        }
    }
    FxGhost* slot = nullptr;
    for (FxGhost& g : sFxGhosts) {
        if (!g.used) {
            slot = &g;
            break;
        }
        if (slot == nullptr || g.age > slot->age) {
            slot = &g;
        }
    }
    std::memset(slot, 0, sizeof(*slot));
    slot->used = true;
    slot->child = FxIsChild();
    slot->id = sFxNextId++;
    slot->pos = pos;
    slot->rot = player->actor.shape.rot;
    slot->scale = player->actor.scale;
    slot->yOffset = player->actor.shape.yOffset * player->actor.scale.y;
    slot->rootDrop = player->unk_6C4;
    slot->upperRot = player->upperLimbRot;
    slot->headRot = player->headLimbRot;
    slot->upperYawSecondary = player->upperLimbYawSecondary;
    std::memcpy(slot->joints, player->skelAnime.jointTable, sizeof(slot->joints));
    sFxSinceSpawn = 0;
    sFxPulse = 1.0f; // the aura pulses when an after image appears
    sFxLastGhostPos = pos;
    sFxHaveLastGhostPos = true;
}

// The same changes the game makes to Link's root, upper body and head while it draws him (Player_OverrideLimbDrawGameplayCommon).
static s32 FxGhostOverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* arg) {
    (void)play;
    (void)dList;
    const FxGhost* g = static_cast<const FxGhost*>(arg);
    constexpr float kToRadians = 3.14159265f / 32768.0f;
    if (limbIndex == PLAYER_LIMB_ROOT) {
        if (g->child) {
            pos->x *= 0.64f;
            pos->y *= 0.64f;
            pos->z *= 0.64f;
        }
        pos->y -= g->rootDrop;
    } else if (limbIndex == PLAYER_LIMB_HEAD) {
        rot->x += g->headRot.z;
        rot->y -= g->headRot.y;
        rot->z += g->headRot.x;
    } else if (limbIndex == PLAYER_LIMB_UPPER) {
        if (g->upperYawSecondary != 0) {
            Matrix_RotateZ(0x44C * kToRadians, MTXMODE_APPLY);
            Matrix_RotateY(g->upperYawSecondary * kToRadians, MTXMODE_APPLY);
        }
        if (g->upperRot.y != 0) {
            Matrix_RotateY(g->upperRot.y * kToRadians, MTXMODE_APPLY);
        }
        if (g->upperRot.x != 0) {
            Matrix_RotateX(g->upperRot.x * kToRadians, MTXMODE_APPLY);
        }
        if (g->upperRot.z != 0) {
            Matrix_RotateZ(g->upperRot.z * kToRadians, MTXMODE_APPLY);
        }
    }
    return false;
}

static void FxDrawGhost(PlayState* play, const FxGhost& g, const FlexSkeletonHeader* skeleton, u8 alpha) {
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    // Each after image is its own piece for the frame interpolation, so it is only ever blended with itself.
    ::FrameInterpolation_RecordOpenChild(reinterpret_cast<const void*>(static_cast<uintptr_t>(0x5AE00000u + g.id)), 0);
    OPEN_DISPS(gfxCtx);

    Matrix_Push();
    Vec3s rot = g.rot;
    Matrix_SetTranslateRotateYXZ(g.pos.x, g.pos.y + g.yOffset + 1.0f, g.pos.z, &rot);
    Matrix_Scale(g.scale.x, g.scale.y, g.scale.z, MTXMODE_APPLY);

    // Flat red with the model's own shading, see-through, depth-tested but not written. (The model's meshes carry no
    // material, so everything that decides how it looks is set here.)
    gDPPipeSync(POLY_OPA_DISP++);
    gSPTexture(POLY_OPA_DISP++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_OFF);
    gDPSetCombineLERP(POLY_OPA_DISP++, SHADE, 0, ENVIRONMENT, PRIMITIVE, 0, 0, 0, ENVIRONMENT, SHADE, 0, ENVIRONMENT,
                      PRIMITIVE, 0, 0, 0, ENVIRONMENT);
    gDPSetOtherMode(POLY_OPA_DISP++,
                    G_AD_NOTPATTERN | G_CD_MAGICSQ | G_CK_NONE | G_TC_FILT | G_TF_BILERP | G_TT_NONE | G_TL_TILE |
                        G_TD_CLAMP | G_TP_PERSP | G_CYC_1CYCLE | G_PM_NPRIMITIVE,
                    G_AC_NONE | G_ZS_PIXEL | G_RM_AA_ZB_XLU_SURF | G_RM_AA_ZB_XLU_SURF2);
    gSPLoadGeometryMode(POLY_OPA_DISP++, G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 90, 10, 10, 255);
    gDPSetEnvColor(POLY_OPA_DISP++, 255, 60, 50, alpha);

    SkelAnime_DrawFlexOpa(play, static_cast<void**>(skeleton->sh.segment), const_cast<Vec3s*>(g.joints),
                          skeleton->dListCount, FxGhostOverrideLimbDraw, nullptr, const_cast<FxGhost*>(&g));
    Matrix_Pop();

    CLOSE_DISPS(gfxCtx);
    ::FrameInterpolation_RecordCloseChild();
}

static void FxDrawGhosts(PlayState* play, const Player* player) {
    if (!FxEnabled() || CVarGetInteger(kCvarFxAfter, 1) == 0) {
        return;
    }
    const float life = static_cast<float>(FxClamp(CVarGetInteger(kCvarFxLife, 10), 3, 40));
    const float opacity = static_cast<float>(FxClamp(CVarGetInteger(kCvarFxOpacity, 55), 0, 100)) / 100.0f;
    // Oldest first, so the newest is drawn on top.
    const FxGhost* order[kFxMaxGhosts];
    int count = 0;
    for (const FxGhost& g : sFxGhosts) {
        if (g.used && g.age >= 1) {
            order[count++] = &g;
        }
    }
    std::sort(order, order + count, [](const FxGhost* a, const FxGhost* b) { return a->age > b->age; });
    for (int i = 0; i < count; i++) {
        const FxGhost& g = *order[i];
        const float dx = g.pos.x - player->actor.world.pos.x;
        const float dy = g.pos.y - player->actor.world.pos.y;
        const float dz = g.pos.z - player->actor.world.pos.z;
        if (dx * dx + dy * dy + dz * dz < 10.0f * 10.0f) {
            continue; // right inside Shadow: would only flicker against him
        }
        const float t = std::min(1.0f, static_cast<float>(g.age) / life);
        const float alpha = opacity * std::pow(1.0f - t, 1.4f);
        const int a8 = static_cast<int>(std::lround(alpha * 255.0f));
        const FlexSkeletonHeader* skeleton = FxGhostSkeleton(g.child);
        if (a8 < 4 || skeleton == nullptr) {
            continue;
        }
        FxDrawGhost(play, g, skeleton, static_cast<u8>(std::min(255, a8)));
    }
}

// ----- The red copies of struck enemies -----

static constexpr int kFxSphereVertices = 32; // 1 + 3 rings of 10 + 1: the most the vertex buffer takes in one load
static constexpr float kFxSphereRadius = 64.0f; // model units of the ball; the matrix scales it to the hit sphere

// A rounded ball: a point on top, three rings of ten (48 degrees up, the middle, 48 degrees down), a point below. The
// colour of every vertex is its brightness (lit from above), so the matrix (position and size only) keeps it right.
static void FxBuildSphere(Vtx* vtx) {
    constexpr float kLat[3] = { 0.84f, 0.0f, -0.84f };
    constexpr float kLight[3] = { 0.35f, 0.85f, 0.40f };
    const float lightLength = std::sqrt(kLight[0] * kLight[0] + kLight[1] * kLight[1] + kLight[2] * kLight[2]);
    auto put = [&](int i, float nx, float ny, float nz) {
        Vtx_t& v = vtx[i].v;
        v.ob[0] = static_cast<s16>(std::lround(nx * kFxSphereRadius));
        v.ob[1] = static_cast<s16>(std::lround(ny * kFxSphereRadius));
        v.ob[2] = static_cast<s16>(std::lround(nz * kFxSphereRadius));
        v.flag = 0;
        v.tc[0] = 0;
        v.tc[1] = 0;
        const float lit = (nx * kLight[0] + ny * kLight[1] + nz * kLight[2]) / lightLength;
        const u8 c = FxToByte(0.40f + 0.60f * std::max(0.0f, lit));
        v.cn[0] = c;
        v.cn[1] = c;
        v.cn[2] = c;
        v.cn[3] = 255;
    };
    put(0, 0.0f, 1.0f, 0.0f);
    for (int r = 0; r < 3; r++) {
        const float cl = std::cos(kLat[r]);
        const float sl = std::sin(kLat[r]);
        for (int s = 0; s < 10; s++) {
            const float az = 6.2831853f * static_cast<float>(s) / 10.0f;
            put(1 + r * 10 + s, cl * std::cos(az), sl, cl * std::sin(az));
        }
    }
    put(31, 0.0f, -1.0f, 0.0f);
}

// Called for a frozen enemy that was just hit (before it gets its one update). colliders are the enemy's hit
// colliders. Leaves a red copy of its body where it stands now.
static void FxRecordEnemyHit(Actor* actor, Collider** colliders, int count) {
    if (!FxEnabled() || CVarGetInteger(kCvarFxHit, 1) == 0 || actor == nullptr) {
        return;
    }
    // The same enemy hit again right away (a sword swing hits on several frames): one copy is enough.
    for (const FxHitGhost& g : sFxHitGhosts) {
        if (g.used && g.key == actor && g.age < 3) {
            return;
        }
    }
    FxHitGhost made;
    std::memset(&made, 0, sizeof(made));
    auto add = [&](float x, float y, float z, float rx, float ry, float rz) {
        if (made.count >= kFxMaxHitSpheres || rx < 3.0f || ry < 3.0f || rz < 3.0f) {
            return;
        }
        const float limit = 250.0f;
        FxHitSphere& s = made.sph[made.count++];
        s.x = x;
        s.y = y;
        s.z = z;
        s.rx = std::min(rx, limit);
        s.ry = std::min(ry, limit);
        s.rz = std::min(rz, limit);
    };
    for (int i = 0; i < count; i++) {
        const Collider* c = colliders[i];
        if (c == nullptr) {
            continue;
        }
        if (c->shape == COLSHAPE_JNTSPH) {
            const ColliderJntSph* sph = reinterpret_cast<const ColliderJntSph*>(c);
            const int n = std::min<int>(sph->count, 64);
            for (int e = 0; e < n; e++) {
                const ColliderJntSphElement& el = sph->elements[e];
                if (!(el.info.bumperFlags & BUMP_ON)) {
                    continue; // not part of the body that can be hit (a weapon, say)
                }
                const Sphere16& w = el.dim.worldSphere;
                const float r = static_cast<float>(w.radius);
                add(w.center.x, w.center.y, w.center.z, r, r, r);
            }
        } else if (c->shape == COLSHAPE_CYLINDER) {
            const ColliderCylinder* cyl = reinterpret_cast<const ColliderCylinder*>(c);
            const float r = static_cast<float>(cyl->dim.radius);
            const float h = static_cast<float>(cyl->dim.height);
            add(cyl->dim.pos.x, cyl->dim.pos.y + cyl->dim.yShift + h * 0.5f, cyl->dim.pos.z, r, h * 0.5f, r);
        }
    }
    if (made.count == 0 && actor->colChkInfo.cylRadius > 0 && actor->colChkInfo.cylHeight > 0) {
        // No usable hit shape: a ball of the size the game gives the enemy for pushing and targeting.
        const float r = static_cast<float>(actor->colChkInfo.cylRadius);
        const float h = static_cast<float>(actor->colChkInfo.cylHeight);
        add(actor->world.pos.x, actor->world.pos.y + static_cast<float>(actor->colChkInfo.cylYShift) + h * 0.5f,
            actor->world.pos.z, r, h * 0.5f, r);
    }
    if (made.count == 0) {
        return;
    }
    FxHitGhost* slot = nullptr;
    for (FxHitGhost& g : sFxHitGhosts) {
        if (!g.used) {
            slot = &g;
            break;
        }
        if (slot == nullptr || g.age > slot->age) {
            slot = &g;
        }
    }
    made.used = true;
    made.id = sFxNextId++;
    made.age = 0;
    made.key = actor;
    *slot = made;
}

static void FxDrawHitGhosts(PlayState* play) {
    if (!FxEnabled() || CVarGetInteger(kCvarFxHit, 1) == 0) {
        return;
    }
    const float life = static_cast<float>(FxClamp(CVarGetInteger(kCvarFxHitLife, 16), 3, 80));
    const float opacity = static_cast<float>(FxClamp(CVarGetInteger(kCvarFxHitOpacity, 55), 0, 100)) / 100.0f;
    const FxHitGhost* order[kFxMaxHitGhosts];
    int count = 0;
    for (const FxHitGhost& g : sFxHitGhosts) {
        if (g.used && g.count > 0) {
            order[count++] = &g;
        }
    }
    if (count == 0) {
        return;
    }
    std::sort(order, order + count, [](const FxHitGhost* a, const FxHitGhost* b) { return a->age > b->age; });

    GraphicsContext* gfxCtx = play->state.gfxCtx;
    Vtx* vtx = static_cast<Vtx*>(Graph_Alloc(gfxCtx, sizeof(Vtx) * kFxSphereVertices));
    if (vtx == nullptr) {
        return;
    }
    FxBuildSphere(vtx);

    for (int i = 0; i < count; i++) {
        const FxHitGhost& g = *order[i];
        const float t = std::min(1.0f, static_cast<float>(g.age) / life);
        const int a8 = static_cast<int>(std::lround(opacity * std::pow(1.0f - t, 1.4f) * 255.0f));
        if (a8 < 4) {
            continue;
        }
        ::FrameInterpolation_RecordOpenChild(reinterpret_cast<const void*>(static_cast<uintptr_t>(0x5AE80000u + g.id)), 0);
        OPEN_DISPS(gfxCtx);

        Matrix_Push();
        // Flat red with a little shading, see-through, depth-tested but not written.
        gDPPipeSync(POLY_XLU_DISP++);
        gSPTexture(POLY_XLU_DISP++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_OFF);
        gDPSetCombineLERP(POLY_XLU_DISP++, SHADE, 0, ENVIRONMENT, PRIMITIVE, 0, 0, 0, ENVIRONMENT, SHADE, 0, ENVIRONMENT,
                          PRIMITIVE, 0, 0, 0, ENVIRONMENT);
        gDPSetOtherMode(POLY_XLU_DISP++,
                        G_AD_NOTPATTERN | G_CD_MAGICSQ | G_CK_NONE | G_TC_FILT | G_TF_BILERP | G_TT_NONE | G_TL_TILE |
                            G_TD_CLAMP | G_TP_PERSP | G_CYC_1CYCLE | G_PM_NPRIMITIVE,
                        G_AC_NONE | G_ZS_PIXEL | G_RM_AA_ZB_XLU_SURF | G_RM_AA_ZB_XLU_SURF2);
        gSPLoadGeometryMode(POLY_XLU_DISP++, G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH);
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 90, 10, 10, 255);
        gDPSetEnvColor(POLY_XLU_DISP++, 255, 60, 50, static_cast<u8>(std::min(255, a8)));

        for (int s = 0; s < g.count; s++) {
            const FxHitSphere& sp = g.sph[s];
            Matrix_Translate(sp.x, sp.y, sp.z, MTXMODE_NEW);
            Matrix_Scale(sp.rx / kFxSphereRadius, sp.ry / kFxSphereRadius, sp.rz / kFxSphereRadius, MTXMODE_APPLY);
            gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPVertex(POLY_XLU_DISP++, reinterpret_cast<uintptr_t>(vtx), kFxSphereVertices, 0);
            for (int k = 0; k < 10; k += 2) { // the cap on top
                gSP2Triangles(POLY_XLU_DISP++, 0, 1 + k, 1 + (k + 1) % 10, 0, 0, 1 + (k + 1) % 10, 1 + (k + 2) % 10, 0);
            }
            for (int r = 0; r < 2; r++) { // the two bands
                for (int k = 0; k < 10; k++) {
                    const int a = 1 + r * 10 + k;
                    const int b = 1 + r * 10 + (k + 1) % 10;
                    const int c = a + 10;
                    const int d = b + 10;
                    gSP2Triangles(POLY_XLU_DISP++, a, c, b, 0, b, c, d, 0);
                }
            }
            for (int k = 0; k < 10; k += 2) { // the cap below
                gSP2Triangles(POLY_XLU_DISP++, 31, 21 + (k + 1) % 10, 21 + k, 0, 31, 21 + (k + 2) % 10,
                              21 + (k + 1) % 10, 0);
            }
        }
        Matrix_Pop();

        CLOSE_DISPS(gfxCtx);
        ::FrameInterpolation_RecordCloseChild();
    }
}

// ----- The aura -----

// One ring of the round gradient: its radius (in 1/8 world units) and colour.
struct FxBandRow {
    float radius;
    u8 r, g, b, a;
};

// A round band made of triangles with the colours of its rows blended between them (a soft edge). Facing +Z, centred on
// the origin. Needs the matrix and the see-through vertex-colour state to be set.
static void FxDrawBand(GraphicsContext* gfxCtx, const FxBandRow* rows, int rowCount, int segments) {
    OPEN_DISPS(gfxCtx);
    constexpr int kSegmentsPerChunk = 6; // keeps every vertex load small
    for (int first = 0; first < segments; first += kSegmentsPerChunk) {
        const int count = std::min(kSegmentsPerChunk, segments - first);
        const int perRow = count + 1;
        const int vertexCount = rowCount * perRow;
        Vtx* vtx = static_cast<Vtx*>(Graph_Alloc(gfxCtx, static_cast<size_t>(vertexCount) * sizeof(Vtx)));
        if (vtx == nullptr) {
            continue;
        }
        for (int row = 0; row < rowCount; row++) {
            for (int i = 0; i < perRow; i++) {
                const float angle = 6.2831853f * static_cast<float>(first + i) / static_cast<float>(segments);
                Vtx_t& v = vtx[row * perRow + i].v;
                v.ob[0] = static_cast<s16>(std::lround(rows[row].radius * std::cos(angle)));
                v.ob[1] = static_cast<s16>(std::lround(rows[row].radius * std::sin(angle)));
                v.ob[2] = 0;
                v.flag = 0;
                v.tc[0] = 0;
                v.tc[1] = 0;
                v.cn[0] = rows[row].r;
                v.cn[1] = rows[row].g;
                v.cn[2] = rows[row].b;
                v.cn[3] = rows[row].a;
            }
        }
        gSPVertex(POLY_XLU_DISP++, reinterpret_cast<uintptr_t>(vtx), vertexCount, 0);
        for (int row = 0; row + 1 < rowCount; row++) {
            for (int i = 0; i < count; i++) {
                const int a = row * perRow + i;
                const int b = a + 1;
                const int c = a + perRow;
                const int d = c + 1;
                gSP2Triangles(POLY_XLU_DISP++, a, c, b, 0, b, c, d, 0);
            }
        }
    }
    CLOSE_DISPS(gfxCtx);
}

// The soft aura behind Shadow, and the flash and ring at the drop.
static void FxDrawAura(PlayState* play, const Player* player) {
    if (!FxEnabled() || CVarGetInteger(kCvarFxAura, 1) == 0) {
        return;
    }
    const float strength = static_cast<float>(FxClamp(CVarGetInteger(kCvarFxAuraStrength, 100), 0, 300)) / 100.0f;
    const float weight = std::min(1.0f, sFxWeight);
    const bool flash = sFxDropAge >= 0 && sFxDropAge <= kFxDropFlashFrames;
    if ((weight <= 0.02f && !flash) || strength <= 0.0f) {
        return;
    }
    const bool child = FxIsChild();
    const float flashT = flash ? 1.0f - static_cast<float>(sFxDropAge) / static_cast<float>(kFxDropFlashFrames) : 0.0f;
    const float radius = (child ? 44.0f : 58.0f) * (1.0f + 0.07f * sFxPulse + 0.9f * flashT);
    const float alpha = std::min(0.9f, ((0.38f + 0.40f * sFxPulse) * weight + 0.55f * flashT) * strength);

    // The middle of the aura: his chest, pushed a little away from the camera, so his body is in front of it.
    Vec3f centre = player->actor.world.pos;
    centre.y += child ? 26.0f : 38.0f;
    const Vec3f& eye = play->view.eye;
    const float vx = centre.x - eye.x;
    const float vy = centre.y - eye.y;
    const float vz = centre.z - eye.z;
    const float length = std::sqrt(vx * vx + vy * vy + vz * vz);
    if (length > 1.0f) {
        const float back = child ? 12.0f : 18.0f;
        centre.x += vx / length * back;
        centre.y += vy / length * back;
        centre.z += vz / length * back;
    }

    static const int sAuraTag = 0;
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    ::FrameInterpolation_RecordOpenChild(&sAuraTag, 0);
    OPEN_DISPS(gfxCtx);

    Gfx_SetupDL_4Xlu(gfxCtx);
    gDPSetCombineMode(POLY_XLU_DISP++, G_CC_SHADE, G_CC_SHADE);
    gSPClearGeometryMode(POLY_XLU_DISP++, G_LIGHTING | G_CULL_BOTH);

    Matrix_Translate(centre.x, centre.y, centre.z, MTXMODE_NEW);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    Matrix_Scale(1.0f / 8.0f, 1.0f / 8.0f, 1.0f / 8.0f, MTXMODE_APPLY); // the vertices are in 1/8 world units
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);

    // Bright red in the middle, fading to nothing at the edge.
    const float r8 = radius * 8.0f;
    const FxBandRow aura[3] = {
        { 0.0f, 255, 170, 150, FxToByte(alpha) },
        { r8 * 0.42f, 255, 70, 60, FxToByte(alpha * 0.6f) },
        { r8, 255, 30, 30, 0 },
    };
    FxDrawBand(gfxCtx, aura, 3, 12);

    // The ring that goes out at the drop.
    if (flash) {
        const float progress = static_cast<float>(sFxDropAge) / static_cast<float>(kFxDropFlashFrames);
        const float ringRadius = (child ? 44.0f : 58.0f) * (0.5f + 2.2f * progress) * 8.0f;
        const float half = ringRadius * 0.2f + 24.0f;
        const float ringAlpha = 0.8f * (1.0f - progress) * std::min(1.5f, strength);
        const FxBandRow ring[3] = {
            { ringRadius - half, 255, 190, 170, 0 },
            { ringRadius, 255, 190, 170, FxToByte(ringAlpha) },
            { ringRadius + half, 255, 190, 170, 0 },
        };
        FxDrawBand(gfxCtx, ring, 3, 24);
    }

    CLOSE_DISPS(gfxCtx);
    ::FrameInterpolation_RecordCloseChild();
}

// Runs at the end of the world's draw, before the HUD: the after images, the copies of struck enemies, the aura.
static void FxDrawEnd() {
    if (gPlayState == nullptr || gSaveContext.gameMode != GAMEMODE_NORMAL) {
        return;
    }
    Player* player = GET_PLAYER(gPlayState);
    if (player == nullptr || gPlayState->pauseCtx.state != 0) {
        return;
    }
    FxTrySpawnGhost(player);
    bool anything = false;
    for (const FxGhost& g : sFxGhosts) {
        anything = anything || g.used;
    }
    for (const FxHitGhost& g : sFxHitGhosts) {
        anything = anything || g.used;
    }
    const bool flash = sFxDropAge >= 0 && sFxDropAge <= kFxDropFlashFrames;
    if (!FxEnabled() || (!anything && sFxWeight <= 0.02f && !flash)) {
        return;
    }
    GraphicsContext* gfxCtx = gPlayState->state.gfxCtx;
    {
        // What we draw now is in colour, not in the world's greyscale.
        OPEN_DISPS(gfxCtx);
        gSPGrayscale(POLY_OPA_DISP++, false);
        gSPGrayscale(POLY_XLU_DISP++, false);
        CLOSE_DISPS(gfxCtx);
    }
    FxDrawGhosts(gPlayState, player);
    FxDrawHitGhosts(gPlayState);
    FxDrawAura(gPlayState, player);
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

// The bar itself.
static void DrawChaosBar() {
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

// Runs while the world is drawn, before the HUD (so the HUD is drawn on top, and nothing is drawn in the pause menu).
static void OnPlayDrawEndChaosControl() {
    FxDrawEnd();
    DrawChaosBar();
}

static void RegisterChaosControl() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayerUpdate>(OnPlayerUpdateChaosControl);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::ShouldActorUpdate>(ShouldActorUpdateChaosControl);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnActorUpdate>(OnActorUpdateChaosControl);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayDrawBegin>(OnPlayDrawBeginChaosControl);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayDrawBegin>(FxDrawBegin);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayDrawEnd>(OnPlayDrawEndChaosControl);
    // After the main update above (hooks run in the order they were added): the look's own once-per-frame step.
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayerUpdate>(FxTick);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneInit>(
        [](int16_t) { ResetChaosControl(); });
    // The pause menu does not run player updates, so end Chaos Control (and its music) when it opens.
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnKaleidoscopeUpdate>(
        [](int16_t) { StopChaosControl(false); sBlendOut = 0; sBarAlpha = 0.0f; FxReset(); });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnLoadGame>([](int32_t) {
        ResetChaosControl();
        sFxGhostSkelState[0] = sFxGhostSkelState[1] = -1; // look for shadow_ghost.o2r again
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
