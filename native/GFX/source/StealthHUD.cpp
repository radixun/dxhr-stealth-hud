// Original tracker UI. Built into a modified MIT-licensed DXHRDC-GFX.
// Reads achievement conditions; never writes them or calls Steam achievements.
#include "StealthHUD.h"
#include "BonusConditions.h"
#include "imgui/imgui.h"
#include <windows.h>
#include <wincrypt.h>
#include <array>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#pragma comment(lib,"advapi32.lib")

namespace {
using namespace BonusConditions;
struct Snapshot { std::array<State,6> states{}; bool dlc=false, ready=false; char map[56]{}; BonusConditions::Result bonuses; };
uintptr_t gameBase=0;
bool supported=false, visible=true, details=false;
float scale=1.0f;
int rightMargin=24, topMargin=24;
Snapshot latest;
const char* names[]={"Pacifist","Foxiest of the Hounds","Legend","Factory Zero","Ghost","Smooth Operator"};

template<class T> bool Read(uintptr_t rva,T& value) {
    SIZE_T got=0;
    return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(gameBase+rva),&value,sizeof(value),&got)&&got==sizeof(value);
}
bool CheckExe() {
    wchar_t path[MAX_PATH];
    if(!GetModuleFileNameW(nullptr,path,MAX_PATH)) return false;
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
    if(file==INVALID_HANDLE_VALUE) return false;
    HCRYPTPROV provider=0; HCRYPTHASH hash=0;
    bool ok=CryptAcquireContextW(&provider,nullptr,nullptr,PROV_RSA_AES,CRYPT_VERIFYCONTEXT)!=FALSE;
    if(ok) ok=CryptCreateHash(provider,CALG_SHA_256,0,0,&hash)!=FALSE;
    BYTE buffer[65536];DWORD size=0;
    while(ok) {
        if(!ReadFile(file,buffer,sizeof(buffer),&size,nullptr)){ok=false;break;}
        if(!size)break;
        ok=CryptHashData(hash,buffer,size,0)!=FALSE;
    }
    BYTE digest[32]; size=32;
    if(ok)ok=CryptGetHashParam(hash,HP_HASHVAL,digest,&size,0)!=FALSE;
    const BYTE expected[]={0x82,0x66,0xb6,0xb4,0xa5,0xbf,0x25,0xf2,0xf4,0xe8,0xde,0x06,0x8a,0xa3,0x72,0x0f,0x62,0x89,0xc9,0x62,0xbb,0x1c,0x2b,0xb7,0x0a,0x7b,0x1c,0x11,0x1b,0xa5,0x10,0xa1};
    ok=ok&&size==32&&memcmp(digest,expected,32)==0;
    if(hash)CryptDestroyHash(hash);
    if(provider)CryptReleaseContext(provider,0);
    CloseHandle(file);return ok;
}
void Initialize() {
    gameBase=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    supported=CheckExe();
    wchar_t path[MAX_PATH];GetModuleFileNameW(nullptr,path,MAX_PATH);
    wchar_t* slash=wcsrchr(path,L'\\'); if(slash)wcscpy_s(slash+1,MAX_PATH-(slash+1-path),L"StealthHUD.ini");
    scale=std::clamp(GetPrivateProfileIntW(L"HUD",L"ScalePercent",100,path),75u,150u)/100.0f;
    rightMargin=static_cast<int>((std::min)(GetPrivateProfileIntW(L"HUD",L"RightMargin",24,path),500u));
    topMargin=static_cast<int>((std::min)(GetPrivateProfileIntW(L"HUD",L"TopMargin",24,path),500u));
}
Snapshot Poll() {
    Snapshot s;
    if(!supported)return s;
    BYTE loading=1, after=1, flags[2]={}; uint32_t difficulty=0, packed=0;
    if(!Read(0x1879DA0,loading)||loading||!Read(0x187BFDC,s.map))return s;
    s.map[55]=0;
    // No valid campaign state in main menu / transition / unexpected stream.
    const bool mapKnown=strncmp(s.map,"det_",4)==0||strncmp(s.map,"sha_",4)==0||strncmp(s.map,"pic_",4)==0||strncmp(s.map,"sin_",4)==0||strncmp(s.map,"pan_",4)==0||strncmp(s.map,"dlc_",4)==0;
    if(!mapKnown)return s;
    if(!Read(0xA10C98,flags)||!Read(0xA01758,difficulty)||!Read(0x18ACB90,packed)||!Read(0x1879DA0,after)||after)return s;
    s.dlc=strncmp(s.map,"dlc_",4)==0;
    if(flags[0]<=1)s.states[0]=flags[0]?Valid:Failed;
    if(flags[1]<=1)s.states[1]=flags[1]?Valid:Failed;
    if(difficulty<=3)s.states[2]=difficulty==3?Valid:Failed;
    if(s.dlc)s.states[3]=(packed&8)?Failed:Valid;
    s.bonuses=BonusConditions::Poll(static_cast<uint32_t>(gameBase),[](uint32_t address,void* buffer,size_t size){
        SIZE_T got=0;
        return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),buffer,size,&got)&&got==size;
    });
    if(!Read(0x1879DA0,after)||after)return Snapshot{};
    s.states[4]=s.bonuses.state(0);s.states[5]=s.bonuses.state(1);
    s.ready=true;return s;
}
void Icon(ImDrawList* d,int id,ImVec2 origin,float z,ImU32 color) {
    auto at=[&](float x,float y){return ImVec2(origin.x+x*z,origin.y+y*z);};
    auto path=[&](std::initializer_list<ImVec2> points,bool close=false){
        for(auto p:points)d->PathLineTo(at(p.x,p.y));
        d->PathStroke(color,close,1.5f*z);
    };
    switch(id){
    case 0:
        path({{8,14},{5,11},{3,12},{3,14},{8,20},{11,22},{15,22},{19,18},{20,14},{20,8},{18,7},{17,9},{17,5},{15,4},{14,6},{14,3},{12,2},{11,4},{11,5},{9,4},{8,6},{8,14}});
        path({{11,5},{11,12}});path({{14,6},{14,12}});path({{17,9},{17,13}});break;
    case 1:
        path({{9,5},{12,5},{16,7},{21,12},{18,16}});
        path({{6,7},{3,12},{8,17},{12,19},{16,18}});
        path({{10,10},{9,12},{11,15},{14,14}});path({{3,3},{21,21}});break;
    case 2:
        path({{12,2},{20,7},{20,16},{12,22},{4,16},{4,7}},true);
        path({{8,10},{12,7},{16,10}});path({{8,15},{12,12},{16,15}});break;
    case 3:
        path({{12,2},{21,7},{21,17},{12,22},{3,17},{3,7}},true);
        path({{12,7},{15,9},{15,15},{12,17},{9,15},{9,9}},true);path({{9,16},{15,8}});break;
    case 4:
        path({{5,21},{5,9},{7,4},{12,2},{17,4},{19,9},{19,21},{15.5f,19},{12,21},{8.5f,19}},true);
        path({{9,9},{9,11}});path({{15,9},{15,11}});break;
    case 5:
        path({{9,5},{12,4},{16,6},{18,10},{18,14}});path({{6,8},{6,15},{4,18},{16,18}});
        path({{10,21},{14,21}});path({{12,2},{12,4}});path({{3,3},{21,21}});break;
    }
}
}

