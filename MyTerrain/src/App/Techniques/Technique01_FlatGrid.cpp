#include "Technique01_FlatGrid.h"

#include "../../GameObject/Scene.h"
#include "../../GameObject/GameObject.h"
#include "../../Framework/Camera.h"
#include "../../Components/FreeCameraController.h"
#include "../../Components/GridControlComponent.h"
#include "../../Terrain/TerrainRenderer.h"
#include "../../UI/UIText.h"

namespace
{
    // ---- 초기 그리드 설정 ----
    constexpr int   kInitialDivisions = 64;
    constexpr float kInitialCellSize = 1.0f;

    // ---- HUD 레이아웃 (픽셀) ----
    constexpr float kHudFontSize = 17.0f;
    constexpr float kHudMarginX = 28.0f;
    constexpr float kHudTopY = 72.0f;      // 좌상단 "돌아가기" 버튼 아래
    constexpr float kHudBottomY = 24.0f;

    constexpr wchar_t kHelpText[] =
        L"[카메라]  이동 W/A/S/D   상하 E/Q   시점 마우스 우클릭 드래그   속도 휠   가속 Shift   리셋 R\n"
        L"[그리드]  분할 수 + / -   셀 크기 [ / ]   표시 모드 Tab            [메뉴로] ESC";
}

void BuildFlatGridScene(Scene& scene)
{
    // ---------------- 카메라 ----------------
    GameObject* cameraObject = scene.CreateGameObject("MainCamera");
    cameraObject->GetTransform()->SetPosition(0.0f, 30.0f, -55.0f);
    cameraObject->GetTransform()->SetRotation(28.0f, 0.0f, 0.0f);   // 살짝 내려다보는 각도

    Camera* camera = cameraObject->AddComponent<Camera>();
    camera->SetClipPlanes(0.1f, 5000.0f);
    camera->SetAsMain();

    FreeCameraController* controller = cameraObject->AddComponent<FreeCameraController>();
    controller->SetMoveSpeed(30.0f);

    // ---------------- 지형(격자) ----------------
    GameObject* terrainObject = scene.CreateGameObject("FlatGrid");
    TerrainRenderer* terrain = terrainObject->AddComponent<TerrainRenderer>();
    terrain->SetGrid(kInitialDivisions, kInitialDivisions, kInitialCellSize);
    terrain->SetDisplayMode(TerrainDisplayMode::SolidWireframe);
    terrain->SetSolidColor(0.36f, 0.58f, 0.34f);
    terrain->SetWireColor(0.92f, 0.96f, 1.0f);
    terrain->SetLightDirection(0.4f, -1.0f, 0.5f);

    // ---------------- HUD: 현재 상태 ----------------
    GameObject* infoObject = scene.CreateGameObject("GridInfo");
    UIText* infoText = infoObject->AddComponent<UIText>();
    infoText->SetText(L"");
    infoText->SetFontSize(kHudFontSize);
    infoText->SetAnchor(UIAnchor::TopLeft);
    infoText->SetOffset(kHudMarginX, kHudTopY);
    infoText->SetColor(1.0f, 1.0f, 1.0f);

    // ---------------- HUD: 조작 안내 ----------------
    GameObject* helpObject = scene.CreateGameObject("GridHelp");
    UIText* helpText = helpObject->AddComponent<UIText>();
    helpText->SetText(kHelpText);
    helpText->SetFontSize(kHudFontSize);
    helpText->SetAnchor(UIAnchor::BottomLeft);
    helpText->SetOffset(kHudMarginX, kHudBottomY);
    helpText->SetColor(1.0f, 1.0f, 1.0f);

    // ---------------- 키 입력으로 그리드 파라미터 조절 ----------------
    GameObject* controlObject = scene.CreateGameObject("GridControl");
    GridControlComponent* control = controlObject->AddComponent<GridControlComponent>();
    control->SetTarget(terrain);
    control->SetInfoText(infoText);
}
