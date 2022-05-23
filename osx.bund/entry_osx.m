//
//  entry_osx.m
//  bgdm
//
//  Created by Bhagwan on 6/30/17.
//  Copyright © 2017 Bhagwan Software. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <objc/runtime.h>
#import "mach_override.h"
#import "Constants.h"
#import "Defines.h"
#import "Logging.h"
#import "BGDMBundSrv.h"
#import "BGDMAppClnt.h"
#import "localization.h"
#import "offset_scan.h"
#import "bgdm_osx.h"
#import "imgui_osx.h"
#import "entry_osx.h"
#import "core/utf.h"
#import "core/segv.h"
#import "core/time_.h"
#import "core/network.h"
#import "hook/hook2.h"
#import "meter/config.h"
#import "meter/autolock.h"
#import "meter/lru_cache.h"
#import "meter/game_data.h"
#import "meter/dps.h"


static int g_init_state = 0;
static int g_is_main_thread = 0;
static int g_shardId = 0;
static int g_mapType = 0;
static uintptr_t g_tlsContext = 0;
static uintptr_t g_charClientCtx = 0;
static uintptr_t g_gadgetCtx = 0;
static uintptr_t g_contactCtx = 0;
static uintptr_t g_squadCtx = 0;
static uintptr_t g_chatCtx = 0;
static uintptr_t g_wmAgentArray = 0;
static NSString* g_accountName = nil;
static NSString* g_bgdmVersion = nil;
static CamData g_camData = { 0 };


// mod state
State g_state = { 0 };


static void except_handler(const char *exception)
{
    CLogCrit(@"Unhandled exception %s!", exception);
}

uintptr_t TlsContext()
{
    return g_tlsContext;
}

uintptr_t CharClientCtx()
{
    return g_charClientCtx;
}

uintptr_t GadgetCtx()
{
    return g_gadgetCtx;
}

uintptr_t ContactCtx()
{
    return g_contactCtx;
}

uintptr_t SquadCtx()
{
    return g_squadCtx;
}

uintptr_t ChatCtx()
{
    return g_chatCtx;
}

uintptr_t WorldViewCtx()
{
    __TRY
    if (g_offsets.viewWorldView.addr)
    {
        return *(uintptr_t*)g_offsets.viewWorldView.addr;
    }
    __EXCEPT("[WorldViewCtx] access violation")
    ;
    __FINALLY
    return 0;
}

uintptr_t WmAgentArray()
{
    return g_wmAgentArray;
}

int currentShardId()
{
    return g_shardId;
}

int currentMapType()
{
    return g_mapType;
}

CamData* currentCam()
{
    return &g_camData;
}


uintptr_t execTlsContext()
{
    __TRY
    GameOffsets *m = &g_offsets;
    if (m->fncGetTlsContext.addr) {
        g_tlsContext = (uintptr_t) ((void*(*)( void ))m->fncGetTlsContext.addr)();
        return g_tlsContext;
    }
    __EXCEPT( "execTlsContext()" )
    __FINALLY
    return 0;
}

uintptr_t execCharClientCtx()
{
    __TRY
    GameOffsets *m = &g_offsets;
    if (m->fncCharClientContext.addr) {
        g_charClientCtx = (uintptr_t) ((void*(*)( void ))m->fncCharClientContext.addr)();
        return g_charClientCtx;
    }
    __EXCEPT( "execCharClientCtx()" )
    __FINALLY
    return 0;
}

uintptr_t execGadgetCtx()
{
    __TRY
    GameOffsets *m = &g_offsets;
    if (m->fncGadgetContext.addr) {
        g_gadgetCtx = (uintptr_t) ((void*(*)( void ))m->fncGadgetContext.addr)();
        return g_gadgetCtx;
    }
    __EXCEPT( "execGadgetCtx()" )
    __FINALLY
    return 0;
}

uintptr_t execContactCtx()
{
    __TRY
    GameOffsets *m = &g_offsets;
    if (m->fncGetContactCtx.addr) {
        g_contactCtx = (uintptr_t) ((void*(*)( void ))m->fncGetContactCtx.addr)();
        return g_contactCtx;
    }
    __EXCEPT( "execContactCtx()" )
    __FINALLY
    return 0;
}

