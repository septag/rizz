
// Windows single header stack walker in C (DbgHelp)
// Copyright (c) 2021, Sepehr Taghdisian.
// Copyright (c) 2005 - 2019, Jochen Kalmbach. All rights reserved.
// Usage:
//  #define SW_IMPL
//  #include "stackwalkerc.h"
// 
//  sw_context* ctx = sw_create_context_capture(SW_OPTIONS_ALL, callbacks, NULL);
//  sw_show_callstack(ctx);
//  
// The usage is super simple, you can see the options/callbacks and check example.cpp
// 
// History:
//      1.0.0: Initial version
//      1.0.1: Bugs fixed, taking more sw_option flags into action
//      1.1.0: Added extra userptr to show_callstack_userptr to override the callbacks ptr per-callstack
//      1.2.0: Added fast backtrace implementation for captures in current thread. 
//             Added utility function sw_load_dbghelp
//             Added limiter function sw_set_callstack_limits
//      1.3.0  Added more advanced functions for resolving callstack lazily, sw_capture_current, sw_resolve_callstack
//      1.4.0  Added module cache and reload modules on-demand
//      1.5.0  House cleaning, added sw_set_dbghelp_hintpath, ditched error_msg callback for SW_LOG_ERROR macro
//
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifndef SW_API_DECL
#define SW_API_DECL
#endif

#ifndef SW_API_IMPL
#define SW_API_IMPL SW_API_DECL
#endif

#ifndef SW_MAX_NAME_LEN
#define SW_MAX_NAME_LEN 1024
#endif

#ifndef SW_MAX_FRAMES
#define SW_MAX_FRAMES 64
#endif

typedef enum sw_options
{
    SW_OPTIONS_NONE          = 0,
    SW_OPTIONS_SYMBOL        = 0x1,     // Get symbol names
    SW_OPTIONS_SOURCEPOS     = 0x2,     // Get symbol file+line
    SW_OPTIONS_MODULEINFO    = 0x4,     // Get module information
    SW_OPTIONS_VERBOSE       = 0xf,     // All above options
    SW_OPTIONS_SYMBUILDPATH  = 0x10,    // Generate a good symbol search path
    SW_OPTIONS_SYMUSESERVER  = 0x20,    // Use public microsoft symbol server
    SW_OPTIONS_SYMALL        = 0x30,    // All symbol options
    SW_OPTIONS_ALL           = 0x3f     // All options
} sw_options;

typedef void* sw_sys_handle;            // HANDLE
typedef void* sw_exception_pointers;    // PEXCEPTION_POINTERS
typedef struct sw_context sw_context;

typedef struct sw_callstack_entry
{
    uint64_t    offset;
    char        name[SW_MAX_NAME_LEN];
    char        und_name[SW_MAX_NAME_LEN];
    uint64_t    offset_from_symbol;
    uint32_t    offset_from_line;
    uint32_t    line;
    char        line_filename[SW_MAX_NAME_LEN];
    uint32_t    symbol_type;
    const char* symbol_type_str;
    char        module_name[SW_MAX_NAME_LEN];
    uint64_t    baseof_image;
    char        loaded_image_name[SW_MAX_NAME_LEN];
} sw_callstack_entry;

typedef struct sw_callbacks
{
    void (*symbol_init)(const char* search_path, uint32_t sym_opts, void* userptr);
    void (*load_module)(const char* img, const char* module, uint64_t base_addr, uint32_t size, void* userptr);
    void (*callstack_begin)(void* userptr);
    void (*callstack_entry)(const sw_callstack_entry* entry, void* userptr);
    void (*callstack_end)(void* userptr);
} sw_callbacks;

#ifdef __cplusplus
extern "C" {
#endif

SW_API_DECL sw_context* sw_create_context_capture(uint32_t options, sw_callbacks callbacks, void* userptr);
SW_API_DECL sw_context* sw_create_context_capture_other(uint32_t options, uint32_t process_id,
                                                        sw_sys_handle process, sw_callbacks callbacks, void* userptr);
SW_API_DECL sw_context* sw_create_context_exception(uint32_t options, 
                                                    sw_exception_pointers exp_ptrs,
                                                    sw_callbacks callbacks, void* userptr);
SW_API_DECL sw_context* sw_create_context_catch(uint32_t options, sw_callbacks callbacks, void* userptr);

SW_API_DECL void sw_destroy_context(sw_context* ctx);

SW_API_DECL void sw_set_symbol_path(sw_context* ctx, const char* sympath);
SW_API_DECL void sw_set_callstack_limits(sw_context* ctx, uint32_t frames_to_skip, uint32_t frames_to_capture);
SW_API_DECL bool sw_show_callstack_userptr(sw_context* ctx, sw_sys_handle thread_hdl /*=NULL*/, void* callbacks_userptr);
SW_API_DECL bool sw_show_callstack(sw_context* ctx, sw_sys_handle thread_hdl /*=NULL*/);

// manual/advanced functions
SW_API_DECL sw_sys_handle sw_load_dbghelp(void);
SW_API_DECL uint16_t sw_capture_current(sw_context* ctx, void* symbols[SW_MAX_FRAMES]);
SW_API_DECL uint16_t sw_resolve_callstack(sw_context* ctx, void* symbols[SW_MAX_FRAMES], 
                                          sw_callstack_entry entries[SW_MAX_FRAMES], uint16_t num_entries);
SW_API_DECL void sw_reload_modules(sw_context* ctx);
SW_API_DECL bool sw_get_symbol_module(sw_context* ctx, void* symbol, char module_name[32]);
SW_API_DECL void sw_set_dbghelp_hintpath(const char* path);

#ifdef __cplusplus
}
#endif

#ifdef SW_IMPL

#ifndef _WIN32
#error "Platforms other than Windows are not supported"
#endif

#define WIN32_LEAN_AND_MEAN
#pragma warning(push)
#pragma warning(disable : 5105)
#include <windows.h>
#pragma warning(pop)
#include <malloc.h> // alloca, malloc
#include <string.h> // strlen, strcat_s

#ifndef SW_ASSERT
#   include <assert.h>
#   define SW_ASSERT(e)   assert(e)
#endif

#ifndef SW_LOG_ERROR
#   include <stdio.h>
#   define SW_LOG_ERROR(err_fmt, ...) printf(err_fmt "\n", ##__VA_ARGS__)
#endif

#ifndef SW_MALLOC
#   define SW_MALLOC(size)        malloc(size)
#   define SW_FREE(ptr)           free(ptr)
#endif

#define _SW_UNUSED(x) (void)(x)

#ifndef _SW_PRIVATE
#   if defined(__GNUC__) || defined(__clang__)
#       define _SW_PRIVATE __attribute__((unused)) static
#   else
#       define _SW_PRIVATE static
#   endif
#endif

_SW_PRIVATE char* sw__strcpy(char* dst, size_t dst_sz, const char* src)
{
    SW_ASSERT(dst);
    SW_ASSERT(src);
    SW_ASSERT(dst_sz > 0);

    const size_t len = strlen(src);
    const size_t _max = dst_sz - 1;
    const size_t num = (len < _max ? len : _max);
    memcpy(dst, src, num);
    dst[num] = '\0';

    return dst;
}

#pragma pack(push, 8)
#include <DbgHelp.h>

