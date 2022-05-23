//
//  game_data.c
//  bgdm
//
//  Created by Bhagwan on 7/9/17.
//  Copyright © 2017 Bhagwan Software. All rights reserved.
//

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <float.h> // FLT_MAX
#include <string>
#include <vector>
#include <cstdint>
#include <algorithm>
#include "core/TargetOS.h"
#include "core/utf.h"
#include "core/mem.h"
#include "core/utf.hpp"
#include "core/time_.h"
#include "core/segv.h"
#include "meter/dps.h"
#include "meter/game.h"
#include "meter/network.h"
#include "meter/process.h"
#include "meter/updater.h"
#include "meter/cache.hpp"
#include "meter/lru_cache.h"
#include "meter/localdb.h"
#include "meter/autolock.h"
#include "meter/imgui_bgdm.h"
#include "meter/ForeignClass.h"
#include "meter/game_text.h"
#include "meter/game_data.h"
#include "offsets/offset_scan.h"

#ifdef __APPLE__
#define __fastcall
#define __thiscall
#include "osx.bund/entry_osx.h"
#include "osx.bund/imgui_osx.h"
#include "osx.common/Logging.h"
#include "offsets/Memory_OSX.h"
#endif

#define GLM_FORCE_NO_CTOR_INIT
#include <glm/glm.hpp>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/matrix_transform.hpp>


static GameContext g_gameContext;

static bool ChCliCtx_read(uintptr_t ctx, struct __GameContext *pGameContext, int64_t now);
static bool AgentSelect_read(uintptr_t ctx, struct __GameContext *pGameContext, int64_t now);
static bool Agent_read(struct Character* c, struct Player *pl, int read, int64_t now);
static void Char_readBuffs(struct Character* c, struct BuffStacks *stacks);
static void Char_readCombat(Character* c, BuffStacks *buffs, CombatData *cd, int64_t now);
static uint32_t Player_HasEliteSpec(uintptr_t pptr);
static uint32_t Squad_GetSubgroup(uintptr_t squadptr, uint64_t uuid);
static void validateAndCalcTargets(int64_t now);
static void validateDpsTarget(Character* selectedTarget, int64_t now);
static void setupGroupDpsData(int64_t now);

#ifdef __APPLE__
static void except_handler(const char *exception)
{
    LogCrit("Unhandled exception in %s", exception);
}
#elif TARGET_OS_WIN
#pragma warning( push )
#pragma warning( disable: 4189 )
DWORD ExceptHandler(const char *msg, DWORD code, EXCEPTION_POINTERS *ep, const char *file, const char *func, int line) {
    EXCEPTION_RECORD *er = ep->ExceptionRecord;
    CONTEXT *ctx = ep->ContextRecord;
    LPCTSTR fmt_dbg = TEXT("%S: 0x%08X - addr: 0x%p");
    LPCTSTR fmt_rel = TEXT("%S");
#if (defined _DEBUG) || (defined DEBUG)
    DBGPRINT(fmt_dbg, msg, code, er->ExceptionAddress);
#else
    DBGPRINT(fmt_rel, msg);
#endif
    return EXCEPTION_EXECUTE_HANDLER;
}
#pragma warning( pop )
#endif


GameContext* currentContext()
{
    return &g_gameContext;
}

Player* controlledPlayer()
{
    return &g_gameContext.controlledPlayer;
}

Character *controlledCharacter()
{
    return &g_gameContext.controlledPlayer.c;
}

Character *currentTarget()
{
    return &g_gameContext.currentTarget;
}

uintptr_t controlledAgChar()
{
    if (controlledPlayer()->c.aptr == 0) return 0;
    
    __TRY
    
    return *(uintptr_t*)(controlledPlayer()->c.aptr + g_offsets.agentAgChar.val);
    
    __EXCEPT("[controlledAgChar] access violation")
    ;
    __FINALLY
    return 0;
}

bool game_init(void)
{
    if ( !g_gameContext.targets.create(MAX_TARGETS) )
        return false;
    
    if ( !g_gameContext.closestPlayers.create(SQUAD_SIZE_MAX) ) {
        g_gameContext.targets.destroy();
        return false;
    }
    
#if !(defined BGDM_TOS_COMPLIANT)
    gameSetHPBarsVisibleMode(-1);
    gameSetHPBarsHighlightMode(-1);
#endif
    
    return true;
}

void game_fini(void)
{
    g_gameContext.targets.destroy();
    g_gameContext.closestPlayers.destroy();
}

void game_update(int64_t now)
{
    if (bgdm_init_state() != InitStateInitialized)
    {
        return;
    }
    
    if (!g_state.global_on_off)
        return;
    
    uintptr_t chCliCtx = CharClientCtx();
    if (chCliCtx == 0) {
        CLogWarn("CharClientContext() returned 0");
        return;
    }
    
    if ( !ChCliCtx_read(chCliCtx, &g_gameContext, now) )
        return;
    
    // Can fail, we don't always have a target
    AgentSelect_read(g_offsets.viewAgentSelect.addr, &g_gameContext, now);
    
    // cleanup the target array
    validateAndCalcTargets(now);
    
    // setup our preferred DPS target
    validateDpsTarget(&g_gameContext.currentTarget, now);
    
    // copy dps numbers for self to DPSData
    setupGroupDpsData(now);
}


bool GetCamData(struct __CamData* pCamData)
{
    pCamData->valid = false;
    __TRY
    
    if (g_offsets.viewWorldView.addr && g_offsets.wvctxVtGetMetrics.val)
    {
        hl::ForeignClass wvctx = *(void**)g_offsets.viewWorldView.addr;
        if (wvctx && wvctx.get<int>(OFF_WVCTX_STATUS) == 1)
        {
            glm::vec3 camPos, lookAt, upVec, viewVec;
            wvctx.call<void>(g_offsets.wvctxVtGetMetrics.val, 1, &camPos, &lookAt, &upVec, &pCamData->fovy);
            viewVec = glm::normalize(lookAt - camPos);
            
            pCamData->camPos.x = camPos.x;
            pCamData->camPos.y = camPos.y;
            pCamData->camPos.z = camPos.z;
            pCamData->lookAt.x = lookAt.x;
            pCamData->lookAt.y = lookAt.y;
            pCamData->lookAt.z = lookAt.z;
            pCamData->upVec.x = upVec.x;
            pCamData->upVec.y = upVec.y;
            pCamData->upVec.z = upVec.z;
            pCamData->viewVec.x = viewVec.x;
            pCamData->viewVec.y = viewVec.y;
            pCamData->viewVec.z = viewVec.z;
            pCamData->curZoom = wvctx.get<float>(OFF_CAM_CUR_ZOOM);
            pCamData->minZoom  = wvctx.get<float>(OFF_CAM_MIN_ZOOM);
            pCamData->maxZoom  = wvctx.get<float>(OFF_CAM_MAX_ZOOM);
            pCamData->valid = true;
        }
    }
    
    __EXCEPT("[GetCamData] access violation")
    ;
    __FINALLY
    
    return false;
}

bool GetCompassData(CompassData* pCompData)
{
    bool bRet = false;
    if (!g_offsets.compassManager.addr)
        return bRet;
    
    __TRY
    
    // TODO: Crashes out of game main thread
    hl::ForeignClass comp = (void*)g_offsets.compassManager.addr;
    if (comp) {
        
        pCompData->pComp = (uintptr_t)comp.data();
        pCompData->width = comp.get<float>(OFF_COMP_WIDTH);
        pCompData->height = comp.get<float>(OFF_COMP_HEIGHT);
        pCompData->zoom = comp.get<int>(OFF_COMP_ZOOM);
        pCompData->maxWidth = comp.get<float>(OFF_COMP_MAX_WIDTH);
        pCompData->maxHeight = comp.get<float>(OFF_COMP_MAX_HEIGHT);
        
        uint32_t flags = comp.get<uint32_t>(OFF_COMP_FLAGS);
        pCompData->flags.rotation = !!(flags & GW2::COMP_ROTATION);
        pCompData->flags.position = !!(flags & GW2::COMP_POSITION);
        pCompData->flags.mouseOver = !!(flags & GW2::COMP_MOUSE_OVER);
    }
    
    __EXCEPT("[GetCompassData] access violation")
    ;
    __FINALLY
    
    return bRet;
}

float CalcDistance(vec3 p, vec3 q, bool isTransform)
{
    static const float meter2inch = 39.370078740157f;
    static const float idkwhatthisis = 0.8128f;
    float scale = isTransform ? (meter2inch * idkwhatthisis) : (1.0f);
    float sum = powf((q.x - p.x), 2) + powf((q.y - p.y), 2) + powf((q.z - p.z), 2);
    return sqrtf(sum) * scale;
}

bool readClosestPlayers(int64_t now)
{
    // Entry to delete if the cache is full
    // must be static to pass through the lambda
    static GameContext *gameContext;
    static float farthest;
    static struct CacheEntry<Player>* toDelete;
    bool bRet = false;
    
    gameContext = currentContext();
    gameContext->closestPlayers.lock();
    
    __TRY
    
    // Set the limit on our close player cache
    // remove excess players if we are resizing to smaller size
    while (g_state.max_players < gameContext->closestPlayers.size()) {
        
        farthest = 0.0f;
        toDelete = NULL;
        gameContext->closestPlayers.iter_nolock([](struct CacheEntry<Player>* entry)
        {
            if (entry->value.data.distance > farthest) {
                farthest = entry->value.data.distance;
                toDelete = entry;
            }
        });
        gameContext->closestPlayers.remove(toDelete);
    }
    gameContext->closestPlayers.resize(g_state.max_players);
    
    // Mark all existing players for deletion
    gameContext->closestPlayers.iter_nolock([](struct CacheEntry<Player>* entry) {
        entry->value.data.markedForDeletion = true;
    });
    
    if (gameContext->playerArray.isValid() && g_state.max_players > 0) {
        
        int count = std::min(gameContext->playerArray.count(), (uint32_t)MAX_PLAYERS);
        for (int i=0; i<count; ++i) {
            
            hl::ForeignClass player = (void*)gameContext->playerArray[i];
            if (player && controlledCharacter()->pptr != (uintptr_t)player.data()) {
                Player p = { 0 };
                p.c.pptr = (uintptr_t)player.data();
                p.c.cptr = player.call<uintptr_t>(g_offsets.playerVtGetChar.val);
                if (!p.c.cptr) continue;
                if (!Agent_read(&p.c, NULL, 0, now)) {
                    CLogWarn("Unable to read agent for player <pptr:%p> <cptr:%p> <aptr:%p>", p.c.pptr, p.c.cptr, p.c.aptr);
                    continue;
                }
                
                if (isCurrentMapPvPorWvW() && p.c.attitude != CHAR_ATTITUDE_FRIENDLY) {
                    continue;
                }
                if (g_state.hideNonSquad && p.c.subgroup == 0) {
                    continue;
                }
                if (g_state.hideNonParty && !p.c.is_party) {
                    continue;
                }
                
                p.distance = CalcDistance(gameContext->controlledPlayer.c.apos, p.c.apos, false);
                
                struct CacheEntry<Player>* entry = gameContext->closestPlayers.find(p.c.agent_id);
                if (!entry && gameContext->closestPlayers.limit()) {
                    
                    farthest = 0.0f;
                    toDelete = NULL;
                    gameContext->closestPlayers.iter_nolock([](struct CacheEntry<Player>* entry)
                    {
                        if (entry->value.data.distance > farthest) {
                            farthest = entry->value.data.distance;
                            toDelete = entry;
                        }
                    });
                    
                    if (p.distance >= farthest)
                        // current player is farther than our array max distance
                        // no need to input it into the array
                        continue;
                    
                    gameContext->closestPlayers.remove(toDelete);
                }
                Player *pp = entry ? &entry->value.data : &gameContext->closestPlayers.insert(p.c.agent_id, &p)->value.data;
                pp->markedForDeletion = false;
                if (entry)
                    // Copy the current agent data for existing players
                    pp->c = p.c;
                Char_readBuffs(&pp->c, &pp->buffs);
                Char_readCombat(&pp->c, &pp->buffs, &pp->cd, now);
            }
        }
    }
    
    // Delete all non-existing / too far players
    gameContext->closestPlayers.iter_nolock([](struct CacheEntry<Player>* entry) {
        if (entry->value.data.markedForDeletion)
            gameContext->closestPlayers.remove(entry);
    });
    
    bRet = true;
    
    __EXCEPT("[readClosestPlayers] access violation")
    ;
    __FINALLY
    
    gameContext->closestPlayers.unlock();
    return bRet;
}