uintptr_t execSquadCtx()
{
    __TRY
    GameOffsets *m = &g_offsets;
    if (m->fncGetSquadCtx.addr) {
        g_squadCtx = (uintptr_t) ((void*(*)( void ))m->fncGetSquadCtx.addr)();
        return g_squadCtx;
    }
    __EXCEPT( "execSquadCtx()" )
    __FINALLY
    return 0;
}

uintptr_t execChatCtx()
{
    __TRY
    GameOffsets *m = &g_offsets;
    if (m->fncGetChatCtx.addr) {
        g_chatCtx = (uintptr_t) ((void*(*)( void ))m->fncGetChatCtx.addr)();
        return g_chatCtx;
    }
    __EXCEPT( "execChatCtx()" )
    __FINALLY
    return 0;
}

uintptr_t execGetWmAgentArray()
{
    __TRY
    GameOffsets *m = &g_offsets;
    if (m->fncGetWmAgentArray.addr) {
        g_wmAgentArray = (uintptr_t) ((void*(*)( void ))m->fncGetWmAgentArray.addr)();
        return g_wmAgentArray;
    }
    __EXCEPT( "execGetWmAgentArray()" )
    __FINALLY
    return 0;
}



int execGetShardId()
{
    __TRY
    GameOffsets *m = &g_offsets;
    if (m->fncGetShardId.addr) {
        g_shardId = ((int (*)( void ))m->fncGetShardId.addr)();
        return g_shardId;
    }
    __EXCEPT( "execGetShardId()" )
    __FINALLY
    return 0;
}

int execGetMapType()
{
    __TRY
    GameOffsets *m = &g_offsets;
    if (m->fncGetMapDef.addr && m->mapDefType.val) {
        void* mapDef = ((void* (*)( void ))m->fncGetMapDef.addr)();
        if (mapDef) {
            g_mapType = *(int*)(mapDef+m->mapDefType.val);
            return g_mapType;
        }
    }
    __EXCEPT( "execGetMapType()" )
    __FINALLY
    return 0;
}

int currentPing()
{
    __TRY
    GameOffsets *m = &g_offsets;
    if (m->pPingCtx.addr) {
        return *(int*)m->pPingCtx.addr;
    }
    __EXCEPT( "currentPing()" )
    __FINALLY
    return 0;
}

int currentFPS()
{
    __TRY
    GameOffsets *m = &g_offsets;
    if (m->pFpsCtx.addr) {
        return *(int*)m->pFpsCtx.addr;
    }
    __EXCEPT( "currentFPS()" )
    __FINALLY
    return 0;
}

int currentMapId()
{
    __TRY
    GameOffsets *m = &g_offsets;
    if (m->pMapId.addr) {
        return *(int*)m->pMapId.addr;
    }
    __EXCEPT( "currentMapId()" )
    __FINALLY
    return 0;
}

int isMapOpen()
{
    __TRY
    GameOffsets *m = &g_offsets;
    if (m->pMapOpen.addr) {
        return (*(int*)m->pMapOpen.addr == 1);
    }
    __EXCEPT( "isMapOpen()" )
    __FINALLY
    return 0;
}

int isInterfaceHidden() {
    
    __TRY
    GameOffsets *m = &g_offsets;
    if (m->pIfHide.addr) {
        int ifHide = *(int*)m->pIfHide.addr;
        return (ifHide == 1 || ifHide == -1);
    }
    __EXCEPT( "isInterfaceHidden()" )
    __FINALLY
    return 0;
}

int isInCutscene() {
    
    __TRY
    GameOffsets *m = &g_offsets;
    if (m->viewAgentSelect.addr && g_offsets.agSelCtxMode.addr) {
        
        int asCtxMode = *(int*)(g_offsets.agSelCtxMode.addr+g_offsets.agSelCtxMode.val);
        return (asCtxMode == 1);
    }
    __EXCEPT( "isInCutscene()" )
    __FINALLY
    return 0;
}

