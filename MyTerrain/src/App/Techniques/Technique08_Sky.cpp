#include "Technique08_Sky.h"

#include "../../GameObject/Scene.h"
#include "../../GameObject/GameObject.h"
#include "../../Framework/Camera.h"
#include "../../Components/FreeCameraController.h"
#include "../../Components/HeightMapControlComponent.h"
#include "../../Components/SkyControlComponent.h"
#include "../../Terrain/TerrainRenderer.h"
#include "../../Terrain/SkyRenderer.h"
#include "../../Terrain/HeightMap.h"
#include "../../UI/UIText.h"
#include "../../UI/UIButton.h"

namespace
{
    // ---- 초기 그리드 설정 (3번과 동일) ----
    constexpr int   kInitialDivisions = 256;
    constexpr float kInitialCellSize = 1.0f;

    constexpr float kInitialWorldSize = 256.0f;
    constexpr float kInitialHeightScale = 60.0f;

    constexpr float kCheckerScale = 8.0f;

    // 카메라가 항상 이 반구의 중심에 있으므로 어떤 값이든 시각적으로 거의 같아 보인다
    // (깊이가 항상 1.0 으로 고정되기 때문). 근평면(0.1)보다 넉넉히 크게만 잡으면 된다.
    constexpr float kSkyRadius = 1000.0f;

    // ---- HUD 레이아웃 (픽셀) ----
    constexpr float kHudFontSize = 16.0f;
    constexpr float kHudMarginX = 28.0f;
    constexpr float kHudTopY = 72.0f;
    constexpr float kHudBottomY = 24.0f;

    // ---- 오른쪽 하단 버튼 패널 (픽셀) ----
    constexpr float kButtonRowHeight = 26.0f;

    constexpr wchar_t kHelpText[] =
        L"[카메라]  이동 W/A/S/D   상하 E/Q   시점 마우스 우클릭 드래그   속도 휠   가속 Shift   리셋 R\n"
        L"[높이맵]  항목 선택 ↑/↓   값 조절 ←/→ (누르고 있으면 연속)\n"
        L"[그리드]  분할 수 + / -   셀 크기 [ / ]   표시 모드 Tab\n"
        L"[높이맵 / 스카이]  그 외 조작은 우측 버튼                                                 [메뉴로] ESC";
}