#if !(defined BGDM_TOS_COMPLIANT)
void readPortalData(int64_t now)
{
    GameContext *pChCliCtx = currentContext();
    BuffStacks &stacks = pChCliCtx->controlledPlayer.buffs;
    PortalData &pd = pChCliCtx->portalData;
    if (stacks.port_weave && !pd.is_weave) {
        pd.is_weave = true;
        pd.time = now;
        pd.apos = pChCliCtx->controlledPlayer.c.apos;
        pd.tpos = pChCliCtx->controlledPlayer.c.tpos;
        pd.map_id = currentMapId();
        pd.shard_id = currentShardId();
#if defined(_WIN32) || defined (_WIN64)
        pd.mpos = g_mum_mem->avatar_pos;
#endif
    }
    else if (!stacks.port_weave) {
        pd.is_weave = false;
    }
}
#endif

bool ChCliCtx_read(uintptr_t ctx, GameContext *pChCliCtx, int64_t now)
{
    bool bRet = false;
    if (!ctx || !pChCliCtx)
        return false;
    
    __TRY
    
    hl::ForeignClass chCliCtx = (void*)ctx;
    if (!chCliCtx)
        return false;
    
    pChCliCtx->wmAgentArray = *(ANet::Collection<uintptr_t>*)(WmAgentArray() + sizeof(uintptr_t));
    pChCliCtx->playerArray = chCliCtx.get<ANet::Collection<uintptr_t>>(g_offsets.charCtxPlayArray.val);
    pChCliCtx->characterArray = chCliCtx.get<ANet::Collection<uintptr_t>>(g_offsets.charCtxCharArray.val);
    
    memset(&pChCliCtx->controlledPlayer.c, 0, sizeof(pChCliCtx->controlledPlayer.c));
    pChCliCtx->controlledPlayer.c.cptr = chCliCtx.get<uintptr_t>(g_offsets.charCtxCtlChar.val);
    pChCliCtx->controlledPlayer.c.pptr = chCliCtx.get<uintptr_t>(g_offsets.charCtxCtlPlayer.val);
    
    if (pChCliCtx->controlledPlayer.c.cptr == 0 ||
        !pChCliCtx->playerArray.isValid() ||
        !pChCliCtx->characterArray.isValid() ||
        !Agent_read(&pChCliCtx->controlledPlayer.c, &pChCliCtx->controlledPlayer, 0, now) ) {
        
        // Invalid ptrs (login screen)?
        return false;
    }
    
    __EXCEPT("[ChCliCtx_Read] access violation")
    ;
    __FINALLY
    
    bRet = readClosestPlayers(now);
    
#if !(defined BGDM_TOS_COMPLIANT)
    readPortalData(now);
#endif
    
    return bRet;
}

void TransportPosToAgentPos(vec3 tpos, vec3* apos)
{
    static const float inch2meter = 0.0254f;
    static const float idkwhatthisis = 0.8128f;
    
    apos->x = (tpos.x * idkwhatthisis) / inch2meter;
    apos->y = (tpos.y * idkwhatthisis) / inch2meter;
    apos->z = - ((tpos.z * idkwhatthisis) / inch2meter);
    
}

bool TransportPosProject2D(vec3 tpos, vec2 *outPos)
{
    vec3 apos = { 0 };
    TransportPosToAgentPos(tpos, &apos);
    return AgentPosProject2D(apos, outPos);
}

bool AgentPosProject2D(vec3 apos, vec2 *outPos)
{
    
    CamData* pCam = currentCam();
    if (!pCam || !pCam->valid)
        return false;
    
    int display_w, display_h;
    ImGui_GetDisplaySize(&display_w, &display_h);
    
    glm::vec4 viewport(0.0f, 0.0f, display_w, display_h);
    glm::vec3 worldPt(apos.x, apos.y, apos.z);
    glm::vec3 camPos(pCam->camPos.x, pCam->camPos.y, pCam->camPos.z);
    glm::vec3 viewVec(pCam->viewVec.x, pCam->viewVec.y, pCam->viewVec.z);
    glm::vec3 upVec(pCam->upVec.x, pCam->upVec.y, pCam->upVec.z);
    
    glm::vec3 lookAt = camPos + viewVec;
    glm::mat4 model = glm::lookAtLH(camPos, lookAt, upVec);
    glm::mat4 projection = glm::perspectiveLH(pCam->fovy, (float)display_w / display_h, 1.0f, 10000.0f);
    
    //glm::mat4 model = glm::translate(glm::mat4(1.0f), viewVec);
    //glm::mat4 projection = glm::frustum(-1.0f, 1.0f, -1.0f, 1.0f, 1.0f, 100.0f);
    glm::vec3 projected = glm::project(worldPt, model, projection, viewport);
    
    outPos->x  = projected.x;
    outPos->y  = display_h-projected.y;
    
    if (outPos->x > 0 && outPos->x < display_w &&
        outPos->y > 0 && outPos->y < display_h)
        return true;
    
    return false;
}

static void Char_readBuffs(struct Character* c, struct BuffStacks *stacks)
{
    
    if (!c || !stacks)
        return;
    
    memset(stacks, 0, sizeof(*stacks));
    
    hl::ForeignClass combatant = (void*)(c->cbptr);
    if (!combatant)
        return;
    
    hl::ForeignClass buffBar = combatant.call<void*>(g_offsets.combatantVtGetBuffbar.val);
    if (!buffBar)
        return;
    
    auto buffs = buffBar.get<ANet::Collection<BuffEntry, false>>(OFF_BUFFBAR_BUFF_ARR);
    assert(buffs.capacity() < 4000);
    if (!buffs.isValid() || buffs.capacity() > 4000)
        return;
    
    for (uint32_t i = 0; i < buffs.capacity(); i++) {
        BuffEntry be = buffs[i];
        hl::ForeignClass pBuff = be.pBuff;
        if (pBuff) {
            i32 efType = pBuff.get<i32>(OFF_BUFF_EF_TYPE);
            switch (efType)
            {
                case(GW2::EFFECT_VIGOR):
                    ++stacks->vigor;
                    break;
                case(GW2::EFFECT_SWIFTNESS):
                    ++stacks->swift;
                    break;
                case(GW2::EFFECT_STABILITY):
                    ++stacks->stab;
                    break;
                case(GW2::EFFECT_RETALIATION):
                    ++stacks->retal;
                    break;
                case(GW2::EFFECT_RESISTANCE):
                    ++stacks->resist;
                    break;
                case(GW2::EFFECT_REGENERATION):
                    ++stacks->regen;
                    break;
                case(GW2::EFFECT_QUICKNESS):
                    ++stacks->quick;
                    break;
                case(GW2::EFFECT_ALACRITY):
                    ++stacks->alacrity;
                    break;
                case(GW2::EFFECT_PROTECTION):
                    ++stacks->prot;
                    break;
                case(GW2::EFFECT_FURY):
                    ++stacks->fury;
                    break;
                case(GW2::EFFECT_AEGIS):
                    ++stacks->aegis;
                    break;
                case(GW2::EFFECT_MIGHT):
                    ++stacks->might;
                    break;
                case(GW2::EFFECT_VAMPIRIC_AURA):
                    ++stacks->necVamp;
                    break;
                case(GW2::EFFECT_EMPOWER_ALLIES):
                    ++stacks->warEA;
                    break;
                case(GW2::EFFECT_BANNER_OF_TACTICS):
                    ++stacks->warTact;
                    break;
                case(GW2::EFFECT_BANNER_OF_POWER):
                    ++stacks->warStr;
                    break;
                case(GW2::EFFECT_BANNER_OF_DISCIPLINE):
                    ++stacks->warDisc;
                    break;
                case(GW2::EFFECT_NATURALISTIC_RESONANCE):
                    ++stacks->revNR;
                    break;
                case(GW2::EFFECT_ASSASSINS_PRESENCE):
                    ++stacks->revAP;
                    break;
                case(GW2::EFFECT_SUN_SPIRIT):
                    ++stacks->sun;
                    break;
                case(GW2::EFFECT_FROST_SPIRIT):
                    ++stacks->frost;
                    break;
                case(GW2::EFFECT_STORM_SPIRIT):
                    ++stacks->storm;
                    break;
                case(GW2::EFFECT_STONE_SPIRIT):
                    ++stacks->stone;
                    break;
                case(GW2::EFFECT_SPOTTER):
                    ++stacks->spotter;
                    break;
                case(GW2::EFFECT_GRACE_OF_THE_LAND):
                case(GW2::EFFECT_GRACE_OF_THE_LAND_CN):
                    ++stacks->GOTL;
                    break;
                case(GW2::EFFECT_GLYPH_OF_EMPOWERMENT):
                case(GW2::EFFECT_GLYPH_OF_EMPOWERMENT_CN):
                    ++stacks->empGlyph;
                    break;
                case(GW2::EFFECT_RITE_OF_THE_GREAT_DWARF):
                    ++stacks->revRite;
                    break;
                case(GW2::EFFECT_SOOTHING_MIST):
                    ++stacks->sooMist;
                    break;
                case(GW2::EFFECT_STRENGTH_IN_NUMBERS):
                    ++stacks->strInNum;
                    break;
                case(GW2::EFFECT_PINPOINT_DISTRIBUTION):
                    ++stacks->engPPD;
                    break;
                case(GW2::EFFECT_PORTAL_WEAVING):
                case(GW2::EFFECT_PORTAL_WEAVING2):
                    ++stacks->port_weave;
            }
        }
    }
    
    // Fix overstacking for might/gotl
    if (stacks->might > 25) stacks->might = 25;
    if (stacks->GOTL > 5) stacks->GOTL = 5;
}