int isActionCam()
{
    __TRY
    GameOffsets *m = &g_offsets;
    if (m->pActionCam.addr) {
        return *(int*)m->pActionCam.addr;
    }
    __EXCEPT( "isActionCam()" )
    __FINALLY
    return 0;
}

int uiInterfaceSize()
{
    __TRY
    GameOffsets *m = &g_offsets;
    if (m->viewUi.addr) {
        return *(int*)(m->viewUi.addr + m->uiIntSize.val);
    }
    __EXCEPT( "uiInterfaceSize()" )
    __FINALLY
    return 0;
}

bool uiOptionTest(int opt)
{
    __TRY
    GameOffsets *m = &g_offsets;
    if (m->viewUi.addr) {
        int uiFlags1 = *(int*)(m->viewUi.addr + m->uiFlags1.val);
        int uiFlags2 = *(int*)(m->viewUi.addr + m->uiFlags2.val);
        
        return !!(opt < 32 ?
                  uiFlags1 & (1 << opt) :
                  uiFlags2 & (1 << (opt - 32)));
    }
    __EXCEPT( "uiOptionTest(int)" )
    __FINALLY
    return false;
}

const char *accountName()
{
    if (g_accountName != nil)
        return [g_accountName UTF8String];
    
    if (g_contactCtx) {
        char buff[PATH_MAX] = {0};
        uintptr_t contact = CtxContact_GetContactSelf(g_contactCtx);
        if (contact) {
            const char* acccountNameUTF16 = Contact_GetAccountName(contact);
            if (acccountNameUTF16) {
                utf16_to_utf8((void*)acccountNameUTF16+2, buff, sizeof(buff));
                g_accountName = [NSString stringWithUTF8String:buff];
                CLogTrace(@"acccountNameUTF16:%@  <contact:%p> <addr:%p>", g_accountName, (void*)contact, acccountNameUTF16);
                return [g_accountName UTF8String];
            }
        }
    }
    return NULL;
}

const char *versionStr()
{
    if (g_bgdmVersion == nil) {
        NSString *bundleVersion = [BGDMosx bundleVersion];
        g_bgdmVersion = [NSString stringWithFormat:@"%@_osx", bundleVersion];
#if (defined _DEBUG) || (defined DEBUG)
        g_bgdmVersion = [g_bgdmVersion stringByAppendingString:@"_dbg"];
#elif !(defined BGDM_TOS_COMPLIANT)
        g_bgdmVersion = [g_bgdmVersion stringByAppendingString:@"_cn"];
#else
        g_bgdmVersion = [g_bgdmVersion stringByAppendingString:@"_na"];
#endif
    }
    return [g_bgdmVersion UTF8String];
}

bool isCurrentMapPvPorWvW()
{
    static u32 const pvp_maps[] = {
        MAP_TYPE_PVP,
        MAP_TYPE_GVG,
        MAP_TYPE_TOURNAMENT,
        MAP_TYPE_WVW_EB,
        MAP_TYPE_WVW_BB,
        MAP_TYPE_WVW_GB,
        MAP_TYPE_WVW_RB,
        MAP_TYPE_WVW_FV,
        MAP_TYPE_WVW_OS,
        MAP_TYPE_WVW_EOTM,
        MAP_TYPE_WVW_EOTM2
    };
    
#if (defined _DEBUG) || (defined DEBUG)
    // We only bypass the competitve mode for debugging purposes
    // this can't be shipped with release code
    return false;
#endif
    
    // Test for competitive maps
    for (u32 i = 0; i < ARRAYSIZE(pvp_maps); ++i)
    {
        if (currentMapType() == pvp_maps[i])
        {
            return true;
        }
    }
    
    return false;
}

bool isInMainThread()
{
    return g_is_main_thread > 0;
}

