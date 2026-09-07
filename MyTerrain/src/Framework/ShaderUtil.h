#pragma once
#include <windows.h>
#include <d3d11.h>
#include <d3dcommon.h>
#include <wrl/client.h>
#include <string>

// HLSL 셰이더를 "실행 중에" 컴파일하기 위한 도우미.
//
// 셰이더 원본(.hlsl)은 MyTerrain/shaders/ 폴더에 그대로 두고,
// 실행 파일 위치나 작업 디렉터리가 어디든 찾을 수 있도록
// ResolveShaderPath() 가 몇 가지 후보 경로를 순서대로 검사한다.
//
// (미리 컴파일된 .cso 를 쓰지 않는 이유: 셰이더를 고치고 다시 실행만 하면
//  바로 반영되어 지형 기법을 실험하기 편하기 때문)
namespace ShaderUtil
{
    template<typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

    // 실행 파일이 들어있는 폴더 (뒤에 '\' 포함)
    std::wstring GetExecutableDirectory();

    // shaders/<fileName> 을 찾아 전체 경로를 돌려준다. 못 찾으면 빈 문자열.
    std::wstring ResolveShaderPath(const std::wstring& fileName);

    // shaders/<fileName> 의 entryPoint 를 target(예: "vs_5_0") 으로 컴파일한다.
    // 실패하면 nullptr 을 돌려주고 outError 에 원인을 담는다.
    ComPtr<ID3DBlob> CompileFromFile(const std::wstring& fileName,
                                     const char* entryPoint,
                                     const char* target,
                                     std::wstring* outError = nullptr);

    // 컴파일 실패 등 치명적인 오류를 사용자에게 1회만 알린다
    // (같은 프레임에 매번 창이 뜨는 것을 막기 위해 내부에서 중복을 걸러낸다)
    void ReportErrorOnce(const std::wstring& message);
}