// Read & calculate combat data
static void Char_readCombat(Character* c, BuffStacks *buffs, CombatData *cd, int64_t now)
{
    if (!c || !cd || !buffs)
        return;
    
    ENTER_SPIN_LOCK(cd->lock);
    
    // Combat entry / exit times
    bool was_in_combat = cd->lastKnownCombatState;
    if (!was_in_combat && c->in_combat)
    {
        bool combat_reenter = false;
        if (g_state.ooc_grace_period > 0) {
            
            // Adjsut to seconds
            int ooc_grace_period = g_state.ooc_grace_period * 1000000;
            if (now - cd->end <= ooc_grace_period)
                combat_reenter = true;
        }
        
        if (!combat_reenter) {
            // Combat enter
            if (cd->update) { CLogTrace("[%s] COMBAT ENTER", std_utf16_to_utf8((utf16str)c->name).c_str()); }
            memset(cd, 0, sizeof(CombatData));
            cd->begin = now;
        }
        else {
            if (cd->update) { CLogTrace("[%s] COMBAT REENTER", std_utf16_to_utf8((utf16str)c->name).c_str()); }
        }
    }
    else if (was_in_combat && !c->in_combat)
    {
        // Combat exit
        cd->end = now;
        cd->duration = (cd->end - cd->begin) / 1000.0f;
        
        CLogTrace("[%s] COMBAT EXIT  duration:%.2f", std_utf16_to_utf8((utf16str)c->name).c_str(), (cd->duration / 1000.0f));
    }
    
    // Calc total combat time
    if (c->in_combat)
    {
        cd->duration = (now - cd->begin) / 1000.0f;
    }
    
    // Calc no. of downs
    bool was_downed = cd->lastKnownDownState;
    cd->lastKnownDownState = c->is_downed;
    if (!was_downed && cd->lastKnownDownState) {
        cd->noDowned++;
    }
    
    
    // Calc buff combat uptime while
    // in combat & on combat exit
    if ( (c->in_combat || was_in_combat) &&
        cd->update &&
        c->cbptr )
    {
        f32 tick = (now - cd->update) / 1000.0f;
        
        // Scholar buff uptime
        if (c->hp_val > 0 &&
            c->hp_max > 0 &&
            c->hp_max >= c->hp_val) {
            u32 hpper = (u32)ceilf(c->hp_val / (f32)c->hp_max * 100.0f);
            if (hpper >= 90)
                cd->sclr_dura += tick;
        }
        
#define SWD_UPDATE_FREQ  500000
        
        if (cd->update_pos == 0)
            cd->update_pos = -SWD_UPDATE_FREQ;
        if (now - cd->update_pos >= SWD_UPDATE_FREQ)
        {
            f32 swd_tick = (now - cd->update_pos) / 1000.0f;
            cd->update_pos = now;
            
            // Seaweed salad buff uptime
            vec3 pos = c->apos;
            if (cd->apos.x == 0 &&
                cd->apos.y == 0 &&
                cd->apos.z == 0) {
                cd->seaw_dura += cd->duration;
                //if (c->pptr == g_player.c.pptr)
                //	DBGPRINT(TEXT("LAST POS INIT  %.2f %.2f %.2f"), pos.x, pos.y, pos.z);
            }
            else if (
                     cd->apos.x != pos.x ||
                     cd->apos.y != pos.y ||
                     cd->apos.z != pos.z) {
                cd->seaw_dura += swd_tick;
                //if (c->pptr == g_player.c.pptr)
                //	DBGPRINT(TEXT("MOVE  dura %.2f total %.2f"), cd->seaw_dura, cd->duration);
            }
            else {
                //if (c->pptr == g_player.c.pptr)
                //	DBGPRINT(TEXT("STAND dura %.2f total %.2f "), cd->seaw_dura, cd->duration);
            }
            memcpy(&cd->apos, &c->apos, sizeof(cd->apos));
            memcpy(&cd->tpos, &c->tpos, sizeof(cd->tpos));
            cd->duration500 = cd->duration;
        }
            
        BuffStacks &stacks = *buffs;
        if (stacks.vigor)
            cd->vigor_dura += tick;
        if (stacks.swift)
            cd->swift_dura += tick;
        if (stacks.stab)
            cd->stab_dura += tick;
        if (stacks.retal)
            cd->retal_dura += tick;
        if (stacks.resist)
            cd->resist_dura += tick;
        if (stacks.regen)
            cd->regen_dura += tick;
        if (stacks.aegis)
            cd->aegis_dura += tick;
        if (stacks.necVamp)
            cd->necVA_dura += tick;
        if (stacks.warEA)
            cd->warEA_dura += tick;
        if (stacks.revNR)
            cd->revNR_dura += tick;
        if (stacks.revAP)
            cd->revAP_dura += tick;
        if (stacks.sun)
            cd->sun_dura += tick;
        if (stacks.frost)
            cd->frost_dura += tick;
        if (stacks.storm)
            cd->storm_dura += tick;
        if (stacks.stone)
            cd->stone_dura += tick;
        if (stacks.spotter)
            cd->spot_dura += tick;
        
        if (stacks.quick)
            cd->quik_dura += tick;
        if (stacks.alacrity)
            cd->alac_dura += tick;
        if (stacks.prot)
            cd->prot_dura += tick;
        if (stacks.fury)
            cd->fury_dura += tick;
        if (stacks.warStr)
            cd->bans_dura += tick;
        if (stacks.warDisc)
            cd->band_dura += tick;
        if (stacks.warTact)
            cd->bant_dura += tick;
        if (stacks.empGlyph)
            cd->glem_dura += tick;
        
        if (stacks.revRite)
            cd->revRD_dura += tick;
        if (stacks.sooMist)
            cd->eleSM_dura += tick;
        if (stacks.strInNum)
            cd->grdSIN_dura += tick;
        if (stacks.engPPD)
            cd->engPPD_dura += tick;
        
        cd->might_avg = (cd->might_dura * cd->might_avg + tick * stacks.might) / (cd->might_dura + tick);
        if (stacks.might)
            cd->might_dura += tick;
        if (cd->might_avg > 25)
            cd->might_avg = 25.0f;
        
        cd->gotl_avg = (cd->gotl_dura * cd->gotl_avg + tick * stacks.GOTL) / (cd->gotl_dura + tick);
        if (stacks.GOTL)
            cd->gotl_dura += tick;
        if (cd->gotl_avg > 5)
            cd->gotl_avg = 5.0f;
    }
    
    // Save last combat state
    cd->update = now;
    cd->lastKnownCombatState = c->in_combat;
    
    EXIT_SPIN_LOCK(cd->lock);
}

static uintptr_t wmAgentFromAgentId(uint32_t agent_id)
{
    uintptr_t wmptr = 0;
    if ( agent_id == 0 ) return 0;
    if ( !currentContext()->wmAgentArray.isValid() ) return 0;
    
    __TRY
    
    wmptr = (agent_id < currentContext()->wmAgentArray.size()) ? currentContext()->wmAgentArray[agent_id] : 0;
    
    __EXCEPT("[wmAgentFromAgentId] access violation")
    ;
    __FINALLY
    
    return wmptr;
}

uintptr_t agentFromAgentId(uint32_t agent_id)
{
    uintptr_t aptr = 0;
    if ( agent_id == 0 ) return 0;
    if ( !currentContext()->wmAgentArray.isValid() ) return 0;
    
    __TRY
    
    hl::ForeignClass wmAgent = (agent_id < currentContext()->wmAgentArray.size()) ? (void*)currentContext()->wmAgentArray[agent_id] : 0;
    if (!wmAgent) {
        LogWarn("Unable to acquire wmAgent for agent id %d", agent_id);
        return 0;
    }
    
    // BUG: not implemeted on Mac, this VT just returns 0
    /*
     ; ================ B E G I N N I N G   O F   P R O C E D U R E ================
     
     
     wmAgent->GetAgent():
     0000000000449550 55                     push       rbp
     0000000000449551 4889E5                 mov        rbp, rsp
     0000000000449554 31C0                   xor        eax, eax
     0000000000449556 5D                     pop        rbp
     0000000000449557 C3                     ret
     ; endp
     0000000000449558 0F1F840000000000       nop        dword [rax+rax]
     
     
     ; ================ B E G I N N I N G   O F   P R O C E D U R E ================
     
     
     sub_449560:
     0000000000449560 55                     push       rbp                          ; CODE XREF=sub_4369b0+183, sub_43c280+69, sub_454ac0+823, sub_4550c0+8, sub_455ca0+467, sub_455ec0+16, sub_45ceb0+51, sub_45cf60+4, sub_45cf80+20, sub_46ec90+467, sub_4aa350+434, …
     0000000000449561 4889E5                 mov        rbp, rsp
     0000000000449564 53                     push       rbx
     0000000000449565 50                     push       rax
     0000000000449566 4889FB                 mov        rbx, rdi
     0000000000449569 E8B2D10600             call       GetAvAgentArray()
     000000000044956e 4889C7                 mov        rdi, rax                     ; argument #1 for method sub_4d9a30
     0000000000449571 4889DE                 mov        rsi, rbx                     ; argument #2 for method sub_4d9a30
     0000000000449574 4883C408               add        rsp, 0x8
     0000000000449578 5B                     pop        rbx
     0000000000449579 5D                     pop        rbp
     000000000044957a E9B1040900             jmp        sub_4d9a30
     ; endp
     000000000044957f                        db         0x90
     */
    hl::ForeignClass agent = wmAgent.call<void*>(g_offsets.wmAgVtGetAgent.val);
    LogDebug("<wmptr:%p> <aptr:%p>", wmAgent.data(), agent.data());
    if (!agent) return 0;
    
    uint32_t verify_id = agent.call<uint32_t>(g_offsets.agentVtGetId.val);
    assert(verify_id == agent_id);
    if (verify_id != agent_id) {
        LogCrit("Internal error: agent_id != verify_id <agent:%p>", agent_id, verify_id, agent.data());
        return 0;
    }
    else {
        aptr = (uintptr_t)agent.data();
    }
    
    __EXCEPT("[agentFromAgentId] access violation")
    ;
    __FINALLY
    
    return aptr;
}

static uintptr_t characterFromAgent(uintptr_t aptr, uint32_t agent_id)
{
    uintptr_t cptr = 0;
    if (aptr == 0) return 0;
    
    __TRY
    
    if (agent_id == 0) {
        
        hl::ForeignClass agent = (void*)aptr;
        agent_id = agent.call<uint32_t>(g_offsets.agentVtGetId.val);
        if (agent_id == 0) {
            LogCrit("Unable to get agent_id for <aptr:%p>", aptr);
            return 0;
        }
    }
    
    if (agent_id == 0) return 0;
    
    if (agent_id < currentContext()->characterArray.count()) {

        hl::ForeignClass character = (void*)currentContext()->characterArray[agent_id];;
        if (character) {
            
            // Verify the character matches our agent
            hl::ForeignClass agent = character.call<void*>(g_offsets.charVtGetAgent.val);
            assert((uintptr_t)agent.data() == aptr);
            if ((uintptr_t)agent.data() != aptr) {
                LogCrit("Internal error: agent != c->aptr <agent:%p> <c->aptr:%p> <c->cptr:%p>", agent.data(), aptr, cptr);
            }
            else {
                cptr = (uintptr_t)character.data();
            }
        }
        else {
            LogWarn("Unable to acquire cptr for agent id: %d <aptr:%p>", agent_id, aptr);
        }
        
    }
    
    __EXCEPT("[characterFromAgent] access violation")
    ;
    __FINALLY
    
    return cptr;
}

static bool Character_read(struct Character *c, struct Player *pl, int doNotCopyName, int64_t now)
{
    if (!c || c->aptr == 0)
    {
        return false;
    }
    
    if (c->cptr == 0)
        // Try to resolve from our char array
        c->cptr = characterFromAgent(c->aptr, c->agent_id);
    
    hl::ForeignClass character = (void*)c->cptr;
    if (!character)
        return false;
    
    c->attitude = character.get<uint32_t>(g_offsets.charAttitude.val);
    c->inventory = character.call<uintptr_t>(g_offsets.charVtGetInventory.val);
    
    c->bptr = character.get<uintptr_t>(g_offsets.charBreakbar.val);
    
    hl::ForeignClass bb = (void*)c->bptr;
    if (bb) {
        c->bb_state = bb.get<int32_t>(OFF_BREAKBAR_STATE);
        c->bb_value = (uint32_t)(bb.get<float>(OFF_BREAKBAR_VALUE) * 100.0f);
        c->bb_value = std::min((int)c->bb_value, 100);
    }
    
    // Get the combatant (call the combatant VT)
    // Is the character in combat
    c->in_combat = character.get<int32_t>(g_offsets.charFlags.val) & GW2::CHAR_FLAG_IN_COMBAT ? 1 : 0;
    c->cbptr = character.call<uintptr_t>(g_offsets.charVtGetCombatant.val);
    
    c->is_clone = character.call<uintptr_t>(g_offsets.charVtIsClone.val);
    c->is_alive = character.call<uintptr_t>(g_offsets.charVtIsAlive.val);
    c->is_downed = character.call<uintptr_t>(g_offsets.charVtIsDowned.val);
    c->is_player = character.call<uintptr_t>(g_offsets.charVtIsPlayer.val);
    
    if (c->is_player ) {
        
        c->player_id = character.call<uint32_t>(g_offsets.charVtGetPlayerId.val);
        if (isInMainThread() && !c->pptr)
            c->pptr = character.call<uintptr_t>(g_offsets.charVtGetPlayer.val);
        
        // Get the contact def
        hl::ForeignClass contactCtx = (void*)ContactCtx();
        if (contactCtx) {
            hl::ForeignClass contact = contactCtx.call<void*>(g_offsets.contactCtxGetContact.val, (void*)c->pptr);
            if (contact) {
                
                c->ctptr = (uintptr_t)contact.data();
                c->uuid = contact.get<uint64_t>(OFF_CONTACT_UUID);
                c->is_party = contact.get<uint32_t>(OFF_CONTACT_ISPARTYSQUAD);
                c->subgroup = Squad_GetSubgroup(SquadCtx(), c->uuid);
            }
        }
        
        hl::ForeignClass coreStats = character.get<void*>(g_offsets.charCoreStats.val);
        if (coreStats) {
            c->race = coreStats.get<int8_t>(OFF_STATS_RACE);
            c->gender = coreStats.get<int8_t>(OFF_STATS_GENDER);
            c->profession = coreStats.get<int32_t>(OFF_STATS_PROFESSION);
        }
        
        hl::ForeignClass profession = character.get<void*>(g_offsets.charProfession.val);
        if (profession) {
            c->stance = profession.get<int32_t>(OFF_PROFESSION_STANCE);
            c->energy_cur = profession.get<float>(OFF_PROFESSION_ENERGY_CUR);
            c->energy_max = profession.get<float>(OFF_PROFESSION_ENERGY_MAX);
        }
        
        c->is_elite = Player_HasEliteSpec(c->pptr);
        
        // if we have pptr, get the target name from the player class
        hl::ForeignClass player = (void*)c->pptr;
        if (player && !doNotCopyName) {
            
            utf16string name;
            const utf16str str = player.get<const utf16str>(g_offsets.playerName.val);
            if (str) name = str;
            if (name.length() > 0)
                memcpy(c->name, str, name.length() * sizeof(utf16chr));
        }
        
        if (pl) {
            Char_readBuffs(c, &pl->buffs);
            Char_readCombat(c, &pl->buffs, &pl->cd, now);
        }
    }
    
    // Get the speciesDef
    if (isInMainThread()) {
#ifdef __APPLE__
        character.call_static<void>(g_offsets.charVtGetSpeciesDef.val, &c->spdef, c->cptr);
#else
        character.call<void>(g_offsets.charVtGetSpeciesDef.val, &c->spdef);
#endif
    }
    
    return true;
}