void gameThread_detour(CpuContext *ctx)
{
    /*uintptr_t *pInst = (uintptr_t*)(ctx->RDI);
    int frame_time = (int)(ctx->RDX);
    int a2 = (int)ctx->RSI;*/
    
    // Can only continue if BGDM is initialized fully
    if (g_init_state != InitStateInitialized)
        return;
    
    // Signal we are in main thread
    // we use this to skip crashing calls called outside of main thread context
    g_is_main_thread = 1;
    
    // Can only get those from the main thread
    // otherwise we crash at a pthread routine
    execTlsContext();
    execCharClientCtx();
    execGetWmAgentArray();
    execGadgetCtx();
    execContactCtx();
    execSquadCtx();
    execChatCtx();
    execGetShardId();
    execGetMapType();
    GetCamData(&g_camData);
    
    // Update game data
    game_update(time_get());
    
    // Send missing IDs/cptr's for resolving
    // Can only be done under main thread context
    lru_resolve();
    
    // No longer in main thread
    // control is returned to game
    g_is_main_thread = 0;
}

/*static inline void scanCheck32(char const* name, uintptr_t addr, uintptr_t rva, uintptr_t rvap)
{
    __TRY
    int32_t val = *(int32_t*)(addr+rva);
    if ((val > 100 && val < 10000))
        CLogDebug(@"[%#lx] [%s:%p:%#lx] <%d>", rvap, name, (void*)addr, rva, val);
    __EXCEPT("scanCheck32")
    __FINALLY
}

static inline void scanCheck64(char const* name, uintptr_t addr, uintptr_t rva, uintptr_t rvap)
{
    __TRY
    uintptr_t val = *(uintptr_t*)(addr+rva);
    if ((uintptr_t)val == controlledCharacter()->aptr)
        CLogDebug(@"[%#lx] [%s:%p:%#lx] source found <%p>", rvap, name, (void*)addr, rva, (void*)val);
    if ((uintptr_t)val == currentTarget()->aptr)
        CLogDebug(@"[%#lx] [%s:%p:%#lx] target found <%p>", rvap, name, (void*)addr, rva, (void*)val);
    
    if (val) {
        uint32_t effect = *(uint32_t*)(val + OFF_SKILLDEF_EFFECT);
        uint32_t hash = *(uint32_t*)(val + OFF_SKILLDEF_HASHID);
        if (effect == 736 || effect == 10561)
            CLogDebug(@"[%#lx] [%s:%p:%#lx] skill found <%p> effect %d hash %#x", rvap, name, (void*)addr, rva, (void*)val, effect, hash);
    }
    
    __EXCEPT("scanCheck64")
    __FINALLY
}

static inline void scanRegister(char const* name, uintptr_t addr, int max_i, int max_j, bool scan32, bool scan64)
{
    for (int i=0; i<max_i; i++) {
        if (scan32) scanCheck32(name, addr, i*sizeof(uint32_t), -1);
        if (scan64) scanCheck64(name, addr, i*sizeof(uint64_t), -1);
    }
    
    for (int i=0; i<max_i; i++) {
        __TRY
        uintptr_t nextaddr = *(uintptr_t*)(addr + i*sizeof(uintptr_t));
        for (int j=0; j<max_j; j++) {
            if (scan32) scanCheck32(name, nextaddr, j*sizeof(uint32_t), i*sizeof(uint32_t));
            if (scan64) scanCheck64(name, nextaddr, j*sizeof(uint64_t), i*sizeof(uint64_t));
        }
        __EXCEPT("scanRegister")
        __FINALLY
    }
}*/


