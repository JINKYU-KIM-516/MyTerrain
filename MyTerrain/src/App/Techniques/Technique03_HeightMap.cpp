#include "Technique03_HeightMap.h"

#include "../../GameObject/Scene.h"
#include "../../GameObject/GameObject.h"
#include "../../Framework/Camera.h"
#include "../../Components/FreeCameraController.h"
#include "../../Components/HeightMapControlComponent.h"
#include "../../Terrain/TerrainRenderer.h"
#include "../../Terrain/HeightMap.h"
#include "../../UI/UIText.h"

namespace
{
    // ---- 초기 그리드 설정 ----
    // 2번과 같은 256 x 256. 샘플 높이맵도 512 / 1024 짜리라
    // "텍셀이 정점보다 잘다" 는 상황을 처음부터 눈으로 보게 된다.
    constexpr int   kInitialDivisions = 256;
    constexpr float kInitialCellSize = 1.0f;

    // 높이맵 한 장이 지형 전체(256 x 256)에 딱 맞게 펼쳐지도록 맞춰둔 값
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
        L"[높이맵]  항목 선택 ↑/↓   값 조절 ←/→ (누르고 있으면 연속)   다음 파일 N   고도 색상 C   다시 읽기 F5   기본값 0\n"
        L"[그리드]  분할 수 + / -   셀 크기 [ / ]   표시 모드 Tab                                   [메뉴로] ESC";
}

void BuildHeightMapScene(Scene& scene)
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

    // ---------------- 지형 ----------------
    GameObject* terrainObject = scene.CreateGameObject("HeightMapTerrain");
    TerrainRenderer* terrain = terrainObject->AddComponent<TerrainRenderer>();
    terrain->SetGrid(kInitialDivisions, kInitialDivisions, kInitialCellSize);

    terrain->SetDisplayMode(TerrainDisplayMode::Solid);
    terrain->SetSolidColor(0.46f, 0.50f, 0.42f);
    terrain->SetWireColor(0.92f, 0.96f, 1.0f);
    terrain->SetLightDirection(0.45f, -1.0f, 0.35f);
    terrain->SetCheckerScale(kCheckerScale);

    // 고도 색상을 켜둔 채로 시작한다.
    // 색 띠가 지형의 기복과 어긋나 보이면 GPU 텍스처와 CPU 샘플링의 좌표 규약이
    // 어긋난 것이므로, 켜두는 것 자체가 로더 검증이 된다. (C 로 끌 수 있다)
    terrain->SetHeightColorMode(true);

    // ---------------- HUD: 현재 상태 ----------------
    GameObject* infoObject = scene.CreateGameObject("HeightMapInfo");
    UIText* infoText = infoObject->AddComponent<UIText>();
    infoText->SetText(L"");
    infoText->SetFontSize(kHudFontSize);
    infoText->SetAnchor(UIAnchor::TopLeft);
    infoText->SetOffset(kHudMarginX, kHudTopY);
    infoText->SetColor(1.0f, 1.0f, 1.0f);

    // ---------------- HUD: 조작 안내 ----------------
    GameObject* helpObject = scene.CreateGameObject("HeightMapHelp");
    UIText* helpText = helpObject->AddComponent<UIText>();
    helpText->SetText(kHelpText);
    helpText->SetFontSize(kHudFontSize);
    helpText->SetAnchor(UIAnchor::BottomLeft);
    helpText->SetOffset(kHudMarginX, kHudBottomY);
    helpText->SetColor(1.0f, 1.0f, 1.0f);

    // ---------------- 높이맵 조절 ----------------
    HeightMap::Params params;
    params.worldSize = kInitialWorldSize;
    params.heightScale = kInitialHeightScale;
    params.heightOffset = 0.0f;
    params.flipZ = true;
    params.wrap = HeightMap::WrapMode::Clamp;

    GameObject* controlObject = scene.CreateGameObject("HeightMapControl");
    HeightMapControlComponent* control = controlObject->AddComponent<HeightMapControlComponent>();
    control->SetTarget(terrain);
    control->SetInfoText(infoText);
    control->SetParams(params);
}
