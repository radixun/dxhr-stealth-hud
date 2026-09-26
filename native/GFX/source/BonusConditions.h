#pragma once
#include <array>
#include <cstdint>
#include <cstring>

// Verified Steam EXE only. See BONUS-RESEARCH.md for the disassembly chain.
// Active objective list, NOT the selected journal entry or NPC combat state.
namespace BonusConditions {
enum State { Unknown, Valid, Failed, NotApplicable, Mixed };
struct Result {
    bool readable=false;
    std::array<unsigned,2> total{}, failed{};
    State state(unsigned i) const {
        if(!readable)return Unknown;
        if(!total[i])return NotApplicable;
        if(!failed[i])return Valid;
        return failed[i]==total[i]?Failed:Mixed;
    }
};
struct Node { uint32_t next,previous,object; };
struct Objective {
    uint32_t config,id,state,pad,parent,manager,unknown;
    uint8_t ghost,op,padding[2];
};
static_assert(sizeof(Objective)==32 && sizeof(Node)==12);

// Reader returns false on unreadable/short reads. No game functions are called.
template<class Reader> Result Poll(uint32_t base,Reader read) {
    Result result;
    auto get=[&](uint32_t address,auto& value){
        return address>=0x10000 && uint64_t(address)+sizeof(value)<=0x100000000ULL
            && read(address,&value,sizeof(value));
    };
    uint32_t root=0,index=0,world=0,manager=0,head=0;
    if(!get(base+0x1607B1C,root)||!root||!get(root+8,index)||index!=0
        ||!get(root,world)||!world||!get(world+0x30,manager)||!manager
        ||!get(manager+12,head))return {};
    uint32_t node=0;
    if(head&&!get(head,node))return {};
    std::array<uint32_t,64> seen{};
    unsigned count=0;
    const uint32_t first=node;
    while(node!=head) {
        if(!node||count==seen.size())return {};
        for(unsigned i=0;i<count;i++)if(seen[i]==node)return {};
        seen[count++]=node;
        Node link{},again{}; Objective obj{},check{};
        std::array<uint8_t,36> config{};
        if(!get(node,link)||!link.object||!get(link.object,obj)
            ||obj.manager!=manager||obj.state!=1||!obj.config
            ||!get(obj.config,config))return {};
        const uint8_t enabled[]={config[0x14],config[0x1c]};
        const uint8_t eligible[]={obj.ghost,obj.op};
        for(unsigned i=0;i<2;i++) {
            if(enabled[i]>1||eligible[i]>1)return {};
            if(enabled[i]){++result.total[i];if(!eligible[i])++result.failed[i];}
        }
        if(!get(link.object,check)||memcmp(&obj,&check,sizeof(obj))
            ||!get(node,again)||memcmp(&link,&again,sizeof(link)))return {};
        node=link.next;
    }
    // Discard traversals that crossed a load or list replacement.
    uint32_t current=0;
    if(!get(base+0x1607B1C,current)||current!=root
        ||!get(root+8,current)||current!=index||!get(root,current)||current!=world
        ||!get(world+0x30,current)||current!=manager
        ||!get(manager+12,current)||current!=head
        ||(head&&(!get(head,current)||current!=first)))return {};
    result.readable=true;
    return result;
}
}