typedef struct _IMAGEHLP_MODULE64_V3
{
    DWORD SizeOfStruct;        // set to sizeof(IMAGEHLP_MODULE64)
    DWORD64 BaseOfImage;       // base load address of module
    DWORD ImageSize;           // virtual size of the loaded module
    DWORD TimeDateStamp;       // date/time stamp from pe header
    DWORD CheckSum;            // checksum from the pe header
    DWORD NumSyms;             // number of symbols in the symbol table
    SYM_TYPE SymType;          // type of symbols loaded
    CHAR ModuleName[32];       // module name
    CHAR ImageName[256];       // image name
    CHAR LoadedImageName[256]; // symbol file name
    // new elements: 07-Jun-2002
    CHAR LoadedPdbName[256];   // pdb file name
    DWORD CVSig;               // Signature of the CV record in the debug directories
    CHAR CVData[MAX_PATH * 3]; // Contents of the CV record
    DWORD PdbSig;              // Signature of PDB
    GUID PdbSig70;             // Signature of PDB (VC 7 and up)
    DWORD PdbAge;              // DBI age of pdb
    BOOL PdbUnmatched;         // loaded an unmatched pdb
    BOOL DbgUnmatched;         // loaded an unmatched dbg
    BOOL LineNumbers;          // we have line number information
    BOOL GlobalSymbols;        // we have internal symbol information
    BOOL TypeInfo;             // we have type information
    // new elements: 17-Dec-2003
    BOOL SourceIndexed; // pdb supports source server
    BOOL Publics;       // contains public symbols
} IMAGEHLP_MODULE64_V3, *PIMAGEHLP_MODULE64_V3;

typedef struct _IMAGEHLP_MODULE64_V2
{
    DWORD SizeOfStruct;        // set to sizeof(IMAGEHLP_MODULE64)
    DWORD64 BaseOfImage;       // base load address of module
    DWORD ImageSize;           // virtual size of the loaded module
    DWORD TimeDateStamp;       // date/time stamp from pe header
    DWORD CheckSum;            // checksum from the pe header
    DWORD NumSyms;             // number of symbols in the symbol table
    SYM_TYPE SymType;          // type of symbols loaded
    CHAR ModuleName[32];       // module name
    CHAR ImageName[256];       // image name
    CHAR LoadedImageName[256]; // symbol file name
} IMAGEHLP_MODULE64_V2, *PIMAGEHLP_MODULE64_V2;
#pragma pack(pop)
  
typedef BOOL(__stdcall* SymCleanup_t)(IN HANDLE process); 
typedef PVOID(__stdcall* SymFunctionTableAccess64_t)(HANDLE process, DWORD64 AddrBase); 
typedef BOOL(__stdcall* SymGetLineFromAddr64_t)(IN HANDLE process,
                                                IN DWORD64 dwAddr,
                                                OUT PDWORD pdwDisplacement,
                                                OUT PIMAGEHLP_LINE64 line);  
typedef DWORD64(__stdcall* SymGetModuleBase64_t)(IN HANDLE process, IN DWORD64 dwAddr);                              
typedef BOOL(__stdcall* SymGetModuleInfo64_t)(IN HANDLE process,
                                              IN DWORD64 dwAddr,
                                              OUT IMAGEHLP_MODULE64_V3* ModuleInfo);
typedef DWORD(__stdcall* SymGetOptions_t)(VOID);
typedef BOOL(__stdcall* SymGetSymFromAddr64_t)(IN HANDLE process,
                                               IN DWORD64 dwAddr,
                                               OUT PDWORD64 pdwDisplacement,
                                               OUT PIMAGEHLP_SYMBOL64 Symbol);
typedef BOOL(__stdcall* SymInitialize_t)(IN HANDLE process, IN LPCSTR UserSearchPath, IN BOOL fInvadeProcess);
typedef DWORD64(__stdcall* SymLoadModule64_t)(IN HANDLE process,
                                              IN HANDLE hFile,
                                              IN LPCSTR ImageName,
                                              IN LPCSTR ModuleName,
                                              IN DWORD64 BaseOfDll,
                                              IN DWORD SizeOfDll);
typedef BOOL(__stdcall* SymUnloadModule64_t)(IN HANDLE hProcess, IN DWORD64 BaseOfDll);                                              
typedef DWORD(__stdcall* SymSetOptions_t)(IN DWORD SymOptions);
typedef BOOL(__stdcall* StackWalk64_t)(DWORD                           MachineType,
                                       HANDLE                           process,
                                       HANDLE                           hThread,
                                       LPSTACKFRAME64                   StackFrame,
                                       PVOID                            ContextRecord,
                                       PREAD_PROCESS_MEMORY_ROUTINE64   ReadMemoryRoutine,
                                       PFUNCTION_TABLE_ACCESS_ROUTINE64 FunctionTableAccessRoutine,
                                       PGET_MODULE_BASE_ROUTINE64       GetModuleBaseRoutine,
                                       PTRANSLATE_ADDRESS_ROUTINE64     TranslateAddress);
typedef DWORD(__stdcall WINAPI* UnDecorateSymbolName_t)(PCSTR DecoratedName,
                                                        PSTR  UnDecoratedName,
                                                        DWORD UndecoratedLength,
                                                        DWORD Flags);
typedef BOOL(__stdcall WINAPI* SymGetSearchPath_t)(HANDLE process, PSTR SearchPath, DWORD SearchPathLength);
typedef BOOL(__stdcall WINAPI* EnumerateLoadedModules64_t)(HANDLE hProcess, PENUMLOADED_MODULES_CALLBACK64  EnumLoadedModulesCallback, PVOID UserContext);

typedef BOOL(__stdcall* ReadProcessMemoryRoutine_t)(
      HANDLE  process,
      DWORD64 qwBaseAddress,
      PVOID   lpBuffer,
      DWORD   nSize,
      LPDWORD lpNumberOfBytesRead,
      LPVOID  pUserData); // optional data, which was passed in "show_callstack"
    
  
// **************************************** ToolHelp32 ************************
#define MAX_MODULE_NAME32 255
#define TH32CS_SNAPMODULE 0x00000008
#pragma pack(push, 8)
typedef struct tagMODULEENTRY32
{
    DWORD dwSize;
    DWORD th32ModuleID;  // This module
    DWORD th32ProcessID; // owning process
    DWORD GlblcntUsage;  // Global usage count on the module
    DWORD ProccntUsage;  // module usage count in th32ProcessID's context
    BYTE *modBaseAddr;   // Base address of module in th32ProcessID's context
    DWORD modBaseSize;   // Size in bytes of module starting at modBaseAddr
    HMODULE hModule;     // The hModule of this module in th32ProcessID's context
    char szModule[MAX_MODULE_NAME32 + 1];
    char szExePath[MAX_PATH];
} MODULEENTRY32;
typedef MODULEENTRY32 *PMODULEENTRY32;
typedef MODULEENTRY32 *LPMODULEENTRY32;
#pragma pack(pop)

// **************************************** PSAPI ************************
typedef struct _MODULEINFO 
{
    LPVOID lpBaseOfDll;
    DWORD SizeOfImage;
    LPVOID EntryPoint;
} MODULEINFO, *LPMODULEINFO;

// Normally it should be enough to use 'CONTEXT_FULL' (better would be 'CONTEXT_ALL')
#define USED_CONTEXT_FLAGS CONTEXT_FULL

// only available on msvc2015+ (_MSC_VER >= 1900)
#ifdef __cplusplus
extern "C" {
#endif
void** __cdecl __current_exception_context();
#ifdef __cplusplus
}
#endif

typedef struct sw_module_cache_item
{
    char    name[32];
    DWORD64 base_addr;
} sw_module_cache_item;