void combatLog_detour(CpuContext *ctx)
{
    static char* const CombatLogTypeStr[] = {
        "CL_CONDI_DMG_IN",
        "CL_CRIT_DMG_IN",
        "CL_GLANCE_DMG_IN",
        "CL_HEAL_IN",
        "CL_PHYS_DMG_IN",
        "CL_UNKNOWN_5",
        "CL_UNKNOWN_6",
        "CL_CONDI_DMG_OUT",
        "CL_CRIT_DMG_OUT",
        "CL_GLANCE_DMG_OUT",
        "CL_HEAL_OUT",
        "CL_PHYS_DMG_OUT",
        "CL_UNKNOWN_12",
        "CL_UNKNOWN_13"
    };
    
    
    __TRY
    
    //scanRegister("RSP", ctx->RSP, 100, 20, true, false);
    
    // R14+0x20 = RDI from areaLog_detour
    uint32_t type = (uint32_t)(ctx->RDX);
    uint32_t hit = *(uint32_t*)(ctx->RSP + 0x1c);
    uintptr_t aptr_src = *(uintptr_t*) ((*(uintptr_t*)(ctx->R14 + 0x40)) + 0x40);
    uintptr_t aptr_tgt = *(uintptr_t*) ((*(uintptr_t*)(ctx->R14 + 0x40)) + 0x58);
    uintptr_t skillDef = *(uintptr_t*) ((*(uintptr_t*)(ctx->R14 + 0x40)) + 0x48);
    //uintptr_t aptr_src = *(uintptr_t*)(ctx->RBP + 0x30);
    //uintptr_t skillDef = *(uintptr_t*) ((*(uintptr_t*)(ctx->RBP)) + 0x28);
    //uintptr_t aptr_tgt = *(uintptr_t*) ((*(uintptr_t*)(ctx->RBP + 0x38)) + 0x50);
    
    combatLogProcess(type, hit, aptr_src, aptr_tgt, skillDef);
    
    CLogTrace(@"%s(%d): hit=%d [source:%p] [target:%p] [skillDef:%p]",
              (type<0xE) ? CombatLogTypeStr[type] : "UNKNOWN", type,
              hit, (void*)aptr_src, (void*)aptr_tgt, (void*)skillDef);
    
#if (defined _DEBUG) || (defined DEBUG)
        __TRY
            if (skillDef) {
                static char buff[128];
                memset(buff, 0, sizeof(buff));
                const uint8_t* name = lru_find(CLASS_TYPE_SKILL, skillDef, NULL, 0);
                if (name) utf16_to_utf8((void*)name, buff, sizeof(buff));
                CLogTrace(@"%s(%d): [source:%p] hits [target:%p] for %d using [%d:%d:%s]",
                          (type<0xE) ? CombatLogTypeStr[type] : "UNKNOWN", type,
                          (void*)aptr_src, (void*)aptr_tgt, hit,
                          *(uint32_t*)(skillDef + OFF_SKILLDEF_EFFECT),
                          *(uint32_t*)(skillDef + OFF_SKILLDEF_HASHID),
                          buff);
            }
        __EXCEPT( "combatLog_detour skillDef" )
        __FINALLY
#endif
    
    __EXCEPT( "combatLog_detour" )
    __FINALLY
}

void combatLogResult_detour(CpuContext *ctx)
{
    __TRY
    
    int result = (int)(ctx->RSI);
    int dmg = *(int*)(ctx->RBP+0xA4);
    //int dmg = *(int*)((*(uintptr_t*)ctx->RBP) + 0x54);
    
    // RBP+0xA8 = RDI from areaLog_detour
    uintptr_t aptr_src = *(uintptr_t*) ((*(uintptr_t*)(ctx->RBP + 0xA8)) + 0x40);
    uintptr_t aptr_tgt = *(uintptr_t*) ((*(uintptr_t*)(ctx->RBP + 0xA8)) + 0x58);
    uintptr_t skillDef = *(uintptr_t*) ((*(uintptr_t*)(ctx->RBP + 0xA8)) + 0x48);
    
    combatLogResultProcess(result, dmg, aptr_src, aptr_tgt, skillDef, 0);
    
    CLogTrace(@"[result:%d] [dmg:%d] [source:%p] [target:%p] [skillDef:%p]",
              result, dmg, (void*)aptr_src, (void*)aptr_tgt, (void*)skillDef);
    
    __EXCEPT( "combatLogResult_detour" )
    __FINALLY
}

