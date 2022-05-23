//
//  Memory_OSX.m
//  bgdm
//
//  Created by Bhagwan on 6/27/17.
//  Copyright © 2017 Bhagwan Software. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <stdio.h>
#import <mach/mach_init.h>
#import <mach/mach_vm.h>
#import <mach/mach.h>
#import <sys/sysctl.h>
#import <sys/stat.h>
#import "Memory_OSX.h"
#import "BSDProcess.h"
#import "osx.common/Logging.h"

#include <mach-o/dyld.h>
#include <mach-o/getsect.h>
#include <mach-o/dyld_images.h>

static uint64_t staticBaseAddress(void)
{
    const struct segment_command_64* command = getsegbyname("__TEXT");
    uint64_t addr = command->vmaddr;
    return addr;
}

intptr_t imageSlide(void)
{
    char path[1024];
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) != 0) return -1;
    for (uint32_t i = 0; i < _dyld_image_count(); i++)
    {
        if (strcmp(_dyld_get_image_name(i), path) == 0)
            return _dyld_get_image_vmaddr_slide(i);
    }
    return 0;
}

uintptr_t procBaseAddressSelf()
{
    return staticBaseAddress() + imageSlide();
}

uintptr_t procBaseAddress(void* handle)
{
    mach_port_name_t task = (mach_port_name_t)handle;
    vm_map_offset_t vmoffset;
    vm_map_size_t vmsize;
    uint32_t nesting_depth = 0;
    struct vm_region_submap_info_64 vbr;
    mach_msg_type_number_t vbrcount = 16;
    
    kern_return_t kr = mach_vm_region_recurse(task, &vmoffset, &vmsize,
                                              &nesting_depth,
                                              (vm_region_recurse_info_t)&vbr,
                                              &vbrcount);
    
    if (kr != KERN_SUCCESS)
        return -1;
    return vmoffset;
}

int pidFromProcessName(const char *name)
{
    NSDictionary *dict = [BSDProcess processInfoProcessName:[NSString stringWithUTF8String:name]];
    if (dict != nil) {
        NSNumber *procId = [dict objectForKey:@"processID"];
        if (procId)
            return procId.intValue;
    }
    return 0;
}

void* openProcess(int pid, void **handle)
{
    if (!handle) return NULL;
    
    mach_port_name_t task;
    kern_return_t kr = task_for_pid(mach_task_self(), pid, &task);
    if (kr == KERN_SUCCESS) {
        CLogDebug(@"Successfully acquired process handle for pid:%d <handle:%#x>", pid, task);
        return (void*)(uintptr_t)task;
    }
    CLogDebug(@"Error acquiring process handle for pid:%d <error:%#x>", pid, kr);
    return NULL;
}

void* getCurrentProcess()
{
    mach_port_name_t task = current_task();
    if (!MACH_PORT_VALID(task))
        CLogCrit(@"Current process handle <handle:%#x> is invalid", task);
    return (void*)(uintptr_t)task;
}

void closeHandle(void* handle)
{
    mach_port_deallocate(mach_task_self(), (task_t)handle);
}

bool readProcessMemory(void *handle, uintptr_t addr, uint8_t* outData, size_t size, uint32_t *bytesRead)
{
    vm_size_t dataCnt = (vm_size_t)size;
    
    // Use vm_read, rather than mach_vm_read, since the latter is different in
    // iOS.
    kern_return_t kr = vm_read_overwrite((mach_port_name_t)handle,  // vm_map_t target_task
                                         addr,                      // vm_address_t address
                                         size,                      // vm_size_t size,
                                         (vm_address_t)outData,     // vm_address_t data,
                                         (vm_size_t*)&dataCnt);     // vm_size_t *outsize

    
    if (kr) {
        CLogError(@"Unable to read target task's memory @%p - kr 0x%x\n" , (void *) addr, kr);
        return false;
    }
    else if (bytesRead) {
        if (*bytesRead != dataCnt)
            CLogWarn(@"Requested read size bigger than actual read, <requested:%d> <read:%d>", *bytesRead, (int)dataCnt);
        *bytesRead = (uint32_t)dataCnt;
    }
    return true;
}

