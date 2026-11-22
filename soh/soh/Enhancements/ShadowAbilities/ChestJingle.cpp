// Shadow the Hedgehog - custom treasure chest opening jingle for Ship of Harkinian 9.3.1
//
// When a big chest is opened the game plays the "Open Treasure Chest" fanfare (NA_BGM_OPEN_TRE_BOX). This mutes that
// fanfare and plays your own tune instead, timed so that the tune's "drop" lands at the moment Link lifts the item
// out of the chest (the moment the item appears above his head).
//
// How the timing works: pressing A on a big chest starts Link's chest-opening animation (133 frames = 6.65 s), then he
// lifts the item (the item shows up on frame 21 of the lift animation, 1.05 s later). The tune starts later in that
// animation, exactly early enough that its drop (drop_ms from config.txt) arrives when the item appears. The game
// normally starts the "You got an item" fanfare right after the lift; that one is muted too while the tune plays.
// Only those fanfares are muted, and the fanfare channel's volume is put back to normal as soon as they are over, so
// every other jingle (ocarina songs, item fanfares outside a chest, ...) plays as usual.
//
// The sound comes from the audio mod chaos_control_audio.o2r (or any .o2r in the mods folder):
//   custom/chest_jingle/jingle.wav   the tune
//   custom/chest_jingle/config.txt   drop_ms=<milliseconds into the tune where the drop is>
// If jingle.wav is missing nothing changes and the normal fanfare plays.
//
// Settings (CVars, change with the in-game console: set <name> <value>):
//   gShadow.ChestJingle.Enabled          1 = on (default), 0 = off (normal fanfare)
//   gShadow.ChestJingle.Volume           0-100, default 80
//   gShadow.ChestJingle.Offset           milliseconds added to the drop time. Negative = drop earlier, positive = later
//   gShadow.ChestJingle.KeepItemFanfare  0 = also mute the "You got an item" fanfare while the tune plays (default),
//                                        1 = let it play
//   gShadow.ChestJingle.Debug            1 = small on-screen messages: when the game loads (is the tune found?), when
//                                        a chest fanfare is seen, when the tune starts. Default 1 while testing, 0 = silent.

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
#include "seqcmd.h"
#include "sequence.h"
extern PlayState* gPlayState;
}

