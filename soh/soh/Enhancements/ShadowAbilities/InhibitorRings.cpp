// Shadow the Hedgehog - Inhibitor Rings for Ship of Harkinian 9.3.1  (version 1)
//
// Shadow's gold inhibitor rings (two on his wrists, two on his ankles) hold his power back. This ability takes them off.
//
// Inhibitor Rings is an item. It sits in the pause menu in the Din's Fire slot (see "The item" below) and is equipped to a
// C button like the Ocarina or the Deku Stick. Press that C button to use it:
//
//  1. Shadow is locked (and cannot be hurt) while he takes the rings off. A voice line plays (one of two, picked at random
//     every time). He pulls off the wrist rings (left, then right), then kicks off the ankle rings (left, then right).
//     Each ring really disappears from his model and drops to the ground in front of him.
//  2. When the last ring is off he powers up: the music starts (and loops until the ability ends), and Shadow glows like he
//     is almost on fire. Flames lick over his body and he leaves a trail of fire on the ground when he moves.
//  3. For as long as it lasts:
//       * he cannot be hurt by anything (enemies, falls, ...),
//       * everything he hits takes three times the damage (bosses too),
//       * he runs faster than his normal run (not as fast as the boost), and his head stays steady while he runs,
//       * he pays for it: every half second he loses a quarter of a heart.
//  4. It ends when he is down to a quarter heart, or when you press the item button again. The rings come back (they are on
//     his model again), the music fades out.
//
// The music, the two voice lines, the pose animation and the ring-less textures come from the separate asset mod
// shadow_rings.o2r (in the mods folder, files under custom/inhibitor_rings/ and objects/...). The music and voices are
// played through their own SDL audio device (the same way the other Shadow files do it).
// The model with the rings removed needs shadow_adult.o2r / shadow_child.o2r (the Shadow model) to be installed. Without
// them the ability still works, only the rings stay on the model. The glow around the body needs shadow_ghost.o2r (the
// same file the boost and Chaos Control use); without it there is no glow shell, only the flames.
//
// The item
// --------
// Inhibitor Rings takes over the game's "Din's Fire" item (ITEM_DINS_FIRE). That item has everything a new item needs: an
// icon, a name and a pause menu slot. The icon and the name come from the o2r mod (textures/icon_item_static/
// gItemIconDinsFireTex and textures/item_name_static/gDinsFireItemName*Tex), and Din's Fire's own spell is switched off
// here (this file takes over the item). If Din's Fire's slot is empty, Inhibitor Rings is put there right away.
// Not used in randomizer saves, where Din's Fire keeps its normal meaning.
//
// Everything is in this one file. CMake picks it up automatically (soh/*.cpp is globbed).
//
// Settings (CVars, change with the in-game console: set <name> <value>):
//   gShadow.Rings.Enabled      1 = on (default), 0 = off
//   gShadow.Rings.Item         1 = Inhibitor Rings is an item in the Din's Fire slot (default), 0 = no item
//   gShadow.Rings.Button       extra button that also starts it, default 0 = none. 32 = L, 8192 = Z
//   gShadow.Rings.Sound        1 = music and voice (default), 0 = silent
//   gShadow.Rings.Volume       0-100, volume of the music and the voice, default 80
//   gShadow.Rings.Animation    1 = play the ring removal pose (default), 0 = no pose (the rings come off at once)
//   gShadow.Rings.PoseLift     extra height added to the pose in model units (100 = 1 game unit), default 0
//   gShadow.Rings.Lock         1 = lock the player while the rings come off (default), 0 = no lock
//   gShadow.Rings.Invincible   1 = Shadow cannot be hurt while it lasts (default), 0 = he can
//   gShadow.Rings.Damage       damage multiplier for everything Shadow hits, default 3 (1 = normal)
//   gShadow.Rings.DrainFrames  game frames (20 per second) between two health drains, default 10 (half a second)
//   gShadow.Rings.Drain        health units lost per drain, default 4 (a quarter heart; one heart = 16)
//   gShadow.Rings.MinHealth    the ability ends at this health, default 4 (a quarter heart). Also the least health it can be
//                              started with (it needs more than this)
//   gShadow.Rings.RunSpeed     run speed in percent of the normal run, default 170 (the boost is about 300)
//   gShadow.Rings.RunAccel     how fast he speeds up, in game speed units per frame, default 1.5
//   gShadow.Rings.Head         1 = the head stays steady while he runs (default), 0 = the game's own head motion
//   gShadow.Rings.Effect       1 = the fire look (glow, flames, fire trail) on (default), 0 = off
//   gShadow.Rings.Glow         1 = the orange glow shell around the body (default), 0 = none (needs shadow_ghost.o2r)
//   gShadow.Rings.GlowStrength glow strength in percent, default 100
//   gShadow.Rings.Flames       1 = flames on the body (default), 0 = none
//   gShadow.Rings.Trail        1 = the fire trail on the ground (default), 0 = none
//   gShadow.Rings.Aura         1 = the soft orange aura behind him (default), 0 = none
//   gShadow.Rings.Debug        1 = show a small on-screen message when the item is used and why it did not start (default 1
//                              while this is being tested), 0 = silent

#include <libultraship/bridge/consolevariablebridge.h>

#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/ActorDB.h"
#include "soh/ShipInit.hpp"
#include "soh/Notification/Notification.h"

#include <SDL2/SDL.h>
#include <ship/Context.h>
#include <ship/resource/File.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/ArchiveManager.h>

#include <algorithm>
#include <chrono>
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
// OPEN_DISPS / CLOSE_DISPS declare these inside the function. They have to be found as C functions.
void FrameInterpolation_RecordOpenChild(const void* a, int b);
void FrameInterpolation_RecordCloseChild(void);
// Finds a skeleton resource by its path (the same call the game makes when it starts a skeleton). nullptr if missing.
SkeletonHeader* ResourceMgr_LoadSkeletonByName(const char* path, SkelAnime* skelAnime);
// Changes one command of a display list that comes from a mod (and puts it back). The game's own Patch... function refuses
// mod assets, this one does not.
void ResourceMgr_PatchCustomGfxByName(const char* path, const char* patchName, int index, Gfx instruction);
void ResourceMgr_UnpatchGfxByName(const char* path, const char* patchName);

// The game's own "where does the stick point" (target speed and direction, the direction is relative to the camera).
s32 Player_GetMovementSpeedAndYaw(Player* player, f32* outSpeedTarget, s16* outYawTarget, f32 speedMode,
                                  PlayState* play);

// The actions Link is in while standing, walking or running freely on the ground.
void Player_Action_Idle(Player* player, PlayState* play);
void Player_Action_80842180(Player* player, PlayState* play); // running
void Player_Action_80840450(Player* player, PlayState* play);
void Player_Action_808407CC(Player* player, PlayState* play);
void Player_Action_80840DE4(Player* player, PlayState* play);
void Player_Action_808414F8(Player* player, PlayState* play);
void Player_Action_8084170C(Player* player, PlayState* play);
void Player_Action_808417FC(Player* player, PlayState* play);
}

// Provided by the other Shadow files. All weak, so this file also works without any of them.
extern "C" __attribute__((weak)) bool ShadowChaosControl_IsBusy(void); // Chaos Control holds Link still
extern "C" __attribute__((weak)) bool ShadowBoost_IsBusy(void);        // the boost charge-up pose plays
extern "C" __attribute__((weak)) bool ShadowBoost_IsActive(void);      // Boost.cpp version 13 and later: the boost runs
extern "C" __attribute__((weak)) bool ShadowHoming_IsBusy(void);       // a homing attack is running

// The fire look (further down in this file). These are used by the code in the nameless namespace below.
static void FxOnIgnite();
static void FxOnRingOff(int ring);
static void FxOnEnd();
static void FxReset();
static void FxPreload();

namespace {

constexpr const char* kCvarEnabled = "gShadow.Rings.Enabled";
constexpr const char* kCvarItem = "gShadow.Rings.Item";
constexpr const char* kCvarButton = "gShadow.Rings.Button";
constexpr const char* kCvarSound = "gShadow.Rings.Sound";
constexpr const char* kCvarVolume = "gShadow.Rings.Volume";
constexpr const char* kCvarAnimation = "gShadow.Rings.Animation";
constexpr const char* kCvarPoseLift = "gShadow.Rings.PoseLift";
constexpr const char* kCvarLock = "gShadow.Rings.Lock";
constexpr const char* kCvarInvincible = "gShadow.Rings.Invincible";
constexpr const char* kCvarDamage = "gShadow.Rings.Damage";
constexpr const char* kCvarDrainFrames = "gShadow.Rings.DrainFrames";
constexpr const char* kCvarDrain = "gShadow.Rings.Drain";
constexpr const char* kCvarMinHealth = "gShadow.Rings.MinHealth";
constexpr const char* kCvarRunSpeed = "gShadow.Rings.RunSpeed";
constexpr const char* kCvarRunAccel = "gShadow.Rings.RunAccel";
constexpr const char* kCvarHead = "gShadow.Rings.Head";
constexpr const char* kCvarFx = "gShadow.Rings.Effect";
constexpr const char* kCvarFxGlow = "gShadow.Rings.Glow";
constexpr const char* kCvarFxGlowStrength = "gShadow.Rings.GlowStrength";
constexpr const char* kCvarFxFlames = "gShadow.Rings.Flames";
constexpr const char* kCvarFxTrail = "gShadow.Rings.Trail";
constexpr const char* kCvarFxAura = "gShadow.Rings.Aura";
constexpr const char* kCvarDebug = "gShadow.Rings.Debug";

// The see-through copy of Shadow's model for the glow shell (shadow_ghost.o2r, the same file the boost uses).
constexpr const char* kGhostSkelAdult = "objects/object_link_boy/gLinkAdultGhostSkel";
constexpr const char* kGhostSkelChild = "objects/object_link_child/gLinkChildGhostSkel";

// Files inside the asset mod (shadow_rings.o2r).
constexpr const char* kFileMusic = "custom/inhibitor_rings/music.wav";
constexpr const char* kFileVoice1 = "custom/inhibitor_rings/voice1.wav";
constexpr const char* kFileVoice2 = "custom/inhibitor_rings/voice2.wav";
constexpr const char* kFileConfig = "custom/inhibitor_rings/config.txt";
constexpr const char* kFileAnimAdult = "custom/inhibitor_rings/anim_adult.bin";
constexpr const char* kFileAnimChild = "custom/inhibitor_rings/anim_child.bin";

// The game logic runs at 20 frames per second.
constexpr int kFramesPerSecond = 20;
// The pose fades in over this many frames and fades out again when the windup ends / is interrupted.
constexpr int kBlendInFrames = 3;
constexpr int kBlendOutFrames = 4;
// The frame of the pose where the voice line starts.
constexpr int kVoiceFrame = 3;
// While the ability runs Shadow's invincibility timer is kept at this. A positive timer is the game's "intangible": Link's body is
// not even offered to the enemies' attacks (no damage, no flinch, no knockback). (A negative timer would only save the health:
// he would still flinch and fly back from every hit.) The game draws Link flashing red while the timer is positive; that is
// hidden again while he is drawn (see OnPlayDrawBeginRings).
constexpr s8 kInvulnerableTimer = 20;
// The stick must be pushed this far (out of about 85) before it counts as "held" (the game ignores a stick below about 20).
constexpr float kStickThreshold = 22.0f;

inline int ClampInt(int v, int lo, int hi) {
    return std::max(lo, std::min(hi, v));
}

// ------------------------------------------------------------------------------------------------
// Small random numbers of our own (the game's random numbers are left alone).
// ------------------------------------------------------------------------------------------------

uint32_t sRng = 0x9E3779B9u;

uint32_t Rnd() {
    sRng ^= sRng << 13;
    sRng ^= sRng >> 17;
    sRng ^= sRng << 5;
    return sRng;
}

float Rnd01() {
    return static_cast<float>(Rnd() & 0xFFFFFF) / 16777216.0f;
}

float RndC(float range) {
    return (Rnd01() * 2.0f - 1.0f) * range;
}

// ------------------------------------------------------------------------------------------------
// Audio: a tiny mixer on its own SDL audio device (44.1 kHz stereo, 16 bit): the music (it loops) and the voice.
// ------------------------------------------------------------------------------------------------

constexpr int kRate = 44100;
constexpr int kChannels = 2;

enum TrackId { kTrackMusic = 0, kTrackVoice, kTrackCount };
constexpr float kTrackLevel[kTrackCount] = { 0.55f, 1.0f };

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
    bool loop = false; // at the end of the clip it starts again from the beginning (until it is told to fade out)
};