typedef struct sw_context_internal
{
    sw_context* parent;
    CONTEXT ctx;
    HMODULE dbg_help;
    HANDLE  process;
    SymCleanup_t fSymCleanup;
    SymFunctionTableAccess64_t fSymFunctionTableAccess64;
    SymGetModuleBase64_t fSymGetModuleBase64;
    SymGetModuleInfo64_t fSymGetModuleInfo64;
    
    SymGetOptions_t fSymGetOptions;
    SymGetSymFromAddr64_t fSymGetSymFromAddr64;
    SymGetLineFromAddr64_t fSymGetLineFromAddr64;
    SymInitialize_t fSymInitialize;
    SymLoadModule64_t fSymLoadModule64;
    SymUnloadModule64_t fSymUnloadModule64;
    
    SymSetOptions_t fSymSetOptions;
    StackWalk64_t fStackWalk64;
    UnDecorateSymbolName_t fUnDecorateSymbolName;
    SymGetSearchPath_t fSymGetSearchPath;
    EnumerateLoadedModules64_t fEnumerateLoadedModules64;

    CRITICAL_SECTION modules_cs;
    uint32_t num_modules;
    uint32_t max_modules;
    sw_module_cache_item* modules;
} sw_context_internal;

typedef struct sw_context
{
    sw_callbacks callbacks;
    void* callbacks_userptr;
    sw_sys_handle process;
    uint32_t process_id;
    bool modules_loaded;
    bool reload_modules;
    bool fatal_error;       // cannot recover anymore
    char sympath[SW_MAX_NAME_LEN];
    uint32_t options;
    uint32_t max_recursion;
    sw_context_internal internal;
    uint32_t    frames_to_skip;
    uint32_t    frames_to_capture;
} sw_context;

static char sw__dbghelp_hintpath[SW_MAX_NAME_LEN];

_SW_PRIVATE bool sw__get_module_info(sw_context_internal* ctxi, HANDLE process, DWORD64 baseAddr, IMAGEHLP_MODULE64_V3* modinfo)
{
    memset(modinfo, 0, sizeof(IMAGEHLP_MODULE64_V3));
    if (ctxi->fSymGetModuleInfo64 == NULL) {
        SetLastError(ERROR_DLL_INIT_FAILED);
        return false;
    }
    // First try to use the larger ModuleInfo-Structure
    modinfo->SizeOfStruct = sizeof(IMAGEHLP_MODULE64_V3);
    char data[4096];    // reserve enough memory, so the bug in v6.3.5.1 does not lead to memory-overwrites...

    memcpy(data, modinfo, sizeof(IMAGEHLP_MODULE64_V3));
    static bool use_v3_version = true;
    if (use_v3_version) {
        if (ctxi->fSymGetModuleInfo64(process, baseAddr, (IMAGEHLP_MODULE64_V3*)data) != FALSE) {
            // only copy as much memory as is reserved...
            memcpy(modinfo, data, sizeof(IMAGEHLP_MODULE64_V3));
            modinfo->SizeOfStruct = sizeof(IMAGEHLP_MODULE64_V3);
            return true;
        }
        use_v3_version = false;    // to prevent unnecessary calls with the larger struct...
    }

    // could not retrieve the bigger structure, try with the smaller one (as defined in VC7.1)...
    modinfo->SizeOfStruct = sizeof(IMAGEHLP_MODULE64_V2);
    memcpy(data, modinfo, sizeof(IMAGEHLP_MODULE64_V2));
    if (ctxi->fSymGetModuleInfo64(process, baseAddr, (IMAGEHLP_MODULE64_V3*)data) != FALSE) {
        // only copy as much memory as is reserved...
        memcpy(modinfo, data, sizeof(IMAGEHLP_MODULE64_V2));
        modinfo->SizeOfStruct = sizeof(IMAGEHLP_MODULE64_V2);
        return true;
    }
    SetLastError(ERROR_DLL_INIT_FAILED);
    return false;
}

_SW_PRIVATE bool sw__get_module_info_csentry(sw_context_internal* ctxi, HANDLE process, DWORD64 baseAddr, 
                                             sw_callstack_entry* cs_entry)
{
    IMAGEHLP_MODULE64_V3 module;
    bool r = sw__get_module_info(ctxi, process, baseAddr, &module);
    if (r) {
        SW_ASSERT(cs_entry);
        cs_entry->symbol_type = module.SymType;

        switch (module.SymType) {
        case SymNone:       cs_entry->symbol_type_str = "-nosymbols-"; break;
        case SymCoff:       cs_entry->symbol_type_str = "COFF";        break;
        case SymCv:         cs_entry->symbol_type_str = "CV";          break;
        case SymPdb:        cs_entry->symbol_type_str = "PDB";         break;
        case SymExport:     cs_entry->symbol_type_str = "-exported-";  break;
        case SymDeferred:   cs_entry->symbol_type_str = "-deferred-";  break;
        case SymSym:        cs_entry->symbol_type_str = "SYM";         break;
        #if API_VERSION_NUMBER >= 9
        case SymDia:        cs_entry->symbol_type_str = "DIA";         break;
        #endif
        case 8:             cs_entry->symbol_type_str = "Virtual";     break;
        default:            cs_entry->symbol_type_str = NULL;          break;
        }

        sw__strcpy(cs_entry->module_name, sizeof(cs_entry->module_name), module.ModuleName);
        cs_entry->baseof_image = module.BaseOfImage;
        sw__strcpy(cs_entry->loaded_image_name, sizeof(cs_entry->module_name), module.LoadedImageName);
    }

    return r;
}

static BOOL sw__enum_loaded_modules_cb(PCSTR module_path, ULONGLONG module_base, ULONG module_size, PVOID userptr)
{
    // resolve module name
    char module_name[SW_MAX_NAME_LEN];
    const char* backslash = strrchr(module_path, '\\');
    if (backslash) {
        sw__strcpy(module_name, sizeof(module_name), backslash+1);
    } else {
        sw__strcpy(module_name, sizeof(module_name), module_path);
    }
    sw_context_internal* ctxi = (sw_context_internal*)userptr;
    EnterCriticalSection(&ctxi->modules_cs);
    // don't Load modules that are already in the cache
    for (uint32_t mi = 0; mi < ctxi->num_modules; mi++) {
        if (strcmp(ctxi->modules[mi].name, module_name) == 0) {
            LeaveCriticalSection(&ctxi->modules_cs);
            return ERROR_SUCCESS;
        }
    }
    LeaveCriticalSection(&ctxi->modules_cs);

    DWORD64 mod_addr = ctxi->fSymLoadModule64(ctxi->process, NULL, module_path, module_name, module_base, module_size);
    if (mod_addr == 0) {
        return FALSE;
    } else {
        EnterCriticalSection(&ctxi->modules_cs);

        // Add the module to cache
        if (ctxi->num_modules == ctxi->max_modules) {
            ctxi->max_modules <<= 1;
            sw_module_cache_item* modules = (sw_module_cache_item*)SW_MALLOC(ctxi->max_modules*sizeof(sw_module_cache_item));
            if (!modules) {
                LeaveCriticalSection(&ctxi->modules_cs);
                SW_ASSERT(0);
                return 0;
            }
            memcpy(modules, ctxi->modules, ctxi->num_modules*sizeof(sw_module_cache_item));
            SW_FREE(ctxi->modules);
            ctxi->modules = modules;
        }

        sw_module_cache_item* mcache = &ctxi->modules[ctxi->num_modules++];
        sw__strcpy(mcache->name, sizeof(mcache->name), module_name);
        mcache->base_addr = mod_addr;
        LeaveCriticalSection(&ctxi->modules_cs);
    }

    if (ctxi->parent->callbacks.load_module) {
        ctxi->parent->callbacks.load_module(module_path, module_name, mod_addr, module_size, ctxi->parent->callbacks_userptr);
    }    

    return TRUE;
}

