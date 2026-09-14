#include "Technique07_Tessellation.h"

#include "../../GameObject/Scene.h"
#include "../../GameObject/GameObject.h"
#include "../../Framework/Camera.h"
#include "../../Components/FreeCameraController.h"
#include "../../Components/HeightMapControlComponent.h"
#include "../../Components/TessellationControlComponent.h"
#include "../../Terrain/TerrainRenderer.h"
#include "../../Terrain/HeightMap.h"
#include "../../UI/UIText.h"

namespace
{
    // ---- 초기 패치 격자 설정 ----
    // divisions/cellSize 는 여기서는 "패치 개수"와 "패치 한 변의 월드 크기"를 뜻한다
    // (GridMesh 의 셀 하나 = 패치 하나, PatchGrid.h 참고). 16 x 16 패치 x 16 단위 =
    // 3번/6번과 같은 256 x 256 크기 지형이 되도록 맞췄다.
    constexpr int   kInitialPatchCount = 16;
    constexpr float kInitialPatchSize = 16.0f;

    // 높이맵 한 장이 지형 전체(256 x 256)에 딱 맞게 펼쳐지도록 맞춘 값 (3번과 동일)
    constexpr float kInitialWorldSize = 256.0f;
    constexpr float kInitialHeightScale = 60.0f;

    // 패치 개수/크기 조절 범위. PatchGrid.h 에 적어둔 이유대로, 다른 기법처럼
    // 512 분할까지 열어두면 패치가 수만 개가 되어 버리므로 훨씬 좁게 잡는다.
    constexpr int   kMinPatchCount = 4;
    constexpr int   kMaxPatchCount = 64;
    constexpr float kMinPatchSize = 4.0f;
    constexpr float kMaxPatchSize = 64.0f;

    constexpr float kCheckerScale = 8.0f;

    // ---- HUD 레이아웃 (픽셀) ----
    constexpr float kHudFontSize = 16.0f;
    constexpr float kHudMarginX = 28.0f;
    constexpr float kHudTopY = 72.0f;
    constexpr float kHudBottomY = 24.0f;

    constexpr wchar_t kHelpText[] =
        L"[카메라]     이동 W/A/S/D   상하 E/Q   시점 마우스 우클릭 드래그   속도 휠   가속 Shift   리셋 R\n"
        L"[높이맵]     항목 선택 ↑/↓   값 조절 ←/→   다음 파일 N   고도 색상 C   다시 읽기 F5\n"
        L"[패치 격자]  패치 개수 + / -   패치 크기 [ / ]   표시 모드 Tab\n"
        L"[테셀레이션] 디스플레이스먼트 H   파티션 모드 J   팩터 시각화 V   프리즈 F   컬링 X   패치 박스 B   최대 팩터 O/P   기준 거리 ;/'"
        L"                                                                                          [메뉴로] ESC";
}

void BuildTessellationScene(Scene& scene)
{
    // ---------------- 카메라 (3번/6번과 같은 시작 위치) ----------------
    GameObject* cameraObject = scene.CreateGameObject("MainCamera");
    cameraObject->GetTransform()->SetPosition(0.0f, 110.0f, -200.0f);
    cameraObject->GetTransform()->SetRotation(22.0f, 0.0f, 0.0f);

    Camera* camera = cameraObject->AddComponent<Camera>();
    camera->SetClipPlanes(0.1f, 8000.0f);
    camera->SetAsMain();

    FreeCameraController* controller = cameraObject->AddComponent<FreeCameraController>();
    controller->SetMoveSpeed(60.0f);

    // ---------------- 지형 ----------------
    // SetGrid 는 여기서 "패치 격자"를 만든다 (PatchGrid::Build 가 이 코스한 메시의
    // 셀 하나하나를 패치 하나로 그대로 쓴다). 삼각형으로 잘게 쪼갠 정점은 필요 없다.
    GameObject* terrainObject = scene.CreateGameObject("TessellationTerrain");
    TerrainRenderer* terrain = terrainObject->AddComponent<TerrainRenderer>();
    terrain->SetGrid(kInitialPatchCount, kInitialPatchCount, kInitialPatchSize);

    terrain->SetDisplayMode(TerrainDisplayMode::Solid);
    terrain->SetSolidColor(0.46f, 0.50f, 0.42f);
    terrain->SetWireColor(0.92f, 0.96f, 1.0f);
    terrain->SetLightDirection(0.45f, -1.0f, 0.35f);
    terrain->SetCheckerScale(kCheckerScale);

    // ---------------- HUD: 높이맵 상태 (왼쪽) ----------------
    GameObject* infoObject = scene.CreateGameObject("HeightMapInfo");
    UIText* infoText = infoObject->AddComponent<UIText>();
    infoText->SetText(L"");
    infoText->SetFontSize(kHudFontSize);
    infoText->SetAnchor(UIAnchor::TopLeft);
    infoText->SetOffset(kHudMarginX, kHudTopY);
    infoText->SetColor(1.0f, 1.0f, 1.0f);

    // ---------------- HUD: 테셀레이션 상태 (오른쪽) ----------------
    GameObject* tessInfoObject = scene.CreateGameObject("TessellationInfo");
    UIText* tessInfoText = tessInfoObject->AddComponent<UIText>();
    tessInfoText->SetText(L"");
    tessInfoText->SetFontSize(kHudFontSize);
    tessInfoText->SetAnchor(UIAnchor::TopRight);
    tessInfoText->SetOffset(kHudMarginX, kHudTopY);
    tessInfoText->SetColor(1.0f, 0.9f, 0.55f);

    // ---------------- HUD: 조작 안내 ----------------
    GameObject* helpObject = scene.CreateGameObject("TessellationHelp");
    UIText* helpText = helpObject->AddComponent<UIText>();
    helpText->SetText(kHelpText);
    helpText->SetFontSize(kHudFontSize);
    helpText->SetAnchor(UIAnchor::BottomLeft);
    helpText->SetOffset(kHudMarginX, kHudBottomY);
    helpText->SetColor(1.0f, 1.0f, 1.0f);

    // ---------------- 높이맵 (3번과 같은 컴포넌트를 그대로 얹는다) ----------------
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

    // +/- 와 [/] 는 GridControlComponent 가 제공하는 그대로지만, 여기서는 "패치 개수"와
    // "패치 크기"라는 뜻으로 바뀐다 -- 범위를 좁혀서 패치가 수만 개가 되지 않게 한다.
    heightControl->SetDivisionRange(kMinPatchCount, kMaxPatchCount);
    heightControl->SetCellSizeRange(kMinPatchSize, kMaxPatchSize);

    // ---------------- 테셀레이션 조절 (7번에서 새로 추가) ----------------
    GameObject* tessControlObject = scene.CreateGameObject("TessellationControl");
    TessellationControlComponent* tessControl = tessControlObject->AddComponent<TessellationControlComponent>();
    tessControl->SetTarget(terrain);
    tessControl->SetHeightMapControl(heightControl);
    tessControl->SetInfoText(tessInfoText);
}
