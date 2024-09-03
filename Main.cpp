#include "Stdafx.h"
#include "D3D12HelloWindow.h"

// 인텔리센스가 함수 정의를 분석할 때 주석 정보를 가져오도록 함.
_Use_decl_annotations_
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, LPSTR cmdLine, int nCmdShow)
{
    D3D12HelloWindow sample(1280, 720, L"D3D12 Hello Window");
    return Win32Application::Run(&sample, hInstance, nCmdShow);
}