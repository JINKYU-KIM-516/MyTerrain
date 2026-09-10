#include "Technique06_DistanceLOD.h"

#include "../../GameObject/Scene.h"
#include "../../GameObject/GameObject.h"
#include "../../Framework/Camera.h"
#include "../../Components/FreeCameraController.h"
#include "../../Components/PerlinControlComponent.h"
#include "../../Components/LodControlComponent.h"
#include "../../Terrain/TerrainRenderer.h"
#include "../../Terrain/PerlinNoise.h"
#include "../../UI/UIText.h"

namespace
{
    // ---- 초기 그리드 설정 (2·5번과 동일) ----
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
        L"[그리드]  분할 수 + / -   셀 크기 [ / ]   표시 모드 Tab                                   [메뉴로] ESC";
}

void BuildDistanceLODScene(Scene& scene)
{
    // ---------------- 카메라 ----------------
    // 5번보다 낮고 지형 가까이에서 시작한다. 지형을 멀리서 통째로 내려다보면 모든 청크가
    // 비슷하게 멀어서 전부 최저 해상도가 되어 버린다 -- 가까운 쪽과 먼 쪽이 한 화면에
    // 같이 들어와야 레벨 차이(와 그 경계의 이음매)가 보인다.
    // 이 위치 + 기본 기준 거리 60 이면 네 레벨이 모두 화면에 나타난다.
    GameObject* cameraObject = scene.CreateGameObject("MainCamera");
    cameraObject->GetTransform()->SetPosition(0.0f, 60.0f, -140.0f);
    cameraObject->GetTransform()->SetRotation(16.0f, 0.0f, 0.0f);

    Camera* camera = cameraObject->AddComponent<Camera>();
    camera->SetClipPlanes(0.1f, 8000.0f);
    camera->SetAsMain();

    FreeCameraController* controller = cameraObject->AddComponent<FreeCameraController>();
    controller->SetMoveSpeed(70.0f);

    // ---------------- 지형 (2·5번과 같은 펄린 노이즈 지형) ----------------
    GameObject* terrainObject = scene.CreateGameObject("LodTerrain");
    TerrainRenderer* terrain = terrainObject->AddComponent<TerrainRenderer>();
    terrain->SetGrid(kInitialDivisions, kInitialDivisions, kInitialCellSize);

    // 레벨 색상 모드가 gBaseColor 를 갈아끼우므로, 격자 밀도 차이가 색과 함께 보이도록
    // 솔리드로 시작한다 (Tab 으로 와이어를 얹으면 스텝 차이가 더 확실히 보인다).
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

    // ---------------- HUD: LOD 상태 (오른쪽) ----------------
    // 4·5번과 같은 이유로 완전히 독립된 블록을 오른쪽 상단에 둔다 (왼쪽 블록은 줄 수가 가변적).
    GameObject* lodInfoObject = scene.CreateGameObject("LodInfo");
    UIText* lodInfoText = lodInfoObject->AddComponent<UIText>();
    lodInfoText->SetText(L"");
    lodInfoText->SetFontSize(kHudFontSize);
    lodInfoText->SetAnchor(UIAnchor::TopRight);
    lodInfoText->SetOffset(kHudMarginX, kHudTopY);
    lodInfoText->SetColor(1.0f, 0.9f, 0.55f);

    // ---------------- HUD: 조작 안내 ----------------
    GameObject* helpObject = scene.CreateGameObject("LodHelp");
    UIText* helpText = helpObject->AddComponent<UIText>();
    helpText->SetText(kHelpText);
    helpText->SetFontSize(kHudFontSize);
    helpText->SetAnchor(UIAnchor::BottomLeft);
    helpText->SetOffset(kHudMarginX, kHudBottomY);
    helpText->SetColor(1.0f, 1.0f, 1.0f);

    // ---------------- 노이즈 파라미터 조절 (2·5번과 동일) ----------------
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

    // ---------------- 거리 LOD 조절 (6-1 에서 새로 추가) ----------------
    GameObject* lodControlObject = scene.CreateGameObject("LodControl");
    LodControlComponent* lodControl = lodControlObject->AddComponent<LodControlComponent>();
    lodControl->SetTarget(terrain);
    lodControl->SetInfoText(lodInfoText);
}