unsigned char *
readProcessMemory_internal (task_t t,
                            mach_vm_address_t addr,
                            mach_msg_type_number_t* size) {

    mach_msg_type_number_t  dataCnt = (mach_msg_type_number_t) *size;
    vm_offset_t readMem;
    
    // Use vm_read, rather than mach_vm_read, since the latter is different in
    // iOS.
    
    kern_return_t kr = vm_read(t,               // vm_map_t target_task,
                               addr,            // mach_vm_address_t address,
                               *size,           // mach_vm_size_t size
                               &readMem,        // vm_offset_t *data,
                               &dataCnt);       // mach_msg_type_number_t *dataCnt
    
    if (kr) {
        CLogError(@"Unable to read target task's memory @%p - kr 0x%x\n" , (void *) addr, kr);
        return NULL;
    }
    else if (size) {
        if (*size != dataCnt)
            CLogWarn(@"Requested read size bigger than actual read, <requested:%d> <read:%d>", *size, (int)dataCnt);
        *size = dataCnt;
    }
    
    return ( (unsigned char *) readMem);
}

unsigned char * vmReadProcessMemory(void *handle, uintptr_t addr, size_t *size)
{
    return readProcessMemory_internal((task_t)handle, (mach_vm_address_t)addr, (mach_msg_type_number_t*)size);
}

bool writeProcessMemory(void *handle, uintptr_t addr, uint8_t* data, size_t size, uint32_t *bytesWritten)
{
    kern_return_t kr = vm_write((mach_port_name_t)handle,       // vm_map_t target_task
                                addr,                           // vm_address_t address
                                (vm_address_t)data,             // vm_offset_t data
                                (mach_msg_type_number_t)size);  // mach_msg_type_number_t dataCnt
    
    // TODO: fails with KERN_INVALID_ADDRESS
    if (kr) {
        CLogError(@"Unable to write target task's memory @%p - kr 0x%x\n" , (void *) addr, kr);
        return NULL;
    }
    else if (bytesWritten) {
        *bytesWritten = (uint32_t)size;
    }
    return kr == err_none;
}


struct dyld_image_info_ext {
    uintptr_t           imageLoadAddress;	/* base address image is mapped into */
    CFStringRef         imageFilePath;		/* path dyld used to load the image */
    uintptr_t           imageFileModDate;	/* time_t of image file */
    struct stat         st;                 /* image file info      */
    /* if stat().st_mtime of imageFilePath does not match imageFileModDate, */
    /* then file has been modified since dyld loaded it */
};


unsigned int getLoadedModules(void* handle, CFMutableArrayRef* out_dii)
{
    __CTRACE__
    NSMutableArray *g_dii = nil;
    
    task_t task = (task_t)handle;
    struct task_dyld_info dyld_info;
    mach_msg_type_number_t count = TASK_DYLD_INFO_COUNT;
    
    if (task_info(task, TASK_DYLD_INFO, (task_info_t) &dyld_info, &count) == KERN_SUCCESS) {
        
        mach_msg_type_number_t infos_size = sizeof(struct dyld_all_image_infos);
        uint8_t* data = readProcessMemory_internal(task, dyld_info.all_image_info_addr, &infos_size);
        struct dyld_all_image_infos* infos = (struct dyld_all_image_infos *) data;
        
        mach_msg_type_number_t info_size = sizeof(struct dyld_image_info) * infos->infoArrayCount;
        uint8_t* info_addr = readProcessMemory_internal(task, (mach_vm_address_t) infos->infoArray, &info_size);
        struct dyld_image_info* info = (struct dyld_image_info*) info_addr;
        
        size_t dataCnt = infos->infoArrayCount * sizeof(struct dyld_image_info);
        if (dataCnt && out_dii) {
            g_dii = [[NSMutableArray alloc] initWithCapacity:dataCnt];
        }
        
        for (int i=0; i < infos->infoArrayCount; i++) {
            
            struct dyld_image_info_ext dii = { 0 };
            mach_msg_type_number_t fpath_size = PATH_MAX;
            uint8_t* fpath_addr = readProcessMemory_internal(task, (mach_vm_address_t) info[i].imageFilePath, &fpath_size);
            if (fpath_addr) {
                
                struct stat st = { 0 };
                stat((const char *)fpath_addr, &st);
                dii.st = st;
                dii.imageFilePath = CFStringCreateWithCString(NULL, (const char *)fpath_addr, kCFStringEncodingUTF8);
            }
            
            CLogTrace(@"[id:%d] <addr:%p> <date:%#lx> <path:%p> <module:%s>",
                      i, info[i].imageLoadAddress, info[i].imageFileModDate, info[i].imageFilePath, fpath_addr);
            
            if (g_dii) {
                
                dii.imageFileModDate = info[i].imageFileModDate;
                dii.imageLoadAddress = (uintptr_t)info[i].imageLoadAddress;
                g_dii[i] = [NSValue valueWithBytes:&dii objCType:@encode(struct dyld_image_info_ext)];
            }
        }
        
        // Transfer ownership (i.e. get ARC out of the way).
        // i.e. have to call CFRelease
        if (out_dii) *out_dii = (__bridge_retained CFMutableArrayRef)g_dii;
        
        __CTRACE__
        return infos->infoArrayCount;
    }
    __CTRACE__
    return 0;
}

