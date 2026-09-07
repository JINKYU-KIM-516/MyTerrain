#include "ShowcaseApp.h"
#include "../Framework/Framework.h"
#include "../UI/UIText.h"
#include "../UI/UIButton.h"
#include "../Components/KeyCommandComponent.h"

namespace
{
    // ---- 메뉴 화면 레이아웃 (픽셀) ----
    constexpr float kTitleFontSize = 40.0f;
    constexpr float kTitleOffsetY = 70.0f;

    constexpr float kItemFontSize = 26.0f;
    constexpr float kItemStartY = 190.0f;   // 첫 번째 기법 항목의 Y 위치
    constexpr float kItemSpacingY = 48.0f;  // 항목 간 간격

    constexpr float kCornerFontSize = 24.0f;
    constexpr float kCornerMargin = 28.0f;  // 화면 모서리에서 떨어진 여백

    // 기법 목록 표시 문자열: "n. 기법명 (영어 기법명)"
    std::wstring MakeMenuItemText(int index, const TechniqueEntry& entry)
    {
        return std::to_wstring(index) + L". " + entry.nameKo + L" (" + entry.nameEn + L")";
    }
}

ShowcaseApp& ShowcaseApp::GetInstance()
{
    static ShowcaseApp instance;
    return instance;
}

void ShowcaseApp::Initialize(Framework* framework)
{
    m_framework = framework;

    if (m_framework != nullptr)
    {
        // 프레임이 끝난 뒤에 씬 전환을 처리하도록 콜백 연결
        m_framework->SetEndOfFrameCallback([this]() { ProcessPendingRequest(); });
    }
}

void ShowcaseApp::RegisterTechnique(const std::wstring& nameKo, const std::wstring& nameEn,
                                    const std::function<void(Scene&)>& build)
{
    m_techniques.push_back({ nameKo, nameEn, build });
}

void ShowcaseApp::Start()
{
    if (m_framework == nullptr)
    {
        return;
    }

    // 게임 루프 시작 전이므로 즉시 메뉴 화면을 구성한다
    BuildMenuScene(m_framework->GetScene());
}

void ShowcaseApp::RequestMenu()
{
    m_request = Request::Menu;
}

void ShowcaseApp::RequestTechnique(int index)
{
    m_request = Request::Technique;
    m_requestedTechnique = index;
}

void ShowcaseApp::RequestExit()
{
    m_request = Request::Exit;
}

void ShowcaseApp::ProcessPendingRequest()
{
    if (m_request == Request::None || m_framework == nullptr)
    {
        return;
    }

    const Request request = m_request;
    const int techniqueIndex = m_requestedTechnique;

    m_request = Request::None;
    m_requestedTechnique = -1;

    if (request == Request::Exit)
    {
        m_framework->Quit();
        return;
    }

    Scene& scene = m_framework->GetScene();

    // 이전 화면의 GameObject를 모두 정리한 뒤 새 화면을 구성한다
    scene.Clear();

    if (request == Request::Menu)
    {
        BuildMenuScene(scene);
    }
    else
    {
        BuildTechniqueScene(scene, techniqueIndex);
    }

    // 새로 만들어진 GameObject들의 Start() 호출
    scene.Start();
}

void ShowcaseApp::BuildMenuScene(Scene& scene)
{
    // ---- 제목 ----
    GameObject* titleObject = scene.CreateGameObject("MenuTitle");
    UIText* title = titleObject->AddComponent<UIText>();
    title->SetText(L"===Terrain Showcase System===");
    title->SetFontSize(kTitleFontSize);
    title->SetBold(true);
    title->SetAnchor(UIAnchor::TopCenter);
    title->SetOffset(0.0f, kTitleOffsetY);
    title->SetColor(1.0f, 1.0f, 1.0f);

    // ---- 기법 목록 ----
    for (size_t i = 0; i < m_techniques.size(); ++i)
    {
        const int index = static_cast<int>(i);

        GameObject* itemObject = scene.CreateGameObject("MenuItem");
        UIButton* item = itemObject->AddComponent<UIButton>();
        item->SetText(MakeMenuItemText(index, m_techniques[i]));
        item->SetFontSize(kItemFontSize);
        item->SetAnchor(UIAnchor::TopCenter);
        item->SetOffset(0.0f, kItemStartY + kItemSpacingY * static_cast<float>(i));
        item->SetColor(1.0f, 1.0f, 1.0f);
        item->SetOnClick([this, index]() { RequestTechnique(index); });
    }

    // ---- 오른쪽 상단 종료 버튼 ----
    GameObject* exitObject = scene.CreateGameObject("ExitButton");
    UIButton* exitButton = exitObject->AddComponent<UIButton>();
    exitButton->SetText(L"종료");
    exitButton->SetFontSize(kCornerFontSize);
    exitButton->SetAnchor(UIAnchor::TopRight);
    exitButton->SetOffset(kCornerMargin, kCornerMargin);
    exitButton->SetColor(1.0f, 1.0f, 1.0f);
    exitButton->SetOnClick([this]() { RequestExit(); });

    // ---- ESC: 프로그램 종료 ----
    GameObject* hotkeyObject = scene.CreateGameObject("MenuHotkey");
    hotkeyObject->AddComponent<KeyCommandComponent>(VK_ESCAPE, [this]() { RequestExit(); });
}

void ShowcaseApp::BuildTechniqueScene(Scene& scene, int index)
{
    if (index < 0 || index >= static_cast<int>(m_techniques.size()))
    {
        // 잘못된 인덱스면 메뉴로 되돌린다
        BuildMenuScene(scene);
        return;
    }

    // ---- 모든 기법 화면 공통 UI: 왼쪽 상단 돌아가기 버튼 + ESC ----
    GameObject* backObject = scene.CreateGameObject("BackButton");
    UIButton* backButton = backObject->AddComponent<UIButton>();
    backButton->SetText(L"돌아가기");
    backButton->SetFontSize(kCornerFontSize);
    backButton->SetAnchor(UIAnchor::TopLeft);
    backButton->SetOffset(kCornerMargin, kCornerMargin);
    backButton->SetColor(1.0f, 1.0f, 1.0f);
    backButton->SetOnClick([this]() { RequestMenu(); });

    GameObject* hotkeyObject = scene.CreateGameObject("TechniqueHotkey");
    hotkeyObject->AddComponent<KeyCommandComponent>(VK_ESCAPE, [this]() { RequestMenu(); });

    // ---- 기법별 내용 구성 ----
    const TechniqueEntry& entry = m_techniques[static_cast<size_t>(index)];
    if (entry.build)
    {
        entry.build(scene);
    }
}
