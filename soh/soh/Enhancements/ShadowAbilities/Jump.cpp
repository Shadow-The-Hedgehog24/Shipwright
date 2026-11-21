// Shadow the Hedgehog - Jump, Double Jump and carrying things for Ship of Harkinian 9.3.1
//
// Ocarina of Time has no jump button. This adds one: the Y button.
//   * Y while standing, walking or running on the ground: Shadow jumps, in his SA2 jump pose (animation 116, a tucked jump).
//   * Y again while in the air: a second jump, in his SA2 flip (animation 114). One extra jump per time in the air;
//     touching the ground gives it back. Y works during Chaos Control once time has stopped.
//   Walking off a ledge or jumping with the A button still uses the game's own jump and fall animations.
//
// Carrying (this file also fixes two things that show up when Shadow carries something, Princess Ruto for instance):
//   * Ruto sat inside Shadow's head. The game puts a carried actor halfway between Link's two hands, and Shadow's hands
//     are at head height. Ruto is now moved up and forward (a few game units, see the settings below).
//   * Jumping or falling while carrying: the game plays the carry animation on the upper body, so the legs did the jump
//     and the chest and head stayed upright. The chest and head now follow the jump animation; the arms keep holding.
//   The two poses are put onto Link's skeleton while the game draws him (like the boost and Chaos Control poses); they
//   are inside chaos_control_audio.o2r (custom/jump/*). Without them the jumps still work, with the game's jump animation.
// The press is read straight from the game controller (SDL), so it works whatever the N64 buttons are mapped to.
// If Y already does something in your controller settings, unmap it there.
//
// Everything is in this one file. CMake picks it up automatically (soh/*.cpp is globbed).
//
// Settings (CVars, change with the in-game console: set <name> <value>):
//   gShadow.Jump.Enabled         1 = on (default), 0 = off
//   gShadow.Jump.GroundJump      1 = Y jumps from the ground too (default), 0 = double jump only
//   gShadow.Jump.SdlButton       controller button, default 3 (Y). 0 = A, 1 = B, 2 = X, 9 = left shoulder, 10 = right shoulder
//   gShadow.Jump.N64Button       also accept this N64 button number from the game's own controller mapping,
//                                default 0 = none. Use it if the Y button does not reach the game directly:
//                                map Y to "Modifier 1" in the controller settings and set this to 64
//   gShadow.Jump.Count           extra jumps in the air per time in the air, default 1
//   gShadow.Jump.Strength        ground jump strength in percent, default 100 (Roc's Feather's jump: 7.5 adult, 7.0 child)
//   gShadow.Jump.DoubleStrength  air jump strength in percent, default 100
//   gShadow.Jump.Boost           1 = a little forward push on the air jump (default), 0 = straight up
//   gShadow.Jump.Boomerang       1 = Y jumps and double jumps work while the boomerang is in hand: aiming it, with it
//                                thrown and flying, and catching it (default), 0 = no jumping then (the way it was up to v5)
//   gShadow.Jump.Animation       1 = use the jump and flip poses (default), 0 = the game's own jump animation
//   gShadow.Jump.FirstRate       playback speed of the first jump pose in percent, default 0 = automatic (the pose
//                                is sped up so that it lasts as long as the jump)
//   gShadow.Jump.SecondRate      the same for the flip, default 0 = automatic
//   gShadow.Status               1 = after a save has loaded, show which Shadow files are in the build (default), 0 = off
//   gShadow.Carry.Enabled        1 = the carrying fixes are on (default), 0 = off
//   gShadow.Carry.Up             how far Ruto is moved up, in game units (default 10; Link is about 48 units tall as a child)
//   gShadow.Carry.Forward        how far Ruto is moved forward, in game units (default 4)
//   gShadow.Carry.Side           how far Ruto is moved to Shadow's right, in game units (default 0)
//   gShadow.Carry.Actors         0 = only Ruto is moved (default), 1 = everything that is carried
//   gShadow.Carry.JumpFollow     1 = chest and head follow the jump while carrying (default), 0 = the game's way
//   gShadow.Carry.Anchor         1 = a carried Ruto is kept at Shadow's hands during the updates too (default), 0 = the game's way
//   The three offsets can also be set without rebuilding: a mod file with custom/carry/config.txt (up=, forward=, side=,
//   one per line) overrides the defaults. A setting changed in the console beats the file.

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
#include <cstring>
#include <memory>
#include <string>
#include <vector>

extern "C" {
#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
#include "objects/gameplay_keep/gameplay_keep.h"
extern PlayState* gPlayState;

// The game's own "start a jump" (used when Link runs off a ledge). It picks the run or standing jump animation.
s32 func_8083A4A8(Player* player, PlayState* play);

// The action Link is in for the whole time he is in the air (jumping or falling).
void Player_Action_8084411C(Player* player, PlayState* play);

// The actions Link is in while standing, walking, running or Z-targeting on the ground.
void Player_Action_Idle(Player* player, PlayState* play);
void Player_Action_80842180(Player* player, PlayState* play); // running
void Player_Action_8084227C(Player* player, PlayState* play); // running while Z-targeting
void Player_Action_8084193C(Player* player, PlayState* play); // side-stepping while Z-targeting
void Player_Action_80840450(Player* player, PlayState* play);
void Player_Action_808407CC(Player* player, PlayState* play);
void Player_Action_80840DE4(Player* player, PlayState* play);
void Player_Action_808414F8(Player* player, PlayState* play);
void Player_Action_8084170C(Player* player, PlayState* play);
void Player_Action_808417FC(Player* player, PlayState* play);
}