Clip sMusic;
Clip sVoice[2];
Track sTracks[kTrackCount];
SDL_AudioDeviceID sDevice = 0;
bool sAssetsTried = false;

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
            if (tr.clip == nullptr || tr.clip->pcm.empty()) {
                tr.playing = false;
                continue;
            }
            if (tr.pos >= tr.clip->pcm.size()) {
                if (tr.loop && tr.target > 0.0f) {
                    tr.pos = 0;
                } else {
                    tr.playing = false;
                    continue;
                }
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
    const int v = ClampInt(CVarGetInteger(kCvarVolume, 80), 0, 100);
    return static_cast<float>(v) / 100.0f;
}

void PlayTrack(TrackId id, const Clip* clip, bool loop) {
    if (sDevice == 0 || clip == nullptr || !clip->ok) {
        return;
    }
    const float level = kTrackLevel[id] * MasterVolume();
    SDL_LockAudioDevice(sDevice);
    Track& tr = sTracks[id];
    tr.clip = clip;
    tr.pos = 0;
    tr.gain = level;
    tr.target = level;
    tr.step = 0.0f;
    tr.loop = loop;
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
// The ring removal animation (made for Link's skeleton, 20 frames per second).
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

// The frames of the animation where things happen (config.txt in the asset mod). Wrist rings: left, right. Ankle rings:
// left, right. Order of the array: left wrist, right wrist, left ankle, right ankle.
int sRingFrame[4] = { 24, 47, 60, 70 };
int sPowerFrame = 74; // the power-up: the music starts, the fire ignites

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
        if (key == "ring_l_wrist") {
            sRingFrame[0] = value;
        } else if (key == "ring_r_wrist") {
            sRingFrame[1] = value;
        } else if (key == "ring_l_ankle") {
            sRingFrame[2] = value;
        } else if (key == "ring_r_ankle") {
            sRingFrame[3] = value;
        } else if (key == "power_frame") {
            sPowerFrame = value;
        }
    }
}

void LoadAssets() {
    if (sAssetsTried) {
        return;
    }
    sAssetsTried = true;

    auto music = LoadModFile(kFileMusic);
    if (music != nullptr) {
        DecodeWav(*music->Buffer, sMusic);
    }
    const char* voices[2] = { kFileVoice1, kFileVoice2 };
    for (int i = 0; i < 2; i++) {
        auto file = LoadModFile(voices[i]);
        if (file != nullptr) {
            DecodeWav(*file->Buffer, sVoice[i]);
        }
    }
    auto config = LoadModFile(kFileConfig);
    if (config != nullptr) {
        ParseConfig(std::string(config->Buffer->begin(), config->Buffer->end()));
    }
    LoadAnim(kFileAnimAdult, sAnimAdult);
    LoadAnim(kFileAnimChild, sAnimChild);
}

// ------------------------------------------------------------------------------------------------
// Taking the rings off Shadow's model.
//
// Every limb of the Shadow model draws itself with a call to a material (the first command of its display list). The rings are
// part of that material's texture (tex_4). shadow_rings.o2r holds a second version of the texture with the gold painted over
// (and a material that uses it). To take a ring off, the first command of the limb's display list is changed to call that
// second material, and changed back when the ability ends. That also means a ring on his wrist or ankle never comes back
// "by accident": every change is undone in StopRings and in the resets.
// ------------------------------------------------------------------------------------------------

// Display lists of the model that hold each ring. [ring][age] = list of limb display lists, nullptr ended.
// ring 0 = left wrist, 1 = right wrist, 2 = left ankle, 3 = right ankle. The first command of each is the material call.
const char* const kRingLimbsAdult[4][4] = {
    { "bone015_gLinkAdultLeftHandLimb_mesh_layer_Opaque", "bone015_gLinkAdultLeftHandLimb_mesh_layer_Opaque_big",
      "bone015_gLinkAdultLeftHandLimb_mesh_layer_Opaque_bigold", nullptr },
    { "bone018_gLinkAdultRightHandLimb_mesh_layer_Opaque", "bone018_gLinkAdultRightHandLimb_mesh_layer_Opaque_big",
      "bone018_gLinkAdultRightHandLimb_mesh_layer_Opaque_bigold", nullptr },
    { "bone007_gLinkAdultLeftLegLimb_mesh_layer_Opaque", nullptr, nullptr, nullptr },
    { "bone004_gLinkAdultRightLegLimb_mesh_layer_Opaque", nullptr, nullptr, nullptr },
};

const char* const kRingLimbsChild[4][6] = {
    { "bone015_gLinkChildLeftHandLimb_mesh_layer_Opaque", "bone015_gLinkChildLeftHandLimb_mesh_layer_Opaque_big",
      "bone015_gLinkChildLeftHandLimb_mesh_layer_Opaque_bigold", nullptr, nullptr, nullptr },
    { "bone018_gLinkChildRightHandLimb_mesh_layer_Opaque", "bone018_gLinkChildRightHandLimb_mesh_layer_Opaque_big",
      "bone018_gLinkChildRightHandLimb_mesh_layer_Opaque_bigold",
      "bone018_gLinkChildRightHandLimb_mesh_layer_Opaque_big_sh_gLinkChildRightFistAndDekuShieldFarDL",
      "bone018_gLinkChildRightHandLimb_mesh_layer_Opaque_big_sh_gLinkChildRightFistAndDekuShieldNearDL", nullptr },
    { "bone007_gLinkChildLeftShinLimb_mesh_layer_Opaque", nullptr, nullptr, nullptr, nullptr, nullptr },
    { "bone004_gLinkChildRightShinLimb_mesh_layer_Opaque", nullptr, nullptr, nullptr, nullptr, nullptr },
};

// The material paths the patched command calls (these strings must stay where they are: the display list points at them).
alignas(2) const char kMatAdultArm[] = "objects/object_link_boy/shadow_adult/mat_tex_4_off_arm";
alignas(2) const char kMatAdultLeg[] = "objects/object_link_boy/shadow_adult/mat_tex_4_off_leg";
alignas(2) const char kMatChildArm[] = "objects/object_link_child/shadow_child/mat_tex_4_off_arm";
alignas(2) const char kMatChildLeg[] = "objects/object_link_child/shadow_child/mat_tex_4_off_leg";

constexpr const char* kBaseAdult = "__OTR__objects/object_link_boy/shadow_adult/";
constexpr const char* kBaseChild = "__OTR__objects/object_link_child/shadow_child/";

struct PatchRecord {
    std::string path;
    std::string name;
};

std::vector<PatchRecord> sPatches;
bool sRingHidden[4] = { false, false, false, false };

// Takes one ring off the model.
void HideModelRing(int ring) {
    if (ring < 0 || ring > 3 || sRingHidden[ring]) {
        return;
    }
    sRingHidden[ring] = true;
    const bool child = gSaveContext.linkAge == LINK_AGE_CHILD;
    const bool arm = ring < 2;
    const char* material = child ? (arm ? kMatChildArm : kMatChildLeg) : (arm ? kMatAdultArm : kMatAdultLeg);
    const Gfx instruction = gsSPDisplayListOTRFilePath(material);
    const char* base = child ? kBaseChild : kBaseAdult;
    const int count = child ? 6 : 4;
    for (int i = 0; i < count; i++) {
        const char* limb = child ? kRingLimbsChild[ring][i] : kRingLimbsAdult[ring][i];
        if (limb == nullptr) {
            break;
        }
        PatchRecord record;
        record.path = std::string(base) + limb;
        record.name = std::string("shadowRing") + std::to_string(ring) + "_" + std::to_string(i);
        ResourceMgr_PatchCustomGfxByName(record.path.c_str(), record.name.c_str(), 0, instruction);
        sPatches.push_back(record);
    }
}

// Puts every ring back on the model.
void ShowAllModelRings() {
    for (const PatchRecord& record : sPatches) {
        ResourceMgr_UnpatchGfxByName(record.path.c_str(), record.name.c_str());
    }
    sPatches.clear();
    for (bool& hidden : sRingHidden) {
        hidden = false;
    }
}