void areaLog_detour(CpuContext *ctx)
{
    __TRY
    
    // This is the hit address but it doesn't get filled later on
    // right before the call to combatLog_detour
    //int hit = *(int*)(ctx->RSP - 0x7C);
    
    // Result also found in RDX+0x24
    uint32_t result = *(uint32_t*)(ctx->RDI + 0x2C);
    uintptr_t aptr_src = *(uintptr_t*)(ctx->RDI + 0x40);
    uintptr_t aptr_tgt = *(uintptr_t*)(ctx->RDI + 0x58);
    uintptr_t skillDef = *(uintptr_t*)(ctx->RDI + 0x48);
    
    // we get here events from other users as well
    // skip anything that isn't ours
    if (aptr_src == 0 || aptr_tgt == 0) return;
    if (aptr_src != controlledCharacter()->aptr && aptr_tgt != controlledCharacter()->aptr) return;
    
    combatLogResultProcess(result, 0, aptr_src, aptr_tgt, skillDef, 1);
    
    CLogTrace(@"[result:%d] [source:%p] [target:%p] [skillDef:%p] [skillId:%d]",
              result, (void*)aptr_src, (void*)aptr_tgt, (void*)skillDef,
              skillDef ? *(int*)(skillDef+OFF_SKILLDEF_EFFECT): 0);
    
    //scanRegister("areaLog_detour:RDI", ctx->RDI, 50, 50, false, true);
    
    __EXCEPT( "areaLog_detour" )
    __FINALLY
}

int bgdm_init_state() {
    
    return g_init_state;
}


static bool inline detourFunction(const char *name, uintptr_t addr, HookCallback_t cbHook)
{
    if (!hookDetour(addr, cbHook))
    {
        CLogError(@"Unable to hook %s at addr %lx", name, addr);
        return false;
    }
    CLogDebug(@"Successfully hooked %s at addr %lx", name, addr);
    return true;
}

void bgdm_netsend_thread()
{
    static MsgClientDamageUTF8 damageMsg;
    static MsgClientUpdateBeginUTF8 clntUpdateMessage;
    for (;;)
    {
        // Sleep for 0.5 seconds
        [NSThread sleepForTimeInterval:0.5f];
        
        if (g_init_state == InitStateError ||
            g_init_state == InitStateShutdown ||
            g_init_state == InitStateShuttingDown) {
            
            CLogDebug(@"netsend: aborted thread");
            break;
        }
        
        if (g_init_state != InitStateInitialized) {
            // Sleep for 0.5 seconds
            [NSThread sleepForTimeInterval:0.5f];
            continue;
        }
        
        // Send our information to the server
        if (msgClientDamageUTF8_read(&damageMsg)) {
            if ( !network_send(&damageMsg, sizeof(damageMsg), 0, 0) ) {
                CLogCrit(@"net: sendto(MsgClientDamageUTF8) failed, error 0x%x", errno);
            }
        }
        
        if (msgClientUpdateBeginUTF8_read(&clntUpdateMessage)) {
            if ( !network_send(&clntUpdateMessage, sizeof(clntUpdateMessage), 0, 0) ) {
                CLogCrit(@"net: sendto(MsgClientUpdateBeginUTF8) failed, error 0x%x", errno);
            }
        }
    }
    
    CLogDebug(@"netsend: shutdown");
}

void bgdm_netrecv_thread()
{
    for (;;)
    {
        // Sleep for 0.5 seconds
        [NSThread sleepForTimeInterval:0.5f];
        
        if (g_init_state == InitStateError ||
            g_init_state == InitStateShutdown ||
            g_init_state == InitStateShuttingDown) {
            
            CLogDebug(@"netrecv: aborted thread");
            break;
        }
        
        if (g_init_state != InitStateInitialized) {
            // Sleep for 0.5 seconds
            [NSThread sleepForTimeInterval:0.5f];
            continue;
        }
        
        for (;;)
        {
            // Receive queued network messages.
            static i8 buf[UDP_MAX_PKT_SIZE];
            i32 len = network_recv(buf, sizeof(buf), NULL, NULL);
            if (len <= 0) {
                if (len < 0 && errno != EWOULDBLOCK && errno != EAGAIN)
                    CLogCrit(@"net: recvfrom() failed, error 0x%x", errno);
                break;
            }
            
            uint32_t type = *(uint32_t*)buf;
            switch (type)
            {
                case MSG_TYPE_SERVER_DAMAGE_UTF8:
                {
                    // Received a damage update message from the server.
                    if (len != sizeof(MsgServerDamageUTF8))
                    {
                        break;
                    }
                    
                    msgServerDamageUTF8_handler((MsgServerDamageUTF8*)buf);
                } break;
                    
                case MSG_TYPE_SERVER_UPDATE_BEGIN:
                {
                    // Received an update notification from the server.
                    if (len != sizeof(MsgServerUpdateBegin))
                    {
                        break;
                    }
                    
                    msgServerUpdateBegin_handler((MsgServerUpdateBegin*)buf);
                } break;
                    
                default:
                    CLogWarn(@"Unknown message received from server <id:%d> <len:%d>", type, len);
                    break;
            }
        }
    }
    
    CLogDebug(@"netrecv: shutdown");
}