void BuildSkyScene(Scene& scene)
{
    // ---------------- 카메라 (3번과 같은 시작 위치/원거리 설정) ----------------
    GameObject* cameraObject = scene.CreateGameObject("MainCamera");
    cameraObject->GetTransform()->SetPosition(0.0f, 110.0f, -200.0f);
    cameraObject->GetTransform()->SetRotation(22.0f, 0.0f, 0.0f);

    Camera* camera = cameraObject->AddComponent<Camera>();
    camera->SetClipPlanes(0.1f, 8000.0f);
    camera->SetAsMain();

    FreeCameraController* controller = cameraObject->AddComponent<FreeCameraController>();
    controller->SetMoveSpeed(60.0f);

    // ---------------- 지형 (3번과 동일. 이 기법의 주제는 하늘이므로 고도 색상은
    // 켜지 않고 3번의 기본 배색보다 살짝 차분한 초록으로 시작한다) ----------------
    GameObject* terrainObject = scene.CreateGameObject("SkyTerrain");
    TerrainRenderer* terrain = terrainObject->AddComponent<TerrainRenderer>();
    terrain->SetGrid(kInitialDivisions, kInitialDivisions, kInitialCellSize);

    terrain->SetDisplayMode(TerrainDisplayMode::Solid);
    terrain->SetSolidColor(0.40f, 0.52f, 0.36f);
    terrain->SetWireColor(0.92f, 0.96f, 1.0f);
    terrain->SetLightDirection(0.45f, -1.0f, 0.35f);
    terrain->SetCheckerScale(kCheckerScale);

    // ---------------- 스카이 (8번에서 새로 추가) ----------------
    // 지형 GameObject 뒤에 만들어서 렌더링 순서도 지형 -> 스카이가 되게 한다
    // (Scene::Render 는 GameObject 를 생성된 순서대로 그린다). 깊이 트릭 덕분에
    // 순서를 바꿔도 최종 결과는 같지만, 지형을 먼저 그려야 스카이의 픽셀 셰이더가
    // 이미 지형이 채운 자리에서는 깊이 테스트로 걸러져 실행되지 않는다.
    GameObject* skyObject = scene.CreateGameObject("Sky");
    SkyRenderer* sky = skyObject->AddComponent<SkyRenderer>();
    sky->SetRadius(kSkyRadius);
    sky->SetTimeOfDay(0.5f);

    // ---------------- HUD: 높이맵 상태 (왼쪽) ----------------
    GameObject* infoObject = scene.CreateGameObject("HeightMapInfo");
    UIText* infoText = infoObject->AddComponent<UIText>();
    infoText->SetText(L"");
    infoText->SetFontSize(kHudFontSize);
    infoText->SetAnchor(UIAnchor::TopLeft);
    infoText->SetOffset(kHudMarginX, kHudTopY);
    infoText->SetColor(1.0f, 1.0f, 1.0f);

    // ---------------- HUD: 스카이 상태 (오른쪽) ----------------
    GameObject* skyInfoObject = scene.CreateGameObject("SkyInfo");
    UIText* skyInfoText = skyInfoObject->AddComponent<UIText>();
    skyInfoText->SetText(L"");
    skyInfoText->SetFontSize(kHudFontSize);
    skyInfoText->SetAnchor(UIAnchor::TopRight);
    skyInfoText->SetOffset(kHudMarginX, kHudTopY);
    skyInfoText->SetColor(1.0f, 0.9f, 0.7f);

    // ---------------- HUD: 조작 안내 ----------------
    GameObject* helpObject = scene.CreateGameObject("SkyHelp");
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

    GameObject* controlObject = scene.CreateGameObject("HeightMapControl");
    HeightMapControlComponent* control = controlObject->AddComponent<HeightMapControlComponent>();
    control->SetTarget(terrain);
    control->SetInfoText(infoText);
    control->SetParams(params);

    // ---------------- 스카이 조절 (8번에서 새로 추가) ----------------
    GameObject* skyControlObject = scene.CreateGameObject("SkyControl");
    SkyControlComponent* skyControl = skyControlObject->AddComponent<SkyControlComponent>();
    skyControl->SetTarget(sky);
    skyControl->SetInfoText(skyInfoText);

    // ---------------- 오른쪽 하단 버튼 패널 ----------------
    // 예전에 N / C / F5 / 0 키였던 조작들(높이맵)과 , / . / T / Y / I / K / U / J / 0 키였던
    // 조작들(스카이)을 버튼으로 옮긴다. 항목 선택(↑/↓)과 값 조절(←/→)처럼 여러 기법이
    // 공통으로 쓰는 조작은 그대로 키보드로 남아 있다. 시간 스크럽 버튼 두 개는 누르고
    // 있으면 계속 반응하도록 SetRepeatWhileHeld(true) 로 설정한다(예전 , / . 키와 동일).
    float buttonY = kHudBottomY;
    auto AddButton = [&](const char* name, const std::wstring& text, const UIButton::ClickCallback& onClick, bool repeatWhileHeld = false)
    {
        GameObject* obj = scene.CreateGameObject(name);
        UIButton* button = obj->AddComponent<UIButton>();
        button->SetText(text);
        button->SetFontSize(kHudFontSize);
        button->SetAnchor(UIAnchor::BottomRight);
        button->SetOffset(kHudMarginX, buttonY);
        button->SetColor(1.0f, 1.0f, 1.0f);
        button->SetOnClick(onClick);
        button->SetRepeatWhileHeld(repeatWhileHeld);
        buttonY += kButtonRowHeight;
    };

    auto AddHeader = [&](const char* name, const std::wstring& text, float r, float g, float b)
    {
        GameObject* obj = scene.CreateGameObject(name);
        UIText* header = obj->AddComponent<UIText>();
        header->SetText(text);
        header->SetFontSize(kHudFontSize);
        header->SetAnchor(UIAnchor::BottomRight);
        header->SetOffset(kHudMarginX, buttonY);
        header->SetColor(r, g, b);
        buttonY += kButtonRowHeight;
    };

    AddButton("SkyResetButton", L"기본값 복귀", [skyControl]() { skyControl->ResetToDefault(); });
    AddButton("SkyGlowWeakerButton", L"글로우 강하게", [skyControl]() { skyControl->DecreaseGlowExponent(); });
    AddButton("SkyGlowStrongerButton", L"글로우 약하게", [skyControl]() { skyControl->IncreaseGlowExponent(); });
    AddButton("SkySunSizeIncButton", L"태양 각크기 증가", [skyControl]() { skyControl->IncreaseSunSize(); });
    AddButton("SkySunSizeDecButton", L"태양 각크기 감소", [skyControl]() { skyControl->DecreaseSunSize(); });
    AddButton("SkyCyclePresetButton", L"프리셋 순환", [skyControl]() { skyControl->CyclePreset(); });
    AddButton("SkyAutoPlayButton", L"자동 재생 켬/끔", [skyControl]() { skyControl->ToggleAutoPlay(); });
    AddButton("SkyTimeForwardButton", L"시간 감기", [skyControl]() { skyControl->StepTimeForward(); }, true);
    AddButton("SkyTimeBackwardButton", L"시간 되감기", [skyControl]() { skyControl->StepTimeBackward(); }, true);
    AddHeader("SkyHeader", L"[스카이]", 1.0f, 0.9f, 0.7f);

    AddButton("HeightMapResetButton", L"기본값 복귀", [control]() { control->ResetToDefault(); });
    AddButton("HeightMapReloadButton", L"폴더 다시 읽기", [control]() { control->ReloadFiles(); });
    AddButton("HeightMapColorButton", L"고도 색상 켬/끔", [control]() { control->ToggleHeightColorMode(); });
    AddButton("HeightMapNextFileButton", L"다음 높이맵 파일", [control]() { control->NextFile(); });
    AddHeader("HeightMapHeader", L"[높이맵]", 1.0f, 1.0f, 1.0f);
}