_SW_PRIVATE bool sw__get_module_list_TH32(sw_context_internal* ctxi, DWORD pid)
{
    typedef HANDLE(__stdcall * CreateToolhelp32Snapshot_t)(DWORD dwFlags, DWORD th32ProcessID);
    typedef BOOL(__stdcall * Module32First_t)(HANDLE hSnapshot, LPMODULEENTRY32 lpme);
    typedef BOOL(__stdcall * Module32Next_t)(HANDLE hSnapshot, LPMODULEENTRY32 lpme);

    // try both dlls...
    const char* dllname[] = { "kernel32.dll", "tlhelp32.dll" };
    HINSTANCE hToolhelp = NULL;
    CreateToolhelp32Snapshot_t fCreateToolhelp32Snapshot = NULL;
    Module32First_t fModule32First = NULL;
    Module32Next_t fModule32Next = NULL;

    HANDLE hSnap;
    MODULEENTRY32 me;
    me.dwSize = sizeof(me);
    BOOL keepGoing;
    size_t i;

    for (i = 0; i < (sizeof(dllname) / sizeof(dllname[0])); i++) {
        hToolhelp = LoadLibraryA(dllname[i]);
        if (hToolhelp == NULL)
            continue;
        fCreateToolhelp32Snapshot = (CreateToolhelp32Snapshot_t)GetProcAddress(hToolhelp, "CreateToolhelp32Snapshot");
        fModule32First = (Module32First_t)GetProcAddress(hToolhelp, "Module32First");
        fModule32Next = (Module32Next_t)GetProcAddress(hToolhelp, "Module32Next");
        if ((fCreateToolhelp32Snapshot != NULL) && (fModule32First != NULL) && (fModule32Next != NULL))
            break;    // found the functions!
        FreeLibrary(hToolhelp);
        hToolhelp = NULL;
    }

    if (hToolhelp == NULL)
        return FALSE;

    hSnap = fCreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);
    if (hSnap == (HANDLE)-1) {
        FreeLibrary(hToolhelp);
        return FALSE;
    }

    keepGoing = !!fModule32First(hSnap, &me);
    int cnt = 0;
    while (keepGoing) {
        sw__enum_loaded_modules_cb(me.szExePath, (DWORD64)me.modBaseAddr, me.modBaseSize, ctxi);
        cnt++;
        keepGoing = !!fModule32Next(hSnap, &me);
    }
    CloseHandle(hSnap);
    FreeLibrary(hToolhelp);
    return (cnt <= 0) ? false : true;
}    // sw__get_module_list_TH32

_SW_PRIVATE bool sw__load_modules_internal(sw_context_internal* ctxi, HANDLE process, DWORD process_id)
{
    sx_unused(process_id);
    if (!sw__get_module_list_TH32(ctxi, process_id)) {
        return ctxi->fEnumerateLoadedModules64(process, sw__enum_loaded_modules_cb, ctxi) ? true : false;
    }
    return true;
}

_SW_PRIVATE PCONTEXT get_current_exception_context()
{
    PCONTEXT* pctx = (PCONTEXT*)__current_exception_context();
    return pctx ? *pctx : NULL;
}

_SW_PRIVATE sw_context* sw__create_context(uint32_t options, uint32_t process_id, sw_sys_handle process, 
                                           PCONTEXT context_win, sw_callbacks callbacks, void* userptr)
{
    SW_ASSERT(process);

    sw_context* ctx = (sw_context*)SW_MALLOC(sizeof(sw_context));
    memset(ctx, 0x0, sizeof(sw_context));

    ctx->options = options;
    ctx->max_recursion = 1000;
    ctx->process_id = process_id;
    ctx->process = process;
    ctx->callbacks = callbacks;
    ctx->callbacks_userptr = userptr;
    ctx->internal.process = process;
    ctx->internal.parent = ctx;
    ctx->internal.ctx.ContextFlags = 0;
    if (context_win)
        ctx->internal.ctx = *context_win;
    ctx->frames_to_skip = 0;
    ctx->frames_to_capture = 0xffffffff;
    return ctx;
}

SW_API_IMPL sw_sys_handle sw_load_dbghelp()
{
    HMODULE dbg_help = NULL;
    // Dynamically load the Entry-Points for dbghelp.dll:
    // First try to load the newest one from
    TCHAR temp[4096];

    if (sw__dbghelp_hintpath[0]) {
        sw__strcpy(temp, sizeof(temp), sw__dbghelp_hintpath);
        strcat_s(temp, sizeof(temp), "\\dbghelp.dll");
        if (GetFileAttributes(temp) != INVALID_FILE_ATTRIBUTES) {
            dbg_help = LoadLibraryA(temp);
        }
    }

    // ".local" file does not exist, so we can try to load the dbghelp.dll from the
    // "Debugging Tools for Windows" Ok, first try the new path according to the
    // architecture:
    #ifdef _M_IX86
    if ((dbg_help == NULL) && (GetEnvironmentVariableA("ProgramFiles", temp, sizeof(temp)) > 0)) {
        strcat_s(temp, sizeof(temp), "\\Debugging Tools for Windows (x86)\\dbghelp.dll");
        // now check if the file exists:
        if (GetFileAttributes(temp) != INVALID_FILE_ATTRIBUTES) {
            dbg_help = LoadLibraryA(temp);
        }
    }
    #elif _M_X64
    if ((dbg_help == NULL) && (GetEnvironmentVariableA("ProgramFiles", temp, sizeof(temp)) > 0)) {
        strcat_s(temp, sizeof(temp), "\\Debugging Tools for Windows (x64)\\dbghelp.dll");
        // now check if the file exists:
        if (GetFileAttributes(temp) != INVALID_FILE_ATTRIBUTES) {
            dbg_help = LoadLibraryA(temp);
        }
    }
    #elif _M_IA64
    if ((dbg_help == NULL) &&
        (GetEnvironmentVariableA("ProgramFiles", temp, sizeof(temp)) > 0)) {
        strcat_s(temp, sizeof(temp), "\\Debugging Tools for Windows (ia64)\\dbghelp.dll");
        // now check if the file exists:
        if (GetFileAttributes(temp) != INVALID_FILE_ATTRIBUTES) {
            dbg_help = LoadLibraryA(temp);
        }
    }
    #endif

    // If still not found, try the old directories...
    if ((dbg_help == NULL) && (GetEnvironmentVariableA("ProgramFiles", temp, sizeof(temp)) > 0)) {
        strcat_s(temp, sizeof(temp), "\\Debugging Tools for Windows\\dbghelp.dll");
        // now check if the file exists:
        if (GetFileAttributes(temp) != INVALID_FILE_ATTRIBUTES) {
            dbg_help = LoadLibraryA(temp);
        }
    }
    #if defined _M_X64 || defined _M_IA64
    // Still not found? Then try to load the (old) 64-Bit version:
    if ((dbg_help == NULL) &&
        (GetEnvironmentVariableA("ProgramFiles", temp, sizeof(temp)) > 0)) {
        strcat_s(temp, sizeof(temp), "\\Debugging Tools for Windows 64-Bit\\dbghelp.dll");
        if (GetFileAttributes(temp) != INVALID_FILE_ATTRIBUTES) {
            dbg_help = LoadLibraryA(temp);
        }
    }
    #endif
    if (dbg_help == NULL)    // if not already loaded, try to load a default-one
        dbg_help = LoadLibraryA("dbghelp.dll");

    return dbg_help;
}