// ------------------------------------------------------------------------------------------------
// The ability itself
// ------------------------------------------------------------------------------------------------

enum class State { Idle, Windup, Active };

State sState = State::Idle;
int sTick = 0;               // frame of the pose that was played this frame (Windup)
int sActiveTicks = 0;        // game frames the ability has been Active
int sDrainCount = 0;         // game frames since the last health drain
bool sJustStarted = false;
bool sInputFlagSet = false;  // we set PLAYER_STATE1_INPUT_DISABLED for this frame and have to clear it again
bool sFireOn = false;        // the power-up happened: glow, flames, trail, music
int sBlendOut = 0;
bool sHavePose = false;      // sLastPose holds a pose that was shown
Vec3s sLastPose[kJoints];
float sRunSpeed = 0.0f;
float sHeadWeight = 0.0f;    // how much of the steady head is on (0..1)
int sVoicePick = 0;
int sGuardHealth = -1;       // the health that nothing but the drain may take away (-1 = not guarding)
bool sTimerHidden = false;   // the invincibility timer is set to 0 for the draw (no red flicker) and has to be put back
s8 sTimerSaved = 0;

bool SoundOn() {
    return CVarGetInteger(kCvarSound, 1) != 0;
}

void PlayGameCue(u16 sfxId) {
    Sfx_PlaySfxCentered(sfxId);
}

// A small on-screen message (bottom right, the game's own notification box) while the ability is being tested.
void Say(const std::string& text) {
    if (CVarGetInteger(kCvarDebug, 1) != 0) {
        Notification::Emit({ .message = text, .remainingTime = 5.0f, .mute = true });
    }
}

bool Invulnerable() {
    return sState != State::Idle && CVarGetInteger(kCvarInvincible, 1) != 0;
}

int MinHealth() {
    return std::max(0, CVarGetInteger(kCvarMinHealth, 4));
}

// ----- The item ---------------------------------------------------------------------------------

// Din's Fire's slot in the pause menu: if it is empty, Inhibitor Rings goes in. Nothing else depends on it.
void EnsureItemSetup() {
    if (!CVarGetInteger(kCvarEnabled, 1) || !CVarGetInteger(kCvarItem, 1) || IS_RANDO) {
        return;
    }
    if (INV_CONTENT(ITEM_DINS_FIRE) == ITEM_NONE) {
        INV_CONTENT(ITEM_DINS_FIRE) = ITEM_DINS_FIRE;
    }
}

// ----- Starting and stopping ----------------------------------------------------------------------

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

// nullptr when the ability can start, otherwise why not.
const char* CannotStart(Player* player) {
    if (PlayerBusy(player)) {
        return "Link is busy";
    }
    if (gPlayState->msgCtx.msgMode != MSGMODE_NONE) {
        return "text box";
    }
    if (ShadowChaosControl_IsBusy != nullptr && ShadowChaosControl_IsBusy()) {
        return "Chaos Control is starting";
    }
    if (ShadowBoost_IsBusy != nullptr && ShadowBoost_IsBusy()) {
        return "the boost is charging";
    }
    if (ShadowBoost_IsActive != nullptr && ShadowBoost_IsActive()) {
        return "end the boost first";
    }
    if (ShadowHoming_IsBusy != nullptr && ShadowHoming_IsBusy()) {
        return "homing attack";
    }
    const u32 blocked1 = PLAYER_STATE1_IN_WATER | PLAYER_STATE1_ON_HORSE | PLAYER_STATE1_CLIMBING_LEDGE |
                         PLAYER_STATE1_HANGING_OFF_LEDGE | PLAYER_STATE1_CLIMBING_LADDER | PLAYER_STATE1_DAMAGED |
                         PLAYER_STATE1_FIRST_PERSON | PLAYER_STATE1_CARRYING_ACTOR | PLAYER_STATE1_FREEFALL |
                         PLAYER_STATE1_JUMPING;
    const u32 blocked2 = PLAYER_STATE2_UNDERWATER | PLAYER_STATE2_DIVING | PLAYER_STATE2_CRAWLING |
                         PLAYER_STATE2_GRABBED_BY_ENEMY | PLAYER_STATE2_FROZEN;
    if ((player->stateFlags1 & blocked1) || (player->stateFlags2 & blocked2)) {
        return "not possible in this state";
    }
    if ((player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) == 0) {
        return "not on the ground";
    }
    if (gSaveContext.health <= MinHealth()) {
        return "not enough health";
    }
    return nullptr;
}

void EnterActive() {
    sState = State::Active;
    sActiveTicks = 0;
    sDrainCount = 0;
    sRunSpeed = 0.0f;
    sBlendOut = kBlendOutFrames; // the pose fades out into Link's normal pose
}

// The power-up: the music, the fire.
void Ignite(Player* player) {
    (void)player;
    if (sFireOn) {
        return;
    }
    sFireOn = true;
    if (SoundOn() && sMusic.ok && sDevice != 0) {
        PlayTrack(kTrackMusic, &sMusic, true);
    }
    PlayGameCue(NA_SE_EV_FLAME_IGNITION);
    PlayGameCue(NA_SE_PL_MAGIC_FIRE);
    ::FxOnIgnite();
}

// One ring comes off: it disappears from the model and falls to the ground.
void RingOff(Player* player, int ring) {
    (void)player;
    HideModelRing(ring);
    PlayGameCue(ring < 2 ? NA_SE_IT_SHIELD_REFLECT_SW : NA_SE_IT_HAMMER_HIT);
    ::FxOnRingOff(ring);
}

void BeginRings(Player* player) {
    sTick = 0;
    sJustStarted = true;
    sBlendOut = 0;
    sHavePose = false;
    sFireOn = false;
    sHeadWeight = 0.0f;
    sGuardHealth = -1;
    sRng ^= SDL_GetTicks() * 2654435761u;
    if (sRng == 0) {
        sRng = 0x9E3779B9u;
    }
    sVoicePick = static_cast<int>(Rnd() % 2);
    ::FxPreload(); // loads the glow model now, so the first frame of the glow does not make the game hitch

    LoadAssets();
    const bool haveDevice = SoundOn() && (sMusic.ok || sVoice[0].ok || sVoice[1].ok) && EnsureDevice();
    (void)haveDevice;
    const bool havePose = CVarGetInteger(kCvarAnimation, 1) != 0 && CurrentAnim().frames > 0;

    if (!havePose) {
        // No pose: the rings come off at once.
        sState = State::Windup;
        for (int r = 0; r < 4; r++) {
            RingOff(player, r);
        }
        if (SoundOn() && sDevice != 0) {
            const Clip& voice = sVoice[sVoicePick].ok ? sVoice[sVoicePick] : sVoice[1 - sVoicePick];
            PlayTrack(kTrackVoice, &voice, false);
        }
        Ignite(player);
        EnterActive();
        sBlendOut = 0;
        return;
    }
    sState = State::Windup;
}

// normalEnd: the health ran down to the end. Otherwise the player cancelled or something interrupted it.
void StopRings(bool normalEnd) {
    if (sState == State::Idle) {
        return;
    }
    const bool wasWindup = (sState == State::Windup);
    FadeTrack(kTrackVoice, 0.2f);
    FadeTrack(kTrackMusic, normalEnd ? 1.5f : 0.8f);
    sState = State::Idle;
    sJustStarted = false;
    sRunSpeed = 0.0f;
    sGuardHealth = -1;
    ShowAllModelRings();
    if (sFireOn) {
        PlayGameCue(NA_SE_PL_MAGIC_WIND_VANISH);
    }
    sFireOn = false;
    ::FxOnEnd();
    if (wasWindup) {
        sBlendOut = kBlendOutFrames; // interrupted during the start: the pose fades out
    }
}

void ResetRings() {
    if (sState != State::Idle) {
        StopRings(false);
    } else {
        ShowAllModelRings();
    }
    sBlendOut = 0;
    sHeadWeight = 0.0f;
    ::FxReset();
}

// When the ability ends Shadow gets half a second of "invulnerable" (no health loss, no red flashing), so the end never lands
// him in a hit he could not have been hit by a moment ago.
void GraceAfterEnd(Player* player) {
    if (player != nullptr && player->invincibilityTimer > -10) {
        player->invincibilityTimer = -10;
    }
}

// Called when the player presses the button the item is on, or the extra button.
void RequestRings(Player* player) {
    if (!CVarGetInteger(kCvarEnabled, 1)) {
        return;
    }
    if (sState == State::Active) {
        Say("Inhibitor Rings: cancelled");
        StopRings(false); // pressing it again ends it
        GraceAfterEnd(player);
        return;
    }
    if (sState != State::Idle) {
        return;
    }
    const char* why = CannotStart(player);
    if (why != nullptr) {
        Sfx_PlaySfxCentered(NA_SE_SY_ERROR);
        Say(std::string("Inhibitor Rings blocked: ") + why);
        return;
    }
    Say("Inhibitor Rings!");
    BeginRings(player);
}

// ----- Per frame -----------------------------------------------------------------------------------

float Approach(float value, float target, float step) {
    if (value < target) {
        return std::min(target, value + step);
    }
    return std::max(target, value - step);
}

// Standing, walking or running freely on the ground (not Z-targeting, rolling, swinging or anything else).
bool FreeMovementAction(Player* player) {
    const auto action = player->actionFunc;
    return action == Player_Action_Idle || action == Player_Action_80840450 || action == Player_Action_808407CC ||
           action == Player_Action_80840DE4 || action == Player_Action_808414F8 || action == Player_Action_8084170C ||
           action == Player_Action_808417FC || action == Player_Action_80842180;
}

bool OtherSpeedBoss() {
    return (ShadowBoost_IsActive != nullptr && ShadowBoost_IsActive()) ||
           (ShadowHoming_IsBusy != nullptr && ShadowHoming_IsBusy());
}

