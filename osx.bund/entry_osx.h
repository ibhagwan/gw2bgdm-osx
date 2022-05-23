//
//  entry_osx.h
//  bgdm
//
//  Created by Bhagwan on 6/30/17.
//  Copyright © 2017 Bhagwan Software. All rights reserved.
//


#ifdef __cplusplus
extern "C" {
#endif
    
extern struct State g_state;
    
enum BGDMInitState {
        InitStateNotInitialized,
        InitStateNotInitializing,
        InitStateInitialized,
        InitStateError,
        InitStateShuttingDown,
        InitStateShutdown,
};
    
void    bgdm_init();
void    bgdm_fini();
int     bgdm_init_state();


bool isInMainThread();
uintptr_t TlsContext();
uintptr_t CharClientCtx();
uintptr_t GadgetCtx();
uintptr_t ContactCtx();
uintptr_t SquadCtx();
uintptr_t ChatCtx();
uintptr_t WorldViewCtx();
uintptr_t WmAgentArray();
    
int currentPing();
int currentFPS();
int currentMapId();
int currentMapType();
int currentShardId();
int isMapOpen();
int isInterfaceHidden();
int isInCutscene();
int isActionCam();
int uiInterfaceSize();
bool uiOptionTest(int opt);
bool isCurrentMapPvPorWvW();
const char* accountName();
const char* versionStr();
    
struct __CamData;
struct __CamData* currentCam();

#ifdef __cplusplus
}
#endif