_SW_PRIVATE bool sw__init_internal(sw_context_internal* ctxi, const char* sympath)
{
    ctxi->dbg_help = (HMODULE)sw_load_dbghelp();
    if (ctxi->dbg_help == NULL) {
        return false;
    }

    ctxi->fSymInitialize = (SymInitialize_t)GetProcAddress(ctxi->dbg_help, "SymInitialize");
    ctxi->fSymCleanup = (SymCleanup_t)GetProcAddress(ctxi->dbg_help, "SymCleanup");

    ctxi->fStackWalk64 = (StackWalk64_t)GetProcAddress(ctxi->dbg_help, "StackWalk64");
    ctxi->fSymGetOptions = (SymGetOptions_t)GetProcAddress(ctxi->dbg_help, "SymGetOptions");
    ctxi->fSymSetOptions = (SymSetOptions_t)GetProcAddress(ctxi->dbg_help, "SymSetOptions");

    ctxi->fSymFunctionTableAccess64 = (SymFunctionTableAccess64_t)GetProcAddress(ctxi->dbg_help, "SymFunctionTableAccess64");
    ctxi->fSymGetLineFromAddr64 = (SymGetLineFromAddr64_t)GetProcAddress(ctxi->dbg_help, "SymGetLineFromAddr64");
    ctxi->fSymGetModuleBase64 = (SymGetModuleBase64_t)GetProcAddress(ctxi->dbg_help, "SymGetModuleBase64");
    ctxi->fSymGetModuleInfo64 = (SymGetModuleInfo64_t)GetProcAddress(ctxi->dbg_help, "SymGetModuleInfo64");
    ctxi->fSymGetSymFromAddr64 = (SymGetSymFromAddr64_t)GetProcAddress(ctxi->dbg_help, "SymGetSymFromAddr64");
    ctxi->fUnDecorateSymbolName = (UnDecorateSymbolName_t)GetProcAddress(ctxi->dbg_help, "UnDecorateSymbolName");
    ctxi->fSymLoadModule64 = (SymLoadModule64_t)GetProcAddress(ctxi->dbg_help, "SymLoadModule64");
    ctxi->fSymUnloadModule64 = (SymUnloadModule64_t)GetProcAddress(ctxi->dbg_help, "SymUnloadModule64");
    ctxi->fSymGetSearchPath = (SymGetSearchPath_t)GetProcAddress(ctxi->dbg_help, "SymGetSearchPath");
    ctxi->fEnumerateLoadedModules64 = (EnumerateLoadedModules64_t)GetProcAddress(ctxi->dbg_help, "EnumerateLoadedModules64");

    if (ctxi->fSymCleanup == NULL || ctxi->fSymFunctionTableAccess64 == NULL || ctxi->fSymGetModuleBase64 == NULL ||
        ctxi->fSymGetModuleInfo64 == NULL || ctxi->fSymGetOptions == NULL || ctxi->fSymGetSymFromAddr64 == NULL ||
        ctxi->fSymInitialize == NULL || ctxi->fSymSetOptions == NULL || ctxi->fStackWalk64 == NULL ||
        ctxi->fUnDecorateSymbolName == NULL || ctxi->fSymLoadModule64 == NULL || ctxi->fSymUnloadModule64 == NULL || 
        ctxi->fEnumerateLoadedModules64 == NULL) 
    {
        FreeLibrary(ctxi->dbg_help);
        ctxi->dbg_help = NULL;
        ctxi->fSymCleanup = NULL;
        return false;
    }

    // SymInitialize
    if (ctxi->fSymInitialize(ctxi->process, sympath, FALSE) == FALSE) {
        SW_LOG_ERROR("SymInitialize failed (ErrorCode=%lu)", GetLastError());
        return false;
    }

    DWORD sym_opts = ctxi->fSymGetOptions();    // SymGetOptions
    sym_opts |= SYMOPT_LOAD_LINES;
    sym_opts |= SYMOPT_FAIL_CRITICAL_ERRORS;
    sym_opts = ctxi->fSymSetOptions(sym_opts);

    char buf[SW_MAX_NAME_LEN] = { 0 };
    if (ctxi->fSymGetSearchPath != NULL) {
        if (ctxi->fSymGetSearchPath(ctxi->process, buf, SW_MAX_NAME_LEN) == FALSE)
            SW_LOG_ERROR("SymGetSearchPath failed (ErrorCode=%lu)", GetLastError());
    }

    if (ctxi->parent->callbacks.symbol_init) {
        ctxi->parent->callbacks.symbol_init(buf, sym_opts, ctxi->parent->callbacks_userptr);
    }

    // initialize module cache 
    InitializeCriticalSection(&ctxi->modules_cs);
    ctxi->max_modules = 32;
    ctxi->modules = (sw_module_cache_item*)SW_MALLOC(ctxi->max_modules*sizeof(sw_module_cache_item));
    if (!ctxi->modules) {
        return false;
    }

    return true;
}

_SW_PRIVATE bool sw__load_modules(sw_context* ctx)
{
    if (ctx->fatal_error) 
        return false;
    
    if (ctx->modules_loaded)
        return true;

    // Build the sym-path:
    char sympath[SW_MAX_NAME_LEN]; sympath[0] = 0;
    if ((ctx->options & SW_OPTIONS_SYMBUILDPATH) != 0) {
        // Now first add the (optional) provided sympath:
        if (ctx->sympath[0]) {
            strcat_s(sympath, sizeof(sympath), ctx->sympath);
            strcat_s(sympath, sizeof(sympath), ";");
        }

        strcat_s(sympath, sizeof(sympath), ".;");

        char temp[1024];
        // Now add the current directory:
        if (GetCurrentDirectoryA(sizeof(temp), temp) > 0) {
            temp[sizeof(temp) - 1] = 0;
            strcat_s(sympath, sizeof(sympath), temp);
            strcat_s(sympath, sizeof(sympath), ";");
        }

        // Now add the path for the main-module:
        if (GetModuleFileNameA(NULL, temp, sizeof(temp)) > 0) {
            temp[sizeof(temp) - 1] = 0;
            for (char* p = (temp + strlen(temp) - 1); p >= temp; --p) {
                // locate the rightmost path separator
                if ((*p == '\\') || (*p == '/') || (*p == ':')) {
                    *p = 0;
                    break;
                }
            }    // for (search for path separator...)
            if (strlen(temp) > 0) {
                strcat_s(sympath, sizeof(sympath), temp);
                strcat_s(sympath, sizeof(sympath), ";");
            }
        }
        if (GetEnvironmentVariableA("_NT_SYMBOL_PATH", temp, sizeof(temp)) > 0) {
            temp[sizeof(temp) - 1] = 0;
            strcat_s(sympath, sizeof(sympath), temp);
            strcat_s(sympath, sizeof(sympath), ";");
        }
        if (GetEnvironmentVariableA("_NT_ALTERNATE_SYMBOL_PATH", temp, sizeof(temp)) > 0) {
            temp[sizeof(temp) - 1] = 0;
            strcat_s(sympath, sizeof(sympath), temp);
            strcat_s(sympath, sizeof(sympath), ";");
        }
        if (GetEnvironmentVariableA("SYSTEMROOT", temp, sizeof(temp)) > 0) {
            temp[sizeof(temp) - 1] = 0;
            strcat_s(sympath, sizeof(sympath), temp);
            strcat_s(sympath, sizeof(sympath), ";");
            // also add the "system32"-directory:
            strcat_s(temp, sizeof(temp), "\\system32");
            strcat_s(sympath, sizeof(sympath), temp);
            strcat_s(sympath, sizeof(sympath), ";");
        }

        if ((ctx->options & SW_OPTIONS_SYMUSESERVER) != 0) {
            if (GetEnvironmentVariableA("SYSTEMDRIVE", temp, sizeof(temp)) > 0) {
                temp[sizeof(temp) - 1] = 0;
                strcat_s(sympath, sizeof(sympath), "SRV*");
                strcat_s(sympath, sizeof(sympath), temp);
                strcat_s(sympath, sizeof(sympath), "\\websymbols");
                strcat_s(sympath, sizeof(sympath), "*https://msdl.microsoft.com/download/symbols;");
            } else
                strcat_s(sympath, sizeof(sympath),
                         "SRV*c:\\websymbols*https://msdl.microsoft.com/download/symbols;");
        }
    }    // if SW_OPTIONS_SYMBUILDPATH

    // First Init the whole stuff...
    if (!sw__init_internal(&ctx->internal, sympath)) {
        SW_LOG_ERROR("Error initializing dbghelp.dll");
        SetLastError(ERROR_DLL_INIT_FAILED);
        ctx->fatal_error = true;
        return false;
    }

    if (sw__load_modules_internal(&ctx->internal, ctx->process, ctx->process_id)) {
        ctx->modules_loaded = true;
    }
    return ctx->modules_loaded;
}

