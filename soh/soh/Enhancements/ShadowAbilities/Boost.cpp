// Shadow the Hedgehog - Boost (the X button) for Ship of Harkinian 9.3.1
//
// Version 9 (v9: when the boost will not start, the on-screen message names the reason (first-person view, ready to fire, shield, ...); v8: the boost can be started and keeps going with the boomerang in hand; v7: at the drop the world turns greyscale (the HUD and Shadow keep their colour), Shadow leaves blue see-through after images and a soft blue aura that pulses when each one appears; v6: the charge-up pose is a mirror image of Chaos Control's, so the spin turns the other way and ends facing the other side; the pose now really drives the head and right hand; v5: speed 20, enemies and NPCs at half speed during the boost; v4: the game's own run lean and steering sway are taken off the head and chest while the run pose shows). Press X on the ground and the boost starts the way Chaos Control does: Shadow stops and plays the Chaos
// Control pose while the boost tune plays (the tune starts with Shadow saying "Chaos Control", with the same timing as
// the Chaos Control music). When the tune drops (about 2.5 seconds into it) Shadow bursts forward in his fast skating
// run at a much higher speed than Link's normal run, and the tune plays on until the boost ends. Steer with the stick.
//
//   Tap X     : the boost starts at the drop and stays on until you press X again.
//   Hold X    : if X is still held down at the drop, the boost lasts as long as you hold it.
//
// The Y jump (Jump.cpp) does not stop the boost: Shadow keeps his speed through the jump (and the air jump) and carries on
// running when he lands.
//
// The boost ends by itself when: X is let go (hold) or pressed again (tap), Link starts a roll / sword swing / anything else,
// starts Z-targeting, gets hurt in the charge-up, runs into a wall for a moment, or the stick has been let go and he has
// stopped. Chaos Control's own pose ends it as well.
//
// The poses and the tune are in shadow_boost_audio.o2r (custom/boost/*). Without that file the boost still works, just
// without the poses and the tune (and then without the charge-up).
//
// Everything is in this one file. CMake picks it up automatically (soh/*.cpp is globbed).
//
// Settings (CVars, change with the in-game console: set <name> <value>):
//   gShadow.Boost.Enabled        1 = on (default), 0 = off
//   gShadow.Boost.SdlButton      controller button, default 2 (X). 0 = A, 1 = B, 3 = Y, 9 = left shoulder, 10 = right shoulder
//   gShadow.Boost.N64Button      also accept this N64 button number from the game's own controller mapping, default 0 = none
//   gShadow.Boost.BlockButtons   N64 buttons that are hidden from the game while X is held, default 16384 (the B button).
//                                SoH's default controller layout puts the sword (B) on the X button; this stops the boost
//                                from swinging the sword. Set 0 if X is not also your sword button and you want to swing while boosting.
//   gShadow.Boost.Mode           0 = tap or hold (default), 1 = always toggle (press X again to stop), 2 = always hold
//   gShadow.Boost.Speed          boost speed, default 20 (Link's normal run is about 6.6; a sword lunge is 15; v3 was 13)
//   gShadow.Boost.Accel          how fast the speed builds up per game frame, default 4
//   gShadow.Boost.SlowOthers     1 = enemies, bosses and other NPCs move at half speed while the boost is on (default), 0 = off
//   gShadow.Boost.Windup         1 = the Chaos Control style charge-up with the tune (default), 0 = no charge-up, boost at once
//   gShadow.Boost.Animation      1 = use the poses (default), 0 = only the speed
//   gShadow.Boost.RunRate        playback speed of the skating run in percent, default 100
//   gShadow.Boost.Sound          1 = play the tune (default), 0 = silent
//   gShadow.Boost.Volume         tune volume 0-100, default 80
//   gShadow.Boost.DropOffset     move the moment the boost starts, in milliseconds (added to the drop time of the tune)
//   gShadow.Boost.SyncOffset     move the tune against the pose, in milliseconds (negative = tune earlier)
//   gShadow.Boost.Effect         1 = the boost look (greyscale world, blue after images, aura) on (default), 0 = off
//   gShadow.Boost.Greyscale      1 = the world turns greyscale while boosting (default), 0 = no greyscale
//   gShadow.Boost.Afterimages    1 = blue after images behind Shadow (default), 0 = none (needs shadow_ghost.o2r)
//   gShadow.Boost.AfterimageInterval  game frames between two after images, default 2
//   gShadow.Boost.AfterimageLife      game frames an after image lasts, default 10
//   gShadow.Boost.AfterimageOpacity   opacity of a new after image in percent, default 55
//   gShadow.Boost.Aura           1 = the soft blue aura around Shadow (default), 0 = none
//   gShadow.Boost.AuraStrength   aura strength in percent, default 100
//   gShadow.Boost.ItemsInHand    1 = the boost works with the boomerang in hand (default), 0 = it refuses while the boomerang
//                                is the item in hand (the rule up to version 7). A slingshot, hookshot or bow that is out but
//                                resting in the hand never blocked it. Only aiming blocks it (the item button held down:
//                                first-person view, or "ready to fire"), and the shield held up.
//   gShadow.Boost.Debug          1 = show a small on-screen message when X is pressed (started / why it did not start)
//                                and when a boost ends (why). Default 1 while the boost is being tested, 0 = silent.

#include <libultraship/bridge/consolevariablebridge.h>

#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"
#include "soh/Notification/Notification.h"

#include <SDL2/SDL.h>
#include <ship/Context.h>
#include <ship/resource/File.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/ArchiveManager.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <sstream>
#include <cstdio>
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
// The action Link is in while he is in the air after a jump or a fall.
void Player_Action_8084411C(Player* player, PlayState* play);
}

// Provided by ChaosControl.cpp. True while Chaos Control holds Link still. Weak, so this file also works without it.
extern "C" __attribute__((weak)) bool ShadowChaosControl_IsBusy(void);

// Provided by HomingAttack.cpp (version 2 and later). IsBusy: a homing attack is running. EnemiesAtFullSpeed: the same, plus
// three frames after it, so a hit gets processed. Weak, so this file also works without HomingAttack.cpp.
extern "C" __attribute__((weak)) bool ShadowHoming_IsBusy(void);
extern "C" __attribute__((weak)) bool ShadowHoming_EnemiesAtFullSpeed(void);