static bool Agent_read(struct Character* c, struct Player *pl, int doNotCopyName, int64_t now)
{
    bool bRet = false;
    if (!c) return false;
    
    
    __TRY
    
    // If we don't have the agent ptr
    // we can get it from the char ptr
    if (!c->aptr && c->cptr) {
        hl::ForeignClass character = (void*)c->cptr;
        c->aptr = character.call<uintptr_t>(g_offsets.charVtGetAgent.val);
        if (!c->aptr) {
            LogWarn("Unable to acquire aptr for char <cptr:%p>", c->cptr);
            return false;
        }
    }
    
    hl::ForeignClass gdctx = (void*)GadgetCtx();
    hl::ForeignClass agent = (void*)c->aptr;
    if (!agent)
        return false;
    
    c->agent_id = agent.call<uint32_t>(g_offsets.agentVtGetId.val);
    if (c->agent_id == 0) {
        LogWarn("Unable to get agent_id for <aptr:%p>", c->aptr);
        return false;
    }
    
    c->type = agent.call<uint32_t>(g_offsets.agentVtGetType.val);
    
    agent.call_static<void>(g_offsets.agentVtGetPos.val, &c->apos, c->aptr);
    c->tptr = agent.get<uintptr_t>(g_offsets.agentTransform.val);
    hl::ForeignClass transform = (void*)c->tptr;
    if (transform) {
        c->tpos = transform.get<vec3>(OFF_TRANSFORM_POS);
        if (c->apos.x == 0 && c->apos.y ==0 && c->apos.z == 0)
            TransportPosToAgentPos(c->tpos, &c->apos);
    }
    
    if (c->type == AGENT_TYPE_CHAR) {
        
        if (c->cptr == 0)
            // Try to resolve from our char array
            c->cptr = characterFromAgent(c->aptr, c->agent_id);
        
        hl::ForeignClass character = (void*)c->cptr;
        if (character)
            c->hptr = character.get<uintptr_t>(g_offsets.charHealth.val);
    }
    else if (c->type == AGENT_TYPE_GADGET)
    {
        hl::ForeignClass pGadget = gdctx.call<void*>(g_offsets.gadgetCtxVtGetGadget.val, c->agent_id);
        if (pGadget) {
            c->attitude = pGadget.call<uint32_t>(g_offsets.gadgetVtGetAttitude.val);
            c->hptr = pGadget.get<uintptr_t>(g_offsets.gadgetHealth.val);
        }
        c->gdptr = (uintptr_t)pGadget.data();
    }
    else if (c->type == AGENT_TYPE_ATTACK)
    {
        hl::ForeignClass pGadget = NULL;
        hl::ForeignClass pAttackTgt = gdctx.call<void*>(g_offsets.gadgetCtxVtGetAttackTgt.val, c->agent_id);
        if (pAttackTgt) {
            pGadget = pAttackTgt.get<void*>(g_offsets.atkTgtGadget.val);
            if (pGadget) {
                c->attitude = pGadget.call<uint32_t>(g_offsets.gadgetVtGetAttitude.val);
                c->hptr = pGadget.get<uintptr_t>(g_offsets.gadgetHealth.val);
            }
        }
        c->gdptr = (uintptr_t)pGadget.data();
        c->atptr = (uintptr_t)pAttackTgt.data();
    }
    
    hl::ForeignClass health = (void*)c->hptr;
    if (health) {
#ifdef __APPLE__
        c->hp_val = (uint32_t)health.get<float>(OFF_HP_VAL);
        c->hp_max = (uint32_t)health.get<float>(OFF_HP_MAX);
#else
        c->hp_val = health.get<uint32_t>(OFF_HP_VAL);
        c->hp_max = health.get<uint32_t>(OFF_HP_MAX);
#endif
    }
    
    // Read the character data
    if (c->type == AGENT_TYPE_CHAR) {
        
        Character_read(c, pl, doNotCopyName, now);
    }
    
    c->wmptr = ((uintptr_t(*)(uintptr_t))g_offsets.fncGetWmAgent.addr)(c->aptr);
    
    // If we don't have the name yet
    // get it from the wmAgent ptr
    if (c->name[0] == 0) {
        c->decodedName = lru_find(GW2::CLASS_TYPE_AGENT, c->aptr, c->name, ARRAYSIZE(c->name));
    }
    
    
    bRet = true;
    
    __EXCEPT("[Agent_read] access violation")
    ;
    __FINALLY
    
    return bRet;
}

static bool Agent_readTarget(struct DPSTarget* t, int64_t now)
{
    bool bRet = false;
    if (!t || t->aptr == 0) return false;
    
    __TRY
    
    hl::ForeignClass gdctx = (void*)GadgetCtx();
    hl::ForeignClass agent = (void*)t->aptr;
    if (!agent)
        return false;
    
    if (t->agent_id == 0)
        t->agent_id = agent.call<uint32_t>(g_offsets.agentVtGetId.val);
    
    if (t->agent_id == 0) {
        LogWarn("Unable to get agent_id for target <aptr:%p>", t->aptr);
        return false;
    }
    
    uintptr_t hptr = 0;
    bool is_alive = false;
    bool is_downed = false;
    
    t->shard_id = currentShardId();
    t->type = agent.call<uint32_t>(g_offsets.agentVtGetType.val);
    
    if (t->type == AGENT_TYPE_CHAR) {
        
        hl::ForeignClass character = (void*)characterFromAgent(t->aptr, t->agent_id);
        if (character) {
            
            hptr = character.get<uintptr_t>(g_offsets.charHealth.val);
            is_alive = character.call<uintptr_t>(g_offsets.charVtIsAlive.val);
            is_downed = character.call<uintptr_t>(g_offsets.charVtIsDowned.val);
            
            // Get the speciesDef
            if (isInMainThread() &&
                (t->npc_id == 0 || t->species_id == 0) ) {
                uintptr_t spdef = 0;
#ifdef __APPLE__
                character.call_static<void>(g_offsets.charVtGetSpeciesDef.val, &spdef, character.data());
#else
                character.call<void>(g_offsets.charVtGetSpeciesDef.val, &spdef);
#endif
                hl::ForeignClass speciesDef = (void*)spdef;
                if (speciesDef) {
                    t->npc_id = speciesDef.get<uint32_t>(OFF_SPECIES_DEF_ID);
                    t->species_id = speciesDef.get<uint32_t>(OFF_SPECIES_DEF_HASHID);
                }
            }
        }
    }
    else if (t->type == AGENT_TYPE_GADGET)
    {
        hl::ForeignClass pGadget = gdctx.call<void*>(g_offsets.gadgetCtxVtGetGadget.val, t->agent_id);
        if (pGadget) hptr = pGadget.get<uintptr_t>(g_offsets.gadgetHealth.val);
    }
    else if (t->type == AGENT_TYPE_ATTACK)
    {
        hl::ForeignClass pGadget = NULL;
        hl::ForeignClass pAttackTgt = gdctx.call<void*>(g_offsets.gadgetCtxVtGetAttackTgt.val, t->agent_id);
        if (pAttackTgt) {
            pGadget = pAttackTgt.get<void*>(g_offsets.atkTgtGadget.val);
            if (pGadget) hptr = pGadget.get<uintptr_t>(g_offsets.gadgetHealth.val);
        }
    }
    
    hl::ForeignClass health = (void*)hptr;
    if (health) {
#ifdef __APPLE__
        t->hp_val = (uint32_t)health.get<float>(OFF_HP_VAL);
        if (t->hp_max == 0) t->hp_max = (uint32_t)health.get<float>(OFF_HP_MAX);
#else
        t->hp_val = health.get<uint32_t>(OFF_HP_VAL);
        if (t->hp_max == 0) t->hp_max = health.get<uint32_t>(OFF_HP_MAX);
#endif
    }

    bool was_dead = t->isDead;
    t->isDead = (t->type == AGENT_TYPE_CHAR) ? (!is_alive && !is_downed) : (t->hp_val <= 0);
    if (!was_dead && t->isDead) CLogDebug("target %d is now dead <aptr=%p>", t->agent_id, t->aptr);
    
    // Get the wmAgent so we can validate against the array
    if (t->wmptr == 0)
        t->wmptr = ((uintptr_t(*)(uintptr_t))g_offsets.fncGetWmAgent.addr)(t->aptr);
    
    // Always refresh the ptr so the LRU moves it to the top
    t->decodedName = lru_find(GW2::CLASS_TYPE_AGENT, t->aptr, NULL, 0);
    
    t->time_update = now;
    if (t->time_begin==0)
        t->time_begin = now;
    
    bRet = true;
    
    __EXCEPT("[Agent_readTarget] access violation")
    ;
    __FINALLY
    
    return bRet;
}

static bool AgentSelect_read(uintptr_t ctx, struct __GameContext *pGameContext, int64_t now)
{
    bool bRet = false;
    
    __TRY
    
    hl::ForeignClass selCtx = (void*)ctx;
    if (!selCtx)
        return false;
    
    memset(&pGameContext->currentTarget, 0, sizeof(pGameContext->currentTarget));
    pGameContext->currentTarget.aptr = selCtx.get<uintptr_t>(g_offsets.agSelLocked.val);
    bRet = Agent_read(&pGameContext->currentTarget, NULL, 0, now);
    if (!bRet) return false;
    
    Character &c = pGameContext->currentTarget;
    if (!c.aptr || c.agent_id == 0) return false;
    if (c.attitude == CHAR_ATTITUDE_FRIENDLY) return true;
    
    hl::ForeignClass speciesDef = (void*)c.spdef;
    DPSTarget *t = &pGameContext->targets.insert_and_evict(c.agent_id, NULL)->value.data;
    t->aptr = c.aptr;
    t->wmptr = c.wmptr;
    t->agent_id = c.agent_id;
    t->shard_id = currentShardId();
    t->npc_id = speciesDef ? speciesDef.get<uint32_t>(OFF_SPECIES_DEF_ID) : 0;
    t->species_id = speciesDef ? speciesDef.get<uint32_t>(OFF_SPECIES_DEF_HASHID) : 0;
    t->isDead = !c.is_alive && !c.is_downed;
    t->hp_max = c.hp_max;
    t->hp_val = c.hp_val;
    t->time_update = now;
    
    if (t->time_begin==0) {
        t->time_begin = now;
    }
    
    // Always refresh the ptr so the LRU moves it to the top
    t->decodedName = lru_find(GW2::CLASS_TYPE_AGENT, c.aptr, NULL, 0);
    
    __EXCEPT("[AgentSelect_read] access violation")
    ;
    __FINALLY
    
    return bRet;
}


// Returns the difference between two hp values, allowing for wrap around from 0 to max.
static inline int32_t hp_diff(int32_t hp_new, int32_t hp_old, int32_t max)
{
    int32_t diff = hp_old - hp_new;
    if (diff < 0 && diff < -max / 5)
    {
        diff = hp_old + (max - hp_new);
    }
    
    return diff;
}

static inline void hit_array_calc_interval(uint32_t *dmg, const DPSHit *hit_arr, uint32_t hit_cnt, int64_t interval, int64_t now)
{
    *dmg = 0;
    for (i32 i = hit_cnt - 1; i >= 0; --i) {
        if (now - hit_arr[i].time > interval)
            break;
        *dmg += hit_arr[i].dmg;
    }
}