SW_API_IMPL sw_context* sw_create_context_capture(uint32_t options, sw_callbacks callbacks, void* userptr)
{
    return sw__create_context(options, GetCurrentProcessId(), GetCurrentProcess(), NULL, callbacks, userptr);
}

SW_API_IMPL sw_context* sw_create_context_capture_other(uint32_t options, uint32_t process_id,
                                                        sw_sys_handle process, sw_callbacks callbacks,
                                                        void* userptr)
{
    SW_ASSERT(process_id);
    SW_ASSERT(process);

    return sw__create_context(options, process_id, process, NULL, callbacks, userptr);
}

SW_API_IMPL sw_context* sw_create_context_exception(uint32_t options, 
                                                    sw_exception_pointers exp_ptrs,
                                                    sw_callbacks callbacks, void* userptr)
{
    SW_ASSERT(exp_ptrs);

    PCONTEXT ctx_win = ((PEXCEPTION_POINTERS)exp_ptrs)->ContextRecord;
    return sw__create_context(options, GetCurrentProcessId(), GetCurrentProcess(), ctx_win, callbacks, userptr);
}

SW_API_IMPL sw_context* sw_create_context_catch(uint32_t options, sw_callbacks callbacks, void* userptr)
{
    PCONTEXT ctx_win = get_current_exception_context();
    SW_ASSERT(ctx_win);

    return sw__create_context(options, GetCurrentProcessId(), GetCurrentProcess(), ctx_win, callbacks, userptr);
}

SW_API_IMPL void sw_destroy_context(sw_context* ctx)
{
    if (ctx) {
        SW_FREE(ctx->internal.modules);
        DeleteCriticalSection(&ctx->internal.modules_cs);

        if (ctx->internal.fSymCleanup)
            ctx->internal.fSymCleanup(ctx->process);
        if (ctx->internal.dbg_help)
            FreeLibrary(ctx->internal.dbg_help);

        memset(ctx, 0x0, sizeof(*ctx));
        SW_FREE(ctx);
    }
}

SW_API_IMPL void sw_set_symbol_path(sw_context* ctx, const char* sympath)
{
    SW_ASSERT(ctx);
    SW_ASSERT(sympath);

    sw__strcpy(ctx->sympath, sizeof(ctx->sympath), sympath);
    ctx->options |= SW_OPTIONS_SYMBUILDPATH;
}

SW_API_IMPL void sw_set_callstack_limits(sw_context* ctx, uint32_t frames_to_skip, uint32_t frames_to_capture)
{
    ctx->frames_to_skip = frames_to_skip;
    ctx->frames_to_capture = frames_to_capture;
}

// The following is used to pass the "userData"-Pointer to the user-provided readMemoryFunction
// This has to be done due to a problem with the "hProcess"-parameter in x64...
// Because this class is in no case multi-threading-enabled (because of the limitations
// of dbghelp.dll) it is "safe" to use a static-variable
static ReadProcessMemoryRoutine_t   s_read_mem_func = NULL;
static LPVOID                       s_read_mem_func_userdata = NULL;

_SW_PRIVATE BOOL __stdcall sw__read_proc_mem(HANDLE  hProcess,
                                             DWORD64 qwBaseAddress,
                                             PVOID   lpBuffer,
                                             DWORD   nSize,
                                             LPDWORD lpNumberOfBytesRead)
{
    if (s_read_mem_func == NULL) {
        SIZE_T st;
        BOOL ret = ReadProcessMemory(hProcess, (LPVOID)qwBaseAddress, lpBuffer, nSize, &st);
        *lpNumberOfBytesRead = (DWORD)st;
        return ret;
    } else {
        return s_read_mem_func(hProcess, qwBaseAddress, lpBuffer, nSize, lpNumberOfBytesRead, s_read_mem_func_userdata);
    }
}

static bool sw_show_callstack_fast(sw_context* ctx, void* callbacks_userptr)
{
    sw_callstack_entry cs_entry;
    IMAGEHLP_LINE64 line;
    uint32_t max_frames = ctx->frames_to_capture != 0xffffffff ? ctx->frames_to_capture : SW_MAX_FRAMES;
    max_frames = max_frames > SW_MAX_FRAMES ? SW_MAX_FRAMES : max_frames;

    void** backtrace = (void**)alloca(sizeof(void*) * max_frames);
    if (!backtrace) {
        return false;
    }

    uint32_t frames_to_skip = ctx->frames_to_skip > 2 ? ctx->frames_to_skip : 2;
    USHORT num_captures = RtlCaptureStackBackTrace(frames_to_skip, max_frames, backtrace, NULL);
    if (num_captures == 0) {
        return true;
    }

    IMAGEHLP_SYMBOL64* symbol = (IMAGEHLP_SYMBOL64*)alloca(sizeof(IMAGEHLP_SYMBOL64) + SW_MAX_NAME_LEN);
    if (!symbol) {
        return false;
    }
    memset(symbol, 0, sizeof(IMAGEHLP_SYMBOL64) + SW_MAX_NAME_LEN);
    symbol->SizeOfStruct = sizeof(IMAGEHLP_SYMBOL64);
    symbol->MaxNameLength = SW_MAX_NAME_LEN;

    if (ctx->callbacks.callstack_begin) {
        ctx->callbacks.callstack_begin(callbacks_userptr);
    }

    for (USHORT i = 0; i < num_captures; i++) {
        memset(&cs_entry, 0x0, sizeof(cs_entry));
        if ((ctx->options & SW_OPTIONS_SYMBOL) && ctx->internal.fSymGetSymFromAddr64 != NULL) {
            if (ctx->internal.fSymGetSymFromAddr64(ctx->process, (DWORD64)backtrace[i], &(cs_entry.offset_from_symbol), symbol) != FALSE) {

                sw__strcpy(cs_entry.name, sizeof(cs_entry.name), symbol->Name);
                sw__strcpy(cs_entry.und_name, sizeof(cs_entry.und_name), symbol->Name);
            } else {
                DWORD gle = GetLastError();
                if (gle == ERROR_INVALID_ADDRESS)
                    break;
                else if (gle == ERROR_MOD_NOT_FOUND)
                    continue;
                                      
                SW_LOG_ERROR("SymGetSymFromAddr64 failed (ErrorCode=%lu)", gle);
                break;
            }
        }

        // show line number info, NT5.0-method (SymGetLineFromAddr64())
        if ((ctx->options && SW_OPTIONS_SOURCEPOS) && ctx->internal.fSymGetLineFromAddr64 != NULL) {    // yes, we have SymGetLineFromAddr64()
            if (ctx->internal.fSymGetLineFromAddr64(ctx->process, (DWORD64)backtrace[i], (PDWORD)&(cs_entry.offset_from_line), &line) != FALSE) {
                cs_entry.line = line.LineNumber;
                sw__strcpy(cs_entry.line_filename, SW_MAX_NAME_LEN, line.FileName);
            } else {
                DWORD gle = GetLastError();
                if (gle == ERROR_INVALID_ADDRESS)
                    break;
                else if (gle == ERROR_MOD_NOT_FOUND)
                    continue;

                SW_LOG_ERROR("SymGetLineFromAddr64 failed (ErrorCode=%lu)", gle);
                break;
            }
        }    // yes, we have SymGetLineFromAddr64()

        // show module info (SymGetModuleInfo64())
        if (ctx->options & SW_OPTIONS_MODULEINFO) {
            if (!sw__get_module_info_csentry(&ctx->internal, ctx->process, (uint64_t)backtrace[i], &cs_entry)) {    // got module info OK
                SW_LOG_ERROR("SymGetModuleInfo64 failed (ErrorCode=%lu)", GetLastError());
            }
        }        

        if (ctx->callbacks.callstack_entry) {
            ctx->callbacks.callstack_entry(&cs_entry, callbacks_userptr);
        }        
    }

    if (ctx->callbacks.callstack_end) {
        ctx->callbacks.callstack_end(callbacks_userptr);
    }    

    return true;
}