unsigned int getMemRegions(void *handle, void *ctx, void(*regionCb)(void *ctx, uintptr_t base, size_t size, int prot, const char *module))
{
    __CTRACE__
    task_t task = (task_t)handle;
    mach_vm_address_t addr = 1;
    kern_return_t kret;
    vm_region_basic_info_data_t info;
    vm_size_t size;
    mach_port_t object_name;
    mach_msg_type_number_t count;
    vm_address_t firstRegionBegin;
    vm_address_t lastRegionEnd;
    vm_size_t fullSize = 0;
    count = VM_REGION_BASIC_INFO_COUNT_64;
    int regionCount = 0;
    int flag = 0;
    
    NSMutableArray *g_dii = nil;
    CFMutableArrayRef arrRef;
    int g_imageCount = getLoadedModules(handle, &arrRef);
    if (g_imageCount) {
        // Transfer ownership to ARC. ARC kicks in and it's now in charge of releasing the string object.
        // No need to call `CFRelease`
        g_dii = (__bridge_transfer NSMutableArray *)arrRef;
    }
    
    while (flag == 0)
    {
        // Attempts to get the region info for given task
        kret = mach_vm_region(task,
                              &addr,
                              (mach_vm_size_t*)&size,
                              VM_REGION_BASIC_INFO,
                              (vm_region_info_t)
                              &info,
                              &count,
                              &object_name);
        
        if (kret == KERN_SUCCESS)
        {
            int found = -1;
            const char *path = NULL;
            for (int i = 0; i<g_imageCount; i++) {
                
                struct dyld_image_info_ext dii;
                [[g_dii objectAtIndex:i] getValue:&dii];
                
                if (addr >= (mach_vm_address_t)dii.imageLoadAddress &&
                    addr < ((mach_vm_address_t)dii.imageLoadAddress + dii.st.st_size)) {
                    found = i;
                }
            }
            
            if ( found >=0 ) {
                
                struct dyld_image_info_ext dii;
                [[g_dii objectAtIndex:found] getValue:&dii];
                if (dii.imageFilePath) {
                    NSString * file = (__bridge NSString *)dii.imageFilePath;
                    path = [[file lastPathComponent] UTF8String];
                }
            }
            
            CLogTrace(@"[id:%d] <addr:%#llx> <size:%d> <prot:%d> <module:%@>",
                      regionCount, addr, (int)size, (int)info.protection,
                      path ? [[NSString stringWithUTF8String:path] lastPathComponent] : nil);
            
            if (regionCb) {
                regionCb(ctx, (uintptr_t)addr, (size_t)size, info.protection, path);
            }
            
            if (regionCount == 0) {
                firstRegionBegin = addr;
            }
            regionCount += 1;
            fullSize += size;
            addr += size;
        }
        else
            flag = 1;
    }
    lastRegionEnd = addr;
    
    for (int i = 0; i<g_imageCount; i++) {
        
        struct dyld_image_info_ext dii;
        [[g_dii objectAtIndex:i] getValue:&dii];
        
        if (dii.imageFilePath) {
            // No need since we used __bridge_transfer instead of __bridge
            CFRelease(dii.imageFilePath);
        }
    }
    __CTRACE__
    return regionCount;
}