void DrawStealthHUD() {
    static bool initialized=false;
    if(!initialized){Initialize();initialized=true;}
    if(ImGui::IsKeyPressed(VK_F10,false))visible=!visible;
    if(ImGui::IsKeyPressed(VK_F9,false))details=!details;
    static ULONGLONG nextPoll=0,stableSince=0;
    static Snapshot candidate;
    const auto now=GetTickCount64();
    if(now>=nextPoll){
        nextPoll=now+100;
        Snapshot s=Poll();
        if(!s.ready||!candidate.ready||strcmp(s.map,candidate.map)!=0){stableSince=now;latest=Snapshot{};}
        candidate=s;
        if(s.ready&&now-stableSince>=700)latest=s;
    }
    if(!visible)return;
    ImGuiIO& io=ImGui::GetIO();
    if(io.DisplaySize.x<320||io.DisplaySize.y<200)return;
    const int count=latest.dlc?6:5;
    const float width=(16+count*40+8)*scale, height=48*scale;
    const float x=io.DisplaySize.x-rightMargin-width,y=static_cast<float>(topMargin);
    ImDrawList* draw=ImGui::GetForegroundDrawList();
    draw->AddRectFilled(ImVec2(x,y),ImVec2(x+width,y+height),IM_COL32(13,20,17,224),3);
    draw->AddRect(ImVec2(x,y),ImVec2(x+width,y+height),IM_COL32(183,156,94,90),3);
    float cursor=x+8*scale;
    for(int i=0;i<6;i++){
        if(i==3&&!latest.dlc)continue;
        if(i==4){cursor+=8*scale;draw->AddLine(ImVec2(cursor-4*scale,y+12*scale),ImVec2(cursor-4*scale,y+36*scale),IM_COL32(150,160,135,65));}
        State state=latest.states[i];
        ImU32 c=state==Valid?IM_COL32(151,194,160,255):state==Failed?IM_COL32(237,139,115,255):state==Mixed?IM_COL32(222,185,102,255):IM_COL32(137,151,142,255);
        Icon(draw,i,ImVec2(cursor+7*scale,y+10*scale),scale,c);
        const ImVec2 dot(cursor+32*scale,y+36*scale);
        draw->AddCircleFilled(dot,6*scale,IM_COL32(13,20,17,255),12);
        draw->AddCircle(dot,6*scale,c,12,scale);
        if(state==Valid){draw->AddLine(ImVec2(dot.x-3*scale,dot.y),ImVec2(dot.x-scale,dot.y+2*scale),c,scale);draw->AddLine(ImVec2(dot.x-scale,dot.y+2*scale),ImVec2(dot.x+3*scale,dot.y-2*scale),c,scale);}
        else if(state==Failed){draw->AddLine(ImVec2(dot.x-2*scale,dot.y-2*scale),ImVec2(dot.x+2*scale,dot.y+2*scale),c,scale);draw->AddLine(ImVec2(dot.x-2*scale,dot.y+2*scale),ImVec2(dot.x+2*scale,dot.y-2*scale),c,scale);}
        else draw->AddText(nullptr,10*scale,ImVec2(dot.x-2.5f*scale,dot.y-6*scale),c,state==NotApplicable?"-":state==Mixed?"!":"?");
        cursor+=40*scale;
    }
    if(details){
        const char* descriptions[]={
            "Tracks whether the game still considers this playthrough eligible for completing the story without counted kills. Mandatory boss kills are excluded by the game's achievement rules.",
            "Tracks whether the game still considers this playthrough eligible for completing the story without triggering counted alarms. Being spotted or shot at does not necessarily fail it: detection and an actual alarm are different events.",
            "Tracks eligibility for completing the story on Give Me Deus Ex difficulty. Uses the lowest difficulty recorded by the game during this playthrough. Raising the setting again after lowering it does not restore eligibility.",
            "Tracks the game's Factory Zero eligibility flag during The Missing Link. This indicator appears only in the DLC chapter; its chapter boundaries still require gameplay validation.",
            "Tracks eligibility for the undetected-completion bonus on active objectives. Reads each objective's own flag, which the game clears when it counts a detection. This is a local bonus, separate from Foxiest of the Hounds.",
            "Tracks eligibility for the no-alarm bonus on active objectives. Reads a separate local condition and checks whether the objective offers this bonus at all. A controlled gameplay test with an actual alarm is still pending."
        };
        const ImU32 heading=IM_COL32(206,180,123,255),body=IM_COL32(204,211,196,255),muted=IM_COL32(160,167,151,255);
        auto layout=[&](bool render,float left,float top,float z,float panelWidth){
            float cy=top+16*z;
            const float wrap=panelWidth-32*z;
            auto text=[&](const char* value,float size,ImU32 color,float gap){
                float h=ImGui::GetFont()->CalcTextSizeA(size*z,FLT_MAX,wrap,value).y;
                if(render)draw->AddText(nullptr,size*z,ImVec2(left+16*z,cy),color,value,nullptr,wrap);
                cy+=h+gap*z;
            };
            text("F9: collapse     F10: hide",15,heading,14);
            for(int i=0;i<6;i++){
                if(i==3&&!latest.dlc)continue;
                const State current=latest.states[i];
                const char* label=current==Valid?"OK":current==Failed?"FAILED":current==Mixed?"MIXED":current==NotApplicable?"N/A":"UNKNOWN";
                const ImU32 color=current==Valid?IM_COL32(151,194,160,255):current==Failed?IM_COL32(237,139,115,255):current==Mixed?IM_COL32(222,185,102,255):muted;
                char title[180];snprintf(title,sizeof(title),"%s  /  %s",names[i],label);
                text(title,16,color,5);
                text(descriptions[i],14,body,16);
            }
            return cy-top+16*z;
        };
        float z=scale;
        const float maxWidth=(std::max)(280.0f,io.DisplaySize.x-32.0f);
        float panelWidth=(std::min)(640*z,maxWidth);
        float panelHeight=layout(false,0,0,z,panelWidth);
        const float top=(std::min)(y+height+8*scale,io.DisplaySize.y-180.0f);
        // Keep the full explanation visible; scale down only on smaller displays.
        const float available=io.DisplaySize.y-top-16;
        for(int attempt=0;attempt<8 && panelHeight>available;attempt++){
            z*=0.9f;
            panelHeight=layout(false,0,0,z,panelWidth);
        }
        const float left=(std::max)(16.0f,io.DisplaySize.x-rightMargin-panelWidth);
        draw->AddRectFilled(ImVec2(left,top),ImVec2(left+panelWidth,top+panelHeight),IM_COL32(13,20,17,245),3);
        draw->AddRect(ImVec2(left,top),ImVec2(left+panelWidth,top+panelHeight),IM_COL32(183,156,94,90),3);
        layout(true,left,top,z,panelWidth);
    }
}
