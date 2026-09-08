#include "Technique04_TextureSplatting.h"

#include "../../GameObject/Scene.h"
#include "../../GameObject/GameObject.h"
#include "../../Framework/Camera.h"
#include "../../Components/FreeCameraController.h"
#include "../../Components/HeightMapControlComponent.h"
#include "../../Components/SplatControlComponent.h"
#include "../../Terrain/TerrainRenderer.h"
#include "../../Terrain/HeightMap.h"
#include "../../UI/UIText.h"

namespace
{
    // ---- 초기 그리드 설정 (3번과 동일) ----
    constexpr int   kInitialDivisions = 256;
    constexpr float kInitialCellSize = 1.0f;

    constexpr float kInitialWorldSize = 256.0f;
    constexpr float kInitialHeightScale = 60.0f;

    constexpr float kCheckerScale = 8.0f;

    // ---- HUD 레이아웃 (픽셀) ----
    constexpr float kHudFontSize = 16.0f;
    constexpr float kHudMarginX = 28.0f;
    constexpr float kHudTopY = 72.0f;      // 좌상단 "돌아가기" 버튼 아래
    constexpr float kHudBottomY = 24.0f;

    constexpr wchar_t kHelpText[] =
        L"[카메라]  이동 W/A/S/D   상하 E/Q   시점 마우스 우클릭 드래그   속도 휠   가속 Shift   리셋 R\n"
        L"[높이맵]  항목 선택 ↑/↓   값 조절 ←/→ (누르고 있으면 연속)   다음 파일 N   고도 색상(비교용) C   다시 읽기 F5   기본값 0\n"
        L"[스플래팅]  켬/끔 V   타일링 I/K   경사 구간 U/J   기본값 0\n"
        L"[그리드]  분할 수 + / -   셀 크기 [ / ]   표시 모드 Tab                                   [메뉴로] ESC";
}

void BuildTextureSplattingScene(Scene& scene)
{
    // ---------------- 카메라 ----------------
    GameObject* cameraObject = scene.CreateGameObject("MainCamera");
    cameraObject->GetTransform()->SetPosition(0.0f, 110.0f, -200.0f);
    cameraObject->GetTransform()->SetRotation(22.0f, 0.0f, 0.0f);

    Camera* camera = cameraObject->AddComponent<Camera>();
    camera->SetClipPlanes(0.1f, 8000.0f);
    camera->SetAsMain();

    FreeCameraController* controller = cameraObject->AddComponent<FreeCameraController>();
    controller->SetMoveSpeed(60.0f);

    // ---------------- 지형 (3번과 같은 높이맵 지형) ----------------
    GameObject* terrainObject = scene.CreateGameObject("SplattingTerrain");
    TerrainRenderer* terrain = terrainObject->AddComponent<TerrainRenderer>();
    terrain->SetGrid(kInitialDivisions, kInitialDivisions, kInitialCellSize);

    terrain->SetDisplayMode(TerrainDisplayMode::Solid);
    terrain->SetSolidColor(0.46f, 0.50f, 0.42f);
    terrain->SetWireColor(0.92f, 0.96f, 1.0f);
    terrain->SetLightDirection(0.45f, -1.0f, 0.35f);
    terrain->SetCheckerScale(kCheckerScale);

    // 4번은 스플래팅이 주인공이므로 고도 색상은 꺼둔 채로 시작한다.
    // (HeightMapControlComponent 의 C 키로 언제든 켜서 스플래팅과 비교해볼 수 있다 --
    //  스플래팅이 우선이므로 둘 다 켜져 있으면 셰이더가 스플래팅을 그린다)
    terrain->SetHeightColorMode(false);

    // ---------------- HUD: 높이맵 상태 (왼쪽) ----------------
    GameObject* infoObject = scene.CreateGameObject("HeightMapInfo");
    UIText* infoText = infoObject->AddComponent<UIText>();
    infoText->SetText(L"");
    infoText->SetFontSize(kHudFontSize);
    infoText->SetAnchor(UIAnchor::TopLeft);
    infoText->SetOffset(kHudMarginX, kHudTopY);
    infoText->SetColor(1.0f, 1.0f, 1.0f);

    // ---------------- HUD: 스플래팅 상태 (오른쪽) ----------------
    // 높이맵 정보 텍스트는 줄 수가 가변적이라, 아래에 이어 붙이면 겹칠 수 있다.
    // 그래서 오른쪽 상단에 완전히 독립된 블록으로 둔다.
    GameObject* splatInfoObject = scene.CreateGameObject("SplatInfo");
    UIText* splatInfoText = splatInfoObject->AddComponent<UIText>();
    splatInfoText->SetText(L"");
    splatInfoText->SetFontSize(kHudFontSize);
    splatInfoText->SetAnchor(UIAnchor::TopRight);
    splatInfoText->SetOffset(kHudMarginX, kHudTopY);
    splatInfoText->SetColor(1.0f, 1.0f, 1.0f);

    // ---------------- HUD: 조작 안내 ----------------
    GameObject* helpObject = scene.CreateGameObject("SplattingHelp");
    UIText* helpText = helpObject->AddComponent<UIText>();
    helpText->SetText(kHelpText);
    helpText->SetFontSize(kHudFontSize);
    helpText->SetAnchor(UIAnchor::BottomLeft);
    helpText->SetOffset(kHudMarginX, kHudBottomY);
    helpText->SetColor(1.0f, 1.0f, 1.0f);

    // ---------------- 높이맵 조절 (3번과 동일) ----------------
    HeightMap::Params params;
    params.worldSize = kInitialWorldSize;
    params.heightScale = kInitialHeightScale;
    params.heightOffset = 0.0f;
    params.flipZ = true;
    params.wrap = HeightMap::WrapMode::Clamp;

    GameObject* heightControlObject = scene.CreateGameObject("HeightMapControl");
    HeightMapControlComponent* heightControl = heightControlObject->AddComponent<HeightMapControlComponent>();
    heightControl->SetTarget(terrain);
    heightControl->SetInfoText(infoText);
    heightControl->SetParams(params);

    // ---------------- 스플래팅 조절 (4번에서 새로 추가) ----------------
    GameObject* splatControlObject = scene.CreateGameObject("SplatControl");
    SplatControlComponent* splatControl = splatControlObject->AddComponent<SplatControlComponent>();
    splatControl->SetTarget(terrain);
    splatControl->SetInfoText(splatInfoText);
}
