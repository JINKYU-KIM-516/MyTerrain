#include "Technique10_InfiniteChunks.h"

#include "../../GameObject/Scene.h"
#include "../../GameObject/GameObject.h"
#include "../../Framework/Camera.h"
#include "../../Components/FreeCameraController.h"
#include "../../Components/InfiniteControlComponent.h"
#include "../../Terrain/InfiniteTerrainRenderer.h"
#include "../../Terrain/PerlinNoise.h"
#include "../../UI/UIText.h"
#include "../../UI/UIButton.h"

namespace
{
    // ---- 노이즈 설정 ----
    // 청크 한 변이 128 월드단위이므로 스케일 96 이면 큰 굴곡 하나가 청크보다 조금 작다.
    // 청크 경계를 넘어 이어지는 지형인지 눈으로 확인하기 좋은 크기다.
    //
    // 참고: 펄린의 permutation 테이블이 256 칸이라 노이즈 격자는 256 칸마다 반복된다.
    //       스케일 96 이면 월드 좌표로 256 * 96 = 24,576 단위마다 같은 무늬가 돌아온다.
    //       걸어서 닿을 거리가 아니라 실습에서는 문제가 되지 않지만, "절차적 지형은
    //       무한이 아니라 아주 긴 주기를 갖는다"는 점은 알아둘 만하다.
    constexpr float kNoiseScale = 96.0f;
    constexpr int   kNoiseOctaves = 6;
    constexpr float kNoiseAmplitude = 45.0f;

    // 솔리드 모드에서 체커가 1셀이면 너무 잘아 노이즈처럼 보이므로 8셀로 키운다
    constexpr float kCheckerScale = 8.0f;

    // 배경(클리어) 색. Framework 의 기본값과 같게 두고, 안개 색도 여기에 맞춘다.
    // 이 셋이 어긋나면 안개가 끝나는 지점에 색 경계가 그대로 보인다.
    constexpr float kBackgroundR = 0.15f;
    constexpr float kBackgroundG = 0.35f;
    constexpr float kBackgroundB = 0.65f;

    // ---- HUD 레이아웃 (픽셀) ----
    constexpr float kHudFontSize = 16.0f;
    constexpr float kHudMarginX = 28.0f;
    constexpr float kHudTopY = 72.0f;
    constexpr float kHudBottomY = 24.0f;

    // ---- 오른쪽 하단 버튼 패널 (픽셀) ----
    constexpr float kButtonRowHeight = 26.0f;

    constexpr wchar_t kHelpText[] =
        L"[카메라]  이동 W/A/S/D   상하 E/Q   시점 마우스 우클릭 드래그   속도 휠   가속 Shift   리셋 R\n"
        L"[관찰]  주황색으로 반짝이는 청크가 방금 만들어진 것이다. 멀리 순간이동 -> 생성 큐가 차는 것을 보자.\n"
        L"[무한 청크]  표시 모드 Tab   그 외 조작은 우측 버튼                                     [메뉴로] ESC";
}

