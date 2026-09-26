// Standalone render smoke test; does not load or modify the game.
#include "StealthHUD.h"
#include "imgui/imgui.h"
#include <windows.h>
#include <cassert>
#include <cstdio>
#include <initializer_list>
int main(){
    ImGui::CreateContext();auto& io=ImGui::GetIO();
    io.IniFilename=nullptr;io.DisplaySize=ImVec2(3440,1440);io.DeltaTime=1.0f/60;
    unsigned char* pixels;int w,h;io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
    auto frame=[&](){ImGui::NewFrame();DrawStealthHUD();ImGui::Render();return ImGui::GetDrawData()->TotalVtxCount;};
    assert(frame()>0);
    auto* draw=ImGui::GetDrawData();
    for(int i=0;i<draw->CmdListsCount;i++)for(auto v:draw->CmdLists[i]->VtxBuffer){
        assert(v.col!=IM_COL32(151,194,160,255)); // Unsupported EXE must never show green.
        assert(v.pos.x>=0&&v.pos.x<=3440&&v.pos.y>=0&&v.pos.y<=1440);
    }
    io.KeysDown[VK_F10]=true;assert(frame()==0);
    io.KeysDown[VK_F10]=false;assert(frame()==0);
    io.KeysDown[VK_F10]=true;assert(frame()>0);
    io.KeysDown[VK_F10]=false;int compact=frame();
    io.KeysDown[VK_F9]=true;assert(frame()>compact);
    io.KeysDown[VK_F9]=false;
    for(auto size : {ImVec2(3440,1440),ImVec2(1920,1080),ImVec2(1280,720)}){
        io.DisplaySize=size;assert(frame()>0);
        auto* expanded=ImGui::GetDrawData();
        for(int i=0;i<expanded->CmdListsCount;i++)for(auto v:expanded->CmdLists[i]->VtxBuffer){
            assert(v.pos.x>=0&&v.pos.x<=size.x&&v.pos.y>=0&&v.pos.y<=size.y);
        }
    }
    ImGui::DestroyContext();puts("PASS: native draw, compact and expanded bounds at 3440x1440/1920x1080/1280x720, unsupported EXE stays unknown, F9/F10 controls.");
}
