//
//  game_data.h
//  bgdm
//
//  Created by Bhagwan on 7/9/17.
//  Copyright © 2017 Bhagwan Software. All rights reserved.
//

#ifndef game_data_h
#define game_data_h

#include <stdint.h>
#include <string.h>
#ifdef __APPLE__
#include <mach/error.h>
#endif
#include "meter/game.h"

#if (defined __cplusplus)
#include <type_traits> // std::conditional
#pragma warning( push )
#pragma warning( disable : 4127 )
namespace ANet
{
    template <typename T>
    struct Array {
        T *m_basePtr;
        uint32_t m_capacity;
        uint32_t m_count;
    };
    
    template <typename T>
    struct Dictionary {
        uint32_t m_capacity;
        uint32_t m_count;
        T *m_basePtr;
    };
    
    template <typename T, bool IsArray = true>
    class Collection : private std::conditional<IsArray, Array<T>, Dictionary<T>>::type {
    public:
        Collection<T>() {
            this->m_basePtr = NULL;
            this->m_capacity = 0;
            this->m_count = 0;
        }
        Collection<T> &operator= (const Collection<T> &a) {
            if (this != &a) {
                this->m_basePtr = a.m_basePtr;
                this->m_capacity = a.m_capacity;
                this->m_count = a.m_count;
            }
            return *this;
        }
        T &operator[] (uint32_t index) {
            if (IsArray && index < count()) {
                return this->m_basePtr[index];
            }
            else if (index < capacity()) {
                return this->m_basePtr[index];
            }
#ifdef __APPLE__
            throw err_local;
#else
            throw STATUS_ARRAY_BOUNDS_EXCEEDED;
#endif
        }
        bool isValid() {
            return (this->m_basePtr != 0) && (this->m_capacity > 0) && (this->m_count <= this->m_capacity);
        }
        uint32_t count() {
            return this->m_count;
        }
        uint32_t capacity() {
            return this->m_capacity;
        }
        uint32_t size() {
            return this->m_capacity;
        }
        uintptr_t data() {
            return (uintptr_t)this->m_basePtr;
        }
    };
};
#pragma warning( pop )
#include <map>
#include "meter/cache.hpp"
struct __GameContext
{
    ANet::Collection<uintptr_t> wmAgentArray;
    ANet::Collection<uintptr_t> characterArray;
    ANet::Collection<uintptr_t> playerArray;
    Player controlledPlayer;
    Character currentTarget;
    DPSTargetEx currentDpsTarget;
    Cache<Player> closestPlayers;
    Cache<DPSTarget> targets;
    PortalData portalData;
    time_t serverPing;
};
#else
struct __GameContext;
#endif	// __cplusplus

typedef struct __GameContext GameContext;

#ifdef __cplusplus
extern "C" {
#endif
    
struct Player;
struct Character;
    
GameContext* currentContext();
Player *controlledPlayer();
Character *controlledCharacter();
Character *currentTarget();
uintptr_t controlledAgChar();
    
bool game_init(void);
void game_fini(void);
    
void game_update(int64_t now);
    
struct __CamData;
struct __CompassData;
bool GetCamData(struct __CamData* pCamData);
bool GetCompassData(struct __CompassData* pCompData);
    
float CalcDistance(vec3 p, vec3 q, bool isTransform);
bool AgentPosProject2D(vec3 apos, vec2 *outPos);
bool TransportPosProject2D(vec3 tpos, vec2 *outPos);
    
uintptr_t CtxContact_GetContactSelf(uintptr_t pctx);
const void* Contact_GetAccountName(uintptr_t ctptr);
    
bool Ch_GetSpeciesDef(uintptr_t cptr, uintptr_t *pSpeciesDef);
bool Ch_GetInventory(uintptr_t iptr, EquipItems *equipItems);
bool Pl_GetSpec(uintptr_t pptr, Spec *outSpec);
    
void combatLogProcess(int type, int hit, uintptr_t aptr_src, uintptr_t aptr_tgt, uintptr_t skillDef);
void combatLogResultProcess(int result, int hit, uintptr_t aptr_src, uintptr_t aptr_tgt, uintptr_t skillDef, int source);
    
void dpsReset();
void dpsLockTarget();
    
bool msgClientDamageUTF8_read(MsgClientDamageUTF8 *msg);
bool msgClientUpdateBeginUTF8_read(MsgClientUpdateBeginUTF8 *msg);
void msgServerDamageUTF8_handler(MsgServerDamageUTF8* msg);
void msgServerUpdateBegin_handler(MsgServerUpdateBegin* msg);
   
#if !(defined BGDM_TOS_COMPLIANT)
int gameGetHPBarsVisibleMode();
int gameGetHPBarsHighlightMode();
void gameSetHPBarsVisibleMode(int mode);
void gameSetHPBarsHighlightMode(int mode);
#endif

#ifdef __cplusplus
}
#endif


#endif /* game_data_h */
