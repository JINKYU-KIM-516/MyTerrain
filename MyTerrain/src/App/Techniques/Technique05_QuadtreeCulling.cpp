#include "Technique05_QuadtreeCulling.h"

#include "../../GameObject/Scene.h"
#include "../../GameObject/GameObject.h"
#include "../../Framework/Camera.h"
#include "../../Components/FreeCameraController.h"
#include "../../Components/PerlinControlComponent.h"
#include "../../Components/QuadtreeControlComponent.h"
#include "../../Terrain/TerrainRenderer.h"
#include "../../Terrain/PerlinNoise.h"
#include "../../UI/UIText.h"

namespace
{
    // ---- 초기 그리드 설정 (2번과 동일) ----
    constexpr int   kInitialDivisions = 256;
    constexpr float kInitialCellSize = 1.0f;

    constexpr float kCheckerScale = 8.0f;

    // ---- HUD 레이아웃 (픽셀) ----
    constexpr float kHudFontSize = 16.0f;
    constexpr float kHudMarginX = 28.0f;
    constexpr float kHudTopY = 72.0f;      // 좌상단 "돌아가기" 버튼 아래
    constexpr float kHudBottomY = 24.0f;

    constexpr wchar_t kHelpText[] =
        L"[카메라]  이동 W/A/S/D   상하 E/Q   시점 마우스 우클릭 드래그   속도 휠   가속 Shift   리셋 R\n"
        L"[노이즈]  항목 선택 ↑/↓   값 조절 ←/→ (누르고 있으면 연속)   합성 방식 N   시드 무작위 M   기본값 0\n"
        L"[쿼드트리]  컬링 켬/끔 C   디버그 박스 B   리프 크기 , / .   기본값 0\n"
        L"[그리드]  분할 수 + / -   셀 크기 [ / ]   표시 모드 Tab                                   [메뉴로] ESC";
}

void BuildQuadtreeCullingScene(Scene& scene)
{
    // ---------------- 카메라 ----------------
    // 지형을 조금 높은 곳에서 내려다보게 시작해야 절두체 밖으로 나가는 리프가 바로 보인다.
    GameObject* cameraObject = scene.CreateGameObject("MainCamera");
    cameraObject->GetTransform()->SetPosition(0.0f, 140.0f, -220.0f);
    cameraObject->GetTransform()->SetRotation(26.0f, 0.0f, 0.0f);

    Camera* camera = cameraObject->AddComponent<Camera>();
    camera->SetClipPlanes(0.1f, 8000.0f);
    camera->SetAsMain();

    FreeCameraController* controller = cameraObject->AddComponent<FreeCameraController>();
    controller->SetMoveSpeed(70.0f);

    // ---------------- 지형 (2번과 같은 펄린 노이즈 지형) ----------------
    GameObject* terrainObject = scene.CreateGameObject("QuadtreeTerrain");
    TerrainRenderer* terrain = terrainObject->AddComponent<TerrainRenderer>();
    terrain->SetGrid(kInitialDivisions, kInitialDivisions, kInitialCellSize);

    terrain->SetDisplayMode(TerrainDisplayMode::Solid);
    terrain->SetSolidColor(0.42f, 0.53f, 0.33f);
    terrain->SetWireColor(0.92f, 0.96f, 1.0f);
    terrain->SetLightDirection(0.45f, -1.0f, 0.35f);
    terrain->SetCheckerScale(kCheckerScale);

    // ---------------- HUD: 노이즈 상태 (왼쪽) ----------------
    GameObject* infoObject = scene.CreateGameObject("PerlinInfo");
    UIText* infoText = infoObject->AddComponent<UIText>();
    infoText->SetText(L"");
    infoText->SetFontSize(kHudFontSize);
    infoText->SetAnchor(UIAnchor::TopLeft);
    infoText->SetOffset(kHudMarginX, kHudTopY);
    infoText->SetColor(1.0f, 1.0f, 1.0f);

    // ---------------- HUD: 쿼드트리 상태 (오른쪽) ----------------
    // 4번과 같은 이유로 완전히 독립된 블록을 오른쪽 상단에 둔다 (왼쪽 블록은 줄 수가 가변적).
    GameObject* quadtreeInfoObject = scene.CreateGameObject("QuadtreeInfo");
    UIText* quadtreeInfoText = quadtreeInfoObject->AddComponent<UIText>();
    quadtreeInfoText->SetText(L"");
    quadtreeInfoText->SetFontSize(kHudFontSize);
    quadtreeInfoText->SetAnchor(UIAnchor::TopRight);
    quadtreeInfoText->SetOffset(kHudMarginX, kHudTopY);
    quadtreeInfoText->SetColor(1.0f, 0.9f, 0.55f);

    // ---------------- HUD: 조작 안내 ----------------
    GameObject* helpObject = scene.CreateGameObject("QuadtreeHelp");
    UIText* helpText = helpObject->AddComponent<UIText>();
    helpText->SetText(kHelpText);
    helpText->SetFontSize(kHudFontSize);
    helpText->SetAnchor(UIAnchor::BottomLeft);
    helpText->SetOffset(kHudMarginX, kHudBottomY);
    helpText->SetColor(1.0f, 1.0f, 1.0f);

    // ---------------- 노이즈 파라미터 조절 (2번과 동일) ----------------
    Noise::Params params;
    params.scale = 64.0f;
    params.octaves = 6;
    params.lacunarity = 2.0f;
    params.persistence = 0.5f;
    params.amplitude = 30.0f;
    params.seed = 1337u;
    params.type = Noise::Type::FBM;

    GameObject* controlObject = scene.CreateGameObject("PerlinControl");
    PerlinControlComponent* control = controlObject->AddComponent<PerlinControlComponent>();
    control->SetTarget(terrain);
    control->SetInfoText(infoText);
    control->SetParams(params);

    // ---------------- 쿼드트리 컬링 조절 (5번에서 새로 추가) ----------------
    GameObject* quadtreeControlObject = scene.CreateGameObject("QuadtreeControl");
    QuadtreeControlComponent* quadtreeControl = quadtreeControlObject->AddComponent<QuadtreeControlComponent>();
    quadtreeControl->SetTarget(terrain);
    quadtreeControl->SetInfoText(quadtreeInfoText);
}