// Removes invalid or old targets from the array.
// Calculates targets dps numbers
static void validateAndCalcTargets(int64_t now)
{
    static i64 last_update = 0;
    
    // No need to calc twice for the same frame
    if (now == last_update)
        return;
    
    static int64_t lambda_now;
    static bool isPlayerOOC;
    static bool isPlayerDead;
    
    lambda_now = now;
    isPlayerOOC = controlledPlayer()->cd.lastKnownCombatState == 0;
    isPlayerDead = !controlledCharacter()->is_alive && !controlledCharacter()->is_downed;
    
    currentContext()->targets.iter_lock([](struct CacheEntry<DPSTarget>* entry) {
        
        DPSTarget* t = &entry->value.data;
        uintptr_t wmptr = 0;
        
        if (t->shard_id != currentShardId()) {
            CLogDebug("target %d <agent:%p> <wmagent:%p> is from a different instance <instance:%d>, removed", t->agent_id, t->aptr, t->wmptr, t->shard_id);
            goto remove;
        }
        
        wmptr = wmAgentFromAgentId(t->agent_id);
        if (wmptr == 0 || wmptr != t->wmptr) {
            CLogDebug("target %d <agent:%p> <wmagent:%p> invalidated <wmptr:%p>, removed", t->agent_id, t->aptr, t->wmptr, wmptr);
            goto remove;
        }
        
        if (lambda_now - t->time_update > MAX_TARGET_AGE) {
            CLogDebug("target %d <agent:%p> <wmagent:%p> is too old <age:%llu>, removed", t->agent_id, t->aptr, t->wmptr, lambda_now - t->time_update);
            goto remove;
        }
        
        // Target is new and was never hit, skip
        if (t->num_hits == 0)
            return;
        
        // Target is already dead, skip calculation
        if (t->isDead)
            return;
        
        // Read target data
        Agent_readTarget(t, lambda_now);
        
        // If the target is at full HP and it's been a while since we hit it, autoreset.
        if (t->hp_val == t->hp_max && lambda_now - t->time_hit > RESET_TIMER)
        {
            CLogDebug("target %d <agent:%p> <wmagent:%p> reset <age:%llu>, removed", t->agent_id, t->aptr, t->wmptr, lambda_now - t->time_hit);
            goto remove;
        }
        
        // We don't calculate invuln time or OOC time
        if (t->invuln || isPlayerDead || isPlayerOOC) {
            return;
        }
        
        // Total time always starts from first hit
        if (t->duration == 0 || t->last_update == 0)
            t->duration = lambda_now - t->hits[0].time;
        else
            t->duration += lambda_now - t->last_update;
        
        // Update the total DPS and damage.
        if (t->hp_last >= 0)
        {
            i32 diff = hp_diff(t->hp_val, t->hp_last, t->hp_max);
            if (diff > 0) {
                // only calculate a hit if the target lost health
                // otherwise can get buggy if the target uses a healing skill
                t->hp_lost += diff;
                i32 hi = t->num_hits_team;
                if (hi < MAX_HITS)
                {
                    t->hits_team[hi].dmg = diff;
                    t->hits_team[hi].time = lambda_now;
                    ++t->num_hits_team;
                }
            }
        }
        
        // The game reports more damage than the target's HP on the last hit
        // for the sake of consistency, equalize both numbers if we did more dmg
        // than the target's HP, shouldn't matter much only for small targets
        // where we are the only attacker
        if (t->hp_lost < (i32)t->tdmg) {
            CLogDebug("target dmg adjust <aptr=%p> <hp_lost=%d> <tdmg=%d>", t->aptr, t->hp_lost, t->tdmg);
            t->hp_lost = (i32)t->tdmg;
        }
        
        hit_array_calc_interval(&t->c1dmg, t->hits, t->num_hits, DPS_INTERVAL_1, lambda_now);
        hit_array_calc_interval(&t->c2dmg, t->hits, t->num_hits, DPS_INTERVAL_2, lambda_now);
        
        hit_array_calc_interval(&t->c1dmg_team, t->hits_team, t->num_hits_team, DPS_INTERVAL_1, lambda_now);
        hit_array_calc_interval(&t->c2dmg_team, t->hits_team, t->num_hits_team, DPS_INTERVAL_2, lambda_now);
        
        // Save last seen HP & timestamp
        t->hp_last = t->hp_val;
        t->last_update = lambda_now;
        
        return;
        
    remove:
        ImGui_RemoveGraphData(t->agent_id);
        currentContext()->targets.remove(t->agent_id);
        return;
    });
    
    last_update = now;
}

uint32_t Player_HasEliteSpec(uintptr_t pptr) {
    
    if (!pptr) return false;
    
    __TRY
    
    hl::ForeignClass player = (void*)pptr;
    if (player)
    {
        hl::ForeignClass specMgr = player.call<void*>(g_offsets.playerVtGetSpecMgr.val);
        if (specMgr) {
            
            hl::ForeignClass spec3 = specMgr.get<void*>(OFF_SPECMGR_SPECS + (GW2::SPEC_SLOT_3 * sizeof(void*)));
            if (spec3) {
                
                GW2::Specialization specType = spec3.get<GW2::Specialization>(OFF_SPEC_TYPE);
                switch (specType) {
                    case GW2::SPEC_GUARD_DRAGONHUNTER:
                    case GW2::SPEC_MES_CHRONOMANCER:
                    case GW2::SPEC_ELE_TEMPEST:
                    case GW2::SPEC_ENGI_SCRAPPER:
                    case GW2::SPEC_THIEF_DAREDEVIL:
                    case GW2::SPEC_NECRO_REAPER:
                    case GW2::SPEC_RANGER_DRUID:
                    case GW2::SPEC_WAR_BERSERKER:
                    case GW2::SPEC_REV_HERALD:
                        return 1;
                    case GW2::SPEC_GUARD_FIREBRAND:
                    case GW2::SPEC_WAR_SPELLBREAKER:
                    case GW2::SPEC_ENG_HOLOSMITH:
                    case GW2::SPEC_RANGER_SOULBEAST:
                    case GW2::SPEC_THIEF_DEADEYE:
                    case GW2::SPEC_ELE_WEAVER:
                    case GW2::SPEC_MES_MIRAGE:
                    case GW2::SPEC_NEC_SCOURGE:
                    case GW2::SPEC_REV_RENEGADE:
                        return 2;
                    default:
                        return 0;
                        
                }
            }
        }
    }
    
    __EXCEPT("[Pl_HasEliteSpec] access violation")
    ;
    __FINALLY
    
    return false;
}

static void validateDpsTarget(Character* selectedChar, int64_t now)
{
    static i64 last_update = 0;
    static u64 last_autolock_aptr = 0;
    GameContext* gameContext = currentContext();
    DPSTargetEx &dps_target = gameContext->currentDpsTarget;
    
    // No need to update twice in the same frame
    if (last_update != 0 && last_update == now)
        return;
    
    // Save last update timestamp
    last_update = now;
    
    struct CacheEntry<DPSTarget>* entry = (selectedChar && selectedChar->agent_id) ? gameContext->targets.find(selectedChar->agent_id) : NULL;
    if (!entry && selectedChar &&
        selectedChar->agent_id != 0 &&
        selectedChar->attitude != CHAR_ATTITUDE_FRIENDLY) {
        CLogWarn("Unable to locate selected target %d <aptr:%p> in targets array", selectedChar->agent_id, selectedChar->aptr);
    }
    
    DPSTarget* selectedTarget = entry ? &entry->value.data : NULL;
    DPSTarget* target = NULL;
    bool targetIsSelected = false;
    bool selectedTargetAutolock = false;
    bool selectedTargetIsValid = selectedTarget ? (selectedTarget->aptr && selectedTarget->agent_id) : false;
    
    if (selectedTargetIsValid &&
        selectedTarget->aptr != last_autolock_aptr) {
        
        // Check the newly selected target for auto-lock
        DPSTarget* t = selectedTarget;
        if (t->npc_id) {
            last_autolock_aptr = selectedTarget->aptr;
            selectedTargetAutolock = autolock_get(t->npc_id);
        }
        if (selectedTargetAutolock) {
            target = t;
            CLogDebug("target %d autolock [aptr=%p] [npc_id=%i]", t->agent_id, t->aptr, t->npc_id);
        }
    }
    
    // Valid target selected and we aren't target locked?
    // or selected target needs to be auto-locked
    if (selectedTargetIsValid &&
        (selectedTargetAutolock || !dps_target.locked || (dps_target.locked && selectedTarget->aptr == dps_target.t.aptr))) {
        
        target = selectedTarget;
        dps_target.time_sel = now;
        targetIsSelected = true;
    }
    
    // We don't have a valid target, try last known target
    if (!target && dps_target.t.aptr && dps_target.t.agent_id) {
        
        entry = gameContext->targets.find(dps_target.t.agent_id);
        if (entry) target = &entry->value.data;
        else target = &dps_target.t;
    }
    
    // Ignore invalid targets.
    if (!target || target->aptr == 0 || target->agent_id == 0)
        return;
    
    // Traget is selected, check for target swap
    if (targetIsSelected &&
        target->aptr != dps_target.t.aptr) {
        memset(&dps_target, 0, sizeof(dps_target));
    }
    
    // No target selected, linger time has passed
    // reset and hide the UI (by returning NULL)
    if (!targetIsSelected &&
        !dps_target.locked &&
        now - dps_target.time_sel >= g_state.target_retention_time)
    {
        last_update = 0;
        last_autolock_aptr = 0;
        memset(&dps_target, 0, sizeof(dps_target));
        return;
    }
    
    // Toggle target lock
    //module_toggle_bind(&g_state.bind_dps_lock, &dps_target.locked);
    if (selectedTargetAutolock) dps_target.locked = true;
    
    // Copy & update target data
    assert(target != NULL);
    target->time_update = now;
    dps_target.t = *target;
}

static int TargetTimeCompare(void const* pItem1, void const* pItem2)
{
    const DPSTarget* t1 = *(const DPSTarget**)pItem1;
    const DPSTarget* t2 = *(const DPSTarget**)pItem2;
    
    if (t1->time_update < t2->time_update) return 1;
    else if (t1->time_update > t1->time_update) return -1;
    else return 0;
}

static void buildRecentTargetsArray(ClientTarget out_targets[], u32 arrsize, u32 *outsize, i64 now)
{
    if (!out_targets || !arrsize)
        return;
    
    const DPSTargetEx *dps_target = &currentContext()->currentDpsTarget;
    
    static std::vector<DPSTarget*> recentTargets;
    recentTargets.clear();
    
    currentContext()->targets.lock();
    currentContext()->targets.iter_nolock([](struct CacheEntry<DPSTarget>* entry) {
        
        DPSTarget *t = &entry->value.data;
        if (t->time_update > 0) recentTargets.push_back(t);
    });
    
    if ( recentTargets.size() > 0 )
        qsort(recentTargets.data(), recentTargets.size(), sizeof( recentTargets[0] ), TargetTimeCompare);
    
    bool currentTargetFound = false;
    int count = std::min(arrsize, (uint32_t)recentTargets.size());
    for (int i=0; i<count; i++) {
        
        const DPSTarget *t = recentTargets[i];
        if (dps_target->t.agent_id && dps_target->t.agent_id == t->agent_id)
            currentTargetFound = true;
        
        out_targets[i].id = t->agent_id;
        out_targets[i].tdmg = t->tdmg;
        out_targets[i].invuln = t->invuln;
        out_targets[i].reserved1 = t->c1dmg;
        out_targets[i].time = (u32)(t->duration / 1000.0f);
    }
    currentContext()->targets.unlock();
    
    if (dps_target->t.agent_id && !currentTargetFound) {
        
        // Preferred target isn't in our recent targets array
        // overwrite the first entry with our preferred target
        const DPSTarget* t = &dps_target->t;
        if (count==0) count=1;
        out_targets[count-1].id = t->agent_id;
        out_targets[count-1].tdmg = t->tdmg;
        out_targets[count-1].invuln = t->invuln;
        out_targets[count-1].reserved1 = t->c1dmg;
        out_targets[count-1].time = (u32)(t->duration / 1000.0f);
    }
    
    if (outsize) *outsize = count;
}

static void setupGroupDpsData(int64_t now)
{
    // Add the current player into the group listing.
    //get_recent_targets(target, char_array, temp_players[total].targets, MSG_TARGETS, NULL, now);
    Player *p = controlledPlayer();
    p->dps.is_connected = true;
    p->dps.is_connected = true;
    p->dps.in_combat = p->cd.lastKnownCombatState;
    p->dps.time = p->cd.duration;
    p->dps.damage = (uint32_t)p->cd.damageOut;
    p->dps.damage_in = (uint32_t)p->cd.damageIn;
    p->dps.heal_out = (uint32_t)p->cd.healOut;
    p->dps.stats = p->c.stats;
    p->dps.profession = p->c.profession;
    buildRecentTargetsArray(p->dps.targets, ARRAYSIZE(p->dps.targets), NULL, now);
    
    
    const struct DPSTargetEx* target = &currentContext()->currentDpsTarget;
    static std::vector<Player*> closePlayers;
    
    // Set total dmg/duration to current target
    if (target->t.agent_id) {
        
        closePlayers.clear();
        if (controlledCharacter()->aptr != 0)
            closePlayers.push_back(controlledPlayer());
        currentContext()->closestPlayers.lock();
        currentContext()->closestPlayers.iter_nolock([](struct CacheEntry<Player>* entry) {
            if (entry->value.data.dps.is_connected)
                closePlayers.push_back(&entry->value.data);
        });
        
        for (int i = 0; i < closePlayers.size(); i++) {
            Player* p = closePlayers[i];
            p->dps.target_time = 0;
            p->dps.target_dmg = 0;
            for (int j = 0; j < MSG_TARGETS; j++) {
                if (p->dps.targets[j].id == target->t.agent_id) {
                    p->dps.target_time = (f32)p->dps.targets[j].time;
                    p->dps.target_dmg = p->dps.targets[j].tdmg;
                    break;
                }
            }
        }
        
        currentContext()->closestPlayers.unlock();
    }
}

