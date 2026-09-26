#include "BonusConditions.h"
#include <map>
#include <cassert>
#include <cstdio>
using namespace BonusConditions;
int main() {
    std::map<uint32_t,uint8_t> memory;
    auto put=[&](uint32_t a,const auto& value){
        auto p=reinterpret_cast<const uint8_t*>(&value);
        for(size_t i=0;i<sizeof(value);i++)memory[a+uint32_t(i)]=p[i];
    };
    auto read=[&](uint32_t a,void* out,size_t n){
        for(size_t i=0;i<n;i++) {
            auto v=memory.find(a+uint32_t(i));if(v==memory.end())return false;
            static_cast<uint8_t*>(out)[i]=v->second;
        }
        return true;
    };
    constexpr uint32_t base=0x400000, root=0x100000,world=0x110000,manager=0x120000;
    constexpr uint32_t head=0x130000,node=0x140000,obj=0x150000,cfg=0x160000;
    put(base+0x1607B1C,root);put(root+8,uint32_t(0));put(root,world);
    put(world+0x30,manager);put(manager+12,head);put(head,node);
    put(node,Node{head,head,obj});
    Objective objective{cfg,17,1,0,0,manager,0,1,1,{}};put(obj,objective);
    std::array<uint8_t,36> config{};config[0x14]=1;put(cfg,config);
    auto poll=[&](){return Poll(base,read);};
    assert(poll().state(0)==Valid && poll().state(1)==NotApplicable);
    objective.ghost=0;put(obj,objective);assert(poll().state(0)==Failed);
    // Loading a previous save restores the game's values, no sticky mod latch.
    objective.ghost=1;put(obj,objective);assert(poll().state(0)==Valid);
    config[0x1c]=1;put(cfg,config);objective.op=0;put(obj,objective);
    assert(poll().state(0)==Valid && poll().state(1)==Failed);
    // Distinct active tasks must not masquerade as one current area's result.
    constexpr uint32_t node2=0x170000,obj2=0x180000;
    put(node,Node{node2,head,obj});put(node2,Node{head,node,obj2});
    objective.ghost=0;put(obj2,objective);
    auto multiple=poll();assert(multiple.state(0)==Mixed && multiple.failed[0]==1 && multiple.total[0]==2);
    put(node2,Node{node, node,obj2});assert(!poll().readable); // Corrupt cycle.
    put(node,Node{head,head,obj});
    objective.state=3;put(obj,objective);assert(!poll().readable); // List changes mid-read.
    objective.state=1;objective.ghost=2;put(obj,objective);assert(!poll().readable);
    objective.ghost=1;put(obj,objective);
    memory.erase(cfg+3);assert(!poll().readable);put(cfg,config);
    bool changed=false;
    auto race=[&](uint32_t a,void* b,size_t n){
        bool ok=read(a,b,n);
        if(a==cfg&&!changed){changed=true;put(head,head);}
        return ok;
    };
    assert(!Poll(base,race).readable);
    assert(poll().state(0)==NotApplicable); // Empty active list.
    puts("PASS: local bonus flags, availability, restored save, mixed tasks, corruption and concurrent replacement.");
}
