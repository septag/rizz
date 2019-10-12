#include "rizz/ios.h"
#include "sx/sx.h"
#include "sx/string.h"
#include "sx/os.h"

#include <Foundation/Foundation.h>

const char* rizz_ios_cache_dir()
{
    static char cache_dir[512];

    NSString* cache_dir_ns = [NSSearchPathForDirectoriesInDomains(NSCachesDirectory, NSUserDomainMask, YES) lastObject];
    sx_strcpy(cache_dir, sizeof(cache_dir), [cache_dir_ns UTF8String]);

    return cache_dir;
}

const char* rizz_ios_data_dir()
{
    static char data_dir[512];

    NSString* data_dir_ns = [NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES) lastObject];
    sx_strcpy(data_dir, sizeof(data_dir), [data_dir_ns UTF8String]);

    return data_dir;
}

void* rizz_ios_open_bundle(const char* name)
{
    NSBundle* bundle = [NSBundle bundleWithPath:[[NSBundle mainBundle] pathForResource:[NSString stringWithUTF8String:name] ofType:nil]];
    return (__bridge void*)bundle;
}

void rizz_ios_close_bundle(void* bundle)
{
    sx_unused(bundle);
}

void rizz_ios_resolve_path(void* bundle, const char* filepath, char* out_path, int out_path_sz)
{
    sx_assert(bundle);

    char basename[256];
    char fileext[64];
    sx_os_path_splitext(fileext, sizeof(fileext), basename, sizeof(basename), filepath);

    NSBundle* bundle_ns = (__bridge NSBundle*)bundle;
    
    NSString* path = [bundle_ns pathForResource:[NSString stringWithUTF8String:basename] ofType:[NSString stringWithUTF8String:fileext]];
    if (path) {
        sx_strcpy(out_path, out_path_sz, [path UTF8String]);
    } else {
        out_path[0] = '\0';
    }
}
