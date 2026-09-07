#include <windows.h>
#include "Framework/Framework.h"
#include "App/ShowcaseApp.h"
#include "App/Techniques/Technique00_MenuTest.h"

// ------------------------------------------------------------
// Terrain Showcase System - 진입점
//
// 화면 구성
//   [메뉴 화면]
//     - 최상단 제목: ===Terrain Showcase System===
//     - 그 아래에 "n. 기법명 (영어 기법명)" 형식으로 기법 목록 (중앙 정렬)
//     - 기법명을 클릭하면 해당 기법 화면으로 이동
//     - 오른쪽 상단 "종료" 클릭 또는 ESC -> 프로그램 종료
//
//   [기법 화면]
//     - 왼쪽 상단 "돌아가기" 클릭 또는 ESC -> 메뉴 화면으로 복귀
//
// 새 기법 추가 방법
//   1) src/App/Techniques/ 에 TechniqueXX_이름.h/.cpp 를 만들고
//      void BuildXXXScene(Scene& scene) 함수에서 GameObject를 구성한다.
//   2) 아래 RegisterTechnique(...) 에 한 줄 추가한다.
//      -> 메뉴에 자동으로 번호가 매겨져 표시되고 클릭 시 이동한다.
// ------------------------------------------------------------

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nCmdShow);

    Framework framework;

    if (!framework.Initialize(hInstance, 1280, 720, L"Terrain Showcase System"))
    {
        MessageBoxW(nullptr, L"프레임워크 초기화에 실패했습니다.", L"Error", MB_OK | MB_ICONERROR);
        return -1;
    }

    // 화면을 파란색으로 클리어 (텍스트는 하얀색으로 표시된다)
    framework.SetClearColor(0.15f, 0.35f, 0.65f, 1.0f);

    ShowcaseApp& app = ShowcaseApp::GetInstance();
    app.Initialize(&framework);

    // ---- 기법 등록 (등록 순서대로 0번부터 번호가 매겨진다) ----
    app.RegisterTechnique(L"메뉴 구현 테스트", L"Menu Test", &BuildMenuTestScene);
    // app.RegisterTechnique(L"기본 평면 그리드", L"Flat Grid", &BuildFlatGridScene);
    // app.RegisterTechnique(L"펄린 노이즈 지형", L"Perlin Noise Terrain", &BuildPerlinNoiseScene);
    // ...

    // 메뉴 화면 구성
    app.Start();

    // 메시지 루프 + 게임 루프 실행 (창을 닫을 때까지 블록됨)
    framework.Run();

    framework.Shutdown();

    return 0;
}
