// Shadow the Hedgehog - Homing Attack (the Chaos teleport strike) for Ship of Harkinian 9.3.1
//
// Version 2 (version 1 plus: works with the boost, see ShadowHoming_EnemiesAtFullSpeed). In the air, when Navi hovers over an enemy (the one you could lock onto with Z, or the one you are already
// locked onto), press Y: Shadow vanishes in a flash of cyan light, reappears next to the enemy and hits it with one
// master sword slash, then bounces off it. Y in the air with nothing to target is still the double jump (Jump.cpp).
//
//   * The press: Shadow leaves a cyan silhouette where he was (he is in the dive pose), a few fainter copies mark the way, a
//     bright cyan diamond of light with a purple halo opens where he vanished, the screen takes a short blue wash.
//   * He is hidden for HideFrames game frames (2 = a tenth of a second), standing already at the end of the jump.
//   * The strike: he appears in front of the enemy, facing it, in the flip kick pose, with a cyan shell over him that fades
//     to his own colours and the diamond of light closing on him. For the first StrikeFrames game frames he has a hit
//     area (an attack collider with the damage type of a master sword slash, so every enemy reacts exactly as it does to the
//     sword: its own damage table, hit sound, knockback, stun). Only the first frame that hits counts.
//   * Then he bounces up and away from the enemy, so another Y with another enemy under Navi chains the attacks.
//
// The poses (custom/homing/anim_{launch,strike}_{adult,child}.bin) are in chaos_control_audio.o2r and are played by
// Jump.cpp (it owns Shadow's pose blending). The silhouettes use shadow_ghost.o2r like the boost. Without those files the
// attack still works, just with fewer effects.
//
// Settings (CVars, change with the in-game console: set <name> <value>):
//   gShadow.Homing.Enabled       1 = on (default), 0 = off (Y in the air is only the double jump)
//   gShadow.Homing.Range         furthest target in game units, default 800
//   gShadow.Homing.Distance      how far in front of the enemy Shadow lands, default 36 (plus half the enemy's width)
//   gShadow.Homing.HideFrames    game frames Shadow is hidden (the vanish), default 2, 0 = appear at once
//   gShadow.Homing.StrikeFrames  game frames the hit area stays, default 3
//   gShadow.Homing.Damage        damage type: 0 = Kokiri sword slash, 1 = master sword slash (default), 2 = Biggoron slash
//   gShadow.Homing.Radius        radius of the hit area, default 60
//   gShadow.Homing.Height        height of the hit area, default 100
//   gShadow.Homing.Bounce        bounce height after the strike in percent of a jump, default 100
//   gShadow.Homing.BounceBack    how far he is thrown back from the enemy in percent, default 100
//   gShadow.Homing.Invulnerable  1 = Shadow cannot be hurt from the press to the end of the strike (default), 0 = can
//   gShadow.Homing.Effect        1 = the teleport look (silhouettes, light, wash) on (default), 0 = off
//   gShadow.Homing.Ghosts        1 = the cyan silhouettes (default), 0 = none (needs shadow_ghost.o2r)
//   gShadow.Homing.Glow          1 = the diamond of light (default), 0 = none
//   gShadow.Homing.GlowSize      size of the diamond of light in percent, default 100
//   gShadow.Homing.Flash         strength of the blue wash over the screen in percent, default 100, 0 = none
//   gShadow.Homing.Sound         1 = play a game sound at the vanish (default), 0 = silent
//   gShadow.Homing.SoundOut      the game's sound id for the vanish (default: Link's warp out sound), 0 = none
//   gShadow.Homing.SoundIn       the game's sound id for the appearance, default 0 = none (the sword hit has its own sound)
//   gShadow.Homing.Debug         1 = small on-screen messages (started / why a target was refused), default 1 while testing
//
// Everything is in this one file (plus the small hooks in Jump.cpp). CMake picks it up automatically (soh/*.cpp is globbed).

#include <libultraship/bridge/consolevariablebridge.h>

#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"
#include "soh/Notification/Notification.h"

#include <ship/Context.h>
#include <ship/resource/File.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/ArchiveManager.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>

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
}

// Provided by Jump.cpp (which owns the pose blending): 1 = the dive pose (the vanish), 2 = the flip kick (the strike).
// Weak, so this file also links without it (then there are no poses).
extern "C" __attribute__((weak)) void ShadowJump_StartHomingPose(int which);

