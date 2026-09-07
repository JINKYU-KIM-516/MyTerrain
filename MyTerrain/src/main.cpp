#include <windows.h>
#include "Framework/Framework.h"
#include "Components/DemoInputComponent.h"

// ------------------------------------------------------------
// DirectX 11 기반 게임 프레임워크 - 진입점
//
// GameObject / Component 구조:
//   - GameObject 생성 시 Transform 컴포넌트가 자동으로 부착된다.
//   - AddComponent<T>() 로 원하는 동작(Component)을 추가할 수 있다.
//   - 모든 Component는 Start -> Update -> Render -> Destroy 생명주기를 따른다.
//
// 아래 WinMain에서는 예제로 GameObject 하나를 생성하고
// DemoInputComponent를 붙여 키보드/마우스 입력 동작을 확인할 수 있게 했다.
// 화면은 파란색으로 클리어된다 (Framework::SetClearColor 참고).
// ------------------------------------------------------------

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nCmdShow);

    Framework framework;

    if (!framework.Initialize(hInstance, 1280, 720, L"MyTerrain - DirectX11 Framework"))
    {
        MessageBoxW(nullptr, L"프레임워크 초기화에 실패했습니다.", L"Error", MB_OK | MB_ICONERROR);
        return -1;
    }

    // 화면을 파란색으로 클리어
    framework.SetClearColor(0.15f, 0.35f, 0.65f, 1.0f);

    // ---- 예제 GameObject 생성 ----
    // GameObject를 생성하면 내부적으로 Transform 컴포넌트가 자동으로 붙는다.
    GameObject* player = framework.GetScene().CreateGameObject("Player");
    player->GetTransform()->SetPosition(0.0f, 0.0f, 0.0f);

    // 키보드(WASD)/마우스(좌클릭 드래그) 입력을 테스트해볼 수 있는 컴포넌트 부착
    player->AddComponent<DemoInputComponent>();

    // 메시지 루프 + 게임 루프 실행 (창을 닫을 때까지 블록됨)
    framework.Run();

    framework.Shutdown();

    return 0;
}