// The faster run. Runs after Link's own update; his movement next frame uses linearVelocity.
void ApplyRunSpeed(Player* player) {
    const float percent = static_cast<float>(ClampInt(CVarGetInteger(kCvarRunSpeed, 170), 100, 1000));
    const bool grounded = (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) != 0;
    if (percent <= 100.0f || !grounded || !FreeMovementAction(player) || OtherSpeedBoss()) {
        sRunSpeed = player->linearVelocity;
        return;
    }
    const Input& input = gPlayState->state.input[0];
    const float stickX = static_cast<float>(input.cur.stick_x);
    const float stickY = static_cast<float>(input.cur.stick_y);
    const bool stickHeld = std::sqrt(stickX * stickX + stickY * stickY) >= kStickThreshold;
    f32 speedTarget = 0.0f;
    s16 yawTarget = 0;
    Player_GetMovementSpeedAndYaw(player, &speedTarget, &yawTarget, 0.018f, gPlayState);
    // The game brakes (and turns on the spot) when the stick points more than 135 degrees away from where Link runs.
    const bool reversing = stickHeld && std::abs(static_cast<int>(static_cast<s16>(player->yaw - yawTarget))) > 0x6000;

    if (stickHeld && !reversing && speedTarget > 0.0f) {
        const float accel = std::max(0.1f, CVarGetFloat(kCvarRunAccel, 1.5f));
        sRunSpeed = std::max(sRunSpeed, 0.0f);
        sRunSpeed = Approach(sRunSpeed, speedTarget * percent / 100.0f, accel);
        player->linearVelocity = sRunSpeed;
    } else {
        // Stick let go or reversing: the game slows Link down (and turns him around).
        sRunSpeed = player->linearVelocity;
    }
}

void KeepInvulnerable(Player* player) {
    if (!Invulnerable()) {
        sGuardHealth = -1;
        return;
    }
    player->invincibilityTimer = kInvulnerableTimer;
    // Belt and braces: the few things that take health away without going through the player's hit check (heat, direct health
    // changes by some enemies and rooms) are undone here. Only the drain below is allowed to take health.
    if (gSaveContext.healthAccumulator < 0) {
        gSaveContext.healthAccumulator = 0;
    }
    const int health = static_cast<int>(gSaveContext.health);
    if (sGuardHealth < 0) {
        sGuardHealth = health;
    } else if (health < sGuardHealth) {
        gSaveContext.health = static_cast<s16>(sGuardHealth);
    } else {
        sGuardHealth = health; // healed (a heart, a fairy...)
    }
}

void TickWindup(Player* player) {
    const int t = sTick;
    if (t == kVoiceFrame && SoundOn() && sDevice != 0) {
        const Clip& voice = sVoice[sVoicePick].ok ? sVoice[sVoicePick] : sVoice[1 - sVoicePick];
        PlayTrack(kTrackVoice, &voice, false);
    }
    for (int r = 0; r < 4; r++) {
        if (t == sRingFrame[r]) {
            RingOff(player, r);
        }
    }
    if (t == sPowerFrame) {
        Ignite(player);
    }
    sTick++;
    const AnimData& anim = CurrentAnim();
    if (sTick >= anim.frames) {
        // The pose is over (and in case a frame number in the config was wrong, nothing is left undone).
        for (int r = 0; r < 4; r++) {
            if (!sRingHidden[r]) {
                RingOff(player, r);
            }
        }
        Ignite(player);
        EnterActive();
    }
}

void TickActive(Player* player) {
    if (PlayerBusy(player)) {
        return; // talking, a cutscene...: the ability waits (nothing is drained, nothing speeds him up)
    }
    sActiveTicks++;
    const int drainFrames = ClampInt(CVarGetInteger(kCvarDrainFrames, 10), 1, 1000);
    if (++sDrainCount >= drainFrames) {
        sDrainCount = 0;
        const int drain = std::max(0, CVarGetInteger(kCvarDrain, 4));
        const int floorHealth = MinHealth();
        const int health = static_cast<int>(gSaveContext.health);
        const int next = std::max(floorHealth, health - drain);
        if (next < health) {
            gSaveContext.health = static_cast<s16>(next);
        }
        sGuardHealth = next;
        if (next <= floorHealth) {
            Say("Inhibitor Rings: the rings are back");
            StopRings(true);
            GraceAfterEnd(player);
            return;
        }
    }
    ApplyRunSpeed(player);
}

void OnPlayerUpdateRings() {
    // Undo the input lock that was set for this frame's player update.
    if (gPlayState != nullptr && sInputFlagSet) {
        GET_PLAYER(gPlayState)->stateFlags1 &= ~PLAYER_STATE1_INPUT_DISABLED;
    }
    sInputFlagSet = false;

    if (gPlayState == nullptr || !CVarGetInteger(kCvarEnabled, 1)) {
        ResetRings();
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

    switch (sState) {
        case State::Active:
            KeepInvulnerable(player);
            TickActive(player);
            return;
        case State::Windup:
            if (sJustStarted) {
                // The frame the item was used in: the lock and the movement stop start with the next frame.
                sJustStarted = false;
                KeepInvulnerable(player);
                return;
            }
            if (gPlayState->csCtx.state != CS_STATE_IDLE || Player_InCsMode(gPlayState) ||
                gPlayState->transitionTrigger != TRANS_TRIGGER_OFF || (player->stateFlags1 & PLAYER_STATE1_DEAD)) {
                StopRings(false); // a cutscene or a scene change
            } else {
                KeepInvulnerable(player);
                // Hold Link still while the pose plays.
                player->linearVelocity = 0.0f;
                player->actor.speedXZ = 0.0f;
                player->actor.velocity.x = 0.0f;
                player->actor.velocity.z = 0.0f;
                TickWindup(player);
            }
            return;
        case State::Idle:
            break;
    }

    // Optional extra button (off by default).
    const int button = CVarGetInteger(kCvarButton, 0);
    if (button != 0 && CHECK_BTN_ANY(gPlayState->state.input[0].press.button, button)) {
        RequestRings(player);
        sJustStarted = false; // started at the end of this frame already
    }
}

// ----- Damage: three times as much ------------------------------------------------------------------

void ShouldActorUpdateRings(void* refActor, bool* result) {
    (void)result;
    if (gPlayState == nullptr || refActor == nullptr || sState == State::Idle) {
        return;
    }
    Player* player = GET_PLAYER(gPlayState);
    if (player == nullptr) {
        return;
    }
    Actor* actor = static_cast<Actor*>(refActor);

    // The big knockbacks some bosses deal are applied even to a player who cannot be hurt (they are set directly on the player
    // by the attacking actor): taken away right before the player's update.
    if (actor == &player->actor && Invulnerable()) {
        player->knockbackType = 0;
    }

    // Windup: the player gets the game's own "input disabled" flag, so he cannot walk, roll, attack, use items or be steered.
    // The flag is taken off again at the end of the player update.
    if (sState == State::Windup && !sJustStarted && CVarGetInteger(kCvarLock, 1) && actor == &player->actor) {
        if (!(player->stateFlags1 & PLAYER_STATE1_INPUT_DISABLED)) {
            player->stateFlags1 |= PLAYER_STATE1_INPUT_DISABLED;
            sInputFlagSet = true;
        }
        return;
    }

    // The hit that this enemy was dealt in the collision check of the last frame is still in its damage field now (the
    // game clears it after the actor's update). Three times as much.
    if (sState == State::Active && (actor->category == ACTORCAT_ENEMY || actor->category == ACTORCAT_BOSS)) {
        const int scale = ClampInt(CVarGetInteger(kCvarDamage, 3), 1, 50);
        if (scale > 1 && actor->colChkInfo.damage > 0) {
            actor->colChkInfo.damage = static_cast<u8>(std::min(255, static_cast<int>(actor->colChkInfo.damage) * scale));
        }
    }
}

// ----- The pose and the steady head ------------------------------------------------------------------

inline int16_t LerpS16(int16_t a, int16_t b, float w) {
    return static_cast<int16_t>(a + static_cast<int>(std::lround(static_cast<float>(b - a) * w)));
}

// Rotations: blend the short way around the circle.
inline int16_t LerpAngle(int16_t a, int16_t b, float w) {
    const int16_t diff = static_cast<int16_t>(b - a);
    return static_cast<int16_t>(a + static_cast<int>(std::lround(static_cast<float>(diff) * w)));
}

struct Mat3 {
    float m[3][3];
};

// R = Rz * Ry * Rx, the order of the game's limb rotations (angles in s16 units, 65536 = a full circle).
Mat3 RotationZYX(const Vec3s& r) {
    constexpr float kToRadians = 3.14159265f / 32768.0f;
    const float sx = std::sin(r.x * kToRadians), cx = std::cos(r.x * kToRadians);
    const float sy = std::sin(r.y * kToRadians), cy = std::cos(r.y * kToRadians);
    const float sz = std::sin(r.z * kToRadians), cz = std::cos(r.z * kToRadians);
    Mat3 out;
    out.m[0][0] = cz * cy;
    out.m[0][1] = cz * sy * sx - sz * cx;
    out.m[0][2] = cz * sy * cx + sz * sx;
    out.m[1][0] = sz * cy;
    out.m[1][1] = sz * sy * sx + cz * cx;
    out.m[1][2] = sz * sy * cx - cz * sx;
    out.m[2][0] = -sy;
    out.m[2][1] = cy * sx;
    out.m[2][2] = cy * cx;
    return out;
}

Mat3 Multiply(const Mat3& a, const Mat3& b) {
    Mat3 out;
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            out.m[i][j] = a.m[i][0] * b.m[0][j] + a.m[i][1] * b.m[1][j] + a.m[i][2] * b.m[2][j];
        }
    }
    return out;
}

// a transposed times b
Mat3 MultiplyTransposed(const Mat3& a, const Mat3& b) {
    Mat3 out;
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            out.m[i][j] = a.m[0][i] * b.m[0][j] + a.m[1][i] * b.m[1][j] + a.m[2][i] * b.m[2][j];
        }
    }
    return out;
}

// The orientation the head keeps (in the model's own space) while Shadow runs: looking straight ahead, four degrees down,
// no roll, no sway. This is the head of Shadow's standing pose tipped four degrees forward. It is the same orientation the
// boost run uses (make_boost_run.py).
const Mat3 kHeadSteady = { { { 0.041310f, -0.002299f, 0.999144f },
                             { -0.016397f, -0.999864f, -0.001623f },
                             { 0.999012f, -0.016316f, -0.041342f } } };