// Provided by ChaosControl.cpp. True while Chaos Control holds Link still. Weak, so this file also works without it.
extern "C" __attribute__((weak)) bool ShadowChaosControl_IsBusy(void);

// Version numbers that Boost.cpp, ChestJingle.cpp and ChaosControl.cpp define (see the end of each file). Weak, so a file that
// is missing from the build, or an old version of it, simply reads as 0. Used by the status report on load.
extern "C" __attribute__((weak)) int ShadowAbilityVersion_Boost;
extern "C" __attribute__((weak)) bool ShadowBoost_IsBusy(void); // Boost.cpp: true while its charge-up pose plays
extern "C" __attribute__((weak)) int ShadowAbilityVersion_ChestJingle;
extern "C" __attribute__((weak)) int ShadowAbilityVersion_ChaosControl;

namespace {

constexpr const char* kCvarEnabled = "gShadow.Jump.Enabled";
constexpr const char* kCvarGround = "gShadow.Jump.GroundJump";
constexpr const char* kCvarSdlButton = "gShadow.Jump.SdlButton";
constexpr const char* kCvarN64Button = "gShadow.Jump.N64Button";
constexpr const char* kCvarCount = "gShadow.Jump.Count";
constexpr const char* kCvarStrength = "gShadow.Jump.Strength";
constexpr const char* kCvarDoubleStrength = "gShadow.Jump.DoubleStrength";
constexpr const char* kCvarBoost = "gShadow.Jump.Boost";
constexpr const char* kCvarBoomerang = "gShadow.Jump.Boomerang";
constexpr const char* kCvarAnimation = "gShadow.Jump.Animation";
constexpr const char* kCvarFirstRate = "gShadow.Jump.FirstRate";
constexpr const char* kCvarSecondRate = "gShadow.Jump.SecondRate";
constexpr const char* kCvarCarryEnabled = "gShadow.Carry.Enabled";
constexpr const char* kCvarCarryUp = "gShadow.Carry.Up";
constexpr const char* kCvarCarryForward = "gShadow.Carry.Forward";
constexpr const char* kCvarCarrySide = "gShadow.Carry.Side";
constexpr const char* kCvarCarryActors = "gShadow.Carry.Actors";
constexpr const char* kCvarCarryJumpFollow = "gShadow.Carry.JumpFollow";
constexpr const char* kCvarCarryAnchor = "gShadow.Carry.Anchor";
constexpr const char* kFileCarryConfig = "custom/carry/config.txt";

constexpr const char* kFileFirstAdult = "custom/jump/anim_jump1_adult.bin";
constexpr const char* kFileFirstChild = "custom/jump/anim_jump1_child.bin";
constexpr const char* kFileSecondAdult = "custom/jump/anim_jump2_adult.bin";
constexpr const char* kFileSecondChild = "custom/jump/anim_jump2_child.bin";

// An air jump needs the first jump to be at least this many game frames old (the game runs at 20 frames a second).
constexpr int kMinAirFrames = 4;
// A button press seen by the event listener counts for this long (milliseconds) if the game does not look in time.
constexpr uint32_t kPressLifetimeMs = 150;

// Roc's Feather's jump: 7.5 for an adult, 7.0 for a child.
constexpr float kAdultJump = 7.5f;
constexpr float kChildJump = 7.0f;

// ------------------------------------------------------------------------------------------------
// Reading the Y button
// ------------------------------------------------------------------------------------------------

std::atomic<int> sWatchedButton{ SDL_CONTROLLER_BUTTON_Y };
std::atomic<bool> sPressPending{ false };
std::atomic<uint32_t> sPressTick{ 0 };
bool sWatchAdded = false;
bool sPolledDown = false;

// Sees every button press the moment it happens, even one that is over before the game looks (the game
// looks 20 times a second).
int SDLCALL OnSdlEvent(void*, SDL_Event* event) {
    if (event->type == SDL_CONTROLLERBUTTONDOWN && event->cbutton.button == sWatchedButton.load()) {
        sPressTick = SDL_GetTicks();
        sPressPending = true;
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

// True once per press. Called every game frame so that old presses never pile up.
bool JumpPressed() {
    if (!sWatchAdded) {
        SDL_AddEventWatch(OnSdlEvent, nullptr);
        sWatchAdded = true;
    }
    const int button = CVarGetInteger(kCvarSdlButton, SDL_CONTROLLER_BUTTON_Y);
    sWatchedButton = button;

    bool pressed = false;
    if (sPressPending.exchange(false) && SDL_GetTicks() - sPressTick.load() <= kPressLifetimeMs) {
        pressed = true;
    }
    const bool down = AnyControllerHolds(button);
    if (down && !sPolledDown) {
        pressed = true;
    }
    sPolledDown = down;

    const int n64Button = CVarGetInteger(kCvarN64Button, 0);
    if (n64Button != 0 && gPlayState != nullptr && CHECK_BTN_ANY(gPlayState->state.input[0].press.button, n64Button)) {
        pressed = true;
    }
    return pressed;
}

// ------------------------------------------------------------------------------------------------
// The jumps
// ------------------------------------------------------------------------------------------------

int sAirFrames = 0; // game frames since Link left the ground
int sJumpsLeft = 0; // air jumps still available in this stay in the air

// True while Chaos Control's start pose or the boost's charge-up pose holds Link still (Y must not break out of them).
bool ChaosControlBusy() {
    return (ShadowChaosControl_IsBusy != nullptr && ShadowChaosControl_IsBusy()) ||
           (ShadowBoost_IsBusy != nullptr && ShadowBoost_IsBusy());
}

// True when Link is not in control or is in a state where a jump makes no sense.
bool CannotJump(Player* player) {
    if (gPlayState->csCtx.state != CS_STATE_IDLE || Player_InCsMode(gPlayState)) {
        return true;
    }
    if (gPlayState->pauseCtx.state != 0 || gPlayState->transitionTrigger != TRANS_TRIGGER_OFF ||
        gPlayState->msgCtx.msgMode != MSGMODE_NONE) {
        return true;
    }
    const u32 blocked1 = PLAYER_STATE1_INPUT_DISABLED | PLAYER_STATE1_TALKING | PLAYER_STATE1_DEAD |
                         PLAYER_STATE1_GETTING_ITEM | PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_IN_WATER |
                         PLAYER_STATE1_ON_HORSE | PLAYER_STATE1_CLIMBING_LEDGE | PLAYER_STATE1_HANGING_OFF_LEDGE |
                         PLAYER_STATE1_CLIMBING_LADDER | PLAYER_STATE1_DAMAGED | PLAYER_STATE1_FIRST_PERSON |
                         PLAYER_STATE1_CARRYING_ACTOR |
                         PLAYER_STATE1_HOOKSHOT_FALLING | PLAYER_STATE1_LOADING | PLAYER_STATE1_FLOOR_DISABLED |
                         PLAYER_STATE1_READY_TO_FIRE | PLAYER_STATE1_SHIELDING |
                         PLAYER_STATE1_START_CHANGING_HELD_ITEM;
    const u32 blocked2 = PLAYER_STATE2_UNDERWATER | PLAYER_STATE2_DIVING | PLAYER_STATE2_CRAWLING |
                         PLAYER_STATE2_GRABBED_BY_ENEMY | PLAYER_STATE2_FROZEN | PLAYER_STATE2_OCARINA_PLAYING |
                         PLAYER_STATE2_STATIONARY_LADDER;
    if ((player->stateFlags1 & blocked1) || (player->stateFlags2 & blocked2)) {
        return true;
    }
    // PLAYER_STATE1_USING_BOOMERANG is set the whole time the boomerang is the item in hand (aiming, thrown, caught
    // again), so it used to block every jump while Shadow had the boomerang out. Jumping is fine then: the boomerang
    // is an upper body action that carries on by itself in the air. (gShadow.Jump.Boomerang 0 brings the old rule back.)
    if ((player->stateFlags1 & PLAYER_STATE1_USING_BOOMERANG) && CVarGetInteger(kCvarBoomerang, 1) == 0) {
        return true;
    }
    // Not in the middle of a sword swing (a jump slash must finish).
    return player->meleeWeaponState != 0;
}

// Standing, walking, running, side-stepping or Z-targeting on the ground (not in a roll, a swing or anything else).
bool OnFootAction(Player* player) {
    const auto action = player->actionFunc;
    return action == Player_Action_Idle || action == Player_Action_80840450 || action == Player_Action_808407CC ||
           action == Player_Action_80840DE4 || action == Player_Action_808414F8 || action == Player_Action_8084170C ||
           action == Player_Action_808417FC || action == Player_Action_80842180 || action == Player_Action_8084227C ||
           action == Player_Action_8084193C;
}

// ------------------------------------------------------------------------------------------------
// The poses (SA2 animation 116 for the first jump, 114 for the flip; Link's 21 limbs, 20 frames per second)
// One frame = 22 Vec3s (root position, then 21 limb rotations) + 1 face value, 67 big-endian s16.
// They are put onto Link's skeleton while the game draws him, the same way the boost and Chaos Control poses work.
// ------------------------------------------------------------------------------------------------

constexpr int kJoints = 22;
constexpr int kShortsPerFrame = kJoints * 3 + 1;
constexpr int kFadeOutFrames = 3;   // game frames to blend the pose out after landing
constexpr int kBlendInFrames = 3;   // game frames to blend from the old pose into the flip (when jumping again)
constexpr float kMinRate = 1.0f;    // the automatic playback speed stays between these (1 = the animation's own speed)
constexpr float kMaxRate = 2.5f;

struct AnimData {
    std::vector<int16_t> data;
    int frames = 0;
};

AnimData sFirstAdult, sFirstChild, sSecondAdult, sSecondChild;
bool sAssetsTried = false;

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

void LoadAssets() {
    if (sAssetsTried) {
        return;
    }
    sAssetsTried = true;
    LoadAnim(kFileFirstAdult, sFirstAdult);
    LoadAnim(kFileFirstChild, sFirstChild);
    LoadAnim(kFileSecondAdult, sSecondAdult);
    LoadAnim(kFileSecondChild, sSecondChild);
}

bool IsChild() {
    return gSaveContext.linkAge == LINK_AGE_CHILD;
}

enum class Pose { None, First, Second };

const AnimData& AnimFor(Pose pose) {
    if (pose == Pose::Second) {
        return IsChild() ? sSecondChild : sSecondAdult;
    }
    return IsChild() ? sFirstChild : sFirstAdult;
}

inline int16_t LerpS16(int16_t a, int16_t b, float w) {
    return static_cast<int16_t>(a + static_cast<int>(std::lround(static_cast<float>(b - a) * w)));
}

// Rotations: blend the short way around the circle.
inline int16_t LerpAngle(int16_t a, int16_t b, float w) {
    const int16_t diff = static_cast<int16_t>(b - a);
    return static_cast<int16_t>(a + static_cast<int>(std::lround(static_cast<float>(diff) * w)));
}

// Limbs the game drives itself (hat, collar, sheath, torso): the pose leaves them alone.
inline bool KeepGameLimb(int limb) {
    return limb == 11 || limb == 12 || limb == 19 || limb == 20;
}

// The pose at frame time t of an animation (between two frames it is blended; after the last frame it holds).
void SamplePose(const AnimData& anim, float t, Vec3s out[kJoints]) {
    const int frames = anim.frames;
    int i0 = static_cast<int>(std::floor(std::max(0.0f, t)));
    const float frac = std::max(0.0f, t) - static_cast<float>(i0);
    int i1 = i0 + 1;
    i0 = std::max(0, std::min(i0, frames - 1));
    i1 = std::max(0, std::min(i1, frames - 1));
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

void BlendPose(const Vec3s from[kJoints], const Vec3s to[kJoints], float w, Vec3s out[kJoints]) {
    for (int j = 0; j < kJoints; j++) {
        if (j == 0) {
            out[j].x = LerpS16(from[j].x, to[j].x, w);
            out[j].y = LerpS16(from[j].y, to[j].y, w);
            out[j].z = LerpS16(from[j].z, to[j].z, w);
        } else {
            out[j].x = LerpAngle(from[j].x, to[j].x, w);
            out[j].y = LerpAngle(from[j].y, to[j].y, w);
            out[j].z = LerpAngle(from[j].z, to[j].z, w);
        }
    }
}

float Approach(float value, float target, float step) {
    if (value < target) {
        return std::min(target, value + step);
    }
    return std::max(target, value - step);
}

// What is playing right now.
Pose sPose = Pose::None;
float sPoseT = 0.0f;      // frame time in the pose
float sPoseRate = 1.0f;   // pose frames per game frame
int sPoseAge = 0;         // game frames since the pose started
float sWeight = 0.0f;     // how much of the pose is shown (0 = the game's own pose, 1 = ours)
int sFadeOut = 0;         // game frames left of blending the pose out after it ended
float sFadeWeight = 0.0f;
bool sHaveLast = false;
Vec3s sLastPose[kJoints]; // the pose that was drawn last
Vec3s sFromPose[kJoints]; // the pose we blend away from when a new pose starts while one is showing
int sBlendLeft = 0;

// How many game frames until Link lands, if nothing gets in the way (his gravity, from where he is now).
int EstimateAirFrames(Player* player) {
    float y = player->actor.world.pos.y;
    float vy = player->actor.velocity.y;
    const float floorY = player->actor.floorHeight;
    const float gravity = player->actor.gravity;
    const float minVy = player->actor.minVelocityY;
    int frames = 0;
    while (frames < 80) {
        vy = std::max(vy + gravity, minVy);
        y += vy;
        frames++;
        if (y <= floorY && vy < 0.0f) {
            break;
        }
    }
    return std::max(1, frames);
}

// Start (or switch to) a pose. Called right after the jump was started, so the speed up is known.
void StartPose(Pose pose, Player* player) {
    if (!CVarGetInteger(kCvarAnimation, 1)) {
        return;
    }
    if (!sAssetsTried) {
        LoadAssets();
    }
    const AnimData& anim = AnimFor(pose);
    if (anim.frames <= 0) {
        return;
    }
    // From a pose that is showing: blend from it instead of jumping to the new pose.
    if (sHaveLast && (sPose != Pose::None || sFadeOut > 0)) {
        std::memcpy(sFromPose, sLastPose, sizeof(sFromPose));
        sBlendLeft = kBlendInFrames;
        if (sPose == Pose::None) {
            sWeight = sFadeWeight * static_cast<float>(sFadeOut) / static_cast<float>(kFadeOutFrames);
        }
    } else {
        sBlendLeft = 0;
        sWeight = 0.0f;
    }
    sFadeOut = 0;
    sPose = pose;
    sPoseT = 0.0f;
    sPoseAge = 0;

    // The playback speed: set so that the pose is over a little before Link lands (it holds its last frame after that).
    const int userRate = CVarGetInteger(pose == Pose::Second ? kCvarSecondRate : kCvarFirstRate, 0);
    if (userRate > 0) {
        sPoseRate = static_cast<float>(std::max(10, userRate)) / 100.0f;
    } else {
        const float airFrames = static_cast<float>(std::max(1, EstimateAirFrames(player) - 2));
        sPoseRate = std::max(kMinRate, std::min(kMaxRate, static_cast<float>(anim.frames) / airFrames));
    }
}

void EndPose(bool fade) {
    if (sPose == Pose::None) {
        return;
    }
    sPose = Pose::None;
    sBlendLeft = 0;
    sFadeWeight = sWeight;
    sFadeOut = (fade && sHaveLast && sWeight > 0.05f) ? kFadeOutFrames : 0;
}

void ResetPose() {
    EndPose(false);
    sFadeOut = 0;
    sHaveLast = false;
    sWeight = 0.0f;
}

float JumpSpeed(const char* strengthCvar) {
    const bool child = gSaveContext.linkAge == LINK_AGE_CHILD;
    const float strength = static_cast<float>(std::max(0, CVarGetInteger(strengthCvar, 100))) / 100.0f;
    return (child ? kChildJump : kAdultJump) * strength;
}

void GroundJump(Player* player) {
    // The game's own jump: run or standing jump animation, jump sound and voice, then our jump speed on top.
    func_8083A4A8(player, gPlayState);
    player->actor.velocity.y = std::max(player->actor.velocity.y, JumpSpeed(kCvarStrength));
    StartPose(Pose::First, player);
}

void AirJump(Player* player) {
    const float jump = JumpSpeed(kCvarDoubleStrength);

    // The normal jump: Shadow's jump pose, the game's jump sound and voice.
    func_80838940(player, (LinkAnimationHeader*)&gPlayerAnim_link_normal_jump, jump, gPlayState, NA_SE_VO_LI_SWORD_N);

    // Same fix as Roc's Feather: stops the animation from morphing into the falling pose at once.
    player->av2.actionVar2 = 1;
    player->actor.velocity.y = std::max(player->actor.velocity.y, jump);

    if (CVarGetInteger(kCvarBoost, 1) != 0) {
        player->linearVelocity = std::max(player->linearVelocity, 3.0f);
    }
    // Lets Link grab ledges again after a side hop or back flip.
    player->stateFlags2 &= ~PLAYER_STATE2_HOPPING;
    StartPose(Pose::Second, player); // the flip
}

void OnPlayerUpdateJump() {
    if (gPlayState == nullptr) {
        return;
    }
    const bool pressed = JumpPressed(); // always read, so old presses never pile up
    if (!CVarGetInteger(kCvarEnabled, 1)) {
        sAirFrames = 0;
        ResetPose();
        return;
    }
    Player* player = GET_PLAYER(gPlayState);
    if (player == nullptr) {
        return;
    }

    const bool onGround = (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) != 0;
    if (onGround) {
        sAirFrames = 0;
        sJumpsLeft = std::max(0, CVarGetInteger(kCvarCount, 1));
    } else {
        sAirFrames++;
    }

    // The pose that is playing: it follows the jump and ends when Link lands or anything else takes over.
    if (sPose != Pose::None) {
        sPoseAge++;
        sPoseT += sPoseRate;
        sWeight = Approach(sWeight, 1.0f, 0.5f);
        if (sBlendLeft > 0) {
            sBlendLeft--;
        }
        const bool inAir = player->actionFunc == Player_Action_8084411C;
        if ((sPoseAge >= 2 && !inAir) || CannotJump(player) || ChaosControlBusy()) {
            EndPose(true);
        }
    } else if (sFadeOut > 0) {
        sFadeOut--;
    }

    // Y does not work while Chaos Control's start pose plays; once time has stopped it does.
    if (!pressed || ChaosControlBusy() || CannotJump(player)) {
        return;
    }

    if (onGround) {
        if (CVarGetInteger(kCvarGround, 1) != 0 && OnFootAction(player)) {
            GroundJump(player);
        }
        return;
    }

    if (sAirFrames >= kMinAirFrames && sJumpsLeft > 0 &&
        (player->stateFlags1 & (PLAYER_STATE1_JUMPING | PLAYER_STATE1_FREEFALL))) {
        sJumpsLeft--;
        AirJump(player);
    }
}

// Runs right before the game draws the frame, after the game has put Link's normal pose into his joint table.
void OnPlayDrawBeginJump() {
    if (gPlayState == nullptr) {
        return;
    }
    const bool active = sPose != Pose::None;
    const bool fading = !active && sFadeOut > 0 && sHaveLast;
    if (!active && !fading) {
        return;
    }
    Player* player = GET_PLAYER(gPlayState);
    if (player == nullptr || player->skelAnime.jointTable == nullptr) {
        return;
    }

    Vec3s target[kJoints];
    float weight;
    if (active) {
        const AnimData& anim = AnimFor(sPose);
        if (!CVarGetInteger(kCvarAnimation, 1) || anim.frames <= 0 || sWeight <= 0.0f) {
            return;
        }
        SamplePose(anim, sPoseT, target);
        if (sBlendLeft > 0) { // coming from another pose: move over to the new one in a few frames
            const float w = 1.0f - static_cast<float>(sBlendLeft) / static_cast<float>(kBlendInFrames + 1);
            BlendPose(sFromPose, target, w, target);
        }
        std::memcpy(sLastPose, target, sizeof(target));
        sHaveLast = true;
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

// ------------------------------------------------------------------------------------------------
// Carrying things
// ------------------------------------------------------------------------------------------------

// 1. Where the carried actor sits. While Link draws, the game puts the carried actor halfway between his two hands
//    (the right hand's position and Player::leftHandPos). The hook below runs just before that happens and moves both
//    hand positions, so the carried actor ends up moved by the same amount. The positions are rebuilt by the game at
//    every draw, and are put back at the end of the draw anyway.

constexpr float kDefaultCarryUp = 10.0f;
constexpr float kDefaultCarryForward = 4.0f;
constexpr float kDefaultCarrySide = 0.0f;
constexpr float kNoCvar = 12345.0f;
constexpr int kRightHandBodyPart = PLAYER_BODYPART_R_HAND; // the game averages this hand with the left one (index 15)
constexpr int kUpperLimb = 9;          // joint table index of the chest (PLAYER_LIMB_UPPER - 1)
constexpr int kHeadLimb = 10;          // joint table index of the head (PLAYER_LIMB_HEAD - 1)

float sCfgUp = kDefaultCarryUp;
float sCfgForward = kDefaultCarryForward;
float sCfgSide = kDefaultCarrySide;
bool sCarryConfigTried = false;

void LoadCarryConfig() {
    if (sCarryConfigTried) {
        return;
    }
    sCarryConfigTried = true;
    auto file = LoadModFile(kFileCarryConfig);
    if (file == nullptr) {
        return;
    }
    const std::string text(file->Buffer->begin(), file->Buffer->end());
    size_t pos = 0;
    while (pos < text.size()) {
        size_t end = text.find('\n', pos);
        if (end == std::string::npos) {
            end = text.size();
        }
        const std::string line = text.substr(pos, end - pos);
        pos = end + 1;
        const size_t eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        const std::string key = line.substr(0, eq);
        const float value = static_cast<float>(std::atof(line.c_str() + eq + 1));
        if (key == "up") {
            sCfgUp = value;
        } else if (key == "forward") {
            sCfgForward = value;
        } else if (key == "side") {
            sCfgSide = value;
        }
    }
}

// The console's "set" stores a whole number (12) as an integer and a number with a point (12.5) as a float, and each
// getter only sees its own kind, so both are looked at.
float CarrySetting(const char* cvar, float fromFile) {
    const float v = CVarGetFloat(cvar, kNoCvar);
    if (v != kNoCvar) {
        return v;
    }
    const int32_t i = CVarGetInteger(cvar, INT32_MIN);
    return i == INT32_MIN ? fromFile : static_cast<float>(i);
}

bool IsMovedActor(const Actor* held) {
    if (CVarGetInteger(kCvarCarryActors, 0) != 0) {
        return true;
    }
    return held->id == ACTOR_EN_RU1 || held->id == ACTOR_EN_RU2; // Princess Ruto
}

Vec3f sSavedLeftHand;
Vec3f sSavedRightHand;
bool sHandsShifted = false;

// Where the game put the carried actor (relative to Link) the last time he was drawn, and which way he faced.
Vec3f sCarryOffset = { 0.0f, 0.0f, 0.0f };
int16_t sCarryYaw = 0;
bool sCarryOffsetValid = false;

void OnDrawHandsCarry(Player* player) {
    if (player == nullptr || !CVarGetInteger(kCvarCarryEnabled, 1) ||
        !(player->stateFlags1 & PLAYER_STATE1_CARRYING_ACTOR) || player->heldActor == nullptr ||
        !IsMovedActor(player->heldActor)) {
        return;
    }
    if (!sCarryConfigTried) {
        LoadCarryConfig();
    }
    const float up = CarrySetting(kCvarCarryUp, sCfgUp);
    const float forward = CarrySetting(kCvarCarryForward, sCfgForward);
    const float side = CarrySetting(kCvarCarrySide, sCfgSide);
    if (!(up == 0.0f && forward == 0.0f && side == 0.0f)) {
        // Link faces +Z at yaw 0, so forward is (sin, cos); his right is (-cos, sin).
        const float yaw = static_cast<float>(player->actor.shape.rot.y) * (3.14159265f / 32768.0f);
        const float dx = forward * std::sin(yaw) - side * std::cos(yaw);
        const float dz = forward * std::cos(yaw) + side * std::sin(yaw);

        sSavedLeftHand = player->leftHandPos;
        sSavedRightHand = player->bodyPartsPos[kRightHandBodyPart];
        sHandsShifted = true;
        player->leftHandPos.x += dx;
        player->leftHandPos.y += up;
        player->leftHandPos.z += dz;
        player->bodyPartsPos[kRightHandBodyPart].x += dx;
        player->bodyPartsPos[kRightHandBodyPart].y += up;
        player->bodyPartsPos[kRightHandBodyPart].z += dz;
    }
    // Where the game is about to put the carried actor, relative to Link. Used to keep the actor with him during the updates.
    sCarryOffset.x = (player->leftHandPos.x + player->bodyPartsPos[kRightHandBodyPart].x) * 0.5f - player->actor.world.pos.x;
    sCarryOffset.y = (player->leftHandPos.y + player->bodyPartsPos[kRightHandBodyPart].y) * 0.5f - player->actor.world.pos.y;
    sCarryOffset.z = (player->leftHandPos.z + player->bodyPartsPos[kRightHandBodyPart].z) * 0.5f - player->actor.world.pos.z;
    sCarryYaw = player->actor.shape.rot.y;
    sCarryOffsetValid = true;
}

// After the world has been drawn: the hand positions go back to what the game worked out (other code uses them).
void OnPlayDrawEndCarry() {
    if (!sHandsShifted) {
        return;
    }
    sHandsShifted = false;
    if (gPlayState == nullptr) {
        return;
    }
    Player* player = GET_PLAYER(gPlayState);
    if (player == nullptr) {
        return;
    }
    player->leftHandPos = sSavedLeftHand;
    player->bodyPartsPos[kRightHandBodyPart] = sSavedRightHand;
}

// 1b. Keeping a carried Ruto with Shadow. The game only puts the carried actor between Link's hands while it draws him.
//     In between (the updates, which is also when a throw starts) the actor stays wherever it was. Reported Nov 21: Ruto
//     trailed behind while Shadow walked and a throw started from where she was, not from Shadow. So the actor is moved
//     to Link's position plus the offset the game used at the last draw, right after the player update and again right
//     after the actor's own update. The game's own placement at the next draw still has the last word.

bool KeepsHeldActor(Player* player, const Actor* actor) {
    return player != nullptr && actor != nullptr && CVarGetInteger(kCvarCarryEnabled, 1) != 0 &&
           CVarGetInteger(kCvarCarryAnchor, 1) != 0 && (player->stateFlags1 & PLAYER_STATE1_CARRYING_ACTOR) &&
           player->heldActor == actor && IsMovedActor(actor) && sCarryOffsetValid;
}

void AnchorHeldActor(Player* player) {
    Actor* held = player->heldActor;
    if (!KeepsHeldActor(player, held)) {
        return;
    }
    // The offset is turned along with Link if he has turned since the last draw (forward is (sin, cos)).
    const float turn = static_cast<float>(static_cast<int16_t>(player->actor.shape.rot.y - sCarryYaw)) * (3.14159265f / 32768.0f);
    const float c = std::cos(turn);
    const float s = std::sin(turn);
    const float ox = sCarryOffset.x * c + sCarryOffset.z * s;
    const float oz = sCarryOffset.z * c - sCarryOffset.x * s;
    held->world.pos.x = player->actor.world.pos.x + ox;
    held->world.pos.y = player->actor.world.pos.y + sCarryOffset.y;
    held->world.pos.z = player->actor.world.pos.z + oz;
}

void OnActorUpdateCarry(void* actor) {
    if (gPlayState == nullptr || actor == nullptr) {
        return;
    }
    Player* player = GET_PLAYER(gPlayState);
    if (player != nullptr && player->heldActor == static_cast<Actor*>(actor)) {
        AnchorHeldActor(player);
    }
}

// 2. Jumping while carrying. The upper body animation (carrying) is copied over the chest, head and arms of the
//    main skeleton after the jump animation has been loaded, so only the legs showed the jump. At the end of the
//    player update the main skeleton still holds the jump animation's chest and head, so those are saved here and put
//    back when the frame is drawn. The arms keep the carry animation, so Shadow keeps holding on.

Vec3s sJumpChest[2];      // chest and head rotations of the jump animation
float sJumpFollow = 0.0f; // 0..1 how much of the jump animation the chest and head get

void OnPlayerUpdateCarry() {
    if (gPlayState == nullptr) {
        return;
    }
    Player* player = GET_PLAYER(gPlayState);
    if (player != nullptr) {
        if (!(player->stateFlags1 & PLAYER_STATE1_CARRYING_ACTOR) || player->heldActor == nullptr) {
            sCarryOffsetValid = false; // nothing carried: the next carry starts from the game's own placement
        } else {
            AnchorHeldActor(player);
        }
    }
    if (player == nullptr || player->skelAnime.jointTable == nullptr || player->skelAnime.limbCount <= kHeadLimb) {
        sJumpFollow = 0.0f;
        return;
    }
    const bool want = CVarGetInteger(kCvarCarryEnabled, 1) != 0 && CVarGetInteger(kCvarCarryJumpFollow, 1) != 0 &&
                      (player->stateFlags1 & PLAYER_STATE1_CARRYING_ACTOR) && player->heldActor != nullptr &&
                      !(player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) &&
                      player->actionFunc == Player_Action_8084411C;
    if (want) {
        sJumpChest[0] = player->skelAnime.jointTable[kUpperLimb];
        sJumpChest[1] = player->skelAnime.jointTable[kHeadLimb];
    }
    sJumpFollow = Approach(sJumpFollow, want ? 1.0f : 0.0f, want ? 0.5f : 0.34f);
}

void OnPlayDrawBeginCarry() {
    if (gPlayState == nullptr || sJumpFollow <= 0.0f) {
        return;
    }
    Player* player = GET_PLAYER(gPlayState);
    if (player == nullptr || player->skelAnime.jointTable == nullptr || player->skelAnime.limbCount <= kHeadLimb) {
        return;
    }
    Vec3s* table = player->skelAnime.jointTable;
    const int limbs[2] = { kUpperLimb, kHeadLimb };
    for (int i = 0; i < 2; i++) {
        Vec3s& t = table[limbs[i]];
        t.x = LerpAngle(t.x, sJumpChest[i].x, sJumpFollow);
        t.y = LerpAngle(t.y, sJumpChest[i].y, sJumpFollow);
        t.z = LerpAngle(t.z, sJumpChest[i].z, sJumpFollow);
    }
}

// ------------------------------------------------------------------------------------------------
// Status report. This file is the one that is known to be in the build, so it tells you which of the other files are in it.
// A few seconds after a save has loaded (a message that is shown during the loading screen is gone before the game starts),
// one small message per ability appears at the bottom right and stays for 12 seconds. "Shadow Boost: NOT in this build"
// means Boost.cpp is missing from the branch or is an old version of the file; the same goes for the others.
// gShadow.Status 0 turns the report off.
// ------------------------------------------------------------------------------------------------

constexpr int kJumpVersion = 6;
constexpr const char* kCvarStatus = "gShadow.Status";
constexpr int kStatusDelayFrames = 40; // game frames of play after the load (2 seconds)

bool sStatusPending = false;
int sStatusFrames = 0;

int VersionOf(const int* version) {
    return version != nullptr ? *version : 0;
}

bool ModFileExists(const char* path) {
    Ship::Context* ctx = Ship::Context::GetRawInstance();
    if (ctx == nullptr || ctx->GetResourceManager() == nullptr) {
        return false;
    }
    auto archiveManager = ctx->GetResourceManager()->GetArchiveManager();
    return archiveManager != nullptr && archiveManager->HasFile(path);
}

void SayStatus(const std::string& text) {
    Notification::Emit({ .message = text, .remainingTime = 12.0f, .mute = true });
}

void ReportStatus() {
    const int boost = VersionOf(&ShadowAbilityVersion_Boost);
    const int chest = VersionOf(&ShadowAbilityVersion_ChestJingle);
    const int chaos = VersionOf(&ShadowAbilityVersion_ChaosControl);

    SayStatus(std::string("Shadow Jump v") + std::to_string(kJumpVersion) + ": OK, " +
              (sFirstAdult.frames > 0 && sSecondAdult.frames > 0 ? "poses found" : "poses NOT found"));
    if (boost > 0) {
        SayStatus(std::string("Shadow Boost v") + std::to_string(boost) + ": OK, " +
                  (ModFileExists("custom/boost/anim_run_adult.bin") ? "poses found, " : "poses NOT found, ") +
                  (ModFileExists("custom/boost/music.wav") ? "tune found" : "tune NOT found"));
    } else {
        SayStatus("Shadow Boost: NOT in this build (Boost.cpp missing or old)");
    }
    if (chest > 0) {
        SayStatus(std::string("Shadow Chest jingle v") + std::to_string(chest) + ": OK, " +
                  (ModFileExists("custom/chest_jingle/jingle.wav") ? "tune found" : "tune NOT found"));
    } else {
        SayStatus("Shadow Chest jingle: NOT in this build (ChestJingle.cpp missing or old)");
    }
    if (chaos > 0) {
        SayStatus(std::string("Shadow Chaos Control v") + std::to_string(chaos) + ": OK");
    } else {
        SayStatus("Shadow Chaos Control: old version (ChaosControl.cpp)");
    }
}

void OnPlayerUpdateStatus() {
    if (!sStatusPending) {
        return;
    }
    if (++sStatusFrames < kStatusDelayFrames) {
        return;
    }
    sStatusPending = false;
    if (CVarGetInteger(kCvarStatus, 1) != 0) {
        ReportStatus();
    }
}

void RegisterJump() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayerUpdate>(OnPlayerUpdateJump);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayDrawBegin>(OnPlayDrawBeginJump);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayerUpdate>(OnPlayerUpdateCarry);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayerUpdate>(OnPlayerUpdateStatus);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayDrawBegin>(OnPlayDrawBeginCarry);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayDrawEnd>(OnPlayDrawEndCarry);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnActorUpdate>(OnActorUpdateCarry);
    // The game asks this while it draws Link's right hand, just before it puts a carried actor between his hands.
    REGISTER_VB_SHOULD(VB_DRAW_ADDITIONAL_RETICLES, {
        (void)should;
        OnDrawHandsCarry(va_arg(args, Player*));
    });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneInit>([](int16_t) {
        sAirFrames = 0;
        sJumpsLeft = 0;
        ResetPose();
    });
    // The pause menu does not run player updates, so end the pose when it opens.
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnKaleidoscopeUpdate>([](int16_t) { ResetPose(); });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnLoadGame>([](int32_t) {
        ResetPose();
        LoadAssets();
        sCarryConfigTried = false; // read custom/carry/config.txt again
        LoadCarryConfig();
        sJumpFollow = 0.0f;
        sStatusPending = true; // the status report comes once the game is running
        sStatusFrames = 0;
    });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnExitGame>([](int32_t) { ResetPose(); });
}

static RegisterShipInitFunc sJumpInit(RegisterJump);

} // namespace

extern "C" {
int ShadowAbilityVersion_Jump = 3;
}