namespace {

constexpr const char* kCvarEnabled = "gShadow.ChestJingle.Enabled";
constexpr const char* kCvarVolume = "gShadow.ChestJingle.Volume";
constexpr const char* kCvarOffset = "gShadow.ChestJingle.Offset";
constexpr const char* kCvarKeepItemFanfare = "gShadow.ChestJingle.KeepItemFanfare";
constexpr const char* kCvarDebug = "gShadow.ChestJingle.Debug";
constexpr const char* kFileJingle = "custom/chest_jingle/jingle.wav";
constexpr const char* kFileConfig = "custom/chest_jingle/config.txt";

constexpr int kRate = 44100;
constexpr int kChannels = 2;

constexpr float kMsPerFrame = 50.0f;          // the game logic runs at 20 frames per second
constexpr float kAnimFramesPerTick = 1.5f;    // an animation with play speed s advances s * 1.5 frames per game frame
constexpr float kLiftItemFrame = 21.0f;       // the item shows up on this frame of Link's lift animation
constexpr float kLongOpenAnimFrames = 100.0f; // the slow chest animation is longer than this, the "kick" is not
constexpr int kGiveUpFrames = 40;             // fanfare heard but no chest animation seen for this long: just play

std::vector<int16_t> sPcm; // interleaved stereo, 44.1 kHz
std::atomic<bool> sReady{ false };
bool sLoadTried = false;
int sDropMs = 0; // where the drop is inside the tune; 0 = no sync, play at once

SDL_AudioDeviceID sDevice = 0;
size_t sPos = 0;                    // protected by the device lock
float sGain = 1.0f;                 // protected by the device lock
std::atomic<bool> sPlaying{ false };

// set on the audio thread when the chest fanfare starts, read and cleared on the game thread
std::atomic<bool> sArmed{ false };
std::atomic<bool> sSawFanfare{ false }; // the chest fanfare started (whether or not the tune is loaded)
// We set the fanfare channel's volume to 0 and have not put it back yet. The game remembers that volume and starts the
// next fanfare with it (Audio_StartSequence re-applies gActiveSeqs[].volCur), so it has to be put back, or every later
// jingle (the ocarina songs, for one) would be silent.
std::atomic<bool> sFanfareMuted{ false };
int sArmedFrames = 0; // game thread only

// A small on-screen message (bottom right) while the jingle is being tested. Game thread only.
void Say(const std::string& text) {
    if (CVarGetInteger(kCvarDebug, 1) != 0) {
        Notification::Emit({ .message = text, .remainingTime = 5.0f, .mute = true });
    }
}

void SDLCALL MixCallback(void*, Uint8* stream, int len) {
    int16_t* out = reinterpret_cast<int16_t*>(stream);
    const int count = len / static_cast<int>(sizeof(int16_t));
    bool playing = sPlaying.load();
    for (int i = 0; i < count; i++) {
        float v = 0.0f;
        if (playing) {
            if (sPos >= sPcm.size()) {
                playing = false;
            } else {
                v = static_cast<float>(sPcm[sPos++]) * sGain;
            }
        }
        v = std::max(-32768.0f, std::min(32767.0f, v));
        out[i] = static_cast<int16_t>(v);
    }
    sPlaying.store(playing);
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

// Reads a .wav of any common format and converts it to 44.1 kHz stereo 16 bit.
bool DecodeWav(const std::vector<char>& bytes, std::vector<int16_t>& pcm) {
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
        pcm.assign(reinterpret_cast<int16_t*>(data), reinterpret_cast<int16_t*>(data) + length / 2);
        ok = true;
    } else if (needed > 0) {
        cvt.len = static_cast<int>(length);
        cvt.buf = static_cast<Uint8*>(SDL_malloc(static_cast<size_t>(cvt.len) * static_cast<size_t>(cvt.len_mult)));
        if (cvt.buf != nullptr) {
            std::memcpy(cvt.buf, data, length);
            if (SDL_ConvertAudio(&cvt) == 0) {
                pcm.assign(reinterpret_cast<int16_t*>(cvt.buf), reinterpret_cast<int16_t*>(cvt.buf) + cvt.len_cvt / 2);
                ok = true;
            }
            SDL_free(cvt.buf);
        }
    }
    SDL_FreeWAV(data);
    return ok && !pcm.empty();
}

void ParseConfig(const std::string& text) {
    std::istringstream lines(text);
    std::string line;
    while (std::getline(lines, line)) {
        const size_t eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        if (line.substr(0, eq) == "drop_ms") {
            sDropMs = std::max(0, std::atoi(line.c_str() + eq + 1));
        }
    }
}

// Runs on the game thread, never on the audio thread.
void LoadJingle() {
    if (sLoadTried) {
        return;
    }
    Ship::Context* ctx = Ship::Context::GetRawInstance();
    if (ctx == nullptr || ctx->GetResourceManager() == nullptr) {
        return;
    }
    sLoadTried = true;
    auto archiveManager = ctx->GetResourceManager()->GetArchiveManager();
    if (archiveManager == nullptr || !archiveManager->HasFile(kFileJingle)) {
        return;
    }
    auto file = archiveManager->LoadFile(kFileJingle);
    if (file == nullptr || file->Buffer == nullptr || file->Buffer->empty()) {
        return;
    }
    if (archiveManager->HasFile(kFileConfig)) {
        auto config = archiveManager->LoadFile(kFileConfig);
        if (config != nullptr && config->Buffer != nullptr) {
            ParseConfig(std::string(config->Buffer->begin(), config->Buffer->end()));
        }
    }
    std::vector<int16_t> pcm;
    if (DecodeWav(*file->Buffer, pcm) && EnsureDevice()) {
        sPcm = std::move(pcm);
        sReady = true;
    }
}

// Starts the tune. startMs = how far into the tune to begin (used when we are a little late).
void PlayJingle(int startMs) {
    int v = CVarGetInteger(kCvarVolume, 80);
    v = std::max(0, std::min(100, v));
    size_t pos = static_cast<size_t>(std::max(0, startMs)) * kRate / 1000 * kChannels;
    SDL_LockAudioDevice(sDevice);
    sGain = static_cast<float>(v) / 100.0f;
    sPos = std::min(pos, sPcm.size());
    sPlaying.store(sPos < sPcm.size());
    SDL_UnlockAudioDevice(sDevice);
}

void StopJingle() {
    if (sDevice == 0) {
        return;
    }
    SDL_LockAudioDevice(sDevice);
    sPlaying.store(false);
    SDL_UnlockAudioDevice(sDevice);
}

// The fanfares the game plays when Link holds up what came out of a chest.
bool IsItemFanfare(int32_t seqId) {
    const int32_t id = seqId & 0xFF;
    return id == NA_BGM_ITEM_GET || id == NA_BGM_SMALL_ITEM_GET || id == NA_BGM_HEART_GET;
}

void MuteFanfare() {
    SEQCMD_SET_SEQPLAYER_VOLUME(SEQ_PLAYER_FANFARE, 0, 0);
    sFanfareMuted.store(true);
}

// Normal volume again (0x7F = 1.0).
void RestoreFanfareVolume() {
    if (sFanfareMuted.exchange(false)) {
        SEQCMD_SET_SEQPLAYER_VOLUME(SEQ_PLAYER_FANFARE, 0, 0x7F);
    }
}

// Called by the game whenever a sequence player starts a sequence. This runs on the audio thread, so it only does
// small things: remember that the chest fanfare started, and queue the command that mutes the fanfare player.
// The muted fanfare keeps running silently, so the game's own timing (music ducking and restoring) is not disturbed.
// The volume is put back when the fanfare channel goes quiet again (OnPlayerUpdateChestJingle), or right here when a
// fanfare that is not ours starts while the channel is still muted.
void OnSeqPlayerInitChestJingle(int32_t playerIdx, int32_t seqId) {
    if (playerIdx != SEQ_PLAYER_FANFARE) {
        return;
    }
    const bool chestFanfare = (seqId & 0xFF) == NA_BGM_OPEN_TRE_BOX;
    if (chestFanfare) {
        sSawFanfare.store(true);
    }
    if (sReady && CVarGetInteger(kCvarEnabled, 1)) {
        if (chestFanfare) {
            sArmed.store(true);
            MuteFanfare();
            return;
        }
        if (IsItemFanfare(seqId) && (sPlaying.load() || sArmed.load()) && !CVarGetInteger(kCvarKeepItemFanfare, 0)) {
            // the "You got an item" fanfare that starts right after Link lifts the item
            MuteFanfare();
            return;
        }
    }
    // Some other fanfare (an ocarina song, an item outside a chest, ...): it must be heard.
    RestoreFanfareVolume();
}

// Game thread, once per game frame.
void OnPlayerUpdateChestJingle() {
    // The muted fanfares are over: give the fanfare channel its volume back for the next jingle.
    if (sFanfareMuted.load() && func_800FA0B4(SEQ_PLAYER_FANFARE) == NA_BGM_DISABLED) {
        RestoreFanfareVolume();
        Say("Fanfare volume restored");
    }
    if (sSawFanfare.exchange(false)) {
        Say(sReady ? "Chest fanfare seen, jingle armed" : "Chest fanfare seen, but the jingle tune is NOT loaded");
    }
    if (!sReady || gPlayState == nullptr || !sArmed.load()) {
        sArmedFrames = 0;
        return;
    }
    sArmedFrames++;

    if (sDropMs <= 0) {
        sArmed.store(false);
        PlayJingle(0);
        Say("Chest jingle playing");
        return;
    }

    Player* player = GET_PLAYER(gPlayState);
    const SkelAnime& anim = player->skelAnime;
    const bool opening = (player->stateFlags1 & PLAYER_STATE1_GETTING_ITEM) && anim.endFrame > kLongOpenAnimFrames;

    if (!opening) {
        // Not seeing the slow chest animation (some other mod or setting changed it): do not leave it silent.
        if (sArmedFrames > kGiveUpFrames) {
            sArmed.store(false);
            PlayJingle(0);
            Say("Chest jingle playing (no chest animation seen)");
        }
        return;
    }

    // Game frames until the animation ends, plus the frames Link needs to raise the item.
    const float perTick = std::max(0.1f, anim.playSpeed * kAnimFramesPerTick);
    const float framesLeft = std::max(0.0f, anim.endFrame - anim.curFrame) / perTick + kLiftItemFrame / perTick;
    const float msUntilItem = framesLeft * kMsPerFrame;

    // We want the drop (dropMs into the tune, moved by the Offset setting) to land when the item appears.
    const float dropMs = static_cast<float>(sDropMs + CVarGetInteger(kCvarOffset, 0));
    if (msUntilItem <= dropMs) {
        sArmed.store(false);
        PlayJingle(static_cast<int>(dropMs - msUntilItem)); // 0 when on time, a little more if we are late
        Say("Chest jingle playing");
    }
}

void RegisterChestJingle() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSeqPlayerInit>(OnSeqPlayerInitChestJingle);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayerUpdate>(OnPlayerUpdateChestJingle);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnLoadGame>([](int32_t) {
        LoadJingle();
    });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneInit>([](int16_t) {
        LoadJingle();
        sArmed.store(false);
        RestoreFanfareVolume();
    });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnExitGame>([](int32_t) {
        sArmed.store(false);
        StopJingle();
        RestoreFanfareVolume();
    });
}

static RegisterShipInitFunc sChestJingleInit(RegisterChestJingle);

} // namespace

// Read by the status report in Jump.cpp: tells it that this file is in the build, and which version it is.
extern "C" {
int ShadowAbilityVersion_ChestJingle = 3;
}