// Sets the head joint so that the head's orientation is kHeadSteady whatever the body does. table = the joint table.
Vec3s SteadyHead(const Vec3s* table) {
    // Limb 0 (the root) turns by table[1], the upper body (limb 9, which carries the head) by table[10].
    const Mat3 chain = Multiply(RotationZYX(table[1]), RotationZYX(table[10]));
    const Mat3 local = MultiplyTransposed(chain, kHeadSteady);
    constexpr float kToS16 = 32768.0f / 3.14159265f;
    const float y = -std::asin(std::max(-1.0f, std::min(1.0f, local.m[2][0])));
    const float x = std::atan2(local.m[2][1], local.m[2][2]);
    const float z = std::atan2(local.m[1][0], local.m[0][0]);
    Vec3s out;
    out.x = static_cast<int16_t>(std::lround(x * kToS16));
    out.y = static_cast<int16_t>(std::lround(y * kToS16));
    out.z = static_cast<int16_t>(std::lround(z * kToS16));
    return out;
}

// Runs right before the game draws the frame, after the game has put Link's normal pose into his joint table.
void OnPlayDrawBeginRings() {
    if (gPlayState == nullptr) {
        return;
    }
    Player* player = GET_PLAYER(gPlayState);
    if (player == nullptr) {
        return;
    }
    // The game draws Link flashing red while his invincibility timer is above 0: for the draw it is 0 (OnPlayDrawEndRings puts it
    // back before the next update looks at it).
    if (Invulnerable() && player->invincibilityTimer > 0 && !sTimerHidden) {
        sTimerSaved = player->invincibilityTimer;
        player->invincibilityTimer = 0;
        sTimerHidden = true;
    }

    Vec3s* table = player->skelAnime.jointTable;
    if (table == nullptr) {
        return;
    }
    const int joints = std::min<int>(kJoints, player->skelAnime.limbCount);

    // --- The ring removal pose (blended in and out, so there are no jumps).
    const bool posing = sState == State::Windup && !sJustStarted && CVarGetInteger(kCvarAnimation, 1) != 0 &&
                        CurrentAnim().frames > 0;
    const bool fadingOut = !posing && sBlendOut > 0 && sHavePose;
    if (posing || fadingOut) {
        Vec3s target[kJoints];
        float weight;
        if (posing) {
            const AnimData& anim = CurrentAnim();
            const int frame = std::max(0, std::min(sTick - 1, anim.frames - 1));
            const int16_t* src = &anim.data[static_cast<size_t>(frame) * kShortsPerFrame];
            for (int j = 0; j < kJoints; j++) {
                target[j].x = src[j * 3 + 0];
                target[j].y = src[j * 3 + 1];
                target[j].z = src[j * 3 + 2];
            }
            // Optional extra height for the whole pose.
            const int lift = CVarGetInteger(kCvarPoseLift, 0);
            if (lift != 0) {
                target[0].y = static_cast<int16_t>(std::max(-32768, std::min(32767, static_cast<int>(target[0].y) + lift)));
            }
            weight = std::min(1.0f, static_cast<float>(frame + 1) / static_cast<float>(kBlendInFrames));
        } else {
            std::memcpy(target, sLastPose, sizeof(target));
            weight = static_cast<float>(sBlendOut) / static_cast<float>(kBlendOutFrames);
        }
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
        return;
    }

    // --- The steady head while he runs.
    const bool wantHead = sState == State::Active && CVarGetInteger(kCvarHead, 1) != 0 &&
                          FreeMovementAction(player) && player->linearVelocity > 1.0f &&
                          (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) != 0 && !OtherSpeedBoss() &&
                          (player->stateFlags2 & PLAYER_STATE2_DISABLE_DRAW) == 0 && joints > 11;
    sHeadWeight = Approach(sHeadWeight, wantHead ? 1.0f : 0.0f, wantHead ? 0.34f : 0.25f);
    if (sHeadWeight <= 0.0f || joints <= 11) {
        return;
    }
    // While Link runs faster than 5 the game adds its own forward lean to the chest and a counter-tilt plus a steering sway
    // (up to about 20 degrees, it follows the smallest stick movement) to the head, on top of whatever the joint table
    // holds (Player_UpdateLookAngles, applied in the limb override). They are what makes the head bob, so they are taken out.
    // (Only the pitch and roll parts; the yaw parts are the game's look-at turn.)
    const float keep = 1.0f - std::min(1.0f, sHeadWeight);
    player->upperLimbRot.x = static_cast<int16_t>(std::lround(player->upperLimbRot.x * keep));
    player->upperLimbRot.z = static_cast<int16_t>(std::lround(player->upperLimbRot.z * keep));
    player->headLimbRot.x = static_cast<int16_t>(std::lround(player->headLimbRot.x * keep));
    player->headLimbRot.z = static_cast<int16_t>(std::lround(player->headLimbRot.z * keep));
    const Vec3s steady = SteadyHead(table);
    table[11].x = LerpAngle(table[11].x, steady.x, sHeadWeight);
    table[11].y = LerpAngle(table[11].y, steady.y, sHeadWeight);
    table[11].z = LerpAngle(table[11].z, steady.z, sHeadWeight);
}

} // namespace

// Used by Jump.cpp and Boost.cpp: true only while the rings come off (Shadow is held still), so Y and X cannot break out of
// the pose. Once the rings are off (State::Active) Shadow is free.
extern "C" bool ShadowRings_IsBusy(void) {
    return sState == State::Windup;
}

// ------------------------------------------------------------------------------------------------
// The fire look
//
// From the power-up until the ability ends:
//  * A see-through orange copy of Shadow's body (shadow_ghost.o2r), a little bigger than he is, is drawn over him: his body
//    stays visible, but he glows. Two layers (a close bright one and a wider dim one) that flicker.
//  * Flames lick up from his body (head, chest, hands, feet...).
//  * He leaves a trail of fire on the ground where he moves.
//  * A soft orange aura glows behind him; a ring of light goes out at the power-up.
// And while the rings are taken off, every ring drops to the ground as a ring of its own (gold, a little glowing at first).
// ------------------------------------------------------------------------------------------------

static constexpr int kFxMaxFlames = 72;
static constexpr int kFxMaxRings = 4;
static constexpr int kFxFlashFrames = 10;     // the ring of light at the power-up
static constexpr int kFxRingBackFrames = 8;   // the flash when the rings come back

struct FxFlame {
    bool used;
    uint32_t id;
    int age;
    int life;
    Vec3f pos;
    Vec3f vel;
    float size;   // height in world units at the start
    float sway;   // radians
};

struct FxRingProp {
    bool used;
    int age;
    Vec3f pos;
    Vec3f vel;
    Vec3f rot;     // radians
    Vec3f spin;    // radians per game frame
    float floorY;
    float scale;   // 1 for adult, smaller for child
    int bounces;
};

static FxFlame sFxFlames[kFxMaxFlames];
static FxRingProp sFxRings[kFxMaxRings];
static uint32_t sFxNextId = 1;
static uint32_t sFxTick = 0;
static float sFxWeight = 0.0f;       // 0..1: how much of the look is on
static int sFxFlashAge = -2;         // game frames since the power-up (ring of light), -2 = none
static int sFxBackAge = -2;          // game frames since the rings came back, -2 = none
static Vec3f sFxLastTrailPos = { 0.0f, 0.0f, 0.0f };
static bool sFxHaveLastTrailPos = false;
static int sFxGhostSkelState[2] = { -1, -1 }; // adult, child: -1 not checked, 0 missing, 1 found

static bool FxEnabled() {
    return CVarGetInteger(kCvarEnabled, 1) != 0 && CVarGetInteger(kCvarFx, 1) != 0;
}

static bool FxIsChild() {
    return gSaveContext.linkAge == LINK_AGE_CHILD;
}

static u8 FxToByte(float v) {
    return static_cast<u8>(std::lround(std::max(0.0f, std::min(1.0f, v)) * 255.0f));
}

