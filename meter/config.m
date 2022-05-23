//
//  config.m
//  bgdm
//
//  Created by Bhagwan on 7/4/17.
//  Copyright © 2017 Bhagwan Software. All rights reserved.
//
#import <Foundation/Foundation.h>

#define INI_ANSIONLY
#import "minIni.h"

#import "Defines.h"
#import "meter/config.h"


const char* config_get_my_documentsA()
{
    return [kDocumentsDirectory UTF8String];
}

const char* config_get_ini_fileA()
{
    return [[NSString stringWithFormat:@"%@/%@/%@", kDocumentsDirectory, kBGDMDocFolderPath, kBGDMIniFile] UTF8String];
}

char* config_get_str(char const* sec, char const* key, char const* val)
{
    i8 buf[256] = { 0 };
    ini_gets(sec, key, val, buf, ARRAYSIZE(buf), config_get_ini_fileA());
    return strdup(buf);
}

bool config_set_str(char const* sec, char const* key, char const *val)
{
    return ini_puts(sec, key, val, config_get_ini_fileA()) == 1;
}

int config_get_int(char const* sec, char const* key, int val)
{
    return (int)ini_getl(sec, key, val, config_get_ini_fileA());
}

float config_get_float(char const* sec, char const* key, float val)
{
    return ini_getf(sec, key, val, config_get_ini_fileA());
}


bool config_set_int(char const* sec, char const* key, int val)
{
    return ini_putl(sec, key, val, config_get_ini_fileA()) == 1;
}

bool config_set_float(char const* sec, char const* key, float val)
{
    return ini_putf(sec, key, val, config_get_ini_fileA()) == 1;
}

KeyBind config_get_bind(char const* sec, char const* key, i8 const* val, keybCallback cb)
{
    KeyBind ret = NULL;
    char *str = config_get_str(sec, key, val);
    if (str) {
        ret = registerKeyStr(str, cb, NULL);
        free(str);
    }
    return ret;
}


const i8* config_get_dbg2()
{
    static char sect[128];
    /*char buff[128] = { 0 };
     utf8_to_base64("MyNameIsArcAndISukDick", buff, sizeof(buff));
     DBGPRINT(L"BASE64-2: %S", buff);*/
    NSString *base64String = @"TXlOYW1lSXNBcmNBbmRJU3VrRGljaw==";
    NSData *decodedData = [[NSData alloc] initWithBase64EncodedString:base64String options:0];
    NSString *decodedString = [[NSString alloc] initWithData:decodedData encoding:NSUTF8StringEncoding];
    memset(sect, 0, sizeof(sect));
    strncpy(sect, [decodedString UTF8String], sizeof(sect));
    return sect;
}