bool Pl_GetSpec(uintptr_t pptr, Spec *outSpec)
{
    bool bRet = false;
    
    __TRY
    
    if (!pptr || !outSpec)
        return bRet;
    
    memset(outSpec, 0, sizeof(Spec));
    
    hl::ForeignClass player = (void*)pptr;
    if (player)
    {
        hl::ForeignClass specMgr = player.call<void*>(g_offsets.playerVtGetSpecMgr.val);
        if (specMgr) {
            
            for (size_t i = 0; i < GW2::SPEC_SLOT_END; i++) {
                hl::ForeignClass spec = specMgr.get<void*>(OFF_SPECMGR_SPECS + (i * sizeof(void*)));
                if (spec) {
                    outSpec->specs[i].ptr = (u64)spec.data();
                    outSpec->specs[i].name = lru_find(GW2::CLASS_TYPE_SPEC, outSpec->specs[i].ptr, NULL, 0);
                    GW2::Specialization specType = spec.get<GW2::Specialization>(OFF_SPEC_TYPE);
                    if (specType < GW2::SPEC_NONE || specType > GW2::SPEC_END) {
                        outSpec->specs[i].id = GW2::SPEC_NONE;
                    }
                    else {
                        outSpec->specs[i].id = specType;
                    }
                }
                
                for (size_t j = 0; j < GW2::TRAIT_SLOT_END; j++) {
                    hl::ForeignClass trait = specMgr.get<void*>(OFF_SPECMGR_TRAITS + (i * GW2::TRAIT_SLOT_END + j) * sizeof(void*));
                    if (trait) {
                        outSpec->traits[i][j].ptr = (u64)trait.data();
                        outSpec->traits[i][j].name = lru_find(GW2::CLASS_TYPE_TRAIT, outSpec->traits[i][j].ptr, NULL, 0);
                        GW2::Trait traitType = trait.get<GW2::Trait>(OFF_TRAIT_TYPE);
                        if (traitType < GW2::TRAIT_NONE || traitType >= GW2::TRAIT_END) {
                            outSpec->traits[i][j].id = GW2::TRAIT_NONE;
                        }
                        else {
                            outSpec->traits[i][j].id = traitType;
                        }
                    }
                }
            }
            
            bRet = true;
        }
    }
    
    __EXCEPT("[Pl_GetSpec] access violation")
    ;
    __FINALLY
    
    return bRet;
}



static inline bool WeaponIs2H(uint32_t type)
{
    switch (type)
    {
        case (GW2::WEP_TYPE_HAMMER):
        case (GW2::WEP_TYPE_LONGBOW):
        case (GW2::WEP_TYPE_SHORTBOW):
        case (GW2::WEP_TYPE_GREATSWORD):
        case (GW2::WEP_TYPE_RIFLE):
        case (GW2::WEP_TYPE_STAFF):
            return true;
        default:
            return false;
    }
}

static inline bool EquipSlotIsWeap(uint32_t slot)
{
    switch (slot)
    {
        case (GW2::EQUIP_SLOT_AQUATIC_WEAP1):
        case (GW2::EQUIP_SLOT_AQUATIC_WEAP2):
        case (GW2::EQUIP_SLOT_MAINHAND_WEAP1):
        case (GW2::EQUIP_SLOT_OFFHAND_WEAP1):
        case (GW2::EQUIP_SLOT_MAINHAND_WEAP2):
        case (GW2::EQUIP_SLOT_OFFHAND_WEAP2):
            return true;
        default:
            return false;
    }
}

static inline EquipItem *EquipItemBySlot(uint32_t slot, EquipItems *equipItems)
{
    EquipItem *eqItem = NULL;
    
    switch (slot) {
        case (GW2::EQUIP_SLOT_AQUATIC_HELM):
            eqItem = &equipItems->head_aqua;
            break;
        case (GW2::EQUIP_SLOT_BACK):
            eqItem = &equipItems->back;
            break;
        case (GW2::EQUIP_SLOT_CHEST):
            eqItem = &equipItems->chest;
            break;
        case (GW2::EQUIP_SLOT_BOOTS):
            eqItem = &equipItems->boots;
            break;
        case (GW2::EQUIP_SLOT_GLOVES):
            eqItem = &equipItems->gloves;
            break;
        case (GW2::EQUIP_SLOT_HELM):
            eqItem = &equipItems->head;
            break;
        case (GW2::EQUIP_SLOT_PANTS):
            eqItem = &equipItems->leggings;
            break;
        case (GW2::EQUIP_SLOT_SHOULDERS):
            eqItem = &equipItems->shoulder;
            break;
        case (GW2::EQUIP_SLOT_ACCESSORY1):
            eqItem = &equipItems->acc_ear1;
            break;
        case (GW2::EQUIP_SLOT_ACCESSORY2):
            eqItem = &equipItems->acc_ear2;
            break;
        case (GW2::EQUIP_SLOT_RING1):
            eqItem = &equipItems->acc_ring1;
            break;
        case (GW2::EQUIP_SLOT_RING2):
            eqItem = &equipItems->acc_ring2;
            break;
        case (GW2::EQUIP_SLOT_AMULET):
            eqItem = &equipItems->acc_amulet;
            break;
        case (GW2::EQUIP_SLOT_AQUATIC_WEAP1):
            eqItem = &equipItems->wep1_aqua;
            break;
        case (GW2::EQUIP_SLOT_AQUATIC_WEAP2):
            eqItem = &equipItems->wep2_aqua;
            break;
        case (GW2::EQUIP_SLOT_MAINHAND_WEAP1):
            eqItem = &equipItems->wep1_main;
            break;
        case (GW2::EQUIP_SLOT_OFFHAND_WEAP1):
            eqItem = &equipItems->wep1_off;
            break;
        case (GW2::EQUIP_SLOT_MAINHAND_WEAP2):
            eqItem = &equipItems->wep2_main;
            break;
        case (GW2::EQUIP_SLOT_OFFHAND_WEAP2):
            eqItem = &equipItems->wep2_off;
            break;
    };
    
    return eqItem;
}

bool Ch_GetInventory(uintptr_t iptr, EquipItems *equipItems)
{
    bool bRet = false;
    
    __TRY
    
    if (!iptr || !equipItems)
        return bRet;
    
    memset(equipItems, 0, sizeof(*equipItems));
    
    hl::ForeignClass inventory = (void*)iptr;
    if (!inventory)
        return bRet;
    
    for (uint32_t i = 0; i < GW2::EQUIP_SLOT_END; i++) {
        
        EquipItem *eqItem = EquipItemBySlot(i, equipItems);
        if (!eqItem)
            continue;
        
        eqItem->type = i;
        eqItem->is_wep = EquipSlotIsWeap(i);
        
        hl::ForeignClass equip = inventory.get<void*>(g_offsets.invEquipItemArr.val + i * sizeof(void*));
        if (!equip) {
            
            eqItem->stat_def.id = 0xFFFF; // "NOEQUIP"
            if (eqItem->is_wep)
                eqItem->wep_type = GW2::WEP_TYPE_NOEQUIP;
            
        }
        else {
            
            eqItem->ptr = (u64)equip.data();
            
            hl::ForeignClass itemDef = equip.get<void*>(g_offsets.equipItemDef.val);
            if (itemDef) {
                eqItem->item_def.ptr = (u64)itemDef.data();
                eqItem->item_def.id = itemDef.get<uint32_t>(OFF_ITEM_DEF_ID);
                eqItem->item_def.rarity = itemDef.get<GW2::ItemRarity>(OFF_ITEM_DEF_RARITY);
                eqItem->item_def.name = lru_find(GW2::CLASS_TYPE_ITEM, eqItem->item_def.ptr, NULL, 0);
                if (eqItem->is_wep) {
                    hl::ForeignClass wepDef = itemDef.get<void*>(OFF_ITEM_DEF_WEAPON);
                    if (wepDef) {
                        eqItem->wep_type = wepDef.get<int32_t>(OFF_WEP_TYPE);
                    }
                }
                
                hl::ForeignClass itemType = itemDef.get<void*>(OFF_ITEM_DEF_ITEMTYPE);
                if (itemType) {
                    eqItem->name = lru_find(GW2::CLASS_TYPE_ITEMTYPE, (u64)itemType.data(), NULL, 0);
                }
            }
            
            hl::ForeignClass statDef = equip.get<void*>(eqItem->is_wep ?
                                                        g_offsets.equipItemStats.val + sizeof(uintptr_t) :
                                                        g_offsets.equipItemStats.val);
            if (statDef) {
                eqItem->stat_def.ptr = (u64)statDef.data();
                eqItem->stat_def.id = statDef.get<uint32_t>(OFF_STAT_ID);
                eqItem->stat_def.name = lru_find(GW2::CLASS_TYPE_STAT, eqItem->stat_def.ptr, NULL, 0);
            }
            
            // Runes / Sigils
            hl::ForeignClass up1Def = equip.get<void*>(eqItem->is_wep ?
                                                       g_offsets.equipItemUpgrade.val + sizeof(uintptr_t) :
                                                       g_offsets.equipItemUpgrade.val);
            if (up1Def) {
                eqItem->upgrade1.ptr = (u64)up1Def.data();
                eqItem->upgrade1.id = up1Def.get<uint32_t>(OFF_ITEM_DEF_ID);
                eqItem->upgrade1.rarity = up1Def.get<GW2::ItemRarity>(OFF_ITEM_DEF_RARITY);
                eqItem->upgrade1.name = lru_find(GW2::CLASS_TYPE_UPGRADE, eqItem->upgrade1.ptr, NULL, 0);
            }
            
            // Only wep can have 2nd upgrade
            if (eqItem->is_wep && WeaponIs2H(eqItem->wep_type))
            {
                hl::ForeignClass up2Def = equip.get<void*>(g_offsets.equipItemUpgrade.val + sizeof(uintptr_t)*2);
                if (up2Def) {
                    eqItem->upgrade2.ptr = (u64)up2Def.data();
                    eqItem->upgrade2.id = up2Def.get<uint32_t>(OFF_ITEM_DEF_ID);
                    eqItem->upgrade2.rarity = up2Def.get<GW2::ItemRarity>(OFF_ITEM_DEF_RARITY);
                    eqItem->upgrade2.name = lru_find(GW2::CLASS_TYPE_UPGRADE, eqItem->upgrade2.ptr, NULL, 0);
                }
            }
            
            // Infusions
            hl::ForeignClass upgradeDef = equip.call<void*>(g_offsets.equipItemVtGetUpgrades.val);
            if (upgradeDef)
            {
                auto infusions = upgradeDef.get<ANet::Collection<uintptr_t, true>>(OFF_UPGRADE_DEF_INFUS_ARR);
                if (infusions.isValid()) {
                    uint32_t cap = 0;
                    for (uint32_t j = 0; j < infusions.capacity() && cap < ARRAYSIZE(eqItem->infus_arr); j++) {
                        hl::ForeignClass infuse = (void*)infusions[j];
                        if (infuse) {
                            eqItem->infus_arr[cap].ptr = (u64)infuse.data();
                            eqItem->infus_arr[cap].id = infuse.get<uint32_t>(OFF_ITEM_DEF_ID);
                            eqItem->infus_arr[cap].rarity = infuse.get<GW2::ItemRarity>(OFF_ITEM_DEF_RARITY);
                            eqItem->infus_arr[cap].name = lru_find(GW2::CLASS_TYPE_ITEM, eqItem->infus_arr[cap].ptr, NULL, 0);
                            //DBGPRINT(TEXT("infus[%d]: id:%d  rarity:%d  ptr:%p"),
                            //	cap-1, eqItem->infus_arr[cap-1].id, eqItem->infus_arr[cap-1].rarity, infuse.data());
                            cap++;
                        }
                    }
                    eqItem->infus_len = cap;
                }
            }
            
            // Skin
            if (itemDef) {
                hl::ForeignClass skinDef = equip.call<void*>(g_offsets.equipItemVtGetSkin.val);
                if (skinDef) {
                    eqItem->skin_def.ptr = (u64)skinDef.data();
                    eqItem->skin_def.id = skinDef.get<uint32_t>(OFF_ITEM_DEF_ID);
                    eqItem->skin_def.name = lru_find(GW2::CLASS_TYPE_SKIN, eqItem->skin_def.ptr, NULL, 0);
                }
            }
        }
    }
    
    bRet = true;
    
    __EXCEPT("[Ch_GetInventory] access violation")
    ;
    __FINALLY
    
    return bRet;
}


