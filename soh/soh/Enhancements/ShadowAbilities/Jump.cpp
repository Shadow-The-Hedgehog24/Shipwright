// Shadow the Hedgehog - Jump and Double Jump for Ship of Harkinian 9.3.1
//
// Ocarina of Time has no jump button. This adds one: the Y button.
//   * Y while standing, walking or running on the ground: Shadow jumps, in his SA2 jump pose (animation 116, a tucked jump).
//   * Y again while in the air: a second jump, in his SA2 flip (animation 114). One extra jump per time in the air;
//     touching the ground gives it back. Y works during Chaos Control once time has stopped.
//   Walking off a ledge or jumping with the A button still uses the game's own jump and fall animations.
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
//   gShadow.Jump.Animation       1 = use the jump and flip poses (default), 0 = the game's own jump animation
//   gShadow.Jump.FirstRate       playback speed of the first jump pose in percent, default 0 = automatic (the pose
//                                is sped up so that it lasts as long as the jump)
//   gShadow.Jump.SecondRate      the same for the flip, default 0 = automatic

#include <libultraship/bridge/consolevariablebridge.h>

#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"

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

namespace {

constexpr const char* kCvarEnabled = "gShadow.Jump.Enabled";
constexpr const char* kCvarGround = "gShadow.Jump.GroundJump";
constexpr const char* kCvarSdlButton = "gShadow.Jump.SdlButton";
constexpr const char* kCvarN64Button = "gShadow.Jump.N64Button";
constexpr const char* kCvarCount = "gShadow.Jump.Count";
constexpr const char* kCvarStrength = "gShadow.Jump.Strength";
constexpr const char* kCvarDoubleStrength = "gShadow.Jump.DoubleStrength";
constexpr const char* kCvarBoost = "gShadow.Jump.Boost";
constexpr const char* kCvarAnimation = "gShadow.Jump.Animation";
constexpr const char* kCvarFirstRate = "gShadow.Jump.FirstRate";
constexpr const char* kCvarSecondRate = "gShadow.Jump.SecondRate";

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

bool ChaosControlBusy() {
    return ShadowChaosControl_IsBusy != nullptr && ShadowChaosControl_IsBusy();
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
                         PLAYER_STATE1_CARRYING_ACTOR | PLAYER_STATE1_USING_BOOMERANG |
                         PLAYER_STATE1_HOOKSHOT_FALLING | PLAYER_STATE1_LOADING | PLAYER_STATE1_FLOOR_DISABLED |
                         PLAYER_STATE1_READY_TO_FIRE | PLAYER_STATE1_SHIELDING |
                         PLAYER_STATE1_START_CHANGING_HELD_ITEM;
    const u32 blocked2 = PLAYER_STATE2_UNDERWATER | PLAYER_STATE2_DIVING | PLAYER_STATE2_CRAWLING |
                         PLAYER_STATE2_GRABBED_BY_ENEMY | PLAYER_STATE2_FROZEN | PLAYER_STATE2_OCARINA_PLAYING |
                         PLAYER_STATE2_STATIONARY_LADDER;
    if ((player->stateFlags1 & blocked1) || (player->stateFlags2 & blocked2)) {
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

void RegisterJump() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayerUpdate>(OnPlayerUpdateJump);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayDrawBegin>(OnPlayDrawBeginJump);
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
    });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnExitGame>([](int32_t) { ResetPose(); });
}

static RegisterShipInitFunc sJumpInit(RegisterJump);

} // namespace