void BuildInfiniteChunksScene(Scene& scene)
{
    // ---------------- 카메라 ----------------
    GameObject* cameraObject = scene.CreateGameObject("MainCamera");
    cameraObject->GetTransform()->SetPosition(0.0f, 90.0f, -150.0f);
    cameraObject->GetTransform()->SetRotation(18.0f, 0.0f, 0.0f);

    Camera* camera = cameraObject->AddComponent<Camera>();
    camera->SetClipPlanes(0.1f, 8000.0f);
    camera->SetAsMain();

    FreeCameraController* controller = cameraObject->AddComponent<FreeCameraController>();
    controller->SetMoveSpeed(60.0f);

    // ---------------- 무한 지형 ----------------
    // 지금까지와 달리 TerrainRenderer 가 아니다. 청크마다 파이프라인을 새로 만들지
    // 않기 위해, 이 기법은 전용 렌더러 하나가 모든 청크의 버퍼를 직접 관리한다.
    GameObject* terrainObject = scene.CreateGameObject("InfiniteTerrain");
    InfiniteTerrainRenderer* terrain = terrainObject->AddComponent<InfiniteTerrainRenderer>();

    terrain->SetDisplayMode(ChunkDisplayMode::Solid);
    terrain->SetSolidColor(0.40f, 0.52f, 0.36f);
    terrain->SetWireColor(0.92f, 0.96f, 1.0f);
    terrain->SetLightDirection(0.45f, -1.0f, 0.35f);
    terrain->SetCheckerScale(kCheckerScale);

    // 안개 색 = 배경색. 로드 반경 끝에서 청크가 사라지는 경계를 이 색으로 덮는다.
    terrain->SetFogColor(kBackgroundR, kBackgroundG, kBackgroundB);
    terrain->SetFogEnabled(true);

    // ---------------- HUD: 현재 상태 ----------------
    GameObject* infoObject = scene.CreateGameObject("InfiniteInfo");
    UIText* infoText = infoObject->AddComponent<UIText>();
    infoText->SetText(L"");
    infoText->SetFontSize(kHudFontSize);
    infoText->SetAnchor(UIAnchor::TopLeft);
    infoText->SetOffset(kHudMarginX, kHudTopY);
    infoText->SetColor(1.0f, 1.0f, 1.0f);

    // ---------------- HUD: 조작 안내 ----------------
    GameObject* helpObject = scene.CreateGameObject("InfiniteHelp");
    UIText* helpText = helpObject->AddComponent<UIText>();
    helpText->SetText(kHelpText);
    helpText->SetFontSize(kHudFontSize);
    helpText->SetAnchor(UIAnchor::BottomLeft);
    helpText->SetOffset(kHudMarginX, kHudBottomY);
    helpText->SetColor(1.0f, 1.0f, 1.0f);

    // ---------------- 조작 + 높이 함수 ----------------
    Noise::Params params;
    params.scale = kNoiseScale;
    params.octaves = kNoiseOctaves;
    params.lacunarity = 2.0f;
    params.persistence = 0.5f;
    params.amplitude = kNoiseAmplitude;
    params.seed = 1337u;
    params.type = Noise::Type::FBM;

    GameObject* controlObject = scene.CreateGameObject("InfiniteControl");
    InfiniteControlComponent* control = controlObject->AddComponent<InfiniteControlComponent>();
    control->SetTarget(terrain);
    control->SetInfoText(infoText);
    control->SetParams(params);

    // ---------------- 오른쪽 하단 버튼 패널 ----------------
    float buttonY = kHudBottomY;
    auto AddButton = [&](const char* name, const std::wstring& text, const UIButton::ClickCallback& onClick)
    {
        GameObject* obj = scene.CreateGameObject(name);
        UIButton* button = obj->AddComponent<UIButton>();
        button->SetText(text);
        button->SetFontSize(kHudFontSize);
        button->SetAnchor(UIAnchor::BottomRight);
        button->SetOffset(kHudMarginX, buttonY);
        button->SetColor(1.0f, 1.0f, 1.0f);
        button->SetOnClick(onClick);
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

    AddButton("InfiniteResetButton", L"기본값 복귀", [control]() { control->ResetToDefault(); });
    AddButton("InfiniteJumpButton", L"멀리 순간이동", [control]() { control->JumpFar(); });
    AddButton("InfiniteSeedButton", L"시드 무작위 (전부 재생성)", [control]() { control->RandomizeSeed(); });
    AddButton("InfiniteFogButton", L"안개 켬/끔", [control]() { control->ToggleFog(); });
    AddButton("InfiniteHighlightButton", L"신규 청크 강조 켬/끔", [control]() { control->ToggleHighlight(); });
    AddButton("InfiniteColorButton", L"청크 색상 켬/끔", [control]() { control->ToggleChunkColor(); });
    AddButton("InfiniteCullButton", L"절두체 컬링 켬/끔", [control]() { control->ToggleFrustumCulling(); });
    AddButton("InfinitePauseButton", L"스트리밍 일시정지", [control]() { control->ToggleStreaming(); });
    AddButton("InfiniteBudgetDecButton", L"프레임당 생성 감소", [control]() { control->DecreaseBudget(); });
    AddButton("InfiniteBudgetIncButton", L"프레임당 생성 증가", [control]() { control->IncreaseBudget(); });
    AddButton("InfiniteDivDecButton", L"분할 수 감소", [control]() { control->DecreaseDivisions(); });
    AddButton("InfiniteDivIncButton", L"분할 수 증가", [control]() { control->IncreaseDivisions(); });
    AddButton("InfiniteRadiusDecButton", L"유지 반경 감소", [control]() { control->DecreaseRadius(); });
    AddButton("InfiniteRadiusIncButton", L"유지 반경 증가", [control]() { control->IncreaseRadius(); });
    AddHeader("InfiniteHeader", L"[무한 청크]", 0.85f, 0.95f, 0.85f);
}
