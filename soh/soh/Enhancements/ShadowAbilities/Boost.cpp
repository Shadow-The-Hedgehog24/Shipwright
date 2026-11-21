// Shadow the Hedgehog - Boost (the X button) for Ship of Harkinian 9.3.1
//
// Version 3. Press X on the ground and the boost starts the way Chaos Control does: Shadow stops and plays the Chaos
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
//   gShadow.Boost.Speed          boost speed, default 13 (Link's normal run is about 6.6; a sword lunge is 15)
//   gShadow.Boost.Accel          how fast the speed builds up per game frame, default 3
//   gShadow.Boost.Windup         1 = the Chaos Control style charge-up with the tune (default), 0 = no charge-up, boost at once
//   gShadow.Boost.Animation      1 = use the poses (default), 0 = only the speed
//   gShadow.Boost.RunRate        playback speed of the skating run in percent, default 100
//   gShadow.Boost.Sound          1 = play the tune (default), 0 = silent
//   gShadow.Boost.Volume         tune volume 0-100, default 80
//   gShadow.Boost.DropOffset     move the moment the boost starts, in milliseconds (added to the drop time of the tune)
//   gShadow.Boost.SyncOffset     move the tune against the pose, in milliseconds (negative = tune earlier)
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
#include <cmath>
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

constexpr const char* kFileRunAdult = "custom/boost/anim_run_adult.bin";
constexpr const char* kFileRunChild = "custom/boost/anim_run_child.bin";
constexpr const char* kFilePoseAdult = "custom/boost/pose_adult.bin";            // the Chaos Control pose
constexpr const char* kFilePoseChild = "custom/boost/pose_child.bin";
constexpr const char* kFilePoseAdultCC = "custom/chaos_control/anim_adult.bin";  // fallback: the one in chaos_control_audio.o2r
constexpr const char* kFilePoseChildCC = "custom/chaos_control/anim_child.bin";
constexpr const char* kFileMusic = "custom/boost/music.wav";
constexpr const char* kFileSongConfig = "custom/boost/song_config.txt";

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

// Limbs the game drives itself (hat, collar, sheath, torso): the pose leaves them alone.
inline bool KeepGameLimb(int limb) {
    return limb == 11 || limb == 12 || limb == 19 || limb == 20;
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
                         PLAYER_STATE1_CARRYING_ACTOR | PLAYER_STATE1_USING_BOOMERANG |
                         PLAYER_STATE1_HOOKSHOT_FALLING | PLAYER_STATE1_LOADING | PLAYER_STATE1_FLOOR_DISABLED |
                         PLAYER_STATE1_READY_TO_FIRE | PLAYER_STATE1_SHIELDING |
                         PLAYER_STATE1_START_CHANGING_HELD_ITEM;
    const u32 blocked2 = PLAYER_STATE2_UNDERWATER | PLAYER_STATE2_DIVING | PLAYER_STATE2_CRAWLING |
                         PLAYER_STATE2_GRABBED_BY_ENEMY | PLAYER_STATE2_FROZEN | PLAYER_STATE2_OCARINA_PLAYING |
                         PLAYER_STATE2_STATIONARY_LADDER;
    if (player->stateFlags1 & blocked1) {
        return "Link state (flags1)";
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
    const float speed = std::max(0.0f, CVarGetFloat(kCvarSpeed, 13.0f));
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
void ShouldActorUpdateBoost(void* refActor, bool*) {
    if (gPlayState == nullptr || refActor == nullptr || !CVarGetInteger(kCvarEnabled, 1)) {
        return;
    }
    Player* player = GET_PLAYER(gPlayState);
    if (player == nullptr || static_cast<Actor*>(refActor) != &player->actor) {
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

void OnPlayerUpdateBoost() {
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
                why = "Link is busy (Z-targeting, rolling, swinging...)";
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

    // The speed. This runs after Link's own update; his movement next frame uses linearVelocity.
    if (inJump) {
        // In the air: keep the speed Link had (the game would slow him down), unless the stick says "turn around".
        if (jumping && !reversing) {
            player->linearVelocity = sBoostSpeed;
        }
        sStuckCount = 0;
        sNeutralCount = 0;
    } else if (stickHeld && !reversing) {
        sBoostSpeed = Approach(sBoostSpeed, TargetSpeed(), std::max(0.1f, CVarGetFloat(kCvarAccel, 3.0f)));
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

// Runs right before the game draws the frame, after the game has put Link's normal pose into his joint table.
void OnPlayDrawBeginBoost() {
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

void RegisterBoost() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayerUpdate>(OnPlayerUpdateBoost);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::ShouldActorUpdate>(ShouldActorUpdateBoost);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayDrawBegin>(OnPlayDrawBeginBoost);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneInit>([](int16_t) { ResetBoost(); });
    // The pause menu does not run player updates, so end the boost when it opens.
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnKaleidoscopeUpdate>([](int16_t) { ResetBoost(); });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnLoadGame>([](int32_t) {
        ResetBoost();
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

// Read by the status report in Jump.cpp: tells it that this file is in the build, and which version it is.
// 3 = Chaos Control style charge-up with the tune, burst at the drop, boost survives jumps.
extern "C" {
int ShadowAbilityVersion_Boost = 3;
}
