//
// Copyright 2018 Sepehr Taghdisian (septag@github). All rights reserved.
// License: https://github.com/septag/sx#license-bsd-2-clause
//

#include "sx/vmem.h"
#include "sx/os.h"

#if SX_PLATFORM_WINDOWS
#    define VC_EXTRALEAN
#    define WIN32_LEAN_AND_MEAN
#    include <windows.h>
#elif SX_PLATFORM_POSIX
#    include <sys/mman.h>
#    include <unistd.h>
#    if defined(MAP_ANON) && !defined(MAP_ANONYMOUS)
#        define MAP_ANONYMOUS MAP_ANON
#    endif
#endif


size_t sx_vmem_get_bytes(int num_pages)
{
    return sx_os_pagesz() * num_pages;
}

int sx_vmem_get_needed_pages(size_t bytes)
{
    size_t page_sz = sx_os_pagesz();
    size_t page_cnt = (bytes + page_sz - 1) / page_sz;
    sx_assert(page_cnt < INT32_MAX);
    return (int)page_cnt;
}

#if SX_PLATFORM_WINDOWS
bool sx_vmem_init(sx_vmem_context* vmem, sx_vmem_flags flags, int max_pages)
{
    sx_assert(vmem);
    sx_assert(max_pages > 0);

    vmem->page_size = (int)sx_os_pagesz();
    vmem->num_pages = 0;
    vmem->max_pages = max_pages;
    vmem->ptr =
        VirtualAlloc(NULL, (size_t)vmem->page_size * (size_t)max_pages,
                     MEM_RESERVE | ((flags & SX_VMEM_WATCH) ? MEM_WRITE_WATCH : 0), PAGE_READWRITE);

    if (!vmem->ptr) {
        return false;
    }

    return true;
}

void sx_vmem_release(sx_vmem_context* vmem)
{
    sx_assert(vmem);

    if (vmem->ptr) {
        BOOL r = VirtualFree(vmem->ptr, 0, MEM_RELEASE);
        sx_unused(r);
        sx_assert(r);
    }
    vmem->num_pages = vmem->max_pages = 0;
}

void* sx_vmem_commit_page(sx_vmem_context* vmem, int page_id)
{
    sx_assert(vmem);
    sx_assert(vmem->ptr);
    
    if (page_id >= vmem->max_pages || vmem->num_pages == vmem->max_pages) {
        return NULL;
    }

    void* ptr = (uint8_t*)vmem->ptr + vmem->page_size * page_id;
    if (!VirtualAlloc(ptr, vmem->page_size, MEM_COMMIT, PAGE_READWRITE)) {
        return NULL;
    }

    ++vmem->num_pages;

    return ptr;
}

void sx_vmem_free_page(sx_vmem_context* vmem, int page_id)
{
    sx_assert(vmem);
    sx_assert(vmem->ptr);
    sx_assert(page_id < vmem->max_pages);
    sx_assert(vmem->num_pages > 0);

    void* ptr = (uint8_t*)vmem->ptr + vmem->page_size * page_id;
    BOOL r = VirtualFree(ptr, vmem->page_size, MEM_DECOMMIT);
    sx_unused(r);
    sx_assert(r);
    --vmem->num_pages;
}

void* sx_vmem_commit_pages(sx_vmem_context* vmem, int start_page_id, int num_pages)
{
    sx_assert(vmem);
    sx_assert(vmem->ptr);
    
    if ((start_page_id + num_pages) > vmem->max_pages ||
        (vmem->num_pages + num_pages) > vmem->max_pages)
    {
        return NULL;
    }

    void* ptr = (uint8_t*)vmem->ptr + vmem->page_size * start_page_id;
    if (!VirtualAlloc(ptr, vmem->page_size*num_pages, MEM_COMMIT, PAGE_READWRITE)) {
        return NULL;
    }
    vmem->num_pages += num_pages;

    return ptr;
}

void sx_vmem_free_pages(sx_vmem_context* vmem, int start_page_id, int num_pages)
{
    sx_assert(vmem);
    sx_assert(vmem->ptr);
    sx_assert((start_page_id + num_pages) <= vmem->max_pages);
    sx_assert(vmem->num_pages >= num_pages);

    if (num_pages > 0) {
        void* ptr = (uint8_t*)vmem->ptr + vmem->page_size * start_page_id;
        BOOL r = VirtualFree(ptr, (size_t)vmem->page_size*(size_t)num_pages, MEM_DECOMMIT);
        sx_unused(r);
        sx_assert(r);
        vmem->num_pages -= num_pages;
    }
}

void* sx_vmem_get_page(sx_vmem_context* vmem, int page_id)
{
    sx_assert(vmem);
    sx_assert(vmem->ptr);
    sx_assert(page_id < vmem->max_pages);
    
    return (uint8_t*)vmem->ptr + vmem->page_size * page_id;
}

size_t sx_vmem_commit_size(sx_vmem_context* vmem)
{
    return (size_t)vmem->page_size * (size_t)vmem->num_pages;
}


#endif // SX_PLATFORM_WINDOWS