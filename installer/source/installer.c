// golden
// 6/12/2018
//

#include "installer.h"

extern uint8_t kernelelf[];
extern int32_t kernelelf_size;

extern uint8_t debuggerbin[];
extern int32_t debuggerbin_size;

void patch_kernel() {
    cpu_disable_wp();

    uint64_t kernbase = get_kbase();

    // patch memcpy first
    *(uint8_t *)(kernbase + 0x1EA53D) = 0xEB;

    // patch sceSblACMgrIsAllowedSystemLevelDebugging
    memcpy((void *)(kernbase + 0x11730), "\x48\xC7\xC0\x01\x00\x00\x00\xC3", 8);

    // patch sceSblACMgrHasMmapSelfCapability
    memcpy((void *)(kernbase + 0x117B0), "\x48\xC7\xC0\x01\x00\x00\x00\xC3", 8);

    // patch sceSblACMgrIsAllowedToMmapSelf
    memcpy((void *)(kernbase + 0x117C0), "\x48\xC7\xC0\x01\x00\x00\x00\xC3", 8);

    // disable sysdump_perform_dump_on_fatal_trap
    // will continue execution and give more information on crash, such as rip
    *(uint8_t *)(kernbase + 0x7673E0) = 0xC3;

    // self patches
    memcpy((void *)(kernbase + 0x13F03F), "\x31\xC0\x90\x90\x90", 5);

    // patch vm_map_protect check
    memcpy((void *)(kernbase + 0x1A3C08), "\x90\x90\x90\x90\x90\x90", 6);

    // patch ptrace, thanks 2much4u
    *(uint8_t *)(kernbase + 0x30D9AA) = 0xEB;

    // remove all these bullshit checks from ptrace, by golden
    memcpy((void *)(kernbase + 0x30DE01), "\xE9\xD0\x00\x00\x00", 5);

    // patch ASLR, thanks 2much4u
    *(uint16_t *)(kernbase + 0x194875) = 0x9090;

    // patch kmem_alloc
    *(uint8_t *)(kernbase + 0xFCD48) = VM_PROT_ALL;
    *(uint8_t *)(kernbase + 0xFCD56) = VM_PROT_ALL;

    cpu_enable_wp();
}

void *rwx_alloc(uint64_t size) {
    uint64_t alignedSize = (size + 0x3FFFull) & ~0x3FFFull;
    return (void *)kmem_alloc(*kernel_map, alignedSize);
}

int load_kdebugger() {
    uint64_t mapsize;
    void *kmemory;
    int (*payload_entry)(void *p);

    // calculate mapped size
    if (elf_mapped_size(kernelelf, &mapsize)) {
        printf("[webrte] invalid kdebugger elf!\n");
        return 1;
    }
    
    // allocate memory
    kmemory = rwx_alloc(mapsize);
    if(!kmemory) {
        printf("[webrte] could not allocate memory for kdebugger!\n");
        return 1;
    }

    // load the elf
    if (load_elf(kernelelf, kernelelf_size, kmemory, mapsize, (void **)&payload_entry)) {
        printf("[webrte] could not load kdebugger elf!\n");
        return 1;
    }

    // call entry
    if (payload_entry(NULL)) {
        return 1;
    }

    return 0;
}

int load_debugger() {
    struct proc *p;
    struct vmspace *vm;
    struct vm_map *map;
    int r;

    p = proc_find_by_name("SceShellCore");
    if(!p) {
        printf("[webrte] could not find SceShellCore process!\n");
        return 1;
    }

    vm = p->p_vmspace;
    map = &vm->vm_map;

    // allocate some memory
    vm_map_lock(map);
    r = vm_map_insert(map, NULL, NULL, PAYLOAD_BASE, PAYLOAD_BASE + 0x400000, VM_PROT_ALL, VM_PROT_ALL, 0);
    vm_map_unlock(map);
    if(r) {
        printf("[webrte] failed to allocate payload memory!\n");
        return r;
    }

    // write the payload
    r = proc_write_mem(p, (void *)PAYLOAD_BASE, debuggerbin_size, debuggerbin, NULL);
    if(r) {
        printf("[webrte] failed to write payload!\n");
        return r;
    }

    // create a thread
    r = proc_create_thread(p, PAYLOAD_BASE);
    if(r) {
        printf("[webrte] failed to create payload thread!\n");
        return r;
    }

    return 0;
}

int runinstaller() {
    init_ksdk();

    // enable uart
    *disable_console_output = 0;

    printf("[webrte] installer running\n");

    // patch the kernel
    printf("[webrte] patching kernel...\n");
    patch_kernel();

    printf("[webrte] loading kdebugger...\n");

    if(load_kdebugger()) {
        return 1;
    }

    printf("[webrte] loading webrte...\n");

    if(load_debugger()) {
        return 1;
    }

    printf("[webrte] webrte created by golden\n");
    
    return 0;
}
