#include "StealthEvents.h"
#include <cassert>
#include <cstring>
#include <cstdio>
int main() {
    auto base=static_cast<BYTE*>(VirtualAlloc(nullptr,0x1880000,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE));
    assert(base);
    // Synthetic x86 functions reproduce the verified relocated prologues and
    // calling conventions, then return immediately. Never attaches to the game.
    const BYTE detection[]={0x53,0x56,0x57,0x8b,0xf9,0x8b,0x47,0x0c,0xff,0x01,0x5f,0x5e,0x5b,0xc2,0x04,0x00};
    const BYTE alarm[]={0x56,0x8b,0x74,0x24,0x0c,0x5e,0xc3};
    const BYTE restore[]={0x51,0x8b,0x54,0x24,0x08,0x59,0xb0,0x01,0xc2,0x04,0x00};
    memcpy(base+0x2d6920,detection,sizeof(detection));
    memcpy(base+0x2d75b0,alarm,sizeof(alarm));
    memcpy(base+0x2d8330,restore,sizeof(restore));
    base[0x2d8330]=0x90;
    assert(!StealthEvents::Install(reinterpret_cast<uintptr_t>(base)));
    assert(base[0x2d6920]==0x53); // Reject mismatch before patching any entry.
    base[0x2d8330]=0x51;
    assert(StealthEvents::Install(reinterpret_cast<uintptr_t>(base)));
    auto detect=reinterpret_cast<void(__thiscall*)(void*,const char*)>(base+0x2d6920);
    auto alarmEvent=reinterpret_cast<void(__cdecl*)(void*,void*)>(base+0x2d75b0);
    auto load=reinterpret_cast<bool(__thiscall*)(void*,void*)>(base+0x2d8330);
    unsigned manager[4]={}; int payload=1;
    // No objective list exists: the monitor must still retain both events.
    detect(manager,"police");alarmEvent(manager,&payload);
    auto s=StealthEvents::Read();assert(s.available&&s.detections==1&&s.alarms==1);
    assert(manager[0]==1); // Original function ran with the original this pointer.
    base[0x1879da0]=1;
    detect(manager,"police");alarmEvent(manager,&payload);
    s=StealthEvents::Read();assert(s.detections==1&&s.alarms==1&&manager[0]==2);
    assert(load(manager,&payload));
    s=StealthEvents::Read();assert(s.detections==0&&s.alarms==0&&s.restores==1);
    base[0x1879da0]=0;
    for(int i=0;i<10000;i++){detect(manager,"police");alarmEvent(manager,&payload);}
    s=StealthEvents::Read();assert(s.detections==10000&&s.alarms==10000&&manager[0]==10002);
    puts("PASS: x86 calling conventions, original forwarding, empty objectives, loading suppression, restore reset, mismatch rejection, 10000 repeated calls. Not a gameplay test.");
}
