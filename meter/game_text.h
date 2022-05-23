//
//  game_text.h
//  bgdm
//
//  Created by Bhagwan on 7/13/17.
//  Copyright © 2017 Bhagwan Software. All rights reserved.
//

#ifndef game_text_h
#define game_text_h
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


#ifdef TARGET_OS_WIN
typedef void(__fastcall *cbDecodeText_t)(uint8_t*, wchar_t*);
void __fastcall gameDecodeTextCallback(uint8_t* ctx, wchar_t* decodedText);
#else
typedef void(*cbDecodeText_t)(uint8_t*, unsigned char*);
void gameDecodeTextCallback(uint8_t* ctx, unsigned char* decodedText);
#endif
    
uint8_t* CodedTextFromHashId(uint32_t hashId, uint32_t a2);
bool DecodeText(uint8_t* codedText, cbDecodeText_t cbDecodeText, uintptr_t ctx);
uint32_t HashIdFromPtr(int type, uintptr_t ptr);

bool Agent_GetCodedName(uintptr_t aptr, cbDecodeText_t cbDecodeText, uintptr_t ctx);
bool WmAgent_GetCodedName(uintptr_t wmptr, cbDecodeText_t cbDecodeText, uintptr_t ctx);
    
#ifdef __cplusplus
}
#endif


#endif /* game_text_h */