uintptr_t CtxContact_GetContactSelf(uintptr_t pctx)
{
    __TRY
    
    if (!pctx)
        return 0;
    
    hl::ForeignClass ctx = (void*)pctx;
    if (!ctx)
        return 0;
    
    return ctx.call<uintptr_t>(g_offsets.contactCtxGetSelf.val);
    
    __EXCEPT("[CtxContact_GetContactSelf] access violation")
    ;
    __FINALLY
    return 0;
}

const void* Contact_GetAccountName(uintptr_t ctptr)
{
    __TRY
    
    if (!ctptr)
        return 0;
    
    hl::ForeignClass contact = (void*)ctptr;
    if (!contact) return 0;
    
    return contact.call<const void* *>(g_offsets.contactGetAccountName.val);
    
    __EXCEPT("[Contact_GetAccountName] access violation")
    ;
    __FINALLY
    return 0;
    
}

static uint32_t Squad_GetSubgroup(uintptr_t squadptr, uint64_t uuid)
{
    
#ifdef _MSC_VER
#  define PACK( __Declaration__ ) __pragma( pack(push, 1) ) __Declaration__ __pragma( pack(pop) )
#elif defined (__APPLE__)
#  define PACK( __Declaration__ ) __Declaration__ __attribute__((packed))
#elif defined(__GNUC__)
#  define PACK( __Declaration__ ) __pragma( pack(push, 1) ) __Declaration__ __pragma( pack(pop) )
#endif
    
    PACK(
         struct squadArrElement {
             uint64_t uuid;
             uint64_t unknown1;
             uint32_t unknown2;
             uint32_t subGroup;
             uint32_t unknown3;
         });
    
    uint32_t retVal = 0;
    
    __TRY
    
    if (!squadptr || !uuid)
        return 0;
    
    hl::ForeignClass squadCtx = (void*)squadptr;
    if (!squadCtx)
        return retVal;
    
    uint32_t groupNo = squadCtx.get<uint32_t>(OFF_SQUAD_GROUP_NUM);
    auto squadArr = squadCtx.get<ANet::Collection<squadArrElement, false>>(OFF_SQUAD_MEMBER_ARR);
    if (!squadArr.isValid() || groupNo == 0)
        return retVal;
    
    for (uint32_t i = 0; i < squadArr.capacity(); i++) {
        
        if (squadArr[i].uuid == uuid) {
            if (squadArr[i].subGroup < SQUAD_MAX_SUBGROUPS) {
                retVal = squadArr[i].subGroup + 1;
                return retVal;
            }
            break;
        }
    }
    
    
    __EXCEPT("[Pl_GetSquadSubgroup] access violation")
    ;
    __FINALLY
    
    return retVal;
}

bool Ch_GetSpeciesDef(uintptr_t cptr, uintptr_t *pSpeciesDef)
{
    if (!isInMainThread()) return false;
    bool bRet = false;
    
    __TRY
    
    hl::ForeignClass c = (void*)cptr;
    if (!c || !pSpeciesDef)
        return bRet;
    
#ifdef __APPLE__
    c.call_static<void>(g_offsets.charVtGetSpeciesDef.val, pSpeciesDef, cptr);
#else
    c.call<void*>(g_offsets.charVtGetSpeciesDef.val, pSpeciesDef);
#endif
    if (*pSpeciesDef)
        bRet = true;
    
    __EXCEPT("[Ch_GetSpeciesDef] access violation")
    ;
    __FINALLY
    
    return bRet;
}

uintptr_t Ag_GetWmAgemt(uintptr_t aptr)
{
    uintptr_t ret = 0;
    typedef uintptr_t(__fastcall*fpGetWmAgent)(uintptr_t);
    
    __TRY
    
    if (!aptr || !g_offsets.fncGetWmAgent.addr)
        return ret;
    
    ret = ((fpGetWmAgent)g_offsets.fncGetWmAgent.addr)(aptr);
    
    
    __EXCEPT("[Ag_GetWmAgemt] access violation")
    ;
    __FINALLY
    
    return ret;
}

uint8_t* CodedTextFromHashId(uint32_t hashId, uint32_t a2)
{
    //static uintptr_t pfcCodedNameFromHashId = 0;
    
    typedef uint8_t*(__fastcall *fCodedTextFromHashId)(uint32_t hashId, uint32_t a2);
    
    if (!g_offsets.fncCodedTextFromHashId.addr || !hashId)
        return nullptr;
    
    __TRY
    
    return ((fCodedTextFromHashId)g_offsets.fncCodedTextFromHashId.addr)(hashId, a2);
    
    __EXCEPT("[CodedTextFromHashId] access violation")
    ;
    __FINALLY
    
    return nullptr;
}

bool DecodeText(uint8_t* codedText, cbDecodeText_t cbDecodeText, uintptr_t ctx)
{
    
    bool bRet = false;
    //static uintptr_t pfcDecodeText = 0;
    
    typedef void(__fastcall *fDecodeText)(uint8_t*, cbDecodeText_t, uintptr_t);
    
    if (!g_offsets.fncDecodeCodedText.addr || !codedText || !cbDecodeText)
        return bRet;
    
    __TRY
    
    ((fDecodeText)g_offsets.fncDecodeCodedText.addr)(codedText, cbDecodeText, ctx);
    bRet = true;
    
    __EXCEPT("[DecodeText] access violation")
    ;
    __FINALLY
    
    return bRet;
}

bool Agent_GetCodedName(uintptr_t aptr, cbDecodeText_t cbDecodeText, uintptr_t ctx)
{
    bool bRet = false;
    
    uintptr_t wmptr = Ag_GetWmAgemt(aptr);
    if (!wmptr) return false;
    
    __TRY
    
    hl::ForeignClass wmAgent = (void*)wmptr;
    
    if (!wmAgent || !cbDecodeText || !ctx)
        return bRet;
    
    uint8_t* codedName = (uint8_t*)wmAgent.call<void*>(g_offsets.wmAgVtGetCodedName.val);
    if (codedName) {
        bRet = DecodeText(codedName, cbDecodeText, ctx);
    }
    
    __EXCEPT("[Agent_GetCodedName] access violation")
    ;
    __FINALLY
    
    return bRet;
}

bool WmAgent_GetCodedName(uintptr_t wmptr, cbDecodeText_t cbDecodeText, uintptr_t ctx)
{
    bool bRet = false;
    if (!wmptr) return false;
    
    __TRY
    
    hl::ForeignClass wmAgent = (void*)wmptr;
    
    if (!wmAgent || !cbDecodeText || !ctx)
        return bRet;
    
    uint8_t* codedName = (uint8_t*)wmAgent.call<void*>(g_offsets.wmAgVtGetCodedName.val);
    if (codedName) {
        bRet = DecodeText(codedName, cbDecodeText, ctx);
    }
    
    __EXCEPT("[Agent_GetCodedName] access violation")
    ;
    __FINALLY
    
    return bRet;
}

uint32_t HashIdFromPtr(int type, uintptr_t ptr)
{
    uint32_t hashId = 0;
    hl::ForeignClass cls = (void*)ptr;
    if (!cls)
        return 0;
    
    __TRY
    
    switch (type) {
        case (GW2::CLASS_TYPE_ITEM):
            hashId = cls.get<uint32_t>(OFF_ITEM_HASHID);
            break;
        case (GW2::CLASS_TYPE_UPGRADE):
            hashId = cls.get<uint32_t>(OFF_ITEM_HASHID);
            //hashId = process_read_u32(ptr + OFF_ITEM_UPG_HASHID);
            break;
        case (GW2::CLASS_TYPE_SPEC):
            hashId = cls.get<uint32_t>(OFF_SPEC_HASHID);
            break;
        case (GW2::CLASS_TYPE_TRAIT):
            hashId = cls.get<uint32_t>(OFF_TRAIT_HASHID);
            break;
        case (GW2::CLASS_TYPE_SKILL):
            hashId = cls.get<uint32_t>(OFF_SKILLDEF_HASHID);
            break;
        case (GW2::CLASS_TYPE_SKIN):
            hashId = cls.get<uint32_t>(OFF_SKIN_HASHID);
            break;
        case (GW2::CLASS_TYPE_STAT):
            hashId = cls.get<uint32_t>(OFF_STAT_HASHID);
            break;
        case (GW2::CLASS_TYPE_ITEMTYPE):
            hashId = cls.get<uint32_t>(OFF_ITEM_TYPE_HASHID);
            break;
        case (GW2::CLASS_TYPE_SPECIESDEF):
            hashId = cls.get<uint32_t>(OFF_SPECIES_DEF_HASHID);
            break;
    };
    
    __EXCEPT("[HashIdFromPtr] access violation")
    ;
    __FINALLY
    
    return hashId;
}


void combatLogProcess(int type, int hit, uintptr_t aptr_src, uintptr_t aptr_tgt, uintptr_t skillDef)
{

    if (aptr_tgt == 0 || hit == 0)
        return;
    
    int64_t now = time_now();
    Player* player = controlledPlayer();
    GameContext *gameContext = currentContext();
    hl::ForeignClass agent = (void*)aptr_tgt;
    if (!agent) return;
    
    // If the player isn't in combat yet, enter combat
    if (player->cd.lastKnownCombatState == 0)
    {
        CLogDebug("UPDATE COMBAT STATE [%s]", std_utf16_to_utf8((const utf16str)player->c.name).c_str());
        Agent_read(&gameContext->controlledPlayer.c, &gameContext->controlledPlayer, 0, now);
    }
    
    ENTER_SPIN_LOCK(player->cd.lock);
    
    __TRY
    
    if (type == GW2::CL_CONDI_DMG_OUT ||
        type == GW2::CL_CRIT_DMG_OUT ||
        type == GW2::CL_GLANCE_DMG_OUT ||
        type == GW2::CL_PHYS_DMG_OUT)
    {
        
        uint32_t agent_id = agent.call<uint32_t>(g_offsets.agentVtGetId.val);
        if (agent_id == 0) return;
        DPSTarget *t = &gameContext->targets.insert_and_evict(agent_id, NULL)->value.data;
        t->aptr = (uintptr_t)agent.data();
        t->agent_id = agent_id;
        t->invuln = false;
        t->tdmg += hit;
        t->time_update = now;
        t->time_hit = now;
        
        i32 hi = t->num_hits;
        if (hi < MAX_HITS)
        {
            t->hits[hi].dmg = hit;
            t->hits[hi].time = now;
            t->hits[hi].eff_id = skillDef ? *(uint32_t*)(skillDef + OFF_SKILLDEF_EFFECT) : 0;
            t->hits[hi].eff_hash = skillDef ? *(uint32_t*)(skillDef + OFF_SKILLDEF_HASHID) : 0;
            ++t->num_hits;
        }
        
        Agent_readTarget(t, now);
    }
    
    
    // if the player is now in combat, add hit to totals
    if (player->cd.lastKnownCombatState == 1)
    {
        
        if (type == GW2::CL_CONDI_DMG_OUT ||
            type == GW2::CL_CRIT_DMG_OUT ||
            type == GW2::CL_GLANCE_DMG_OUT ||
            type == GW2::CL_PHYS_DMG_OUT)
        {
           player->cd.damageOut += hit;
        }
        
        if (type == GW2::CL_CONDI_DMG_IN ||
            type == GW2::CL_CRIT_DMG_IN ||
            type == GW2::CL_GLANCE_DMG_IN ||
            type == GW2::CL_PHYS_DMG_IN)
        {
            player->cd.damageIn += hit;
        }
        
        if (type == GW2::CL_HEAL_OUT)
        {
            player->cd.healOut += hit;
        }
    }
    
    __EXCEPT("[combatLogProcess] access violation")
    ;
    __FINALLY
    
    EXIT_SPIN_LOCK(player->cd.lock);
}