bool bgdm_network_init()
{
    if (g_state.network_port == 0)
        return false;
    
    CLogDebug(@"net: initializing... <%s:%d>", g_state.network_addr, g_state.network_port);
    
    if ( !network_create(g_state.network_addr, g_state.network_port) ) {
        CLogCrit(@"net: bind() failed, error 0x%x", errno);
        return false;
    }
    CLogDebug(@"net: initialized succesfully.");
    
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_BACKGROUND, 0), ^{
        bgdm_netsend_thread();
    });
    
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_BACKGROUND, 0), ^{
        bgdm_netrecv_thread();
    });
    
    return true;
}

void bgdm_init()
{
    CLogDebug(@"Initializing BGDM...");

    g_init_state = InitStateNotInitializing;
    GameOffsets *m = &g_offsets;
    
    NSString * iniFilePath = [BGDMosx pathForResource:kBGDMIniFile];
    NSString * cnFilePath = [BGDMosx pathForResource:kBGDMCNFile];
    NSString * langFolderPath = [NSString stringWithFormat:@"%@/%@/%@", kDocumentsDirectory, kBGDMDocFolderPath, kBGDMLangFolderPath];
    
    config_init([iniFilePath UTF8String]);
    localtext_init([langFolderPath UTF8String], g_state.lang_file, [cnFilePath UTF8String]);
    time_create();
    lru_create();
    autolock_init();
    
    if ( hookInit() != err_none ) {
        CLogError(@"Unable to initialize hook library");
         goto error;
    }
    
    if ( !scanOffsets(0, [kGW2ModuleName UTF8String], g_state.panel_debug.enabled) ) {
        
        goto error;
    }
    
    CLogDebug(@"Hooking game functions...");
    
    if ( !detourFunction("gameThread", m->fncGameMainThread.addr, gameThread_detour) ) goto error;
    
    // Area log is good for invln but we don't get the hit num until later
    if ( !detourFunction("areaLog", m->fncAreaLog.addr, areaLog_detour) ) goto error;
    if ( !detourFunction("combatLog", m->fncCombatLog.addr, combatLog_detour) ) goto error;
    
    // NOT NEEDED, we get the result from areaLog
    //if ( !detourFunction("combatLogResult", m->fncCombatLogResult.addr, combatLogResult_detour) ) goto error;
    
    if (!game_init())
        goto error;
    
    // Ignore network init error
    bgdm_network_init();

success:
    g_init_state = InitStateInitialized;
    CLogDebug(@"BGDM Succesfully initialized");
    return;
    
error:
    g_init_state = InitStateError;
    CLogError(@"Error initliazing BGDM!");
}

void bgdm_fini()
{
    CLogDebug(@"Uninitializing BGDM...");
    g_init_state = InitStateShuttingDown;
    
    network_destroy();
    hookFini();
    game_fini();
    
#if defined(TARGET_OS_WIN)
    updater_destroy();
    crypto_destroy();
    mumble_link_destroy();
#endif
    autolock_free();
    lru_destroy();
    config_fini();
    
    CLogDebug(@"BGDM Succesfully uninitialized");
    g_init_state = InitStateShutdown;
}
