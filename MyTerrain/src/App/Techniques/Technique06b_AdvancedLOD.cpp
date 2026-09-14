#include "Technique06b_AdvancedLOD.h"

#include "../../GameObject/Scene.h"
#include "../../GameObject/GameObject.h"
#include "../../Framework/Camera.h"
#include "../../Components/FreeCameraController.h"
#include "../../Components/PerlinControlComponent.h"
#include "../../Components/GeoLodControlComponent.h"
#include "../../Terrain/TerrainRenderer.h"
#include "../../Terrain/PerlinNoise.h"
#include "../../UI/UIText.h"

namespace
{
    // ---- 초기 그리드 설정 (6-1 과 동일하게 맞춰서 바로 비교되게 한다) ----
    constexpr int   kInitialDivisions = 256;
    constexpr float kInitialCellSize = 1.0f;

    constexpr float kCheckerScale = 8.0f;

    // ---- HUD 레이아웃 (픽셀) ----
    constexpr float kHudFontSize = 16.0f;
    constexpr float kHudMarginX = 28.0f;
    constexpr float kHudTopY = 72.0f;
    constexpr float kHudBottomY = 24.0f;

    constexpr wchar_t kHelpText[] =
        L"[카메라]  이동 W/A/S/D   상하 E/Q   시점 마우스 우클릭 드래그   속도 휠   가속 Shift   리셋 R\n"
        L"[노이즈]  항목 선택 ↑/↓   값 조절 ←/→ (누르고 있으면 연속)   합성 방식 N   시드 무작위 M   기본값 0\n"
        L"[보는 법]  F 로 LOD 를 얼리고 레벨 경계까지 날아가서 H 를 껐다 켜 보기 / 앞뒤로 움직이며 G 껐다 켜 보기\n"
        L"[그리드]  분할 수 + / -   셀 크기 [ / ]   표시 모드 Tab                                   [메뉴로] ESC";
}

void BuildAdvancedLODScene(Scene& scene)
{
    // ---------------- 카메라 (6-1 과 같은 시작 위치) ----------------
    GameObject* cameraObject = scene.CreateGameObject("MainCamera");
    cameraObject->GetTransform()->SetPosition(0.0f, 60.0f, -140.0f);
    cameraObject->GetTransform()->SetRotation(16.0f, 0.0f, 0.0f);

    Camera* camera = cameraObject->AddComponent<Camera>();
    camera->SetClipPlanes(0.1f, 8000.0f);
    camera->SetAsMain();

    FreeCameraController* controller = cameraObject->AddComponent<FreeCameraController>();
    controller->SetMoveSpeed(70.0f);

    // ---------------- 지형 ----------------
    GameObject* terrainObject = scene.CreateGameObject("AdvancedLodTerrain");
    TerrainRenderer* terrain = terrainObject->AddComponent<TerrainRenderer>();
    terrain->SetGrid(kInitialDivisions, kInitialDivisions, kInitialCellSize);

    // 6-2 는 "이음매가 안 보인다" 를 보는 화면이라 솔리드로 시작한다.
    // Tab 으로 와이어를 얹으면 스티칭이 테두리를 어떻게 다시 엮었는지 직접 보인다.
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

    // ---------------- HUD: 6-2 상태 (오른쪽) ----------------
    GameObject* lodInfoObject = scene.CreateGameObject("AdvancedLodInfo");
    UIText* lodInfoText = lodInfoObject->AddComponent<UIText>();
    lodInfoText->SetText(L"");
    lodInfoText->SetFontSize(kHudFontSize);
    lodInfoText->SetAnchor(UIAnchor::TopRight);
    lodInfoText->SetOffset(kHudMarginX, kHudTopY);
    lodInfoText->SetColor(1.0f, 0.9f, 0.55f);

    // ---------------- HUD: 조작 안내 ----------------
    GameObject* helpObject = scene.CreateGameObject("AdvancedLodHelp");
    UIText* helpText = helpObject->AddComponent<UIText>();
    helpText->SetText(kHelpText);
    helpText->SetFontSize(kHudFontSize);
    helpText->SetAnchor(UIAnchor::BottomLeft);
    helpText->SetOffset(kHudMarginX, kHudBottomY);
    helpText->SetColor(1.0f, 1.0f, 1.0f);

    // ---------------- 노이즈 파라미터 조절 ----------------
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

    // ---------------- 스티칭 / 지오머핑 조절 (6-2 에서 새로 추가) ----------------
    GameObject* lodControlObject = scene.CreateGameObject("AdvancedLodControl");
    GeoLodControlComponent* lodControl = lodControlObject->AddComponent<GeoLodControlComponent>();
    lodControl->SetTarget(terrain);
    lodControl->SetInfoText(lodInfoText);
}
