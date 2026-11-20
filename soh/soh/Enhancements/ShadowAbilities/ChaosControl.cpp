// Shadow the Hedgehog - Chaos Control for Ship of Harkinian 9.3.1
//
// Press the Chaos Control button (default: the N64 "L" button).
//
//  1. The music starts from its beginning.
//  2. Shadow says "Chaos Control!" so that the line ends right at the beat drop.
//  3. On the beat drop time stops for everything dangerous (enemies, bosses, bombs, deku nuts) for a few
//     seconds while Link keeps moving.
//  4. When time resumes the music fades out and an ending sound plays. Then there is a cooldown.
//
// The music, voice line, ending sound and the beat drop time come from a separate audio mod
// (chaos_control_audio.o2r in the mods folder, files under custom/chaos_control/). They are played
// through their own SDL audio device, so they do not depend on the game's sound engine.
// If the audio mod is missing, Chaos Control still works: it starts instantly with a game sound.
//
// While time is stopped, frozen actors are skipped by the game's actor update, so they also stop
// registering their hit boxes: Link walks through them and cannot damage them, and they cannot damage
// Link. That is how the game treats every actor it pauses (talking, ocarina, etc.).
//
// Everything is in this one file. CMake picks it up automatically (soh/*.cpp is globbed).
//
// Settings (CVars, change with the in-game console: set <name> <value>):
//   gShadow.ChaosControl.Enabled   1 = on (default), 0 = off
//   gShadow.ChaosControl.Button    controller button number, default 32 (L). 8192 = Z, 4096 = Start
//   gShadow.ChaosControl.Duration  seconds time stays stopped after the beat drop, default 6
//   gShadow.ChaosControl.Cooldown  seconds before it can be used again, default 10
//   gShadow.ChaosControl.Sound     1 = music and sounds (default), 0 = silent, starts instantly
//   gShadow.ChaosControl.Volume    0-100, volume of the music, voice and ending sound, default 80
//   gShadow.ChaosControl.DropOffset  milliseconds added to the beat drop time, default 0. Use it to make the freeze
//                                  start earlier (negative) or later (positive), e.g. set ...DropOffset -300

#include <libultraship/bridge/consolevariablebridge.h>

#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"

#include <SDL2/SDL.h>
#include <ship/Context.h>
#include <ship/resource/File.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/ArchiveManager.h>

#include <algorithm>
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
extern PlayState* gPlayState;
}