namespace {

constexpr const char* kCvarEnabled = "gShadow.Homing.Enabled";
constexpr const char* kCvarRange = "gShadow.Homing.Range";
constexpr const char* kCvarDistance = "gShadow.Homing.Distance";
constexpr const char* kCvarHide = "gShadow.Homing.HideFrames";
constexpr const char* kCvarStrikeFrames = "gShadow.Homing.StrikeFrames";
constexpr const char* kCvarDamage = "gShadow.Homing.Damage";
constexpr const char* kCvarRadius = "gShadow.Homing.Radius";
constexpr const char* kCvarHeight = "gShadow.Homing.Height";
constexpr const char* kCvarBounce = "gShadow.Homing.Bounce";
constexpr const char* kCvarBounceBack = "gShadow.Homing.BounceBack";
constexpr const char* kCvarInvulnerable = "gShadow.Homing.Invulnerable";
constexpr const char* kCvarFx = "gShadow.Homing.Effect";
constexpr const char* kCvarFxGhosts = "gShadow.Homing.Ghosts";
constexpr const char* kCvarFxGlow = "gShadow.Homing.Glow";
constexpr const char* kCvarFxGlowSize = "gShadow.Homing.GlowSize";
constexpr const char* kCvarFxFlash = "gShadow.Homing.Flash";
constexpr const char* kCvarSound = "gShadow.Homing.Sound";
constexpr const char* kCvarSoundOut = "gShadow.Homing.SoundOut";
constexpr const char* kCvarSoundIn = "gShadow.Homing.SoundIn";
constexpr const char* kCvarDebug = "gShadow.Homing.Debug";

// The see-through copy of Shadow's model (shadow_ghost.o2r), the same one the boost uses for its after images.
constexpr const char* kGhostSkelAdult = "objects/object_link_boy/gLinkAdultGhostSkel";
constexpr const char* kGhostSkelChild = "objects/object_link_child/gLinkChildGhostSkel";

constexpr int kJoints = 22; // root position + 21 limbs

// How long the looks last, in game frames (20 a second).
constexpr int kGlowLifeOut = 6;
constexpr int kGlowLifeIn = 5;
constexpr int kGhostLifeDepart = 6;
constexpr int kGhostLifeStreak = 4;
constexpr int kGhostLifeShell = 4;
constexpr int kFlashLife = 4;

// ------------------------------------------------------------------------------------------------
// Small helpers
// ------------------------------------------------------------------------------------------------

bool IsChild() {
    return gSaveContext.linkAge == LINK_AGE_CHILD;
}

int ClampInt(int v, int lo, int hi) {
    return std::max(lo, std::min(hi, v));
}

void Say(const std::string& text) {
    if (CVarGetInteger(kCvarDebug, 1) != 0) {
        Notification::Emit({ .message = text, .remainingTime = 3.0f, .mute = true });
    }
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

// OPEN_DISPS / CLOSE_DISPS declare the frame interpolation functions inside the function; in a nameless namespace those
// would be new functions that do not exist. These two do the same with the C functions (declared at the top) by name.
#define HOMING_OPEN_DISPS(gfxCtx)                   \
    {                                               \
        GraphicsContext* __gfxCtx = (gfxCtx);       \
        (void)__gfxCtx;                             \
        ::FrameInterpolation_RecordOpenChild(__FILE__, __LINE__)
#define HOMING_CLOSE_DISPS()                        \
    ::FrameInterpolation_RecordCloseChild();        \
    }

// ------------------------------------------------------------------------------------------------
// The attack
// ------------------------------------------------------------------------------------------------

enum class Phase { Idle, Hidden, Strike };

Phase sPhase = Phase::Idle;
u32 sStartFrame = 0;   // PlayState::gameplayFrames at the press
u32 sRevealFrame = 0;  // ... at the moment Shadow appears
u32 sLastBusyFrame = 0;     // ... at the last frame the attack was running (see ShadowHoming_EnemiesAtFullSpeed)
bool sHaveBusyFrame = false;
Vec3f sDepart = { 0.0f, 0.0f, 0.0f };
Vec3s sDepartRot = { 0, 0, 0 };
Vec3f sDepartScale = { 0.01f, 0.01f, 0.01f };
Vec3f sDest = { 0.0f, 0.0f, 0.0f };
Vec3f sTargetPos = { 0.0f, 0.0f, 0.0f };
s16 sFaceYaw = 0;
bool sHidden = false;     // we set PLAYER_STATE2_DISABLE_DRAW and have to take it off again
bool sHit = false;
bool sBounced = false;
int sRegistered = 0;      // how many frames the hit area was registered

ColliderCylinder sCollider;

u32 DamageFlags() {
    switch (CVarGetInteger(kCvarDamage, 1)) {
        case 0:
            return DMG_SLASH_KOKIRI;
        case 2:
            return DMG_SLASH_GIANT;
        default:
            return DMG_SLASH_MASTER;
    }
}

// Builds the hit area: an attack collider of the player's kind (it hurts what the player's sword hurts) with the damage
// type of a sword slash, attached to Shadow so the game treats the hit as his.
void SetupCollider(PlayState* play, Player* player) {
    ColliderCylinderInit init;
    std::memset(&init, 0, sizeof(init));
    init.base.colType = COLTYPE_NONE;
    init.base.atFlags = AT_ON | AT_TYPE_PLAYER;
    init.base.acFlags = AC_NONE;
    init.base.ocFlags1 = OC1_NONE;
    init.base.ocFlags2 = OC2_TYPE_PLAYER;
    init.base.shape = COLSHAPE_CYLINDER;
    init.info.elemType = ELEMTYPE_UNK2;
    init.info.toucher.dmgFlags = DamageFlags();
    init.info.toucher.effect = 0;
    init.info.toucher.damage = 1;
    init.info.bumper.dmgFlags = 0;
    init.info.bumper.effect = 0;
    init.info.bumper.defense = 0;
    init.info.toucherFlags = TOUCH_ON | TOUCH_SFX_NORMAL;
    init.info.bumperFlags = BUMP_NONE;
    init.info.ocElemFlags = OCELEM_NONE;
    init.dim.radius = static_cast<s16>(ClampInt(CVarGetInteger(kCvarRadius, 60), 10, 400));
    init.dim.height = static_cast<s16>(ClampInt(CVarGetInteger(kCvarHeight, 100), 10, 600));
    init.dim.yShift = -30;
    init.dim.pos.x = 0;
    init.dim.pos.y = 0;
    init.dim.pos.z = 0;
    Collider_InitCylinder(play, &sCollider);
    Collider_SetCylinder(play, &sCollider, &player->actor, &init);
}

// Puts the hit area at Shadow for this frame. The game checks it against everything that can be hurt after all actors
// have updated, and the enemies see the hit in their next update, the same as with the sword.
void RegisterStrike(PlayState* play) {
    Vec3s pos;
    pos.x = static_cast<s16>(std::lround(sDest.x));
    pos.y = static_cast<s16>(std::lround(sDest.y));
    pos.z = static_cast<s16>(std::lround(sDest.z));
    Collider_SetCylinderPosition(&sCollider, &pos);
    CollisionCheck_SetAT(play, &play->colChkCtx, &sCollider.base);
    sRegistered++;
}

// ------------------------------------------------------------------------------------------------
// The looks (all drawn at the end of the world's draw, in the colours of the cyan teleport light)
// ------------------------------------------------------------------------------------------------

constexpr int kMaxGhosts = 16;
constexpr int kMaxGlows = 4;

struct Ghost {
    bool used;
    bool child;
    uint32_t id;
    u32 startFrame;
    int life;
    float scaleMul;
    float opacity;
    Vec3f pos;
    Vec3s rot;
    Vec3f scale;
    float yOffset;
    float rootDrop;
    Vec3s upperRot;
    Vec3s headRot;
    s16 upperYawSecondary;
    Vec3s joints[kJoints];
};

struct Glow {
    bool used;
    bool arrival; // false: grows and fades where Shadow vanished; true: closes on Shadow where he appears
    uint32_t id;
    u32 startFrame;
    int life;
    Vec3f pos;
};

Ghost sGhosts[kMaxGhosts];
Glow sGlows[kMaxGlows];
uint32_t sNextId = 1;
bool sDepartPending = false; // the silhouette at the vanish is made when the frame is drawn (the pose is in the joint table then)
bool sArrivePending = false;
u32 sFlashStart = 0;
bool sFlashOn = false;
int sGhostSkelState[2] = { -1, -1 }; // adult, child: -1 not checked, 0 missing, 1 found

bool FxEnabled() {
    return CVarGetInteger(kCvarFx, 1) != 0;
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

void FxClear() {
    std::memset(sGhosts, 0, sizeof(sGhosts));
    std::memset(sGlows, 0, sizeof(sGlows));
    sDepartPending = false;
    sArrivePending = false;
    sFlashOn = false;
}

Ghost* NewGhost() {
    Ghost* slot = nullptr;
    const u32 now = gPlayState != nullptr ? gPlayState->gameplayFrames : 0;
    for (Ghost& g : sGhosts) {
        if (!g.used) {
            slot = &g;
            break;
        }
        if (slot == nullptr || (now - g.startFrame) > (now - slot->startFrame)) {
            slot = &g;
        }
    }
    std::memset(slot, 0, sizeof(*slot));
    slot->used = true;
    slot->id = sNextId++;
    return slot;
}

void NewGlow(const Vec3f& pos, bool arrival) {
    Glow* slot = nullptr;
    for (Glow& g : sGlows) {
        if (!g.used) {
            slot = &g;
            break;
        }
        if (slot == nullptr || g.startFrame < slot->startFrame) {
            slot = &g;
        }
    }
    std::memset(slot, 0, sizeof(*slot));
    slot->used = true;
    slot->arrival = arrival;
    slot->id = sNextId++;
    slot->startFrame = gPlayState->gameplayFrames;
    slot->life = arrival ? kGlowLifeIn : kGlowLifeOut;
    slot->pos = pos;
}

// Copies how Shadow stands right now (his pose is in the joint table) into a silhouette.
void CapturePose(Ghost* g, const Player* player) {
    g->child = IsChild();
    g->scale = player->actor.scale;
    g->yOffset = player->actor.shape.yOffset * player->actor.scale.y;
    g->rootDrop = player->unk_6C4;
    g->upperRot = player->upperLimbRot;
    g->headRot = player->headLimbRot;
    g->upperYawSecondary = player->upperLimbYawSecondary;
    std::memcpy(g->joints, player->skelAnime.jointTable, sizeof(g->joints));
}

// Makes the silhouettes that were asked for during the update. Runs at the end of the world's draw, so the joint table
// holds the pose that was just drawn (the dive pose at the vanish, the kick at the arrival).
void ResolvePending(Player* player) {
    const bool ghostsOn = FxEnabled() && CVarGetInteger(kCvarFxGhosts, 1) != 0;
    const bool haveSkeleton = ghostsOn && player->skelAnime.jointTable != nullptr && player->skelAnime.limbCount >= kJoints &&
                              GhostSkeleton(IsChild()) != nullptr;
    if (sDepartPending) {
        sDepartPending = false;
        if (haveSkeleton) {
            const u32 now = gPlayState->gameplayFrames;
            // The silhouette where he vanished, and fainter copies on the way to where he reappears.
            Ghost* first = NewGhost();
            CapturePose(first, player);
            first->pos = sDepart;
            first->rot = sDepartRot;
            first->rot.y = sFaceYaw; // he vanishes in the direction he is going
            first->scale = sDepartScale;
            first->startFrame = now;
            first->life = kGhostLifeDepart;
            first->scaleMul = 1.0f;
            first->opacity = 0.85f;
            const Ghost proto = *first;
            for (int i = 1; i <= 4; i++) {
                const float t = static_cast<float>(i) / 5.0f;
                Ghost* g = NewGhost();
                const uint32_t id = g->id;
                *g = proto;
                g->id = id;
                g->pos.x = sDepart.x + (sDest.x - sDepart.x) * t;
                g->pos.y = sDepart.y + (sDest.y - sDepart.y) * t;
                g->pos.z = sDepart.z + (sDest.z - sDepart.z) * t;
                g->life = kGhostLifeStreak;
                g->opacity = 0.5f - 0.07f * static_cast<float>(i);
            }
        }
    }
    if (sArrivePending) {
        sArrivePending = false;
        if (haveSkeleton) {
            // A cyan shell a little bigger than Shadow that fades into his own colours: he turns solid.
            Ghost* g = NewGhost();
            CapturePose(g, player);
            g->pos = player->actor.world.pos;
            g->rot = player->actor.shape.rot;
            g->startFrame = gPlayState->gameplayFrames;
            g->life = kGhostLifeShell;
            g->scaleMul = 1.07f;
            g->opacity = 0.85f;
        }
    }
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
    // Each silhouette is its own piece for the frame interpolation, so it is only ever blended with itself.
    ::FrameInterpolation_RecordOpenChild(reinterpret_cast<const void*>(static_cast<uintptr_t>(0x5AD10000u + g.id)), 0);
    HOMING_OPEN_DISPS(gfxCtx);

    Matrix_Push();
    Vec3s rot = g.rot;
    Matrix_SetTranslateRotateYXZ(g.pos.x, g.pos.y + g.yOffset * g.scaleMul + 1.0f, g.pos.z, &rot);
    Matrix_Scale(g.scale.x * g.scaleMul, g.scale.y * g.scaleMul, g.scale.z * g.scaleMul, MTXMODE_APPLY);

    // Flat cyan with the model's own shading, see-through, depth-tested but not written. (The model's meshes carry no
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
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 40, 95, 115, 255);
    gDPSetEnvColor(POLY_OPA_DISP++, 120, 240, 255, alpha);

    SkelAnime_DrawFlexOpa(play, static_cast<void**>(skeleton->sh.segment), const_cast<Vec3s*>(g.joints),
                          skeleton->dListCount, GhostOverrideLimbDraw, nullptr, const_cast<Ghost*>(&g));
    Matrix_Pop();

    HOMING_CLOSE_DISPS();
    ::FrameInterpolation_RecordCloseChild();
}

void DrawGhosts(PlayState* play) {
    for (Ghost& g : sGhosts) {
        if (!g.used) {
            continue;
        }
        const int age = static_cast<int>(play->gameplayFrames - g.startFrame);
        if (age >= g.life) {
            g.used = false;
            continue;
        }
        const FlexSkeletonHeader* skeleton = GhostSkeleton(g.child);
        if (skeleton == nullptr) {
            g.used = false;
            continue;
        }
        const float t = static_cast<float>(age) / static_cast<float>(g.life);
        const float alpha = g.opacity * std::pow(1.0f - t, 1.3f);
        const int a8 = static_cast<int>(std::lround(alpha * 255.0f));
        if (a8 >= 4) {
            DrawGhost(play, g, skeleton, static_cast<u8>(std::min(255, a8)));
        }
    }
}

// One ring of a round gradient: its radius (in 1/8 world units) and colour.
struct BandRow {
    float radius;
    u8 r, g, b, a;
};

// A band made of triangles with the colours of its rows blended between them (a soft edge). Facing +Z, centred on the
// origin. 4 segments make a diamond, 12 a round halo. Needs the matrix and the see-through vertex-colour state to be set.
void DrawBand(GraphicsContext* gfxCtx, const BandRow* rows, int rowCount, int segments) {
    HOMING_OPEN_DISPS(gfxCtx);
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
    HOMING_CLOSE_DISPS();
}

u8 ToByte(float v) {
    return static_cast<u8>(std::lround(std::max(0.0f, std::min(1.0f, v)) * 255.0f));
}

// Sets up the see-through vertex colour drawing the bands need (the same as the boost's aura).
void BeginLight(GraphicsContext* gfxCtx) {
    HOMING_OPEN_DISPS(gfxCtx);
    Gfx_SetupDL_4Xlu(gfxCtx);
    gDPSetCombineMode(POLY_XLU_DISP++, G_CC_SHADE, G_CC_SHADE);
    gSPClearGeometryMode(POLY_XLU_DISP++, G_LIGHTING | G_CULL_BOTH);
    HOMING_CLOSE_DISPS();
}

// The diamond of light, facing the camera: a tall bright diamond (white in the middle, cyan at the tips) with a round
// blue-to-purple halo behind it, the way the teleport glows in the reference.
void DrawGlow(PlayState* play, const Glow& glow, int age) {
    const bool child = IsChild();
    const float t = static_cast<float>(age) / static_cast<float>(glow.life);
    const float baseSize = (child ? 40.0f : 52.0f) * static_cast<float>(ClampInt(CVarGetInteger(kCvarFxGlowSize, 100), 10, 400)) / 100.0f;
    float size;
    float alpha;
    if (glow.arrival) { // closes on Shadow
        size = baseSize * (1.45f - 1.15f * t);
        alpha = 0.95f * std::pow(1.0f - t, 0.9f);
    } else { // opens where he vanished
        size = baseSize * (0.55f + 0.95f * std::sqrt(t));
        alpha = 1.0f * std::pow(1.0f - t, 1.1f);
    }
    if (alpha <= 0.02f) {
        return;
    }

    Vec3f centre = glow.pos;
    centre.y += child ? 26.0f : 38.0f;
    // A little towards the camera, so the light is not cut by the floor or by Shadow's own body.
    const Vec3f& eye = play->view.eye;
    const float vx = eye.x - centre.x;
    const float vy = eye.y - centre.y;
    const float vz = eye.z - centre.z;
    const float length = std::sqrt(vx * vx + vy * vy + vz * vz);
    if (length > 1.0f) {
        const float forward = child ? 10.0f : 14.0f;
        centre.x += vx / length * forward;
        centre.y += vy / length * forward;
        centre.z += vz / length * forward;
    }

    GraphicsContext* gfxCtx = play->state.gfxCtx;
    ::FrameInterpolation_RecordOpenChild(reinterpret_cast<const void*>(static_cast<uintptr_t>(0x5AD20000u + glow.id)), 0);
    HOMING_OPEN_DISPS(gfxCtx);
    BeginLight(gfxCtx);

    Matrix_Translate(centre.x, centre.y, centre.z, MTXMODE_NEW);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    Matrix_Scale(1.0f / 8.0f, 1.0f / 8.0f, 1.0f / 8.0f, MTXMODE_APPLY); // the vertices are in 1/8 world units

    const float r8 = size * 8.0f;

    // The round halo: blue in the middle, purple towards the edge, nothing at the rim.
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    const BandRow halo[3] = {
        { 0.0f, 110, 190, 255, ToByte(alpha * 0.55f) },
        { r8 * 0.9f, 150, 90, 255, ToByte(alpha * 0.28f) },
        { r8 * 1.7f, 170, 60, 255, 0 },
    };
    DrawBand(gfxCtx, halo, 3, 12);

    // The diamond: taller than wide, a bright core inside a cyan one.
    Matrix_Push();
    Matrix_Scale(0.62f, 1.0f, 1.0f, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    Matrix_Pop();
    const BandRow outer[2] = {
        { 0.0f, 200, 250, 255, ToByte(alpha * 0.95f) },
        { r8 * 1.15f, 70, 225, 255, ToByte(alpha * 0.25f) },
    };
    DrawBand(gfxCtx, outer, 2, 4);

    Matrix_Push();
    Matrix_Scale(0.5f, 1.0f, 1.0f, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    Matrix_Pop();
    const BandRow core[2] = {
        { 0.0f, 255, 255, 255, ToByte(alpha) },
        { r8 * 0.62f, 190, 245, 255, ToByte(alpha * 0.6f) },
    };
    DrawBand(gfxCtx, core, 2, 4);

    HOMING_CLOSE_DISPS();
    ::FrameInterpolation_RecordCloseChild();
}

void DrawGlows(PlayState* play) {
    if (CVarGetInteger(kCvarFxGlow, 1) == 0) {
        for (Glow& g : sGlows) {
            g.used = false;
        }
        return;
    }
    for (Glow& g : sGlows) {
        if (!g.used) {
            continue;
        }
        const int age = static_cast<int>(play->gameplayFrames - g.startFrame);
        if (age >= g.life) {
            g.used = false;
            continue;
        }
        DrawGlow(play, g, age);
    }
}

// The short blue-purple wash over the whole picture: one big diamond right in front of the camera, no depth test.
void DrawFlash(PlayState* play) {
    if (!sFlashOn) {
        return;
    }
    const int age = static_cast<int>(play->gameplayFrames - sFlashStart);
    const int strength = ClampInt(CVarGetInteger(kCvarFxFlash, 100), 0, 300);
    if (age >= kFlashLife || strength == 0) {
        sFlashOn = false;
        return;
    }
    const float t = static_cast<float>(age) / static_cast<float>(kFlashLife);
    const float alpha = 0.30f * std::pow(1.0f - t, 1.4f) * static_cast<float>(strength) / 100.0f;

    const Vec3f& eye = play->view.eye;
    const Vec3f& at = play->view.lookAt;
    float fx = at.x - eye.x;
    float fy = at.y - eye.y;
    float fz = at.z - eye.z;
    const float length = std::sqrt(fx * fx + fy * fy + fz * fz);
    if (length < 1.0f) {
        return;
    }
    const float kDistance = std::max(30.0f, play->view.zNear * 2.5f);
    fx = fx / length * kDistance;
    fy = fy / length * kDistance;
    fz = fz / length * kDistance;

    GraphicsContext* gfxCtx = play->state.gfxCtx;
    static const int sFlashTag = 0;
    ::FrameInterpolation_RecordOpenChild(&sFlashTag, 0);
    HOMING_OPEN_DISPS(gfxCtx);
    BeginLight(gfxCtx);
    gSPClearGeometryMode(POLY_XLU_DISP++, G_ZBUFFER); // nothing in the world hides it

    Matrix_Translate(eye.x + fx, eye.y + fy, eye.z + fz, MTXMODE_NEW);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    Matrix_Scale(1.0f / 8.0f, 1.0f / 8.0f, 1.0f / 8.0f, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    // A diamond big enough to cover the whole picture at that distance, whatever the field of view and the screen shape.
    const float viewW = static_cast<float>(std::max(1, play->view.viewport.rightX - play->view.viewport.leftX));
    const float viewH = static_cast<float>(std::max(1, play->view.viewport.bottomY - play->view.viewport.topY));
    const float halfH = kDistance * std::tan(std::max(10.0f, std::min(120.0f, play->view.fovy)) * 0.5f * 3.14159265f / 180.0f);
    const float cover = halfH * (1.0f + viewW / viewH) * 1.2f * 8.0f;
    const BandRow wash[2] = {
        { 0.0f, 90, 120, 255, ToByte(alpha) },
        { cover, 150, 70, 255, ToByte(alpha) },
    };
    DrawBand(gfxCtx, wash, 2, 4);
    HOMING_CLOSE_DISPS();
    ::FrameInterpolation_RecordCloseChild();
}

// Runs at the end of the world's draw, before the HUD.
void OnPlayDrawEndHoming() {
    if (gPlayState == nullptr) {
        return;
    }
    Player* player = GET_PLAYER(gPlayState);
    if (player == nullptr || gPlayState->pauseCtx.state != 0) {
        return;
    }
    if (!FxEnabled()) {
        FxClear();
        return;
    }
    ResolvePending(player);
    bool any = sFlashOn;
    for (const Ghost& g : sGhosts) {
        any = any || g.used;
    }
    for (const Glow& g : sGlows) {
        any = any || g.used;
    }
    if (!any) {
        return;
    }
    GraphicsContext* gfxCtx = gPlayState->state.gfxCtx;
    {
        // What we draw is in colour, not in the world's greyscale (the boost and Chaos Control turn that on).
        HOMING_OPEN_DISPS(gfxCtx);
        gSPGrayscale(POLY_OPA_DISP++, false);
        gSPGrayscale(POLY_XLU_DISP++, false);
        HOMING_CLOSE_DISPS();
    }
    DrawFlash(gPlayState);
    DrawGhosts(gPlayState);
    DrawGlows(gPlayState);
}

// ------------------------------------------------------------------------------------------------
// Starting, running and ending the attack
// ------------------------------------------------------------------------------------------------

void ShowShadow(Player* player) {
    if (sHidden) {
        player->stateFlags2 &= ~PLAYER_STATE2_DISABLE_DRAW;
        sHidden = false;
    }
}

// Back to nothing happening. With the player: Shadow is made visible again. Without (scene change, load, exit game): the
// player actor is not ours to touch then (it is gone, or not set up yet), and a new one starts visible anyway.
void Abort(Player* player) {
    if (player != nullptr) {
        ShowShadow(player);
    }
    sHidden = false;
    sPhase = Phase::Idle;
}

void Reset() {
    Abort(nullptr);
    FxClear();
    sHaveBusyFrame = false;
}

// A target is an enemy (or boss, or anything that shows as hostile) the game lets Navi hover over.
bool IsValidTarget(Player* player, Actor* actor) {
    if (actor == nullptr || actor == &player->actor || actor->update == nullptr) {
        return false;
    }
    if (!(actor->flags & ACTOR_FLAG_ATTENTION_ENABLED)) {
        return false;
    }
    return actor->category == ACTORCAT_ENEMY || actor->category == ACTORCAT_BOSS || (actor->flags & ACTOR_FLAG_HOSTILE) != 0;
}

// The actor the lock-on reticle is on, or the one Navi hovers over (that is what Z would lock onto).
Actor* FindTarget(PlayState* play, Player* player, bool* tooFar) {
    Actor* candidates[2] = { player->focusActor, play->actorCtx.targetCtx.arrowPointedActor };
    const float range = static_cast<float>(ClampInt(CVarGetInteger(kCvarRange, 800), 50, 5000));
    for (Actor* actor : candidates) {
        if (!IsValidTarget(player, actor)) {
            continue;
        }
        if (Math_Vec3f_DistXYZ(&player->actor.world.pos, &actor->world.pos) > range) {
            *tooFar = true;
            continue;
        }
        return actor;
    }
    return nullptr;
}

// Where Shadow reappears: in front of the enemy on the side he comes from, a little above the floor (so he is in the air,
// at the height of the enemy's body), stopping short of a wall that is in the way.
void ComputeDestination(PlayState* play, Player* player, Actor* target, Vec3f* out) {
    const Vec3f tp = target->world.pos;
    float dx = player->actor.world.pos.x - tp.x;
    float dz = player->actor.world.pos.z - tp.z;
    float len = std::sqrt(dx * dx + dz * dz);
    if (len < 1.0f) {
        dx = -Math_SinS(player->actor.shape.rot.y);
        dz = -Math_CosS(player->actor.shape.rot.y);
        len = 1.0f;
    }
    dx /= len;
    dz /= len;
    const float width = std::max(0.0f, std::min(70.0f, static_cast<float>(target->colChkInfo.cylRadius)));
    const float distance = static_cast<float>(ClampInt(CVarGetInteger(kCvarDistance, 36), 10, 300)) + width * 0.5f;

    Vec3f dest = { tp.x + dx * distance, tp.y, tp.z + dz * distance };
    CollisionPoly* poly = nullptr;
    s32 bgId = 0;
    Vec3f hit;
    Vec3f from = { tp.x, tp.y + 30.0f, tp.z };
    Vec3f to = { dest.x, tp.y + 30.0f, dest.z };
    if (BgCheck_EntityLineTest1(&play->colCtx, &from, &to, &hit, &poly, true, false, false, true, &bgId)) {
        dest.x = hit.x - dx * 8.0f; // a wall in between: stop in front of it
        dest.z = hit.z - dz * 8.0f;
    }

    Vec3f probe = { dest.x, tp.y + 40.0f, dest.z };
    float floorY = BgCheck_EntityRaycastFloor1(&play->colCtx, &poly, &probe);
    if (floorY <= BGCHECK_Y_MIN + 1.0f) { // nothing under that spot (a ledge, a pit): land on the enemy instead
        dest.x = tp.x;
        dest.z = tp.z;
        probe.x = tp.x;
        probe.z = tp.z;
        floorY = BgCheck_EntityRaycastFloor1(&play->colCtx, &poly, &probe);
    }
    float y = tp.y + 12.0f;
    if (floorY > BGCHECK_Y_MIN + 1.0f) {
        y = std::max(y, floorY + 18.0f);
    }
    dest.y = y;
    *out = dest;
}

void PlayGameSfx(Player* player, const char* cvar, int fallback) {
    if (CVarGetInteger(kCvarSound, 1) == 0) {
        return;
    }
    const int id = CVarGetInteger(cvar, fallback);
    if (id > 0 && id < 0xFFFF) {
        Player_PlaySfx(&player->actor, static_cast<u16>(id));
    }
}

// The moment Shadow appears in front of the enemy: visible again, the kick pose, the hit area, the bounce.
void Reveal(PlayState* play, Player* player) {
    ShowShadow(player);
    sPhase = Phase::Strike;
    sRevealFrame = play->gameplayFrames;
    sHit = false;
    sBounced = false;
    sRegistered = 0;

    const bool child = IsChild();
    const float bounce = static_cast<float>(ClampInt(CVarGetInteger(kCvarBounce, 100), 0, 500)) / 100.0f;
    const float back = static_cast<float>(ClampInt(CVarGetInteger(kCvarBounceBack, 100), 0, 500)) / 100.0f;
    player->actor.velocity.y = (child ? 6.5f : 7.0f) * bounce;
    player->linearVelocity = -2.0f * back;
    player->actor.speedXZ = player->linearVelocity;

    if (ShadowJump_StartHomingPose != nullptr) {
        ShadowJump_StartHomingPose(2);
    }
    NewGlow(sDest, true);
    sArrivePending = true;
    sFlashOn = true;
    sFlashStart = play->gameplayFrames;
    PlayGameSfx(player, kCvarSoundIn, 0);

    SetupCollider(play, player);
    RegisterStrike(play);
}

bool ShouldAbort(Player* player) {
    if (gPlayState->csCtx.state != CS_STATE_IDLE || Player_InCsMode(gPlayState)) {
        return true;
    }
    return (player->stateFlags1 & (PLAYER_STATE1_DEAD | PLAYER_STATE1_IN_CUTSCENE)) != 0;
}

// Once per game frame, at the end of the player's update.
void OnPlayerUpdateHoming() {
    if (gPlayState == nullptr || sPhase == Phase::Idle) {
        return;
    }
    Player* player = GET_PLAYER(gPlayState);
    if (player == nullptr) {
        return;
    }
    if (ShouldAbort(player)) {
        Abort(player);
        return;
    }
    const u32 frame = gPlayState->gameplayFrames;
    sLastBusyFrame = frame;
    sHaveBusyFrame = true;

    if (sPhase == Phase::Hidden) {
        if (frame == sStartFrame) {
            return; // still the frame of the press (this hook may run after Jump.cpp's)
        }
        // Held where he reappears: no falling, no drifting, nothing that could push him.
        player->actor.world.pos = sDest;
        player->actor.prevPos = sDest;
        player->actor.velocity.x = 0.0f;
        player->actor.velocity.y = 0.0f;
        player->actor.velocity.z = 0.0f;
        player->linearVelocity = 0.0f;
        player->actor.speedXZ = 0.0f;
        const int hide = ClampInt(CVarGetInteger(kCvarHide, 2), 0, 10);
        if (static_cast<int>(frame - sStartFrame) >= hide) {
            Reveal(gPlayState, player);
        }
        return;
    }

    // Phase::Strike. The hit area was registered in the previous frame(s); see what it did, then register it again until
    // it has hit something or the time is up.
    if (frame == sRevealFrame) {
        return; // the frame it was registered in; the game has not checked it yet
    }
    if (!sHit && (sCollider.base.atFlags & AT_HIT)) {
        sHit = true;
        if (sCollider.base.atFlags & AT_BOUNCED) { // it hit something hard: thrown back harder
            sBounced = true;
            player->linearVelocity = -5.0f;
            player->actor.speedXZ = player->linearVelocity;
            player->actor.velocity.y = std::max(player->actor.velocity.y, 5.0f);
        }
    }
    const int strikeFrames = ClampInt(CVarGetInteger(kCvarStrikeFrames, 3), 1, 10);
    if (!sHit && static_cast<int>(frame - sRevealFrame) < strikeFrames) {
        RegisterStrike(gPlayState);
    } else if (sHit || static_cast<int>(frame - sRevealFrame) >= strikeFrames) {
        sPhase = Phase::Idle;
    }
}

// Jump.cpp calls this when Y is pressed in the air. True when the press was used: the attack started, or one is already
// running (a second press must not turn into a double jump in the middle of it).
bool TryStart(Player* player) {
    if (!CVarGetInteger(kCvarEnabled, 1) || gPlayState == nullptr || player == nullptr) {
        return false;
    }
    if (sPhase != Phase::Idle) {
        return true;
    }
    bool tooFar = false;
    Actor* target = FindTarget(gPlayState, player, &tooFar);
    if (target == nullptr) {
        if (tooFar) {
            Say("Homing: the target is too far away");
        }
        return false;
    }

    Vec3f dest;
    ComputeDestination(gPlayState, player, target, &dest);

    sStartFrame = gPlayState->gameplayFrames;
    sLastBusyFrame = sStartFrame;
    sHaveBusyFrame = true;
    sDepart = player->actor.world.pos;
    sDepartRot = player->actor.shape.rot;
    sDepartScale = player->actor.scale;
    sDest = dest;
    sTargetPos = target->world.pos;
    sFaceYaw = Math_Vec3f_Yaw(&dest, &sTargetPos);
    sPhase = Phase::Hidden;
    sHit = false;
    sBounced = false;

    // Gone from here, standing there. He faces the enemy.
    player->actor.world.pos = dest;
    player->actor.prevPos = dest;
    player->actor.world.rot.y = sFaceYaw;
    player->actor.shape.rot.y = sFaceYaw;
    player->yaw = sFaceYaw;
    player->actor.velocity.x = 0.0f;
    player->actor.velocity.y = 0.0f;
    player->actor.velocity.z = 0.0f;
    player->linearVelocity = 0.0f;
    player->actor.speedXZ = 0.0f;

    if (CVarGetInteger(kCvarInvulnerable, 1) != 0 && player->invincibilityTimer == 0) {
        const int hide = ClampInt(CVarGetInteger(kCvarHide, 2), 0, 10);
        const int strike = ClampInt(CVarGetInteger(kCvarStrikeFrames, 3), 1, 10);
        player->invincibilityTimer = static_cast<s8>(-(hide + strike + 3)); // negative: cannot be hurt, and no flicker
    }

    if (ShadowJump_StartHomingPose != nullptr) {
        ShadowJump_StartHomingPose(1);
    }
    NewGlow(sDepart, false);
    sDepartPending = true;
    sFlashOn = true;
    sFlashStart = gPlayState->gameplayFrames;
    PlayGameSfx(player, kCvarSoundOut, NA_SE_EV_LINK_WARP_OUT);
    Say("Homing attack!");

    if (CVarGetInteger(kCvarHide, 2) <= 0) {
        Reveal(gPlayState, player);
    } else {
        player->stateFlags2 |= PLAYER_STATE2_DISABLE_DRAW;
        sHidden = true;
    }
    return true;
}

void RegisterHoming() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayerUpdate>(OnPlayerUpdateHoming);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayDrawEnd>(OnPlayDrawEndHoming);
    // None of these touch the player actor: on a scene change it is gone or not set up yet, and the new one starts visible.
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneInit>([](int16_t) { Reset(); });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnLoadGame>([](int32_t) {
        Reset();
        sGhostSkelState[0] = sGhostSkelState[1] = -1; // look for shadow_ghost.o2r again
    });
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnExitGame>([](int32_t) { Reset(); });
}

static RegisterShipInitFunc sHomingInit(RegisterHoming);

} // namespace

// Used by Jump.cpp: called when Y is pressed in the air. True = the press was used (an attack started or is running).
extern "C" bool ShadowHoming_TryStart(Player* player) {
    return TryStart(player);
}

// True from the press to the end of the strike.
extern "C" bool ShadowHoming_IsBusy(void) {
    return sPhase != Phase::Idle;
}

// Used by Boost.cpp (and Chaos Control): true from the press until 3 game frames after the end of the strike. The boost
// lets enemies update only every second frame, and the game throws away an enemy's damage in a frame in which it does not
// update, so a hit that lands on such a frame would do nothing. While this is true they run at normal speed.
extern "C" bool ShadowHoming_EnemiesAtFullSpeed(void) {
    if (sPhase != Phase::Idle) {
        return true;
    }
    return sHaveBusyFrame && gPlayState != nullptr && (gPlayState->gameplayFrames - sLastBusyFrame) <= 3;
}

// Read by the status report in Jump.cpp: tells it that this file is in the build, and which version it is.
// 1 = first version: teleport to the target under Navi, one master sword hit, bounce.
// 2 = works with the boost (enemies at full speed during the attack so the hit does damage; the boost leaves Shadow's speed
//     alone while the attack runs).
extern "C" {
int ShadowAbilityVersion_Homing = 2;
}