void combatLogResultProcess(int result, int hit, uintptr_t aptr_src, uintptr_t aptr_tgt, uintptr_t skillDef, int source)
{
    int64_t now = time_now();
    GameContext *gameContext = currentContext();
    hl::ForeignClass agent = (void*)aptr_tgt;
    if (!agent) return;
    
    bool isInvuln = false;
    if (source == 0)
        isInvuln = (hit == 0) && (result == GW2::SKILL_RESULT_INVULN ||
                                  result == GW2::SKILL_RESULT_INVULN_0);
    else if (source == 1)
        isInvuln = (result == GW2::CBT_RESULT_ABSORB);
    
    __TRY
    
    if (isInvuln) {
            
        uint32_t agent_id = agent.call<uint32_t>(g_offsets.agentVtGetId.val);
        if (agent_id == 0) return;
        DPSTarget *t = &gameContext->targets.insert_and_evict(agent_id, NULL)->value.data;
        t->aptr = (uintptr_t)agent.data();
        t->agent_id = agent_id;
        t->invuln = true;
        
        Agent_readTarget(t, now);
        
        std::string utf8 = "unresolved";
        if (t->decodedName)
            utf8 = std_utf16_to_utf8((const utf16str)t->decodedName);
        CLogDebug("[%s] INVULNERABLE", utf8.c_str());
    }

    __EXCEPT("[combatLogResultProcess] access violation")
    ;
    __FINALLY
}


#ifdef TARGET_OS_WIN
void __fastcall gameDecodeTextCallback(uint8_t* ctx, wchar_t* decodedText)
#else
void gameDecodeTextCallback(uint8_t* ctx, unsigned char* decodedText)
#endif
{
    __TRY
    
    std::string utf8 = "<unresolved>";
    if (decodedText)
        utf8 = std_utf16_to_utf8((const utf16str)decodedText);
    CLogTrace("[ctx=%p] [text=%s]", ctx, utf8.c_str());
    
    __EXCEPT("[gameDecodeTextCallback] access violation")
    ;
    __FINALLY
}

void dpsReset()
{
    GameContext* gameContext = currentContext();
    memset(&gameContext->controlledPlayer, 0, sizeof(gameContext->controlledPlayer));
    memset(&gameContext->currentDpsTarget, 0, sizeof(gameContext->currentDpsTarget));
    memset(&gameContext->currentTarget, 0, sizeof(gameContext->currentTarget));
    gameContext->closestPlayers.clear();
    gameContext->targets.clear();
    CLogDebug("User called for DPS data reset.");
}

void dpsLockTarget()
{
    DPSTargetEx &target = currentContext()->currentDpsTarget;
    if (target.t.agent_id != 0 && target.t.aptr != 0) {
        target.locked ^= 1;
        CLogDebug("%s target %d <agent:%p)", target.locked ? "Locked" : "Unlocked", target.t.agent_id, target.t.aptr);
    }
}

static int playerDistanceCompare(void const* pItem1, void const* pItem2)
{
    const Player* p1 = *(const Player**)pItem1;
    const Player* p2 = *(const Player**)pItem2;
    
    if (p1->distance < p2->distance) return -1;
    else if (p1->distance > p2->distance) return 1;
    else return 0;
}

bool msgClientDamageUTF8_read(MsgClientDamageUTF8 *msg)
{
    if (!msg) return false;
    if (controlledCharacter()->agent_id == 0) return false;
    
    Player* player = controlledPlayer();
    memset(msg, 0, sizeof(*msg));
    msg->msg_type = MSG_TYPE_CLIENT_DAMAGE_UTF8;
    msg->shard_id = currentShardId();
    msg->selected_id = currentContext()->currentDpsTarget.t.agent_id;
    msg->stats = player->c.stats;
    msg->in_combat = player->c.in_combat;
    msg->profession = player->c.profession;
    msg->time = player->cd.duration;
    msg->damage = (uint32_t)player->cd.damageOut;
    msg->damage_in = (uint32_t)player->cd.damageIn;
    msg->heal_out = (uint32_t)player->cd.healOut;
    
    memcpy(msg->targets, player->dps.targets, sizeof(msg->targets));
    utf16_to_utf8(player->c.name, msg->name, sizeof(msg->name));
    
    static std::vector<Player*> closePlayers;
    closePlayers.clear();
        
    currentContext()->closestPlayers.lock();
    currentContext()->closestPlayers.iter_nolock([](struct CacheEntry<Player>* entry) {
        closePlayers.push_back(&entry->value.data);
    });
        
    if ( closePlayers.size() > 0 )
        qsort(closePlayers.data(), closePlayers.size(), sizeof( closePlayers[0] ), playerDistanceCompare);
    
    size_t count = std::min(closePlayers.size(), (size_t)MSG_CLOSEST_PLAYERS);
    for (size_t i=0; i<count; i++)
        utf16_to_utf8(closePlayers[i]->c.name, msg->closest[i].name, sizeof(msg->closest[i].name));
    
    currentContext()->closestPlayers.unlock();
    
    return true;
}

void msgServerDamageUTF8_handler(MsgServerDamageUTF8* msg)
{
    static Player* player;
    static ServerPlayerUTF8* s;
    GameContext* gameContext = currentContext();
    
    for (int i = 0; i < MSG_CLOSEST_PLAYERS; ++i)
    {
        s = msg->closest + i;
        
        s->name[MSG_NAME_SIZE_UTF8-1] = 0;
        if (s->name[0] == 0)
        {
            continue;
        }
        
        CLogTrace("MSG_TYPE_SERVER_DAMAGE_UTF8: [%d] <%s>", i, s->name);
        
        player = NULL;
        gameContext->closestPlayers.lock();
        gameContext->closestPlayers.iter_nolock([](struct CacheEntry<Player>* entry)
        {
            utf16str name = entry->value.data.c.name[0] ? (utf16str)entry->value.data.c.name : (utf16str)entry->value.data.c.decodedName;
            if (!name || !name[0]) return;

            std::string utf8 = std_utf16_to_utf8(name);
            if (strncasecmp(utf8.c_str(), s->name, sizeof(s->name)) == 0)
                player = &entry->value.data;
        });
        
        if (!player) {
            CLogWarn("Unable to find server player <%s> in close players array", s->name);
            gameContext->closestPlayers.unlock();
            return;
        }
        
        player->dps.is_connected = true;
        player->dps.in_combat = s->in_combat;
        player->dps.time = s->time;
        player->dps.damage = s->damage;
        player->dps.damage_in = s->damage_in;
        player->dps.heal_out = s->heal_out;
        player->dps.stats = s->stats;
        player->dps.profession = s->profession;
        memcpy(player->dps.targets, s->targets, sizeof(player->dps.targets));
        
        for (int j = 0; j < MSG_TARGETS; ++j)
        {
            // Since we hijacked the old c1dmg only value of '1' is valid
            if (player->dps.targets[j].invuln != 1) {
                continue;
            }
            
            CLogTrace("[%s] reported target [id:%d] is invulnerable", s->name, player->dps.targets[j].id);
            
            struct CacheEntry<DPSTarget>* entry = gameContext->targets.find(player->dps.targets[j].id);
            if (entry) {
                DPSTarget* t = &entry->value.data;
                if (!t->invuln) CLogDebug("<%d:%p> is now invuln, source [%s]", t->agent_id, t->aptr, s->name);
                t->invuln = 1;
            }
        }
        
        gameContext->closestPlayers.unlock();
    }
}

bool msgClientUpdateBeginUTF8_read(MsgClientUpdateBeginUTF8 *msg)
{
    if (!msg) return false;
    if (!accountName()) return false;
    
    Player* player = controlledPlayer();
    msg->msg_type = MSG_TYPE_CLIENT_UPDATE_BEGIN_UTF8;
    msg->data_version = -1;
    msg->sequence = arc4random();
    strncpy(msg->version, versionStr(), ARRAYSIZE(msg->version));
    strncpy(msg->acctName, accountName(), ARRAYSIZE(msg->acctName));
    utf16_to_utf8(player->c.name, msg->charName, sizeof(msg->charName));
    
    return true;
}

void msgServerUpdateBegin_handler(MsgServerUpdateBegin* msg)
{
    CLogTrace("MSG_TYPE_SERVER_UPDATE_BEGIN: [seq=%x] [version=%x] [size=%d] [pubkey_len=%d]",
              msg->sequence,
              msg->data_version,
              msg->data_size,
              msg->pubkey_len);
    
    time(&currentContext()->serverPing);
}

#if !(defined BGDM_TOS_COMPLIANT)
static bool gamePatchAddress(uintptr_t addr, uint8_t *orig, uint8_t *patch, size_t size, int mode, bool init)
{
    bool bRet = false;
    static uint8_t buff[128];
    if (addr == 0 || addr == (uintptr_t)(-1)) return false;
    if (!orig || !patch || size == 0) return false;
    
    __TRY
    

    if (init) {
        
        if ( !readProcessMemory(getCurrentProcess(), addr, orig, size, NULL) )
            return false;
        /*else {
            for (int i=0; i<size; ++i)
                CLogDebug("%#x", orig[i]);
        }*/
    }
    
    memset(buff, 0, sizeof(buff));
    if ( !readProcessMemory(getCurrentProcess(), addr, buff, size, NULL) )
        return false;
    
    if (memcmp(buff, orig, size) == 0 ||
        memcmp(buff, patch, size) == 0) {
        
        // 2 - IsPtrValid check
        if (mode == 2) return true;
        
        if (PageProtect((void*)addr, size, PROTECTION_READ_WRITE_EXECUTE))
        {
            bRet = writeProcessMemory(getCurrentProcess(), addr, (mode == 1) ? patch : orig, size, NULL);
            //memcpy((void*)addr, (mode == 1) ? patch : orig, size);
            PageProtect((void*)addr, size, PROTECTION_READ_EXECUTE);
            
        }
        
    } else {
        CLogWarn("unable to verify patch data for address %p", addr);
    }
    
    __EXCEPT ("[gamePatchOffset] access violation")
        ;
    __FINALLY
    
    return bRet;
}

static int s_hpBarsVisibleMode = -1;
static int s_hpBarsHighlightMode = -1;


int gameGetHPBarsVisibleMode()
{
    return s_hpBarsVisibleMode;
}

int gameGetHPBarsHighlightMode()
{
    return s_hpBarsHighlightMode;
}

void gameSetHPBarsVisibleMode(int mode)
{
    // first patch: makes sure the bar is drawn
    // in a non-called brach (should be called but idk...)
    // 0000000000e83d1c 0F95C0                 setne      al
    // 0000000000e83d1c 41B601                 mov r14b, 0x1
    //static uint8_t orig[] = { 0x0F, 0x95, 0xC0 };
    //static uint8_t patch[] = { 0x41, 0xB6, 0x01 };
    static uint8_t orig[] = { 0x0A, 0x45, 0xCC };
    static uint8_t patch[] = { 0x41, 0xB6, 0x01 };
    
    // divert to our patched branch when character->IsPlayer()
    // changed from:
    // 0000000000e83cff 837DBC00               cmp        dword [rbp+var_44], 0x0      ; CODE XREF=sub_e83630+1631
    // 0000000000e83cff 837DCF00               cmp        dword [rbp+var_31], 0x0      ; CODE XREF=sub_e83630+1631
    //static uint8_t orig2[] = { 0x83, 0x7D, 0xBC, 0x00 };
    //static uint8_t patch2[] = { 0x83, 0x7D, 0xCF, 0x00 };
    
    int enable = mode;
    if (mode == -1)
        enable = config_get_int(CONFIG_SECTION_GLOBAL, "HPBarsEnable", 0) > 0;
    
    bool success =  gamePatchAddress(g_offsets.ptrHPBarEnable.addr-0x3, orig, patch, sizeof(orig), enable, 0/*(mode == -1)*/);
    //          && gamePatchAddress(g_offsets.ptrHPBarEnable.addr-0x1D, orig2, patch2, sizeof(orig2), enable, 0/*(mode == -1)*/);
    
    if (success) {
        s_hpBarsVisibleMode = enable;
    }
    
    CLogDebug("gameSetHPBarsVisibleMode(%d, %d): %s", mode, enable, success ? "success" : "fail");
    
    if (mode != -1)
        config_set_int(CONFIG_SECTION_GLOBAL, "HPBarsEnable", mode);
}

void gameSetHPBarsHighlightMode(int mode)
{
    static uint8_t orig[] = { 0x85, 0xC0 };
    static uint8_t patch[] = { 0x85, 0xF8 };
    
    int enable = mode;
    if (mode == -1)
        enable = config_get_int(CONFIG_SECTION_GLOBAL, "HPBarsBrighten", 0) > 0;
    
    bool success = gamePatchAddress(g_offsets.ptrHPBarBrighten.addr, orig, patch, sizeof(orig), enable, 0/*(mode == -1)*/);
    if (success) s_hpBarsHighlightMode = enable;
    
    CLogDebug("gameSetHPBarsHighlightMode(%d, %d): %s", mode, enable, success ? "success" : "fail");
    
    if (mode != -1)
        config_set_int(CONFIG_SECTION_GLOBAL, "HPBarsBrighten", mode);
}

#endif // !(defined BGDM_TOS_COMPLIANT)