SW_API_IMPL bool sw_show_callstack_userptr(sw_context* ctx, sw_sys_handle thread_hdl, void* callbacks_userptr)
{
    CONTEXT c;
    callbacks_userptr = callbacks_userptr ? callbacks_userptr : ctx->callbacks_userptr;

    if (thread_hdl == NULL) {
        thread_hdl = GetCurrentThread();
    }

    if (!ctx->modules_loaded) {
        sw__load_modules(ctx); 
    }

    if (ctx->reload_modules) {
        sw__load_modules_internal(&ctx->internal, ctx->process, ctx->process_id);
        ctx->reload_modules = false;
    }

    if (ctx->internal.dbg_help == NULL) {
        SetLastError(ERROR_DLL_INIT_FAILED);
        return false;
    }

    // If no context is provided, capture the context
    // See: https://stackwalker.codeplex.com/discussions/446958
    if (GetThreadId(thread_hdl) == GetCurrentThreadId()) {
        if (ctx->internal.ctx.ContextFlags != 0) {
            c = ctx->internal.ctx;    // context taken at Init
        } else {
            // Take the faster path
            return sw_show_callstack_fast(ctx, callbacks_userptr);
        }
    } else {
        SuspendThread(thread_hdl);
        memset(&c, 0, sizeof(CONTEXT));
        c.ContextFlags = USED_CONTEXT_FLAGS;

        // TODO: Detect if you want to get a thread context of a different process, which is
        // running a different processor architecture... This does only work if we are x64 and
        // the target process is x64 or x86; It cannot work, if this process is x64 and the
        // target process is x64... this is not supported... See also:
        // http://www.howzatt.demon.co.uk/articles/DebuggingInWin64.html
        if (GetThreadContext(thread_hdl, &c) == FALSE) {
            ResumeThread(thread_hdl);
            return false;
        }
    }

    sw_callstack_entry cs_entry;
    IMAGEHLP_SYMBOL64* symbol = NULL;
    IMAGEHLP_LINE64 line;
    uint32_t frame_num;
    uint32_t cur_recursion_count = 0;

    // init STACKFRAME for first call
    STACKFRAME64 s;    // in/out stackframe
    memset(&s, 0, sizeof(s));
    DWORD image_type;
#ifdef _M_IX86
    // normally, call ImageNtHeader() and use machine info from PE header
    image_type = IMAGE_FILE_MACHINE_I386;
    s.AddrPC.Offset = c.Eip;
    s.AddrPC.Mode = AddrModeFlat;
    s.AddrFrame.Offset = c.Ebp;
    s.AddrFrame.Mode = AddrModeFlat;
    s.AddrStack.Offset = c.Esp;
    s.AddrStack.Mode = AddrModeFlat;
#elif _M_X64
    image_type = IMAGE_FILE_MACHINE_AMD64;
    s.AddrPC.Offset = c.Rip;
    s.AddrPC.Mode = AddrModeFlat;
    s.AddrFrame.Offset = c.Rsp;
    s.AddrFrame.Mode = AddrModeFlat;
    s.AddrStack.Offset = c.Rsp;
    s.AddrStack.Mode = AddrModeFlat;
#elif _M_IA64
    image_type = IMAGE_FILE_MACHINE_IA64;
    s.AddrPC.Offset = c.StIIP;
    s.AddrPC.Mode = AddrModeFlat;
    s.AddrFrame.Offset = c.IntSp;
    s.AddrFrame.Mode = AddrModeFlat;
    s.AddrBStore.Offset = c.RsBSP;
    s.AddrBStore.Mode = AddrModeFlat;
    s.AddrStack.Offset = c.IntSp;
    s.AddrStack.Mode = AddrModeFlat;
#else
#   error "Platform not supported!"
#endif

    symbol = (IMAGEHLP_SYMBOL64*)alloca(sizeof(IMAGEHLP_SYMBOL64) + SW_MAX_NAME_LEN);
    if (!symbol) {
        ResumeThread(thread_hdl);
        return false;
    }
    memset(symbol, 0, sizeof(IMAGEHLP_SYMBOL64) + SW_MAX_NAME_LEN);
    symbol->SizeOfStruct = sizeof(IMAGEHLP_SYMBOL64);
    symbol->MaxNameLength = SW_MAX_NAME_LEN;

    memset(&line, 0, sizeof(line));
    line.SizeOfStruct = sizeof(line);

    uint32_t frames_to_skip = ctx->frames_to_skip > 1 ? ctx->frames_to_skip : 1;
    uint32_t max_frames = ctx->frames_to_capture != 0xffffffff ? ctx->frames_to_capture : SW_MAX_FRAMES;
    max_frames = max_frames > SW_MAX_FRAMES ? SW_MAX_FRAMES : max_frames;

    if (ctx->callbacks.callstack_begin) {
        ctx->callbacks.callstack_begin(callbacks_userptr);
    }

    for (frame_num = 0;; ++frame_num) {
        // get next stack frame (StackWalk64(), SymFunctionTableAccess64(), SymGetModuleBase64())
        // if this returns ERROR_INVALID_ADDRESS (487) or ERROR_NOACCESS (998), you can
        // assume that either you are done, or that the stack is so hosed that the next
        // deeper frame could not be found.
        // CONTEXT need not to be supplied if imageTyp is IMAGE_FILE_MACHINE_I386!
        if (!ctx->internal.fStackWalk64(image_type, ctx->process, thread_hdl, &s, &c, sw__read_proc_mem,
                                        ctx->internal.fSymFunctionTableAccess64,
                                        ctx->internal.fSymGetModuleBase64, NULL)) {
            // INFO: "StackWalk64" does not set "GetLastError"...
            SW_LOG_ERROR("StackWalk64 failed");
            break;
        }

        if (frame_num < frames_to_skip) {
            continue;
        }
        if (frame_num >= max_frames) {
            break;
        }

        cs_entry.offset = s.AddrPC.Offset;
        cs_entry.name[0] = 0;
        cs_entry.und_name[0] = 0;
        cs_entry.offset_from_symbol = 0;
        cs_entry.offset_from_line = 0;
        cs_entry.line_filename[0] = 0;
        cs_entry.line = 0;
        cs_entry.loaded_image_name[0] = 0;
        cs_entry.module_name[0] = 0;
        if (s.AddrPC.Offset == s.AddrReturn.Offset) {
            if ((ctx->max_recursion > 0) && (cur_recursion_count > ctx->max_recursion)) {
                SW_LOG_ERROR("StackWalk64-Endless-Callstack!");
                break;
            }
            cur_recursion_count++;
        } else {
            cur_recursion_count = 0;
        }

        if (s.AddrPC.Offset != 0) {
            // we seem to have a valid PC
            // show procedure info (SymGetSymFromAddr64())
            if ((ctx->options & SW_OPTIONS_SYMBOL) && ctx->internal.fSymGetSymFromAddr64 != NULL) {
                if (ctx->internal.fSymGetSymFromAddr64(ctx->process, s.AddrPC.Offset, &(cs_entry.offset_from_symbol), symbol) != FALSE) {

                    sw__strcpy(cs_entry.name, SW_MAX_NAME_LEN, symbol->Name);
                    ctx->internal.fUnDecorateSymbolName(symbol->Name, cs_entry.und_name, SW_MAX_NAME_LEN, UNDNAME_COMPLETE);
                } else {
                    DWORD gle = GetLastError();
                    if (gle == ERROR_INVALID_ADDRESS)
                        break;
                    else if (gle == ERROR_MOD_NOT_FOUND)
                        continue;
                    SW_LOG_ERROR("SymGetSymFromAddr64 failed (ErrorCode=%lu)", gle);
                }
            }

            // show line number info, NT5.0-method (SymGetLineFromAddr64())
            if ((ctx->options && SW_OPTIONS_SOURCEPOS) && ctx->internal.fSymGetLineFromAddr64 != NULL) {    // yes, we have SymGetLineFromAddr64()
                if (ctx->internal.fSymGetLineFromAddr64(ctx->process, s.AddrPC.Offset, (PDWORD)&(cs_entry.offset_from_line), &line) != FALSE) {
                    cs_entry.line = line.LineNumber;
                    sw__strcpy(cs_entry.line_filename, SW_MAX_NAME_LEN, line.FileName);
                } else {
                    DWORD gle = GetLastError();
                    if (gle == ERROR_INVALID_ADDRESS)
                        break;
                    else if (gle == ERROR_MOD_NOT_FOUND)
                        continue;
                    SW_LOG_ERROR("SymGetLineFromAddr64 (ErrorCode=%lu)", gle);
                }
            }    // yes, we have SymGetLineFromAddr64()

            // show module info (SymGetModuleInfo64())
            if (ctx->options & SW_OPTIONS_MODULEINFO) {
                if (!sw__get_module_info_csentry(&ctx->internal, ctx->process, s.AddrPC.Offset, &cs_entry)) {    // got module info OK
                    SW_LOG_ERROR("SymGetModuleInfo64 (ErrorCode=%lu)", GetLastError());
                }
            }
        }    // s.AddrPC.Offset != 0

        // we skip the first one, because it's always from this function
        if (ctx->callbacks.callstack_entry) {
            ctx->callbacks.callstack_entry(&cs_entry, callbacks_userptr);
        }
    }    // for ( frame_num )

    if (ctx->callbacks.callstack_end && frame_num > 1 ) {
        ctx->callbacks.callstack_end(callbacks_userptr);
    }

    return true;
}