static float FxHash01(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7FEB352Du;
    x ^= x >> 15;
    x *= 0x846CA68Bu;
    x ^= x >> 16;
    return static_cast<float>(x & 0xFFFFFF) / 16777216.0f;
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

static void FxPreload() {
    if (FxEnabled() && CVarGetInteger(kCvarFxGlow, 1) != 0) {
        (void)FxGhostSkeleton(FxIsChild());
    }
}

static void FxReset() {
    std::memset(sFxFlames, 0, sizeof(sFxFlames));
    std::memset(sFxRings, 0, sizeof(sFxRings));
    sFxWeight = 0.0f;
    sFxFlashAge = -2;
    sFxBackAge = -2;
    sFxHaveLastTrailPos = false;
    // Deliberately does NOT touch the player actor or walk the actor lists. This runs from the scene init hook, which the
    // game calls before it has set up the actor lists of the new play state, and from the load and exit hooks, where the
    // play state may already be gone.
}

static FxFlame* FxNewFlame() {
    FxFlame* slot = nullptr;
    for (FxFlame& f : sFxFlames) {
        if (!f.used) {
            slot = &f;
            break;
        }
        if (slot == nullptr || f.age > slot->age) {
            slot = &f;
        }
    }
    std::memset(slot, 0, sizeof(*slot));
    slot->used = true;
    slot->id = sFxNextId++;
    return slot;
}

static void FxSpawnFlame(const Vec3f& pos, float size, int life, float vx, float vy, float vz) {
    FxFlame* f = FxNewFlame();
    f->pos = pos;
    f->size = size;
    f->life = life;
    f->vel = { vx, vy, vz };
    f->sway = RndC(0.5f);
}

static void FxSpawnRingProp(Player* player, int ring) {
    FxRingProp* slot = nullptr;
    for (FxRingProp& r : sFxRings) {
        if (!r.used) {
            slot = &r;
            break;
        }
        if (slot == nullptr || r.age > slot->age) {
            slot = &r;
        }
    }
    std::memset(slot, 0, sizeof(*slot));
    slot->used = true;
    static const int kPart[4] = { PLAYER_BODYPART_L_HAND, PLAYER_BODYPART_R_HAND, PLAYER_BODYPART_L_FOOT,
                                  PLAYER_BODYPART_R_FOOT };
    slot->pos = player->bodyPartsPos[kPart[ring]];
    slot->floorY = player->actor.floorHeight;
    slot->scale = FxIsChild() ? 0.75f : 1.0f;
    const float yaw = static_cast<float>(player->actor.shape.rot.y) * (3.14159265f / 32768.0f);
    const float fx = std::sin(yaw);   // forward
    const float fz = std::cos(yaw);
    const float side = (ring == 0 || ring == 2) ? 1.0f : -1.0f; // +1 = Shadow's left
    const float sx = fz * side;       // sideways (left of Shadow)
    const float sz = -fx * side;
    if (ring < 2) {
        // Pulled off the wrist and dropped: a little out to the side and up.
        slot->vel = { sx * 1.6f + fx * 0.6f, 2.4f, sz * 1.6f + fz * 0.6f };
        slot->spin = { RndC(0.5f), RndC(0.5f), RndC(0.5f) };
    } else {
        // Kicked off the ankle: it flies forward and up.
        slot->vel = { fx * 6.5f + sx * 1.2f, 5.0f, fz * 6.5f + sz * 1.2f };
        slot->spin = { RndC(0.9f), RndC(0.9f), RndC(0.9f) };
    }
    slot->rot = { Rnd01() * 6.28f, Rnd01() * 6.28f, Rnd01() * 6.28f };
}

static void FxOnRingOff(int ring) {
    if (gPlayState == nullptr) {
        return;
    }
    Player* player = GET_PLAYER(gPlayState);
    if (player == nullptr) {
        return;
    }
    FxSpawnRingProp(player, ring);
    // A little burst of sparks where the ring was.
    static const int kPart[4] = { PLAYER_BODYPART_L_HAND, PLAYER_BODYPART_R_HAND, PLAYER_BODYPART_L_FOOT,
                                  PLAYER_BODYPART_R_FOOT };
    if (FxEnabled()) {
        const float s = FxIsChild() ? 0.75f : 1.0f;
        for (int i = 0; i < 4; i++) {
            FxSpawnFlame(player->bodyPartsPos[kPart[ring]], (5.0f + Rnd01() * 5.0f) * s, 8, RndC(1.2f), 1.0f + Rnd01() * 1.5f,
                         RndC(1.2f));
        }
    }
}

static void FxOnIgnite() {
    sFxFlashAge = -1;
    if (gPlayState == nullptr || !FxEnabled()) {
        return;
    }
    Player* player = GET_PLAYER(gPlayState);
    if (player == nullptr) {
        return;
    }
    const float s = FxIsChild() ? 0.75f : 1.0f;
    for (int i = 0; i < 14; i++) {
        const float a = Rnd01() * 6.2831853f;
        Vec3f pos = player->actor.world.pos;
        pos.x += std::cos(a) * 10.0f * s;
        pos.z += std::sin(a) * 10.0f * s;
        FxSpawnFlame(pos, (14.0f + Rnd01() * 12.0f) * s, 12, std::cos(a) * 1.6f, 0.4f, std::sin(a) * 1.6f);
    }
}

static void FxOnEnd() {
    if (sFxWeight > 0.05f) {
        sFxBackAge = -1; // the flash of the rings coming back
    }
}

// Once per game frame, after the player's update.
static void FxTick() {
    const bool on = FxEnabled() && sFireOn;
    sFxWeight = on ? std::min(1.0f, sFxWeight + 0.2f) : std::max(0.0f, sFxWeight - 0.1f);
    if (sFxFlashAge > -2 && ++sFxFlashAge > kFxFlashFrames) {
        sFxFlashAge = -2;
    }
    if (sFxBackAge > -2 && ++sFxBackAge > kFxRingBackFrames) {
        sFxBackAge = -2;
    }

    for (FxFlame& f : sFxFlames) {
        if (!f.used) {
            continue;
        }
        if (++f.age >= f.life) {
            f.used = false;
            continue;
        }
        f.pos.x += f.vel.x;
        f.pos.y += f.vel.y;
        f.pos.z += f.vel.z;
        f.vel.x *= 0.9f;
        f.vel.y *= 0.92f;
        f.vel.z *= 0.9f;
    }

    for (FxRingProp& r : sFxRings) {
        if (!r.used) {
            continue;
        }
        if (++r.age > 90) {
            r.used = false;
            continue;
        }
        r.vel.y -= 1.2f;
        r.pos.x += r.vel.x;
        r.pos.y += r.vel.y;
        r.pos.z += r.vel.z;
        r.rot.x += r.spin.x;
        r.rot.y += r.spin.y;
        r.rot.z += r.spin.z;
        const float rest = r.floorY + 1.5f * r.scale;
        if (r.pos.y < rest) {
            r.pos.y = rest;
            if (r.vel.y < -1.5f && r.bounces < 3) {
                r.vel.y = -r.vel.y * 0.45f;
                r.vel.x *= 0.7f;
                r.vel.z *= 0.7f;
                r.spin.x *= 0.6f;
                r.spin.y *= 0.6f;
                r.spin.z *= 0.6f;
                r.bounces++;
            } else {
                r.vel = { 0.0f, 0.0f, 0.0f };
                r.spin = { 0.0f, 0.0f, 0.0f };
            }
        }
    }

    // The fire trail: flames where he ran.
    if (gPlayState == nullptr) {
        sFxTick++;
        return;
    }
    Player* player = GET_PLAYER(gPlayState);
    const bool trail = on && player != nullptr && CVarGetInteger(kCvarFxTrail, 1) != 0 && sState == State::Active &&
                       (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) != 0 &&
                       (player->stateFlags2 & PLAYER_STATE2_DISABLE_DRAW) == 0 &&
                       gPlayState->pauseCtx.state == 0;
    if (trail) {
        const Vec3f pos = player->actor.world.pos;
        const float s = FxIsChild() ? 0.75f : 1.0f;
        if (!sFxHaveLastTrailPos) {
            sFxLastTrailPos = pos;
            sFxHaveLastTrailPos = true;
        }
        const float dx = pos.x - sFxLastTrailPos.x;
        const float dz = pos.z - sFxLastTrailPos.z;
        const float dist = std::sqrt(dx * dx + dz * dz);
        if (dist > 400.0f) { // teleported
            sFxLastTrailPos = pos;
        } else if (dist >= 7.0f) {
            // One flame every 7 units along the way he came, so a fast run leaves no gaps.
            const int steps = std::min(8, static_cast<int>(dist / 7.0f));
            for (int i = 1; i <= steps; i++) {
                const float t = static_cast<float>(i) / static_cast<float>(steps);
                Vec3f at = { sFxLastTrailPos.x + dx * t, pos.y, sFxLastTrailPos.z + dz * t };
                at.x += RndC(5.0f * s);
                at.z += RndC(5.0f * s);
                FxSpawnFlame(at, (16.0f + Rnd01() * 10.0f) * s, 10 + static_cast<int>(Rnd() % 7), 0.0f, 0.15f, 0.0f);
            }
            sFxLastTrailPos = pos;
        }
    } else {
        sFxHaveLastTrailPos = false;
    }
    sFxTick++;
}

// ----- Drawing helpers -----

// The state of the game's own limb changes while it draws Link, copied for the glow shell.
struct FxShellState {
    bool child;
    float rootDrop;
    Vec3s upperRot;
    Vec3s headRot;
    s16 upperYawSecondary;
};

// The same changes the game makes to Link's root, upper body and head while it draws him (Player_OverrideLimbDrawGameplayCommon).
static s32 FxShellOverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* arg) {
    (void)play;
    (void)dList;
    const FxShellState* g = static_cast<const FxShellState*>(arg);
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

// One layer of the glow: Shadow's body again, scaled up a little around his middle, in flat orange with the model's own
// shading, see-through, depth-tested but not written. (The model's meshes carry no material, so everything that decides how
// it looks is set here.)
static void FxDrawShell(PlayState* play, Player* player, const FlexSkeletonHeader* skeleton, int layer, float scale,
                        u8 alpha, u8 envR, u8 envG, u8 envB, u8 primR, u8 primG, u8 primB) {
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    FxShellState state;
    state.child = FxIsChild();
    state.rootDrop = player->unk_6C4;
    state.upperRot = player->upperLimbRot;
    state.headRot = player->headLimbRot;
    state.upperYawSecondary = player->upperLimbYawSecondary;

    ::FrameInterpolation_RecordOpenChild(reinterpret_cast<const void*>(static_cast<uintptr_t>(0x5B100000u + layer)), 0);
    OPEN_DISPS(gfxCtx);

    Matrix_Push();
    Vec3s rot = player->actor.shape.rot;
    Matrix_SetTranslateRotateYXZ(player->actor.world.pos.x, player->actor.world.pos.y + player->actor.shape.yOffset *
                                                                                              player->actor.scale.y + 1.0f,
                                 player->actor.world.pos.z, &rot);
    Matrix_Scale(player->actor.scale.x, player->actor.scale.y, player->actor.scale.z, MTXMODE_APPLY);
    // Bigger around his middle (model units: the adult is about 6000 tall, the child about 4300).
    const float middle = state.child ? 1900.0f : 2800.0f;
    Matrix_Translate(0.0f, middle, 0.0f, MTXMODE_APPLY);
    Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
    Matrix_Translate(0.0f, -middle, 0.0f, MTXMODE_APPLY);

    gDPPipeSync(POLY_OPA_DISP++);
    gSPTexture(POLY_OPA_DISP++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_OFF);
    gDPSetCombineLERP(POLY_OPA_DISP++, SHADE, 0, ENVIRONMENT, PRIMITIVE, 0, 0, 0, ENVIRONMENT, SHADE, 0, ENVIRONMENT,
                      PRIMITIVE, 0, 0, 0, ENVIRONMENT);
    gDPSetOtherMode(POLY_OPA_DISP++,
                    G_AD_NOTPATTERN | G_CD_MAGICSQ | G_CK_NONE | G_TC_FILT | G_TF_BILERP | G_TT_NONE | G_TL_TILE |
                        G_TD_CLAMP | G_TP_PERSP | G_CYC_1CYCLE | G_PM_NPRIMITIVE,
                    G_AC_NONE | G_ZS_PIXEL | G_RM_AA_ZB_XLU_SURF | G_RM_AA_ZB_XLU_SURF2);
    gSPLoadGeometryMode(POLY_OPA_DISP++, G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, primR, primG, primB, 255);
    gDPSetEnvColor(POLY_OPA_DISP++, envR, envG, envB, alpha);

    SkelAnime_DrawFlexOpa(play, static_cast<void**>(skeleton->sh.segment), player->skelAnime.jointTable,
                          skeleton->dListCount, FxShellOverrideLimbDraw, nullptr, &state);
    Matrix_Pop();

    CLOSE_DISPS(gfxCtx);
    ::FrameInterpolation_RecordCloseChild();
}

// How strongly the fire flickers right now (about 0.75 .. 1.25), different at every game frame.
static float FxFlicker(uint32_t salt) {
    return 0.75f + 0.5f * FxHash01(sFxTick * 2654435761u + salt * 40503u);
}

static void FxDrawGlow(PlayState* play, Player* player) {
    if (!FxEnabled() || CVarGetInteger(kCvarFxGlow, 1) == 0 || sFxWeight <= 0.02f) {
        return;
    }
    const FlexSkeletonHeader* skeleton = FxGhostSkeleton(FxIsChild());
    if (skeleton == nullptr || player->skelAnime.jointTable == nullptr || player->skelAnime.limbCount < kJoints) {
        return;
    }
    const float strength = static_cast<float>(ClampInt(CVarGetInteger(kCvarFxGlowStrength, 100), 0, 300)) / 100.0f;
    const float w = sFxWeight * strength;
    // Wide dim layer first, the close bright one on top of it.
    const float outer = std::min(0.9f, 0.20f * w * FxFlicker(1));
    const float inner = std::min(0.9f, 0.42f * w * FxFlicker(2));
    FxDrawShell(play, player, skeleton, 0, 1.11f, FxToByte(outer), 255, 70, 10, 120, 20, 0);
    FxDrawShell(play, player, skeleton, 1, 1.035f, FxToByte(inner), 255, 140, 30, 150, 50, 0);
}

// A flame: a pointed tongue of fire facing the camera, standing on (x, y, z). Size in world units. alpha 0..1. heat 0..1 (1 =
// white-yellow core, 0 = deep red).
static void FxDrawFlame(PlayState* play, uint32_t tag, float x, float y, float z, float height, float width, float sway,
                        float alpha, float heat) {
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    Vtx* vtx = static_cast<Vtx*>(Graph_Alloc(gfxCtx, sizeof(Vtx) * 7));
    if (vtx == nullptr) {
        return;
    }
    const float h8 = height * 8.0f;
    const float w8 = width * 8.0f;
    struct Point {
        float x, y;
        float r, g, b, a;
    };
    const float h = std::max(0.0f, std::min(1.0f, heat));
    const Point pts[7] = {
        { -w8, 0.0f, 255, 200 + 55 * h, 80 + 140 * h, alpha * 0.95f },
        { w8, 0.0f, 255, 200 + 55 * h, 80 + 140 * h, alpha * 0.95f },
        { -0.85f * w8, 0.35f * h8, 255, 150 + 60 * h, 30 + 40 * h, alpha * 0.9f },
        { 0.85f * w8, 0.35f * h8, 255, 150 + 60 * h, 30 + 40 * h, alpha * 0.9f },
        { -0.4f * w8, 0.68f * h8, 240, 90, 15, alpha * 0.6f },
        { 0.4f * w8, 0.68f * h8, 240, 90, 15, alpha * 0.6f },
        { 0.25f * w8, h8, 190, 40, 0, 0.0f },
    };
    for (int i = 0; i < 7; i++) {
        Vtx_t& v = vtx[i].v;
        v.ob[0] = static_cast<s16>(std::lround(pts[i].x));
        v.ob[1] = static_cast<s16>(std::lround(pts[i].y));
        v.ob[2] = 0;
        v.flag = 0;
        v.tc[0] = 0;
        v.tc[1] = 0;
        v.cn[0] = FxToByte(pts[i].r / 255.0f);
        v.cn[1] = FxToByte(pts[i].g / 255.0f);
        v.cn[2] = FxToByte(pts[i].b / 255.0f);
        v.cn[3] = FxToByte(pts[i].a);
    }

    ::FrameInterpolation_RecordOpenChild(reinterpret_cast<const void*>(static_cast<uintptr_t>(tag)), 0);
    OPEN_DISPS(gfxCtx);
    Matrix_Translate(x, y, z, MTXMODE_NEW);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    Matrix_RotateZ(sway, MTXMODE_APPLY);
    Matrix_Scale(1.0f / 8.0f, 1.0f / 8.0f, 1.0f / 8.0f, MTXMODE_APPLY); // the vertices are in 1/8 world units
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPVertex(POLY_XLU_DISP++, reinterpret_cast<uintptr_t>(vtx), 7, 0);
    gSP2Triangles(POLY_XLU_DISP++, 0, 1, 2, 0, 1, 3, 2, 0);
    gSP2Triangles(POLY_XLU_DISP++, 2, 3, 4, 0, 3, 5, 4, 0);
    gSP1Triangle(POLY_XLU_DISP++, 4, 5, 6, 0);
    CLOSE_DISPS(gfxCtx);
    ::FrameInterpolation_RecordCloseChild();
}

// Sets up the see-through vertex-colour drawing that the flames, the aura and the ring props use.
static void FxBeginXlu(PlayState* play) {
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    OPEN_DISPS(gfxCtx);
    // What we draw now is in colour, not in the world's greyscale (the time stop or the boost may have turned it on).
    gSPGrayscale(POLY_OPA_DISP++, false);
    gSPGrayscale(POLY_XLU_DISP++, false);
    Gfx_SetupDL_4Xlu(gfxCtx);
    gDPSetCombineMode(POLY_XLU_DISP++, G_CC_SHADE, G_CC_SHADE);
    gSPClearGeometryMode(POLY_XLU_DISP++, G_LIGHTING | G_CULL_BOTH);
    CLOSE_DISPS(gfxCtx);
}

// Flames on his body: one at each of these body parts, new random places every game frame (the frame interpolation smooths
// them). The base is the body part, the tongue rises from it.
static void FxDrawBodyFlames(PlayState* play, Player* player) {
    if (!FxEnabled() || CVarGetInteger(kCvarFxFlames, 1) == 0 || sFxWeight <= 0.02f) {
        return;
    }
    struct Part {
        int part;
        float size; // flame height for the adult, in world units
    };
    static const Part kParts[] = {
        { PLAYER_BODYPART_TORSO, 16.0f },      { PLAYER_BODYPART_WAIST, 13.0f },     { PLAYER_BODYPART_HEAD, 11.0f },
        { PLAYER_BODYPART_L_HAND, 13.0f },     { PLAYER_BODYPART_R_HAND, 13.0f },    { PLAYER_BODYPART_L_FOREARM, 10.0f },
        { PLAYER_BODYPART_R_FOREARM, 10.0f },  { PLAYER_BODYPART_L_SHOULDER, 10.0f }, { PLAYER_BODYPART_R_SHOULDER, 10.0f },
        { PLAYER_BODYPART_L_THIGH, 11.0f },    { PLAYER_BODYPART_R_THIGH, 11.0f },   { PLAYER_BODYPART_L_SHIN, 11.0f },
        { PLAYER_BODYPART_R_SHIN, 11.0f },     { PLAYER_BODYPART_L_FOOT, 12.0f },    { PLAYER_BODYPART_R_FOOT, 12.0f },
    };
    const float s = FxIsChild() ? 0.72f : 1.0f;
    const float weight = sFxWeight;
    const bool moving = player->linearVelocity > 3.0f;
    FxBeginXlu(play);
    int index = 0;
    for (const Part& p : kParts) {
        const uint32_t salt = static_cast<uint32_t>(index * 977 + 13);
        const float flick = FxFlicker(salt);
        const Vec3f& base = player->bodyPartsPos[p.part];
        const float jx = (FxHash01(sFxTick * 31u + salt) - 0.5f) * 7.0f * s;
        const float jz = (FxHash01(sFxTick * 57u + salt * 3u) - 0.5f) * 7.0f * s;
        float height = p.size * s * flick * (moving ? 1.15f : 1.0f);
        const float sway = (FxHash01(sFxTick * 19u + salt * 7u) - 0.5f) * 0.7f;
        FxDrawFlame(play, 0x5B200000u + static_cast<uint32_t>(index), base.x + jx, base.y - 2.0f * s, base.z + jz, height,
                    height * 0.34f, sway, 0.85f * weight, 0.55f * FxHash01(sFxTick * 11u + salt));
        index++;
    }
}

// The fire trail on the ground.
static void FxDrawTrail(PlayState* play) {
    if (!FxEnabled() || CVarGetInteger(kCvarFxTrail, 1) == 0) {
        return;
    }
    bool any = false;
    for (const FxFlame& f : sFxFlames) {
        any = any || f.used;
    }
    if (!any) {
        return;
    }
    FxBeginXlu(play);
    for (const FxFlame& f : sFxFlames) {
        if (!f.used) {
            continue;
        }
        const float t = static_cast<float>(f.age) / static_cast<float>(std::max(1, f.life));
        const float flick = FxFlicker(f.id);
        const float height = f.size * (1.0f - 0.7f * t) * flick;
        const float alpha = std::pow(1.0f - t, 0.8f) * 0.95f;
        if (alpha < 0.03f || height < 1.0f) {
            continue;
        }
        const float sway = f.sway + (FxHash01(sFxTick * 23u + f.id) - 0.5f) * 0.5f;
        FxDrawFlame(play, 0x5B300000u + f.id, f.pos.x, f.pos.y, f.pos.z, height, height * 0.36f, sway, alpha, 1.0f - t);
    }
}

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

// The soft aura behind Shadow, the ring of light at the power-up, the flash when the rings come back.
static void FxDrawAura(PlayState* play, const Player* player) {
    if (!FxEnabled() || CVarGetInteger(kCvarFxAura, 1) == 0) {
        return;
    }
    const bool flash = sFxFlashAge >= 0 && sFxFlashAge <= kFxFlashFrames;
    const bool back = sFxBackAge >= 0 && sFxBackAge <= kFxRingBackFrames;
    const float weight = std::min(1.0f, sFxWeight);
    if (weight <= 0.02f && !flash && !back) {
        return;
    }
    const bool child = FxIsChild();
    const float flashT = flash ? 1.0f - static_cast<float>(sFxFlashAge) / static_cast<float>(kFxFlashFrames) : 0.0f;
    const float backT = back ? 1.0f - static_cast<float>(sFxBackAge) / static_cast<float>(kFxRingBackFrames) : 0.0f;
    const float flick = FxFlicker(99);
    const float radius = (child ? 46.0f : 60.0f) * (0.95f + 0.1f * flick + 0.8f * flashT);
    const float alpha = std::min(0.9f, (0.36f * flick * weight) + 0.5f * flashT + 0.35f * backT);

    // The middle of the aura: his chest, pushed a little away from the camera, so his body is in front of it.
    Vec3f centre = player->actor.world.pos;
    centre.y += child ? 26.0f : 38.0f;
    const Vec3f& eye = play->view.eye;
    const float vx = centre.x - eye.x;
    const float vy = centre.y - eye.y;
    const float vz = centre.z - eye.z;
    const float length = std::sqrt(vx * vx + vy * vy + vz * vz);
    if (length > 1.0f) {
        const float back2 = child ? 12.0f : 18.0f;
        centre.x += vx / length * back2;
        centre.y += vy / length * back2;
        centre.z += vz / length * back2;
    }

    static const int sAuraTag = 0;
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    FxBeginXlu(play);
    ::FrameInterpolation_RecordOpenChild(&sAuraTag, 0);
    OPEN_DISPS(gfxCtx);

    Matrix_Translate(centre.x, centre.y, centre.z, MTXMODE_NEW);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    Matrix_Scale(1.0f / 8.0f, 1.0f / 8.0f, 1.0f / 8.0f, MTXMODE_APPLY); // the vertices are in 1/8 world units
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);

    // Bright orange in the middle, fading to nothing at the edge. (Gold when the rings come back.)
    const float r8 = radius * 8.0f;
    const u8 gr = back ? 210 : 110;
    const u8 bl = back ? 60 : 10;
    const FxBandRow aura[3] = {
        { 0.0f, 255, 200, 90, FxToByte(alpha) },
        { r8 * 0.42f, 255, gr, bl, FxToByte(alpha * 0.6f) },
        { r8, 255, 50, 0, 0 },
    };
    FxDrawBand(gfxCtx, aura, 3, 12);

    // The ring of light that goes out at the power-up.
    if (flash) {
        const float progress = static_cast<float>(sFxFlashAge) / static_cast<float>(kFxFlashFrames);
        const float ringRadius = (child ? 46.0f : 60.0f) * (0.5f + 2.4f * progress) * 8.0f;
        const float half = ringRadius * 0.2f + 24.0f;
        const float ringAlpha = 0.85f * (1.0f - progress);
        const FxBandRow ring[3] = {
            { ringRadius - half, 255, 190, 110, 0 },
            { ringRadius, 255, 190, 110, FxToByte(ringAlpha) },
            { ringRadius + half, 255, 190, 110, 0 },
        };
        FxDrawBand(gfxCtx, ring, 3, 24);
    }

    CLOSE_DISPS(gfxCtx);
    ::FrameInterpolation_RecordCloseChild();
}

