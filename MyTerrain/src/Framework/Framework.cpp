#include "Framework.h"

Framework* Framework::s_instance = nullptr;

Framework::~Framework()
{
    Shutdown();
}

bool Framework::Initialize(HINSTANCE hInstance, int width, int height, const std::wstring& title)
{
    if (!m_window.Create(hInstance, width, height, title))
    {
        return false;
    }

    if (!m_renderer.Initialize(m_window.GetHandle(), width, height))
    {
        return false;
    }

    // 3D 렌더링 위에 UI 텍스트를 그리기 위한 Direct2D/DirectWrite 초기화
    if (!m_textRenderer.Initialize(m_renderer.GetSwapChain()))
    {
        return false;
    }

    // 창 크기가 바뀌면 스왑체인/렌더타겟도 다시 생성되도록 콜백 연결
    m_window.SetResizeCallback([this](UINT w, UINT h)
        {
            // D2D 렌더타겟이 백버퍼를 잡고 있으면 ResizeBuffers가 실패하므로 먼저 해제한다
            m_textRenderer.ReleaseSizeDependentResources();
            m_renderer.OnResize(w, h);
            m_textRenderer.CreateSizeDependentResources(m_renderer.GetSwapChain());
        });

    m_time.Reset();

    m_initialized = true;
    s_instance = this;
    return true;
}

void Framework::Quit()
{
    m_quitRequested = true;
}

void Framework::Run()
{
    if (!m_initialized)
    {
        return;
    }

    // 게임 루프 시작 전, 이미 생성되어 있는 모든 GameObject의 Start()를 호출
    m_scene.Start();

    bool running = true;
    while (running)
    {
        // 이번 프레임의 입력 메시지를 받기 "전에" 이전 프레임 상태를 저장해야
        // IsKeyPressed / IsMouseButtonPressed 같은 "눌린 순간" 판정이 동작한다.
        // (메시지 처리 후에 BeginFrame을 호출하면 prev == curr 가 되어 항상 false가 된다)
        InputManager::GetInstance().BeginFrame();

        running = m_window.ProcessMessages();
        if (!running)
        {
            break;
        }

        m_time.Tick();
        const float deltaTime = m_time.GetDeltaTime();

        // ---- Update ----
        m_scene.Update(deltaTime);

        // ---- Render ----
        m_renderer.BeginFrame(m_clearColor);
        m_scene.Render();       // 3D

        m_textRenderer.BeginDraw();
        m_scene.RenderUI();     // 2D UI(텍스트)
        m_textRenderer.EndDraw();

        m_renderer.EndFrame();

        // ---- 파괴 예약된 GameObject 정리 ----
        m_scene.LateUpdate();

        // ---- 프레임 종료 처리 (씬 전환 등) ----
        if (m_endOfFrameCallback)
        {
            m_endOfFrameCallback();
        }

        if (m_quitRequested)
        {
            break;
        }
    }
}

void Framework::Shutdown()
{
    if (!m_initialized)
    {
        return;
    }

    m_scene.Clear();
    m_textRenderer.Shutdown();
    m_initialized = false;

    if (s_instance == this)
    {
        s_instance = nullptr;
    }
}
