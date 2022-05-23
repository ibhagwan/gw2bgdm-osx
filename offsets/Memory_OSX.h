//
//  Memory_OSX.h
//  bgdm
//
//  Created by Bhagwan on 6/27/17.
//  Copyright © 2017 Bhagwan Software. All rights reserved.
//

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int pidFromProcessName(const char *name);
    
void * getCurrentProcess();
void *openProcess(int pid, void **handle);
void closeHandle(void *handle);
bool readProcessMemory(void *handle, uintptr_t addr, uint8_t* outData, size_t size, uint32_t *bytesRead);
bool writeProcessMemory(void *handle, uintptr_t addr, uint8_t* data, size_t size, uint32_t *bytesWritten);
unsigned char * vmReadProcessMemory(void *handle, uintptr_t addr, size_t *size);
    
unsigned int getMemRegions(void *handle, void *ctx, void(*regionCb)(void *ctx, uintptr_t base, size_t size, int prot, const char *module));

#ifdef __cplusplus
}
#endif