// ----- The rings that fall to the ground -----

static constexpr int kFxTorusSegments = 8;
static constexpr int kFxTorusSides = 4;
static constexpr int kFxTorusVertices = kFxTorusSegments * kFxTorusSides; // 32: the most one vertex load takes

static void FxDrawRingProps(PlayState* play) {
    bool any = false;
    for (const FxRingProp& r : sFxRings) {
        any = any || r.used;
    }
    if (!any) {
        return;
    }
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    FxBeginXlu(play);
    int slot = 0;
    for (const FxRingProp& r : sFxRings) {
        slot++;
        if (!r.used) {
            continue;
        }
        // Gold with a glow that dies down in the first second; the last half second it fades away.
        const float glow = std::max(0.0f, 1.0f - static_cast<float>(r.age) / 24.0f);
        const float fade = r.age > 70 ? std::max(0.0f, 1.0f - static_cast<float>(r.age - 70) / 20.0f) : 1.0f;
        const u8 alpha = FxToByte(fade);
        if (alpha < 4) {
            continue;
        }
        Vtx* vtx = static_cast<Vtx*>(Graph_Alloc(gfxCtx, sizeof(Vtx) * kFxTorusVertices));
        if (vtx == nullptr) {
            continue;
        }
        // A ring: radius 4.5 world units (36 in 1/8 units), a tube 1.3 units thick, built from 8 x 4 points. The shading is
        // fixed in the ring's own space (the outer side is brighter), which is good enough for a ring that tumbles.
        const float ringRadius = 36.0f;
        const float tube = 10.0f;
        for (int i = 0; i < kFxTorusSegments; i++) {
            const float a = 6.2831853f * static_cast<float>(i) / static_cast<float>(kFxTorusSegments);
            for (int j = 0; j < kFxTorusSides; j++) {
                const float b = 6.2831853f * static_cast<float>(j) / static_cast<float>(kFxTorusSides) + 0.785398f;
                const float rr = ringRadius + tube * std::cos(b);
                Vtx_t& v = vtx[i * kFxTorusSides + j].v;
                v.ob[0] = static_cast<s16>(std::lround(rr * std::cos(a)));
                v.ob[1] = static_cast<s16>(std::lround(rr * std::sin(a)));
                v.ob[2] = static_cast<s16>(std::lround(tube * std::sin(b)));
                v.flag = 0;
                v.tc[0] = 0;
                v.tc[1] = 0;
                const float shade = 0.62f + 0.38f * std::cos(b - 0.6f);
                v.cn[0] = FxToByte(std::min(1.0f, (0.98f * shade) + 0.35f * glow));
                v.cn[1] = FxToByte(std::min(1.0f, (0.76f * shade) + 0.10f * glow));
                v.cn[2] = FxToByte(std::min(1.0f, 0.16f * shade));
                v.cn[3] = alpha;
            }
        }
        ::FrameInterpolation_RecordOpenChild(reinterpret_cast<const void*>(static_cast<uintptr_t>(0x5B400000u + slot)), 0);
        OPEN_DISPS(gfxCtx);
        Matrix_Translate(r.pos.x, r.pos.y, r.pos.z, MTXMODE_NEW);
        Matrix_RotateY(r.rot.y, MTXMODE_APPLY);
        Matrix_RotateX(r.rot.x, MTXMODE_APPLY);
        Matrix_RotateZ(r.rot.z, MTXMODE_APPLY);
        const float s = r.scale / 8.0f;
        Matrix_Scale(s, s, s, MTXMODE_APPLY);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPVertex(POLY_XLU_DISP++, reinterpret_cast<uintptr_t>(vtx), kFxTorusVertices, 0);
        for (int i = 0; i < kFxTorusSegments; i++) {
            const int i2 = (i + 1) % kFxTorusSegments;
            for (int j = 0; j < kFxTorusSides; j++) {
                const int j2 = (j + 1) % kFxTorusSides;
                const int a = i * kFxTorusSides + j;
                const int b = i * kFxTorusSides + j2;
                const int c = i2 * kFxTorusSides + j;
                const int d = i2 * kFxTorusSides + j2;
                gSP2Triangles(POLY_XLU_DISP++, a, c, b, 0, b, c, d, 0);
            }
        }
        CLOSE_DISPS(gfxCtx);
        ::FrameInterpolation_RecordCloseChild();
    }
}