namespace {

constexpr const char* kCvarEnabled = "gShadow.Boost.Enabled";
constexpr const char* kCvarSdlButton = "gShadow.Boost.SdlButton";
constexpr const char* kCvarN64Button = "gShadow.Boost.N64Button";
constexpr const char* kCvarBlock = "gShadow.Boost.BlockButtons";
constexpr const char* kCvarMode = "gShadow.Boost.Mode";
constexpr const char* kCvarSpeed = "gShadow.Boost.Speed";
constexpr const char* kCvarAccel = "gShadow.Boost.Accel";
constexpr const char* kCvarWindup = "gShadow.Boost.Windup";
constexpr const char* kCvarAnimation = "gShadow.Boost.Animation";
constexpr const char* kCvarRunRate = "gShadow.Boost.RunRate";
constexpr const char* kCvarSound = "gShadow.Boost.Sound";
constexpr const char* kCvarVolume = "gShadow.Boost.Volume";
constexpr const char* kCvarDropOffset = "gShadow.Boost.DropOffset";
constexpr const char* kCvarSyncOffset = "gShadow.Boost.SyncOffset";
constexpr const char* kCvarDebug = "gShadow.Boost.Debug";
constexpr const char* kCvarSlowOthers = "gShadow.Boost.SlowOthers";
constexpr const char* kCvarItems = "gShadow.Boost.ItemsInHand";
constexpr const char* kCvarFx = "gShadow.Boost.Effect";
constexpr const char* kCvarFxGrey = "gShadow.Boost.Greyscale";
constexpr const char* kCvarFxAfter = "gShadow.Boost.Afterimages";
constexpr const char* kCvarFxInterval = "gShadow.Boost.AfterimageInterval";
constexpr const char* kCvarFxLife = "gShadow.Boost.AfterimageLife";
constexpr const char* kCvarFxOpacity = "gShadow.Boost.AfterimageOpacity";
constexpr const char* kCvarFxAura = "gShadow.Boost.Aura";
constexpr const char* kCvarFxAuraStrength = "gShadow.Boost.AuraStrength";
constexpr const char* kCvarFxAuraPulse = "gShadow.Boost.AuraPulse";

constexpr const char* kFileRunAdult = "custom/boost/anim_run_adult.bin";
constexpr const char* kFileRunChild = "custom/boost/anim_run_child.bin";
constexpr const char* kFilePoseAdult = "custom/boost/pose_adult.bin";            // the Chaos Control pose
constexpr const char* kFilePoseChild = "custom/boost/pose_child.bin";
constexpr const char* kFilePoseAdultCC = "custom/chaos_control/anim_adult.bin";  // fallback: the one in chaos_control_audio.o2r
constexpr const char* kFilePoseChildCC = "custom/chaos_control/anim_child.bin";
constexpr const char* kFileMusic = "custom/boost/music.wav";
constexpr const char* kFileSongConfig = "custom/boost/song_config.txt";
// The see-through copy of Shadow's model for the after images (shadow_ghost.o2r).
constexpr const char* kGhostSkelAdult = "objects/object_link_boy/gLinkAdultGhostSkel";
constexpr const char* kGhostSkelChild = "objects/object_link_child/gLinkChildGhostSkel";

constexpr uint32_t kPressLifetimeMs = 150; // a press seen by the event listener counts for this long
constexpr float kChildSpeedScale = 0.9f;
constexpr float kPoseSpeed = 5.5f;         // below this speed the game's own pose is shown
constexpr float kBurstStartSpeed = 7.0f;   // the speed the burst starts with at the drop
constexpr int kMaxAirFrames = 3;           // game frames in the air (not jumping) before the boost ends
constexpr int kMaxJumpAirFrames = 100;     // game frames in the air after a jump before the boost ends
constexpr int kLandingGraceFrames = 24;    // game frames after landing before Link is free to run again
constexpr int kStuckFrames = 8;            // game frames against a wall before the boost ends
constexpr int kNeutralFrames = 14;         // game frames standing still with the stick let go before the boost ends
constexpr int kFadeOutFrames = 4;
constexpr int kBlendInFrames = 3;          // the charge-up pose fades in over this many frames
constexpr int kMorphFrames = 3;            // the charge-up pose turns into the run over this many frames
constexpr int kFramesPerSecond = 20;
constexpr int kMsPerFrame = 1000 / kFramesPerSecond;
constexpr int kMaxWindupFrames = 60 * kFramesPerSecond; // safety: the burst starts after this long even without a drop
constexpr float kStickThreshold = 22.0f;  // the game ignores a stick below about 20

// Joint-table entries the game keeps for itself: the pose leaves them alone. The table is numbered like PLAYER_LIMB_*
// (entry 0 = root position, entry 1 = root limb, 2 = waist ... 11 = head, 12 = hat, 13 = collar, 19 = right hand,
// 20 = sheath, 21 = torso), so this keeps the hat, collar, sheath and torso. (Up to v5 the numbers were one too low:
// the head and right hand stayed under the game's control and the pose never turned the head.)
inline bool KeepGameLimb(int limb) {
    return limb == 12 || limb == 13 || limb == 20 || limb == 21;
}

// ------------------------------------------------------------------------------------------------
// Reading the X button
// ------------------------------------------------------------------------------------------------

std::atomic<int> sWatchedButton{ SDL_CONTROLLER_BUTTON_X };
std::atomic<bool> sPressPending{ false };
std::atomic<uint32_t> sPressTick{ 0 };
std::atomic<bool> sEventHeld{ false }; // the button is down, according to the button down / up events
bool sWatchAdded = false;
bool sPolledDown = false;

// Sees every button press and release the moment it happens. The hold state comes from here as well as from looking
// at the controller, because looking at the controller does not always work (then the boost would end the frame after
// it started, as if X had been let go).
int SDLCALL OnSdlEvent(void*, SDL_Event* event) {
    switch (event->type) {
        case SDL_CONTROLLERBUTTONDOWN:
            if (event->cbutton.button == sWatchedButton.load()) {
                sPressTick = SDL_GetTicks();
                sPressPending = true;
                sEventHeld = true;
            }
            break;
        case SDL_CONTROLLERBUTTONUP:
            if (event->cbutton.button == sWatchedButton.load()) {
                sEventHeld = false;
            }
            break;
        case SDL_CONTROLLERDEVICEREMOVED:
            sEventHeld = false;
            break;
        case SDL_WINDOWEVENT:
            if (event->window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                sEventHeld = false;
            }
            break;
        default:
            break;
    }
    return 0;
}

bool AnyControllerHolds(int button) {
    const int count = SDL_NumJoysticks();
    for (int i = 0; i < count; i++) {
        if (!SDL_IsGameController(i)) {
            continue;
        }
        SDL_GameController* controller = SDL_GameControllerFromInstanceID(SDL_JoystickGetDeviceInstanceID(i));
        if (controller != nullptr && SDL_GameControllerGetButton(controller, static_cast<SDL_GameControllerButton>(button))) {
            return true;
        }
    }
    return false;
}

void EnsureWatch() {
    if (!sWatchAdded) {
        SDL_AddEventWatch(OnSdlEvent, nullptr);
        sWatchAdded = true;
    }
    sWatchedButton = CVarGetInteger(kCvarSdlButton, SDL_CONTROLLER_BUTTON_X);
}

bool sHeld = false; // X is down right now (read once per game frame)

// Reads the button once per game frame. Returns true once per press.
bool ReadButton() {
    EnsureWatch();
    const int button = CVarGetInteger(kCvarSdlButton, SDL_CONTROLLER_BUTTON_X);

    bool pressed = false;
    if (sPressPending.exchange(false) && SDL_GetTicks() - sPressTick.load() <= kPressLifetimeMs) {
        pressed = true;
    }
    const bool down = AnyControllerHolds(button);
    if (down && !sPolledDown) {
        pressed = true;
    }
    sPolledDown = down;
    sHeld = down || sEventHeld.load();

    const int n64Button = CVarGetInteger(kCvarN64Button, 0);
    if (n64Button != 0 && gPlayState != nullptr) {
        if (CHECK_BTN_ANY(gPlayState->state.input[0].press.button, n64Button)) {
            pressed = true;
        }
        if (CHECK_BTN_ANY(gPlayState->state.input[0].cur.button, n64Button)) {
            sHeld = true;
        }
    }
    return pressed;
}

// ------------------------------------------------------------------------------------------------
// Audio: a tiny mixer on its own SDL audio device (44.1 kHz stereo, 16 bit), one track for the tune.
// ------------------------------------------------------------------------------------------------

constexpr int kRate = 44100;
constexpr int kChannels = 2;
constexpr float kMusicLevel = 0.6f;

struct Clip {
    std::vector<int16_t> pcm; // interleaved stereo
    bool ok = false;
};

struct Track {
    const Clip* clip = nullptr;
    size_t pos = 0; // in interleaved samples
    size_t loopPos = 0; // when the end is reached while the track is not fading out, it continues here (0 = no loop)
    float gain = 0.0f;
    float target = 0.0f;
    float step = 0.0f; // gain change per interleaved sample while fading
    bool playing = false;
};

Clip sMusic;
Track sTrack;
SDL_AudioDeviceID sDevice = 0;
bool sAssetsTried = false;
int sDropMs = 0;            // music time of the beat drop (the boost starts)
int sControlMs = -1;        // music time where the shouted word CONTROL starts; -1 = unknown (music starts at once)
int sLoopMs = -1;           // music time the tune jumps back to if it ever reaches its end; -1 = the drop time
int sAnimControlFrame = 50; // frame of the original (SA2) animation where CONTROL is shouted
int sAnimSourceFps = 24;    // frame rate of the original (SA2) animation

void SDLCALL MixCallback(void*, Uint8* stream, int len) {
    int16_t* out = reinterpret_cast<int16_t*>(stream);
    const int count = len / static_cast<int>(sizeof(int16_t));
    for (int i = 0; i < count; i++) {
        float acc = 0.0f;
        Track& tr = sTrack;
        if (tr.playing) {
            if (tr.clip == nullptr) {
                tr.playing = false;
            } else if (tr.pos >= tr.clip->pcm.size()) {
                if (tr.loopPos > 0 && tr.loopPos < tr.clip->pcm.size() && tr.target > 0.0f) {
                    tr.pos = tr.loopPos;
                } else {
                    tr.playing = false;
                }
            }
        }
        if (tr.playing) {
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
void PlayMusic(int skipMs = 0) {
    if (sDevice == 0 || !sMusic.ok) {
        return;
    }
    const float level = kMusicLevel * MasterVolume();
    size_t startPos = static_cast<size_t>(std::max(0, skipMs)) * static_cast<size_t>(kRate * kChannels) / 1000;
    startPos &= ~static_cast<size_t>(1); // stay on a stereo frame boundary
    const int loopMs = sLoopMs >= 0 ? sLoopMs : sDropMs;
    size_t loopPos = static_cast<size_t>(std::max(0, loopMs)) * static_cast<size_t>(kRate * kChannels) / 1000;
    loopPos &= ~static_cast<size_t>(1);
    SDL_LockAudioDevice(sDevice);
    sTrack.clip = &sMusic;
    sTrack.pos = std::min(startPos, sMusic.pcm.size());
    sTrack.loopPos = loopPos;
    sTrack.gain = level;
    sTrack.target = level;
    sTrack.step = 0.0f;
    sTrack.playing = true;
    SDL_UnlockAudioDevice(sDevice);
}

void FadeMusic(float seconds) {
    if (sDevice == 0) {
        return;
    }
    SDL_LockAudioDevice(sDevice);
    if (sTrack.playing) {
        if (seconds <= 0.0f || sTrack.gain <= 0.0f) {
            sTrack.playing = false;
        } else {
            sTrack.target = 0.0f;
            sTrack.step = sTrack.gain / (seconds * static_cast<float>(kRate * kChannels));
        }
    }
    SDL_UnlockAudioDevice(sDevice);
}

bool MusicPlaying() {
    if (sDevice == 0) {
        return false;
    }
    SDL_LockAudioDevice(sDevice);
    const bool playing = sTrack.playing;
    SDL_UnlockAudioDevice(sDevice);
    return playing;
}

// Where the tune is, in ms (after it looped, this is the position in the clip).
int MusicMs() {
    if (sDevice == 0) {
        return 0;
    }
    SDL_LockAudioDevice(sDevice);
    const size_t pos = sTrack.pos;
    SDL_UnlockAudioDevice(sDevice);
    return static_cast<int>(pos * 1000 / static_cast<size_t>(kRate * kChannels));
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
// The poses: the Chaos Control pose (SA2 animation 180, the charge-up) and the skating run (SA2 animation 174, with a
// steady head). Link's 21 limbs, 20 frames per second.
// One frame = 22 Vec3s (root position, then 21 limb rotations) + 1 face value, 67 big-endian s16.
// ------------------------------------------------------------------------------------------------

constexpr int kJoints = 22;
constexpr int kShortsPerFrame = kJoints * 3 + 1;

struct AnimData {
    std::vector<int16_t> data;
    int frames = 0;
};

AnimData sPoseAdult, sPoseChild, sRunAdult, sRunChild;

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
        } else if (key == "loop_ms") {
            sLoopMs = value;
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

    auto music = LoadModFile(kFileMusic);
    if (music != nullptr) {
        DecodeWav(*music->Buffer, sMusic);
    }
    auto config = LoadModFile(kFileSongConfig);
    if (config != nullptr) {
        ParseConfig(std::string(config->Buffer->begin(), config->Buffer->end()));
    }
    LoadAnim(kFilePoseAdult, sPoseAdult);
    if (sPoseAdult.frames <= 0) {
        LoadAnim(kFilePoseAdultCC, sPoseAdult);
    }
    LoadAnim(kFilePoseChild, sPoseChild);
    if (sPoseChild.frames <= 0) {
        LoadAnim(kFilePoseChildCC, sPoseChild);
    }
    LoadAnim(kFileRunAdult, sRunAdult);
    LoadAnim(kFileRunChild, sRunChild);
}

bool IsChild() {
    return gSaveContext.linkAge == LINK_AGE_CHILD;
}

const AnimData& PoseAnim() {
    return IsChild() ? sPoseChild : sPoseAdult;
}

const AnimData& RunAnim() {
    return IsChild() ? sRunChild : sRunAdult;
}

bool HaveRun() {
    return RunAnim().frames > 0;
}

bool HavePose() {
    return PoseAnim().frames > 0;
}

inline int16_t LerpS16(int16_t a, int16_t b, float w) {
    return static_cast<int16_t>(a + static_cast<int>(std::lround(static_cast<float>(b - a) * w)));
}

// Rotations: blend the short way around the circle.
inline int16_t LerpAngle(int16_t a, int16_t b, float w) {
    const int16_t diff = static_cast<int16_t>(b - a);
    return static_cast<int16_t>(a + static_cast<int>(std::lround(static_cast<float>(diff) * w)));
}

// The pose at frame time t of an animation (between two frames it is blended). A loop wraps around.
void SamplePose(const AnimData& anim, float t, bool wrap, Vec3s out[kJoints]) {
    const int frames = anim.frames;
    int i0 = static_cast<int>(std::floor(t));
    const float frac = t - static_cast<float>(i0);
    int i1 = i0 + 1;
    if (wrap) {
        i0 = ((i0 % frames) + frames) % frames;
        i1 = ((i1 % frames) + frames) % frames;
    } else {
        i0 = std::max(0, std::min(i0, frames - 1));
        i1 = std::max(0, std::min(i1, frames - 1));
    }
    const int16_t* a = &anim.data[static_cast<size_t>(i0) * kShortsPerFrame];
    const int16_t* b = &anim.data[static_cast<size_t>(i1) * kShortsPerFrame];
    for (int j = 0; j < kJoints; j++) {
        if (j == 0) { // root position
            out[j].x = LerpS16(a[0], b[0], frac);
            out[j].y = LerpS16(a[1], b[1], frac);
            out[j].z = LerpS16(a[2], b[2], frac);
        } else {
            out[j].x = LerpAngle(a[j * 3 + 0], b[j * 3 + 0], frac);
            out[j].y = LerpAngle(a[j * 3 + 1], b[j * 3 + 1], frac);
            out[j].z = LerpAngle(a[j * 3 + 2], b[j * 3 + 2], frac);
        }
    }
}

// ------------------------------------------------------------------------------------------------
// The boost
// ------------------------------------------------------------------------------------------------

enum class State { Idle, Windup, Active };

State sState = State::Idle;
float sAnimT = 0.0f;         // frame time in the run loop
float sBoostSpeed = 0.0f;    // the speed we are driving Link at
float sWeight = 0.0f;        // how much of the pose is shown (0 = the game's own pose, 1 = ours)
int sAirFrames = 0;
int sStuckCount = 0;
int sNeutralCount = 0;
float sLastX = 0.0f;
float sLastZ = 0.0f;
bool sLatched = false;       // the boost was started with a tap: X has to be pressed again to stop it
bool sWasJump = false;       // Link jumped during this boost and has not run freely again yet
int sLandingFrames = 0;
int sMorphLeft = 0;          // frames left of the morph from the charge-up pose into the run

// The charge-up (same timing logic as Chaos Control).
int sWindupFrames = 0;       // frames since the press (the frame of the pose)
int sPoseTick = 0;           // frame number of the pose that is shown
bool sMusicStarted = false;
int sMusicStartMs = 0;       // time after the press when the tune starts
int sAnimDelayFrames = 0;    // frames the pose waits before it starts (only when the tune has to start first)
bool sInputFlagSet = false;  // we set PLAYER_STATE1_INPUT_DISABLED for this frame and have to clear it again
s16 sStartHealth = 0;

int sFadeOut = 0;            // frames left of blending the pose out after the boost ended
float sFadeWeight = 0.0f;
bool sHavePose = false;
Vec3s sLastPose[kJoints];    // the pose that was shown last (before blending with the game's own pose)

// ------------------------------------------------------------------------------------------------
// The boost look (v7): greyscale world, blue after images, soft aura. State only; the code is further down.
// ------------------------------------------------------------------------------------------------

constexpr int kMaxGhosts = 16;
constexpr int kFxFadeFrames = 5;       // the greyscale and the aura fade in and out over this many game frames
constexpr int kDropFlashFrames = 9;    // the flash and the ring at the drop

// One after image: Shadow's place and pose when it was made. It stays where it is (in the world) and fades out.
struct Ghost {
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

Ghost sGhosts[kMaxGhosts];
uint32_t sGhostNextId = 1;
int sSinceSpawn = 0;          // game frames since the last after image
Vec3f sLastGhostPos = { 0.0f, 0.0f, 0.0f };
bool sHaveLastGhostPos = false;
float sFxWeight = 0.0f;       // 0..1: how much of the look is on (greyscale amount, aura strength)
std::chrono::steady_clock::time_point sPulseAt; // when the aura last pulsed (a new after image, or the drop)
bool sPulseOn = false;
int sDropAge = -2;            // game frames since the drop (flash and ring), -2 = none
uint32_t sFxTick = 0;         // counts game frames
uint32_t sFxSpawnTick = 0xFFFFFFFFu;
bool sGrayOn = false;         // greyscale was switched on in this frame's display lists
bool sGrayWasOn = false;      // ... and in the frame before
ActorFunc sOrigPlayerDraw = nullptr;
int sGhostSkelState[2] = { -1, -1 }; // adult, child: -1 not checked, 0 missing, 1 found

void FxOnDrop();
void FxReset();
void FxPreload();

// Hiding the sword button while X is held (see gShadow.Boost.BlockButtons).
bool sInputHidden = false;
u16 sHiddenCur = 0;
u16 sHiddenPress = 0;

bool ChaosControlBusy() {
    return ShadowChaosControl_IsBusy != nullptr && ShadowChaosControl_IsBusy();
}

bool SoundOn() {
    return CVarGetInteger(kCvarSound, 1) != 0;
}

// A small on-screen message (bottom right, the game's own notification box) while the boost is being tested.
void Say(const std::string& text) {
    if (CVarGetInteger(kCvarDebug, 1) != 0) {
        Notification::Emit({ .message = text, .remainingTime = 5.0f, .mute = true });
    }
}

// nullptr when Link can boost, otherwise why not: Link is not in control or is in a state where boosting makes no sense.
const char* CannotBoost(Player* player) {
    if (gPlayState->csCtx.state != CS_STATE_IDLE || Player_InCsMode(gPlayState)) {
        return "cutscene";
    }
    if (gPlayState->pauseCtx.state != 0) {
        return "menu open";
    }
    if (gPlayState->transitionTrigger != TRANS_TRIGGER_OFF) {
        return "scene change";
    }
    if (gPlayState->msgCtx.msgMode != MSGMODE_NONE) {
        return "text box";
    }
    const u32 blocked1 = PLAYER_STATE1_INPUT_DISABLED | PLAYER_STATE1_TALKING | PLAYER_STATE1_DEAD |
                         PLAYER_STATE1_GETTING_ITEM | PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_IN_WATER |
                         PLAYER_STATE1_ON_HORSE | PLAYER_STATE1_CLIMBING_LEDGE | PLAYER_STATE1_HANGING_OFF_LEDGE |
                         PLAYER_STATE1_CLIMBING_LADDER | PLAYER_STATE1_DAMAGED | PLAYER_STATE1_FIRST_PERSON |
                         PLAYER_STATE1_CARRYING_ACTOR | PLAYER_STATE1_HOOKSHOT_FALLING | PLAYER_STATE1_LOADING |
                         PLAYER_STATE1_FLOOR_DISABLED | PLAYER_STATE1_READY_TO_FIRE | PLAYER_STATE1_SHIELDING |
                         PLAYER_STATE1_START_CHANGING_HELD_ITEM;
    const u32 blocked2 = PLAYER_STATE2_UNDERWATER | PLAYER_STATE2_DIVING | PLAYER_STATE2_CRAWLING |
                         PLAYER_STATE2_GRABBED_BY_ENEMY | PLAYER_STATE2_FROZEN | PLAYER_STATE2_OCARINA_PLAYING |
                         PLAYER_STATE2_STATIONARY_LADDER;
    if (player->stateFlags1 & blocked1) {
        // Say which state it is (version 9), so a report of "it will not start" shows the cause on screen.
        const u32 f = player->stateFlags1;
        if (f & PLAYER_STATE1_FIRST_PERSON) {
            return "first-person view (aiming)";
        }
        if (f & PLAYER_STATE1_READY_TO_FIRE) {
            return "aiming (bow / slingshot / hookshot ready to fire)";
        }
        if (f & PLAYER_STATE1_SHIELDING) {
            return "shield held up";
        }
        if (f & PLAYER_STATE1_CARRYING_ACTOR) {
            return "carrying something";
        }
        if (f & PLAYER_STATE1_START_CHANGING_HELD_ITEM) {
            return "changing the held item";
        }
        if (f & PLAYER_STATE1_DAMAGED) {
            return "hurt";
        }
        static char sWhy[64];
        std::snprintf(sWhy, sizeof(sWhy), "Link state (flags1 %08X)", static_cast<unsigned>(f & blocked1));
        return sWhy;
    }
    // Items in hand do not stop the boost (version 8). PLAYER_STATE1_USING_BOOMERANG is set the whole time the
    // boomerang is the item in hand (taken out, in flight, being caught), not only while it is being thrown. Aiming
    // with it (the item button held down) puts Link into the first-person view, which stays blocked above.
    // gShadow.Boost.ItemsInHand = 0 brings the old rule back.
    if ((player->stateFlags1 & PLAYER_STATE1_USING_BOOMERANG) && CVarGetInteger(kCvarItems, 1) == 0) {
        return "boomerang in hand";
    }
    if (player->stateFlags2 & blocked2) {
        return "Link state (flags2)";
    }
    if (player->meleeWeaponState != 0) {
        return "sword swing";
    }
    return nullptr;
}

// Standing, walking or running freely on the ground (not Z-targeting, rolling, swinging or anything else).
bool FreeMovementAction(Player* player) {
    const auto action = player->actionFunc;
    return action == Player_Action_Idle || action == Player_Action_80840450 || action == Player_Action_808407CC ||
           action == Player_Action_80840DE4 || action == Player_Action_808414F8 || action == Player_Action_8084170C ||
           action == Player_Action_808417FC || action == Player_Action_80842180;
}

// In the air after a jump (the Y jump, the air jump, or the game's own jump).
bool JumpingInAir(Player* player) {
    return player->actionFunc == Player_Action_8084411C && (player->stateFlags1 & PLAYER_STATE1_JUMPING) != 0 &&
           (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) == 0;
}

float TargetSpeed() {
    const float speed = std::max(0.0f, CVarGetFloat(kCvarSpeed, 20.0f));
    return IsChild() ? speed * kChildSpeedScale : speed;
}

int AnimControlMs() {
    return sAnimControlFrame * 1000 / std::max(1, sAnimSourceFps);
}

// The boost itself starts here (at the beat drop, or at once without the charge-up).
void EnterActive(Player* player, bool afterWindup) {
    sState = State::Active;
    sAnimT = 0.0f;
    sAirFrames = 0;
    sStuckCount = 0;
    sNeutralCount = 0;
    sWasJump = false;
    sLandingFrames = 0;
    sLastX = player->actor.world.pos.x;
    sLastZ = player->actor.world.pos.z;
    // The burst: Link starts at a good speed (the stick decides where he goes).
    sBoostSpeed = std::max(player->linearVelocity, std::min(kBurstStartSpeed, TargetSpeed()));
    player->linearVelocity = sBoostSpeed;
    // The charge-up pose turns into the run. Without a charge-up pose the run fades in.
    sMorphLeft = sHavePose ? kMorphFrames : 0;
    sWeight = sHavePose ? 1.0f : 0.0f;
    // Tap or hold: after the charge-up, X still held at the drop means "hold"; X already let go means the boost stays on
    // until X is pressed again. Without a charge-up the press is the start, so it is "hold" unless the mode says toggle.
    const int mode = CVarGetInteger(kCvarMode, 0);
    sLatched = (mode == 1) || (mode == 0 && afterWindup && !sHeld);
    Say(sLatched ? "Boost! (press X to stop)" : "Boost! (hold X)");
    FxOnDrop();
}

void BeginBoost(Player* player) {
    if (!sAssetsTried) {
        LoadAssets();
    }
    sFadeOut = 0;
    sHavePose = false;
    sWeight = 0.0f;
    sInputFlagSet = false;
    sStartHealth = gSaveContext.health;
    FxPreload(); // the see-through model is read during the charge-up, not at the drop

    const bool windup = CVarGetInteger(kCvarWindup, 1) != 0;
    const bool haveMusic = windup && SoundOn() && sMusic.ok && EnsureDevice();
    const bool havePose = windup && CVarGetInteger(kCvarAnimation, 1) != 0 && HavePose();
    sMusicStarted = false;
    sMusicStartMs = 0;
    sAnimDelayFrames = 0;
    sWindupFrames = 1; // the frame the button is pressed in is frame 0
    sPoseTick = 0;

    if (!haveMusic && !havePose) {
        // Nothing to charge up with: boost right away.
        EnterActive(player, false);
        return;
    }
    if (haveMusic) {
        // The tune has to start so that the word CONTROL is shouted exactly on frame 50 of the pose.
        int musicStart = 0;
        if (havePose && sControlMs >= 0) {
            musicStart = AnimControlMs() - sControlMs + CVarGetInteger(kCvarSyncOffset, 0);
        }
        if (musicStart >= 0) {
            sMusicStartMs = musicStart;
        } else {
            // CONTROL comes earlier in the tune than in the pose: the pose has to wait instead.
            sMusicStartMs = 0;
            sAnimDelayFrames = (-musicStart + kMsPerFrame - 1) / kMsPerFrame;
        }
    }
    sState = State::Windup;
    sLatched = false;
}

// fade: blend the pose out over a few frames (false = it disappears at once).
void EndBoost(bool fade, const char* why = nullptr) {
    if (sState == State::Idle) {
        return;
    }
    if (why != nullptr) {
        Say(std::string("Boost ended: ") + why);
    }
    const bool wasWindup = (sState == State::Windup);
    sState = State::Idle;
    sWasJump = false;
    sMorphLeft = 0;
    sFadeWeight = sWeight;
    sFadeOut = (fade && sHavePose && sWeight > 0.05f) ? kFadeOutFrames : 0;
    FadeMusic(wasWindup ? 0.3f : 0.9f);
}

void ResetBoost() {
    EndBoost(false);
    FadeMusic(0.2f);
    sFadeOut = 0;
    sHavePose = false;
    sWeight = 0.0f;
    sPolledDown = false;
    sPressPending = false;
    sEventHeld = false;
    FxReset();
}

void RestoreHiddenButtons() {
    if (!sInputHidden) {
        return;
    }
    sInputHidden = false;
    if (gPlayState == nullptr) {
        return;
    }
    Input* input = &gPlayState->state.input[0];
    input->cur.button |= sHiddenCur;
    input->press.button |= sHiddenPress;
}

// Runs right before an actor updates. For Link: during the charge-up the game's own input lock (Link cannot walk, roll,
// attack or use items), otherwise hide the sword button while X is held, so a boost is not a sword swing.
// While the boost is on, enemies, bosses and other NPCs only get an update every second game frame, so they move,
// animate and act at half speed. (Their hit boxes are only active in the frames they update, the same as in Chaos Control.)
bool SlowedByBoost(const Actor* actor, const Player* player) {
    if (actor == &player->actor || actor == player->heldActor || actor == player->naviActor ||
        actor->parent == &player->actor) {
        return false;
    }
    switch (actor->category) {
        case ACTORCAT_ENEMY:
        case ACTORCAT_BOSS:
        case ACTORCAT_NPC:
            return true;
        default:
            return actor->id == ACTOR_EN_NUTSBALL; // deku scrub nuts
    }
}

void ShouldActorUpdateBoost(void* refActor, bool* result) {
    if (gPlayState == nullptr || refActor == nullptr || !CVarGetInteger(kCvarEnabled, 1)) {
        return;
    }
    Player* player = GET_PLAYER(gPlayState);
    if (player == nullptr) {
        return;
    }
    if (static_cast<Actor*>(refActor) != &player->actor) {
        // Not during a homing attack (version 12): the game throws an enemy's damage away in a frame in which it does not
        // update, and the hit of the attack lands one frame after it is registered, so at half speed it would do nothing.
        if (sState == State::Active && (gPlayState->gameplayFrames & 1) != 0 && CVarGetInteger(kCvarSlowOthers, 1) != 0 &&
            !(ShadowHoming_EnemiesAtFullSpeed != nullptr && ShadowHoming_EnemiesAtFullSpeed()) &&
            SlowedByBoost(static_cast<Actor*>(refActor), player)) {
            *result = false;
        }
        return;
    }
    if (sState == State::Windup) {
        if (!(player->stateFlags1 & PLAYER_STATE1_INPUT_DISABLED)) {
            player->stateFlags1 |= PLAYER_STATE1_INPUT_DISABLED;
            sInputFlagSet = true;
        }
        return;
    }
    const int block = CVarGetInteger(kCvarBlock, BTN_B);
    if (block == 0) {
        return;
    }
    // Looks at the button right now (not at the last game frame), so the very first frame of a press is covered too.
    EnsureWatch();
    Input* input = &gPlayState->state.input[0];
    const int n64Button = CVarGetInteger(kCvarN64Button, 0);
    const bool held = AnyControllerHolds(CVarGetInteger(kCvarSdlButton, SDL_CONTROLLER_BUTTON_X)) ||
                      sEventHeld.load() || (sPressPending.load() && SDL_GetTicks() - sPressTick.load() <= kPressLifetimeMs) ||
                      (n64Button != 0 && CHECK_BTN_ANY(input->cur.button, n64Button));
    if (!held) {
        return;
    }
    sHiddenCur = static_cast<u16>(input->cur.button & block);
    sHiddenPress = static_cast<u16>(input->press.button & block);
    input->cur.button &= ~block;
    input->press.button &= ~block;
    sInputHidden = true;
}

float Approach(float value, float target, float step) {
    if (value < target) {
        return std::min(target, value + step);
    }
    return std::max(target, value - step);
}

// While the airborne code of the game clamps Link's speed to the normal running speed, let the boost speed through.
void OnLimitJumpSpeed(bool* should) {
    if (sState == State::Active && CVarGetInteger(kCvarEnabled, 1)) {
        *should = false;
    }
}

// One game frame of the charge-up. The tune starts, the pose advances, and at the beat drop the boost begins.
void TickWindup(Player* player) {
    const int t = sWindupFrames;
    const int ms = t * kMsPerFrame;
    const bool haveMusic = SoundOn() && sMusic.ok && sDevice != 0;

    if (haveMusic && !sMusicStarted && ms >= sMusicStartMs) {
        PlayMusic(ms - sMusicStartMs);
        sMusicStarted = true;
    }
    sPoseTick = std::max(0, t - sAnimDelayFrames);
    sWindupFrames++;

    bool go = false;
    if (sMusicStarted) {
        const int dropMs = sDropMs + CVarGetInteger(kCvarDropOffset, 0);
        go = MusicMs() >= dropMs || !MusicPlaying();
    } else if (!haveMusic) {
        // No tune: wait until the pose is finished.
        const AnimData& anim = PoseAnim();
        go = anim.frames <= 0 || t - sAnimDelayFrames >= anim.frames - 1;
    }
    if (go || sWindupFrames > kMaxWindupFrames) {
        EnterActive(player, true);
    }
}

void UpdateBoost() {
    if (gPlayState == nullptr) {
        return;
    }
    // Undo the input lock that was set for this frame's player update.
    if (sInputFlagSet) {
        GET_PLAYER(gPlayState)->stateFlags1 &= ~PLAYER_STATE1_INPUT_DISABLED;
    }
    sInputFlagSet = false;
    RestoreHiddenButtons();
    const bool pressed = ReadButton(); // always read, so old presses never pile up

    if (!CVarGetInteger(kCvarEnabled, 1)) {
        EndBoost(false);
        sFadeOut = 0;
        return;
    }
    Player* player = GET_PLAYER(gPlayState);
    if (player == nullptr) {
        return;
    }
    const bool onGround = (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) != 0;

    // ----- not boosting: wait for the button -----
    if (sState == State::Idle) {
        if (sFadeOut > 0) {
            sFadeOut--;
        }
        if (pressed) {
            const char* why = nullptr;
            if (!onGround) {
                why = "not on the ground";
            } else if (ChaosControlBusy()) {
                why = "Chaos Control pose is playing";
            } else if ((why = CannotBoost(player)) != nullptr) {
                // why says it
            } else if (!FreeMovementAction(player)) {
                static char sBusy[96];
                std::snprintf(sBusy, sizeof(sBusy), "Link is busy (Z-targeting, rolling, swinging... flags1 %08X)",
                              static_cast<unsigned>(player->stateFlags1));
                why = sBusy;
            }
            if (why == nullptr) {
                BeginBoost(player);
                if (sState == State::Windup) {
                    Say("Boost charging...");
                }
            } else {
                Say(std::string("Boost blocked: ") + why);
            }
        }
        return;
    }

    // ----- the charge-up: Link is held still while the pose plays and the tune builds up to the drop -----
    if (sState == State::Windup) {
        if (ChaosControlBusy()) {
            EndBoost(false, "Chaos Control pose started");
            return;
        }
        const char* why = CannotBoost(player);
        if (why != nullptr || gSaveContext.health < sStartHealth) {
            EndBoost(true, why != nullptr ? why : "Link got hurt");
            return;
        }
        player->linearVelocity = 0.0f;
        player->actor.speedXZ = 0.0f;
        player->actor.velocity.x = 0.0f;
        player->actor.velocity.z = 0.0f;
        TickWindup(player);
        return;
    }

    // ----- boosting -----
    if (ChaosControlBusy()) {
        EndBoost(false, "Chaos Control pose started");
        return;
    }
    if (const char* why = CannotBoost(player)) {
        EndBoost(true, why);
        return;
    }

    // Jumping does not stop the boost: Link keeps his speed in the air and runs on after he has landed.
    const bool jumping = JumpingInAir(player);
    if (jumping) {
        sWasJump = true;
        sLandingFrames = 0;
    }
    const bool free = FreeMovementAction(player);
    if (free) {
        if (sWasJump) {
            // Back on his feet after a jump: pick the speed up again.
            sBoostSpeed = std::max(player->linearVelocity, std::min(kBurstStartSpeed, TargetSpeed()));
            player->linearVelocity = sBoostSpeed;
            sWasJump = false;
        }
        sLandingFrames = 0;
    } else if (!(sWasJump && (jumping || (onGround && ++sLandingFrames <= kLandingGraceFrames)))) {
        EndBoost(true, "Link is busy (Z-targeting, rolling, swinging...)");
        return;
    }
    const bool inJump = sWasJump && !free; // in the air after a jump, or landing from one

    const bool toggleStop = sLatched ? pressed : !sHeld;
    if (toggleStop) {
        EndBoost(true, sLatched ? "X pressed again" : "X let go");
        return;
    }
    if (onGround) {
        sAirFrames = 0;
    } else if (jumping) {
        if (++sAirFrames >= kMaxJumpAirFrames) {
            EndBoost(true, "in the air too long");
            return;
        }
    } else if (++sAirFrames >= kMaxAirFrames) {
        EndBoost(true, "left the ground");
        return;
    }

    // Animation clock: the run loops.
    sAnimT += static_cast<float>(std::max(10, CVarGetInteger(kCvarRunRate, 100))) / 100.0f;
    const float runFrames = static_cast<float>(std::max(1, RunAnim().frames));
    while (sAnimT >= runFrames) {
        sAnimT -= runFrames;
    }
    if (sMorphLeft > 0) {
        sMorphLeft--;
    }

    // The stick: held, and which way (relative to the camera).
    const Input& input = gPlayState->state.input[0];
    const float stickX = static_cast<float>(input.cur.stick_x);
    const float stickY = static_cast<float>(input.cur.stick_y);
    const bool stickHeld = std::sqrt(stickX * stickX + stickY * stickY) >= kStickThreshold;
    f32 speedTarget = 0.0f;
    s16 yawTarget = 0;
    Player_GetMovementSpeedAndYaw(player, &speedTarget, &yawTarget, 0.018f, gPlayState);
    // The game brakes (and turns on the spot) when the stick points more than 135 degrees away from where Link runs.
    const bool reversing = stickHeld && std::abs(static_cast<int>(static_cast<s16>(player->yaw - yawTarget))) > 0x6000;

    // A homing attack (HomingAttack.cpp) is running: it sets Shadow's position and speed itself (the vanish, the strike, the
    // bounce), so the boost leaves them alone until it is over (version 12). The boost carries on afterwards at its speed.
    if (ShadowHoming_IsBusy != nullptr && ShadowHoming_IsBusy()) {
        sAirFrames = 0;
        sStuckCount = 0;
        sNeutralCount = 0;
        sLastX = player->actor.world.pos.x;
        sLastZ = player->actor.world.pos.z;
        return;
    }

    // The speed. This runs after Link's own update; his movement next frame uses linearVelocity.
    if (inJump) {
        // In the air: keep the speed Link had (the game would slow him down), unless the stick says "turn around".
        if (jumping && !reversing) {
            player->linearVelocity = sBoostSpeed;
        }
        sStuckCount = 0;
        sNeutralCount = 0;
    } else if (stickHeld && !reversing) {
        sBoostSpeed = Approach(sBoostSpeed, TargetSpeed(), std::max(0.1f, CVarGetFloat(kCvarAccel, 4.0f)));
        player->linearVelocity = sBoostSpeed;

        // Pushing against a wall: give up after a moment.
        const float dx = player->actor.world.pos.x - sLastX;
        const float dz = player->actor.world.pos.z - sLastZ;
        const float moved = std::sqrt(dx * dx + dz * dz);
        if (sBoostSpeed > 8.0f && moved < sBoostSpeed * 0.3f) {
            if (++sStuckCount >= kStuckFrames) {
                EndBoost(true, "ran into a wall");
                return;
            }
        } else {
            sStuckCount = 0;
        }
    } else {
        // Stick let go or reversing: the game slows Link down (and turns him around).
        sBoostSpeed = player->linearVelocity;
        sStuckCount = 0;
    }
    sLastX = player->actor.world.pos.x;
    sLastZ = player->actor.world.pos.z;

    if (!inJump) {
        if (stickHeld) {
            sNeutralCount = 0;
        } else if (player->linearVelocity < 1.0f && ++sNeutralCount >= kNeutralFrames) {
            EndBoost(true, "stick let go");
            return;
        }
    }

    // How much of the pose to show: the run once Link is moving fast on the ground. In the air after a jump the jump
    // pose (Jump.cpp) is shown instead.
    const bool wantPose = !inJump && (sMorphLeft > 0 || player->linearVelocity >= kPoseSpeed);
    sWeight = Approach(sWeight, wantPose ? 1.0f : 0.0f, wantPose ? 0.5f : 0.34f);
}

// Puts the boost poses into Link's joint table. Runs right before the game draws the frame, after the game has put
// Link's normal pose into his joint table.
void ApplyBoostPose() {
    if (gPlayState == nullptr) {
        return;
    }
    const bool windup = sState == State::Windup;
    const bool active = sState == State::Active;
    const bool fading = sState == State::Idle && sFadeOut > 0 && sHavePose;
    if (!windup && !active && !fading) {
        return;
    }
    Player* player = GET_PLAYER(gPlayState);
    if (player == nullptr || player->skelAnime.jointTable == nullptr) {
        return;
    }

    Vec3s target[kJoints];
    float weight;
    if (windup) {
        if (CVarGetInteger(kCvarAnimation, 1) == 0 || !HavePose()) {
            return;
        }
        const AnimData& anim = PoseAnim();
        const int frame = std::max(0, std::min(sPoseTick, anim.frames - 1));
        const int16_t* src = &anim.data[static_cast<size_t>(frame) * kShortsPerFrame];
        for (int j = 0; j < kJoints; j++) {
            target[j].x = src[j * 3 + 0];
            target[j].y = src[j * 3 + 1];
            target[j].z = src[j * 3 + 2];
        }
        weight = std::min(1.0f, static_cast<float>(sPoseTick + 1) / static_cast<float>(kBlendInFrames));
        sWeight = weight;
        std::memcpy(sLastPose, target, sizeof(target));
        sHavePose = true;
    } else if (active) {
        if (CVarGetInteger(kCvarAnimation, 1) == 0 || !HaveRun() || sWeight <= 0.0f) {
            return;
        }
        SamplePose(RunAnim(), sAnimT, true, target);
        // Turning from the charge-up pose into the run: blend from the last charge-up pose.
        if (sMorphLeft > 0 && sHavePose) {
            const float w = 1.0f - static_cast<float>(sMorphLeft) / static_cast<float>(kMorphFrames + 1);
            for (int j = 0; j < kJoints; j++) {
                if (j == 0) {
                    target[j].x = LerpS16(sLastPose[j].x, target[j].x, w);
                    target[j].y = LerpS16(sLastPose[j].y, target[j].y, w);
                    target[j].z = LerpS16(sLastPose[j].z, target[j].z, w);
                } else {
                    target[j].x = LerpAngle(sLastPose[j].x, target[j].x, w);
                    target[j].y = LerpAngle(sLastPose[j].y, target[j].y, w);
                    target[j].z = LerpAngle(sLastPose[j].z, target[j].z, w);
                }
            }
        }
        std::memcpy(sLastPose, target, sizeof(target));
        sHavePose = true;
        weight = sWeight;
    } else {
        std::memcpy(target, sLastPose, sizeof(target));
        weight = sFadeWeight * static_cast<float>(sFadeOut) / static_cast<float>(kFadeOutFrames);
    }
    if (weight <= 0.0f) {
        return;
    }

    // While Link runs faster than 5 the game adds its own forward lean to the chest and a counter-tilt plus a steering
    // sway (up to about 20 degrees, it follows the smallest stick movement) to the head, on top of whatever pose the
    // joint table holds (Player_UpdateLookAngles / func_8083DDC8, applied in the limb override). The head of the boost run
    // is placed with those left out, so they would tilt it down and make it shake. They are taken out while the pose shows.
    // (Only the pitch and roll parts; the yaw parts are the game's look-at turn.)
    {
        const float keep = 1.0f - std::min(1.0f, weight);
        player->upperLimbRot.x = static_cast<int16_t>(std::lround(player->upperLimbRot.x * keep));
        player->upperLimbRot.z = static_cast<int16_t>(std::lround(player->upperLimbRot.z * keep));
        player->headLimbRot.x = static_cast<int16_t>(std::lround(player->headLimbRot.x * keep));
        player->headLimbRot.z = static_cast<int16_t>(std::lround(player->headLimbRot.z * keep));
    }

    const int joints = std::min<int>(kJoints, player->skelAnime.limbCount);
    Vec3s* table = player->skelAnime.jointTable;
    for (int j = 0; j < joints; j++) {
        if (KeepGameLimb(j)) {
            continue;
        }
        if (j == 0) { // root position
            table[j].x = LerpS16(table[j].x, target[j].x, weight);
            table[j].y = LerpS16(table[j].y, target[j].y, weight);
            table[j].z = LerpS16(table[j].z, target[j].z, weight);
        } else { // limb rotations
            table[j].x = LerpAngle(table[j].x, target[j].x, weight);
            table[j].y = LerpAngle(table[j].y, target[j].y, weight);
            table[j].z = LerpAngle(table[j].z, target[j].z, weight);
        }
    }
}

// OPEN_DISPS / CLOSE_DISPS declare the frame interpolation functions inside the function; in a nameless namespace those
// would be new functions that do not exist. These two do the same with the C functions (declared at the top) by name.
#define BOOST_OPEN_DISPS(gfxCtx)                    \
    {                                               \
        GraphicsContext* __gfxCtx = (gfxCtx);       \
        (void)__gfxCtx;                             \
        ::FrameInterpolation_RecordOpenChild(__FILE__, __LINE__)
#define BOOST_CLOSE_DISPS()                         \
    ::FrameInterpolation_RecordCloseChild();        \
    }

// ------------------------------------------------------------------------------------------------
// The boost look (v7): from the drop on the world is greyscale (the HUD and Shadow keep their colour), Shadow leaves
// blue see-through after images behind him, and a soft blue aura glows around him and pulses when a new after image
// appears. A flash and a ring go out at the drop.
//
//  * Greyscale: the renderer has a "grayscale" switch (the same one the medallion pause screen uses). It is switched on
//    at the start of the world's display lists and off again for the HUD. Shadow's own draw is wrapped (the player's
//    draw function is replaced while the look is on) so the switch is off while he is drawn.
//  * After images: a copy of Shadow's pose is stored every few frames (where he is, how he stands). They are drawn
//    after the whole world, with the see-through copy of his model from shadow_ghost.o2r, a flat blue with his shading,
//    and fade out where they are. They are drawn in the world's own space, so they stay put while Shadow runs on.
//  * Aura: a soft round gradient facing the camera, a little behind Shadow, so it only shows around him.
// ------------------------------------------------------------------------------------------------

bool FxEnabled() {
    return CVarGetInteger(kCvarEnabled, 1) != 0 && CVarGetInteger(kCvarFx, 1) != 0;
}

int ClampInt(int v, int lo, int hi) {
    return std::max(lo, std::min(hi, v));
}

bool ArchiveHasFile(const char* path) {
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
const FlexSkeletonHeader* GhostSkeleton(bool child) {
    int& state = sGhostSkelState[child ? 1 : 0];
    const char* path = child ? kGhostSkelChild : kGhostSkelAdult;
    if (state < 0) {
        state = ArchiveHasFile(path) ? 1 : 0;
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

u8 GrayAmount() {
    return static_cast<u8>(std::lround(std::max(0.0f, std::min(1.0f, sFxWeight)) * 255.0f));
}

// Shadow himself is drawn in colour: the greyscale switch is off while the player's draw function runs.
void ShadowPlayerDraw(Actor* thisx, PlayState* play) {
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    BOOST_OPEN_DISPS(gfxCtx);
    if (sGrayOn) {
        gSPGrayscale(POLY_OPA_DISP++, false);
        gSPGrayscale(POLY_XLU_DISP++, false);
    }
    if (sOrigPlayerDraw != nullptr) {
        sOrigPlayerDraw(thisx, play);
    }
    if (sGrayOn) {
        const u8 amount = GrayAmount();
        gSPGrayscale(POLY_OPA_DISP++, true);
        gDPSetGrayscaleColor(POLY_OPA_DISP++, 255, 255, 255, amount);
        gSPGrayscale(POLY_XLU_DISP++, true);
        gDPSetGrayscaleColor(POLY_XLU_DISP++, 255, 255, 255, amount);
    }
    BOOST_CLOSE_DISPS();
}

// Puts our draw function in the player's place while the look is on, and the game's own back afterwards.
void SetDrawWrapper(Player* player, bool wrap) {
    Actor* actor = &player->actor;
    if (wrap) {
        if (actor->draw != nullptr && actor->draw != ShadowPlayerDraw) {
            sOrigPlayerDraw = actor->draw;
            actor->draw = ShadowPlayerDraw;
        }
    } else if (actor->draw == ShadowPlayerDraw) {
        actor->draw = sOrigPlayerDraw;
    }
}

// The aura pulse: 1 at the moment a new after image is made, dying away fast (about 70 ms to a third). It is measured in
// real time, not in game frames, so every rendered frame (not only every fifth one at 60 fps) shows a different step of it.
void StartPulse() {
    sPulseAt = std::chrono::steady_clock::now();
    sPulseOn = true;
}

float PulseNow() {
    if (!sPulseOn) {
        return 0.0f;
    }
    const float ms = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - sPulseAt).count();
    return ms >= 400.0f ? 0.0f : std::exp(-std::max(0.0f, ms) / 65.0f);
}

void FxReset() {
    std::memset(sGhosts, 0, sizeof(sGhosts));
    sSinceSpawn = 0;
    sHaveLastGhostPos = false;
    sFxWeight = 0.0f;
    sPulseOn = false;
    sDropAge = -2;
    sGrayOn = false; // sGrayWasOn stays, so the next frame switches the greyscale off in the display lists
    // Deliberately does NOT touch the player actor. This runs from the scene init hook, which the game calls before it has
    // set up the actor lists of the new play state (on the very first scene, the title screen, they hold garbage), and from
    // the load and exit hooks, where the play state may already be gone. The draw wrapper needs no undoing here:
    // FxDrawBegin puts the game's own draw function back on the next frame (the weight is 0 now, so nothing is wanted),
    // and after a scene change the old player actor, and the wrapper in it, no longer exist.
}

// Loads the see-through skeleton ahead of time, so the first after image does not make the game hitch.
void FxPreload() {
    if (FxEnabled() && CVarGetInteger(kCvarFxAfter, 1) != 0) {
        (void)GhostSkeleton(IsChild());
    }
}

void FxOnDrop() {
    sDropAge = -1; // the game frame update that follows makes it 0, the first frame that is drawn
    sSinceSpawn = 1000;
    sHaveLastGhostPos = false;
    StartPulse();
}

// Once per game frame, after the player's update.
void FxTick() {
    const bool on = FxEnabled() && sState == State::Active;
    sFxWeight = Approach(sFxWeight, on ? 1.0f : 0.0f, 1.0f / static_cast<float>(kFxFadeFrames));
    if (sDropAge > -2 && ++sDropAge > kDropFlashFrames) {
        sDropAge = -2;
    }
    if (sSinceSpawn < 1000) {
        sSinceSpawn++;
    }
    if (!FxEnabled()) {
        std::memset(sGhosts, 0, sizeof(sGhosts));
    }
    const int life = ClampInt(CVarGetInteger(kCvarFxLife, 10), 3, 40);
    for (Ghost& g : sGhosts) {
        if (g.used && ++g.age >= life) {
            g.used = false;
        }
    }
    sFxTick++;
}

void OnPlayerUpdateBoost() {
    UpdateBoost();
    FxTick();
}

// Turns the greyscale on for the world's display lists (and off for the HUD's), and puts our draw function in place.
void FxDrawBegin() {
    sGrayOn = false;
    if (gPlayState == nullptr) {
        return;
    }
    Player* player = GET_PLAYER(gPlayState);
    const bool want = player != nullptr && FxEnabled() && CVarGetInteger(kCvarFxGrey, 1) != 0 && sFxWeight > 0.01f &&
                      gPlayState->pauseCtx.state == 0;
    if (want || sGrayWasOn) {
        GraphicsContext* gfxCtx = gPlayState->state.gfxCtx;
        BOOST_OPEN_DISPS(gfxCtx);
        if (want) {
            const u8 amount = GrayAmount();
            gSPGrayscale(POLY_OPA_DISP++, true);
            gDPSetGrayscaleColor(POLY_OPA_DISP++, 255, 255, 255, amount);
            gSPGrayscale(POLY_XLU_DISP++, true);
            gDPSetGrayscaleColor(POLY_XLU_DISP++, 255, 255, 255, amount);
        } else {
            gSPGrayscale(POLY_OPA_DISP++, false);
            gSPGrayscale(POLY_XLU_DISP++, false);
        }
        gSPGrayscale(OVERLAY_DISP++, false); // the HUD stays in colour
        BOOST_CLOSE_DISPS();
    }
    sGrayOn = want;
    sGrayWasOn = want;
    if (player != nullptr) {
        SetDrawWrapper(player, want);
    }
}

// Runs right before the game draws the frame, after the game has put Link's normal pose into his joint table.
void OnPlayDrawBeginBoost() {
    ApplyBoostPose();
    FxDrawBegin();
}

// Stores a new after image when it is time. Runs at the end of the world's draw, so the joint table is the one that was
// just drawn (every pose change of every file is in it).
void TrySpawnGhost(Player* player) {
    if (sFxSpawnTick == sFxTick) {
        return;
    }
    sFxSpawnTick = sFxTick;
    if (!FxEnabled() || CVarGetInteger(kCvarFxAfter, 1) == 0 || sState != State::Active ||
        (player->stateFlags2 & PLAYER_STATE2_DISABLE_DRAW) != 0) { // not while the homing attack hides Shadow
        return;
    }
    const int interval = ClampInt(CVarGetInteger(kCvarFxInterval, 2), 1, 20);
    if (sSinceSpawn < interval || player->skelAnime.jointTable == nullptr || player->skelAnime.limbCount < kJoints ||
        GhostSkeleton(IsChild()) == nullptr) {
        return;
    }
    const Vec3f& pos = player->actor.world.pos;
    if (sHaveLastGhostPos) {
        const float dx = pos.x - sLastGhostPos.x;
        const float dy = pos.y - sLastGhostPos.y;
        const float dz = pos.z - sLastGhostPos.z;
        if (dx * dx + dy * dy + dz * dz < 12.0f * 12.0f) {
            return; // standing still: no new after image
        }
    }
    Ghost* slot = nullptr;
    for (Ghost& g : sGhosts) {
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
    slot->child = IsChild();
    slot->id = sGhostNextId++;
    slot->pos = pos;
    slot->rot = player->actor.shape.rot;
    slot->scale = player->actor.scale;
    slot->yOffset = player->actor.shape.yOffset * player->actor.scale.y;
    slot->rootDrop = player->unk_6C4;
    slot->upperRot = player->upperLimbRot;
    slot->headRot = player->headLimbRot;
    slot->upperYawSecondary = player->upperLimbYawSecondary;
    std::memcpy(slot->joints, player->skelAnime.jointTable, sizeof(slot->joints));
    sSinceSpawn = 0;
    StartPulse(); // the aura pulses when an after image appears
    sLastGhostPos = pos;
    sHaveLastGhostPos = true;
}

// The same changes the game makes to Link's root, upper body and head while it draws him (Player_OverrideLimbDrawGameplayCommon).
s32 GhostOverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* arg) {
    (void)play;
    (void)dList;
    const Ghost* g = static_cast<const Ghost*>(arg);
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

void DrawGhost(PlayState* play, const Ghost& g, const FlexSkeletonHeader* skeleton, u8 alpha) {
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    // Each after image is its own piece for the frame interpolation, so it is only ever blended with itself.
    ::FrameInterpolation_RecordOpenChild(reinterpret_cast<const void*>(static_cast<uintptr_t>(0x5AD00000u + g.id)), 0);
    BOOST_OPEN_DISPS(gfxCtx);

    Matrix_Push();
    Vec3s rot = g.rot;
    Matrix_SetTranslateRotateYXZ(g.pos.x, g.pos.y + g.yOffset + 1.0f, g.pos.z, &rot);
    Matrix_Scale(g.scale.x, g.scale.y, g.scale.z, MTXMODE_APPLY);

    // Flat blue with the model's own shading, see-through, depth-tested but not written. (The model's meshes carry no
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
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 30, 60, 90, 255);
    gDPSetEnvColor(POLY_OPA_DISP++, 90, 160, 255, alpha);

    SkelAnime_DrawFlexOpa(play, static_cast<void**>(skeleton->sh.segment), const_cast<Vec3s*>(g.joints),
                          skeleton->dListCount, GhostOverrideLimbDraw, nullptr, const_cast<Ghost*>(&g));
    Matrix_Pop();

    BOOST_CLOSE_DISPS();
    ::FrameInterpolation_RecordCloseChild();
}

void DrawGhosts(PlayState* play, const Player* player) {
    if (!FxEnabled() || CVarGetInteger(kCvarFxAfter, 1) == 0) {
        return;
    }
    const float life = static_cast<float>(ClampInt(CVarGetInteger(kCvarFxLife, 10), 3, 40));
    const float opacity = static_cast<float>(ClampInt(CVarGetInteger(kCvarFxOpacity, 55), 0, 100)) / 100.0f;
    // Oldest first, so the newest is drawn on top.
    const Ghost* order[kMaxGhosts];
    int count = 0;
    for (const Ghost& g : sGhosts) {
        if (g.used && g.age >= 1) {
            order[count++] = &g;
        }
    }
    std::sort(order, order + count, [](const Ghost* a, const Ghost* b) { return a->age > b->age; });
    for (int i = 0; i < count; i++) {
        const Ghost& g = *order[i];
        const float dx = g.pos.x - player->actor.world.pos.x;
        const float dy = g.pos.y - player->actor.world.pos.y;
        const float dz = g.pos.z - player->actor.world.pos.z;
        if (dx * dx + dy * dy + dz * dz < 10.0f * 10.0f) {
            continue; // right inside Shadow: would only flicker against him
        }
        const float t = std::min(1.0f, static_cast<float>(g.age) / life);
        const float alpha = opacity * std::pow(1.0f - t, 1.4f);
        const int a8 = static_cast<int>(std::lround(alpha * 255.0f));
        const FlexSkeletonHeader* skeleton = GhostSkeleton(g.child);
        if (a8 < 4 || skeleton == nullptr) {
            continue;
        }
        DrawGhost(play, g, skeleton, static_cast<u8>(std::min(255, a8)));
    }
}

// One ring of the round gradient: its radius (in 1/8 world units) and colour.
struct BandRow {
    float radius;
    u8 r, g, b, a;
};

// A round band made of triangles with the colours of its rows blended between them (a soft edge). Facing +Z, centred on
// the origin. Needs the matrix and the see-through vertex-colour state to be set.
void DrawBand(GraphicsContext* gfxCtx, const BandRow* rows, int rowCount, int segments) {
    BOOST_OPEN_DISPS(gfxCtx);
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
    BOOST_CLOSE_DISPS();
}

u8 ToByte(float v) {
    return static_cast<u8>(std::lround(std::max(0.0f, std::min(1.0f, v)) * 255.0f));
}

// The soft aura behind Shadow, and the flash and ring at the drop.
void DrawAura(PlayState* play, const Player* player) {
    if (!FxEnabled() || CVarGetInteger(kCvarFxAura, 1) == 0) {
        return;
    }
    const float strength = static_cast<float>(ClampInt(CVarGetInteger(kCvarFxAuraStrength, 100), 0, 300)) / 100.0f;
    const float weight = std::min(1.0f, sFxWeight);
    const bool flash = sDropAge >= 0 && sDropAge <= kDropFlashFrames;
    if ((weight <= 0.02f && !flash) || strength <= 0.0f) {
        return;
    }
    const bool child = IsChild();
    const float flashT = flash ? 1.0f - static_cast<float>(sDropAge) / static_cast<float>(kDropFlashFrames) : 0.0f;
    // The pulse (AuraPulse, percent): the aura swells and brightens when a new after image appears, then settles.
    const float pulse = PulseNow() * static_cast<float>(ClampInt(CVarGetInteger(kCvarFxAuraPulse, 100), 0, 300)) / 100.0f;
    const float radius = (child ? 44.0f : 58.0f) * (1.0f + 0.22f * pulse + 0.9f * flashT);
    const float alpha = std::min(0.9f, ((0.30f + 0.50f * pulse) * weight + 0.55f * flashT) * strength);

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
    BOOST_OPEN_DISPS(gfxCtx);

    Gfx_SetupDL_4Xlu(gfxCtx);
    gDPSetCombineMode(POLY_XLU_DISP++, G_CC_SHADE, G_CC_SHADE);
    gSPClearGeometryMode(POLY_XLU_DISP++, G_LIGHTING | G_CULL_BOTH);

    Matrix_Translate(centre.x, centre.y, centre.z, MTXMODE_NEW);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    Matrix_Scale(1.0f / 8.0f, 1.0f / 8.0f, 1.0f / 8.0f, MTXMODE_APPLY); // the vertices are in 1/8 world units
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);

    // Bright blue in the middle, fading to nothing at the edge.
    const float r8 = radius * 8.0f;
    const BandRow aura[3] = {
        { 0.0f, 170, 225, 255, ToByte(alpha) },
        { r8 * 0.42f, 80, 160, 255, ToByte(alpha * 0.6f) },
        { r8, 40, 100, 255, 0 },
    };
    DrawBand(gfxCtx, aura, 3, 12);

    // The ring that goes out at the drop.
    if (flash) {
        const float progress = static_cast<float>(sDropAge) / static_cast<float>(kDropFlashFrames);
        const float ringRadius = (child ? 44.0f : 58.0f) * (0.5f + 2.2f * progress) * 8.0f;
        const float half = ringRadius * 0.2f + 24.0f;
        const float ringAlpha = 0.8f * (1.0f - progress) * std::min(1.5f, strength);
        const BandRow ring[3] = {
            { ringRadius - half, 190, 230, 255, 0 },
            { ringRadius, 190, 230, 255, ToByte(ringAlpha) },
            { ringRadius + half, 190, 230, 255, 0 },
        };
        DrawBand(gfxCtx, ring, 3, 24);
    }

    BOOST_CLOSE_DISPS();
    ::FrameInterpolation_RecordCloseChild();
}

// Runs at the end of the world's draw, before the HUD: the after images, then the aura.
void OnPlayDrawEndBoost() {
    if (gPlayState == nullptr) {
        return;
    }
    Player* player = GET_PLAYER(gPlayState);
    if (player == nullptr || gPlayState->pauseCtx.state != 0) {
        return;
    }
    TrySpawnGhost(player);
    bool anyGhost = false;
    for (const Ghost& g : sGhosts) {
        anyGhost = anyGhost || g.used;
    }
    const bool flash = sDropAge >= 0 && sDropAge <= kDropFlashFrames;
    if (!FxEnabled() || (!anyGhost && sFxWeight <= 0.02f && !flash)) {
        return;
    }
    GraphicsContext* gfxCtx = gPlayState->state.gfxCtx;
    {
        // What we draw now is in colour, not in the world's greyscale.
        BOOST_OPEN_DISPS(gfxCtx);
        gSPGrayscale(POLY_OPA_DISP++, false);
        gSPGrayscale(POLY_XLU_DISP++, false);
        BOOST_CLOSE_DISPS();
    }
    DrawGhosts(gPlayState, player);
    if ((player->stateFlags2 & PLAYER_STATE2_DISABLE_DRAW) == 0) { // no aura around a hidden Shadow (homing attack)
        DrawAura(gPlayState, player);
    }
}

void RegisterBoost() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayerUpdate>(OnPlayerUpdateBoost);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::ShouldActorUpdate>(ShouldActorUpdateBoost);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayDrawBegin>(OnPlayDrawBeginBoost);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayDrawEnd>(OnPlayDrawEndBoost);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneInit>([](int16_t) { ResetBoost(); });
    // The pause menu does not run player updates, so end the boost when it opens.
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnKaleidoscopeUpdate>([](int16_t) { ResetBoost(); });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnLoadGame>([](int32_t) {
        ResetBoost();
        sGhostSkelState[0] = sGhostSkelState[1] = -1; // look for shadow_ghost.o2r again
        LoadAssets();
    });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnExitGame>([](int32_t) { ResetBoost(); });

    // The airborne code limits Link's speed to the normal running speed; not while boosting.
    REGISTER_VB_SHOULD(VB_PLAYER_LIMIT_JUMP_SPEED, {
        (void)va_arg(args, Player*);
        OnLimitJumpSpeed(should);
    });
}

static RegisterShipInitFunc sBoostInit(RegisterBoost);

} // namespace

// Used by Jump.cpp: true only while the charge-up pose plays (Link is held still), so Y cannot break out of the pose.
extern "C" bool ShadowBoost_IsBusy(void) {
    return sState == State::Windup;
}

// Used by ChaosControl.cpp: true when fn is this file's draw wrapper, so the two files never wrap each other's wrapper.
extern "C" bool ShadowBoost_IsPlayerDrawWrapper(ActorFunc fn) {
    return fn == ShadowPlayerDraw;
}

// Read by the status report in Jump.cpp: tells it that this file is in the build, and which version it is.
// 3 = Chaos Control style charge-up with the tune, burst at the drop, boost survives jumps.
// 7 = greyscale world, blue after images (shadow_ghost.o2r) and aura while boosting.
// 8 = works with the boomerang (and other items) in hand.
// 9 = the "Boost blocked" message names the reason.
// 10 = boot crash fix (the reset does not touch the player actor).
// 11 = aura pulse is visible (real-time pulse on every new after image).
// 12 = works with the homing attack: the boost leaves Shadow's speed alone while it runs, enemies are not slowed down
//      during it (so the hit does damage), no aura or new after image while Shadow is hidden by it.
extern "C" {
int ShadowAbilityVersion_Boost = 12;
}
