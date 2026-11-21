// Shadow the Hedgehog - Jump and Double Jump for Ship of Harkinian 9.3.1
//
// Ocarina of Time has no jump button. This adds one: the Y button.
//   * Y while standing, walking or running on the ground: Shadow jumps (the game's own jump animation, so he uses
//     Shadow's jump pose from the animation mod).
//   * Y again while in the air: a second jump. One extra jump per time in the air; touching the ground gives it back.
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

#include <libultraship/bridge/consolevariablebridge.h>

#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"

#include <SDL2/SDL.h>

#include <algorithm>
#include <atomic>
#include <cstdint>

extern "C" {
#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
#include "objects/gameplay_keep/gameplay_keep.h"
extern PlayState* gPlayState;

// The game's own "start a jump" (used when Link runs off a ledge). It picks the run or standing jump animation.
s32 func_8083A4A8(Player* player, PlayState* play);

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

float JumpSpeed(const char* strengthCvar) {
    const bool child = gSaveContext.linkAge == LINK_AGE_CHILD;
    const float strength = static_cast<float>(std::max(0, CVarGetInteger(strengthCvar, 100))) / 100.0f;
    return (child ? kChildJump : kAdultJump) * strength;
}

void GroundJump(Player* player) {
    // The game's own jump: run or standing jump animation, jump sound and voice, then our jump speed on top.
    func_8083A4A8(player, gPlayState);
    player->actor.velocity.y = std::max(player->actor.velocity.y, JumpSpeed(kCvarStrength));
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
}

void OnPlayerUpdateJump() {
    if (gPlayState == nullptr) {
        return;
    }
    const bool pressed = JumpPressed(); // always read, so old presses never pile up
    if (!CVarGetInteger(kCvarEnabled, 1)) {
        sAirFrames = 0;
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

void RegisterJump() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayerUpdate>(OnPlayerUpdateJump);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneInit>([](int16_t) {
        sAirFrames = 0;
        sJumpsLeft = 0;
    });
}

static RegisterShipInitFunc sJumpInit(RegisterJump);

} // namespace