// Runs at the end of the world's draw, before the HUD: the glow, the aura, the flames, the trail and the rings on the ground.
static void FxDrawEnd() {
    if (gPlayState == nullptr || gSaveContext.gameMode != GAMEMODE_NORMAL) {
        return;
    }
    Player* player = GET_PLAYER(gPlayState);
    if (player == nullptr || gPlayState->pauseCtx.state != 0) {
        return;
    }
    // The falling rings are drawn whatever the settings (they are part of taking the rings off).
    const bool hidden = (player->stateFlags2 & PLAYER_STATE2_DISABLE_DRAW) != 0; // the homing attack hides Shadow
    if (FxEnabled() && !hidden) {
        FxDrawGlow(gPlayState, player);
        FxDrawAura(gPlayState, player);
        FxDrawBodyFlames(gPlayState, player);
    }
    if (FxEnabled()) {
        FxDrawTrail(gPlayState);
    }
    FxDrawRingProps(gPlayState);
}

// Runs when the world's draw is done: the timer goes back, then the fire look is drawn.
static void OnPlayDrawEndRings() {
    if (sTimerHidden) {
        sTimerHidden = false;
        if (gPlayState != nullptr) {
            Player* player = GET_PLAYER(gPlayState);
            if (player != nullptr && player->invincibilityTimer == 0) {
                player->invincibilityTimer = sTimerSaved;
            }
        }
    }
    FxDrawEnd();
}

static void RegisterRings() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayerUpdate>(OnPlayerUpdateRings);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::ShouldActorUpdate>(ShouldActorUpdateRings);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayDrawBegin>(OnPlayDrawBeginRings);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayDrawEnd>(OnPlayDrawEndRings);
    // After the main update above (hooks run in the order they were added): the look's own once-per-frame step.
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayerUpdate>(FxTick);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneInit>([](int16_t) { ResetRings(); });
    // The pause menu does not run player updates, so end the ability (and its music) when it opens.
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnKaleidoscopeUpdate>([](int16_t) { ResetRings(); });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnLoadGame>([](int32_t) {
        ResetRings();
        sFxGhostSkelState[0] = sFxGhostSkelState[1] = -1; // look for shadow_ghost.o2r again
        LoadAssets();
    });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnExitGame>([](int32_t) { ResetRings(); });

    // Using the item: the C button it is on. This replaces Din's Fire's spell.
    REGISTER_VB_SHOULD(VB_OVERRIDE_BUTTON_ITEM_USED, {
        (void)should;
        (void)va_arg(args, s32*); // button index
        Player* player = va_arg(args, Player*);
        s32* item = va_arg(args, s32*);
        if (*item == ITEM_DINS_FIRE && !IS_RANDO && CVarGetInteger(kCvarItem, 1) && CVarGetInteger(kCvarEnabled, 1)) {
            *item = ITEM_NONE_FE; // the game then does nothing else with this button press
            RequestRings(player);
        }
    });

    // He cannot be hurt by a fall either.
    REGISTER_VB_SHOULD(VB_RECIEVE_FALL_DAMAGE, {
        if (Invulnerable()) {
            *should = false;
        }
    });
}

static RegisterShipInitFunc sRingsInit(RegisterRings);

// Read by the status report in Jump.cpp: tells it that this file is in the build, and which version it is.
// 1 = the first version.
extern "C" {
int ShadowAbilityVersion_Rings = 1;
}
