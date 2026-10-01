#include "StealthEvents.h"
#include <cstring>

namespace StealthEvents {
namespace {
uintptr_t gameBase;
volatile LONG detections=0, alarms=0, restores=0;
bool installed=false;
using Detection = void (__thiscall*)(void*, const char*);
using Alarm = void (__cdecl*)(void*, void*);
using Restore = bool (__thiscall*)(void*, void*);
Detection originalDetection;
Alarm originalAlarm;
Restore originalRestore;

bool Playing() {
    BYTE loading=1; SIZE_T got=0;
    return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(gameBase+0x1879DA0),
        &loading,1,&got) && got==1 && loading==0;
}
void __fastcall DetectionHook(void* manager, void*, const char* group) {
    // Runs before the game's active-objective loop, including an empty loop.
    if(manager && group && Playing())InterlockedIncrement(&detections);
    originalDetection(manager,group);
}
void __cdecl AlarmHook(void* manager, void* event) {
    // The registered 0x8f handler is invoked independently of bonus eligibility.
    if(manager && event && Playing())InterlockedIncrement(&alarms);
    originalAlarm(manager,event);
}
bool __fastcall RestoreHook(void* objective, void*, void* stream) {
    const bool result=originalRestore(objective,stream);
    // Restoring objectives is a reliable load boundary even if rendering never
    // sees the loading byte. Historical custom observations are not in saves.
    InterlockedExchange(&detections,0);
    InterlockedExchange(&alarms,0);
    InterlockedIncrement(&restores);
    return result;
}
struct Patch {
    uintptr_t rva; unsigned length; BYTE bytes[8]; void* hook; BYTE* trampoline;
};
void Jump(BYTE* at, const void* target) {
    at[0]=0xe9;
    const auto offset=static_cast<uint32_t>(reinterpret_cast<uintptr_t>(target)-reinterpret_cast<uintptr_t>(at)-5);
    memcpy(at+1,&offset,4);
}
}
bool Install(uintptr_t base) {
    if(installed)return true;
    // Exact instruction boundaries; copied instructions contain no relative
    // branches or absolute addresses. Caller verifies the complete EXE hash.
    Patch patches[]={
        {0x2d6920,8,{0x53,0x56,0x57,0x8b,0xf9,0x8b,0x47,0x0c},reinterpret_cast<void*>(DetectionHook)},
        {0x2d75b0,5,{0x56,0x8b,0x74,0x24,0x0c},reinterpret_cast<void*>(AlarmHook)},
        {0x2d8330,5,{0x51,0x8b,0x54,0x24,0x08},reinterpret_cast<void*>(RestoreHook)}
    };
    for(auto& p:patches)if(memcmp(reinterpret_cast<void*>(base+p.rva),p.bytes,p.length))return false;
    // Allocate all trampolines before changing any game code.
    for(auto& p:patches) {
        p.trampoline=static_cast<BYTE*>(VirtualAlloc(nullptr,32,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
        if(!p.trampoline){for(auto& q:patches)if(q.trampoline)VirtualFree(q.trampoline,0,MEM_RELEASE);return false;}
        memcpy(p.trampoline,p.bytes,p.length);
        Jump(p.trampoline+p.length,reinterpret_cast<void*>(base+p.rva+p.length));
        DWORD old;
        if(!VirtualProtect(p.trampoline,32,PAGE_EXECUTE_READ,&old)) {
            for(auto& q:patches)if(q.trampoline)VirtualFree(q.trampoline,0,MEM_RELEASE);
            return false;
        }
        FlushInstructionCache(GetCurrentProcess(),p.trampoline,32);
    }
    gameBase=base;
    originalDetection=reinterpret_cast<Detection>(patches[0].trampoline);
    originalAlarm=reinterpret_cast<Alarm>(patches[1].trampoline);
    originalRestore=reinterpret_cast<Restore>(patches[2].trampoline);
    unsigned applied=0;
    for(auto& p:patches) {
        DWORD old;
        if(!VirtualProtect(reinterpret_cast<void*>(base+p.rva),p.length,PAGE_EXECUTE_READWRITE,&old))break;
        BYTE code[8]; memset(code,0x90,sizeof(code));
        // Compute the relative displacement against the destination address.
        code[0]=0xe9;
        uint32_t offset=static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p.hook)-(base+p.rva)-5);
        memcpy(code+1,&offset,4);
        memcpy(reinterpret_cast<void*>(base+p.rva),code,p.length);
        DWORD ignored;VirtualProtect(reinterpret_cast<void*>(base+p.rva),p.length,old,&ignored);
        FlushInstructionCache(GetCurrentProcess(),reinterpret_cast<void*>(base+p.rva),p.length);
        ++applied;
    }
    if(applied!=3) {
        // Startup-only installation: no gameplay threads may be executing here.
        for(unsigned i=0;i<applied;i++) {
            auto& p=patches[i];DWORD old,ignored;
            if(VirtualProtect(reinterpret_cast<void*>(base+p.rva),p.length,PAGE_EXECUTE_READWRITE,&old)) {
                memcpy(reinterpret_cast<void*>(base+p.rva),p.bytes,p.length);
                VirtualProtect(reinterpret_cast<void*>(base+p.rva),p.length,old,&ignored);
                FlushInstructionCache(GetCurrentProcess(),reinterpret_cast<void*>(base+p.rva),p.length);
            }
        }
        // Keep trampolines alive if rollback could not restore a patched entry.
        return false;
    }
    installed=true;return true;
}
Sample Read() {
    return {installed,InterlockedCompareExchange(&detections,0,0),
        InterlockedCompareExchange(&alarms,0,0),InterlockedCompareExchange(&restores,0,0)};
}
}