namespace {

constexpr const char* kCvarEnabled = "gShadow.ChaosControl.Enabled";
constexpr const char* kCvarButton = "gShadow.ChaosControl.Button";
constexpr const char* kCvarDuration = "gShadow.ChaosControl.Duration";
constexpr const char* kCvarCooldown = "gShadow.ChaosControl.Cooldown";
constexpr const char* kCvarSound = "gShadow.ChaosControl.Sound";
constexpr const char* kCvarVolume = "gShadow.ChaosControl.Volume";
constexpr const char* kCvarDropOffset = "gShadow.ChaosControl.DropOffset";

// Files inside the audio mod (chaos_control_audio.o2r).
constexpr const char* kFileMusic = "custom/chaos_control/music.wav";
constexpr const char* kFileVoice = "custom/chaos_control/voice.wav";
constexpr const char* kFileEnd = "custom/chaos_control/end.wav";
constexpr const char* kFileConfig = "custom/chaos_control/config.txt";

// The game logic runs at 20 frames per second.
constexpr int kFramesPerSecond = 20;
// If the music never reaches the beat drop (bad settings), start the freeze after this many frames anyway.
constexpr int kMaxWindupFrames = 60 * kFramesPerSecond;

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
int sDropMs = 0;        // time from the button press to the beat drop (freeze starts)
int sVoiceStartMs = -1; // when the voice line starts; -1 = computed so that it ends at the beat drop

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

void PlayTrack(TrackId id) {
    if (sDevice == 0 || !sClips[id].ok) {
        return;
    }
    const float level = kTrackLevel[id] * MasterVolume();
    SDL_LockAudioDevice(sDevice);
    Track& tr = sTracks[id];
    tr.clip = &sClips[id];
    tr.pos = 0;
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
        } else if (key == "voice_start_ms") {
            sVoiceStartMs = value;
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
}

// ------------------------------------------------------------------------------------------------
// Chaos Control itself
// ------------------------------------------------------------------------------------------------

enum class State { Idle, Windup, Active };

State sState = State::Idle;
int sActiveFrames = 0;
int sCooldownFrames = 0;
int sWindupFrames = 0;
bool sVoicePlayed = false;

bool SoundOn() {
    return CVarGetInteger(kCvarSound, 1) != 0;
}

void PlayGameCue(u16 sfxId) {
    if (SoundOn()) {
        Sfx_PlaySfxCentered(sfxId);
    }
}

void EnterActive() {
    int seconds = CVarGetInteger(kCvarDuration, 6);
    if (seconds < 1) {
        seconds = 1;
    }
    sState = State::Active;
    sActiveFrames = seconds * kFramesPerSecond;
}

void BeginChaosControl() {
    sVoicePlayed = false;
    sWindupFrames = 0;
    if (SoundOn()) {
        LoadAssets();
        if (sClips[kTrackMusic].ok && EnsureDevice()) {
            sState = State::Windup;
            PlayTrack(kTrackMusic);
            return;
        }
        // No audio mod: start right away with a game sound.
        PlayGameCue(NA_SE_PL_MAGIC_WIND_WARP);
    }
    EnterActive();
}

void TickWindup() {
    const int ms = TrackMs(kTrackMusic);
    if (!sVoicePlayed && sClips[kTrackVoice].ok && ms >= sVoiceStartMs) {
        PlayTrack(kTrackVoice);
        sVoicePlayed = true;
    }
    sWindupFrames++;
    const int dropMs = sDropMs + CVarGetInteger(kCvarDropOffset, 0);
    if (ms >= dropMs || !TrackPlaying(kTrackMusic) || sWindupFrames > kMaxWindupFrames) {
        EnterActive();
    }
}

// normalEnd: the time ran out (music fades, ending sound). Otherwise something interrupted it.
void StopChaosControl(bool normalEnd) {
    if (sState == State::Idle) {
        return;
    }
    const bool wasActive = (sState == State::Active);
    sState = State::Idle;
    sActiveFrames = 0;
    sWindupFrames = 0;

    FadeTrack(kTrackVoice, 0.2f);
    FadeTrack(kTrackMusic, normalEnd ? 1.2f : 0.3f);

    if (wasActive) {
        int seconds = CVarGetInteger(kCvarCooldown, 10);
        if (seconds < 0) {
            seconds = 0;
        }
        sCooldownFrames = seconds * kFramesPerSecond;
        if (normalEnd && SoundOn()) {
            if (sClips[kTrackEnd].ok && sDevice != 0) {
                PlayTrack(kTrackEnd);
            } else {
                PlayGameCue(NA_SE_PL_MAGIC_WIND_VANISH);
            }
        }
    }
}

void ResetChaosControl() {
    if (sState != State::Idle) {
        StopChaosControl(false);
    }
    sCooldownFrames = 0;
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

void OnPlayerUpdateChaosControl() {
    if (gPlayState == nullptr || !CVarGetInteger(kCvarEnabled, 1)) {
        ResetChaosControl();
        return;
    }

    Player* player = GET_PLAYER(gPlayState);
    if (player == nullptr) {
        return;
    }

    switch (sState) {
        case State::Active:
            if (PlayerBusy(player)) {
                StopChaosControl(false);
            } else if (--sActiveFrames <= 0) {
                StopChaosControl(true);
            }
            return;
        case State::Windup:
            if (PlayerBusy(player)) {
                StopChaosControl(false);
            } else {
                TickWindup();
            }
            return;
        case State::Idle:
            break;
    }

    if (sCooldownFrames > 0) {
        sCooldownFrames--;
        return;
    }

    const int button = CVarGetInteger(kCvarButton, BTN_L);
    if (CHECK_BTN_ANY(gPlayState->state.input[0].press.button, button) && !PlayerBusy(player)) {
        BeginChaosControl();
    }
}

// Which actors does time stop for?
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

void ShouldActorUpdateChaosControl(void* refActor, bool* result) {
    // Time only stops after the beat drop (State::Active), not during the build-up.
    if (sState != State::Active || gPlayState == nullptr || refActor == nullptr) {
        return;
    }
    Player* player = GET_PLAYER(gPlayState);
    if (player == nullptr) {
        return;
    }
    if (IsFrozenByChaosControl(static_cast<Actor*>(refActor), player)) {
        *result = false;
    }
}

void RegisterChaosControl() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayerUpdate>(OnPlayerUpdateChaosControl);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::ShouldActorUpdate>(ShouldActorUpdateChaosControl);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneInit>(
        [](int16_t) { ResetChaosControl(); });
    // The pause menu does not run player updates, so end Chaos Control (and its music) when it opens.
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnKaleidoscopeUpdate>(
        [](int16_t) { StopChaosControl(false); });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnLoadGame>([](int32_t) {
        ResetChaosControl();
        if (SoundOn()) {
            LoadAssets();
        }
    });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnExitGame>(
        [](int32_t) { ResetChaosControl(); });
}

static RegisterShipInitFunc sChaosControlInit(RegisterChaosControl);

} // namespace