SW_API_IMPL bool sw_show_callstack(sw_context* ctx, sw_sys_handle thread_hdl)
{
    return sw_show_callstack_userptr(ctx, thread_hdl, NULL);
}

SW_API_IMPL uint16_t sw_capture_current(sw_context* ctx, void* symbols[SW_MAX_FRAMES])
{
    SW_ASSERT(ctx);

    uint32_t max_frames = ctx->frames_to_capture != 0xffffffff ? ctx->frames_to_capture : SW_MAX_FRAMES;
    max_frames = max_frames > SW_MAX_FRAMES ? SW_MAX_FRAMES : max_frames;

    uint32_t frames_to_skip = ctx->frames_to_skip > 1 ? ctx->frames_to_skip : 1;
    return (uint16_t)RtlCaptureStackBackTrace(frames_to_skip, max_frames, symbols, NULL);
}

SW_API_IMPL uint16_t sw_resolve_callstack(sw_context* ctx, void* symbols[SW_MAX_FRAMES], 
                                          sw_callstack_entry entries[SW_MAX_FRAMES], uint16_t num_entries)
{
    SW_ASSERT(ctx);
    IMAGEHLP_LINE64 line;

    if (!sw__load_modules(ctx)) {
        return 0;
    }

    if (ctx->reload_modules) {
        sw__load_modules_internal(&ctx->internal, ctx->process, ctx->process_id);
        ctx->reload_modules = false;
    }

    IMAGEHLP_SYMBOL64* symbol = (IMAGEHLP_SYMBOL64*)alloca(sizeof(IMAGEHLP_SYMBOL64) + SW_MAX_NAME_LEN);
    if (!symbol) {
        return false;
    }
    memset(symbol, 0, sizeof(IMAGEHLP_SYMBOL64) + SW_MAX_NAME_LEN);
    symbol->SizeOfStruct = sizeof(IMAGEHLP_SYMBOL64);
    symbol->MaxNameLength = SW_MAX_NAME_LEN;

    uint16_t count = 0;
    for (uint16_t i = 0; i < num_entries; i++) {
        sw_callstack_entry entry = {0};
        if ((ctx->options & SW_OPTIONS_SYMBOL) && ctx->internal.fSymGetSymFromAddr64 != NULL) {
            if (ctx->internal.fSymGetSymFromAddr64(ctx->process, (DWORD64)symbols[i], &(entry.offset_from_symbol), symbol) != FALSE) {

                sw__strcpy(entry.name, sizeof(entry.name), symbol->Name);
                sw__strcpy(entry.und_name, sizeof(entry.und_name), symbol->Name);
            } else {
                DWORD gle = GetLastError();
                if (gle == ERROR_INVALID_ADDRESS || gle == ERROR_MOD_NOT_FOUND) {
                    continue;
                }                                       
                SW_LOG_ERROR("SymGetSymFromAddr64 failed (ErrorCode=%lu)", gle);
                break;
            }
        }

        // show line number info, NT5.0-method (SymGetLineFromAddr64())
        if ((ctx->options && SW_OPTIONS_SOURCEPOS) && ctx->internal.fSymGetLineFromAddr64 != NULL) {    // yes, we have SymGetLineFromAddr64()
            if (ctx->internal.fSymGetLineFromAddr64(ctx->process, (DWORD64)symbols[i], 
                                                    (PDWORD)&(entry.offset_from_line), &line) != FALSE) 
            {
                entry.line = line.LineNumber;
                sw__strcpy(entry.line_filename, SW_MAX_NAME_LEN, line.FileName);
            } else {
                DWORD gle = GetLastError();
                if (gle == ERROR_INVALID_ADDRESS || gle == ERROR_MOD_NOT_FOUND) {
                    continue;
                }
                SW_LOG_ERROR("SymGetLineFromAddr64 failed (ErrorCode=%lu)", gle);
                break;
            }
        }    // yes, we have SymGetLineFromAddr64()

        // show module info (SymGetModuleInfo64())
        if (ctx->options & SW_OPTIONS_MODULEINFO) {
            if (!sw__get_module_info_csentry(&ctx->internal, ctx->process, (uint64_t)symbols[i], &entry)) {
                SW_LOG_ERROR("SymGetModuleInfo64 failed (ErrorCode=%lu)", GetLastError());
            }
        }        

        memcpy(&entries[count++], &entry, sizeof(sw_callstack_entry));
    }

    return count;
}

SW_API_IMPL void sw_reload_modules(sw_context* ctx)
{
    SW_ASSERT(ctx);
    ctx->reload_modules = true;
}

SW_API_IMPL bool sw_get_symbol_module(sw_context* ctx, void* symbol, char module_name[32])
{
    if (!sw__load_modules(ctx)) {
        return false;
    }

    if (ctx->reload_modules) {
        sw__load_modules_internal(&ctx->internal, ctx->process, ctx->process_id);
        ctx->reload_modules = false;
    }

    SW_ASSERT(ctx);
    if (ctx->options & SW_OPTIONS_MODULEINFO) {
        IMAGEHLP_MODULE64_V3 modinfo;
        if (!sw__get_module_info(&ctx->internal, ctx->process, (DWORD64)symbol, &modinfo)) {   
            SW_LOG_ERROR("SymGetModuleInfo64 failed (ErrorCode=%lu)", GetLastError());
        } else {
            sw__strcpy(module_name, 32, modinfo.ModuleName);
            return true;
        }
    }
    return false;    
}

SW_API_IMPL void sw_set_dbghelp_hintpath(const char* path)
{
    sw__strcpy(sw__dbghelp_hintpath, sizeof(sw__dbghelp_hintpath), path);
}

#endif // SW_IMPL
